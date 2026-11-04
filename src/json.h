/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com> */
/* JSON as the store's layer files need it: a tree read whole, with the strict grammar of RFC 8259 and nothing more,
 * and a writer that escapes a string. plugin-db depends on libc and libcrypto only, so this is its own. */

#ifndef PDB_JSON_H
#define PDB_JSON_H

#include <stddef.h>

enum { PDB_JSON_NULL, PDB_JSON_FALSE, PDB_JSON_TRUE, PDB_JSON_NUMBER, PDB_JSON_STRING, PDB_JSON_ARRAY, PDB_JSON_OBJECT };

typedef struct PDB_JSON_T {
    int type;
    double number;
    char *string;               /* a string's bytes, NUL-terminated (a string holding NUL is refused) */
    char **keys;                /* an object's keys, in their order */
    struct PDB_JSON_T **items;  /* an array's items or an object's values */
    size_t count;
} pdb_json_t;

/* the tree of text, which must be one value and nothing but whitespace after it; NULL when it is not JSON, nests
 * deeper than 64, or memory runs out */
pdb_json_t *pdb_json_parse(const char *text, size_t size);
void pdb_json_free(pdb_json_t *value);

/* the value of key in an object, NULL when value is no object or has no such key; a repeated key is refused at parse */
const pdb_json_t *pdb_json_get(const pdb_json_t *object, const char *key);
/* the string of key, NULL when it is absent or no string */
const char *pdb_json_string(const pdb_json_t *object, const char *key);

/* s as a JSON string with its quotes into out; the length it needs, as snprintf counts it */
size_t pdb_json_quote(const char *s, char *out, size_t size);

#endif
