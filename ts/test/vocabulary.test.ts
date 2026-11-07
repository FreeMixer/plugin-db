// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
// The vocabulary the twin reads is the one data/ declares, in its order, and the bar and the latency standings are
// declared once, here, for plugin-hostd and both surfaces to read.
import { test } from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import * as v from "../src/vocabulary.ts";

const data = { ...JSON.parse(readFileSync(new URL("../../data/pin.json", import.meta.url), "utf8")),
               ...JSON.parse(readFileSync(new URL("../../data/db.json", import.meta.url), "utf8")) };
const words = (item: string) => Object.values<any>(data[item].words).map((w) => w.word);

test("each words item of data/ is in the vocabulary, its words in their order", () => {
  assert.deepEqual(Object.values(v.states), words("PLUGIN_DB_STATES"));
  assert.deepEqual(Object.values(v.runs), words("PLUGIN_DB_RUNS"));
  assert.deepEqual(Object.values(v.reasons), words("PLUGIN_DB_REASONS"));
  assert.deepEqual(Object.values(v.pinVerdicts), words("PLUGIN_DB_PIN_VERDICTS"));
});

test("the bar's three words are declared once", () => {
  assert.deepEqual(Object.values(v.bar), ["qualified-only", "warn-on-crashes", "load-all"]);
});

test("the latency standings are within, over and unmeasured", () => {
  assert.deepEqual(Object.values(v.latency), ["within", "over", "unmeasured"]);
});

test("a state runs by its group", () => {
  assert.equal(v.statesGroup[v.states.unqualified], v.runs.alone);
  assert.equal(v.statesGroup[v.states.qualified], v.runs.shared);
  assert.equal(v.statesGroup[v.states.refused], v.runs.notLoaded);
});
