/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com> */

/* plugin-db: the one way to ask. pin writes an entry's pinned layer from its files, put writes another layer for the
 * current pin, show prints one entry's attributes, state and reason, verify checks every entry as rpm -V does. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <plugin-db/db.h>

static const char *const g_files[] = OMX_PLUGIN_DB_FILES_INIT;
static const char *const g_layout[] = OMX_PLUGIN_DB_LAYOUT_INIT;
static const char *const g_record[] = OMX_PLUGIN_DB_RECORD_INIT;
static const char *const g_changes[] = OMX_PLUGIN_DB_RECORD_CHANGES_INIT;
static const char *const g_actions[] = OMX_PLUGIN_DB_RECORD_ACTIONS_INIT;
static const char *const g_states[] = OMX_PLUGIN_DB_STATES_INIT;
static const char *const g_runs[] = OMX_PLUGIN_DB_RUNS_INIT;
static const char *const g_reasons[] = OMX_PLUGIN_DB_REASONS_INIT;
static const char *const g_sources[] = OMX_PLUGIN_DB_SOURCES_INIT;
static const char *const g_layers[] = OMX_PLUGIN_DB_LAYERS_INIT;
static const char *const g_verdicts[] = OMX_PLUGIN_DB_PIN_VERDICTS_INIT;
static const char *const g_standings[] = OMX_PLUGIN_DB_LATENCY_INIT;

#define WORD(table, item, index) pdb_word(table, PDB_##item##_COUNT, index)

static int usage(void)
{
    fputs("usage: plugin-db [--store <dir> | --system] pin <address> [--lv2-path <path> | --file <file.clap>]\n"
          "                 [--layout <scheme>:<sha256>]\n"
          "       plugin-db [--store <dir> | --system] put <address> <layer> [--writer <name>] < body.json\n"
          "       plugin-db [--store <dir> | --system] show <address> [<here>] [--layout <scheme>:<sha256>]\n"
          "       plugin-db [--store <dir> | --system] query --json <address> [<here>] [--layout <scheme>:<sha256>]\n"
          "       plugin-db [--store <dir> | --system] verify [-v] [<here>]\n"
          "  <address>  <format>:<id> or <format>@<sha256>:<id>, a format of PLUGIN_DB_ADDRESS_FORMATS\n"
          "  <here>     --machine-class <class> --host <name> --host-version <version> --hosting-epoch <n>\n"
          "             --rules <version> --record-schema <version> --rate <hz> --quantum <frames>\n"
          "             --fact <key>=<value> ...; a fact not given is not known here, and no record matches it\n",
          stderr);
    return 2;
}

typedef struct OPTS_T {
    pdb_here_t here;
    const char *facts[64];
    const char *layout, *lv2_path, *file, *writer;
    int verbose;
} opts_t;

/* the options after the verb; 0, or -1 for one that is not the verb's */
static int options(int argc, char **argv, opts_t *o)
{
    int n;

    for (n = 0; n < argc; n++)
    {
        const char *a = argv[n], *v = n + 1 < argc ? argv[n + 1] : NULL;

        if (!strcmp(a, "-v"))
        {
            o->verbose = 1;
            continue;
        }
        if (!v)
            return -1;
        n++;
        if (!strcmp(a, "--machine-class"))
            o->here.machine_class = v;
        else if (!strcmp(a, "--host"))
            o->here.host_name = v;
        else if (!strcmp(a, "--host-version"))
            o->here.host_version = v;
        else if (!strcmp(a, "--hosting-epoch"))
            o->here.hosting_epoch = strtol(v, NULL, 10);
        else if (!strcmp(a, "--rules"))
            o->here.rules = v;
        else if (!strcmp(a, "--record-schema"))
            o->here.record_schema = v;
        else if (!strcmp(a, "--rate"))
            o->here.rate = strtol(v, NULL, 10);
        else if (!strcmp(a, "--quantum"))
            o->here.quantum = strtol(v, NULL, 10);
        else if (!strcmp(a, "--fact") && strchr(v, '=') && o->here.nfacts < 64)
            o->facts[o->here.nfacts++] = v;
        else if (!strcmp(a, "--layout"))
            o->layout = v;
        else if (!strcmp(a, "--lv2-path"))
            o->lv2_path = v;
        else if (!strcmp(a, "--file"))
            o->file = v;
        else if (!strcmp(a, "--writer"))
            o->writer = v;
        else
            return -1;
    }
    o->here.facts = o->facts;
    return 0;
}

static char *read_stdin(void)
{
    size_t n = 0, cap = 4096;
    char *s = malloc(cap);

    while (s)
    {
        size_t got = fread(s + n, 1, cap - n - 1, stdin);

        n += got;
        if (got == 0)
            break;
        if (n + 1 == cap)
        {
            char *grown = realloc(s, cap *= 2);

            if (!grown)
                free(s);
            s = grown;
        }
    }
    if (s)
        s[n] = '\0';
    return s;
}

static int show(const char *store, const char *address, const opts_t *o)
{
    char why[PDB_PATH_MAX];
    pdb_answer_t a;
    pdb_detail_t d;

    if (pdb_check(store, address, &o->here, o->layout, &a, &d, why, sizeof(why)) != 0)
    {
        fprintf(stderr, "plugin-db: %s\n", why);
        return 1;
    }
    printf("address  %s\n", d.address);
    printf("entry    %s\n", d.entry);
    printf("files    %s\n", WORD(g_files, FILES, a.files));
    printf("layout   %s\n", WORD(g_layout, LAYOUT, a.layout));
    printf("record   %s\n", WORD(g_record, RECORD, a.record));
    if (a.change >= 0)
        printf("change   %s (%s)\n", WORD(g_changes, RECORD_CHANGES, a.change), WORD(g_actions, RECORD_ACTIONS, a.action));
    printf("state    %s\n", WORD(g_states, STATES, a.state));
    printf("runs     %s\n", WORD(g_runs, RUNS, a.runs));
    printf("reason   %s\n", WORD(g_reasons, REASONS, a.reason));
    printf("source   %s\n", WORD(g_sources, SOURCES, a.source));
    if (d.verdict[0] && a.source != PDB_SOURCES_NONE)
        printf("verdict  %s, measured under %s %s on %s\n", d.verdict, d.host_name, d.host_version, d.machine_class);
    if (d.root[0])
        printf("root     %s\n", d.root);
    if (d.layout[0])
        printf("pinned   %s\n", d.layout);
    if (d.problem[0])
        printf("problem  %s\n", d.problem);
    return 0;
}

static void json_string(const char *s)
{
    putchar('"');
    for (; *s; s++)
    {
        unsigned char c = (unsigned char)*s;

        if (c == '"' || c == '\\')
            printf("\\%c", c);
        else if (c < 0x20)
            printf("\\u%04x", c);
        else
            putchar(c);
    }
    putchar('"');
}

static void json_word_or_null(const char *word)
{
    if (word)
        json_string(word);
    else
        fputs("null", stdout);
}

static void json_number_or_null(int has, double n)
{
    if (has)
        printf("%.17g", n);
    else
        fputs("null", stdout);
}

/* the one reader's answer as JSON, for scripts and the Zynthian side: the attributes, state, run, reason and source
 * of show, the record's tier, and the figure at the rate and quantum here. Nothing is computed here: every word is
 * the state function's or the record's, and the standing is pdb_latency_standing's. */
static int query(const char *store, const char *address, const opts_t *o)
{
    char why[PDB_PATH_MAX];
    const char *tier = NULL;
    pdb_answer_t a;
    pdb_detail_t d;
    int k;

    if (pdb_check(store, address, &o->here, o->layout, &a, &d, why, sizeof(why)) != 0)
    {
        fprintf(stderr, "plugin-db: %s\n", why);
        return 1;
    }
    /* the tier is the answering record's verdict when it says where the plugin runs; a refusal has none */
    for (k = 0; k < PDB_PIN_VERDICTS_COUNT; k++)
        if (a.source != PDB_SOURCES_NONE && k != PDB_PIN_VERDICTS_REFUSED && !strcmp(d.verdict, g_verdicts[k]))
            tier = g_verdicts[k];
    fputs("{\"address\": ", stdout);
    json_string(d.address);
    printf(", \"files\": \"%s\", \"layout\": \"%s\", \"record\": \"%s\", \"change\": ", WORD(g_files, FILES, a.files),
           WORD(g_layout, LAYOUT, a.layout), WORD(g_record, RECORD, a.record));
    json_word_or_null(a.change >= 0 ? WORD(g_changes, RECORD_CHANGES, a.change) : NULL);
    fputs(", \"action\": ", stdout);
    json_word_or_null(a.change >= 0 ? WORD(g_actions, RECORD_ACTIONS, a.action) : NULL);
    printf(", \"state\": \"%s\", \"runs\": \"%s\", \"reason\": \"%s\", \"source\": \"%s\", \"tier\": ",
           WORD(g_states, STATES, a.state), WORD(g_runs, RUNS, a.runs), WORD(g_reasons, REASONS, a.reason),
           WORD(g_sources, SOURCES, a.source));
    json_word_or_null(tier);
    fputs(", \"figures\": ", stdout);
    if (tier && d.has_cell)
    {
        printf("{\"rate\": %ld, \"quantum\": %ld, \"latencyMs\": ", d.rate, d.quantum);
        json_number_or_null(d.has_latency, d.latency_ms);
        fputs(", \"lineMs\": ", stdout);
        json_number_or_null(d.has_line, d.line_ms);
        printf(", \"standing\": \"%s\"}", WORD(g_standings, LATENCY, d.standing));
    }
    else
        fputs("null", stdout);
    puts("}");
    return 0;
}

typedef struct VERIFY_T {
    const char *store;
    const opts_t *o;
    int dirty, failed;
} verify_t;

/* one line per entry that is not same, same and current, as rpm -V prints a file that differs */
static int verify_one(void *ctx, const char *address)
{
    verify_t *v = ctx;
    char why[PDB_PATH_MAX];
    pdb_answer_t a;
    pdb_detail_t d;
    int clean;

    if (pdb_check(v->store, address, &v->o->here, NULL, &a, &d, why, sizeof(why)) != 0)
    {
        fprintf(stderr, "plugin-db: %s\n", why);
        v->failed = 1;
        return 0;
    }
    clean = a.files == PDB_FILES_SAME && a.layout == PDB_LAYOUT_SAME && a.record == PDB_RECORD_CURRENT;
    v->dirty |= !clean;
    if (!clean || v->o->verbose)
        printf("files=%s layout=%s record=%s state=%s %s%s%s\n", WORD(g_files, FILES, a.files),
               WORD(g_layout, LAYOUT, a.layout), WORD(g_record, RECORD, a.record), WORD(g_states, STATES, a.state),
               address, d.problem[0] ? "\t" : "", d.problem);
    return 0;
}

int main(int argc, char **argv)
{
    char store[PDB_PATH_MAX], why[PDB_PATH_MAX];
    int n = 1, system = 0;
    const char *verb;
    opts_t o;

    memset(&o, 0, sizeof(o));
    o.here.hosting_epoch = -1;
    o.here.rate = o.here.quantum = -1;
    store[0] = '\0';
    for (; n < argc && argv[n][0] == '-'; n++)
    {
        if (!strcmp(argv[n], "--system"))
            system = 1;
        else if (!strcmp(argv[n], "--store") && n + 1 < argc)
            snprintf(store, sizeof(store), "%s", argv[++n]);
        else
            return usage();
    }
    if (n >= argc)
        return usage();
    if (!store[0] && pdb_store_default(system, store, sizeof(store)) != 0)
    {
        fprintf(stderr, "plugin-db: no store: set %s, or HOME\n", PDB_STORE_ENV);
        return 2;
    }
    verb = argv[n++];
    if (!strcmp(verb, "pin") && n < argc)
    {
        const char *address = argv[n++];

        if (options(argc - n, argv + n, &o) != 0)
            return usage();
        if (pdb_pin(store, address, o.file ? o.file : o.lv2_path, o.layout, why, sizeof(why)) != 0)
        {
            fprintf(stderr, "plugin-db: %s\n", why);
            return 1;
        }
        return 0;
    }
    if (!strcmp(verb, "put") && n + 1 < argc)
    {
        const char *address = argv[n], *name = argv[n + 1];
        char *body;
        int layer, rc;

        if (options(argc - n - 2, argv + n + 2, &o) != 0)
            return usage();
        for (layer = 0; layer < PDB_LAYERS_COUNT && strcmp(g_layers[layer], name); layer++)
            ;
        if (!(body = read_stdin()))
            return 2;
        rc = pdb_layer_put(store, address, layer, o.writer, body, why, sizeof(why));
        free(body);
        if (rc != 0)
        {
            fprintf(stderr, "plugin-db: %s\n", why);
            return 1;
        }
        return 0;
    }
    if (!strcmp(verb, "show") && n < argc)
    {
        const char *address = argv[n++];

        if (options(argc - n, argv + n, &o) != 0)
            return usage();
        return show(store, address, &o);
    }
    if (!strcmp(verb, "query") && n + 1 < argc && !strcmp(argv[n], "--json"))
    {
        const char *address = argv[n + 1];

        n += 2;
        if (options(argc - n, argv + n, &o) != 0)
            return usage();
        return query(store, address, &o);
    }
    if (!strcmp(verb, "verify"))
    {
        verify_t v = { store, &o, 0, 0 };

        if (options(argc - n, argv + n, &o) != 0 || o.layout)
            return usage();
        if (pdb_entries(store, verify_one, &v) < 0)
        {
            fprintf(stderr, "plugin-db: the store %s can't be read\n", store);
            return 2;
        }
        return v.failed ? 2 : v.dirty ? 1 : 0;
    }
    return usage();
}
