/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com> */
/* a list of names: the files a format loads for a plugin (plugin-hostd's names.h, moved) */

#ifndef PDB_NAMES_H
#define PDB_NAMES_H

#include <stdlib.h>
#include <string.h>

#include <plugin-db/db.h>

/* 0, or -1 with the list as it was when memory runs out */
static inline int pdb_names_add(pdb_names_t *names, const char *name)
{
    char **grown = realloc(names->name, (names->n + 1) * sizeof(char *));
    char *copy;

    if (!grown)
        return -1;
    names->name = grown;
    copy = strdup(name);
    if (!copy)
        return -1;
    names->name[names->n++] = copy;
    return 0;
}

static inline int pdb_names_has(const pdb_names_t *names, const char *name)
{
    int n;

    for (n = 0; n < names->n; n++)
        if (!strcmp(names->name[n], name))
            return 1;
    return 0;
}

static inline void pdb_names_free(pdb_names_t *names)
{
    while (names->n)
        free(names->name[--names->n]);
    free(names->name);
    names->name = NULL;
}

#endif
