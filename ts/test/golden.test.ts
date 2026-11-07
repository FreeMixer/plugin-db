// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
// The twin against tests/golden/*.json: the same files tests/golden.py holds `plugin-db query --json` to. Each is a
// plugin in one of the states of §4.2 (and the way to it), and the answer the one reader gives.
import { test } from "node:test";
import assert from "node:assert/strict";
import { readdirSync, readFileSync } from "node:fs";
import { read, type Layer, type Entry } from "../src/reader.ts";
import { states } from "../src/vocabulary.ts";

const dir = new URL("../../tests/golden/", import.meta.url);
const FILES = "a".repeat(64), LAYOUT = "omx-layout/1:" + "a".repeat(64), OTHER_LAYOUT = "omx-layout/1:" + "b".repeat(64);
const OTHER_FILES = "e".repeat(64);

function entryOf(fx: any): Entry {
  const pin = (spec: { pin: string }) => ({
    files: spec.pin === "files" ? OTHER_FILES : FILES,
    layout: spec.pin === "layout" ? OTHER_LAYOUT : fx.pinnedLayout ? LAYOUT : null,
  });
  const layer = (name: string): Layer | null => (fx.layers[name] ? { pin: pin(fx.layers[name]), body: fx.layers[name].body } : null);
  return {
    files: fx.files,
    currentLayout: fx.currentLayout === "match" ? LAYOUT : fx.currentLayout === "other" ? OTHER_LAYOUT : null,
    pinned: fx.pinned ? { pin: { files: FILES, layout: fx.pinnedLayout ? LAYOUT : null }, body: {} } : null,
    judged: layer("judged"),
    reference: layer("reference"),
    warnedOverride: layer("warned-override"),
  };
}

const fixtures = readdirSync(dir).filter((n) => n.endsWith(".json")).sort();

test("there are fixtures, and the state function's six states are among them", () => {
  assert.ok(fixtures.length >= 20);
  const reached = new Set(fixtures.map((n) => JSON.parse(readFileSync(new URL(n, dir), "utf8")).expect.state));
  for (const state of Object.values(states)) assert.ok(reached.has(state), `no fixture reaches the state ${state}`);
});

for (const name of fixtures) {
  const fx = JSON.parse(readFileSync(new URL(name, dir), "utf8"));
  test(`the twin equals the fixture: ${fx.name}`, () => {
    assert.deepEqual(read(entryOf(fx), fx.here), fx.expect);
  });
}

test("one byte changed makes the twin say unqualified", () => {
  const fx = JSON.parse(readFileSync(new URL("qualified.json", dir), "utf8"));
  assert.equal(read(entryOf(fx), fx.here).state, states.qualified);
  assert.equal(read({ ...entryOf(fx), files: "changed" }, fx.here).state, states.unqualified);
});
