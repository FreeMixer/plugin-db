// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
// The TypeScript twin of plugin-db's reader (src/state.c and the check of src/store.c): the entry's layers and what a
// check of its files found, here, to the one answer of the plugin-db spec §4.2 and §5: state, run, reason, source, the
// record's tier and the figure at this rate and quantum. Nothing here is a word: every word is the vocabulary's,
// generated from data/, and tests/golden/ holds this file equal to the C reader and to `plugin-db query --json`.
// Neither this nor a surface computes a tier, a state or an RT verdict of its own.
import * as v from "./vocabulary.ts";

type Word<T> = T[keyof T];
export type Files = Word<typeof v.files>;
export type Layout = Word<typeof v.layout>;

/** a layer file of the store: the pin it was written for, and its body */
export interface Layer {
  pin: { files: string; layout: string | null };
  body: Record<string, unknown>;
}

/** what the reader is given: the check's findings about the files (the hashing is the C library's) and the layers */
export interface Entry {
  files: Files;
  currentLayout: string | null;
  pinned: Layer | null;
  judged: Layer | null;
  reference: Layer | null;
  warnedOverride: Layer | null;
}

/** the facts of here a record is compared with; absent or null is not known here, which no record matches */
export interface Here {
  machineClass?: string | null;
  hostName?: string | null;
  hostVersion?: string | null;
  hostingEpoch?: number | null;
  rules?: string | null;
  recordSchema?: string | null;
  rate?: number | null;
  quantum?: number | null;
  facts?: readonly string[];
}

export interface Figures {
  rate: number;
  quantum: number;
  latencyMs: number | null;
  lineMs: number | null;
  standing: Word<typeof v.latency>;
}

export interface Reading {
  files: Files;
  layout: Layout;
  record: Word<typeof v.record>;
  change: Word<typeof v.recordChanges> | null;
  action: Word<typeof v.recordActions> | null;
  state: Word<typeof v.states>;
  runs: Word<typeof v.runs>;
  reason: Word<typeof v.reasons>;
  source: Word<typeof v.sources>;
  tier: Word<typeof v.pinVerdicts> | null;
  figures: Figures | null;
}

interface Rec {
  present: boolean;
  pinFiles: boolean;
  pinLayout: boolean;
  verdict: string | null;
  machineClass: string | null;
  hostName: string | null;
  hostVersion: string | null;
  hostingEpoch: number;
  rules: string | null;
  recordSchema: string | null;
  cells: { rate: number; quantum: number; latencyMs: number | null }[];
  conditions: string[];
  lineMs: number | null;
}

const isObject = (x: unknown): x is Record<string, unknown> => typeof x === "object" && x !== null && !Array.isArray(x);
const str = (x: unknown): string | null => (typeof x === "string" ? x : null);
const num = (x: unknown): number | null => (typeof x === "number" && Number.isFinite(x) ? x : null);

const none: Rec = {
  present: false, pinFiles: false, pinLayout: false, verdict: null, machineClass: null, hostName: null, hostVersion: null,
  hostingEpoch: -1, rules: null, recordSchema: null, cells: [], conditions: [], lineMs: null,
};

/** a layer that does not carry the fields of a record, or whose verdict is no word of the vocabulary, is no record */
function valid(body: Record<string, unknown>): boolean {
  const host = body.host;
  return v.recordRequired.every((k) => k in body) && isObject(host) && v.recordHostRequired.every((k) => k in host) &&
    Object.values<string>(v.pinVerdicts).includes(body.verdict as string);
}

/** a record from its layer, against the pinned layer's pin */
function load(l: Layer | null, pinned: Layer | null): Rec {
  if (!l || !valid(l.body)) return none;
  const body = l.body, host = isObject(body.host) ? body.host : {};
  const cells: Rec["cells"] = [];
  if (Array.isArray(body.cells))
    for (const c of body.cells) {
      const rate = isObject(c) ? num(c.rate) : null, quantum = isObject(c) ? num(c.quantum) : null;
      if (rate !== null && quantum !== null) cells.push({ rate: Math.trunc(rate), quantum: Math.trunc(quantum), latencyMs: num((c as any).latencyMs) });
    }
  // conditions that cannot be read are conditions no fact here meets
  const conditions = isObject(body.conditions) ? Object.entries(body.conditions).map(([k, x]) => `${k}=${str(x) ?? ""}`) : [""];
  const epoch = num(host.hostingEpoch);
  return {
    present: true,
    pinFiles: pinned !== null && l.pin.files === pinned.pin.files,
    pinLayout: pinned !== null && (l.pin.layout !== null ? pinned.pin.layout !== null && l.pin.layout === pinned.pin.layout : pinned.pin.layout === null),
    verdict: str(body.verdict),
    machineClass: str(body.machineClass),
    hostName: str(host.name),
    hostVersion: str(host.version),
    hostingEpoch: epoch === null ? -1 : Math.trunc(epoch),
    rules: str(body.rules),
    recordSchema: str(body.recordSchema),
    cells,
    conditions,
    lineMs: num(body.latencyLineMs),
  };
}

/** a fact a record names and here does not know differs */
const differs = (a: string | null | undefined, b: string | null | undefined) => a == null || b == null || a !== b;

type Inputs = { files: Files; layout: Layout };

/** whether one row of the changes table holds for the record */
function changed(change: string, inp: Inputs, r: Rec, h: Here): boolean {
  const epoch = h.hostingEpoch ?? -1;
  switch (change) {
    case v.recordChanges.files: return inp.files !== v.files.same || !r.pinFiles;
    case v.recordChanges.layout: return inp.layout !== v.layout.same || !r.pinLayout;
    case v.recordChanges.hostingChange: return epoch < 0 || r.hostingEpoch < epoch;
    case v.recordChanges.machineClass: return differs(r.machineClass, h.machineClass);
    case v.recordChanges.schema: return differs(r.recordSchema, h.recordSchema);
    case v.recordChanges.rules: return differs(r.rules, h.rules);
    case v.recordChanges.hostVersion: return differs(r.hostName, h.hostName) || differs(r.hostVersion, h.hostVersion);
  }
  return true; // a change with no test here is never passed
}

/** the first change, in the changes table's order, since the record was written */
function changeOf(inp: Inputs, r: Rec, h: Here): Word<typeof v.recordChanges> | null {
  return Object.values(v.recordChanges).find((c) => changed(c, inp, r, h)) ?? null;
}

const current = (change: Word<typeof v.recordChanges> | null) =>
  change === null || v.recordActionsGroup[v.recordChangesGroup[change]] === v.record.current;

const cellHere = (r: Rec, h: Here) =>
  r.cells.find((c) => (h.rate ?? -1) > 0 && (h.quantum ?? -1) > 0 && c.rate === h.rate && c.quantum === h.quantum);

const conditionsHere = (r: Rec, h: Here) => r.conditions.every((c) => (h.facts ?? []).includes(c));

const isolationHost = (host: string | null) => host !== null && (Object.values(v.isolationHosts) as string[]).includes(host);

interface Answer { state: Reading["state"]; runs: Reading["runs"]; reason: Reading["reason"]; source: Reading["source"] }

const answer = (state: Answer["state"], reason: Answer["reason"], source: Answer["source"]): Answer =>
  ({ state, runs: v.statesGroup[state], reason, source });

/** alone, if it may: isolated only needs a measurement under a host of the isolation hosts */
const alone = (r: Rec, reason: Answer["reason"], source: Answer["source"]) =>
  isolationHost(r.hostName) ? answer(v.states.isolatedOnly, reason, source)
    : answer(v.states.unqualified, v.reasons.notMeasuredUnderHost, v.sources.none);

/** THE state function, as src/state.c has it */
function state(inp: Inputs & { pinned: boolean; local: Rec; reference: Rec; warnedOverride: boolean }, h: Here):
  { record: Reading["record"]; change: Reading["change"]; a: Answer; rec: Rec | null } {
  let change: Reading["change"] = null;
  let record: Reading["record"] = v.record.none;
  if (inp.local.present) {
    change = changeOf(inp, inp.local, h);
    record = current(change) ? v.record.current : v.record.stale;
  }
  const out = (a: Answer, rec: Rec | null = null) => ({ record, change, a, rec });

  if (inp.files === v.files.absent) return out(answer(v.states.absent, v.reasons.filesGone, v.sources.none));
  // a shipped refusal of these bytes wins over everything, a warned override included
  if (inp.reference.present && inp.files === v.files.same && inp.reference.pinFiles && inp.reference.verdict === v.pinVerdicts.refused)
    return out(answer(v.states.refused, v.reasons.shippedRefusal, v.sources.reference));

  let rec: Rec | null = null, source: Answer["source"] = v.sources.none;
  if (record === v.record.current) { rec = inp.local; source = v.sources.local; }
  else if (!inp.local.present && inp.reference.present) {
    // a local measurement wins; with none, a shipped record of these bytes answers on its class of machine and gives
    // reference figures only on another
    const c = changeOf(inp, inp.reference, h);
    if (current(c)) { rec = inp.reference; source = v.sources.reference; }
    else if (c === v.recordChanges.machineClass) return out(alone(inp.reference, v.reasons.referenceOnly, v.sources.reference), inp.reference);
  }

  if (!rec) {
    const reason = !inp.pinned ? v.reasons.neverPinned
      : inp.files === v.files.couldNotTest ? v.reasons.couldNotTest
      : inp.files === v.files.changed ? v.reasons.filesChanged
      : inp.layout === v.layout.changed ? v.reasons.layoutChanged
      : inp.layout === v.layout.notYetKnown ? v.reasons.layoutNotYetKnown
      : record === v.record.stale ? v.reasons.recordStale
      : v.reasons.measurementPending;
    return out(answer(v.states.unqualified, reason, v.sources.none));
  }
  if (rec.verdict === v.pinVerdicts.refused) {
    const a = answer(v.states.refused, v.reasons.recordRefused, source);
    if (inp.warnedOverride) a.runs = v.runs.alone;
    return out(a, rec);
  }
  if (rec.verdict !== v.pinVerdicts.inProcess && rec.verdict !== v.pinVerdicts.isolated)
    return out(answer(v.states.unqualified, v.reasons.couldNotTest, v.sources.none));
  if (!cellHere(rec, h)) return out(answer(v.states.conditionsUnmet, v.reasons.noCell, source), rec);
  if (!conditionsHere(rec, h)) return out(answer(v.states.conditionsUnmet, v.reasons.conditionFails, source), rec);
  if (rec.verdict === v.pinVerdicts.isolated) return out(alone(rec, v.reasons.verdictIsolated, source), rec);
  return out(answer(v.states.qualified, v.reasons.verdictAllows, source), rec);
}

/** where a figure stands against the line: unmeasured when either is missing, never a guess */
export function standing(latencyMs: number | null, lineMs: number | null): Figures["standing"] {
  if (latencyMs === null || lineMs === null) return v.latency.unmeasured;
  return latencyMs > lineMs ? v.latency.over : v.latency.within;
}

/** the entry's one answer, here */
export function read(entry: Entry, here: Here): Reading {
  const pinned = entry.pinned;
  const files = pinned ? entry.files : v.files.couldNotTest;
  const layout: Layout = !pinned || pinned.pin.layout === null ? v.layout.notYetKnown
    : entry.currentLayout !== null ? (entry.currentLayout !== pinned.pin.layout ? v.layout.changed : v.layout.same)
    : files === v.files.same ? v.layout.same : v.layout.notYetKnown;
  const local = load(entry.judged, pinned), reference = load(entry.reference, pinned);
  const warnedOverride = entry.warnedOverride !== null && pinned !== null && files === v.files.same &&
    entry.warnedOverride.pin.files === pinned.pin.files;
  const { record, change, a, rec } = state({ files, layout, pinned: pinned !== null, local, reference, warnedOverride }, here);

  // the tier is the answering record's verdict when it says where the plugin runs; a refusal has none
  const answering = a.source === v.sources.reference ? reference : a.source === v.sources.local ? local : none;
  const tier = (answering.verdict === v.pinVerdicts.inProcess || answering.verdict === v.pinVerdicts.isolated) ? answering.verdict : null;
  const cell = tier ? cellHere(answering, here) : undefined;
  return {
    files, layout, record,
    change,
    action: change === null ? null : v.recordChangesGroup[change],
    state: a.state, runs: a.runs, reason: a.reason, source: a.source,
    tier,
    figures: cell ? { rate: cell.rate, quantum: cell.quantum, latencyMs: cell.latencyMs, lineMs: answering.lineMs, standing: standing(cell.latencyMs, answering.lineMs) } : null,
  };
}
