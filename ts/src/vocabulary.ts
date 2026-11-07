// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
// GENERATED: DO NOT EDIT BY HAND.
// Produced by tools/render-ts.py from data/pin.json and data/db.json. Each words item is an object of its words in
// their order, keyed as data/ keys them; a grouped item has a <item>Group object (the group word of each word).

/** PLUGIN_DB_PIN_VERDICTS */
export const pinVerdicts = {
  inProcess: "in-process",
  isolated: "isolated",
  refused: "refused",
} as const;

/** PLUGIN_DB_PIN_POOLS */
export const pinPools = {
  cores: "cores",
  off: "off",
} as const;

/** PLUGIN_DB_PIN_KEYS */
export const pinKeys = {
  verdict: "verdict",
  pool: "pool",
} as const;

/** PLUGIN_DB_PIN_CHANGES */
export const pinChanges = {
  files: "files",
  layout: "layout",
} as const;

/** PLUGIN_DB_PIN_LAYOUT_FORMATS */
export const pinLayoutFormats = {
  clap: "clap",
  lv2: "lv2",
} as const;

/** PLUGIN_DB_ADDRESS_FORMATS */
export const addressFormats = {
  clap: "clap",
  lv2: "lv2",
} as const;

/** PLUGIN_DB_BUNDLE_RULES */
export const bundleRules = {
  newestVersion: "newest-version",
} as const;

/** PLUGIN_DB_LAYERS */
export const layers = {
  declared: "declared",
  scanned: "scanned",
  pinned: "pinned",
  measured: "measured",
  judged: "judged",
  reference: "reference",
  corrected: "corrected",
  warnedOverride: "warned-override",
  observed: "observed",
} as const;

/** PLUGIN_DB_FILES */
export const files = {
  same: "same",
  changed: "changed",
  absent: "absent",
  couldNotTest: "could-not-test",
} as const;

/** PLUGIN_DB_LAYOUT */
export const layout = {
  same: "same",
  changed: "changed",
  notYetKnown: "not-yet-known",
} as const;

/** PLUGIN_DB_RECORD */
export const record = {
  current: "current",
  stale: "stale",
  none: "none",
} as const;

/** PLUGIN_DB_RECORD_ACTIONS */
export const recordActions = {
  fullPath: "full-path",
  measureHere: "measure-here",
  convert: "convert",
  rejudge: "rejudge",
  remeasureBackground: "remeasure-background",
} as const;

/** PLUGIN_DB_RECORD_CHANGES */
export const recordChanges = {
  files: "files",
  layout: "layout",
  hostingChange: "hosting-change",
  machineClass: "machine-class",
  schema: "schema",
  rules: "rules",
  hostVersion: "host-version",
} as const;

/** PLUGIN_DB_STATES */
export const states = {
  unqualified: "unqualified",
  qualified: "qualified",
  isolatedOnly: "isolated-only",
  conditionsUnmet: "conditions-unmet",
  refused: "refused",
  absent: "absent",
} as const;

/** PLUGIN_DB_RUNS */
export const runs = {
  shared: "shared",
  alone: "alone",
  notLoaded: "not-loaded",
} as const;

/** PLUGIN_DB_REASONS */
export const reasons = {
  neverPinned: "never-pinned",
  measurementPending: "measurement-pending",
  couldNotTest: "could-not-test",
  filesChanged: "files-changed",
  layoutChanged: "layout-changed",
  layoutNotYetKnown: "layout-not-yet-known",
  recordStale: "record-stale",
  notMeasuredUnderHost: "not-measured-under-host",
  verdictAllows: "verdict-allows",
  verdictIsolated: "verdict-isolated",
  referenceOnly: "reference-only",
  noCell: "no-cell",
  conditionFails: "condition-fails",
  recordRefused: "record-refused",
  shippedRefusal: "shipped-refusal",
  filesGone: "files-gone",
} as const;

/** PLUGIN_DB_ISOLATION_HOSTS */
export const isolationHosts = {
  pluginHostd: "plugin-hostd",
} as const;

/** PLUGIN_DB_SOURCES */
export const sources = {
  none: "none",
  local: "local",
  reference: "reference",
} as const;

/** PLUGIN_DB_BAR */
export const bar = {
  qualifiedOnly: "qualified-only",
  warnOnCrashes: "warn-on-crashes",
  loadAll: "load-all",
} as const;

/** PLUGIN_DB_LATENCY */
export const latency = {
  within: "within",
  over: "over",
  unmeasured: "unmeasured",
} as const;

/** the PLUGIN_DB_RECORD word of each word of PLUGIN_DB_RECORD_ACTIONS */
export const recordActionsGroup = {
  [recordActions.fullPath]: record.stale,
  [recordActions.measureHere]: record.stale,
  [recordActions.convert]: record.stale,
  [recordActions.rejudge]: record.stale,
  [recordActions.remeasureBackground]: record.current,
} as const;

/** the PLUGIN_DB_RECORD_ACTIONS word of each word of PLUGIN_DB_RECORD_CHANGES */
export const recordChangesGroup = {
  [recordChanges.files]: recordActions.fullPath,
  [recordChanges.layout]: recordActions.fullPath,
  [recordChanges.hostingChange]: recordActions.fullPath,
  [recordChanges.machineClass]: recordActions.measureHere,
  [recordChanges.schema]: recordActions.convert,
  [recordChanges.rules]: recordActions.rejudge,
  [recordChanges.hostVersion]: recordActions.remeasureBackground,
} as const;

/** the PLUGIN_DB_RUNS word of each word of PLUGIN_DB_STATES */
export const statesGroup = {
  [states.unqualified]: runs.alone,
  [states.qualified]: runs.shared,
  [states.isolatedOnly]: runs.alone,
  [states.conditionsUnmet]: runs.alone,
  [states.refused]: runs.notLoaded,
  [states.absent]: runs.notLoaded,
} as const;

/** the PLUGIN_DB_STATES word of each word of PLUGIN_DB_REASONS */
export const reasonsGroup = {
  [reasons.neverPinned]: states.unqualified,
  [reasons.measurementPending]: states.unqualified,
  [reasons.couldNotTest]: states.unqualified,
  [reasons.filesChanged]: states.unqualified,
  [reasons.layoutChanged]: states.unqualified,
  [reasons.layoutNotYetKnown]: states.unqualified,
  [reasons.recordStale]: states.unqualified,
  [reasons.notMeasuredUnderHost]: states.unqualified,
  [reasons.verdictAllows]: states.qualified,
  [reasons.verdictIsolated]: states.isolatedOnly,
  [reasons.referenceOnly]: states.isolatedOnly,
  [reasons.noCell]: states.conditionsUnmet,
  [reasons.conditionFails]: states.conditionsUnmet,
  [reasons.recordRefused]: states.refused,
  [reasons.shippedRefusal]: states.refused,
  [reasons.filesGone]: states.absent,
} as const;

/** the keys a record must carry (data/store.schema.json, recordFields) and its host's */
export const recordRequired = ["verdict", "machineClass", "host", "rules", "recordSchema", "cells", "conditions"] as const;
export const recordHostRequired = ["name", "version", "hostingEpoch"] as const;
