#!/bin/sh
# Downloads the standard JSON benchmark datasets used by `make bench`, pinned to
# one commit of github.com/miloyip/nativejson-benchmark and verified by SHA-256,
# so every run measures exactly the same bytes.
set -eu

DIR=${1:-bench/data}
COMMIT=478d5727c2a4048e835a29c65adecc7d795360d5
BASE="https://raw.githubusercontent.com/miloyip/nativejson-benchmark/$COMMIT/data"

mkdir -p "$DIR"

sha256() {
  if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | cut -d' ' -f1
  else shasum -a 256 "$1" | cut -d' ' -f1; fi
}

fetch() {
  name=$1 expected=$2
  file="$DIR/$name.json"
  if [ -f "$file" ] && [ "$(sha256 "$file")" = "$expected" ]; then return 0; fi
  echo "downloading $name.json"
  curl -fsSL "$BASE/$name.json" -o "$file.part"
  got=$(sha256 "$file.part")
  if [ "$got" != "$expected" ]; then
    echo "checksum mismatch for $name.json: $got" >&2
    rm -f "$file.part"
    exit 1
  fi
  mv "$file.part" "$file"
}

fetch twitter      a08b769f32b95f426cbc3abafcec65c1a19d3eb544d4ddf320eae142c99efc5d
fetch canada       f83b3b354030d5dd58740c68ac4fecef64cb730a0d12a90362a7f23077f50d78
fetch citm_catalog a73e7a883f6ea8de113dff59702975e60119b4b58d451d518a929f31c92e2059
echo "datasets ready in $DIR"
