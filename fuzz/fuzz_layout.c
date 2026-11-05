// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>

/* libFuzzer harness: THE PARAMETER LAYOUT OF A PLUGIN (include/plugin-db/pin.h, omx-layout/1),
 * the bytes a host writes from the parameters a CLAP plugin reports or the control ports an
 * LV2 plugin describes, and the fingerprint of them. A plugin chooses every name, symbol,
 * number and property of it, so they are any bytes.
 *
 * Input: byte 0 chooses the format (bit 0: 0 CLAP, 1 LV2) and the count of records (bits 1-3,
 * up to 7); then the records, each a few fixed fields and its strings, until the input ends
 * (a cursor that is total: past the end it reads zeros). A CLAP record is an id, flags, three
 * numbers as their 64 bits (so NaN, infinities and -0 are inputs) and a name; an LV2 record is
 * an index, a direction, which numbers are declared, three numbers, a symbol and up to three
 * properties. A name or symbol of length 255 is a NULL pointer. Each string is copied into an
 * allocation of exactly its length, so a read past it is a sanitizer report.
 *
 * The harness writes the layout itself, from the comment at the head of pin.h, and the library
 * is held to it field by field: the bytes the library writes are read with a cursor, and each
 * check eats the text the harness expects at that place. The first field that differs aborts
 * in the check of its own property.
 *
 * Properties, each an abort() in a check_ function:
 *   - scheme line: the first line is "omx-layout/1 clap" or "omx-layout/1 lv2".
 *   - row index: the rows are in order of id (CLAP) or index (LV2), equal ones in the order
 *     given, and the id or index is written in decimal.
 *   - row name: a name or symbol is written with '\', a tab and a newline escaped, every other
 *     byte as given; none is empty.
 *   - row number: a number is 16 lowercase hex digits of its binary64 bits, "-" when not
 *     declared (a CLAP number always is).
 *   - row flags: CLAP flags are 8 lowercase hex digits.
 *   - row direction: an LV2 port is "in" or "out".
 *   - row props: an LV2 port's properties are sorted bytewise and joined by one space, "-" when
 *     none.
 *   - row end: every row ends with one newline.
 *   - end: nothing follows the last row.
 *   - digest: the fingerprint is the SHA-256 of those bytes (the harness's own, fuzz/spec.h).
 */
#include <inttypes.h>
#include <stdio.h>

#include <plugin-db/pin.h>

#include "fuzz.h"
#include "spec.h"

#define MAX_REC 7
#define MAX_PROPS 3

/* ---------------------------------------------------------------- the library's bytes */

struct out {
	char *p;
	size_t n, cap;
};

static void out_sink(void *ctx, const char *data, size_t size)
{
	struct out *o = ctx;

	if (o->n + size > o->cap) {
		o->cap = (o->n + size) * 2 + 64;
		o->p = realloc(o->p, o->cap);
		/* not a property: the harness's own allocation failed */
		if (!o->p)
			abort();
	}
	if (size)
		memcpy(o->p + o->n, data, size);
	o->n += size;
}

/* ---------------------------------------------------------------- the harness's own reading */

struct cur {
	const char *p;
	size_t n;
};

/* the next len bytes are lit: eaten, or not */
static int eat(struct cur *c, const char *lit, size_t len)
{
	if (c->n < len || memcmp(c->p, lit, len) != 0)
		return 0;
	c->p += len;
	c->n -= len;
	return 1;
}

static int eat_str(struct cur *c, const char *s)
{
	return eat(c, s, strlen(s));
}

/* contract: pin.h:30-32 */
static size_t spec_escape(const char *s, char *out)
{
	size_t n = 0;

	for (; *s; s++) {
		if (*s == '\\') { out[n++] = '\\'; out[n++] = '\\'; }
		else if (*s == '\t') { out[n++] = '\\'; out[n++] = 't'; }
		else if (*s == '\n') { out[n++] = '\\'; out[n++] = 'n'; }
		else out[n++] = *s;
	}
	return n;
}

static int spec_str_before(const char *a, const char *b)
{
	return strcmp(a, b) < 0;
}

/* ---------------------------------------------------------------- the checks */

FUZZ_CHECK check_scheme_line(struct cur *c, int lv2)
{
	/* The layout opens with the scheme and the format on one line.
	 * contract: pin.h:25-26 `line 1: "omx-layout/1 clap" or "omx-layout/1 lv2"`
	 * mutant: include/plugin-db/pin.h:49 `"omx-layout/1"` -> `"omx-layout/2"`
	 * input: clap-basic
	 * mutant: include/plugin-db/pin.h:314 `PHD_PIN_LAYOUT_CLAP "\n"` -> `PHD_PIN_LAYOUT_LV2 "\n"`
	 * input: clap-basic
	 * mutant: include/plugin-db/pin.h:348 `PHD_PIN_LAYOUT_LV2 "\n"` -> `PHD_PIN_LAYOUT_CLAP "\n"`
	 * input: lv2-basic */
	if (!eat_str(c, lv2 ? "omx-layout/1 lv2\n" : "omx-layout/1 clap\n"))
		abort();
}

FUZZ_CHECK check_row_index(struct cur *c, uint32_t value)
{
	char text[16];

	snprintf(text, sizeof text, "%u\t", (unsigned)value);
	/* The rows come in order of id or index, and it is written in decimal and a tab.
	 * contract: pin.h:27-29 `every parameter, sorted by id`, `every control port, sorted by index`
	 * mutant: include/plugin-db/pin.h:313 `qsort(order, count, sizeof(*order), phd_layout_clap_order);` -> ``
	 * input: clap-unsorted
	 * mutant: include/plugin-db/pin.h:286 `x->id < y->id ? -1 : 1` -> `x->id < y->id ? 1 : -1`
	 * input: clap-unsorted
	 * mutant: include/plugin-db/pin.h:347 `qsort(order, count, sizeof(*order), phd_layout_lv2_order);` -> ``
	 * input: lv2-unsorted
	 * mutant: include/plugin-db/pin.h:276 `value /= 10;` -> `value /= 8;`
	 * input: clap-basic
	 * mutant: include/plugin-db/pin.h:320 `"\t"` -> `" "`
	 * input: clap-basic */
	if (!eat_str(c, text))
		abort();
}

FUZZ_CHECK check_row_name(struct cur *c, const char *name)
{
	char *esc = (char *)fuzz_alloc(strlen(name) * 2 + 2);
	size_t n = spec_escape(name, esc);

	esc[n++] = '\t';
	/* A name is written with the backslash, the tab and the newline escaped, and a NULL one as
	 * empty.
	 * Rows with an equal id or index keep the order given, which their names show.
	 * contract: pin.h:27-29, 31-32 `in a name or symbol: '\' is "\\", a tab "\t", a newline "\n"`
	 * mutant: include/plugin-db/pin.h:286 `(x < y ? -1 : x > y)` -> `(x < y ? 1 : x > y)`
	 * input: clap-tie
	 * mutant: include/plugin-db/pin.h:294 `(x < y ? -1 : x > y)` -> `(x < y ? 1 : x > y)`
	 * input: lv2-tie
	 * mutant: include/plugin-db/pin.h:248 `*s == '\t' ? "\\t" : ` -> ``
	 * input: clap-escapes
	 * mutant: include/plugin-db/pin.h:248 `*s == '\n' ? "\\n" : ` -> ``
	 * input: clap-escapes
	 * mutant: include/plugin-db/pin.h:248 `*s == '\\' ? "\\\\" : ` -> ``
	 * input: clap-escapes
	 * mutant: include/plugin-db/pin.h:253 `sink(ctx, esc, 2);` -> `sink(ctx, esc, 1);`
	 * input: clap-escapes
	 * mutant: include/plugin-db/pin.h:254 `run = s + 1;` -> `run = s;`
	 * input: clap-escapes
	 * mutant: include/plugin-db/pin.h:321 `: ""` -> `: "-"`
	 * input: clap-null-name
	 * mutant: include/plugin-db/pin.h:367 `: ""` -> `: "-"`
	 * input: lv2-null-symbol
	 * mutant: include/plugin-db/pin.h:322 `"\t"` -> `" "`
	 * input: clap-basic */
	if (!eat(c, esc, n))
		abort();
	free(esc);
}

FUZZ_CHECK check_row_number(struct cur *c, uint64_t bits, int declared)
{
	char text[24];

	if (declared)
		snprintf(text, sizeof text, "%016" PRIx64 "\t", bits);
	else
		snprintf(text, sizeof text, "-\t");
	/* A number is the 16 lowercase hex digits of its binary64 bits, or "-" when not declared,
	 * and a tab.
	 * contract: pin.h:29-30 `a number: 16 lowercase hex digits of its IEEE-754 binary64 bits, "-" when none is declared`
	 * mutant: include/plugin-db/pin.h:231 `!declared` -> `0`
	 * input: lv2-undeclared
	 * mutant: include/plugin-db/pin.h:236 `memcpy(&bits, &value, sizeof(bits));` -> `memcpy(&bits, &value, sizeof(bits)); bits ^= 1;`
	 * input: clap-basic
	 * mutant: include/plugin-db/pin.h:238 `& 15` -> `& 7`
	 * input: clap-basic
	 * mutant: include/plugin-db/pin.h:371 `PHD_LAYOUT_HAS_MIN` -> `PHD_LAYOUT_HAS_MAX`
	 * input: lv2-has-min
	 * mutant: include/plugin-db/pin.h:373 `PHD_LAYOUT_HAS_MAX` -> `PHD_LAYOUT_HAS_MIN`
	 * input: lv2-has-max
	 * mutant: include/plugin-db/pin.h:375 `PHD_LAYOUT_HAS_DEF` -> `PHD_LAYOUT_HAS_MIN`
	 * input: lv2-has-def
	 * mutant: include/plugin-db/pin.h:325 `p->max` -> `p->min`
	 * input: clap-basic
	 * mutant: include/plugin-db/pin.h:324 `"\t"` -> `" "`
	 * input: clap-basic */
	if (!eat_str(c, text))
		abort();
}

FUZZ_CHECK check_row_flags(struct cur *c, uint32_t flags)
{
	char text[16];

	snprintf(text, sizeof text, "%08x", (unsigned)flags);
	/* The flags of a CLAP parameter are 8 lowercase hex digits.
	 * contract: pin.h:27 `flags 8 lowercase hex digits`
	 * mutant: include/plugin-db/pin.h:329 `p->flags, 1` -> `p->flags, 0`
	 * input: clap-basic
	 * mutant: include/plugin-db/pin.h:269 `& 15` -> `& 7`
	 * input: clap-basic */
	if (!eat_str(c, text))
		abort();
}

FUZZ_CHECK check_row_direction(struct cur *c, int output)
{
	/* A control port is "in" or "out", and a tab.
	 * contract: pin.h:28 `<index>\t<symbol>\t<in|out>`
	 * mutant: include/plugin-db/pin.h:369 `p->output ? "out" : "in"` -> `p->output ? "in" : "out"`
	 * input: lv2-basic */
	if (!eat_str(c, output ? "out\t" : "in\t"))
		abort();
}

FUZZ_CHECK check_row_props(struct cur *c, const char *const *props, size_t count)
{
	size_t i;
	int ok = count != 0 || eat_str(c, "-");

	for (i = 0; ok && i < count; i++)
		ok = (i == 0 || eat_str(c, " ")) && eat_str(c, props[i]);
	/* A port's properties are its URIs sorted bytewise, one space between, "-" when none.
	 * contract: pin.h:28-29 `properties the full URIs sorted bytewise and joined by one space, "-" when none`
	 * mutant: include/plugin-db/pin.h:363 `qsort(props, p->nproperties, sizeof(*props), phd_layout_bytewise);` -> ``
	 * input: lv2-props
	 * mutant: include/plugin-db/pin.h:381 `if (k)` -> `if (!k)`
	 * input: lv2-props
	 * mutant: include/plugin-db/pin.h:377 `!p->nproperties` -> `0`
	 * input: lv2-basic
	 * mutant: include/plugin-db/pin.h:383 `props[k]` -> `p->properties[k]`
	 * input: lv2-props */
	if (!ok)
		abort();
}

FUZZ_CHECK check_row_end(struct cur *c)
{
	/* A row ends with one newline.
	 * contract: pin.h:21 `one '\n' after every line`
	 * mutant: include/plugin-db/pin.h:330 `"\n"` -> `" "`
	 * input: clap-basic
	 * mutant: include/plugin-db/pin.h:385 `"\n"` -> `" "`
	 * input: lv2-basic */
	if (!eat_str(c, "\n"))
		abort();
}

FUZZ_CHECK check_end(const struct cur *c)
{
	/* Nothing follows the last row.
	 * contract: pin.h:21
	 * mutant: include/plugin-db/pin.h:332 `free(order);` -> `sink(ctx, "\n", 1); free(order);`
	 * input: clap-basic
	 * mutant: include/plugin-db/pin.h:388 `free(order);` -> `sink(ctx, "\n", 1); free(order);`
	 * input: lv2-basic */
	if (c->n != 0)
		abort();
}

FUZZ_CHECK check_digest(const char *got, int rc, const struct out *o)
{
	char want[SPEC_HEX_LEN + 1];

	spec_sha256((const uint8_t *)o->p, o->n, want);
	/* The fingerprint of a layout is the SHA-256 of the bytes written, 64 lowercase hex digits.
	 * contract: pin.h:19 `the fingerprint of a parameter layout in the omx-layout/1 serialisation`
	 * mutant: include/plugin-db/pin.h:394 `data, size` -> `data, size > 1 ? size - 1 : size`
	 * input: clap-basic */
	if (rc != 0 || strcmp(got, want) != 0)
		abort();
}

/* ---------------------------------------------------------------- the input as records */

static char *take_string(struct fuzz_in *in, unsigned mod, int *is_null)
{
	unsigned len = fuzz_u8(in);
	const uint8_t *p;
	size_t got;

	*is_null = len == 255;
	if (*is_null)
		return NULL;
	got = fuzz_take(in, len % mod, &p);
	return fuzz_strdup(p, got);
}

static void run_clap(struct fuzz_in *in, unsigned count)
{
	phd_layout_clap_param_t params[MAX_REC];
	char *names[MAX_REC];
	size_t order[MAX_REC], i, j;
	struct out o = { NULL, 0, 0 };
	struct cur cur;
	char hex[SPEC_HEX_LEN + 1];
	int rc, is_null;

	for (i = 0; i < count; i++) {
		params[i].id = fuzz_u32(in);
		params[i].flags = fuzz_u32(in);
		memcpy(&params[i].min, (uint64_t[]){ fuzz_u64(in) }, 8);
		memcpy(&params[i].max, (uint64_t[]){ fuzz_u64(in) }, 8);
		memcpy(&params[i].def, (uint64_t[]){ fuzz_u64(in) }, 8);
		names[i] = take_string(in, 24, &is_null);
		params[i].name = names[i];
	}
	/* the order the format asks for: by id, equal ids in the order given */
	for (i = 0; i < count; i++) {
		for (j = i; j > 0 && params[order[j - 1]].id > params[i].id; j--)
			order[j] = order[j - 1];
		order[j] = i;
	}

	rc = phd_layout_clap_write(params, count, out_sink, &o);
	/* not a property: the library's own allocation failed */
	if (rc != 0)
		abort();
	cur.p = o.p;
	cur.n = o.n;
	check_scheme_line(&cur, 0);
	for (i = 0; i < count; i++) {
		const phd_layout_clap_param_t *p = &params[order[i]];
		uint64_t bits[3];

		memcpy(&bits[0], &p->min, 8);
		memcpy(&bits[1], &p->max, 8);
		memcpy(&bits[2], &p->def, 8);
		check_row_index(&cur, p->id);
		check_row_name(&cur, p->name ? p->name : "");
		check_row_number(&cur, bits[0], 1);
		check_row_number(&cur, bits[1], 1);
		check_row_number(&cur, bits[2], 1);
		check_row_flags(&cur, p->flags);
		check_row_end(&cur);
	}
	check_end(&cur);

	memset(hex, 'x', SPEC_HEX_LEN);
	hex[SPEC_HEX_LEN] = '\0';
	rc = phd_layout_clap_digest(params, count, hex);
	check_digest(hex, rc, &o);

	for (i = 0; i < count; i++)
		free(names[i]);
	free(o.p);
}

static void run_lv2(struct fuzz_in *in, unsigned count)
{
	phd_layout_lv2_port_t ports[MAX_REC];
	char *symbols[MAX_REC];
	char *props[MAX_REC][MAX_PROPS];
	const char *propv[MAX_REC][MAX_PROPS];
	size_t order[MAX_REC], i, j, k;
	struct out o = { NULL, 0, 0 };
	struct cur cur;
	char hex[SPEC_HEX_LEN + 1];
	int rc, is_null;

	for (i = 0; i < count; i++) {
		uint8_t flag;

		ports[i].index = fuzz_u32(in);
		flag = fuzz_u8(in);
		ports[i].output = flag & 1;
		ports[i].has = (flag >> 1) & 7;
		memcpy(&ports[i].min, (uint64_t[]){ fuzz_u64(in) }, 8);
		memcpy(&ports[i].max, (uint64_t[]){ fuzz_u64(in) }, 8);
		memcpy(&ports[i].def, (uint64_t[]){ fuzz_u64(in) }, 8);
		symbols[i] = take_string(in, 24, &is_null);
		ports[i].symbol = symbols[i];
		ports[i].nproperties = fuzz_u8(in) % (MAX_PROPS + 1);
		for (k = 0; k < ports[i].nproperties; k++) {
			props[i][k] = take_string(in, 16, &is_null);
			propv[i][k] = props[i][k] ? props[i][k] : "";
			if (!props[i][k])
				propv[i][k] = props[i][k] = fuzz_strdup((const uint8_t *)"", 0);
		}
		ports[i].properties = propv[i];
	}
	for (i = 0; i < count; i++) {
		for (j = i; j > 0 && ports[order[j - 1]].index > ports[i].index; j--)
			order[j] = order[j - 1];
		order[j] = i;
	}

	rc = phd_layout_lv2_write(ports, count, out_sink, &o);
	/* not a property: the library's own allocation failed */
	if (rc != 0)
		abort();
	cur.p = o.p;
	cur.n = o.n;
	check_scheme_line(&cur, 1);
	for (i = 0; i < count; i++) {
		const phd_layout_lv2_port_t *p = &ports[order[i]];
		const char *sorted[MAX_PROPS];
		uint64_t bits[3];

		memcpy(&bits[0], &p->min, 8);
		memcpy(&bits[1], &p->max, 8);
		memcpy(&bits[2], &p->def, 8);
		for (j = 0; j < p->nproperties; j++) {
			size_t at;

			for (at = j; at > 0 && spec_str_before(p->properties[j], sorted[at - 1]); at--)
				sorted[at] = sorted[at - 1];
			sorted[at] = p->properties[j];
		}
		check_row_index(&cur, p->index);
		check_row_name(&cur, p->symbol ? p->symbol : "");
		check_row_direction(&cur, p->output);
		check_row_number(&cur, bits[0], (p->has & 1) != 0);
		check_row_number(&cur, bits[1], (p->has & 2) != 0);
		check_row_number(&cur, bits[2], (p->has & 4) != 0);
		check_row_props(&cur, sorted, p->nproperties);
		check_row_end(&cur);
	}
	check_end(&cur);

	memset(hex, 'x', SPEC_HEX_LEN);
	hex[SPEC_HEX_LEN] = '\0';
	rc = phd_layout_lv2_digest(ports, count, hex);
	check_digest(hex, rc, &o);

	for (i = 0; i < count; i++) {
		free(symbols[i]);
		for (k = 0; k < ports[i].nproperties; k++)
			free(props[i][k]);
	}
	free(o.p);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	struct fuzz_in in = fuzz_in(data, size);
	unsigned sel;

	if (size >= FUZZ_INPUT_MAX)
		return -1;
	sel = fuzz_u8(&in);
	if (sel & 1)
		run_lv2(&in, (sel >> 1) & 7);
	else
		run_clap(&in, (sel >> 1) & 7);
	return 0;
}
