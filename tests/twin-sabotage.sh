#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
#
# Each arm breaks one rule of the TypeScript twin, one word of its generated vocabulary, or the conformance of the
# sources, in a copy of the tree, and the tests over it must go red. An arm whose text is gone is itself a failure: a
# guard nothing reaches is no guard.
set -eu
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
fail=0
TWIN='cd ts && node --test "test/*.test.ts"'

arm() { # <what> <file> <text => replacement | append: text> <command, default the twin's tests>
    rm -rf "$tmp/t"
    mkdir "$tmp/t"
    cp -r Makefile VERSION data include src tests tools ts "$tmp/t/"
    if ! python3 - "$tmp/t/$2" "$3" <<'PY'
import sys
path, spec = sys.argv[1:]
s = open(path).read()
if spec.startswith("append: "):
    open(path, "w").write(s + spec[len("append: "):] + "\n")
    sys.exit(0)
before, after = spec.split(" => ")
if before not in s:
    sys.exit("the text is gone: " + before)
open(path, "w").write(s.replace(before, after, 1))
PY
    then echo "FAIL $1: the arm no longer applies"; fail=1; return; fi
    if (cd "$tmp/t" && sh -c "${4:-$TWIN}") >"$tmp/log" 2>&1; then echo "FAIL $1: stays green"; fail=1
    else echo "ok   $1 is red"; fi
}

R=ts/src/reader.ts V=ts/src/vocabulary.ts
arm "a shipped refusal is not read" $R "&& inp.reference.verdict === v.pinVerdicts.refused) => && false)"
arm "a warned override is not read" $R "if (inp.warnedOverride) a.runs = v.runs.alone; => if (false) a.runs = v.runs.alone;"
arm "a figure at the line is over it" $R "return latencyMs > lineMs ? => return latencyMs >= lineMs ?"
arm "a missing line is guessed" $R "if (latencyMs === null || lineMs === null) => if (latencyMs === null)"
arm "a refusal has a tier" $R "answering.verdict === v.pinVerdicts.inProcess || answering.verdict === v.pinVerdicts.isolated => answering.verdict !== null"
arm "the rules are not compared" $R "return differs(r.rules, h.rules); => return false;"
arm "absent is not first" $R "if (inp.files === v.files.absent) return out( => if (false) return out("
arm "the figure is the cell of any quantum" $R "c.quantum === h.quantum => true"
arm "a record of other bytes is trusted" $R "pinned !== null && l.pin.files === pinned.pin.files => true"
arm "isolated only without plugin-hostd" $R "isolationHost(r.hostName) ? => true ?"
arm "a run word renamed in the generated vocabulary" $V 'alone: "alone" => alone: "solo"'
arm "a reason renamed in the generated vocabulary" $V 'noCell: "no-cell" => noCell: "no-room"'
arm "a bar word renamed in the generated vocabulary" $V 'loadAll: "load-all" => loadAll: "load-every"'
arm "a vocabulary changed by hand is red to the render check" $V 'alone: "alone" => alone: "solo"' "make check-ts"
arm "a run word typed in the twin" $R 'append: const typed = "alone";'
arm "a tier word typed in the twin" $R "append: const typed = 'in-process';"
arm "a reason word typed in the C command" src/cli.c 'append: static const char *g_typed = "no-cell";'
arm "an owner's sentence typed in the twin" $R "append: const typed = \"the plugin's files are gone; show names the path that was pinned\";"
[ "$fail" = 0 ] || exit 1
echo "ok   twin sabotage: every arm is red"
