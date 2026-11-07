/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com> */

/* The store of data/store.schema.json: one directory per entry, named by the SHA-256 of its address, one file per
 * layer in the declared envelope, each written to a temporary name and renamed over its own, each recording the pin it
 * was written for. The entry is joined at read time, here, and nowhere stored. */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <plugin-db/db.h>

#include "collect.h"
#include "json.h"
#include "names.h"

static const char *const g_layers[] = OMX_PLUGIN_DB_LAYERS_INIT;
static const char *const g_verdicts[] = OMX_PLUGIN_DB_PIN_VERDICTS_INIT;
static const char *const g_rules[] = OMX_PLUGIN_DB_BUNDLE_RULES_INIT;
static const char *const g_formats[] = OMX_PLUGIN_DB_ADDRESS_FORMATS_INIT;
static const char *const g_envelope_required[] = PDB_ENVELOPE_REQUIRED_INIT;
static const char *const g_pin_required[] = PDB_PIN_REQUIRED_INIT;
static const char *const g_pinned_required[] = PDB_PINNED_BODY_REQUIRED_INIT;
static const char *const g_record_required[] = PDB_RECORD_REQUIRED_INIT;
static const char *const g_record_host_required[] = PDB_RECORD_HOST_REQUIRED_INIT;

static int say(char *why, size_t size, const char *format, ...)
{
    va_list ap;

    if (why && size)
    {
        va_start(ap, format);
        vsnprintf(why, size, format, ap);
        va_end(ap);
    }
    return -1;
}

/* ---------------------------------------------------------------- a growing string */

typedef struct BUF_T {
    char *s;
    size_t n, cap;
    int failed;
} buf_t;

static void put(buf_t *b, const char *s, size_t n)
{
    if (b->failed)
        return;
    if (b->n + n + 1 > b->cap)
    {
        size_t cap = (b->n + n + 1) * 2;
        char *grown = realloc(b->s, cap);

        if (!grown)
        {
            b->failed = 1;
            return;
        }
        b->s = grown;
        b->cap = cap;
    }
    memcpy(b->s + b->n, s, n);
    b->n += n;
    b->s[b->n] = '\0';
}

static void puts_(buf_t *b, const char *s)
{
    put(b, s, strlen(s));
}

static void quoted(buf_t *b, const char *s)
{
    size_t need = pdb_json_quote(s, NULL, 0) + 1;
    char *q = malloc(need);

    if (!q)
    {
        b->failed = 1;
        return;
    }
    pdb_json_quote(s, q, need);
    puts_(b, q);
    free(q);
}

/* "key": */
static void key(buf_t *b, const char *k, int first)
{
    if (!first)
        puts_(b, ", ");
    quoted(b, k);
    puts_(b, ": ");
}

/* ---------------------------------------------------------------- where things are */

int pdb_store_default(int system, char *out, size_t size)
{
    const char *env = getenv(PDB_STORE_ENV), *state = getenv(PDB_STORE_USER_STATE_HOME), *home = getenv("HOME");

    if (env && *env)
        return snprintf(out, size, "%s", env) < (int)size ? 0 : -1;
    if (system)
        return snprintf(out, size, "%s", PDB_STORE_SYSTEM_ROOT) < (int)size ? 0 : -1;
    if (state && *state == '/')
        return snprintf(out, size, "%s/%s", state, PDB_STORE_USER_DIR) < (int)size ? 0 : -1;
    if (!home || !*home)
        return -1;
    return snprintf(out, size, "%s/%s/%s", home, PDB_STORE_USER_STATE_FALLBACK, PDB_STORE_USER_DIR) < (int)size ? 0
                                                                                                               : -1;
}

static int entry_dir(const char *store, const char *address, char *out, size_t size)
{
    char hex[PHD_SHA256_HEX_LEN + 1];

    if (phd_sha256_hex(address, strlen(address), hex) != 0)
        return -1;
    return snprintf(out, size, "%s/%s/%s", store, PDB_STORE_ENTRIES, hex) < (int)size ? 0 : -1;
}

static int layer_path(const char *entry, int layer, char *out, size_t size)
{
    return snprintf(out, size, "%s/%s%s", entry, g_layers[layer], PDB_STORE_LAYER_SUFFIX) < (int)size ? 0 : -1;
}

/* every directory of path, as mkdir -p */
static int make_dirs(const char *path)
{
    char copy[PDB_PATH_MAX];
    char *c;

    if (snprintf(copy, sizeof(copy), "%s", path) >= (int)sizeof(copy))
        return -1;
    for (c = copy + 1; *c; c++)
        if (*c == '/')
        {
            *c = '\0';
            if (mkdir(copy, 0755) != 0 && errno != EEXIST)
                return -1;
            *c = '/';
        }
    return mkdir(copy, 0755) == 0 || errno == EEXIST ? 0 : -1;
}

/* the bytes to a temporary name in the file's directory, flushed, then renamed over the file: a reader sees the old
 * file or the new one, never a part */
static int write_atomic(const char *dir, const char *path, const char *bytes, size_t size)
{
    char temp[PDB_PATH_MAX];
    size_t done = 0;
    int fd, dfd;

    if (snprintf(temp, sizeof(temp), "%s/%sXXXXXX", dir, PDB_STORE_TEMP_PREFIX) >= (int)sizeof(temp))
        return -1;
    fd = mkstemp(temp);
    if (fd < 0)
        return -1;
    while (done < size)
    {
        ssize_t n = write(fd, bytes + done, size - done);

        if (n <= 0)
            break;
        done += (size_t)n;
    }
    if (done != size || fchmod(fd, 0644) != 0 || fsync(fd) != 0)
    {
        close(fd);
        unlink(temp);
        return -1;
    }
    close(fd);
    if (rename(temp, path) != 0)
    {
        unlink(temp);
        return -1;
    }
    dfd = open(dir, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dfd >= 0)
    {
        fsync(dfd);
        close(dfd);
    }
    return 0;
}

static char *read_all(const char *path, size_t *size, int *err)
{
    struct stat st;
    char *bytes;
    size_t got = 0;
    int fd = open(path, O_RDONLY | O_CLOEXEC);

    *err = 0;
    if (fd < 0)
    {
        *err = errno;
        return NULL;
    }
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size > 64 * 1024 * 1024 ||
        !(bytes = malloc((size_t)st.st_size + 1)))
    {
        *err = EINVAL;
        close(fd);
        return NULL;
    }
    while (got < (size_t)st.st_size)
    {
        ssize_t n = read(fd, bytes + got, (size_t)st.st_size - got);

        if (n <= 0)
            break;
        got += (size_t)n;
    }
    close(fd);
    if (got != (size_t)st.st_size)
    {
        *err = EIO;
        free(bytes);
        return NULL;
    }
    bytes[got] = '\0';
    *size = got;
    return bytes;
}

/* ---------------------------------------------------------------- the pin a layer is written for */

typedef struct PIN_FILE_T {
    const char *path;
    char sha256[PHD_SHA256_HEX_LEN + 1];
} pin_file_t;

static int by_path(const void *a, const void *b)
{
    return strcmp(((const pin_file_t *)a)->path, ((const pin_file_t *)b)->path);
}

/* the files pin of PDB_STORE_FILES_PIN */
static int files_pin(pin_file_t *files, int n, char hex[PHD_SHA256_HEX_LEN + 1])
{
    phd_sha256_t c;
    int k;

    qsort(files, (size_t)n, sizeof(*files), by_path);
    phd_sha256_init(&c);
    phd_sha256_update(&c, PDB_STORE_FILES_PIN_SCHEME "\n", strlen(PDB_STORE_FILES_PIN_SCHEME) + 1);
    for (k = 0; k < n; k++)
    {
        phd_sha256_update(&c, files[k].path, strlen(files[k].path));
        phd_sha256_update(&c, "\t", 1);
        phd_sha256_update(&c, files[k].sha256, PHD_SHA256_HEX_LEN);
        phd_sha256_update(&c, "\n", 1);
    }
    return phd_sha256_final(&c, hex);
}

static int layout_valid(const char *layout)
{
    char scheme[64], hex[PHD_SHA256_HEX_LEN + 1];

    return phd_pin_layout_parse(layout, scheme, sizeof(scheme), hex) == 0 && !strcmp(scheme, PHD_PIN_LAYOUT_SCHEME);
}

static int required(const pdb_json_t *object, const char *const *keys, int count, const char *what, char *why,
                    size_t size)
{
    int n;

    if (!object || object->type != PDB_JSON_OBJECT)
        return say(why, size, "%s is not a JSON object", what);
    for (n = 0; n < count; n++)
        if (!pdb_json_get(object, keys[n]))
            return say(why, size, "%s has no %s", what, keys[n]);
    return 0;
}

/* a body as the schema requires it of its layer */
static int body_valid(int layer, const pdb_json_t *body, char *why, size_t size)
{
    if (required(body, NULL, 0, "the body", why, size) != 0)
        return -1;
    if (layer == PDB_LAYERS_PINNED)
        return required(body, g_pinned_required, PDB_PINNED_BODY_REQUIRED_COUNT, "the pinned body", why, size);
    if (layer == PDB_LAYERS_JUDGED || layer == PDB_LAYERS_REFERENCE)
    {
        const char *verdict = pdb_json_string(body, PDB_KEY_VERDICT);

        if (required(body, g_record_required, PDB_RECORD_REQUIRED_COUNT, "the record", why, size) != 0 ||
            required(pdb_json_get(body, PDB_KEY_HOST), g_record_host_required, PDB_RECORD_HOST_REQUIRED_COUNT,
                     "the record's host", why, size) != 0)
            return -1;
        if (!verdict || !phd_pin_verdict_valid(verdict))
            return say(why, size, "the record's verdict is no word of PLUGIN_DB_PIN_VERDICTS");
    }
    return 0;
}

/* a layer file as the schema declares it: its envelope, its layer and address, its pin, and its body */
typedef struct LAYER_T {
    pdb_json_t *tree;
    const char *files;          /* the files pin it was written for */
    const char *layout;         /* the layout, or NULL */
    const pdb_json_t *body;
} layer_t;

/* 0 with the layer read; 1 when there is no such file; -1 when it cannot be read or is not the schema's, with why */
static int layer_read(const char *entry, int layer, const char *address, layer_t *out, char *why, size_t size)
{
    char path[PDB_PATH_MAX];
    const pdb_json_t *envelope, *pin, *layout;
    const char *name, *addr;
    size_t len;
    int err;
    char *text;

    memset(out, 0, sizeof(*out));
    if (layer_path(entry, layer, path, sizeof(path)) != 0)
        return say(why, size, "the entry's path is too long");
    text = read_all(path, &len, &err);
    if (!text)
        return err == ENOENT ? 1 : say(why, size, "%s can't be read: %s", path, strerror(err));
    out->tree = pdb_json_parse(text, len);
    free(text);
    if (required(out->tree, g_envelope_required, PDB_ENVELOPE_REQUIRED_COUNT, path, why, size) != 0)
        goto bad;
    envelope = pdb_json_get(out->tree, PDB_KEY_ENVELOPE);
    if (envelope->type != PDB_JSON_NUMBER || envelope->number != PDB_ENVELOPE_VERSION)
    {
        say(why, size, "%s is in an envelope this version does not read", path);
        goto bad;
    }
    name = pdb_json_string(out->tree, PDB_KEY_LAYER);
    addr = pdb_json_string(out->tree, PDB_KEY_ADDRESS);
    if (!name || strcmp(name, g_layers[layer]) || !addr || (address && strcmp(addr, address)))
    {
        say(why, size, "%s is not the %s layer of %s", path, g_layers[layer], address ? address : "its entry");
        goto bad;
    }
    pin = pdb_json_get(out->tree, PDB_KEY_PIN);
    if (required(pin, g_pin_required, PDB_PIN_REQUIRED_COUNT, "its pin", why, size) != 0)
        goto bad;
    out->files = pdb_json_string(pin, PDB_KEY_FILES);
    layout = pdb_json_get(pin, PDB_KEY_LAYOUT);
    out->layout = layout->type == PDB_JSON_STRING ? layout->string : NULL;
    if (!out->files || !phd_pin_hex_valid(out->files) || (layout->type != PDB_JSON_NULL && !out->layout) ||
        (out->layout && !layout_valid(out->layout)))
    {
        say(why, size, "%s records no pin of the schema's", path);
        goto bad;
    }
    out->body = pdb_json_get(out->tree, PDB_KEY_BODY);
    if (body_valid(layer, out->body, why, size) != 0)
        goto bad;
    return 0;
bad:
    pdb_json_free(out->tree);
    memset(out, 0, sizeof(*out));
    return -1;
}

static int layer_write(const char *entry, int layer, const char *address, const char *files, const char *layout,
                       const char *writer, const char *body)
{
    char path[PDB_PATH_MAX], version[16];
    buf_t b = { NULL, 0, 0, 0 };
    int rc;

    if (layer_path(entry, layer, path, sizeof(path)) != 0 || make_dirs(entry) != 0)
        return -1;
    snprintf(version, sizeof(version), "%d", PDB_ENVELOPE_VERSION);
    puts_(&b, "{");
    key(&b, PDB_KEY_ENVELOPE, 1);
    puts_(&b, version);
    key(&b, PDB_KEY_LAYER, 0);
    quoted(&b, g_layers[layer]);
    key(&b, PDB_KEY_ADDRESS, 0);
    quoted(&b, address);
    key(&b, PDB_KEY_PIN, 0);
    puts_(&b, "{");
    key(&b, PDB_KEY_FILES, 1);
    quoted(&b, files);
    key(&b, PDB_KEY_LAYOUT, 0);
    if (layout)
        quoted(&b, layout);
    else
        puts_(&b, "null");
    puts_(&b, "}");
    if (writer)
    {
        key(&b, PDB_KEY_WRITER, 0);
        quoted(&b, writer);
    }
    key(&b, PDB_KEY_BODY, 0);
    puts_(&b, body);
    puts_(&b, "}\n");
    rc = b.failed ? -1 : write_atomic(entry, path, b.s, b.n);
    free(b.s);
    return rc;
}

/* ---------------------------------------------------------------- the entries */

static int is_entry_name(const char *name)
{
    return strlen(name) == PHD_SHA256_HEX_LEN && phd_pin_hex_valid(name);
}

static int bytewise(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* the address the entry's file of one layer records; 0, or -1 when there is none */
static int address_of(const char *entry, int layer, char *out, size_t size)
{
    char path[PDB_PATH_MAX];
    pdb_json_t *tree;
    const char *addr;
    size_t len;
    int err, rc = -1;
    char *text;

    if (layer_path(entry, layer, path, sizeof(path)) != 0 || !(text = read_all(path, &len, &err)))
        return -1;
    tree = pdb_json_parse(text, len);
    free(text);
    addr = pdb_json_string(tree, PDB_KEY_ADDRESS);
    if (addr && snprintf(out, size, "%s", addr) < (int)size)
        rc = 0;
    pdb_json_free(tree);
    return rc;
}

/* the address an entry's files record: its pinned layer's, else the first layer's that has one */
static int entry_address(const char *entry, char *out, size_t size)
{
    int layer;

    if (address_of(entry, PDB_LAYERS_PINNED, out, size) == 0)
        return 0;
    for (layer = 0; layer < PDB_LAYERS_COUNT; layer++)
        if (layer != PDB_LAYERS_PINNED && address_of(entry, layer, out, size) == 0)
            return 0;
    return -1;
}

int pdb_entries(const char *store, int (*fn)(void *ctx, const char *address), void *ctx)
{
    char dir[PDB_PATH_MAX];
    pdb_names_t names = { NULL, 0 };
    struct dirent *e;
    DIR *d;
    int n, count = 0, stop = 0;

    if (snprintf(dir, sizeof(dir), "%s/%s", store, PDB_STORE_ENTRIES) >= (int)sizeof(dir))
        return -1;
    if (!(d = opendir(dir)))
        return errno == ENOENT ? 0 : -1;
    while ((e = readdir(d)))
        if (is_entry_name(e->d_name) && pdb_names_add(&names, e->d_name) != 0)
        {
            closedir(d);
            pdb_names_free(&names);
            return -1;
        }
    closedir(d);
    if (names.n)
        qsort(names.name, (size_t)names.n, sizeof(char *), bytewise);
    for (n = 0; n < names.n && !stop; n++)
    {
        char entry[PDB_PATH_MAX], address[PDB_PATH_MAX];

        if (snprintf(entry, sizeof(entry), "%s/%s", dir, names.name[n]) >= (int)sizeof(entry) ||
            entry_address(entry, address, sizeof(address)) != 0)
            continue;
        count++;
        stop = fn(ctx, address);
    }
    pdb_names_free(&names);
    return count;
}

typedef struct FIND_T {
    const char *id;
    int found;
    char address[PDB_PATH_MAX];
} find_t;

static int find_clap(void *ctx, const char *address)
{
    find_t *f = ctx;
    pdb_address_t a;

    if (pdb_address_parse(address, &a) == 0 && a.format == PDB_ADDRESS_FORMATS_CLAP && a.pin[0] &&
        !strcmp(a.id, f->id))
    {
        if (!f->found++)
            snprintf(f->address, sizeof(f->address), "%s", address);
    }
    return 0;
}

/* the entry of an address: its own, or for clap:<id> with no entry of its own the one entry clap@<sha256>:<id> */
static int resolve(const char *store, const char *text, char *address, size_t size, char *entry, size_t entry_size,
                   char *why, size_t why_size)
{
    pdb_address_t a;
    struct stat st;
    find_t f;

    if (pdb_address_parse(text, &a) != 0)
        return say(why, why_size, "%s is no address: <format>:<id> or <format>@<sha256>:<id>, a format of %s", text,
                   "PLUGIN_DB_ADDRESS_FORMATS, and never a path");
    if (entry_dir(store, text, entry, entry_size) != 0)
        return say(why, why_size, "the store's path is too long");
    if (stat(entry, &st) == 0 && S_ISDIR(st.st_mode))
        return snprintf(address, size, "%s", text) < (int)size ? 0 : -1;
    if (a.format != PDB_ADDRESS_FORMATS_CLAP || a.pin[0])
        return say(why, why_size, "%s has no entry in %s", text, store);
    memset(&f, 0, sizeof(f));
    f.id = a.id;
    if (pdb_entries(store, find_clap, &f) < 0)
        return say(why, why_size, "the store %s can't be read", store);
    if (f.found > 1)
        return say(why, why_size, "%d files share the CLAP id %s: address one as clap@<sha256>:%s", f.found, a.id,
                   a.id);
    if (!f.found)
        return say(why, why_size, "%s has no entry in %s", text, store);
    if (entry_dir(store, f.address, entry, entry_size) != 0)
        return -1;
    return snprintf(address, size, "%s", f.address) < (int)size ? 0 : -1;
}

/* ---------------------------------------------------------------- writing */

static int digest_of(const char *root, const char *path, char hex[PHD_SHA256_HEX_LEN + 1], int *err)
{
    char full[PDB_PATH_MAX * 2];
    struct stat st;
    int fd, rc = -1;

    *err = EINVAL;
    if (pdb_full_path(root, path, full, sizeof(full)) != 0)
        return -1;
    fd = open(full, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
    {
        *err = errno;
        return -1;
    }
    if (fstat(fd, &st) == 0 && S_ISREG(st.st_mode))
        rc = phd_pin_fd_digest(fd, hex);
    *err = rc == 0 ? 0 : S_ISREG(st.st_mode) ? EIO : EISDIR;
    close(fd);
    return rc;
}

int pdb_pin(const char *store, const char *address, const char *where, const char *layout, char *why, size_t why_size)
{
    char entry[PDB_PATH_MAX], pin[PHD_SHA256_HEX_LEN + 1];
    pdb_collection_t c;
    pdb_address_t a;
    pin_file_t *files = NULL;
    buf_t b = { NULL, 0, 0, 0 };
    int n, rc = -1, err;

    if (pdb_address_parse(address, &a) != 0)
        return say(why, why_size, "%s is no address", address);
    if (layout && !layout_valid(layout))
        return say(why, why_size, "%s is not a layout pin %s:<sha256>", layout, PHD_PIN_LAYOUT_SCHEME);
    if (entry_dir(store, address, entry, sizeof(entry)) != 0)
        return say(why, why_size, "the store's path is too long");
    if (a.format == PDB_ADDRESS_FORMATS_CLAP)
    {
        layer_t old;

        /* one id, two files: the address must tell them apart by the file's pin */
        if (!a.pin[0] && layer_read(entry, PDB_LAYERS_PINNED, address, &old, NULL, 0) == 0)
        {
            const char *binary = pdb_json_string(old.body, PDB_KEY_BINARY);
            int other = binary && where && strcmp(binary, where);

            pdb_json_free(old.tree);
            if (other)
                return say(why, why_size, "%s and %s share the CLAP id %s: address each as clap@<sha256>:%s", binary,
                           where, a.id, a.id);
        }
    }
    switch (pdb_collect(&a, where, &c, why, why_size))
    {
    case PDB_COLLECT_OK:
        break;
    case PDB_COLLECT_ERROR:
        pdb_collection_free(&c);
        return say(why, why_size, "out of memory");
    default:
        pdb_collection_free(&c);
        return -1;
    }
    if (!(files = calloc((size_t)c.files.n, sizeof(*files))))
        goto out;
    for (n = 0; n < c.files.n; n++)
    {
        if (!pdb_path_valid(c.files.name[n]) || digest_of(c.root, c.files.name[n], files[n].sha256, &err) != 0)
        {
            say(why, why_size, "%s/%s is missing or not a file", c.root, c.files.name[n]);
            goto out;
        }
        files[n].path = c.files.name[n];
    }
    if (a.format == PDB_ADDRESS_FORMATS_CLAP && a.pin[0] && strcmp(a.pin, files[0].sha256))
    {
        say(why, why_size, "%s is not the file of %s: its sha256 is %s", where, address, files[0].sha256);
        goto out;
    }
    if (files_pin(files, c.files.n, pin) != 0)
        goto out;
    puts_(&b, "{");
    key(&b, PDB_KEY_FORMAT, 1);
    quoted(&b, g_formats[a.format]);
    key(&b, PDB_KEY_ROOT, 0);
    quoted(&b, c.root);
    if (a.format == PDB_ADDRESS_FORMATS_LV2)
    {
        key(&b, PDB_KEY_SEARCH, 0);
        quoted(&b, pdb_lv2_search(where));
    }
    else
    {
        key(&b, PDB_KEY_BINARY, 0);
        quoted(&b, where);
    }
    key(&b, PDB_KEY_BUNDLE_RULE, 0);
    quoted(&b, g_rules[PDB_BUNDLE_RULES_NEWEST_VERSION]);
    key(&b, PDB_KEY_FILES, 0);
    puts_(&b, "[");
    for (n = 0; n < c.files.n; n++)
    {
        puts_(&b, n ? ", {" : "{");
        key(&b, PDB_KEY_PATH, 1);
        quoted(&b, files[n].path);
        key(&b, PDB_KEY_SHA256, 0);
        quoted(&b, files[n].sha256);
        puts_(&b, "}");
    }
    puts_(&b, "]");
    key(&b, PDB_KEY_LAYOUT, 0);
    if (layout)
        quoted(&b, layout);
    else
        puts_(&b, "null");
    puts_(&b, "}");
    if (b.failed || layer_write(entry, PDB_LAYERS_PINNED, address, pin, layout, "plugin-db", b.s) != 0)
        say(why, why_size, "the pinned layer of %s can't be written under %s", address, entry);
    else
        rc = 0;
out:
    free(b.s);
    free(files);
    pdb_collection_free(&c);
    return rc;
}

int pdb_layer_put(const char *store, const char *address, int layer, const char *writer, const char *body, char *why,
                  size_t why_size)
{
    char entry[PDB_PATH_MAX], resolved[PDB_PATH_MAX];
    layer_t pinned;
    pdb_json_t *tree;
    int rc;

    if (layer < 0 || layer >= PDB_LAYERS_COUNT)
        return say(why, why_size, "no layer of PLUGIN_DB_LAYERS");
    if (layer == PDB_LAYERS_PINNED)
        return say(why, why_size, "the pinned layer is written by pdb_pin, from the files");
    if (resolve(store, address, resolved, sizeof(resolved), entry, sizeof(entry), why, why_size) != 0)
        return -1;
    tree = pdb_json_parse(body, strlen(body));
    rc = body_valid(layer, tree, why, why_size);
    pdb_json_free(tree);
    if (rc != 0)
        return -1;
    rc = layer_read(entry, PDB_LAYERS_PINNED, resolved, &pinned, why, why_size);
    if (rc != 0)
        return rc > 0 ? say(why, why_size, "%s has no pinned layer: a layer is written for a pin", resolved) : -1;
    rc = layer_write(entry, layer, resolved, pinned.files, pinned.layout, writer, body);
    pdb_json_free(pinned.tree);
    return rc == 0 ? 0 : say(why, why_size, "the %s layer can't be written under %s", g_layers[layer], entry);
}

/* ---------------------------------------------------------------- the check */

/* a record from its layer, against the pinned layer's pin; its strings point into the layer's tree */
typedef struct RECORD_STORE_T {
    long (*cells)[2];
    char **conditions;
    int nconditions;
} record_store_t;

static void record_load(const layer_t *l, const layer_t *pinned, pdb_record_t *r, record_store_t *keep)
{
    const pdb_json_t *host, *cells, *conds, *epoch;
    const char *verdict;
    size_t n;
    int k;

    memset(r, 0, sizeof(*r));
    memset(keep, 0, sizeof(*keep));
    if (!l->tree)
        return;
    r->present = 1;
    r->pin_files = pinned->tree && !strcmp(l->files, pinned->files);
    r->pin_layout = pinned->tree && (l->layout ? pinned->layout && !strcmp(l->layout, pinned->layout) : !pinned->layout);
    verdict = pdb_json_string(l->body, PDB_KEY_VERDICT);
    r->verdict = -1;
    for (k = 0; verdict && k < PDB_PIN_VERDICTS_COUNT; k++)
        if (!strcmp(verdict, g_verdicts[k]))
            r->verdict = k;
    r->machine_class = pdb_json_string(l->body, PDB_KEY_MACHINE_CLASS);
    host = pdb_json_get(l->body, PDB_KEY_HOST);
    r->host_name = pdb_json_string(host, PDB_KEY_NAME);
    r->host_version = pdb_json_string(host, PDB_KEY_VERSION);
    epoch = pdb_json_get(host, PDB_KEY_HOSTING_EPOCH);
    r->hosting_epoch = epoch && epoch->type == PDB_JSON_NUMBER ? (long)epoch->number : -1;
    r->rules = pdb_json_string(l->body, PDB_KEY_RULES);
    r->record_schema = pdb_json_string(l->body, PDB_KEY_RECORD_SCHEMA);
    cells = pdb_json_get(l->body, PDB_KEY_CELLS);
    if (cells && cells->type == PDB_JSON_ARRAY && cells->count &&
        (keep->cells = calloc(cells->count, sizeof(*keep->cells))))
    {
        for (n = 0; n < cells->count; n++)
        {
            const pdb_json_t *rate = pdb_json_get(cells->items[n], PDB_KEY_RATE);
            const pdb_json_t *quantum = pdb_json_get(cells->items[n], PDB_KEY_QUANTUM);

            if (rate && quantum && rate->type == PDB_JSON_NUMBER && quantum->type == PDB_JSON_NUMBER)
            {
                keep->cells[r->ncells][0] = (long)rate->number;
                keep->cells[r->ncells][1] = (long)quantum->number;
                r->ncells++;
            }
        }
        r->cells = (const long (*)[2])keep->cells;
    }
    conds = pdb_json_get(l->body, PDB_KEY_CONDITIONS);
    if (conds && conds->type == PDB_JSON_OBJECT && conds->count &&
        (keep->conditions = calloc(conds->count, sizeof(char *))))
    {
        for (n = 0; n < conds->count; n++)
        {
            const char *value = conds->items[n]->type == PDB_JSON_STRING ? conds->items[n]->string : "";
            size_t len = strlen(conds->keys[n]) + strlen(value) + 2;

            /* a condition that cannot be held in memory is one that no fact here matches */
            if ((keep->conditions[keep->nconditions] = malloc(len)))
                snprintf(keep->conditions[keep->nconditions], len, "%s=%s", conds->keys[n], value);
            else if (!(keep->conditions[keep->nconditions] = strdup("")))
                continue;
            keep->nconditions++;
        }
        r->conditions = (const char *const *)keep->conditions;
        r->nconditions = keep->nconditions;
    }
    else if (!conds || conds->type != PDB_JSON_OBJECT || conds->count)
    {
        /* conditions that cannot be read are conditions no fact here meets */
        static const char *const unmet[] = { "" };

        r->conditions = unmet;
        r->nconditions = 1;
    }
}

static void record_drop(record_store_t *keep)
{
    int n;

    for (n = 0; n < keep->nconditions; n++)
        free(keep->conditions[n]);
    free(keep->conditions);
    free(keep->cells);
}

/* the figure of the answering record's cell at the rate and quantum here, and the line it declares */
static void figures_of(const pdb_json_t *body, const pdb_answer_t *answer, const pdb_here_t *here, pdb_detail_t *d)
{
    const pdb_json_t *cells = answer->source == PDB_SOURCES_NONE ? NULL : pdb_json_get(body, PDB_KEY_CELLS);
    const pdb_json_t *line = pdb_json_get(body, PDB_KEY_LATENCY_LINE_MS);
    size_t n;

    d->standing = PDB_LATENCY_UNMEASURED;
    for (n = 0; cells && cells->type == PDB_JSON_ARRAY && n < cells->count; n++)
    {
        const pdb_json_t *rate = pdb_json_get(cells->items[n], PDB_KEY_RATE);
        const pdb_json_t *quantum = pdb_json_get(cells->items[n], PDB_KEY_QUANTUM);
        const pdb_json_t *ms = pdb_json_get(cells->items[n], PDB_KEY_LATENCY_MS);

        if (!rate || !quantum || rate->type != PDB_JSON_NUMBER || quantum->type != PDB_JSON_NUMBER ||
            (long)rate->number != here->rate || (long)quantum->number != here->quantum || here->rate <= 0 ||
            here->quantum <= 0)
            continue;
        d->has_cell = 1;
        d->rate = here->rate;
        d->quantum = here->quantum;
        d->has_latency = ms && ms->type == PDB_JSON_NUMBER;
        d->latency_ms = d->has_latency ? ms->number : 0;
        d->has_line = line && line->type == PDB_JSON_NUMBER;
        d->line_ms = d->has_line ? line->number : 0;
        d->standing = pdb_latency_standing(d->has_latency, d->latency_ms, d->has_line, d->line_ms);
        return;
    }
}

/* the files attribute: the plugin collected again where the pinned layer says, and every file hashed again */
static int files_check(const char *address, const layer_t *pinned, char *problem, size_t size)
{
    const char *format = pdb_json_string(pinned->body, PDB_KEY_FORMAT);
    const char *root = pdb_json_string(pinned->body, PDB_KEY_ROOT);
    const pdb_json_t *files = pdb_json_get(pinned->body, PDB_KEY_FILES);
    const char *where;
    pdb_collection_t c;
    pdb_address_t a;
    int verdict = PDB_FILES_SAME, n;
    size_t k;

    if (pdb_address_parse(address, &a) != 0 || !format || strcmp(format, g_formats[a.format]) || !root || !files ||
        files->type != PDB_JSON_ARRAY)
    {
        say(problem, size, "the pinned layer is not the schema's");
        return PDB_FILES_COULD_NOT_TEST;
    }
    where = pdb_json_string(pinned->body, a.format == PDB_ADDRESS_FORMATS_LV2 ? PDB_KEY_SEARCH : PDB_KEY_BINARY);
    switch (pdb_collect(&a, where, &c, problem, size))
    {
    case PDB_COLLECT_OK:
        break;
    case PDB_COLLECT_ABSENT:
        pdb_collection_free(&c);
        if (a.format == PDB_ADDRESS_FORMATS_LV2)
            say(problem, size, "no bundle of %s names the plugin; it was %s", where ? where : "", root);
        return PDB_FILES_ABSENT;
    default:
        pdb_collection_free(&c);
        return PDB_FILES_COULD_NOT_TEST;
    }
    if (strcmp(c.root, root))
    {
        say(problem, size, "the plugin is now %s; it was pinned in %s", c.root, root);
        verdict = PDB_FILES_CHANGED;
    }
    for (n = 0; n < c.files.n && verdict == PDB_FILES_SAME; n++)
    {
        int held = 0;

        for (k = 0; k < files->count && !held; k++)
        {
            const char *path = pdb_json_string(files->items[k], PDB_KEY_PATH);

            held = path && !strcmp(path, c.files.name[n]);
        }
        if (!held)
        {
            say(problem, size, "%s is named by the plugin and not held by the pin", c.files.name[n]);
            verdict = PDB_FILES_CHANGED;
        }
    }
    for (k = 0; k < files->count && verdict == PDB_FILES_SAME; k++)
    {
        const char *path = pdb_json_string(files->items[k], PDB_KEY_PATH);
        const char *want = pdb_json_string(files->items[k], PDB_KEY_SHA256);
        char hex[PHD_SHA256_HEX_LEN + 1];
        int err;

        if (!path || !want || !pdb_path_valid(path))
        {
            say(problem, size, "the pinned layer names a file that is no file of its root");
            verdict = PDB_FILES_COULD_NOT_TEST;
        }
        else if (digest_of(root, path, hex, &err) != 0)
        {
            say(problem, size, "%s %s", path, err == ENOENT ? "is missing" : err == EISDIR ? "is not a file"
                                                                             : "can't be read");
            verdict = err == ENOENT || err == EISDIR ? PDB_FILES_CHANGED : PDB_FILES_COULD_NOT_TEST;
        }
        else if (strcmp(hex, want))
        {
            say(problem, size, "%s differs from its pin", path);
            verdict = PDB_FILES_CHANGED;
        }
    }
    pdb_collection_free(&c);
    return verdict;
}

int pdb_check(const char *store, const char *text, const pdb_here_t *here, const char *current_layout,
              pdb_answer_t *out, pdb_detail_t *detail, char *why, size_t why_size)
{
    char entry[PDB_PATH_MAX], address[PDB_PATH_MAX], problem[PDB_PATH_MAX] = "";
    layer_t pinned, local, reference, warned;
    record_store_t keep_local, keep_reference;
    pdb_state_in_t in;
    int rc;

    if (resolve(store, text, address, sizeof(address), entry, sizeof(entry), why, why_size) != 0)
        return -1;
    memset(&in, 0, sizeof(in));
    rc = layer_read(entry, PDB_LAYERS_PINNED, address, &pinned, problem, sizeof(problem));
    in.pinned = rc <= 0;
    if (rc > 0)
        say(problem, sizeof(problem), "no pinned layer");
    in.files = rc == 0 ? files_check(address, &pinned, problem, sizeof(problem)) : PDB_FILES_COULD_NOT_TEST;
    if (rc != 0 || !pinned.layout)
        in.layout = PDB_LAYOUT_NOT_YET_KNOWN;
    else if (current_layout)
        in.layout = strcmp(current_layout, pinned.layout) ? PDB_LAYOUT_CHANGED : PDB_LAYOUT_SAME;
    else
        in.layout = in.files == PDB_FILES_SAME ? PDB_LAYOUT_SAME : PDB_LAYOUT_NOT_YET_KNOWN;

    /* a layer that cannot be read is no record: could not test is never passed */
    if (layer_read(entry, PDB_LAYERS_JUDGED, address, &local, problem[0] ? NULL : problem, sizeof(problem)) < 0)
        memset(&local, 0, sizeof(local));
    if (layer_read(entry, PDB_LAYERS_REFERENCE, address, &reference, problem[0] ? NULL : problem, sizeof(problem)) < 0)
        memset(&reference, 0, sizeof(reference));
    if (layer_read(entry, PDB_LAYERS_WARNED_OVERRIDE, address, &warned, NULL, 0) < 0)
        memset(&warned, 0, sizeof(warned));
    record_load(&local, &pinned, &in.local, &keep_local);
    record_load(&reference, &pinned, &in.reference, &keep_reference);
    in.warned_override = warned.tree && pinned.tree && in.files == PDB_FILES_SAME && !strcmp(warned.files, pinned.files);

    pdb_state(&in, here, out);

    if (detail)
    {
        const pdb_json_t *host = pdb_json_get(local.body, PDB_KEY_HOST);
        const char *s;

        memset(detail, 0, sizeof(*detail));
        snprintf(detail->address, sizeof(detail->address), "%s", address);
        snprintf(detail->entry, sizeof(detail->entry), "%s", entry);
        snprintf(detail->root, sizeof(detail->root), "%s",
                 (s = pdb_json_string(pinned.body, PDB_KEY_ROOT)) ? s : "");
        snprintf(detail->layout, sizeof(detail->layout), "%s", pinned.layout ? pinned.layout : "");
        snprintf(detail->problem, sizeof(detail->problem), "%s", problem);
        if (out->source == PDB_SOURCES_REFERENCE)
            host = pdb_json_get(reference.body, PDB_KEY_HOST);
        snprintf(detail->verdict, sizeof(detail->verdict), "%s",
                 (s = pdb_json_string(out->source == PDB_SOURCES_REFERENCE ? reference.body : local.body,
                                      PDB_KEY_VERDICT)) ? s : "");
        snprintf(detail->machine_class, sizeof(detail->machine_class), "%s",
                 (s = pdb_json_string(out->source == PDB_SOURCES_REFERENCE ? reference.body : local.body,
                                      PDB_KEY_MACHINE_CLASS)) ? s : "");
        figures_of(out->source == PDB_SOURCES_REFERENCE ? reference.body : local.body, out, here, detail);
        snprintf(detail->host_name, sizeof(detail->host_name), "%s", (s = pdb_json_string(host, PDB_KEY_NAME)) ? s : "");
        snprintf(detail->host_version, sizeof(detail->host_version), "%s",
                 (s = pdb_json_string(host, PDB_KEY_VERSION)) ? s : "");
    }
    record_drop(&keep_local);
    record_drop(&keep_reference);
    pdb_json_free(pinned.tree);
    pdb_json_free(local.tree);
    pdb_json_free(reference.tree);
    pdb_json_free(warned.tree);
    return 0;
}
