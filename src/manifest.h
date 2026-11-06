/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com> */

#ifndef PDB_MANIFEST_H
#define PDB_MANIFEST_H

#include <stddef.h>

#include "names.h"

#define PDB_MANIFEST     "manifest.ttl"
#define PDB_MANIFEST_MAX (4 * 1024 * 1024)

/* what one manifest says of one plugin */
typedef struct PDB_MANIFEST_T {
    int names;                  /* a statement has the plugin for its subject */
    pdb_names_t files;          /* the objects of lv2:binary and rdfs:seeAlso, as paths in the bundle, in their order */
    int binaries;               /* how many of them are lv2:binary: the bundle realises the plugin */
    long minor, micro;          /* lv2:minorVersion and lv2:microVersion, 0 when not stated */
    int based;                  /* the manifest sets a base, which is not resolved */
} pdb_manifest_t;

/* the manifest text of the bundle for the plugin uri; 0, or -1 when memory ran out */
int pdb_manifest_scan(const char *text, size_t size, const char *bundle, const char *uri, pdb_manifest_t *out);
void pdb_manifest_free(pdb_manifest_t *manifest);

/* the bundle's manifest.ttl, malloc'd and NUL-terminated; 0, or -1 when it is no regular file, past
 * PDB_MANIFEST_MAX or cannot be read */
int pdb_manifest_read(const char *bundle, char **bytes, size_t *size);

#endif
