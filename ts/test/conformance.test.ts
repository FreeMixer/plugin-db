// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
// No source of this repository types a state, run, reason, tier (a record's verdict), bar or latency-standing word, or
// a sentence of data/, as a string literal: they come from the generated vocabulary, and a word renamed in data/ turns
// the readers red instead of leaving a stale copy. The generated files, data/ and the tests are the places that name them.
import { test } from "node:test";
import assert from "node:assert/strict";
import { readdirSync, readFileSync, statSync } from "node:fs";
import { join } from "node:path";

const root = new URL("../../", import.meta.url).pathname;
const ITEMS = ["PLUGIN_DB_STATES", "PLUGIN_DB_RUNS", "PLUGIN_DB_REASONS", "PLUGIN_DB_PIN_VERDICTS", "PLUGIN_DB_BAR", "PLUGIN_DB_LATENCY"];

function vocabulary(): { words: Set<string>; sentences: Set<string> } {
  const words = new Set<string>(), sentences = new Set<string>();
  for (const f of ["db.json", "pin.json"]) {
    const data = JSON.parse(readFileSync(join(root, "data", f), "utf8"));
    for (const [item, body] of Object.entries<any>(data)) {
      if (!body || body.kind !== "words") continue;
      for (const w of Object.values<any>(body.words)) {
        if (ITEMS.includes(item)) words.add(w.word);
        if (w.doc) sentences.add(w.doc);
        for (const t of Object.values<string>(w.text ?? {})) sentences.add(t);
      }
    }
  }
  return { words, sentences };
}

const GENERATED = new Set(["ts/src/vocabulary.ts", "include/plugin-db/plugin_db_vocabulary.h", "include/plugin-db/plugin_db_tables.h"]);

function sources(): string[] {
  const out: string[] = [];
  const walk = (dir: string) => {
    for (const n of readdirSync(join(root, dir))) {
      const rel = join(dir, n);
      if (n === "node_modules" || n === "build" || n.startsWith(".")) continue;
      if (statSync(join(root, rel)).isDirectory()) walk(rel);
      else if (/\.(c|h|ts)$/.test(n) && !GENERATED.has(rel)) out.push(rel);
    }
  };
  for (const d of ["src", "include", "ts/src"]) walk(d);
  return out;
}

/** the string literals of a C or TypeScript source: "..." and '...' with escapes, comments left out */
export function literals(text: string): string[] {
  const out: string[] = [];
  const re = /\/\/[^\n]*|\/\*[\s\S]*?\*\/|"((?:[^"\\\n]|\\.)*)"|'((?:[^'\\\n]|\\.)*)'/g;
  for (let m; (m = re.exec(text)); ) if (m[1] !== undefined || m[2] !== undefined) out.push((m[1] ?? m[2]) as string);
  return out;
}

export function offenders(files: Record<string, string>, v = vocabulary()): string[] {
  const bad: string[] = [];
  for (const [file, text] of Object.entries(files))
    for (const lit of literals(text)) if (v.words.has(lit) || v.sentences.has(lit)) bad.push(`${file}: ${JSON.stringify(lit)}`);
  return bad;
}

test("the sources are found, and the vocabulary is read", () => {
  assert.ok(sources().includes("src/cli.c") && sources().includes("ts/src/reader.ts"));
  const v = vocabulary();
  for (const w of ["alone", "refused", "in-process", "no-cell", "within", "load-all"]) assert.ok(v.words.has(w), w);
});

test("no source types a state, run, reason, tier, bar or standing word, or a sentence of data/", () => {
  const files = Object.fromEntries(sources().map((f) => [f, readFileSync(join(root, f), "utf8")]));
  assert.deepEqual(offenders(files), []);
});

test("the check sees a typed word in C and in TypeScript, and passes the generated name", () => {
  assert.equal(offenders({ "a.c": 'if (!strcmp(run, "alone")) return 1;' }).length, 1);
  assert.equal(offenders({ "a.ts": 'const run = "in-process";' }).length, 1);
  assert.equal(offenders({ "a.ts": "const run = 'no-cell';" }).length, 1);
  assert.deepEqual(offenders({ "a.ts": 'const run = runs.alone; // "alone" in a comment\n' }), []);
});
