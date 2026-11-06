#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
#
# The store, show and verify through the command: an LV2 bundle and two CLAP files in a scratch tree, their entries
# pinned, records put, the files changed, moved and removed, and every answer read back. The layer files are held to
# data/store.schema.json by a reading of the schema itself.
set -eu
PDB=${PDB:-build/plugin-db}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
store=$tmp/store
fail=0
AMP=http://example.org/plugins/amp
LAYOUT=omx-layout/1:$(printf 'a%.0s' $(seq 64))
OTHER_LAYOUT=omx-layout/1:$(printf 'b%.0s' $(seq 64))
HERE="--machine-class rpi5 --host plugin-hostd --host-version 1.4.0 --hosting-epoch 2 --rules rules-7 --record-schema record-3 --rate 48000 --quantum 128"

ok() { echo "ok   $1"; }
bad() { echo "FAIL $1"; fail=1; }
pdb() { "$PDB" --store "$store" "$@"; }
# show's value of one attribute
attr() { # <attribute> <show args...>
    a=$1; shift
    pdb show "$@" | sed -n "s/^$a  *//p"
}
expect() { # <what> <attribute> <want> <show args...>
    what=$1 a=$2 want=$3; shift 3
    got=$(attr "$a" "$@" 2>&1) || true
    if [ "$got" = "$want" ]; then ok "$what: $a $want"; else bad "$what: $a is '$got', not '$want'"; fi
}
bundle() { # <dir> <minor>
    mkdir -p "$1"
    cat > "$1/manifest.ttl" <<EOF
@prefix lv2: <http://lv2plug.in/ns/lv2core#> .
@prefix rdfs: <http://www.w3.org/2000/01/rdf-schema#> .
<$AMP> a lv2:Plugin ; lv2:binary <amp.so> ; lv2:minorVersion $2 ; rdfs:seeAlso <amp.ttl> .
EOF
    printf 'code %s' "$2" > "$1/amp.so"
    printf 'ports' > "$1/amp.ttl"
}
record() { # <verdict> [<machine class>]
    printf '{"verdict": "%s", "machineClass": "%s", "host": {"name": "plugin-hostd", "version": "1.4.0", "hostingEpoch": 2}, "rules": "rules-7", "recordSchema": "record-3", "cells": [{"rate": 48000, "quantum": 128}], "conditions": {}}' "$1" "${2:-rpi5}"
}

bundle "$tmp/lv2/amp.lv2" 1
LV2PATH=$tmp/lv2:$tmp/lv2-new

# an entry is pinned from its files: before a layout is known, then with one
pdb pin "lv2:$AMP" --lv2-path "$LV2PATH"
expect "pinned, no layout" files same "lv2:$AMP" $HERE
expect "pinned, no layout" layout not-yet-known "lv2:$AMP" $HERE
expect "pinned, no layout" reason layout-not-yet-known "lv2:$AMP" $HERE
pdb pin "lv2:$AMP" --lv2-path "$LV2PATH" --layout "$LAYOUT"
expect "pinned" record none "lv2:$AMP" $HERE
expect "pinned" state unqualified "lv2:$AMP" $HERE
expect "pinned" reason measurement-pending "lv2:$AMP" $HERE

# the store's layout: the entry is named by the sha256 of its address, a layer by its word, no temporary file stays
entry=$(python3 -c 'import hashlib,sys; print(hashlib.sha256(sys.argv[1].encode()).hexdigest())' "lv2:$AMP")
if [ -f "$store/entries/$entry/pinned.json" ]; then ok "the entry is entries/<sha256 of the address>/pinned.json"
else bad "no entries/$entry/pinned.json"; fi
if ls -A "$store/entries/$entry" | grep -q '^\.tmp-'; then bad "a temporary file is left in the entry"; else ok "no temporary file is left"; fi

# a record is put for the current pin and answers
record in-process | pdb put "lv2:$AMP" judged --writer plugin-qualify
expect "a current record" record current "lv2:$AMP" $HERE
expect "a current record" state qualified "lv2:$AMP" $HERE
expect "a current record" runs shared "lv2:$AMP" $HERE
expect "a current record" source local "lv2:$AMP" $HERE
expect "a current record" verdict "in-process, measured under plugin-hostd 1.4.0 on rpi5" "lv2:$AMP" $HERE

# every layer file is the schema's: the envelope, the layer, the address, the pin and the body's required keys
if python3 - "$store/entries/$entry" data/store.schema.json "lv2:$AMP" <<'PY'
import json, os, re, sys
entry, schema_path, address = sys.argv[1:]
schema = json.load(open(schema_path))
defs = schema["$defs"]
sha = re.compile(defs["sha256"]["pattern"])
layout = re.compile(defs["layoutPin"]["pattern"])
bodies = {r["if"]["properties"]["layer"]["const"]: r["then"]["properties"]["body"]["$ref"].split("/")[-1] for r in schema["allOf"]}
for name in sorted(os.listdir(entry)):
    f = json.load(open(os.path.join(entry, name)))
    assert set(schema["required"]) <= set(f) <= set(schema["properties"]), (name, sorted(f))
    assert f["envelope"] == schema["properties"]["envelope"]["const"], name
    assert name == f["layer"] + schema["x-plugin-db-store"]["layerSuffix"], name
    assert f["address"] == address, name
    assert set(f["pin"]) == set(schema["properties"]["pin"]["required"]) and sha.match(f["pin"]["files"]), name
    assert f["pin"]["layout"] is None or layout.match(f["pin"]["layout"]), name
    body = defs.get(bodies.get(f["layer"], ""), {})
    assert set(body.get("required", [])) <= set(f["body"]), (name, body.get("required"))
    if f["layer"] == "pinned":
        assert set(f["body"]) <= set(body["properties"]), name
        for item in f["body"]["files"]:
            assert sha.match(item["sha256"]), name
print("ok   every layer file holds to data/store.schema.json")
PY
then :; else bad "a layer file is not the schema's"; fi

# 4.4 rows through the command: here changes, the record is judged again
expect "4.4 rules" change "rules (rejudge)" "lv2:$AMP" $HERE --rules rules-8
expect "4.4 rules" state unqualified "lv2:$AMP" $HERE --rules rules-8
expect "4.4 record schema" change "schema (convert)" "lv2:$AMP" $HERE --record-schema record-4
expect "4.4 machine class" change "machine-class (measure-here)" "lv2:$AMP" $HERE --machine-class x86-64-v3
expect "4.4 hosting change" change "hosting-change (full-path)" "lv2:$AMP" $HERE --hosting-epoch 3
expect "4.4 host normal release" change "host-version (remeasure-background)" "lv2:$AMP" $HERE --host-version 1.4.1
expect "4.4 host normal release" state qualified "lv2:$AMP" $HERE --host-version 1.4.1
expect "4.2 conditions unmet" state conditions-unmet "lv2:$AMP" $HERE --quantum 64
expect "4.2 conditions unmet" reason no-cell "lv2:$AMP" $HERE --quantum 64
expect "a layout a host found that is not the pinned one" layout changed "lv2:$AMP" $HERE --layout "$OTHER_LAYOUT"
expect "a layout a host found that is not the pinned one" reason layout-changed "lv2:$AMP" $HERE --layout "$OTHER_LAYOUT"
"$PDB" --store "$store" verify $HERE >/dev/null && ok "verify: a clean store exits 0" || bad "verify is red on a clean store"

# 4.4 plugin files: one byte changes
printf 'code 2' > "$tmp/lv2/amp.lv2/amp.so"
expect "4.4 plugin files" files changed "lv2:$AMP" $HERE
expect "4.4 plugin files" record stale "lv2:$AMP" $HERE
expect "4.4 plugin files" change "files (full-path)" "lv2:$AMP" $HERE
expect "4.4 plugin files" reason files-changed "lv2:$AMP" $HERE
expect "4.4 plugin files" problem "amp.so differs from its pin" "lv2:$AMP" $HERE
if out=$(pdb verify $HERE); then bad "verify exits 0 with a changed file"; else
    case $out in "files=changed layout=not-yet-known record=stale state=unqualified lv2:$AMP"*) ok "verify: a changed file is a line and exit 1" ;;
    *) bad "verify printed '$out'" ;; esac
fi
printf 'code 1' > "$tmp/lv2/amp.lv2/amp.so"
expect "the bytes back" state qualified "lv2:$AMP" $HERE

# the bundle rule at work: a newer version elsewhere in the search path is now the plugin
bundle "$tmp/lv2-new/amp-2.lv2" 2
expect "a newer bundle in the search path" files changed "lv2:$AMP" $HERE
expect "a newer bundle in the search path" problem "the plugin is now $tmp/lv2-new/amp-2.lv2; it was pinned in $tmp/lv2/amp.lv2" "lv2:$AMP" $HERE
rm -rf "$tmp/lv2-new"

# a refusal, and a warned override that runs it alone; the override does not survive new bytes
record refused | pdb put "lv2:$AMP" judged
expect "4.2 refused" state refused "lv2:$AMP" $HERE
expect "4.2 refused" runs not-loaded "lv2:$AMP" $HERE
echo '{}' | pdb put "lv2:$AMP" warned-override --writer override
expect "a warned override" runs alone "lv2:$AMP" $HERE
pdb pin "lv2:$AMP" --lv2-path "$LV2PATH" --layout "$OTHER_LAYOUT"
expect "a re-pin: the record was for another layout" change "layout (full-path)" "lv2:$AMP" $HERE
expect "a re-pin: the record was for another layout" reason record-stale "lv2:$AMP" $HERE
pdb pin "lv2:$AMP" --lv2-path "$LV2PATH" --layout "$LAYOUT"

# a shipped refusal wins over the warned override
record refused | pdb put "lv2:$AMP" reference --writer plugin-db-qualification
expect "4.5 a shipped refusal" reason shipped-refusal "lv2:$AMP" $HERE
expect "4.5 a shipped refusal" runs not-loaded "lv2:$AMP" $HERE
expect "4.5 a shipped refusal" source reference "lv2:$AMP" $HERE

# 4.2 absent: the files are gone
rm -rf "$tmp/lv2/amp.lv2"
expect "4.2 absent" files absent "lv2:$AMP" $HERE
expect "4.2 absent" state absent "lv2:$AMP" $HERE
expect "4.2 absent" runs not-loaded "lv2:$AMP" $HERE

# what is refused at the door
if record in-process | pdb put "lv2:http://example.org/plugins/none" judged 2>/dev/null; then bad "a record is put for no entry"; else ok "a record needs an entry"; fi
if echo '{}' | pdb put "lv2:$AMP" pinned 2>/dev/null; then bad "put writes the pinned layer"; else ok "the pinned layer is written from the files only"; fi
if echo '{"verdict": "fine"}' | pdb put "lv2:$AMP" judged 2>/dev/null; then bad "a record without the schema's fields is put"; else ok "a record is held to the schema's fields"; fi
if record maybe | pdb put "lv2:$AMP" judged 2>/dev/null; then bad "a verdict outside the words is put"; else ok "a verdict is a word of PLUGIN_DB_PIN_VERDICTS"; fi
if echo 'not json' | pdb put "lv2:$AMP" scanned 2>/dev/null; then bad "a body that is no JSON is put"; else ok "a body must be a JSON object"; fi
if pdb show "clap:/usr/lib/clap/synth.clap#com.example.synth" 2>/dev/null; then bad "a path is taken as an address"; else ok "a path is no address"; fi

# CLAP: no path in the address; two files of one id are told apart by their pin
mkdir -p "$tmp/clap/a" "$tmp/clap/b"
printf 'synth a' > "$tmp/clap/a/synth.clap"
printf 'synth b' > "$tmp/clap/b/synth.clap"
sha_a=$(sha256sum "$tmp/clap/a/synth.clap" | cut -d' ' -f1)
sha_b=$(sha256sum "$tmp/clap/b/synth.clap" | cut -d' ' -f1)
pdb pin "clap@$sha_a:com.example.synth" --file "$tmp/clap/a/synth.clap" --layout "$LAYOUT"
expect "one entry of a CLAP id" address "clap@$sha_a:com.example.synth" clap:com.example.synth $HERE
if pdb pin "clap@$sha_a:com.example.synth" --file "$tmp/clap/b/synth.clap" 2>/dev/null; then bad "a pin that is not the file's is taken"
else ok "an address's pin must be its file's"; fi
pdb pin "clap@$sha_b:com.example.synth" --file "$tmp/clap/b/synth.clap" --layout "$LAYOUT"
if err=$(pdb show clap:com.example.synth 2>&1); then bad "an id two files share answers for one"
else case $err in *"2 files share the CLAP id com.example.synth"*) ok "an id two files share needs the pin" ;; *) bad "said '$err'" ;; esac; fi
pdb pin clap:com.example.other --file "$tmp/clap/a/synth.clap"
if pdb pin clap:com.example.other --file "$tmp/clap/b/synth.clap" 2>/dev/null; then bad "a second file takes an id's entry"
else ok "a second file of an id is refused its entry"; fi
expect "a CLAP entry" files same "clap@$sha_b:com.example.synth" $HERE
rm "$tmp/clap/b/synth.clap"
expect "a CLAP file gone" state absent "clap@$sha_b:com.example.synth" $HERE

# verify reads every entry, in the order of their names, and an empty store is clean
n=$(pdb verify -v $HERE | wc -l)
[ "$n" = 4 ] && ok "verify -v prints every entry" || bad "verify -v printed $n lines, not 4"
"$PDB" --store "$tmp/empty" verify && ok "verify: an empty store is clean" || bad "verify is red on an empty store"

[ "$fail" = 0 ] || exit 1
echo "ok   store: pin, put, show and verify"
