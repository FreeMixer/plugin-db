# plugin-db

The plugin database, in layers: one path for a plugin from install to load. It holds the pin of a plugin, the one
address form, which files are a plugin (the LV2 bundle rule included), the store of layers, the one state function,
and the command that asks it: `plugin-db show` and `plugin-db verify`. It depends on libc and libcrypto only, and it
loads no plugin: it reads LV2 manifests as text and hashes files.

## What is declared, and where

Everything a reader compares is declared once in `data/` and derived from there, never typed twice:

- `data/pin.json`: the words of a pin (verdicts, pool words, optional keys, what counts as changed, layout formats).
- `data/db.json`: the address formats, the bundle rule, the layers, the attributes the check reports (files, layout,
  record), what changes a record and the action each change asks for, the states with how each runs, the reasons, the
  sources, and the hosts an isolated verdict must be measured under.
- `data/store.schema.json`: the JSON Schema of a layer file, with the store's layout in `x-plugin-db-store`.

omx-contract renders the words of `pin.json` and `db.json` to `<plugin-db/plugin_db_vocabulary.h>`, and the package
`plugin-db-contract` puts them in its contract directory. `tools/render-db.py` derives
`<plugin-db/plugin_db_tables.h>` from all three: each word's index as an enum, the group each word of a grouped item
falls in (a change's action, an action's record attribute, a state's run, a reason's state), the store's layout and
every key the schema names. `make test` holds that header to `data/`; `make check-tools` holds the vocabulary header.

## The one address form

`<format>:<id>`, or `clap@<sha256>:<id>` when several files share one CLAP id. The format is `clap` or `lv2`; the id is
the CLAP plugin's id or the LV2 plugin's URI; the sha256 is that of the `.clap` file. An LV2 address carries none: the
bundle rule makes one bundle the plugin. A path is never part of an address, so a session keeps its plugins on another machine: `clap:<path>#<id>` and `urn:clap:<id>`
are not addresses.

## Which files are a plugin

For CLAP, the `.clap` file the plugin's code is loaded from; its path is given when the entry is pinned (plugin-db
cannot read an id out of a binary without loading it). For LV2, the plugin's bundle in the search path (`LV2_PATH`,
else `~/.lv2:/usr/local/lib/lv2:/usr/lib/lv2`): its `manifest.ttl` and every file the manifest names for the plugin
with `lv2:binary` or `rdfs:seeAlso`. A manifest that sets a base, or names a file that is not percent-encoded or climbs
out of the bundle, cannot be pinned. This is plugin-hostd's collection (`lv2.c`, `formats.c`), moved.

**The bundle rule, `newest-version`.** When several bundles of the search path name one LV2 plugin, the candidates
are the bundles whose manifest gives it an `lv2:binary`, or, when none does, the bundles that name it (a MOD GUI, a
preset bundle). Of the candidates, the one whose manifest gives the plugin the highest `lv2:minorVersion`, then the
highest `lv2:microVersion` wins, a version not stated counting as 0; of equal versions, the first in the search path,
its directories in order and the bundles of one directory in byte order of their names. This is lilv's rule, so the
host loads the bundle plugin-db pins; plugin-hostd took the first bundle in path order.

## The store

A directory: `$PLUGIN_DB_STORE`, else `/var/lib/plugin-db` for a system service (`--system`), else
`$XDG_STATE_HOME/plugin-db` (`~/.local/state/plugin-db`). It holds `entries/<sha256 of the address>/`, and in each entry
one file per layer, named by its word of `PLUGIN_DB_LAYERS`: `pinned.json`, `judged.json`, `reference.json`,
`warned-override.json` and the others as their writers arrive. Each file is written to a temporary name in its entry and
renamed over its own, so a reader sees the old file or the new one. Each records the pin it was written for (the
`omx-files/1` digest of the files and the layout fingerprint), so a layer written for other bytes is found stale and
never trusted. The entry is the join of its files at read time; no file holds it.

## The state

`pdb_state()` is the one function that answers what a plugin is. It takes the attributes the check found (files:
same, changed, absent, could-not-test; layout: same, changed, not-yet-known), the local record, a shipped record, a
warned override and the facts of here (machine class, host and its hosting epoch, judgement rules, record schema, rate,
quantum, conditions), and answers the record attribute (current, stale, none) with the change that made it stale and
the action it asks for, the state (unqualified, qualified, isolated-only, conditions-unmet, refused, absent), how it
runs (shared, alone, not-loaded), the reason and the source. No reader computes this for itself.

## Use

```
plugin-db pin lv2:http://example.org/amp --layout omx-layout/1:<sha256>
plugin-db pin clap:com.example.synth --file /usr/lib/clap/synth.clap
plugin-db put lv2:http://example.org/amp judged --writer plugin-qualify < record.json
plugin-db show lv2:http://example.org/amp --machine-class rpi5 --host plugin-hostd --host-version 1.4.0 \
    --hosting-epoch 2 --rules rules-7 --record-schema record-3 --rate 48000 --quantum 128
plugin-db verify
```

`show` prints the attributes, the change and its action, the state, how it runs, the reason, the source, the verdict
and what it was measured under, and the first thing the check found. `verify` checks every entry as `rpm -V` does: one
line for each entry that is not files same, layout same and record current, and exit 1 when there is one (`-v` prints
every entry). A fact of here that is not given is not known, and no record matches it: could not test is never passed.

A C program includes `<plugin-db/db.h>` (and `<plugin-db/pin.h>`, which it includes) and links `libplugin-db.a`:

```
cc $(pkg-config --cflags plugin-db) your.c $(pkg-config --libs plugin-db)
```

## Install

Fedora 44:

```
sudo dnf install https://www.openmixer.org/rpm/openmixer-release.noarch.rpm
sudo dnf install plugin-db plugin-db-devel
```

Debian bookworm and trixie (also Raspberry Pi OS and Zynthian):

```
curl -fsSLO https://www.openmixer.org/deb/openmixer-release.deb && sudo apt install ./openmixer-release.deb
sudo apt install plugin-db plugin-db-dev
```

## Build and test

```
make
make test
make test-asan
make install PREFIX=/usr/local
```

`make test` needs a C compiler, python3 and libcrypto's headers, and no plugin: the pin's known answers, the tables
derived from `data/`, the state function by table (every state, reason, change and action reached by a fixture), the
address form, the bundle rule and the collection over fixture bundles, the store, `show` and `verify` through the
command, a consumer of an install, and the sabotage arms, each of which breaks one rule and must go red.
`make test-asan` runs the C tests and the command under AddressSanitizer and UBSan. `make check-tools` also holds the
rendered vocabulary header to `data/` and validates the package against the contract directory; it needs omx-contract
and omx-contract-devel. After a change to `data/`, `make render-words render-tables` renders both headers again.

## Licence

GPL-3.0-or-later. See [COPYING](COPYING).
