#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
#
# A scratch install, and a consumer built with only what `pkg-config plugin-db` says of it, its digests compared with
# python's hashlib over the bytes the format's text prescribes and an address read through libplugin-db. And the arms:
# a pc file that does not require libcrypto cannot link the consumer, one without its Libs cannot either, and one
# without its Cflags cannot compile it. The install holds the command and the store's schema.
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

pcdir=$PREFIX/lib/pkgconfig
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
want.append("lv2")
got = open(sys.argv[1]).read().split()
sys.exit(0 if got == want else print("FAIL the consumer's digests and address", got, want) or 1)
PY

mkdir "$tmp/bad"
sed '/^Requires:/d' "$pcdir/plugin-db.pc" > "$tmp/bad/plugin-db.pc"
if build "$tmp/bad" "$tmp/bad/consumer"; then echo "FAIL a pc file without libcrypto still links the consumer"; exit 1; fi
sed '/^Libs:/d' "$pcdir/plugin-db.pc" > "$tmp/bad/plugin-db.pc"
if build "$tmp/bad" "$tmp/bad/consumer"; then echo "FAIL a pc file without Libs still links the consumer"; exit 1; fi
sed '/^Cflags:/d' "$pcdir/plugin-db.pc" > "$tmp/bad/plugin-db.pc"
if build "$tmp/bad" "$tmp/bad/consumer"; then echo "FAIL a pc file without Cflags still compiles the consumer"; exit 1; fi
test -x "$PREFIX/bin/plugin-db" || { echo "FAIL no plugin-db command in the install"; exit 1; }
"$PREFIX/bin/plugin-db" --store "$tmp/store" verify || { echo "FAIL the installed command is red on an empty store"; exit 1; }
test -s "$PREFIX/share/man/man1/plugin-db.1" || { echo "FAIL the command has no manual page"; exit 1; }
cmp -s data/store.schema.json "$PREFIX/share/plugin-db/store.schema.json" || { echo "FAIL the store's schema is not installed"; exit 1; }
echo "ok   install: a consumer builds with only pkg-config plugin-db, its digests are hashlib's, the pc file's lines are needed, the command, its manual page and the schema are installed"
