/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com> */
/* The state function, by table: each row is a fixture, one change from a plugin qualified here, and the answer it must
 * give. Every state, reason, change, action, run and source of data/db.json must be reached by a row: a word no
 * fixture reaches is red. */

#include <stdio.h>
#include <string.h>

#include <plugin-db/db.h>

static const char *const g_states[] = OMX_PLUGIN_DB_STATES_INIT;
static const char *const g_reasons[] = OMX_PLUGIN_DB_REASONS_INIT;
static const char *const g_changes[] = OMX_PLUGIN_DB_RECORD_CHANGES_INIT;
static const char *const g_actions[] = OMX_PLUGIN_DB_RECORD_ACTIONS_INIT;
static const char *const g_runs[] = OMX_PLUGIN_DB_RUNS_INIT;
static const char *const g_sources[] = OMX_PLUGIN_DB_SOURCES_INIT;
static const char *const g_record[] = OMX_PLUGIN_DB_RECORD_INIT;

static const long g_cells[][2] = { { 48000, 128 }, { 48000, 256 } };
static const char *const g_conditions[] = { "cpu=cortex-a76" };
static const char *const g_facts_ok[] = { "kernel=rt", "cpu=cortex-a76" };
static const char *const g_facts_other[] = { "cpu=cortex-a72" };

/* a record measured here, under plugin-hostd, for the pinned files and layout */
static pdb_record_t record(int verdict)
{
    pdb_record_t r = { 1, 1, 1, verdict, "rpi5", "plugin-hostd", "1.4.0", 2, "rules-7", "record-3",
                       g_cells, 2, g_conditions, 1 };

    return r;
}

/* a plugin qualified here: the base every row changes one thing of */
static void base(pdb_state_in_t *in, pdb_here_t *h)
{
    pdb_here_t here = { "rpi5", "plugin-hostd", "1.4.0", 2, "rules-7", "record-3", 48000, 128, g_facts_ok, 2 };

    memset(in, 0, sizeof(*in));
    in->pinned = 1;
    in->files = PDB_FILES_SAME;
    in->layout = PDB_LAYOUT_SAME;
    in->local = record(PDB_PIN_VERDICTS_IN_PROCESS);
    *h = here;
}

typedef void (*fixture_t)(pdb_state_in_t *, pdb_here_t *);

#define FIXTURE(name, body) static void name(pdb_state_in_t *in, pdb_here_t *h) { (void)in; (void)h; body }

FIXTURE(f_qualified, )
FIXTURE(f_isolated, in->local.verdict = PDB_PIN_VERDICTS_ISOLATED;)
FIXTURE(f_isolated_not_hostd, in->local.verdict = PDB_PIN_VERDICTS_ISOLATED; in->local.host_name = "clap-qualify";)
FIXTURE(f_no_cell, h->quantum = 64;)
FIXTURE(f_rate_unknown, h->rate = -1;)
FIXTURE(f_condition_fails, h->facts = g_facts_other; h->nfacts = 1;)
FIXTURE(f_condition_unknown, h->nfacts = 0;)
FIXTURE(f_refused, in->local.verdict = PDB_PIN_VERDICTS_REFUSED;)
FIXTURE(f_refused_override, in->local.verdict = PDB_PIN_VERDICTS_REFUSED; in->warned_override = 1;)
FIXTURE(f_override_not_refused, in->warned_override = 1;)
FIXTURE(f_shipped_refusal, in->reference = record(PDB_PIN_VERDICTS_REFUSED); in->warned_override = 1;)
FIXTURE(f_shipped_refusal_other_bytes, in->reference = record(PDB_PIN_VERDICTS_REFUSED); in->reference.pin_files = 0;)
FIXTURE(f_absent, in->files = PDB_FILES_ABSENT;)
FIXTURE(f_absent_refused, in->files = PDB_FILES_ABSENT; in->reference = record(PDB_PIN_VERDICTS_REFUSED);)
FIXTURE(f_never_pinned, in->pinned = 0; in->files = PDB_FILES_COULD_NOT_TEST; in->layout = PDB_LAYOUT_NOT_YET_KNOWN;
        in->local.present = 0;)
FIXTURE(f_pending, in->local.present = 0;)
FIXTURE(f_could_not_test, in->files = PDB_FILES_COULD_NOT_TEST;)
FIXTURE(f_files_changed, in->files = PDB_FILES_CHANGED; in->layout = PDB_LAYOUT_NOT_YET_KNOWN;)
FIXTURE(f_record_other_files, in->local.pin_files = 0;)
FIXTURE(f_layout_changed, in->layout = PDB_LAYOUT_CHANGED;)
FIXTURE(f_layout_unknown, in->layout = PDB_LAYOUT_NOT_YET_KNOWN;)
FIXTURE(f_record_other_layout, in->local.pin_layout = 0;)
FIXTURE(f_hosting_change, h->hosting_epoch = 3; h->host_version = "2.0.0";)
FIXTURE(f_hosting_unknown, h->hosting_epoch = -1;)
FIXTURE(f_machine_class, h->machine_class = "x86-64-v3";)
FIXTURE(f_schema, h->record_schema = "record-4";)
FIXTURE(f_rules, h->rules = "rules-8";)
FIXTURE(f_host_release, h->host_version = "1.4.1";)
FIXTURE(f_schema_and_rules, h->record_schema = "record-4"; h->rules = "rules-8";)
FIXTURE(f_class_and_rules, h->machine_class = "x86-64-v3"; h->rules = "rules-8";)
FIXTURE(f_ref_same_class, in->local.present = 0; in->reference = record(PDB_PIN_VERDICTS_IN_PROCESS);)
FIXTURE(f_ref_other_class, in->local.present = 0; in->reference = record(PDB_PIN_VERDICTS_IN_PROCESS);
        in->reference.machine_class = "rpi4";)
FIXTURE(f_ref_other_class_not_hostd, in->local.present = 0; in->reference = record(PDB_PIN_VERDICTS_IN_PROCESS);
        in->reference.machine_class = "rpi4"; in->reference.host_name = "clap-qualify";)
FIXTURE(f_ref_other_bytes, in->local.present = 0; in->reference = record(PDB_PIN_VERDICTS_IN_PROCESS);
        in->reference.pin_files = 0;)
FIXTURE(f_local_wins, in->local.verdict = PDB_PIN_VERDICTS_ISOLATED;
        in->reference = record(PDB_PIN_VERDICTS_IN_PROCESS);)
FIXTURE(f_stale_local_over_reference, h->rules = "rules-8"; in->reference = record(PDB_PIN_VERDICTS_IN_PROCESS);
        in->reference.rules = "rules-8";)
FIXTURE(f_verdict_unknown, in->local.verdict = -1;)

enum { ANY = -2, NONE = -1 };

typedef struct ROW_T {
    const char *name;           /* the spec row it stands for */
    fixture_t fixture;
    int record, change, action, state, runs, reason, source;
} row_t;

#define S(x) PDB_STATES_##x
#define R(x) PDB_REASONS_##x
#define C(x) PDB_RECORD_CHANGES_##x
#define A(x) PDB_RECORD_ACTIONS_##x
#define U(x) PDB_RUNS_##x
#define O(x) PDB_SOURCES_##x
#define REC(x) PDB_RECORD_##x

static const row_t g_rows[] = {
    /* 4.2, one row per state */
    { "4.2 qualified", f_qualified, REC(CURRENT), NONE, NONE, S(QUALIFIED), U(SHARED), R(VERDICT_ALLOWS), O(LOCAL) },
    { "4.2 isolated only", f_isolated, REC(CURRENT), NONE, NONE, S(ISOLATED_ONLY), U(ALONE), R(VERDICT_ISOLATED), O(LOCAL) },
    { "8 isolated only needs plugin-hostd", f_isolated_not_hostd, REC(CURRENT), C(HOST_VERSION), A(REMEASURE_BACKGROUND),
      S(UNQUALIFIED), U(ALONE), R(NOT_MEASURED_UNDER_HOST), O(NONE) },
    { "4.2 conditions unmet: no cell", f_no_cell, REC(CURRENT), NONE, NONE, S(CONDITIONS_UNMET), U(ALONE), R(NO_CELL), O(LOCAL) },
    { "4.2 conditions unmet: rate not known", f_rate_unknown, REC(CURRENT), NONE, NONE, S(CONDITIONS_UNMET), U(ALONE),
      R(NO_CELL), O(LOCAL) },
    { "4.2 conditions unmet: a condition fails", f_condition_fails, REC(CURRENT), NONE, NONE, S(CONDITIONS_UNMET), U(ALONE),
      R(CONDITION_FAILS), O(LOCAL) },
    { "4.2 conditions unmet: a condition not known", f_condition_unknown, REC(CURRENT), NONE, NONE, S(CONDITIONS_UNMET),
      U(ALONE), R(CONDITION_FAILS), O(LOCAL) },
    { "4.2 refused", f_refused, REC(CURRENT), NONE, NONE, S(REFUSED), U(NOT_LOADED), R(RECORD_REFUSED), O(LOCAL) },
    { "4.2 refused, a warned override runs it alone", f_refused_override, REC(CURRENT), NONE, NONE, S(REFUSED), U(ALONE),
      R(RECORD_REFUSED), O(LOCAL) },
    { "3 a warned override is no rename", f_override_not_refused, REC(CURRENT), NONE, NONE, S(QUALIFIED), U(SHARED),
      R(VERDICT_ALLOWS), O(LOCAL) },
    { "4.5 a shipped refusal wins over everything", f_shipped_refusal, REC(CURRENT), NONE, NONE, S(REFUSED), U(NOT_LOADED),
      R(SHIPPED_REFUSAL), O(REFERENCE) },
    { "4.5 a shipped refusal of other bytes is ignored", f_shipped_refusal_other_bytes, REC(CURRENT), NONE, NONE,
      S(QUALIFIED), U(SHARED), R(VERDICT_ALLOWS), O(LOCAL) },
    { "4.2 absent", f_absent, REC(STALE), C(FILES), A(FULL_PATH), S(ABSENT), U(NOT_LOADED), R(FILES_GONE), O(NONE) },
    { "4.2 absent before a shipped refusal", f_absent_refused, REC(STALE), C(FILES), A(FULL_PATH), S(ABSENT), U(NOT_LOADED),
      R(FILES_GONE), O(NONE) },
    { "4.2 unqualified: never pinned", f_never_pinned, REC(NONE), NONE, NONE, S(UNQUALIFIED), U(ALONE), R(NEVER_PINNED),
      O(NONE) },
    { "4.2 unqualified: measurement pending", f_pending, REC(NONE), NONE, NONE, S(UNQUALIFIED), U(ALONE),
      R(MEASUREMENT_PENDING), O(NONE) },
    { "4.2 unqualified: could not test", f_could_not_test, REC(STALE), C(FILES), A(FULL_PATH), S(UNQUALIFIED), U(ALONE),
      R(COULD_NOT_TEST), O(NONE) },
    { "4.2 unqualified: an unknown verdict", f_verdict_unknown, REC(CURRENT), NONE, NONE, S(UNQUALIFIED), U(ALONE),
      R(COULD_NOT_TEST), O(NONE) },
    /* 4.4, one row per change */
    { "4.4 plugin files", f_files_changed, REC(STALE), C(FILES), A(FULL_PATH), S(UNQUALIFIED), U(ALONE), R(FILES_CHANGED),
      O(NONE) },
    { "4.4 plugin files: the record was written for other bytes", f_record_other_files, REC(STALE), C(FILES), A(FULL_PATH),
      S(UNQUALIFIED), U(ALONE), R(RECORD_STALE), O(NONE) },
    { "4.4 layout", f_layout_changed, REC(STALE), C(LAYOUT), A(FULL_PATH), S(UNQUALIFIED), U(ALONE), R(LAYOUT_CHANGED),
      O(NONE) },
    { "4.4 layout not yet known", f_layout_unknown, REC(STALE), C(LAYOUT), A(FULL_PATH), S(UNQUALIFIED), U(ALONE),
      R(LAYOUT_NOT_YET_KNOWN), O(NONE) },
    { "4.4 layout: the record was written for another layout", f_record_other_layout, REC(STALE), C(LAYOUT), A(FULL_PATH),
      S(UNQUALIFIED), U(ALONE), R(RECORD_STALE), O(NONE) },
    { "4.4 judgement rules", f_rules, REC(STALE), C(RULES), A(REJUDGE), S(UNQUALIFIED), U(ALONE), R(RECORD_STALE), O(NONE) },
    { "4.4 record schema", f_schema, REC(STALE), C(SCHEMA), A(CONVERT), S(UNQUALIFIED), U(ALONE), R(RECORD_STALE), O(NONE) },
    { "4.4 host version, normal release", f_host_release, REC(CURRENT), C(HOST_VERSION), A(REMEASURE_BACKGROUND),
      S(QUALIFIED), U(SHARED), R(VERDICT_ALLOWS), O(LOCAL) },
    { "4.4 host release that declares a hosting change", f_hosting_change, REC(STALE), C(HOSTING_CHANGE), A(FULL_PATH),
      S(UNQUALIFIED), U(ALONE), R(RECORD_STALE), O(NONE) },
    { "4.4 hosting epoch not known here", f_hosting_unknown, REC(STALE), C(HOSTING_CHANGE), A(FULL_PATH), S(UNQUALIFIED),
      U(ALONE), R(RECORD_STALE), O(NONE) },
    { "4.4 machine class", f_machine_class, REC(STALE), C(MACHINE_CLASS), A(MEASURE_HERE), S(UNQUALIFIED), U(ALONE),
      R(RECORD_STALE), O(NONE) },
    { "4.4 two changes: the first in order answers", f_schema_and_rules, REC(STALE), C(SCHEMA), A(CONVERT), S(UNQUALIFIED),
      U(ALONE), R(RECORD_STALE), O(NONE) },
    { "4.4 machine class before rules", f_class_and_rules, REC(STALE), C(MACHINE_CLASS), A(MEASURE_HERE), S(UNQUALIFIED),
      U(ALONE), R(RECORD_STALE), O(NONE) },
    /* 4.5, one row per shipped record */
    { "4.5 pin matches, same class", f_ref_same_class, REC(NONE), NONE, NONE, S(QUALIFIED), U(SHARED), R(VERDICT_ALLOWS),
      O(REFERENCE) },
    { "4.5 pin matches, other class", f_ref_other_class, REC(NONE), NONE, NONE, S(ISOLATED_ONLY), U(ALONE),
      R(REFERENCE_ONLY), O(REFERENCE) },
    { "4.5 other class, not measured under plugin-hostd", f_ref_other_class_not_hostd, REC(NONE), NONE, NONE,
      S(UNQUALIFIED), U(ALONE), R(NOT_MEASURED_UNDER_HOST), O(NONE) },
    { "4.5 pin differs", f_ref_other_bytes, REC(NONE), NONE, NONE, S(UNQUALIFIED), U(ALONE), R(MEASUREMENT_PENDING), O(NONE) },
    { "4.5 a local measurement wins", f_local_wins, REC(CURRENT), NONE, NONE, S(ISOLATED_ONLY), U(ALONE), R(VERDICT_ISOLATED),
      O(LOCAL) },
    { "4.5 a stale local record is not replaced by a reference", f_stale_local_over_reference, REC(STALE), C(RULES),
      A(REJUDGE), S(UNQUALIFIED), U(ALONE), R(RECORD_STALE), O(NONE) },
};

#define WORD(t, n, i) pdb_word(t, n, i)

int main(void)
{
    int reached_state[PDB_STATES_COUNT] = { 0 }, reached_reason[PDB_REASONS_COUNT] = { 0 };
    int reached_change[PDB_RECORD_CHANGES_COUNT] = { 0 }, reached_action[PDB_RECORD_ACTIONS_COUNT] = { 0 };
    int reached_runs[PDB_RUNS_COUNT] = { 0 }, reached_source[PDB_SOURCES_COUNT] = { 0 };
    int reached_record[PDB_RECORD_COUNT] = { 0 };
    int fail = 0, n;

    for (n = 0; n < (int)(sizeof(g_rows) / sizeof(g_rows[0])); n++)
    {
        const row_t *r = &g_rows[n];
        pdb_state_in_t in;
        pdb_here_t here;
        pdb_answer_t a;

        base(&in, &here);
        r->fixture(&in, &here);
        pdb_state(&in, &here, &a);
        if (a.record != r->record || a.change != r->change || a.action != r->action || a.state != r->state ||
            a.runs != r->runs || a.reason != r->reason || a.source != r->source || a.files != in.files ||
            a.layout != in.layout)
        {
            printf("FAIL %s: record %s change %s action %s state %s runs %s reason %s source %s\n"
                   "     want record %s change %s action %s state %s runs %s reason %s source %s\n",
                   r->name, WORD(g_record, PDB_RECORD_COUNT, a.record),
                   WORD(g_changes, PDB_RECORD_CHANGES_COUNT, a.change), WORD(g_actions, PDB_RECORD_ACTIONS_COUNT, a.action),
                   WORD(g_states, PDB_STATES_COUNT, a.state), WORD(g_runs, PDB_RUNS_COUNT, a.runs),
                   WORD(g_reasons, PDB_REASONS_COUNT, a.reason), WORD(g_sources, PDB_SOURCES_COUNT, a.source),
                   WORD(g_record, PDB_RECORD_COUNT, r->record), WORD(g_changes, PDB_RECORD_CHANGES_COUNT, r->change),
                   WORD(g_actions, PDB_RECORD_ACTIONS_COUNT, r->action), WORD(g_states, PDB_STATES_COUNT, r->state),
                   WORD(g_runs, PDB_RUNS_COUNT, r->runs), WORD(g_reasons, PDB_REASONS_COUNT, r->reason),
                   WORD(g_sources, PDB_SOURCES_COUNT, r->source));
            fail = 1;
            continue;
        }
        reached_state[a.state] = reached_reason[a.reason] = reached_runs[a.runs] = reached_source[a.source] = 1;
        reached_record[a.record] = 1;
        if (a.change >= 0)
            reached_change[a.change] = reached_action[a.action] = 1;
    }
#define REACHED(table, words, count, what)                                                                             \
    for (n = 0; n < (count); n++)                                                                                      \
        if (!table[n])                                                                                                 \
        {                                                                                                              \
            printf("FAIL no fixture reaches the %s %s\n", what, words[n]);                                             \
            fail = 1;                                                                                                  \
        }
    REACHED(reached_state, g_states, PDB_STATES_COUNT, "state")
    REACHED(reached_reason, g_reasons, PDB_REASONS_COUNT, "reason")
    REACHED(reached_change, g_changes, PDB_RECORD_CHANGES_COUNT, "change")
    REACHED(reached_action, g_actions, PDB_RECORD_ACTIONS_COUNT, "action")
    REACHED(reached_runs, g_runs, PDB_RUNS_COUNT, "run")
    REACHED(reached_source, g_sources, PDB_SOURCES_COUNT, "source")
    REACHED(reached_record, g_record, PDB_RECORD_COUNT, "record attribute")
    if (fail)
        return 1;
    printf("ok   state: %d fixtures, every state, reason, change, action, run and source of data/db.json reached\n",
           (int)(sizeof(g_rows) / sizeof(g_rows[0])));
    return 0;
}
