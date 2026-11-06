/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com> */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "json.h"

#define DEPTH_MAX 64

typedef struct PARSER_T {
    const char *p, *end;
} parser_t;

static void space(parser_t *s)
{
    while (s->p < s->end && (*s->p == ' ' || *s->p == '\t' || *s->p == '\n' || *s->p == '\r'))
        s->p++;
}

static int literal(parser_t *s, const char *word)
{
    size_t n = strlen(word);

    if ((size_t)(s->end - s->p) < n || memcmp(s->p, word, n))
        return 0;
    s->p += n;
    return 1;
}

static int hex4(const char *p, unsigned *out)
{
    unsigned v = 0;
    int n;

    for (n = 0; n < 4; n++)
    {
        char c = p[n];

        v <<= 4;
        if (c >= '0' && c <= '9')
            v |= (unsigned)(c - '0');
        else if (c >= 'a' && c <= 'f')
            v |= (unsigned)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F')
            v |= (unsigned)(c - 'A' + 10);
        else
            return -1;
    }
    *out = v;
    return 0;
}

static size_t utf8(unsigned cp, char *out)
{
    if (cp < 0x80)
    {
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800)
    {
        out[0] = (char)(0xc0 | cp >> 6);
        out[1] = (char)(0x80 | (cp & 0x3f));
        return 2;
    }
    if (cp < 0x10000)
    {
        out[0] = (char)(0xe0 | cp >> 12);
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3f));
        out[2] = (char)(0x80 | (cp & 0x3f));
        return 3;
    }
    out[0] = (char)(0xf0 | cp >> 18);
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3f));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3f));
    out[3] = (char)(0x80 | (cp & 0x3f));
    return 4;
}

/* the string at s->p, its escapes undone, malloc'd; NULL when it is no string */
static char *string(parser_t *s)
{
    const char *start;
    char *out, *o;

    if (s->p >= s->end || *s->p != '"')
        return NULL;
    start = ++s->p;
    while (s->p < s->end && *s->p != '"')
    {
        if ((unsigned char)*s->p < 0x20)
            return NULL;
        s->p += *s->p == '\\' ? 2 : 1;
    }
    if (s->p >= s->end)
        return NULL;
    out = o = malloc((size_t)(s->p - start) + 1);
    if (!out)
        return NULL;
    for (const char *c = start; c < s->p; c++)
    {
        unsigned cp, lo;

        if (*c != '\\')
        {
            *o++ = *c;
            continue;
        }
        switch (*++c)
        {
        case '"': *o++ = '"'; break;
        case '\\': *o++ = '\\'; break;
        case '/': *o++ = '/'; break;
        case 'b': *o++ = '\b'; break;
        case 'f': *o++ = '\f'; break;
        case 'n': *o++ = '\n'; break;
        case 'r': *o++ = '\r'; break;
        case 't': *o++ = '\t'; break;
        case 'u':
            if (s->p - c < 5 || hex4(c + 1, &cp) != 0)
                goto bad;
            c += 4;
            if (cp >= 0xd800 && cp < 0xdc00)
            {
                if (s->p - c < 7 || c[1] != '\\' || c[2] != 'u' || hex4(c + 3, &lo) != 0 || lo < 0xdc00 || lo >= 0xe000)
                    goto bad;
                cp = 0x10000 + ((cp - 0xd800) << 10) + (lo - 0xdc00);
                c += 6;
            }
            else if (cp >= 0xdc00 && cp < 0xe000)
                goto bad;
            if (!cp)
                goto bad;
            o += utf8(cp, o);
            break;
        default:
            goto bad;
        }
    }
    *o = '\0';
    s->p++;
    return out;
bad:
    free(out);
    return NULL;
}

static pdb_json_t *value(parser_t *s, int depth);

static int push(pdb_json_t *v, char *key, pdb_json_t *item)
{
    pdb_json_t **items = realloc(v->items, (v->count + 1) * sizeof(*items));

    if (!items)
        return -1;
    v->items = items;
    if (v->type == PDB_JSON_OBJECT)
    {
        char **keys = realloc(v->keys, (v->count + 1) * sizeof(*keys));

        if (!keys)
            return -1;
        v->keys = keys;
        v->keys[v->count] = key;
    }
    v->items[v->count++] = item;
    return 0;
}

static pdb_json_t *container(parser_t *s, int depth, int object)
{
    pdb_json_t *v = calloc(1, sizeof(*v));
    char close = object ? '}' : ']';

    if (!v)
        return NULL;
    v->type = object ? PDB_JSON_OBJECT : PDB_JSON_ARRAY;
    s->p++;
    space(s);
    if (s->p < s->end && *s->p == close)
    {
        s->p++;
        return v;
    }
    for (;;)
    {
        char *key = NULL;
        pdb_json_t *item;

        space(s);
        if (object)
        {
            if (!(key = string(s)) || pdb_json_get(v, key))
                goto bad_key;
            space(s);
            if (s->p >= s->end || *s->p != ':')
                goto bad_key;
            s->p++;
        }
        if (!(item = value(s, depth + 1)))
            goto bad_key;
        if (push(v, key, item) != 0)
        {
            pdb_json_free(item);
            goto bad_key;
        }
        space(s);
        if (s->p < s->end && *s->p == ',')
        {
            s->p++;
            continue;
        }
        if (s->p < s->end && *s->p == close)
        {
            s->p++;
            return v;
        }
        goto bad;
bad_key:
        free(key);
        goto bad;
    }
bad:
    pdb_json_free(v);
    return NULL;
}

static pdb_json_t *number(parser_t *s)
{
    const char *start = s->p;
    pdb_json_t *v;
    char buffer[64];

    if (s->p < s->end && *s->p == '-')
        s->p++;
    if (s->p < s->end && *s->p == '0')
        s->p++;
    else if (s->p < s->end && *s->p >= '1' && *s->p <= '9')
        while (s->p < s->end && *s->p >= '0' && *s->p <= '9')
            s->p++;
    else
        return NULL;
    if (s->p < s->end && *s->p == '.')
    {
        s->p++;
        if (s->p >= s->end || *s->p < '0' || *s->p > '9')
            return NULL;
        while (s->p < s->end && *s->p >= '0' && *s->p <= '9')
            s->p++;
    }
    if (s->p < s->end && (*s->p == 'e' || *s->p == 'E'))
    {
        s->p++;
        if (s->p < s->end && (*s->p == '+' || *s->p == '-'))
            s->p++;
        if (s->p >= s->end || *s->p < '0' || *s->p > '9')
            return NULL;
        while (s->p < s->end && *s->p >= '0' && *s->p <= '9')
            s->p++;
    }
    if ((size_t)(s->p - start) >= sizeof(buffer) || !(v = calloc(1, sizeof(*v))))
        return NULL;
    memcpy(buffer, start, (size_t)(s->p - start));
    buffer[s->p - start] = '\0';
    v->type = PDB_JSON_NUMBER;
    v->number = strtod(buffer, NULL);
    return v;
}

static pdb_json_t *value(parser_t *s, int depth)
{
    pdb_json_t *v;

    if (depth > DEPTH_MAX)
        return NULL;
    space(s);
    if (s->p >= s->end)
        return NULL;
    if (*s->p == '{' || *s->p == '[')
        return container(s, depth, *s->p == '{');
    if (*s->p == '"')
    {
        char *text = string(s);

        if (!text || !(v = calloc(1, sizeof(*v))))
        {
            free(text);
            return NULL;
        }
        v->type = PDB_JSON_STRING;
        v->string = text;
        return v;
    }
    if (*s->p == '-' || (*s->p >= '0' && *s->p <= '9'))
        return number(s);
    if (!(v = calloc(1, sizeof(*v))))
        return NULL;
    if (literal(s, "null"))
        v->type = PDB_JSON_NULL;
    else if (literal(s, "true"))
        v->type = PDB_JSON_TRUE;
    else if (literal(s, "false"))
        v->type = PDB_JSON_FALSE;
    else
    {
        free(v);
        return NULL;
    }
    return v;
}

pdb_json_t *pdb_json_parse(const char *text, size_t size)
{
    parser_t s = { text, text + size };
    pdb_json_t *v = value(&s, 0);

    space(&s);
    if (v && s.p != s.end)
    {
        pdb_json_free(v);
        return NULL;
    }
    return v;
}

void pdb_json_free(pdb_json_t *v)
{
    size_t n;

    if (!v)
        return;
    for (n = 0; n < v->count; n++)
    {
        pdb_json_free(v->items[n]);
        if (v->keys)
            free(v->keys[n]);
    }
    free(v->items);
    free(v->keys);
    free(v->string);
    free(v);
}

const pdb_json_t *pdb_json_get(const pdb_json_t *v, const char *key)
{
    size_t n;

    if (!v || v->type != PDB_JSON_OBJECT)
        return NULL;
    for (n = 0; n < v->count; n++)
        if (!strcmp(v->keys[n], key))
            return v->items[n];
    return NULL;
}

const char *pdb_json_string(const pdb_json_t *v, const char *key)
{
    const pdb_json_t *s = pdb_json_get(v, key);

    return s && s->type == PDB_JSON_STRING ? s->string : NULL;
}

size_t pdb_json_quote(const char *s, char *out, size_t size)
{
    size_t n = 0;

#define PUT(c) do { if (n + 1 < size) out[n] = (c); n++; } while (0)
    PUT('"');
    for (; *s; s++)
    {
        unsigned char c = (unsigned char)*s;

        if (c == '"' || c == '\\')
        {
            PUT('\\');
            PUT((char)c);
        }
        else if (c < 0x20)
        {
            char esc[8];
            int k;

            snprintf(esc, sizeof(esc), "\\u%04x", c);
            for (k = 0; esc[k]; k++)
                PUT(esc[k]);
        }
        else
            PUT((char)c);
    }
    PUT('"');
#undef PUT
    if (size)
        out[n < size ? n : size - 1] = '\0';
    return n;
}
