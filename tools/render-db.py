#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
"""Derive include/plugin-db/plugin_db_tables.h from data/pin.json, data/db.json and data/store.schema.json.

The words themselves are omx-contract's render (plugin_db_vocabulary.h); this adds what that render does not carry:
each word's index as a C enum, the group each word of a grouped item falls in, the store layout of
x-plugin-db-store, the envelope's version and every key name the schema declares. Nothing here is typed twice.

    render-db.py [--check] <data dir> <out header>
"""
import json
import re
import sys


def snake(name):
    return re.sub(r"(?<=[a-z0-9])([A-Z])", r"_\1", name).replace("-", "_").upper()


def short(item):
    return item[len("PLUGIN_DB_"):] if item.startswith("PLUGIN_DB_") else item


def keys_of(node, out):
    if isinstance(node, dict):
        for k, v in node.items():
            if k == "properties" and isinstance(v, dict):
                out.update(v.keys())
            keys_of(v, out)
    elif isinstance(node, list):
        for v in node:
            keys_of(v, out)


def render(data_dir):
    db = {}
    for name in ("pin.json", "db.json"):
        db.update(json.load(open(f"{data_dir}/{name}")))
    schema = json.load(open(f"{data_dir}/store.schema.json"))
    out = [
        "// SPDX-License-Identifier: GPL-3.0-or-later",
        "// Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>",
        "#ifndef PLUGIN_DB_TABLES_H",
        "#define PLUGIN_DB_TABLES_H",
        "/*",
        " * GENERATED: DO NOT EDIT BY HAND.",
        " * Produced by tools/render-db.py from data/pin.json, data/db.json and data/store.schema.json. Each words item is an",
        " * enum of its words' indexes in their order, PDB_<ITEM>_<NAME>, the index into the item's _INIT of",
        " * plugin_db_vocabulary.h; a grouped item has a _GROUP_INIT, the index of each word's group in the item it names.",
        " * The store layout is x-plugin-db-store's, PDB_STORE_<NAME>, and every key the schema declares is PDB_KEY_<NAME>.",
        " */",
        "",
        "#include <plugin-db/plugin_db_vocabulary.h>",
        "",
    ]
    index = {}
    for item, body in db.items():
        if not isinstance(body, dict) or body.get("kind") != "words":
            continue
        names = list(body["words"].keys())
        index[item] = {w["word"]: n for n, w in enumerate(body["words"].values())}
        out.append(f"/* {item} */")
        out.append("enum {")
        for n, name in enumerate(names):
            out.append(f"    PDB_{short(item)}_{snake(name)} = {n},")
        out.append("};")
        out.append(f"#define PDB_{short(item)}_COUNT {len(names)}")
        out.append("")
    for item, body in db.items():
        if not isinstance(body, dict) or "groups" not in body:
            continue
        group = body["groups"]
        if group not in index:
            sys.exit(f"render-db.py: {item} groups {group}, which is not a words item of data/")
        groups = []
        for name, w in body["words"].items():
            if w.get("group") not in index[group]:
                sys.exit(f"render-db.py: {item}.{name}: its group {w.get('group')!r} is no word of {group}")
            groups.append(str(index[group][w["group"]]))
        out.append(f"/* the {group} index of each word of {item} */")
        out.append(f"#define PDB_{short(item)}_GROUP_INIT {{ {', '.join(groups)} }}")
        out.append("")
    store = schema["x-plugin-db-store"]
    out.append("/* the store's layout (data/store.schema.json, x-plugin-db-store) */")
    for k, v in store.items():
        out.append(f"#define PDB_STORE_{snake(k)} {json.dumps(v)}")
    out.append(f"#define PDB_ENVELOPE_VERSION {schema['properties']['envelope']['const']}")
    out.append("")
    keys = set()
    keys_of(schema, keys)
    out.append("/* every key the schema declares */")
    for k in sorted(keys, key=snake):
        out.append(f"#define PDB_KEY_{snake(k)} {json.dumps(k)}")
    out.append("")
    for name, node in (("ENVELOPE", schema), ("PIN", schema["properties"]["pin"]),
                       ("PINNED_BODY", schema["$defs"]["pinnedBody"]),
                       ("RECORD", schema["$defs"]["recordFields"]),
                       ("RECORD_HOST", schema["$defs"]["recordFields"]["properties"]["host"])):
        req = node["required"]
        out.append(f"#define PDB_{name}_REQUIRED_COUNT {len(req)}")
        out.append(f"#define PDB_{name}_REQUIRED_INIT {{ {', '.join(json.dumps(r) for r in req)} }}")
    out.append("")
    out.append("#endif /* PLUGIN_DB_TABLES_H */")
    return "\n".join(out) + "\n"


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
            sys.exit(f"render-db.py: {args[1]} is not what data/ renders: run make render-tables")
        print(f"ok   {args[1]} is what data/ renders")
        return
    open(args[1], "w").write(text)


if __name__ == "__main__":
    main(sys.argv[1:])
