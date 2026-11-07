#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
"""Derive ts/src/vocabulary.ts, the TypeScript twin's words, from data/pin.json and data/db.json.

Each words item becomes an object of its words in their order, keyed as data/ keys them (PLUGIN_DB_STATES is `states`),
and a grouped item also a <item>Group object: the group word of each word. When a word carries owners' `text` (en, ca)
it is folded into <item>Text. Nothing here is typed twice, and the twin's source types no word.

    render-ts.py [--check] <data dir> <out file>
"""
import json
import sys


def camel(item):
    parts = (item[len("PLUGIN_DB_"):] if item.startswith("PLUGIN_DB_") else item).lower().split("_")
    return parts[0] + "".join(p.capitalize() for p in parts[1:])


def render(data_dir):
    db = {}
    for name in ("pin.json", "db.json"):
        db.update(json.load(open(f"{data_dir}/{name}")))
    out = [
        "// SPDX-License-Identifier: GPL-3.0-or-later",
        "// Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>",
        "// GENERATED: DO NOT EDIT BY HAND.",
        "// Produced by tools/render-ts.py from data/pin.json and data/db.json. Each words item is an object of its words in",
        "// their order, keyed as data/ keys them; a grouped item has a <item>Group object (the group word of each word).",
        "",
    ]
    for item, body in db.items():
        if not isinstance(body, dict) or body.get("kind") != "words":
            continue
        name = camel(item)
        out.append(f"/** {item} */")
        out.append(f"export const {name} = {{")
        for key, w in body["words"].items():
            out.append(f"  {key}: {json.dumps(w['word'])},")
        out.append("} as const;")
        texts = {w["word"]: w["text"] for w in body["words"].values() if "text" in w}
        if texts:
            out.append(f"/** the owners' text of each word of {item}, en and ca */")
            out.append(f"export const {name}Text: Readonly<Record<string, Readonly<Record<string, string>>>> = {json.dumps(texts, ensure_ascii=False)};")
        out.append("")
    # every group after every item: a group names words of an item that may come later in data/
    for item, body in db.items():
        if not isinstance(body, dict) or body.get("kind") != "words" or "groups" not in body:
            continue
        name = camel(item)
        out.append(f"/** the {body['groups']} word of each word of {item} */")
        out.append(f"export const {name}Group = {{")
        for key, w in body["words"].items():
            out.append(f"  [{name}.{key}]: {camel(body['groups'])}.{next(k for k, g in db[body['groups']]['words'].items() if g['word'] == w['group'])},")
        out.append("} as const;")
        out.append("")
    schema = json.load(open(f"{data_dir}/store.schema.json"))["$defs"]["recordFields"]
    out.append("/** the keys a record must carry (data/store.schema.json, recordFields) and its host's */")
    out.append(f"export const recordRequired = {json.dumps(schema['required'])} as const;")
    out.append(f"export const recordHostRequired = {json.dumps(schema['properties']['host']['required'])} as const;")
    out.append("")
    return "\n".join(out)


def main(argv):
    check = "--check" in argv
    args = [a for a in argv if a != "--check"]
    if len(args) != 2:
        sys.exit(__doc__)
    text = render(args[0])
    if check:
        try:
            have = open(args[1]).read()
        except OSError:
            have = None
        if have != text:
            sys.exit(f"render-ts.py: {args[1]} is not what data/ renders: run make render-ts")
        return
    open(args[1], "w").write(text)


main(sys.argv[1:])
