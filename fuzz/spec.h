// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>

/* The format as the harnesses hold it, written from the comment at the head of
 * include/plugin-db/pin.h and from FIPS 180-4, not from the functions under test: a check
 * that takes its expected value from the code it judges holds for a header that is wrong the
 * same way. Everything here uses libc only (snprintf for the numbers), never the header's
 * own helpers and never libcrypto. */
#ifndef PLUGIN_DB_FUZZ_SPEC_H
#define PLUGIN_DB_FUZZ_SPEC_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* contract: pin.h:48 (a digest is 64 lowercase hex digits) */
#define SPEC_HEX_LEN 64

static inline int spec_is_hex_digit(char c)
{
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

/* whether s is a digest as a pin writes it */
static inline int spec_hex_ok(const char *s)
{
	size_t n;

	if (strlen(s) != SPEC_HEX_LEN)
		return 0;
	for (n = 0; n < SPEC_HEX_LEN; n++)
		if (!spec_is_hex_digit(s[n]))
			return 0;
	return 1;
}

/* ---------------------------------------------------------------- SHA-256 (FIPS 180-4) */

static inline uint32_t spec_rotr(uint32_t x, unsigned n) { return (x >> n) | (x << (32 - n)); }

static const uint32_t spec_k[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

static inline void spec_sha256_block(uint32_t h[8], const uint8_t *p)
{
	uint32_t w[64], a, b, c, d, e, f, g, hh;
	unsigned i;

	for (i = 0; i < 16; i++)
		w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 | (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
	for (i = 16; i < 64; i++) {
		uint32_t s0 = spec_rotr(w[i - 15], 7) ^ spec_rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
		uint32_t s1 = spec_rotr(w[i - 2], 17) ^ spec_rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
		w[i] = w[i - 16] + s0 + w[i - 7] + s1;
	}
	a = h[0]; b = h[1]; c = h[2]; d = h[3]; e = h[4]; f = h[5]; g = h[6]; hh = h[7];
	for (i = 0; i < 64; i++) {
		uint32_t t1 = hh + (spec_rotr(e, 6) ^ spec_rotr(e, 11) ^ spec_rotr(e, 25)) + ((e & f) ^ (~e & g)) + spec_k[i] + w[i];
		uint32_t t2 = (spec_rotr(a, 2) ^ spec_rotr(a, 13) ^ spec_rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
		hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
	}
	h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

/* the SHA-256 of n bytes as 64 lowercase hex digits and a NUL */
static inline void spec_sha256(const uint8_t *p, size_t n, char hex[SPEC_HEX_LEN + 1])
{
	uint32_t h[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
	uint8_t tail[128];
	size_t full = n / 64, rest = n % 64, i, tl;
	uint64_t bits = (uint64_t)n * 8;

	for (i = 0; i < full; i++)
		spec_sha256_block(h, p + 64 * i);
	memset(tail, 0, sizeof tail);
	if (rest)
		memcpy(tail, p + 64 * full, rest);
	tail[rest] = 0x80;
	tl = rest < 56 ? 64 : 128;
	for (i = 0; i < 8; i++)
		tail[tl - 1 - i] = (uint8_t)(bits >> (8 * i));
	spec_sha256_block(h, tail);
	if (tl == 128)
		spec_sha256_block(h, tail + 64);
	for (i = 0; i < 8; i++)
		snprintf(hex + 8 * i, 9, "%08x", (unsigned)h[i]);
}

#endif /* PLUGIN_DB_FUZZ_SPEC_H */
