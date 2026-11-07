# Contributing to NyxJ Engine

Thank you for considering a contribution to NyxJ. As a project managed by FomaDev, a Fordima Inc. company, we value professional,
well-tested contributions that keep the engine fast and reliable.

## License Agreement

By contributing to this repository, you agree that your contributions will be licensed under the
**Fordima Public License (FPL) v1.0**, and that Fordima Inc. may integrate them into the official project.
Forks are authorized only for the purpose of contributing back through a pull request; independent or
permanent forks created to bypass the official project are not permitted. See [LICENSE](LICENSE) for the
full terms.

## Code of Conduct

Be respectful and constructive. Technical disagreements are settled with data: tests, benchmarks and
references.

## How Can I Contribute?

### Reporting bugs
* Search the existing issues first.
* Include the NyxJ version (`nyxj version`), your platform, a minimal input that reproduces the problem
  and the exact error (code, line and column).

### Suggesting enhancements
* Open an issue describing the use case. Check [ROADMAP.md](ROADMAP.md) for planned work.

### Pull requests
1. Fork the repository and create a branch from `main`.
2. Add or update tests in `tests/` for every behavior change.
3. Run `make test` (and `make SAN=1 test` on Linux or macOS when you touch memory handling).
4. Update the documentation (`DOCUMENTATION.md`, `README.md`, `docs/`, header comments) and add an entry under
   `## [Unreleased]` or the upcoming version in `CHANGELOG.md`.
5. Open the pull request: CI must pass on Linux, macOS, Windows, sanitizers and WebAssembly.

## Commit messages

Commits follow [Conventional Commits](https://www.conventionalcommits.org/). The release pipeline groups
them in the release notes, so the type matters:

| Type | Use for | Release notes section |
|---|---|---|
| `feat` | A new feature | New features |
| `fix` | A bug fix | Bug fixes |
| `perf` | A performance improvement | Performance |
| `refactor` | Code change without behavior change | Refactoring |
| `docs` | Documentation | Documentation |
| `test` | Tests | Tests |
| `build`, `ci` | Build system and pipelines | Build and CI |
| `chore`, `style` | Maintenance | Maintenance |

Add `!` after the type (`feat!: ...`) or a `BREAKING CHANGE:` footer for incompatible changes.

## Coding standards

* **C standard:** C11 (GNU dialect allowed for POSIX functions), no compiler-specific extensions in public
  headers.
* **Memory:** check every allocation, free everything on error paths, document ownership in headers
  ("takes ownership", "caller frees").
* **Thread safety:** no hidden global state in the core; runtime modules protect shared state.
* **Portability:** code must build on Linux, macOS and Windows (MinGW-w64); keep the core free of
  networking so it still compiles to WebAssembly.
* **Style:** 4-space indentation, `snake_case`, the `nyxj_` prefix for public symbols, short comments that
  explain why.

## Releases

Maintainers release by pushing a `vX.Y.Z` tag that matches `NYXJ_VERSION` in `include/nyxj.h`. The
pipeline builds every platform, runs the tests, and publishes a GitHub release with notes and assets.

## Contact

Open an issue or reach the FomaDev team through the GitHub repository.