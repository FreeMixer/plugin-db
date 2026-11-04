// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>

/* Writes the seed corpus of fuzz/corpus/<harness>/, one file per input, named for what the
 * input shows. Each file is the input as libFuzzer takes it. Nothing is read from a network or
 * a capture: the inputs are built here, and `make fuzz-corpus-check` is red when fuzz/corpus/
 * is not what this writes. Usage: gen-corpus <directory> */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static uint8_t buf[4096];
static size_t len;
static const char *dir;

static void u8(unsigned v) { buf[len++] = (uint8_t)v; }
static void u32(uint32_t v) { for (int i = 0; i < 4; i++) u8(v >> (8 * i)); }
static void u64(uint64_t v) { for (int i = 0; i < 8; i++) u8(v >> (8 * i)); }
static void raw(const void *p, size_t n) { memcpy(buf + len, p, n); len += n; }

/* a string of the harnesses: a length byte, then the bytes; 255 is a NULL pointer */
static void str(const char *s, size_t n) { u8((unsigned)n); raw(s, n); }
static void cstr(const char *s) { str(s, strlen(s)); }
static void nul(void) { u8(255); }

static void emit(const char *harness, const char *name)
{
	char path[256];
	FILE *f;

	snprintf(path, sizeof path, "%s/%s", dir, harness);
	mkdir(path, 0755);
	snprintf(path, sizeof path, "%s/%s/%s", dir, harness, name);
	f = fopen(path, "wb");
	if (!f || fwrite(buf, 1, len, f) != len || fclose(f)) {
		perror(path);
		exit(1);
	}
	len = 0;
}

/* a string written as the whole input of fuzz_pin */
static void pin(const char *name, const char *s)
{
	raw(s, strlen(s));
	emit("pin", name);
}

static const uint64_t D_HALF = 0x3fe0000000000000ull, D_ONE = 0x3ff0000000000000ull, D_PI = 0x400921fb54442d18ull,
		      D_NEG = 0xc059000000000000ull, D_ZERO = 0, D_NZERO = 0x8000000000000000ull,
		      D_NAN = 0x7ff8000000000000ull, D_INF = 0x7ff0000000000000ull, D_TEN = 0x4024000000000000ull;

/* a CLAP record: id, flags, min, max, def, name (NULL for none) */
static void clap(uint32_t id, uint32_t flags, uint64_t mn, uint64_t mx, uint64_t df, const char *name, size_t n)
{
	u32(id); u32(flags); u64(mn); u64(mx); u64(df);
	if (name) str(name, n); else nul();
}

/* an LV2 record: index, output, which numbers, min, max, def, symbol, properties */
static void lv2(uint32_t index, int output, unsigned has, uint64_t mn, uint64_t mx, uint64_t df, const char *sym,
		unsigned nprops, const char *const *props)
{
	u32(index); u8((output ? 1 : 0) | (has << 1)); u64(mn); u64(mx); u64(df);
	if (sym) cstr(sym); else nul();
	u8(nprops);
	for (unsigned i = 0; i < nprops; i++)
		cstr(props[i]);
}

static void sel(int is_lv2, unsigned count) { u8((count << 1) | (is_lv2 ? 1 : 0)); }

int main(int argc, char **argv)
{
	const char H[] = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
	char t[256];
	const char *none[1] = { 0 };

	if (argc != 2) {
		fprintf(stderr, "usage: gen-corpus <directory>\n");
		return 2;
	}
	dir = argv[1];
	mkdir(dir, 0755);

	/* ---- fuzz_pin */
	pin("empty", "");
	pin("digest-ok", H);
	memset(t, 'A', 64); t[64] = 0;
	pin("digest-upper", t);
	memset(t, '0', 64); t[0] = 'g'; t[64] = 0;
	pin("digest-g", t);
	memset(t, '0', 64); t[63] = ':'; t[64] = 0;
	pin("digest-colon", t);
	pin("digest-63", "012345678901234567890123456789012345678901234567890123456789abc");
	snprintf(t, sizeof t, "%s0", H);
	pin("digest-65", t);
	snprintf(t, sizeof t, "omx-layout/1:%s", H);
	pin("token-ok", t);
	snprintf(t, sizeof t, ":%s", H);
	pin("token-empty-scheme", t);
	snprintf(t, sizeof t, "a:b:%s", H);
	pin("token-two-colons", t);
	snprintf(t, sizeof t, "%.31s:%s", "sssssssssssssssssssssssssssssssss", H);
	pin("token-scheme-31", t);
	snprintf(t, sizeof t, "%.32s:%s", "sssssssssssssssssssssssssssssssss", H);
	pin("token-scheme-32", t);
	pin("token-no-colon", H);
	pin("token-bad-digest", "omx-layout/1:0123456789");
	pin("verdict-in-process", "in-process");
	pin("verdict-isolated", "isolated");
	pin("verdict-refused", "refused");
	pin("verdict-prefix", "in");
	pin("verdict-bogus", "trusted");
	pin("pool-cores", "cores");
	pin("pool-off", "off");
	pin("pool-bogus", "serial");

	/* ---- fuzz_digest: a mode byte (chunk size - 1, bit 7 gives a digest up), then the message */
	u8(0); emit("digest", "empty");
	u8(2); raw("abc", 3); emit("digest", "abc");
	u8(0x80 | 2); raw("abc", 3); emit("digest", "abandoned");
	u8(0); raw("chunk by chunk, a byte at a time", 32); emit("digest", "chunked-1");
	u8(63); for (int i = 0; i < 300; i++) u8(i * 7 + 1); emit("digest", "chunked-300");
	u8(5); for (int i = 0; i < 55; i++) u8('a'); emit("digest", "block-55");
	u8(5); for (int i = 0; i < 56; i++) u8('a'); emit("digest", "block-56");
	u8(127); for (int i = 0; i < 64; i++) u8('a'); emit("digest", "block-64");

	/* ---- fuzz_layout */
	sel(0, 0); emit("layout", "clap-empty");
	sel(0, 3);
	clap(4000, 0xdeadbeef, D_HALF, D_ONE, D_PI, "Volume", 6);
	clap(12, 0x00000001, D_NEG, D_TEN, D_HALF, "Pan", 3);
	clap(700, 0x0a1b2c3d, D_ZERO, D_ONE, D_PI, "Cutoff", 6);
	emit("layout", "clap-basic");
	sel(0, 2); clap(9, 0, D_ZERO, D_ONE, D_HALF, "hi", 2); clap(3, 0, D_ZERO, D_ONE, D_HALF, "lo", 2);
	emit("layout", "clap-unsorted");
	sel(0, 2); clap(5, 1, D_ZERO, D_ONE, D_HALF, "a", 1); clap(5, 2, D_ZERO, D_ONE, D_HALF, "b", 1);
	emit("layout", "clap-tie");
	sel(0, 1); clap(1, 0, D_ZERO, D_ONE, D_HALF, "back\\slash\ttab\nnewline", 22);
	emit("layout", "clap-escapes");
	sel(0, 1); clap(1, 0, D_ZERO, D_ONE, D_HALF, NULL, 0); emit("layout", "clap-null-name");
	sel(0, 2); clap(1, 0, D_NAN, D_INF, D_NZERO, "specials", 8); clap(2, 0xffffffff, D_INF, D_NAN, D_NZERO, "", 0);
	emit("layout", "clap-specials");
	sel(1, 0); emit("layout", "lv2-empty");
	{
		const char *p1[] = { "http://lv2plug.in/ns/lv2core#toggled" };
		sel(1, 3);
		lv2(3, 1, 7, D_ZERO, D_ONE, D_HALF, "gain_out", 0, none);
		lv2(1, 0, 7, D_NEG, D_TEN, D_PI, "gain", 1, p1);
		lv2(2, 0, 7, D_ZERO, D_ONE, D_HALF, "mix", 0, none);
		emit("layout", "lv2-basic");
	}
	sel(1, 2); lv2(5, 0, 7, D_ZERO, D_ONE, D_HALF, "hi", 0, none); lv2(2, 0, 7, D_ZERO, D_ONE, D_HALF, "lo", 0, none);
	emit("layout", "lv2-unsorted");
	sel(1, 2); lv2(4, 0, 7, D_ZERO, D_ONE, D_HALF, "first", 0, none); lv2(4, 1, 7, D_ZERO, D_ONE, D_HALF, "second", 0, none);
	emit("layout", "lv2-tie");
	sel(1, 1); lv2(0, 0, 0, D_PI, D_PI, D_PI, "none_declared", 0, none); emit("layout", "lv2-undeclared");
	sel(1, 1); lv2(0, 0, 1, D_PI, D_NEG, D_NEG, "only_min", 0, none); emit("layout", "lv2-has-min");
	sel(1, 1); lv2(0, 0, 2, D_NEG, D_PI, D_NEG, "only_max", 0, none); emit("layout", "lv2-has-max");
	sel(1, 1); lv2(0, 0, 4, D_NEG, D_NEG, D_PI, "only_def", 0, none); emit("layout", "lv2-has-def");
	{
		const char *p3[] = { "http://lv2plug.in/ns/ext/port-props#logarithmic", "http://lv2plug.in/ns/lv2core#toggled",
				     "\xc3\xa9" };
		sel(1, 1); lv2(0, 0, 7, D_ZERO, D_ONE, D_HALF, "tab\there", 3, p3); emit("layout", "lv2-props");
	}
	sel(1, 1); lv2(0, 0, 7, D_ZERO, D_ONE, D_HALF, NULL, 0, none); emit("layout", "lv2-null-symbol");
	return 0;
}
