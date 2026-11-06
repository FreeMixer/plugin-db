/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com> */

/* The one address form of PLUGIN_DB_ADDRESS_FORMATS: <format>:<id> or <format>@<sha256>:<id>. */

#include <string.h>

#include <plugin-db/db.h>

static const char *const g_formats[] = OMX_PLUGIN_DB_ADDRESS_FORMATS_INIT;

const char *pdb_word(const char *const *words, int count, int index)
{
    return index >= 0 && index < count ? words[index] : "?";
}

int pdb_address_parse(const char *text, pdb_address_t *a)
{
    const char *colon = text ? strchr(text, ':') : NULL, *at, *c;
    size_t len;
    int n;

    memset(a, 0, sizeof(*a));
    a->format = -1;
    if (!colon)
        return -1;
    at = memchr(text, '@', (size_t)(colon - text));
    len = (size_t)((at ? at : colon) - text);
    for (n = 0; n < PDB_ADDRESS_FORMATS_COUNT; n++)
        if (strlen(g_formats[n]) == len && !strncmp(text, g_formats[n], len))
            a->format = n;
    if (a->format < 0)
        return -1;
    /* the bundle rule makes one bundle the plugin, so an LV2 id never needs a pin to tell files apart */
    if (at)
    {
        if (a->format != PDB_ADDRESS_FORMATS_CLAP || colon - at - 1 != PHD_SHA256_HEX_LEN)
            return -1;
        memcpy(a->pin, at + 1, PHD_SHA256_HEX_LEN);
        a->pin[PHD_SHA256_HEX_LEN] = '\0';
        if (!phd_pin_hex_valid(a->pin))
            return -1;
    }
    a->id = colon + 1;
    if (!*a->id || (a->format == PDB_ADDRESS_FORMATS_CLAP && a->id[0] == '/'))
        return -1;
    for (c = a->id; *c; c++)
        if ((unsigned char)*c <= 0x20 || *c == 0x7f)
            return -1;
    return 0;
}
