/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com> */

/* libFuzzer harness over what plugin-db parses from text: a digest, a layout pin "<scheme>:<sha256>", a verdict word and
 * a pool word. Each input is held as a C string (the parsers take one); every verdict is checked against the rule
 * itself, written again here without the header's code. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <plugin-db/pin.h>

static void fail(const char *what)
{
    fprintf(stderr, "fuzz_pin: %s\n", what);
    abort();
}

static int is_hex_digit(char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }

/* the digest rule, written here and not taken from the header under test */
static int hex_ok(const char *s)
{
    size_t n;

    if (strlen(s) != PHD_SHA256_HEX_LEN)
        return 0;
    for (n = 0; n < PHD_SHA256_HEX_LEN; n++)
        if (!is_hex_digit(s[n]))
            return 0;
    return 1;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    char *s = malloc(size + 1);
    char scheme[32], hex[PHD_SHA256_HEX_LEN + 1];
    size_t n;
    int ok, want;
    const char *colon;

    if (!s)
        return 0;
    memcpy(s, data, size);
    s[size] = '\0';

    /* a digest is exactly 64 lowercase hex digits */
    want = hex_ok(s);
    if (phd_pin_hex_valid(s) != want)
        fail("phd_pin_hex_valid disagrees with the rule");

    /* a layout pin: split at the last ':', a non-empty scheme that fits, a valid digest after it */
    ok = phd_pin_layout_parse(s, scheme, sizeof(scheme), hex) == 0;
    colon = strrchr(s, ':');
    {
        int expect = colon && colon != s && (size_t)(colon - s) < sizeof(scheme) && hex_ok(colon + 1);

        if (ok != expect)
            fail("phd_pin_layout_parse disagrees with the rule");
        if (ok)
        {
            if (strlen(scheme) != (size_t)(colon - s) || memcmp(scheme, s, strlen(scheme)) != 0)
                fail("the scheme is not what precedes the last ':'");
            if (strcmp(hex, colon + 1) != 0 || !hex_ok(hex))
                fail("the digest is not what follows the last ':'");
        }
    }

    /* a verdict or a pool word is one of the declared words, whole */
    {
        static const char *const verdicts[] = OMX_PLUGIN_DB_PIN_VERDICTS_INIT;
        static const char *const pools[] = OMX_PLUGIN_DB_PIN_POOLS_INIT;
        int v = 0, p = 0;

        for (n = 0; n < OMX_PLUGIN_DB_PIN_VERDICTS_COUNT; n++)
            v |= strcmp(s, verdicts[n]) == 0;
        for (n = 0; n < OMX_PLUGIN_DB_PIN_POOLS_COUNT; n++)
            p |= strcmp(s, pools[n]) == 0;
        if (phd_pin_verdict_valid(s) != v)
            fail("phd_pin_verdict_valid disagrees with the declared words");
        if (phd_pin_pool_valid(s) != p)
            fail("phd_pin_pool_valid disagrees with the declared words");
    }

    free(s);
    return 0;
}
