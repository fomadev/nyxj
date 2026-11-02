#!/usr/bin/env bash
# End-to-end smoke test of the built binaries: CLI, server, plugin and API.
# Usage: tests/smoke.sh [build-dir]
set -euo pipefail

BIN=${1:-build}
EXE=""
SO=".so"
case "$(uname -s)" in
  MINGW* | MSYS* | CYGWIN*) EXE=".exe"; SO=".dll" ;;
esac
PORT=${PORT:-18980}
BASE="http://127.0.0.1:$PORT"

"$BIN/nyxj$EXE" version

fixed=$(printf '%s' "{name: 'NyxJ', tags: ['fast' 'small',], ok: True}" | "$BIN/nyxj$EXE" repair --indent 0 2>/dev/null)
[ "$fixed" = '{"name":"NyxJ","tags":["fast","small"],"ok":true}' ] || { echo "FAIL repair: $fixed"; exit 1; }
echo "ok   cli repair"

"$BIN/nyxj-server$EXE" --port "$PORT" --plugin "$BIN/plugins/hello_plugin$SO" --no-access-log --log-level warn &
PID=$!
trap 'kill $PID 2>/dev/null || true' EXIT
for _ in $(seq 1 100); do
  curl -sf "$BASE/health" >/dev/null 2>&1 && break
  sleep 0.1
done

check() {
  local name=$1 expected=$2
  shift 2
  local out
  out=$(curl -s "$@")
  case "$out" in
    *"$expected"*) echo "ok   $name" ;;
    *) echo "FAIL $name: $out"; exit 1 ;;
  esac
}

check health    '"status":"ok"'               "$BASE/health"
check ready     '"status":"ready"'            "$BASE/ready"
check plugin    'dynamically loaded plugin'   "$BASE/hello/CI"
check echo      '{"x":[1,2,{"y":null}]}'      -X POST -d '{"x":[1,2,{"y":null}]}' "$BASE/api/data"
check invalid   '"error":"invalid_json"'      -X POST -d '{"x":}' "$BASE/api/data"
check query     '"count":2'                   -X POST -d '{"query":"$..p","data":{"a":{"p":1},"b":[{"p":2}]}}' "$BASE/api/query"
check schema    '"valid":false'               -X POST -d '{"schema":{"required":["id"]},"data":{}}' "$BASE/api/validate"
check repair    '"valid":true'                -X POST -d "{a: 1, b: [1 2]}" "$BASE/api/repair"
check diff      '"op":"replace"'              -X POST -d '{"from":{"a":1},"to":{"a":2}}' "$BASE/api/diff"
check kv_put    '"stored":true'               -X PUT -d '{"user":"ci"}' "$BASE/api/kv/session"
check kv_get    '"user":"ci"'                 "$BASE/api/kv/session"
check kv_incr   '"value":3'                   -X POST "$BASE/api/kv/hits/incr?by=3"
curl -s -X POST -d '{"hello":"world"}' -o msgpack.bin "$BASE/api/msgpack/encode"
out=$(curl -s -X POST --data-binary @msgpack.bin "$BASE/api/msgpack/decode")
rm -f msgpack.bin
[ "$out" = '{"hello":"world"}' ] && echo "ok   msgpack roundtrip" || { echo "FAIL msgpack: $out"; exit 1; }
check not_found '"status":404'                "$BASE/nowhere"
check metrics   'nyxj_http_requests_total'    "$BASE/metrics"
check openapi   '"/api/kv/{key}"'             "$BASE/openapi.json"
check info      '"version":"'                 "$BASE/api/info"
echo "smoke test passed"
