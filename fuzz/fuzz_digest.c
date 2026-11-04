// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>

/* libFuzzer harness: THE SHA-256 OF BYTES (include/plugin-db/pin.h, phd_sha256_*), the digest
 * every pin is made of, over any bytes. Nothing here opens a descriptor: phd_pin_fd_digest()
 * reads a descriptor, and these are the steps it takes (init, update, final) on bytes the
 * harness holds.
 *
 * Input: byte 0 is the mode, the rest is the message, copied into an allocation of exactly its
 * length. The message is hashed in one call and again in chunks, whose size is the low seven
 * bits of the mode plus one; bit 7 of the mode also gives up a digest half made
 * (phd_sha256_abandon) and asks for its end. The harness hashes the message itself with its
 * own SHA-256 of FIPS 180-4 (fuzz/spec.h).
 *
 * Properties, each an abort() in a check_ function:
 *   - value: the digest of a message is the SHA-256 of it, in one call.
 *   - chunks: it is the same however the message is cut into updates.
 *   - form: it is 64 lowercase hex digits and a NUL, written whole.
 *   - state: a digest finished leaves no context behind.
 *   - failed: a digest given up before its end is refused with -1 and an empty string, never
 *     with the digest of what it had.
 */
#include <plugin-db/pin.h>

#include "fuzz.h"
#include "spec.h"

FUZZ_CHECK check_value(const char *got, const char *want)
{
	/* The digest of the message is its SHA-256.
	 * contract: include/plugin-db/pin.h:81; FIPS 180-4 section 6.2
	 * mutant: include/plugin-db/pin.h:108 `data, size` -> `data, size / 2`
	 * input: abc
	 * mutant: include/plugin-db/pin.h:96 `md[n] >> 4` -> `md[n] & 15`
	 * input: abc
	 * mutant: include/plugin-db/pin.h:97 `md[n] & 15` -> `md[n] & 7`
	 * input: abc */
	if (strcmp(got, want) != 0)
		abort();
}

FUZZ_CHECK check_chunks(const char *got, const char *want)
{
	/* Feeding the message in pieces gives the same digest as feeding it whole.
	 * contract: include/plugin-db/pin.h:68 (update appends)
	 * mutant: include/plugin-db/pin.h:70 `data, size` -> `data, size > 1 ? size : 0`
	 * input: chunked-1 */
	if (strcmp(got, want) != 0)
		abort();
}

FUZZ_CHECK check_form(const char *hex, int rc)
{
	/* A digest is 64 lowercase hex digits and a NUL, and the call that wrote it returned 0.
	 * contract: include/plugin-db/pin.h:81
	 * mutant: include/plugin-db/pin.h:99 `hex[PHD_SHA256_HEX_LEN] = '\0';` -> ``
	 * input: abc
	 * mutant: include/plugin-db/pin.h:84 `0123456789abcdef` -> `0123456789ABCDEF`
	 * input: abc */
	if (rc != 0 || !spec_hex_ok(hex))
		abort();
}

FUZZ_CHECK check_state(const phd_sha256_t *c)
{
	/* A finished digest has freed its context and holds none.
	 * contract: include/plugin-db/pin.h:82
	 * mutant: include/plugin-db/pin.h:90 `c->ctx = NULL;` -> ``
	 * input: abc */
	if (c->ctx != NULL)
		abort();
}

FUZZ_CHECK check_failed(const phd_sha256_t *c, const char *hex, int rc)
{
	/* A digest given up is refused: -1, the string empty, no context kept.
	 * contract: include/plugin-db/pin.h:81 `0, or -1 (and an empty string) when libcrypto failed`
	 * mutant: include/plugin-db/pin.h:91 `hex[0] = '\0';` -> ``
	 * input: abandoned
	 * mutant: include/plugin-db/pin.h:92 `!c->ok` -> `0`
	 * input: abandoned */
	if (rc != -1 || hex[0] != '\0' || c->ctx != NULL)
		abort();
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	char got[SPEC_HEX_LEN + 2], whole[SPEC_HEX_LEN + 2], want[SPEC_HEX_LEN + 1];
	struct fuzz_in in = fuzz_in(data, size);
	unsigned mode = fuzz_u8(&in);
	size_t chunk = (mode & 0x7f) + 1, n = in.n, at;
	const uint8_t *msg = in.p;
	phd_sha256_t c;
	uint8_t *copy;
	int rc;

	if (size >= FUZZ_INPUT_MAX)
		return -1;
	copy = fuzz_alloc(n);
	if (n)
		memcpy(copy, msg, n);
	spec_sha256(copy, n, want);

	/* the message whole */
	memset(whole, 'x', sizeof whole - 1);
	whole[sizeof whole - 1] = '\0';
	rc = phd_sha256_hex(copy, n, whole);
	check_form(whole, rc);
	check_value(whole, want);

	/* the message in pieces */
	memset(got, 'x', sizeof got - 1);
	got[sizeof got - 1] = '\0';
	phd_sha256_init(&c);
	for (at = 0; at < n; at += chunk)
		phd_sha256_update(&c, copy + at, n - at < chunk ? n - at : chunk);
	rc = phd_sha256_final(&c, got);
	check_state(&c);
	check_form(got, rc);
	check_chunks(got, want);

	/* a digest given up */
	if (mode & 0x80) {
		memset(got, 'x', sizeof got - 1);
		got[sizeof got - 1] = '\0';
		phd_sha256_init(&c);
		phd_sha256_update(&c, copy, n);
		phd_sha256_abandon(&c);
		rc = phd_sha256_final(&c, got);
		check_failed(&c, got, rc);
	}

	free(copy);
	return 0;
}
