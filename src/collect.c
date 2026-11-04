/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com> */

/* Which files are a plugin: the LV2 bundle by PLUGIN_DB_BUNDLE_RULES and the files its manifest names, and the .clap
 * of a CLAP plugin. Moved from plugin-hostd (src/lv2.c lv2_pin_collect, src/formats.c clap_pin_collect, src/proc.c
 * proc_find_lv2_bundle), with one change of behaviour: the bundle rule, which was the first bundle in path order. */

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "collect.h"
#include "manifest.h"

int pdb_percent_decode(const char *in, char *out, size_t size)
{
    size_t o = 0;

    for (; *in; in++, o++)
    {
        int hi, lo;

        if (o + 1 >= size)
            return -1;
        if (*in != '%')
        {
            out[o] = *in;
            continue;
        }
        hi = isxdigit((unsigned char)in[1]) ? in[1] : -1;
        lo = hi >= 0 && isxdigit((unsigned char)in[2]) ? in[2] : -1;
        if (lo < 0)
            return -1;
        hi = hi <= '9' ? hi - '0' : (hi | 0x20) - 'a' + 10;
        lo = lo <= '9' ? lo - '0' : (lo | 0x20) - 'a' + 10;
        if (!(hi * 16 + lo))
            return -1;
        out[o] = (char)(hi * 16 + lo);
        in += 2;
    }
    if (o >= size)
        return -1;
    out[o] = '\0';
    return 0;
}

/* a file name as a pin holds it: '%', whitespace, the separators of a pin line and bytes outside printable ASCII as
 * %XX, which pdb_percent_decode undoes */
static int percent_encode(const char *in, char *out, size_t size)
{
    static const char digits[] = "0123456789ABCDEF";
    size_t o = 0;

    for (; *in; in++)
    {
        unsigned char c = (unsigned char)*in;

        if (c <= 0x20 || c >= 0x7f || c == '%' || c == ',' || c == '=')
        {
            if (o + 4 > size)
                return -1;
            out[o++] = '%';
            out[o++] = digits[c >> 4];
            out[o++] = digits[c & 15];
        }
        else
        {
            if (o + 2 > size)
                return -1;
            out[o++] = (char)c;
        }
    }
    out[o] = '\0';
    return 0;
}

int pdb_full_path(const char *root, const char *path, char *full, size_t size)
{
    char name[PATH_MAX];
    const char *c;
    int slashes = 0, decoded = 0;

    if (pdb_percent_decode(path, name, sizeof(name)) != 0)
        return -1;
    for (c = path; *c; c++)
        slashes += *c == '/';
    for (c = name; *c; c++)
        decoded += *c == '/';
    if (slashes != decoded)
        return -1;
    return snprintf(full, size, "%s/%s", root, name) >= (int)size ? -1 : 0;
}

int pdb_path_valid(const char *path)
{
    const char *c = path;

    if (!*path || *path == '/' || strpbrk(path, " \t\n,="))
        return 0;
    while (*c)
    {
        size_t n = strcspn(c, "/");

        if (n == 0 || (n == 2 && !strncmp(c, "..", 2)))
            return 0;
        c += n;
        if (*c == '/')
            c++;
    }
    return c[-1] != '/';
}

/* ---------------------------------------------------------------- the bundle rule */

static int bytewise(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* the .lv2 entries of dir in byte order, malloc'd; their count, or -1 when dir cannot be read */
static int bundles_of(const char *dir, char ***out)
{
    DIR *d = opendir(dir);
    struct dirent *entry;
    char **names = NULL;
    int n = 0;

    *out = NULL;
    if (!d)
        return -1;
    while ((entry = readdir(d)))
    {
        size_t len = strlen(entry->d_name);
        char **grown;

        if (len < 5 || strcmp(entry->d_name + len - 4, ".lv2") != 0)
            continue;
        if (!(grown = realloc(names, (n + 1) * sizeof(*names))) || !(grown[n] = strdup(entry->d_name)))
        {
            names = grown ? grown : names;
            while (n)
                free(names[--n]);
            free(names);
            closedir(d);
            return -1;
        }
        names = grown;
        n++;
    }
    closedir(d);
    if (n)
        qsort(names, n, sizeof(*names), bytewise);
    *out = names;
    return n;
}

/* a directory of a search path, "~/" as the home directory; 0, or -1 when it does not fit or there is no home */
static int search_dir(const char *dir, char *out, size_t size)
{
    const char *home = getenv("HOME");

    if (!strncmp(dir, "~/", 2))
        return home && snprintf(out, size, "%s/%s", home, dir + 2) < (int)size ? 0 : -1;
    return snprintf(out, size, "%s", dir) < (int)size ? 0 : -1;
}

/* how a bundle ranks for the plugin: whether it realises it (an lv2:binary), then its version; 0 when it does not
 * name the plugin */
typedef struct RANK_T {
    int names, realises;
    long minor, micro;
} rank_t;

static rank_t rank_of(const char *bundle, const char *uri)
{
    rank_t r = { 0, 0, 0, 0 };
    pdb_manifest_t m;
    char *text;
    size_t size;

    if (pdb_manifest_read(bundle, &text, &size) != 0)
        return r;
    if (pdb_manifest_scan(text, size, bundle, uri, &m) == 0)
    {
        r.names = m.names;
        r.realises = m.names && m.binaries > 0;
        r.minor = m.minor;
        r.micro = m.micro;
        pdb_manifest_free(&m);
    }
    free(text);
    return r;
}

/* whether a outranks b under newest-version: a realising bundle over one that only names the plugin, then the
 * higher minor version, then the higher micro version; equal is not better, so the first in the search path stays */
static int better(const rank_t *a, const rank_t *b)
{
    if (a->realises != b->realises)
        return a->realises > b->realises;
    if (a->minor != b->minor)
        return a->minor > b->minor;
    return a->micro > b->micro;
}

char *pdb_lv2_bundle(const char *search, const char *uri)
{
    char *copy = strdup(search), *dir, *save = NULL, *best = NULL;
    rank_t top = { 0, 0, 0, 0 };

    if (!copy)
        return NULL;
    for (dir = strtok_r(copy, ":", &save); dir; dir = strtok_r(NULL, ":", &save))
    {
        char base[PATH_MAX], **names;
        int n, k;

        if (search_dir(dir, base, sizeof(base)) != 0 || (n = bundles_of(base, &names)) < 0)
            continue;
        for (k = 0; k < n; k++)
        {
            char bundle[PATH_MAX * 2];
            rank_t r;

            snprintf(bundle, sizeof(bundle), "%s/%s", base, names[k]);
            r = rank_of(bundle, uri);
            if (r.names && (!best || better(&r, &top)))
            {
                char *keep = strdup(bundle);

                if (keep)
                {
                    free(best);
                    best = keep;
                    top = r;
                }
            }
            free(names[k]);
        }
        free(names);
    }
    free(copy);
    return best;
}

/* ---------------------------------------------------------------- the files of a plugin */

static int refuse(char *why, size_t size, const char *path, const char *reason)
{
    if (why && size)
        snprintf(why, size, "%s %s", path, reason);
    return PDB_COLLECT_REFUSED;
}

/* the bundle of the search path that is the plugin, and its manifest with the files it names for it */
static int lv2_collect(const char *uri, const char *search, pdb_collection_t *out, char *why, size_t why_size)
{
    char *bundle = pdb_lv2_bundle(search, uri), *manifest = NULL;
    size_t manifest_size = 0;
    pdb_manifest_t m;
    int n, rc = PDB_COLLECT_OK;

    if (!bundle)
    {
        if (why && why_size)
            snprintf(why, why_size, "no bundle of %s names the plugin", search);
        return PDB_COLLECT_ABSENT;
    }
    snprintf(out->root, sizeof(out->root), "%s", bundle);
    free(bundle);
    if (pdb_manifest_read(out->root, &manifest, &manifest_size) != 0)
        return refuse(why, why_size, PDB_MANIFEST, "can't be read as a manifest");
    /* a host resolves a base, which plugin-db does not, so it cannot know the file the host would load. A name is an
     * IRI: the pin holds it as the manifest writes it, and it is decoded where the file is opened */
    if (pdb_manifest_scan(manifest, manifest_size, out->root, uri, &m) != 0)
    {
        free(manifest);
        return PDB_COLLECT_ERROR;
    }
    if (m.based)
        rc = refuse(why, why_size, PDB_MANIFEST, "sets a base plugin-db does not resolve");
    if (rc == PDB_COLLECT_OK && pdb_names_add(&out->files, PDB_MANIFEST) != 0)
        rc = PDB_COLLECT_ERROR;
    for (n = 0; n < m.files.n && rc == PDB_COLLECT_OK; n++)
    {
        char decoded[PATH_MAX];

        if (pdb_percent_decode(m.files.name[n], decoded, sizeof(decoded)) != 0)
            rc = refuse(why, why_size, m.files.name[n], "is not a percent-encoded name");
        else if (!pdb_path_valid(m.files.name[n]))
            rc = refuse(why, why_size, m.files.name[n], "is not a file of the bundle");
        else if (!pdb_names_has(&out->files, m.files.name[n]) && pdb_names_add(&out->files, m.files.name[n]) != 0)
            rc = PDB_COLLECT_ERROR;
    }
    pdb_manifest_free(&m);
    free(manifest);
    return rc;
}

/* the .clap file the plugin's code is loaded from: its directory, and its name as a pin holds it */
static int clap_collect(const char *file, pdb_collection_t *out, char *why, size_t why_size)
{
    const char *slash = file ? strrchr(file, '/') : NULL;
    char encoded[PATH_MAX];
    struct stat st;

    if (!file || file[0] != '/' || !slash || !slash[1])
        return refuse(why, why_size, file ? file : "(no file)", "is not the absolute path of a .clap file");
    if (stat(file, &st) != 0 || !S_ISREG(st.st_mode))
    {
        if (why && why_size)
            snprintf(why, why_size, "%s is gone or no file", file);
        return PDB_COLLECT_ABSENT;
    }
    if ((size_t)(slash - file) >= sizeof(out->root) || percent_encode(slash + 1, encoded, sizeof(encoded)) != 0)
        return refuse(why, why_size, file, "is too long a name");
    snprintf(out->root, sizeof(out->root), "%.*s", slash == file ? 1 : (int)(slash - file), file);
    return pdb_names_add(&out->files, encoded) == 0 ? PDB_COLLECT_OK : PDB_COLLECT_ERROR;
}

const char *pdb_lv2_search(const char *where)
{
    const char *env = getenv("LV2_PATH");

    return where && *where ? where : env && *env ? env : PDB_STORE_LV2_DEFAULT_SEARCH;
}

int pdb_collect(const pdb_address_t *a, const char *where, pdb_collection_t *out, char *why, size_t why_size)
{
    memset(out, 0, sizeof(*out));
    if (why && why_size)
        why[0] = '\0';
    if (a->format == PDB_ADDRESS_FORMATS_LV2)
        return lv2_collect(a->id, pdb_lv2_search(where), out, why, why_size);
    return clap_collect(where, out, why, why_size);
}

void pdb_collection_free(pdb_collection_t *c)
{
    pdb_names_free(&c->files);
}
