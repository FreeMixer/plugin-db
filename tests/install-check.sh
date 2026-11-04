#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
#
# A scratch install, and a consumer built with only what `pkg-config plugin-db` says of it, its digests compared with
# python's hashlib over the bytes the format's text prescribes. And the arms: a pc file that does not require libcrypto
# cannot link the consumer, and one without its Cflags cannot compile it.
set -eu
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
PREFIX=$tmp/prefix
make --no-print-directory install PREFIX="$PREFIX" >/dev/null

build() { # <PKG_CONFIG_PATH> <output>
    # shellcheck disable=SC2046 # the flags are words
    PKG_CONFIG_PATH=$1 cc -std=gnu99 -Wall -Wextra -Werror $(PKG_CONFIG_PATH=$1 pkg-config --cflags plugin-db) \
        -o "$2" tests/consumer.c $(PKG_CONFIG_PATH=$1 pkg-config --libs plugin-db) 2>"$tmp/cc.log"
}

pcdir=$PREFIX/share/pkgconfig
test "$(PKG_CONFIG_PATH=$pcdir pkg-config --modversion plugin-db)" = "$(cat VERSION)"
build "$pcdir" "$tmp/consumer" || { cat "$tmp/cc.log"; echo "FAIL a consumer does not build with pkg-config plugin-db"; exit 1; }

printf 'abc' > "$tmp/fixture"
"$tmp/consumer" "$tmp/fixture" > "$tmp/got"
python3 - "$tmp/got" <<'PY'
import hashlib, sys
want = [hashlib.sha256(b).hexdigest() for b in (
    b"abc",
    b"omx-layout/1 clap\n1\tGain\t0000000000000000\t3ff0000000000000\t3fe0000000000000\t00000001\n",
    b"omx-layout/1 lv2\n0\tgain\tin\t0000000000000000\t3ff0000000000000\t-\thttp://lv2plug.in/ns/lv2core#integer\n",
)]
got = open(sys.argv[1]).read().split()
sys.exit(0 if got == want else print("FAIL the consumer's digests", got, want) or 1)
PY

mkdir "$tmp/bad"
sed '/^Requires:/d' "$pcdir/plugin-db.pc" > "$tmp/bad/plugin-db.pc"
if build "$tmp/bad" "$tmp/bad/consumer"; then echo "FAIL a pc file without libcrypto still links the consumer"; exit 1; fi
sed '/^Cflags:/d' "$pcdir/plugin-db.pc" > "$tmp/bad/plugin-db.pc"
if build "$tmp/bad" "$tmp/bad/consumer"; then echo "FAIL a pc file without Cflags still compiles the consumer"; exit 1; fi
echo "ok   install: a consumer builds with only pkg-config plugin-db, its digests are hashlib's, and the pc file's lines are needed"
