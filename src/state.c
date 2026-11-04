/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com> */

/* THE state function. Its tables are data/db.json's: which action each change asks for, which record attribute each
 * action leaves, how each state runs and the order the changes are judged in; the code holds only the tests. */

#include <string.h>

#include <plugin-db/db.h>

static const int g_change_action[] = PDB_RECORD_CHANGES_GROUP_INIT;
static const int g_action_record[] = PDB_RECORD_ACTIONS_GROUP_INIT;
static const int g_state_runs[] = PDB_STATES_GROUP_INIT;
static const char *const g_isolation_hosts[] = OMX_PLUGIN_DB_ISOLATION_HOSTS_INIT;

/* a fact a record names and here does not know differs */
static int differs(const char *a, const char *b)
{
    return !a || !b || strcmp(a, b) != 0;
}

/* whether one row of PLUGIN_DB_RECORD_CHANGES holds for the record */
static int changed(int change, const pdb_state_in_t *in, const pdb_record_t *r, const pdb_here_t *h)
{
    switch (change)
    {
    case PDB_RECORD_CHANGES_FILES:
        return in->files != PDB_FILES_SAME || !r->pin_files;
    case PDB_RECORD_CHANGES_LAYOUT:
        return in->layout != PDB_LAYOUT_SAME || !r->pin_layout;
    case PDB_RECORD_CHANGES_HOSTING_CHANGE:
        return h->hosting_epoch < 0 || r->hosting_epoch < h->hosting_epoch;
    case PDB_RECORD_CHANGES_MACHINE_CLASS:
        return differs(r->machine_class, h->machine_class);
    case PDB_RECORD_CHANGES_SCHEMA:
        return differs(r->record_schema, h->record_schema);
    case PDB_RECORD_CHANGES_RULES:
        return differs(r->rules, h->rules);
    case PDB_RECORD_CHANGES_HOST_VERSION:
        return differs(r->host_name, h->host_name) || differs(r->host_version, h->host_version);
    }
    return 1;   /* a change with no test here is never passed */
}

/* the first change, in PLUGIN_DB_RECORD_CHANGES's order, since the record was written; -1 for none */
static int change_of(const pdb_state_in_t *in, const pdb_record_t *r, const pdb_here_t *h)
{
    int c;

    for (c = 0; c < PDB_RECORD_CHANGES_COUNT; c++)
        if (changed(c, in, r, h))
            return c;
    return -1;
}

static int current(int change)
{
    return change < 0 || g_action_record[g_change_action[change]] == PDB_RECORD_CURRENT;
}

/* whether the record has a cell for the rate and quantum here, and every condition it names holds here */
static int cell_here(const pdb_record_t *r, const pdb_here_t *h)
{
    int n;

    for (n = 0; n < r->ncells; n++)
        if (h->rate > 0 && h->quantum > 0 && r->cells[n][0] == h->rate && r->cells[n][1] == h->quantum)
            return 1;
    return 0;
}

static int conditions_here(const pdb_record_t *r, const pdb_here_t *h)
{
    int n, k;

    for (n = 0; n < r->nconditions; n++)
    {
        int holds = 0;

        for (k = 0; k < h->nfacts && !holds; k++)
            holds = !strcmp(r->conditions[n], h->facts[k]);
        if (!holds)
            return 0;
    }
    return 1;
}

static int isolation_host(const char *host)
{
    int n;

    for (n = 0; host && n < (int)OMX_PLUGIN_DB_ISOLATION_HOSTS_COUNT; n++)
        if (!strcmp(host, g_isolation_hosts[n]))
            return 1;
    return 0;
}

static void answer(pdb_answer_t *out, int state, int reason, int source)
{
    out->state = state;
    out->runs = g_state_runs[state];
    out->reason = reason;
    out->source = source;
}

/* alone, if it may: isolated only needs a measurement under a host of PLUGIN_DB_ISOLATION_HOSTS */
static void alone(pdb_answer_t *out, const pdb_record_t *r, int reason, int source)
{
    if (isolation_host(r->host_name))
        answer(out, PDB_STATES_ISOLATED_ONLY, reason, source);
    else
        answer(out, PDB_STATES_UNQUALIFIED, PDB_REASONS_NOT_MEASURED_UNDER_HOST, PDB_SOURCES_NONE);
}

void pdb_state(const pdb_state_in_t *in, const pdb_here_t *here, pdb_answer_t *out)
{
    const pdb_record_t *rec = NULL;
    int source = PDB_SOURCES_NONE;

    memset(out, 0, sizeof(*out));
    out->files = in->files;
    out->layout = in->layout;
    out->change = out->action = -1;
    out->record = PDB_RECORD_NONE;
    if (in->local.present)
    {
        out->change = change_of(in, &in->local, here);
        out->action = out->change < 0 ? -1 : g_change_action[out->change];
        out->record = current(out->change) ? PDB_RECORD_CURRENT : PDB_RECORD_STALE;
    }

    if (in->files == PDB_FILES_ABSENT)
    {
        answer(out, PDB_STATES_ABSENT, PDB_REASONS_FILES_GONE, PDB_SOURCES_NONE);
        return;
    }
    /* a shipped refusal of these bytes wins over everything, a warned override included */
    if (in->reference.present && in->files == PDB_FILES_SAME && in->reference.pin_files &&
        in->reference.verdict == PDB_PIN_VERDICTS_REFUSED)
    {
        answer(out, PDB_STATES_REFUSED, PDB_REASONS_SHIPPED_REFUSAL, PDB_SOURCES_REFERENCE);
        return;
    }

    if (out->record == PDB_RECORD_CURRENT)
    {
        rec = &in->local;
        source = PDB_SOURCES_LOCAL;
    }
    else if (!in->local.present && in->reference.present)
    {
        /* a local measurement wins; with none, a shipped record of these bytes answers on its class of machine and
         * gives reference figures only on another */
        int change = change_of(in, &in->reference, here);

        if (current(change))
        {
            rec = &in->reference;
            source = PDB_SOURCES_REFERENCE;
        }
        else if (change == PDB_RECORD_CHANGES_MACHINE_CLASS)
        {
            alone(out, &in->reference, PDB_REASONS_REFERENCE_ONLY, PDB_SOURCES_REFERENCE);
            return;
        }
    }

    if (!rec)
    {
        int reason = !in->pinned                             ? PDB_REASONS_NEVER_PINNED
                     : in->files == PDB_FILES_COULD_NOT_TEST ? PDB_REASONS_COULD_NOT_TEST
                     : in->files == PDB_FILES_CHANGED        ? PDB_REASONS_FILES_CHANGED
                     : in->layout == PDB_LAYOUT_CHANGED      ? PDB_REASONS_LAYOUT_CHANGED
                     : in->layout == PDB_LAYOUT_NOT_YET_KNOWN ? PDB_REASONS_LAYOUT_NOT_YET_KNOWN
                     : out->record == PDB_RECORD_STALE       ? PDB_REASONS_RECORD_STALE
                                                             : PDB_REASONS_MEASUREMENT_PENDING;

        answer(out, PDB_STATES_UNQUALIFIED, reason, PDB_SOURCES_NONE);
        return;
    }
    if (rec->verdict == PDB_PIN_VERDICTS_REFUSED)
    {
        answer(out, PDB_STATES_REFUSED, PDB_REASONS_RECORD_REFUSED, source);
        if (in->warned_override)
            out->runs = PDB_RUNS_ALONE;
        return;
    }
    if (rec->verdict != PDB_PIN_VERDICTS_IN_PROCESS && rec->verdict != PDB_PIN_VERDICTS_ISOLATED)
    {
        answer(out, PDB_STATES_UNQUALIFIED, PDB_REASONS_COULD_NOT_TEST, PDB_SOURCES_NONE);
        return;
    }
    if (!cell_here(rec, here))
    {
        answer(out, PDB_STATES_CONDITIONS_UNMET, PDB_REASONS_NO_CELL, source);
        return;
    }
    if (!conditions_here(rec, here))
    {
        answer(out, PDB_STATES_CONDITIONS_UNMET, PDB_REASONS_CONDITION_FAILS, source);
        return;
    }
    if (rec->verdict == PDB_PIN_VERDICTS_ISOLATED)
    {
        alone(out, rec, PDB_REASONS_VERDICT_ISOLATED, source);
        return;
    }
    answer(out, PDB_STATES_QUALIFIED, PDB_REASONS_VERDICT_ALLOWS, source);
}
