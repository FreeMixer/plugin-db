// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
#ifndef PLUGIN_DB_TABLES_H
#define PLUGIN_DB_TABLES_H
/*
 * GENERATED: DO NOT EDIT BY HAND.
 * Produced by tools/render-db.py from data/pin.json, data/db.json and data/store.schema.json. Each words item is an
 * enum of its words' indexes in their order, PDB_<ITEM>_<NAME>, the index into the item's _INIT of
 * plugin_db_vocabulary.h; a grouped item has a _GROUP_INIT, the index of each word's group in the item it names.
 * The store layout is x-plugin-db-store's, PDB_STORE_<NAME>, and every key the schema declares is PDB_KEY_<NAME>.
 */

#include <plugin-db/plugin_db_vocabulary.h>

/* PLUGIN_DB_PIN_VERDICTS */
enum {
    PDB_PIN_VERDICTS_IN_PROCESS = 0,
    PDB_PIN_VERDICTS_ISOLATED = 1,
    PDB_PIN_VERDICTS_REFUSED = 2,
};
#define PDB_PIN_VERDICTS_COUNT 3

/* PLUGIN_DB_PIN_POOLS */
enum {
    PDB_PIN_POOLS_CORES = 0,
    PDB_PIN_POOLS_OFF = 1,
};
#define PDB_PIN_POOLS_COUNT 2

/* PLUGIN_DB_PIN_KEYS */
enum {
    PDB_PIN_KEYS_VERDICT = 0,
    PDB_PIN_KEYS_POOL = 1,
};
#define PDB_PIN_KEYS_COUNT 2

/* PLUGIN_DB_PIN_CHANGES */
enum {
    PDB_PIN_CHANGES_FILES = 0,
    PDB_PIN_CHANGES_LAYOUT = 1,
};
#define PDB_PIN_CHANGES_COUNT 2

/* PLUGIN_DB_PIN_LAYOUT_FORMATS */
enum {
    PDB_PIN_LAYOUT_FORMATS_CLAP = 0,
    PDB_PIN_LAYOUT_FORMATS_LV2 = 1,
};
#define PDB_PIN_LAYOUT_FORMATS_COUNT 2

/* PLUGIN_DB_ADDRESS_FORMATS */
enum {
    PDB_ADDRESS_FORMATS_CLAP = 0,
    PDB_ADDRESS_FORMATS_LV2 = 1,
};
#define PDB_ADDRESS_FORMATS_COUNT 2

/* PLUGIN_DB_BUNDLE_RULES */
enum {
    PDB_BUNDLE_RULES_NEWEST_VERSION = 0,
};
#define PDB_BUNDLE_RULES_COUNT 1

/* PLUGIN_DB_LAYERS */
enum {
    PDB_LAYERS_DECLARED = 0,
    PDB_LAYERS_SCANNED = 1,
    PDB_LAYERS_PINNED = 2,
    PDB_LAYERS_MEASURED = 3,
    PDB_LAYERS_JUDGED = 4,
    PDB_LAYERS_REFERENCE = 5,
    PDB_LAYERS_CORRECTED = 6,
    PDB_LAYERS_WARNED_OVERRIDE = 7,
    PDB_LAYERS_OBSERVED = 8,
};
#define PDB_LAYERS_COUNT 9

/* PLUGIN_DB_FILES */
enum {
    PDB_FILES_SAME = 0,
    PDB_FILES_CHANGED = 1,
    PDB_FILES_ABSENT = 2,
    PDB_FILES_COULD_NOT_TEST = 3,
};
#define PDB_FILES_COUNT 4

/* PLUGIN_DB_LAYOUT */
enum {
    PDB_LAYOUT_SAME = 0,
    PDB_LAYOUT_CHANGED = 1,
    PDB_LAYOUT_NOT_YET_KNOWN = 2,
};
#define PDB_LAYOUT_COUNT 3

/* PLUGIN_DB_RECORD */
enum {
    PDB_RECORD_CURRENT = 0,
    PDB_RECORD_STALE = 1,
    PDB_RECORD_NONE = 2,
};
#define PDB_RECORD_COUNT 3

/* PLUGIN_DB_RECORD_ACTIONS */
enum {
    PDB_RECORD_ACTIONS_FULL_PATH = 0,
    PDB_RECORD_ACTIONS_MEASURE_HERE = 1,
    PDB_RECORD_ACTIONS_CONVERT = 2,
    PDB_RECORD_ACTIONS_REJUDGE = 3,
    PDB_RECORD_ACTIONS_REMEASURE_BACKGROUND = 4,
};
#define PDB_RECORD_ACTIONS_COUNT 5

/* PLUGIN_DB_RECORD_CHANGES */
enum {
    PDB_RECORD_CHANGES_FILES = 0,
    PDB_RECORD_CHANGES_LAYOUT = 1,
    PDB_RECORD_CHANGES_HOSTING_CHANGE = 2,
    PDB_RECORD_CHANGES_MACHINE_CLASS = 3,
    PDB_RECORD_CHANGES_SCHEMA = 4,
    PDB_RECORD_CHANGES_RULES = 5,
    PDB_RECORD_CHANGES_HOST_VERSION = 6,
};
#define PDB_RECORD_CHANGES_COUNT 7

/* PLUGIN_DB_STATES */
enum {
    PDB_STATES_UNQUALIFIED = 0,
    PDB_STATES_QUALIFIED = 1,
    PDB_STATES_ISOLATED_ONLY = 2,
    PDB_STATES_CONDITIONS_UNMET = 3,
    PDB_STATES_REFUSED = 4,
    PDB_STATES_ABSENT = 5,
};
#define PDB_STATES_COUNT 6

/* PLUGIN_DB_RUNS */
enum {
    PDB_RUNS_SHARED = 0,
    PDB_RUNS_ALONE = 1,
    PDB_RUNS_NOT_LOADED = 2,
};
#define PDB_RUNS_COUNT 3

/* PLUGIN_DB_REASONS */
enum {
    PDB_REASONS_NEVER_PINNED = 0,
    PDB_REASONS_MEASUREMENT_PENDING = 1,
    PDB_REASONS_COULD_NOT_TEST = 2,
    PDB_REASONS_FILES_CHANGED = 3,
    PDB_REASONS_LAYOUT_CHANGED = 4,
    PDB_REASONS_LAYOUT_NOT_YET_KNOWN = 5,
    PDB_REASONS_RECORD_STALE = 6,
    PDB_REASONS_NOT_MEASURED_UNDER_HOST = 7,
    PDB_REASONS_VERDICT_ALLOWS = 8,
    PDB_REASONS_VERDICT_ISOLATED = 9,
    PDB_REASONS_REFERENCE_ONLY = 10,
    PDB_REASONS_NO_CELL = 11,
    PDB_REASONS_CONDITION_FAILS = 12,
    PDB_REASONS_RECORD_REFUSED = 13,
    PDB_REASONS_SHIPPED_REFUSAL = 14,
    PDB_REASONS_FILES_GONE = 15,
};
#define PDB_REASONS_COUNT 16

/* PLUGIN_DB_ISOLATION_HOSTS */
enum {
    PDB_ISOLATION_HOSTS_PLUGIN_HOSTD = 0,
};
#define PDB_ISOLATION_HOSTS_COUNT 1

/* PLUGIN_DB_SOURCES */
enum {
    PDB_SOURCES_NONE = 0,
    PDB_SOURCES_LOCAL = 1,
    PDB_SOURCES_REFERENCE = 2,
};
#define PDB_SOURCES_COUNT 3

/* PLUGIN_DB_BAR */
enum {
    PDB_BAR_QUALIFIED_ONLY = 0,
    PDB_BAR_WARN_ON_CRASHES = 1,
    PDB_BAR_LOAD_ALL = 2,
};
#define PDB_BAR_COUNT 3

/* PLUGIN_DB_LATENCY */
enum {
    PDB_LATENCY_WITHIN = 0,
    PDB_LATENCY_OVER = 1,
    PDB_LATENCY_UNMEASURED = 2,
};
#define PDB_LATENCY_COUNT 3

/* the PLUGIN_DB_RECORD index of each word of PLUGIN_DB_RECORD_ACTIONS */
#define PDB_RECORD_ACTIONS_GROUP_INIT { 1, 1, 1, 1, 0 }

/* the PLUGIN_DB_RECORD_ACTIONS index of each word of PLUGIN_DB_RECORD_CHANGES */
#define PDB_RECORD_CHANGES_GROUP_INIT { 0, 0, 0, 1, 2, 3, 4 }

/* the PLUGIN_DB_RUNS index of each word of PLUGIN_DB_STATES */
#define PDB_STATES_GROUP_INIT { 1, 0, 1, 1, 2, 2 }

/* the PLUGIN_DB_STATES index of each word of PLUGIN_DB_REASONS */
#define PDB_REASONS_GROUP_INIT { 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 2, 3, 3, 4, 4, 5 }

/* the store's layout (data/store.schema.json, x-plugin-db-store) */
#define PDB_STORE_ENV "PLUGIN_DB_STORE"
#define PDB_STORE_SYSTEM_ROOT "/var/lib/plugin-db"
#define PDB_STORE_USER_STATE_HOME "XDG_STATE_HOME"
#define PDB_STORE_USER_STATE_FALLBACK ".local/state"
#define PDB_STORE_USER_DIR "plugin-db"
#define PDB_STORE_ENTRIES "entries"
#define PDB_STORE_ENTRY_NAME "the SHA-256 of the entry's address, 64 lowercase hex digits"
#define PDB_STORE_LAYER_SUFFIX ".json"
#define PDB_STORE_TEMP_PREFIX ".tmp-"
#define PDB_STORE_FILES_PIN_SCHEME "omx-files/1"
#define PDB_STORE_FILES_PIN "the SHA-256 of the line 'omx-files/1', then one line '<path>\\t<sha256>' per file in byte order of the paths, each line ending in one '\\n'"
#define PDB_STORE_LV2_DEFAULT_SEARCH "~/.lv2:/usr/local/lib/lv2:/usr/lib/lv2"
#define PDB_ENVELOPE_VERSION 1

/* every key the schema declares */
#define PDB_KEY_ADDRESS "address"
#define PDB_KEY_BINARY "binary"
#define PDB_KEY_BODY "body"
#define PDB_KEY_BUNDLE_RULE "bundleRule"
#define PDB_KEY_CELLS "cells"
#define PDB_KEY_CONDITIONS "conditions"
#define PDB_KEY_ENVELOPE "envelope"
#define PDB_KEY_FILES "files"
#define PDB_KEY_FORMAT "format"
#define PDB_KEY_HOST "host"
#define PDB_KEY_HOSTING_EPOCH "hostingEpoch"
#define PDB_KEY_LATENCY_LINE_MS "latencyLineMs"
#define PDB_KEY_LATENCY_MS "latencyMs"
#define PDB_KEY_LAYER "layer"
#define PDB_KEY_LAYOUT "layout"
#define PDB_KEY_MACHINE_CLASS "machineClass"
#define PDB_KEY_NAME "name"
#define PDB_KEY_PATH "path"
#define PDB_KEY_PIN "pin"
#define PDB_KEY_QUANTUM "quantum"
#define PDB_KEY_RATE "rate"
#define PDB_KEY_RECORD_SCHEMA "recordSchema"
#define PDB_KEY_ROOT "root"
#define PDB_KEY_RULES "rules"
#define PDB_KEY_SEARCH "search"
#define PDB_KEY_SHA256 "sha256"
#define PDB_KEY_VERDICT "verdict"
#define PDB_KEY_VERSION "version"
#define PDB_KEY_WRITER "writer"

#define PDB_ENVELOPE_REQUIRED_COUNT 5
#define PDB_ENVELOPE_REQUIRED_INIT { "envelope", "layer", "address", "pin", "body" }
#define PDB_PIN_REQUIRED_COUNT 2
#define PDB_PIN_REQUIRED_INIT { "files", "layout" }
#define PDB_PINNED_BODY_REQUIRED_COUNT 5
#define PDB_PINNED_BODY_REQUIRED_INIT { "format", "root", "files", "layout", "bundleRule" }
#define PDB_RECORD_REQUIRED_COUNT 7
#define PDB_RECORD_REQUIRED_INIT { "verdict", "machineClass", "host", "rules", "recordSchema", "cells", "conditions" }
#define PDB_RECORD_HOST_REQUIRED_COUNT 3
#define PDB_RECORD_HOST_REQUIRED_INIT { "name", "version", "hostingEpoch" }

#endif /* PLUGIN_DB_TABLES_H */
