/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com> */

#ifndef PDB_COLLECT_H
#define PDB_COLLECT_H

#include <stddef.h>

#include "names.h"


/* root/path as a file name, path decoded, and no '/' that was escaped; 0, or -1 */
int pdb_full_path(const char *root, const char *path, char *full, size_t size);

/* a path of a pin: relative to its root, never out of it, and none of the separators of a pin line */
int pdb_path_valid(const char *path);

/* the LV2 search path: where, else LV2_PATH, else PDB_STORE_LV2_DEFAULT_SEARCH */
const char *pdb_lv2_search(const char *where);

#endif
