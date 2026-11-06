/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com> */

/* An LV2 manifest, read for one plugin: the files it names for it with lv2:binary and rdfs:seeAlso, and the version
 * it gives it. The tokenizer is plugin-hostd's (src/lv2.c), moved here with the file collection it serves. */

#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#include <plugin-hostd/protocol.h>

#include "manifest.h"

#define LV2_BINARY       "http://lv2plug.in/ns/lv2core#binary"
#define LV2_MINOR        "http://lv2plug.in/ns/lv2core#minorVersion"
#define LV2_MICRO        "http://lv2plug.in/ns/lv2core#microVersion"
#define RDFS_SEEALSO     "http://www.w3.org/2000/01/rdf-schema#seeAlso"

/* Turtle as a manifest writes it, enough to find the objects of lv2:binary and rdfs:seeAlso whose subject is the
 * plugin: prefixes, IRIs, prefixed names, "a", literals skipped, nested [ ] and ( ) skipped. A name the pin cannot
 * hold (an absolute IRI outside the bundle, a relative one that climbs out of it, an @base) is kept as it is, and no
 * pinned path matches it. */

enum { T_END, T_IRI, T_NAME, T_PUNCT, T_LITERAL, T_DIRECTIVE };

typedef struct SCAN_T {
    const char *p, *end;
    char prefix[64][2][256];
    int nprefix;
    int based;
    int typed;                  /* the literal just read carries a ^^ datatype, whose name is the next token */
} scan_t;

typedef struct TOKEN_T {
    int kind;
    char text[PATH_MAX];
} token_t;

static void skip_space(scan_t *s)
{
    while (s->p < s->end)
    {
        if (*s->p == '#')
            while (s->p < s->end && *s->p != '\n')
                s->p++;
        else if (*s->p == ' ' || *s->p == '\t' || *s->p == '\n' || *s->p == '\r')
            s->p++;
        else
            return;
    }
}

/* past the literal at s->p; *body and *len are its text between the quotes, escapes not undone */
static void skip_literal(scan_t *s, const char **body, size_t *len)
{
    char q = *s->p;
    int triple = s->end - s->p >= 3 && s->p[1] == q && s->p[2] == q;

    s->p += triple ? 3 : 1;
    *body = s->p;
    *len = 0;
    while (s->p < s->end)
    {
        if (*s->p == '\\')
            s->p += 2;
        else if (*s->p == q && (!triple || (s->end - s->p >= 3 && s->p[1] == q && s->p[2] == q)))
        {
            *len = s->p - *body;
            s->p += triple ? 3 : 1;
            break;
        }
        else
            s->p++;
    }
    if (s->p > s->end)
        s->p = s->end;
    if (s->p < s->end && *s->p == '@')
        while (s->p < s->end && *s->p != ' ' && *s->p != '\t' && *s->p != '\n' && !strchr(";,.])", *s->p))
            s->p++;
    else if (s->end - s->p >= 2 && s->p[0] == '^' && s->p[1] == '^')
    {
        s->p += 2;
        s->typed = 1;
    }
}

static void copy_text(token_t *t, const char *from, size_t n)
{
    if (n >= sizeof(t->text))
        n = sizeof(t->text) - 1;
    memcpy(t->text, from, n);
    t->text[n] = '\0';
}

static void next(scan_t *s, token_t *t)
{
    const char *start;

    skip_space(s);
    t->text[0] = '\0';
    if (s->p >= s->end)
    {
        t->kind = T_END;
        return;
    }
    start = s->p;
    if (*s->p == '<')
    {
        while (s->p < s->end && *s->p != '>')
            s->p++;
        copy_text(t, start + 1, s->p - start - 1);
        if (s->p < s->end)
            s->p++;
        t->kind = T_IRI;
        return;
    }
    if (*s->p == '"' || *s->p == '\'')
    {
        const char *body;
        size_t len, used = 0;

        skip_literal(s, &body, &len);
        /* the text with its escapes undone */
        for (; len && used + 1 < sizeof(t->text); body++, len--)
        {
            char c = *body;

            if (c == '\\' && len > 1)
            {
                body++;
                len--;
                c = *body == 'n' ? '\n' : *body == 't' ? '\t' : *body;
            }
            t->text[used++] = c;
        }
        t->text[used] = '\0';
        t->kind = T_LITERAL;
        return;
    }
    if (strchr(".;,[]()", *s->p))
    {
        copy_text(t, s->p++, 1);
        t->kind = T_PUNCT;
        return;
    }
    while (s->p < s->end && !strchr(" \t\r\n<>\"';,[]()#", *s->p))
        s->p++;
    /* a prefixed name never ends with '.': that one ends the statement */
    if (s->p > start + 1 && s->p[-1] == '.')
        s->p--;
    copy_text(t, start, s->p - start);
    t->kind = *start == '@' ? T_DIRECTIVE : T_NAME;
    if (!strcasecmp(t->text, "PREFIX") || !strcasecmp(t->text, "BASE"))
        t->kind = T_DIRECTIVE;
}

/* a prefixed name or "a" as a full IRI */
static void expand(scan_t *s, token_t *t)
{
    char *colon;
    int n;

    if (t->kind != T_NAME)
        return;
    if (!strcmp(t->text, "a"))
    {
        snprintf(t->text, sizeof(t->text), "http://www.w3.org/1999/02/22-rdf-syntax-ns#type");
        t->kind = T_IRI;
        return;
    }
    colon = strchr(t->text, ':');
    if (!colon)
        return;
    for (n = 0; n < s->nprefix; n++)
        if (strlen(s->prefix[n][0]) == (size_t)(colon - t->text) && !strncmp(s->prefix[n][0], t->text, colon - t->text))
        {
            char full[PATH_MAX];

            snprintf(full, sizeof(full), "%s%s", s->prefix[n][1], colon + 1);
            snprintf(t->text, sizeof(t->text), "%s", full);
            t->kind = T_IRI;
            return;
        }
}

static void directive(scan_t *s, token_t *t)
{
    int sparql = t->text[0] != '@';
    token_t name, iri;

    if (!strcasecmp(t->text + !sparql, "base"))
    {
        s->based = 1;
        next(s, &iri);
    }
    else
    {
        next(s, &name);
        next(s, &iri);
        if (name.kind == T_NAME && iri.kind == T_IRI && s->nprefix < 64 && name.text[0] &&
            name.text[strlen(name.text) - 1] == ':')
        {
            name.text[strlen(name.text) - 1] = '\0';
            /* a name or namespace past 255 bytes is cut there, as plugin-hostd cut it */
            snprintf(s->prefix[s->nprefix][0], sizeof(s->prefix[0][0]), "%.255s", name.text);
            snprintf(s->prefix[s->nprefix][1], sizeof(s->prefix[0][1]), "%.255s", iri.text);
            s->nprefix++;
        }
    }
    if (!sparql)
    {
        skip_space(s);
        if (s->p < s->end && *s->p == '.')
            s->p++;
    }
}

/* the object of lv2:binary or rdfs:seeAlso as a path in the bundle, or the IRI as written when it is not one. A file://
 * IRI writes the bundle's directory percent-encoded ("my amp.lv2" is my%20amp.lv2): it is decoded once with the
 * protocol's phd_pin_path_decode before it is matched, and the name in the bundle is written back as one pin word */
static void bundle_path(const scan_t *s, const char *bundle, const char *iri, char *out, size_t size)
{
    char file[PATH_MAX + 8], decoded[PATH_MAX * 3];

    snprintf(file, sizeof(file), "file://%s/", bundle);
    if (!strncmp(iri, "file://", 7) && phd_pin_path_decode(iri, decoded, sizeof(decoded)) == 0 &&
        !strncmp(decoded, file, strlen(file)))
    {
        if (phd_pin_path_encode(decoded + strlen(file), out, size) != 0)
            snprintf(out, size, "%s", iri);
        return;
    }
    else if (strchr(iri, ':') || s->based)
    {
        snprintf(out, size, "%s", iri);
        return;
    }
    while (!strncmp(iri, "./", 2))
        iri += 2;
    snprintf(out, size, "%s", iri);
}

enum { WANT_SUBJECT, WANT_PREDICATE, WANT_OBJECT, AFTER_OBJECT };
enum { P_OTHER, P_BINARY, P_SEEALSO, P_MINOR, P_MICRO };

/* an integer object as a manifest writes a version: a bare 2, or "2" with or without a datatype */
static long version_of(const token_t *t)
{
    char *end;
    long v;

    if (t->kind != T_NAME && t->kind != T_LITERAL)
        return -1;
    v = strtol(t->text, &end, 10);
    return end != t->text && !*end && v >= 0 ? v : -1;
}

int pdb_manifest_scan(const char *text, size_t size, const char *bundle, const char *uri, pdb_manifest_t *out)
{
    scan_t *s = calloc(1, sizeof(scan_t));
    token_t t;
    int state = WANT_SUBJECT, ours = 0, depth = 0, pred = P_OTHER, short_of_memory = 0;

    memset(out, 0, sizeof(*out));
    if (!s)
        return -1;
    s->p = text;
    s->end = text + size;
    for (next(s, &t); t.kind != T_END; next(s, &t))
    {
        if (t.kind == T_PUNCT && (t.text[0] == '[' || t.text[0] == '('))
        {
            if (depth++ == 0 && state == WANT_SUBJECT)
                ours = 0;
            continue;
        }
        if (t.kind == T_PUNCT && (t.text[0] == ']' || t.text[0] == ')'))
        {
            if (depth && --depth == 0)
                state = state == WANT_SUBJECT ? WANT_PREDICATE : AFTER_OBJECT;
            continue;
        }
        if (depth)
            continue;
        if (t.kind == T_DIRECTIVE && state == WANT_SUBJECT)
        {
            directive(s, &t);
            continue;
        }
        expand(s, &t);
        if (t.kind == T_PUNCT && t.text[0] == '.')
            state = WANT_SUBJECT;
        else if (t.kind == T_PUNCT && t.text[0] == ';')
            state = WANT_PREDICATE;
        else if (t.kind == T_PUNCT && t.text[0] == ',')
            state = WANT_OBJECT;
        else if (state == WANT_SUBJECT)
        {
            ours = t.kind == T_IRI && !strcmp(t.text, uri);
            out->names |= ours;
            state = WANT_PREDICATE;
        }
        else if (state == WANT_PREDICATE)
        {
            pred = t.kind != T_IRI ? P_OTHER
                   : !strcmp(t.text, LV2_BINARY) ? P_BINARY
                   : !strcmp(t.text, RDFS_SEEALSO) ? P_SEEALSO
                   : !strcmp(t.text, LV2_MINOR) ? P_MINOR
                   : !strcmp(t.text, LV2_MICRO) ? P_MICRO : P_OTHER;
            state = WANT_OBJECT;
        }
        else if (state == WANT_OBJECT)
        {
            if (ours && (pred == P_BINARY || pred == P_SEEALSO))
            {
                char path[PATH_MAX];

                bundle_path(s, bundle, t.text, path, sizeof(path));
                short_of_memory |= pdb_names_add(&out->files, path) != 0;
                out->binaries += pred == P_BINARY;
            }
            else if (ours && pred == P_MINOR)
                out->minor = version_of(&t);
            else if (ours && pred == P_MICRO)
                out->micro = version_of(&t);
            state = AFTER_OBJECT;
        }
    }
    out->based = s->based;
    free(s);
    if (out->minor < 0)
        out->minor = 0;
    if (out->micro < 0)
        out->micro = 0;
    if (short_of_memory)
    {
        pdb_names_free(&out->files);
        return -1;
    }
    return 0;
}

void pdb_manifest_free(pdb_manifest_t *m)
{
    pdb_names_free(&m->files);
}

int pdb_manifest_read(const char *root, char **bytes, size_t *size)
{
    char full[PATH_MAX * 2];
    struct stat st;
    ssize_t got = 0, r;
    int fd;

    *bytes = NULL;
    *size = 0;
    snprintf(full, sizeof(full), "%s/" PDB_MANIFEST, root);
    fd = open(full, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return -1;
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size > PDB_MANIFEST_MAX)
    {
        close(fd);
        return -1;
    }
    *bytes = malloc((size_t)st.st_size + 1);
    if (!*bytes)
    {
        close(fd);
        return -1;
    }
    while (got < st.st_size && (r = read(fd, *bytes + got, (size_t)(st.st_size - got))) > 0)
        got += r;
    close(fd);
    (*bytes)[got] = '\0';
    *size = (size_t)got;
    if (got == st.st_size)
        return 0;
    free(*bytes);
    *bytes = NULL;
    return -1;
}
