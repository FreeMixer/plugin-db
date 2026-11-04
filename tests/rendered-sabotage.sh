#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
#
# check-rendered must fail on a rendered header changed by hand, and on data/ changed and not rendered again.
set -eu
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
fresh() { rm -rf "$tmp/t"; mkdir "$tmp/t"; cp -r Makefile VERSION data include "$tmp/t/"; }

fresh
make --no-print-directory -C "$tmp/t" check-rendered >/dev/null || { echo "FAIL check-rendered is red on a clean tree"; exit 1; }
fresh
sed -i 's/"in-process"/"in_process"/' "$tmp/t/include/plugin-db/plugin_db_vocabulary.h"
if make --no-print-directory -C "$tmp/t" check-rendered >/dev/null 2>&1; then echo "FAIL a header changed by hand stays green"; exit 1; fi
fresh
sed -i 's/"word": "isolated"/"word": "alone"/' "$tmp/t/data/pin.json"
if make --no-print-directory -C "$tmp/t" check-rendered >/dev/null 2>&1; then echo "FAIL data/ not rendered again stays green"; exit 1; fi
echo "ok   rendered sabotage: a header changed by hand and data/ not rendered are red"
