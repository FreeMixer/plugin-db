#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
#
# Each arm breaks one rule or one byte of the format in a copy of the header, and tests/pin_test.c must go red over it.
# An arm whose text is no longer in the header is itself a failure: a guard nothing reaches is no guard.
set -eu
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
crypto=$(pkg-config --cflags --libs libcrypto)
fail=0

arm() { # <what> <python expression over s, the header's text>
    rm -rf "$tmp/include"
    cp -r include "$tmp/include"
    if ! python3 - "$tmp/include/plugin-db/pin.h" "$2" <<'PY'
import sys
path, before_after = sys.argv[1:]
before, after = before_after.split(" => ")
s = open(path).read()
if before not in s:
    sys.exit("the text is gone: " + before)
open(path, "w").write(s.replace(before, after, 1))
PY
    then echo "FAIL $1: the arm no longer applies"; fail=1; return; fi
    # shellcheck disable=SC2086 # the flags are words
    cc -std=gnu99 -I"$tmp/include" -o "$tmp/t" tests/pin_test.c $crypto
    if "$tmp/t" >/dev/null 2>&1; then echo "FAIL $1: stays green"; fail=1; else echo "ok   $1 is red"; fi
}

arm "the scheme's name in the first line" '"omx-layout/1" => "omx-layout/2"'
arm "a tab in a name is not escaped" "*s == '\\t' ? \"\\\\t\" :  => "
arm "a number not declared is written as its bits" "if (!declared) => if (0)"
arm "the flags are not 8 hex digits" "phd_layout_uint(sink, ctx, p->flags, 1) => phd_layout_uint(sink, ctx, p->flags, 0)"
arm "an LV2 port's properties are not sorted" "qsort(props, p->nproperties, sizeof(*props), phd_layout_bytewise); => "
arm "the CLAP parameters are not sorted" "qsort(order, count, sizeof(*order), phd_layout_clap_order); => "
arm "the file digest starts where the descriptor is" "if (lseek(fd, 0, SEEK_SET) != 0) => if (0)"
arm "upper-case hex passes as a digest" "(s[n] >= 'a' && s[n] <= 'f') => ((s[n] | 32) >= 'a' && (s[n] | 32) <= 'f')"
arm "a digest of 65 digits passes" "return s[n] == '\\0'; => return 1;"
arm "any word is a verdict" "return phd_pin_word_in(s, words, OMX_PLUGIN_DB_PIN_VERDICTS_COUNT); => return 1;"
arm "any word is a pool word" "return phd_pin_word_in(s, words, OMX_PLUGIN_DB_PIN_POOLS_COUNT); => return 1;"

# the door is the one place that looks inside a plugin: this tree opens none
if grep -nE 'dlopen|dlsym|lilv|clap/clap\.h|lv2/' include/plugin-db/pin.h tests/pin_test.c tests/consumer.c; then
    echo "FAIL the pin reaches into a plugin"; fail=1
else
    echo "ok   nothing here loads or scans a plugin"
fi
[ "$fail" = 0 ] || exit 1
echo "ok   sabotage: every arm is red"
