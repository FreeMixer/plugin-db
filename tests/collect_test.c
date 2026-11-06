/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com> */
/* The address form, the bundle rule and the file collection, over fixture bundles written to a scratch directory. */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <plugin-db/db.h>

static int g_fail;
static char g_tmp[1024];

#define CHECK(cond, ...)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(cond))                                                                                                   \
        {                                                                                                              \
            printf("FAIL ");                                                                                           \
            printf(__VA_ARGS__);                                                                                       \
            printf("\n");                                                                                              \
            g_fail = 1;                                                                                                \
        }                                                                                                              \
    } while (0)

static void mkdirs(const char *path)
{
    char copy[PDB_PATH_MAX], *c;

    snprintf(copy, sizeof(copy), "%s", path);
    for (c = copy + 1; *c; c++)
        if (*c == '/')
        {
            *c = '\0';
            mkdir(copy, 0755);
            *c = '/';
        }
    mkdir(copy, 0755);
}

/* a file under the scratch directory, its directories made */
static void file(const char *rel, const char *format, ...)
{
    char path[PDB_PATH_MAX], dir[PDB_PATH_MAX], *slash;
    va_list ap;
    FILE *f;

    snprintf(path, sizeof(path), "%s/%s", g_tmp, rel);
    snprintf(dir, sizeof(dir), "%s", path);
    slash = strrchr(dir, '/');
    *slash = '\0';
    mkdirs(dir);
    f = fopen(path, "w");
    va_start(ap, format);
    vfprintf(f, format, ap);
    va_end(ap);
    fclose(f);
}

#define AMP "http://example.org/plugins/amp"
#define PREFIXES "@prefix lv2: <http://lv2plug.in/ns/lv2core#> .\n@prefix rdfs: <http://www.w3.org/2000/01/rdf-schema#> .\n"

/* a bundle that realises AMP at a version, or only names it when binary is NULL */
static void bundle(const char *rel, const char *binary, int minor, int micro)
{
    char manifest[PDB_PATH_MAX];

    snprintf(manifest, sizeof(manifest), "%s/manifest.ttl", rel);
    if (binary)
        file(manifest, PREFIXES "<" AMP "> a lv2:Plugin ;\n  lv2:binary <%s> ;\n  lv2:minorVersion %d ;\n"
                       "  lv2:microVersion %d ;\n  rdfs:seeAlso <amp.ttl> .\n", binary, minor, micro);
    else
        file(manifest, PREFIXES "<" AMP "> a lv2:Plugin ;\n  lv2:minorVersion %d ;\n  lv2:microVersion %d ;\n"
                       "  rdfs:seeAlso <gui.ttl> .\n", minor, micro);
}

/* the bundle the rule picks for AMP in a search path of directories under the scratch one (or ~/ as given) */
static void expect_bundle(const char *what, const char *search, const char *want)
{
    char full_search[PDB_PATH_MAX * 2] = "", full_want[PDB_PATH_MAX], copy[PDB_PATH_MAX], *dir, *save = NULL;
    char *got;

    snprintf(copy, sizeof(copy), "%s", search);
    for (dir = strtok_r(copy, ":", &save); dir; dir = strtok_r(NULL, ":", &save))
        snprintf(full_search + strlen(full_search), sizeof(full_search) - strlen(full_search), "%s%s%s%s",
                 full_search[0] ? ":" : "", dir[0] == '~' ? "" : g_tmp, dir[0] == '~' ? "" : "/", dir);
    got = pdb_lv2_bundle(full_search, AMP);
    snprintf(full_want, sizeof(full_want), "%s/%s", g_tmp, want ? want : "");
    if (want)
        CHECK(got && !strcmp(got, full_want), "%s: the bundle is %s, not %s", what, got ? got : "(none)", full_want);
    else
        CHECK(!got, "%s: a bundle %s where none names the plugin", what, got);
    free(got);
}

static void test_address(void)
{
    static const char *const good[] = {
        "lv2:http://example.org/plugins/amp", "clap:com.example.synth",
        "clap@0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef:com.example.synth",
        "lv2:urn:example:amp#stereo",
    };
    static const char *const bad[] = {
        "", "lv2", "lv2:", "vst3:com.example.synth", "clap:/usr/lib/clap/synth.clap#com.example.synth",
        "clap@0123:com.example.synth", "clap@0123456789ABCDEF0123456789abcdef0123456789abcdef0123456789abcdef:x",
        "lv2@0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef:http://example.org/plugins/amp",
        "clap:com.example synth", "LV2:http://example.org/plugins/amp", "urn:clap:com.example.synth",
    };
    pdb_address_t a;
    size_t n;

    for (n = 0; n < sizeof(good) / sizeof(good[0]); n++)
        CHECK(pdb_address_parse(good[n], &a) == 0, "address %s is refused", good[n]);
    for (n = 0; n < sizeof(bad) / sizeof(bad[0]); n++)
        CHECK(pdb_address_parse(bad[n], &a) != 0, "address \"%s\" is taken", bad[n]);
    pdb_address_parse("lv2:urn:example:amp#stereo", &a);
    CHECK(a.format == PDB_ADDRESS_FORMATS_LV2 && !strcmp(a.id, "urn:example:amp#stereo") && !a.pin[0],
          "an LV2 id keeps every ':' and '#' after the format");
    pdb_address_parse(good[2], &a);
    CHECK(a.format == PDB_ADDRESS_FORMATS_CLAP && !strcmp(a.id, "com.example.synth") &&
          !strcmp(a.pin, "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"),
          "a CLAP address with a pin parses to its format, pin and id");
}

static void test_bundle_rule(void)
{
    /* newest version across the search path, not the first in path order */
    bundle("r1/a/amp.lv2", "amp.so", 1, 0);
    bundle("r1/b/amp-new.lv2", "amp.so", 2, 0);
    expect_bundle("a newer version later in the path", "r1/a:r1/b", "r1/b/amp-new.lv2");
    expect_bundle("a newer version earlier in the path", "r1/b:r1/a", "r1/b/amp-new.lv2");

    /* equal versions: the first in the path */
    bundle("r2/a/amp.lv2", "amp.so", 3, 1);
    bundle("r2/b/amp.lv2", "amp.so", 3, 1);
    expect_bundle("equal versions", "r2/b:r2/a", "r2/b/amp.lv2");

    /* equal versions in one directory: byte order of the names */
    bundle("r3/z-amp.lv2", "amp.so", 0, 2);
    bundle("r3/a-amp.lv2", "amp.so", 0, 2);
    expect_bundle("equal versions in one directory", "r3", "r3/a-amp.lv2");

    /* the micro version decides between equal minors */
    bundle("r4/a/amp.lv2", "amp.so", 5, 1);
    bundle("r4/b/amp.lv2", "amp.so", 5, 7);
    expect_bundle("a higher micro version", "r4/a:r4/b", "r4/b/amp.lv2");

    /* a complement bundle (no lv2:binary) never wins over one that realises the plugin, whatever its version */
    bundle("r5/a/amp-gui.lv2", NULL, 9, 9);
    bundle("r5/b/amp.lv2", "amp.so", 0, 1);
    expect_bundle("a complement bundle of a higher version", "r5/a:r5/b", "r5/b/amp.lv2");

    /* with no bundle that realises it, the newest that names it */
    bundle("r6/a/amp-gui.lv2", NULL, 1, 0);
    bundle("r6/b/amp-presets.lv2", NULL, 4, 0);
    expect_bundle("only complement bundles", "r6/a:r6/b", "r6/b/amp-presets.lv2");

    /* no version stated counts as 0 */
    file("r7/a/amp.lv2/manifest.ttl", PREFIXES "<" AMP "> a lv2:Plugin ; lv2:binary <amp.so> .\n");
    bundle("r7/b/amp.lv2", "amp.so", 0, 1);
    expect_bundle("a version not stated", "r7/a:r7/b", "r7/b/amp.lv2");

    /* a prefixed name, a typed literal version, a missing directory in the path, a bundle naming another plugin */
    file("r8/a/amp.lv2/manifest.ttl", "@prefix lv2: <http://lv2plug.in/ns/lv2core#> .\n"
                                      "@prefix ex: <http://example.org/plugins/> .\n"
                                      "@prefix xsd: <http://www.w3.org/2001/XMLSchema#> .\n"
                                      "ex:amp a lv2:Plugin ; lv2:binary <amp.so> ; lv2:minorVersion \"6\"^^xsd:integer .\n");
    file("r8/b/other.lv2/manifest.ttl", PREFIXES "<http://example.org/plugins/other> a lv2:Plugin ; lv2:binary <o.so> ;"
                                        " lv2:minorVersion 99 .\n");
    bundle("r8/b/amp.lv2", "amp.so", 5, 0);
    expect_bundle("a prefixed name and a typed version", "r8/none:r8/b:r8/a", "r8/a/amp.lv2");
    /* only a statement about the plugin names it: a mention as an object does not */
    file("r9/a/mention.lv2/manifest.ttl", PREFIXES "<http://example.org/x> rdfs:seeAlso <" AMP "> .\n");
    expect_bundle("a mention as an object", "r9/a", NULL);
    expect_bundle("no bundle", "r9/none", NULL);

    /* a directory given as ~/ is under HOME */
    {
        char home[PDB_PATH_MAX], *old = getenv("HOME") ? strdup(getenv("HOME")) : NULL;

        snprintf(home, sizeof(home), "%s/home", g_tmp);
        bundle("home/.lv2/amp.lv2", "amp.so", 1, 0);
        setenv("HOME", home, 1);
        expect_bundle("a ~/ directory", "~/.lv2", "home/.lv2/amp.lv2");
        if (old)
            setenv("HOME", old, 1);
        free(old);
    }
}

static int collected(const pdb_collection_t *c, int n, ...)
{
    va_list ap;
    int k, ok = c->files.n == n;

    va_start(ap, n);
    for (k = 0; k < n; k++)
    {
        const char *want = va_arg(ap, const char *);

        ok = ok && !strcmp(c->files.name[k], want);
    }
    va_end(ap);
    return ok;
}

static void test_collect(void)
{
    char search[PDB_PATH_MAX], why[PDB_PATH_MAX], path[PDB_PATH_MAX];
    pdb_collection_t c;
    pdb_address_t a;
    int rc;

    pdb_address_parse("lv2:" AMP, &a);

    /* the manifest, then what it names for the plugin in its order, once each, and nothing it names for another */
    file("c1/amp.lv2/manifest.ttl", PREFIXES "<" AMP "> a lv2:Plugin ;\n  lv2:binary <amp.so> ;\n"
                                    "  rdfs:seeAlso <amp.ttl>, <./presets.ttl>, <amp.ttl> .\n"
                                    "<http://example.org/plugins/other> lv2:binary <other.so> .\n");
    snprintf(search, sizeof(search), "%s/c1", g_tmp);
    rc = pdb_collect(&a, search, &c, why, sizeof(why));
    snprintf(path, sizeof(path), "%s/c1/amp.lv2", g_tmp);
    CHECK(rc == PDB_COLLECT_OK && !strcmp(c.root, path) &&
          collected(&c, 4, "manifest.ttl", "amp.so", "amp.ttl", "presets.ttl"),
          "an LV2 plugin's files: rc %d, root %s, %d files (%s)", rc, c.root, c.files.n, why);
    pdb_collection_free(&c);

    /* the file:// form of a name in the bundle is the name */
    file("c2/amp.lv2/manifest.ttl", PREFIXES "<" AMP "> lv2:binary <file://%s/c2/amp.lv2/amp%%20x.so> .\n", g_tmp);
    snprintf(search, sizeof(search), "%s/c2", g_tmp);
    rc = pdb_collect(&a, search, &c, why, sizeof(why));
    CHECK(rc == PDB_COLLECT_OK && collected(&c, 2, "manifest.ttl", "amp%20x.so"),
          "a file:// name in the bundle: rc %d (%s)", rc, why);
    pdb_collection_free(&c);

    /* what cannot be pinned is refused, with its reason */
    file("c3/amp.lv2/manifest.ttl", "@base <http://example.org/> .\n" PREFIXES "<" AMP "> lv2:binary <amp.so> .\n");
    snprintf(search, sizeof(search), "%s/c3", g_tmp);
    rc = pdb_collect(&a, search, &c, why, sizeof(why));
    CHECK(rc == PDB_COLLECT_REFUSED && strstr(why, "base"), "a manifest that sets a base: rc %d (%s)", rc, why);
    pdb_collection_free(&c);
    file("c4/amp.lv2/manifest.ttl", PREFIXES "<" AMP "> lv2:binary <amp%%zz.so> .\n");
    snprintf(search, sizeof(search), "%s/c4", g_tmp);
    rc = pdb_collect(&a, search, &c, why, sizeof(why));
    CHECK(rc == PDB_COLLECT_REFUSED && strstr(why, "percent"), "a name that is not percent-encoded: rc %d (%s)", rc, why);
    pdb_collection_free(&c);
    file("c5/amp.lv2/manifest.ttl", PREFIXES "<" AMP "> lv2:binary <../elsewhere/amp.so> .\n");
    snprintf(search, sizeof(search), "%s/c5", g_tmp);
    rc = pdb_collect(&a, search, &c, why, sizeof(why));
    CHECK(rc == PDB_COLLECT_REFUSED, "a name that climbs out of the bundle: rc %d (%s)", rc, why);
    pdb_collection_free(&c);
    snprintf(search, sizeof(search), "%s/none", g_tmp);
    rc = pdb_collect(&a, search, &c, why, sizeof(why));
    CHECK(rc == PDB_COLLECT_ABSENT, "a plugin in no bundle is absent: rc %d", rc);
    pdb_collection_free(&c);

    /* the search path: LV2_PATH when none is given */
    snprintf(search, sizeof(search), "%s/c1", g_tmp);
    setenv("LV2_PATH", search, 1);
    rc = pdb_collect(&a, NULL, &c, why, sizeof(why));
    CHECK(rc == PDB_COLLECT_OK && c.files.n == 4, "LV2_PATH when no search path is given: rc %d", rc);
    pdb_collection_free(&c);
    unsetenv("LV2_PATH");

    /* a CLAP plugin: the .clap, its name as a pin holds it */
    pdb_address_parse("clap:com.example.synth", &a);
    file("clap/my synth,1.clap", "clap bytes");
    snprintf(path, sizeof(path), "%s/clap/my synth,1.clap", g_tmp);
    rc = pdb_collect(&a, path, &c, why, sizeof(why));
    snprintf(search, sizeof(search), "%s/clap", g_tmp);
    CHECK(rc == PDB_COLLECT_OK && !strcmp(c.root, search) && collected(&c, 1, "my%20synth%2C1.clap"),
          "a CLAP plugin's file: rc %d root %s (%s)", rc, c.root, c.files.n ? c.files.name[0] : "");
    pdb_collection_free(&c);
    snprintf(path, sizeof(path), "%s/clap/gone.clap", g_tmp);
    rc = pdb_collect(&a, path, &c, why, sizeof(why));
    CHECK(rc == PDB_COLLECT_ABSENT, "a CLAP file that is gone is absent: rc %d", rc);
    pdb_collection_free(&c);
    rc = pdb_collect(&a, "synth.clap", &c, why, sizeof(why));
    CHECK(rc == PDB_COLLECT_REFUSED, "a CLAP file that is no absolute path is refused: rc %d", rc);
    pdb_collection_free(&c);
}

int main(void)
{
    char cmd[PDB_PATH_MAX + 16];
    const char *base = getenv("TMPDIR");

    snprintf(g_tmp, sizeof(g_tmp), "%s/plugin-db-collect-XXXXXX", base && *base ? base : "/tmp");
    if (!mkdtemp(g_tmp))
    {
        printf("FAIL no scratch directory: %s\n", strerror(errno));
        return 1;
    }
    test_address();
    test_bundle_rule();
    test_collect();
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", g_tmp);
    if (system(cmd) != 0)
        printf("note: %s is left\n", g_tmp);
    if (g_fail)
        return 1;
    printf("ok   collect: the address form, the bundle rule (newest-version) and the files of LV2 and CLAP plugins\n");
    return 0;
}
