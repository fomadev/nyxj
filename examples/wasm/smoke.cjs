// Node.js smoke test for the WebAssembly build: node examples/wasm/smoke.cjs
const path = require('path');
const NyxJ = require(path.join(__dirname, '..', '..', 'build', 'wasm', 'nyxj.js'));

NyxJ().then((m) => {
  const call = (name, ...args) => {
    const types = args.map((a) => (typeof a === 'number' ? 'number' : 'string'));
    const ptr = m.ccall(name, 'number', types, args);
    const text = m.UTF8ToString(ptr);
    m._nyxj_wasm_free(ptr);
    return JSON.parse(text);
  };
  const assert = (cond, what) => {
    if (!cond) { console.error('FAIL', what); process.exit(1); }
    console.log('ok  ', what);
  };

  console.log('NyxJ WebAssembly', m.UTF8ToString(m.ccall('nyxj_wasm_version', 'number', [], [])));
  assert(call('nyxj_wasm_format', '{"a":[1,2]}', 0).result === '{"a":[1,2]}', 'format');
  assert(call('nyxj_wasm_validate', '{"a":').error.code === 'UNEXPECTED_EOF', 'validate reports errors');
  assert(call('nyxj_wasm_query', '{"a":[{"p":1},{"p":5}]}', '$.a[?(@.p > 2)].p').result[0] === 5, 'query');
  assert(call('nyxj_wasm_repair', "{a: 'x',}").result.json === '{"a":"x"}', 'repair');
  assert(call('nyxj_wasm_schema', '{"type":"string"}', '1').result.valid === false, 'schema');
  assert(call('nyxj_wasm_diff', '{"a":1}', '{"a":2}').result[0].op === 'replace', 'diff');
  assert(call('nyxj_wasm_msgpack_size', '{"hello":"world"}').result.msgpack_bytes < 17, 'msgpack');
  console.log('wasm smoke test passed');
});
