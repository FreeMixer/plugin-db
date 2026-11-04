// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>

/* libFuzzer harness: THE TEXT OF A PIN (include/plugin-db/pin.h): a digest, a layout pin
 * "<scheme>:<sha256>", and the verdict and pool words a pin can carry. These are the readers
 * of a pin line a host reads back from its store, so they take any bytes.
 *
 * Input: the bytes of one string, copied into an allocation of exactly its length plus the NUL,
 * so a read past the string is a sanitizer report. A byte 00 ends the string where it stands.
 * Every reader is run on every input: a digest, a layout pin, a verdict, a pool word.
 *
 * Every expected value is the harness's own reading of the format (fuzz/spec.h and the words
 * of data/pin.json written out below), not a call of the header.
 *
 * Properties, each an abort() in a check_ function:
 *   - digest charset: a string of 64 characters is a digest exactly when each is 0-9 or a-f.
 *   - digest length: a string of any other length is never a digest.
 *   - token refusal: a layout pin is refused exactly when it has no ':', the last ':' is its
 *     first character, the scheme does not fit the buffer, or what follows the last ':' is not
 *     a digest.
 *   - token scheme: the scheme is what precedes the last ':'.
 *   - token digest: the digest is what follows the last ':'.
 *   - verdict: a verdict is exactly one of the words data/pin.json declares, whole.
 *   - pool: a pool word is exactly one of the words data/pin.json declares, whole.
 */
#include <stdio.h>

#include <plugin-db/pin.h>

#include "fuzz.h"
#include "spec.h"

/* the buffer the scheme goes to is one byte larger than the size the reader is told, so a
 * reader that writes one byte too many does not reach a sanitizer before the check does */
#define SCHEME_SIZE 32

/* contract: data/pin.json PLUGIN_DB_PIN_VERDICTS, PLUGIN_DB_PIN_POOLS */
static const char *const spec_verdicts[] = { "in-process", "isolated", "refused" };
static const char *const spec_pools[] = { "cores", "off" };

static int spec_member(const char *s, const char *const *words, size_t count)
{
	for (size_t i = 0; i < count; i++)
		if (strcmp(s, words[i]) == 0)
			return 1;
	return 0;
}

FUZZ_CHECK check_digest_charset(const char *s)
{
	/* Of 64 characters, it is a digest exactly when each is a lowercase hex digit.
	 * contract: include/plugin-db/pin.h:48; fuzz/spec.h spec_hex_ok
	 * mutant: include/plugin-db/pin.h:140 `s[n] <= 'f'` -> `s[n] <= 'g'`
	 * input: digest-g
	 * mutant: include/plugin-db/pin.h:140 `s[n] >= 'a'` -> `s[n] >= 'A'`
	 * input: digest-upper
	 * mutant: include/plugin-db/pin.h:140 `s[n] <= '9'` -> `s[n] <= ':'`
	 * input: digest-colon */
	if (strlen(s) == SPEC_HEX_LEN && phd_pin_hex_valid(s) != spec_hex_ok(s))
		abort();
}

FUZZ_CHECK check_digest_length(const char *s)
{
	/* A string that is not 64 characters long is never a digest.
	 * contract: include/plugin-db/pin.h:48
	 * mutant: include/plugin-db/pin.h:142 `s[n] == '\0'` -> `1`
	 * input: digest-65
	 * mutant: include/plugin-db/pin.h:139 `n < PHD_SHA256_HEX_LEN` -> `n < PHD_SHA256_HEX_LEN - 1`
	 * input: digest-63 */
	if (strlen(s) != SPEC_HEX_LEN && phd_pin_hex_valid(s))
		abort();
}

FUZZ_CHECK check_token_refusal(const char *s, const char *colon, int rc)
{
	int accept = colon && colon != s && (size_t)(colon - s) < SCHEME_SIZE && spec_hex_ok(colon + 1);

	/* A layout pin is refused exactly when it is not "<scheme>:<digest>" with a scheme that is
	 * not empty and fits, split at the LAST ':'.
	 * contract: include/plugin-db/pin.h:145 `split at its last ':'; 0, or -1 when it is not one`
	 * mutant: include/plugin-db/pin.h:152 `colon == token || ` -> ``
	 * input: token-empty-scheme
	 * mutant: include/plugin-db/pin.h:152 `phd_pin_hex_valid(colon + 1)` -> `phd_pin_hex_valid(colon)`
	 * input: token-ok
	 * mutant: include/plugin-db/pin.h:149 `strrchr` -> `strchr`
	 * input: token-two-colons
	 * mutant: include/plugin-db/pin.h:155 `n >= scheme_size` -> `n > scheme_size`
	 * input: token-scheme-32 */
	if ((rc == 0) != accept)
		abort();
}

FUZZ_CHECK check_token_scheme(const char *s, const char *colon, const char *scheme)
{
	/* What the reader gives as the scheme is the characters before the last ':', then a NUL.
	 * contract: include/plugin-db/pin.h:145
	 * mutant: include/plugin-db/pin.h:157 `token` -> `token + 1`
	 * input: token-ok
	 * mutant: include/plugin-db/pin.h:158 `'\0'` -> `'x'`
	 * input: token-ok */
	if (strlen(scheme) != (size_t)(colon - s) || memcmp(scheme, s, (size_t)(colon - s)) != 0)
		abort();
}

FUZZ_CHECK check_token_digest(const char *colon, const char *hex)
{
	/* What the reader gives as the digest is the 64 characters after the last ':', then a NUL.
	 * contract: include/plugin-db/pin.h:145
	 * mutant: include/plugin-db/pin.h:159 `colon + 1` -> `colon`
	 * input: token-ok
	 * mutant: include/plugin-db/pin.h:159 `PHD_SHA256_HEX_LEN + 1` -> `PHD_SHA256_HEX_LEN`
	 * input: token-ok */
	if (memcmp(hex, colon + 1, SPEC_HEX_LEN + 1) != 0)
		abort();
}

FUZZ_CHECK check_verdict(const char *s)
{
	/* A verdict is one of the declared words, whole: not a prefix of one, not an extension.
	 * contract: data/pin.json PLUGIN_DB_PIN_VERDICTS
	 * mutant: include/plugin-db/pin.h:179 `phd_pin_word_in(s, words, OMX_PLUGIN_DB_PIN_VERDICTS_COUNT)` -> `1`
	 * input: verdict-bogus
	 * mutant: include/plugin-db/pin.h:179 `OMX_PLUGIN_DB_PIN_VERDICTS_COUNT` -> `OMX_PLUGIN_DB_PIN_VERDICTS_COUNT - 1`
	 * input: verdict-refused
	 * mutant: include/plugin-db/pin.h:169 `!strcmp(s, words[n])` -> `!strncmp(s, words[n], strlen(s))`
	 * input: verdict-prefix */
	if (phd_pin_verdict_valid(s) != spec_member(s, spec_verdicts, 3))
		abort();
}

FUZZ_CHECK check_pool(const char *s)
{
	/* A pool word is one of the declared words, whole.
	 * contract: data/pin.json PLUGIN_DB_PIN_POOLS
	 * mutant: include/plugin-db/pin.h:187 `phd_pin_word_in(s, words, OMX_PLUGIN_DB_PIN_POOLS_COUNT)` -> `1`
	 * input: pool-bogus
	 * mutant: include/plugin-db/pin.h:187 `OMX_PLUGIN_DB_PIN_POOLS_COUNT` -> `OMX_PLUGIN_DB_PIN_POOLS_COUNT - 1`
	 * input: pool-off */
	if (phd_pin_pool_valid(s) != spec_member(s, spec_pools, 2))
		abort();
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	char scheme[SCHEME_SIZE + 1], hex[SPEC_HEX_LEN + 1];
	const char *colon;
	char *s;
	int rc;

	if (size >= FUZZ_INPUT_MAX)
		return -1;
	s = fuzz_strdup(data, size);

	check_digest_charset(s);
	check_digest_length(s);

	memset(scheme, 'x', sizeof scheme);
	memset(hex, 'x', sizeof hex);
	rc = phd_pin_layout_parse(s, scheme, SCHEME_SIZE, hex);
	colon = strrchr(s, ':');
	check_token_refusal(s, colon, rc);
	if (rc == 0) {
		check_token_scheme(s, colon, scheme);
		check_token_digest(colon, hex);
	}

	check_verdict(s);
	check_pool(s);

	free(s);
	return 0;
}
