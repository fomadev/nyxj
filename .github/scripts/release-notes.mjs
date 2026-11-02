#!/usr/bin/env node
/*
 * Generates the GitHub release notes for a NyxJ version.
 *
 *   node .github/scripts/release-notes.mjs --tag v2.0.0 [--target <ref>] [--assets dist] [--out notes.md]
 *
 * Sources, in order of importance:
 *   1. The CHANGELOG.md section of the version (curated highlights)
 *   2. Conventional commits since the previous tag, grouped by type
 *   3. Merged pull requests and contributors from the GitHub API (when GITHUB_TOKEN is set)
 *   4. Release assets with sizes and SHA-256 checksums
 * Everything degrades gracefully: without network access the notes still build from git alone.
 */
import { execFileSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { existsSync, readFileSync, readdirSync, statSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';

const args = Object.fromEntries(
  process.argv.slice(2).reduce((acc, a, i, all) => (a.startsWith('--') ? [...acc, [a.slice(2), all[i + 1]]] : acc), []),
);
const tag = args.tag;
if (!tag) {
  console.error('usage: release-notes.mjs --tag vX.Y.Z [--target ref] [--assets dir] [--out file]');
  process.exit(2);
}
const version = tag.replace(/^v/, '');
const repo = process.env.GITHUB_REPOSITORY || 'fomadev/nyxj';
const server = process.env.GITHUB_SERVER_URL || 'https://github.com';
const token = process.env.GITHUB_TOKEN || process.env.GH_TOKEN || '';
const repoUrl = `${server}/${repo}`;

const git = (...a) => execFileSync('git', a, { encoding: 'utf8', stdio: ['ignore', 'pipe', 'ignore'] }).trim();
const tryGit = (...a) => {
  try { return git(...a); } catch { return ''; }
};

/* --- Range --- */

const tagExists = tryGit('rev-parse', '-q', '--verify', `refs/tags/${tag}`) !== '';
const target = args.target || (tagExists ? tag : 'HEAD');
const targetSha = git('rev-parse', `${target}^{commit}`);
const previous = tryGit('describe', '--tags', '--abbrev=0', '--match', 'v*', `${targetSha}^`);
const range = previous ? `${previous}..${targetSha}` : targetSha;
const previousDate = previous ? git('log', '-1', '--format=%cI', previous) : '';

/* --- Conventional commits --- */

const SECTIONS = [
  ['breaking', '⚠️ Breaking changes'],
  ['feat', '✨ New features'],
  ['fix', '🐛 Bug fixes'],
  ['perf', '⚡ Performance'],
  ['refactor', '♻️ Refactoring'],
  ['docs', '📚 Documentation'],
  ['test', '🧪 Tests'],
  ['build', '🏗️ Build and CI'],
  ['chore', '🧹 Maintenance'],
  ['other', '📦 Other changes'],
];
const TYPE_TO_SECTION = { feat: 'feat', fix: 'fix', perf: 'perf', refactor: 'refactor', docs: 'docs', test: 'test',
  build: 'build', ci: 'build', chore: 'chore', style: 'chore', revert: 'fix' };

/* Oldest first, so each section reads in the order the work was done. */
const raw = tryGit('log', range, '--no-merges', '--reverse', '--format=%H%x1f%an%x1f%ae%x1f%s%x1f%b%x1e');
const commits = raw.split('\x1e').map((r) => r.trim()).filter(Boolean).map((r) => {
  const [sha, author, email, subject, body = ''] = r.split('\x1f');
  const m = subject.match(/^(\w+)(?:\(([^)]+)\))?(!)?:\s*(.+)$/);
  const type = m ? m[1].toLowerCase() : 'other';
  const coAuthors = [...body.matchAll(/^Co-authored-by:\s*(.+?)\s*<(.+?)>/gim)].map((x) => ({ name: x[1], email: x[2] }));
  return {
    sha, author, email, subject, body, coAuthors,
    type,
    scope: m ? m[2] : '',
    description: m ? m[4] : subject,
    breaking: Boolean(m && m[3]) || /^BREAKING[ -]CHANGE:/m.test(body),
    section: TYPE_TO_SECTION[type] || 'other',
  };
});

/* --- GitHub API (optional) --- */

async function api(path) {
  if (!token) return null;
  try {
    const res = await fetch(`https://api.github.com/${path}`, {
      headers: { Authorization: `Bearer ${token}`, Accept: 'application/vnd.github+json', 'X-GitHub-Api-Version': '2022-11-28' },
    });
    return res.ok ? res.json() : null;
  } catch {
    return null;
  }
}

const loginBySha = new Map();
const contributors = new Map(); /* key -> {name, login, commits} */
const bots = /\[bot\]$|^github-actions|^dependabot/i;

async function collectContributors() {
  if (previous) {
    for (let page = 1; page <= 10; page++) {
      const cmp = await api(`repos/${repo}/compare/${previous}...${targetSha}?per_page=100&page=${page}`);
      if (!cmp || !cmp.commits || cmp.commits.length === 0) break;
      for (const c of cmp.commits) if (c.author && c.author.login) loginBySha.set(c.sha, c.author.login);
      if (cmp.commits.length < 100) break;
    }
  }
  for (const c of commits) {
    const login = loginBySha.get(c.sha);
    const key = login || c.email.toLowerCase();
    const entry = contributors.get(key) || { name: c.author, login, commits: 0 };
    entry.commits++;
    contributors.set(key, entry);
    for (const co of c.coAuthors) {
      const k = co.email.toLowerCase();
      if (!contributors.has(k)) contributors.set(k, { name: co.name, login: null, commits: 0, coAuthor: true });
    }
  }
}

async function newContributors() {
  if (!previousDate) return [];
  const fresh = [];
  for (const c of contributors.values()) {
    if (!c.login || bots.test(c.login)) continue;
    const before = await api(`repos/${repo}/commits?author=${encodeURIComponent(c.login)}&until=${encodeURIComponent(previousDate)}&per_page=1`);
    if (Array.isArray(before) && before.length === 0) fresh.push(c.login);
  }
  return fresh;
}

async function mergedPullRequests() {
  if (!token) return [];
  const shas = new Set(commits.map((c) => c.sha));
  const all = tryGit('log', range, '--format=%H').split('\n').filter(Boolean);
  for (const s of all) shas.add(s);
  const prs = [];
  for (let page = 1; page <= 5; page++) {
    const list = await api(`repos/${repo}/pulls?state=closed&sort=updated&direction=desc&per_page=100&page=${page}`);
    if (!Array.isArray(list) || list.length === 0) break;
    for (const pr of list) {
      if (!pr.merged_at) continue;
      if (shas.has(pr.merge_commit_sha) || (pr.head && shas.has(pr.head.sha))) prs.push(pr);
    }
    if (list.length < 100) break;
  }
  return prs.sort((a, b) => a.number - b.number);
}

/* --- CHANGELOG --- */

function changelogSection() {
  if (!existsSync('CHANGELOG.md')) return '';
  const text = readFileSync('CHANGELOG.md', 'utf8').replace(/\r\n/g, '\n');
  const esc = version.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
  const m = text.match(new RegExp(`^## \\[?v?${esc}\\]?[^\\n]*\\n([\\s\\S]*?)(?=^## \\[|^\\[[^\\]]+\\]:|(?![\\s\\S]))`, 'm'));
  return m ? m[1].trim() : '';
}

/* --- Assets --- */

const PLATFORMS = [
  [/linux-x64|linux-x86_64/, 'Linux x86-64'],
  [/linux-arm64|linux-aarch64/, 'Linux ARM64'],
  [/macos-arm64/, 'macOS Apple Silicon'],
  [/macos-x86_64|macos-x64/, 'macOS Intel'],
  [/windows-x64/, 'Windows x64'],
  [/wasm/, 'WebAssembly (browser, Node.js, edge)'],
  [/SHA256SUMS/, 'Checksums'],
];

function assetRows(dir) {
  if (!dir || !existsSync(dir)) return [];
  return readdirSync(dir).filter((f) => statSync(join(dir, f)).isFile()).sort().map((f) => {
    const data = readFileSync(join(dir, f));
    const size = data.length;
    const human = size > 1048576 ? `${(size / 1048576).toFixed(1)} MiB` : `${Math.max(1, Math.round(size / 1024))} KiB`;
    const platform = (PLATFORMS.find(([re]) => re.test(f)) || [null, 'Other'])[1];
    return { file: f, platform, size: human, sha: createHash('sha256').update(data).digest('hex') };
  });
}

/* --- Rendering --- */

function commitLine(c) {
  const short = c.sha.slice(0, 7);
  const login = loginBySha.get(c.sha);
  const scope = c.scope ? `**${c.scope}:** ` : '';
  const desc = c.description.charAt(0).toUpperCase() + c.description.slice(1);
  const by = login ? ` by @${login}` : '';
  return `- ${scope}${desc} ([\`${short}\`](${repoUrl}/commit/${c.sha}))${by}`;
}

async function main() {
  await collectContributors();
  const [fresh, prs] = await Promise.all([newContributors(), mergedPullRequests()]);
  const highlights = changelogSection();
  const assets = assetRows(args.assets);
  const shortstat = previous ? tryGit('diff', '--shortstat', previous, targetSha) : '';
  const prerelease = /-/.test(version);
  const humans = [...contributors.values()].filter((c) => !(c.login && bots.test(c.login)));
  const date = new Date().toISOString().slice(0, 10);

  const out = [];
  out.push(`# NyxJ ${version}${prerelease ? ' (pre-release)' : ''}`);
  out.push('');
  const counts = [`**${commits.length}** commits`, `**${humans.length}** contributor${humans.length === 1 ? '' : 's'}`];
  if (prs.length) counts.push(`**${prs.length}** pull request${prs.length === 1 ? '' : 's'}`);
  if (shortstat) counts.push(shortstat.replace(/^\s*/, ''));
  out.push(`> Released ${date}${previous ? ` · changes since \`${previous}\`` : ''} · ${counts.join(' · ')}`);
  out.push('');

  const breaking = commits.filter((c) => c.breaking);
  if (highlights) {
    out.push('## Highlights', '', highlights, '');
  } else {
    const feats = commits.filter((c) => c.section === 'feat').slice(0, 8);
    if (feats.length) out.push('## Highlights', '', ...feats.map(commitLine), '');
  }

  if (breaking.length) {
    out.push('## ⚠️ Breaking changes', '');
    for (const c of breaking) {
      const note = (c.body.match(/^BREAKING[ -]CHANGE:\s*([\s\S]+?)(?:\n\n|$)/m) || [])[1];
      out.push(note ? `${commitLine(c)}\n  ${note.trim().replace(/\n/g, ' ')}` : commitLine(c));
    }
    out.push('');
  }

  out.push('## What changed', '');
  out.push('<details open><summary>Commits grouped by type (Conventional Commits)</summary>', '');
  for (const [key, title] of SECTIONS.slice(1)) {
    const list = commits.filter((c) => c.section === key);
    if (!list.length) continue;
    out.push(`### ${title}`, '', ...list.map(commitLine), '');
  }
  out.push('</details>', '');

  if (prs.length) {
    out.push('## Merged pull requests', '');
    for (const pr of prs) out.push(`- ${pr.title} by @${pr.user.login} in [#${pr.number}](${pr.html_url})`);
    out.push('');
  }

  out.push('## Contributors', '');
  if (humans.length) {
    for (const c of humans.sort((a, b) => b.commits - a.commits)) {
      const who = c.login ? `@${c.login}` : c.name;
      out.push(`- ${who}${c.coAuthor ? ' (co-author)' : ` — ${c.commits} commit${c.commits === 1 ? '' : 's'}`}`);
    }
  } else {
    out.push('- No commits in this range.');
  }
  if (fresh.length) {
    out.push('', '### 🎉 New contributors', '');
    for (const l of fresh) out.push(`- @${l} made their first contribution`);
  }
  out.push('');

  if (assets.length) {
    out.push('## Downloads', '');
    out.push('| Asset | Platform | Size | SHA-256 |', '|---|---|---|---|');
    for (const a of assets)
      out.push(`| [\`${a.file}\`](${repoUrl}/releases/download/${tag}/${a.file}) | ${a.platform} | ${a.size} | \`${a.sha.slice(0, 16)}…\` |`);
    out.push('');
    out.push('Each platform archive contains `bin/nyxj` (CLI), `bin/nyxj-server`, `lib/libnyxj.a`, the public headers,');
    out.push('the example plugin and the example programs. Verify downloads with `sha256sum -c SHA256SUMS.txt`.');
    out.push('');
  }

  out.push('## Install', '');
  out.push('```bash');
  out.push(`curl -LO ${repoUrl}/releases/download/${tag}/nyxj-${version}-linux-x64.tar.gz`);
  out.push(`tar -xzf nyxj-${version}-linux-x64.tar.gz`);
  out.push(`./nyxj-${version}-linux-x64/bin/nyxj version`);
  out.push(`./nyxj-${version}-linux-x64/bin/nyxj-server --port 8000   # then open http://localhost:8000/docs`);
  out.push('```', '');
  out.push(`Build from source: \`git clone --branch ${tag} ${repoUrl}.git && cd nyxj && make && make test\`.`);
  out.push('');
  out.push('---');
  out.push(previous ? `**Full changelog:** [\`${previous}...${tag}\`](${repoUrl}/compare/${previous}...${tag})` : `**Full history:** ${repoUrl}/commits/${tag}`);
  out.push(' · [Documentation](' + `${repoUrl}/blob/${tag}/docs/API.md` + ') · [Roadmap](' + `${repoUrl}/blob/${tag}/ROADMAP.md` + ')');

  const md = out.join('\n') + '\n';
  if (args.out) writeFileSync(args.out, md);
  else process.stdout.write(md);
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
