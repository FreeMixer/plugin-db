#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
#
# Each arm breaks one rule of the state function, the bundle rule, the collection, the store or the derivation from
# data/ in a copy of the tree, and the tests over it must go red. An arm whose text is gone is itself a failure: a
# guard nothing reaches is no guard. The first arm is plugin-hostd's rule: of the bundles that hold a binary, the first
# in path order.
set -eu
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
fail=0

arm() { # <what> <file> <text => replacement> <make targets>
    rm -rf "$tmp/t"
    mkdir "$tmp/t"
    cp -r Makefile VERSION data include src tests tools "$tmp/t/"
    if ! python3 - "$tmp/t/$2" "$3" <<'PY'
import sys
path, before_after = sys.argv[1:]
before, after = before_after.split(" => ")
s = open(path).read()
if before not in s:
    sys.exit("the text is gone: " + before)
open(path, "w").write(s.replace(before, after, 1))
PY
    then echo "FAIL $1: the arm no longer applies"; fail=1; return; fi
    # shellcheck disable=SC2086 # the targets are words
    if make --no-print-directory -C "$tmp/t" $4 >"$tmp/log" 2>&1; then echo "FAIL $1: stays green"; fail=1
    else echo "ok   $1 is red"; fi
}

S=src/state.c C=src/collect.c T=src/store.c
# the bundle rule
arm "plugin-hostd's rule: of the bundles with a binary, the first in path order" $C "    if (a->minor != b->minor) => return 0;
    if (a->minor != b->minor)" test-collect
arm "a complement bundle ranks as one that realises" $C "    if (a->realises != b->realises)
        return a->realises > b->realises; => " test-collect
arm "the micro version is not read" $C "    return a->micro > b->micro; => return 0;" test-collect
arm "a later equal version wins" $C "    return a->micro > b->micro; => return a->micro >= b->micro;" test-collect
arm "the bundles of a directory in reverse byte order" $C "    return strcmp(*(const char *const *)a, *(const char *const *)b); => return strcmp(*(const char *const *)b, *(const char *const *)a);" test-collect
arm "the version a manifest gives is not read" src/manifest.c "                out->minor = version_of(&t); => ;" test-collect
# the collection
arm "a manifest's base is resolved as if none" $C "    if (m.based) => if (0)" test-collect
arm "a name that climbs out of the bundle is taken" $C "        else if (!pdb_path_valid(m.files.name[n])) => else if (0)" test-collect
arm "a CLAP id that is a path is an address" src/address.c "(a->format == PDB_ADDRESS_FORMATS_CLAP && a->id[0] == '/') => 0" test-collect
# the state function
arm "a shipped refusal is not read" $S "        in->reference.verdict == PDB_PIN_VERDICTS_REFUSED) => 0)" test-state
arm "a warned override is not read" $S "        if (in->warned_override) => if (0)" test-state
arm "isolated only without plugin-hostd" $S "    if (isolation_host(r->host_name)) => if (1)" test-state
arm "no cell is a cell" $S "    if (!cell_here(rec, here)) => if (0)" test-state
arm "a condition is not read" $S "    if (!conditions_here(rec, here)) => if (0)" test-state
arm "a stale local record gives way to a reference" $S "    else if (!in->local.present && in->reference.present) => else if (in->reference.present)" test-state
arm "absent is not first" $S "    if (in->files == PDB_FILES_ABSENT) => if (0)" test-state
arm "the rules are not compared" $S "        return differs(r->rules, h->rules); => return 0;" test-state
arm "the hosting epoch is not compared" $S "        return h->hosting_epoch < 0 || r->hosting_epoch < h->hosting_epoch; => return 0;" test-state
arm "a host release makes the record stale" $S "    return change < 0 || g_action_record[g_change_action[change]] == PDB_RECORD_CURRENT; => return change < 0;" test-state
arm "could not test passes" $S "        return in->files != PDB_FILES_SAME || !r->pin_files; => return in->files == PDB_FILES_CHANGED || !r->pin_files;" test-state
# the store and the check
arm "a file's digest is not compared" $T "        else if (strcmp(hex, want)) => else if (0)" test-store
arm "another bundle is the same plugin" $T "    if (strcmp(c.root, root)) => if (0)" test-store
arm "a record binds no layout" $T "    r->pin_layout = pinned->tree && => r->pin_layout = 1 ||" test-store
arm "a layer is written in place" $T "    if (rename(temp, path) != 0) => if (0)" test-store
arm "an entry is named by its address as written" $T "    if (phd_sha256_hex(address, strlen(address), hex) != 0) => snprintf(hex, sizeof(hex), \"%.64s\", address); if (0)" test-store
arm "a pin a CLAP address gives is not checked" $T "    if (a.format == PDB_ADDRESS_FORMATS_CLAP && a.pin[0] && strcmp(a.pin, files[0].sha256)) => if (0)" test-store
# the derivation from data/
arm "data/ changed and plugin_db_tables.h not rendered" data/db.json '"group": "rejudge", => "group": "convert",' check-tables
arm "a store key changed in the schema and not rendered" data/store.schema.json '"entries": "entries", => "entries": "entry",' check-tables
arm "plugin_db_tables.h changed by hand" include/plugin-db/plugin_db_tables.h "#define PDB_STATES_GROUP_INIT { 1, 0, => #define PDB_STATES_GROUP_INIT { 0, 0," check-tables

# plugin-db looks inside no plugin: it opens manifests as text and hashes files, and loads nothing
if grep -nE 'dlopen|dlsym|lilv_|clap/clap\.h|#include <lv2/' src/*.c src/*.h include/plugin-db/db.h; then
    echo "FAIL plugin-db reaches into a plugin"; fail=1
else
    echo "ok   nothing in src/ loads a plugin"
fi
[ "$fail" = 0 ] || exit 1
echo "ok   db sabotage: every arm is red"
