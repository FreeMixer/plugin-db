#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
"""The C reader's side of the golden fixtures: for each tests/golden/*.json a store is built through the command (an
LV2 bundle pinned, the layers put, the files changed or removed as the fixture says), `plugin-db query --json` is
asked, and its answer must equal the fixture's expect. ts/test/golden.test.ts holds the TypeScript twin to the same
files, so the twins are held equal by the files and by nothing else.

    golden.py <plugin-db command> <golden dir>
"""
import json, os, shutil, subprocess, sys, tempfile

AMP = "http://example.org/plugins/amp"
LAYOUT = "omx-layout/1:" + "a" * 64
OTHER_LAYOUT = "omx-layout/1:" + "b" * 64
OTHER_FILES = "e" * 64


def run(pdb, store, *args, stdin=None):
    return subprocess.run([pdb, "--store", store, *args], input=stdin, capture_output=True, text=True)


def here_args(h):
    a = ["--machine-class", h["machineClass"], "--host", h["hostName"], "--host-version", h["hostVersion"],
         "--hosting-epoch", str(h["hostingEpoch"]), "--rules", h["rules"], "--record-schema", h["recordSchema"],
         "--rate", str(h["rate"]), "--quantum", str(h["quantum"])]
    for f in h["facts"]:
        a += ["--fact", f]
    return a


def one(pdb, fx):
    tmp = tempfile.mkdtemp()
    try:
        store, lv2 = f"{tmp}/store", f"{tmp}/lv2"
        bundle = f"{lv2}/amp.lv2"
        os.makedirs(bundle)
        open(f"{bundle}/manifest.ttl", "w").write(
            "@prefix lv2: <http://lv2plug.in/ns/lv2core#> .\n@prefix rdfs: <http://www.w3.org/2000/01/rdf-schema#> .\n"
            f"<{AMP}> a lv2:Plugin ; lv2:binary <amp.so> ; lv2:minorVersion 1 ; rdfs:seeAlso <amp.ttl> .\n")
        open(f"{bundle}/amp.so", "w").write("code 1")
        open(f"{bundle}/amp.ttl", "w").write("ports")
        address = f"lv2:{AMP}"
        pin = ["pin", address, "--lv2-path", lv2] + (["--layout", LAYOUT] if fx["pinnedLayout"] else [])
        r = run(pdb, store, *pin)
        if r.returncode:
            return f"pin: {r.stderr}"
        for layer, spec in fx["layers"].items():
            r = run(pdb, store, "put", address, layer, "--writer", "golden", stdin=json.dumps(spec["body"]))
            if r.returncode:
                return f"put {layer}: {r.stderr}"
            if spec["pin"] != "current":
                entries = f"{store}/entries"
                (entry,) = os.listdir(entries)
                path = f"{entries}/{entry}/{layer}.json"
                doc = json.load(open(path))
                if spec["pin"] == "files":
                    doc["pin"]["files"] = OTHER_FILES
                else:
                    doc["pin"]["layout"] = OTHER_LAYOUT
                json.dump(doc, open(path, "w"))
        if not fx["pinned"]:
            (entry,) = os.listdir(f"{store}/entries")
            os.remove(f"{store}/entries/{entry}/pinned.json")
        if fx["files"] == "changed":
            open(f"{bundle}/amp.so", "w").write("code 2")
        elif fx["files"] == "absent":
            shutil.rmtree(bundle)
        q = ["query", "--json", address]
        q += here_args(fx["here"])
        if fx["currentLayout"] != "none":
            q += ["--layout", LAYOUT if fx["currentLayout"] == "match" else OTHER_LAYOUT]
        r = run(pdb, store, *q)
        if r.returncode:
            return f"query exits {r.returncode}: {r.stderr}"
        got = json.loads(r.stdout)
        got.pop("address", None)
        if got != fx["expect"]:
            return "query --json says\n  %s\nthe fixture says\n  %s" % (json.dumps(got, sort_keys=True), json.dumps(fx["expect"], sort_keys=True))
        return None
    finally:
        shutil.rmtree(tmp)


def main(argv):
    pdb, d = argv
    pdb = os.path.abspath(pdb)
    bad = 0
    names = sorted(n for n in os.listdir(d) if n.endswith(".json"))
    if not names:
        sys.exit("golden.py: no fixture in " + d)
    for n in names:
        fx = json.load(open(f"{d}/{n}"))
        if not fx["cli"]:
            continue
        err = one(pdb, fx)
        print(("FAIL " if err else "ok   ") + "query --json equals the fixture: " + fx["name"])
        if err:
            print(err)
            bad += 1
    sys.exit(1 if bad else 0)


main(sys.argv[1:])
