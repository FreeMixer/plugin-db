#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
#
# Each arm breaks one parsing rule in a copy of the header; the harness built over the copy must abort on the seed corpus.
# An arm whose text is gone from the header is itself a failure.
set -eu
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
CLANG=${CLANG:-clang}
crypto=$(pkg-config --cflags --libs libcrypto)
fail=0

arm() { # <what> <before => after>
    rm -rf "$tmp/include"
    cp -r include "$tmp/include"
    if ! python3 - "$tmp/include/plugin-db/pin.h" "$2" <<'PY'
import sys
path, ba = sys.argv[1:]
b, a = ba.split(" => ")
s = open(path).read()
if b not in s:
    sys.exit("the text is gone: " + b)
open(path, "w").write(s.replace(b, a, 1))
PY
    then echo "FAIL $1: the arm no longer applies"; fail=1; return; fi
    # shellcheck disable=SC2086 # the flags are words
    $CLANG -g -O1 -std=gnu99 -D_GNU_SOURCE -fsanitize=fuzzer,address,undefined -I"$tmp/include" -o "$tmp/f" fuzz/fuzz_pin.c $crypto
    if "$tmp/f" -artifact_prefix="$tmp/" -runs=0 fuzz/corpus >"$tmp/log" 2>&1; then cat "$tmp/log" | tail -5; echo "FAIL $1: stays green"; fail=1; else echo "ok   $1 is red"; fi
}

arm "upper-case hex passes as a digest" "(s[n] >= 'a' && s[n] <= 'f') => ((s[n] | 32) >= 'a' && (s[n] | 32) <= 'f')"
arm "a digest of 65 digits passes" "return s[n] == '\\0'; => return 1;"
arm "an empty scheme passes" "colon == token || => "
arm "the pin is split at the first ':'" "strrchr(token, ':') => strchr(token, ':')"
arm "any word is a verdict" "return phd_pin_word_in(s, words, OMX_PLUGIN_DB_PIN_VERDICTS_COUNT); => return 1;"
arm "any word is a pool word" "return phd_pin_word_in(s, words, OMX_PLUGIN_DB_PIN_POOLS_COUNT); => return 1;"
[ "$fail" = 0 ] || exit 1
echo "ok   fuzz-sabotage: every arm is red"
