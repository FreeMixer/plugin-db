<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<!-- Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com> -->

# Fuzzing plugin-db

plugin-db is header-only (`include/plugin-db/pin.h`). Three things in it read bytes that
come from outside, and each has a libFuzzer harness here, built with clang under
AddressSanitizer and UndefinedBehaviorSanitizer and run in CI on every push and pull request
(`.github/workflows/fuzz.yml`). The harnesses are written to the build contract of OSS Verify
(its `docs/build-contract.md`, "The checks of a fuzz target").

| Harness | Reads | Properties |
| --- | --- | --- |
| `fuzz_pin` | a digest, a layout pin `<scheme>:<sha256>`, a verdict word, a pool word: the text of a pin line a host reads back | digest charset, digest length, token refusal, token scheme, token digest, verdict, pool |
| `fuzz_layout` | the `omx-layout/1` bytes written from the parameters of a CLAP plugin or the control ports of an LV2 plugin (any name, number, flag and property a plugin chooses), and their fingerprint | scheme line, row index, row name, row number, row flags, row direction, row props, row end, end, digest |
| `fuzz_digest` | the SHA-256 of any bytes, whole, in chunks, and given up half way | value, chunks, form, state, failed |

`phd_pin_fd_digest()` has no harness: it reads a descriptor, and a harness may not open one.
It takes the steps `fuzz_digest` drives (init, update, final) on what `read()` returns.

Each harness is one C file defining `LLVMFuzzerTestOneInput`. It reads its input as a small
program (`fuzz/fuzz.h`: a cursor that is total, so every input means something and the end of
the input ends it). No harness reads a clock, the environment, a file or the network, starts a
thread or installs a signal handler. An input of 64 MiB or more is refused (`-1`), and every
other outcome returns 0. A string or a name is copied into an allocation of exactly its length,
so a read one byte past it is a sanitizer report.

## The expected values come from the format, not from the header

A check that takes its expected value from the code it judges holds for a header that is wrong
the same way. So no check here calls the header to say what is right. `fuzz/spec.h` holds the
format as the harnesses read it: the digest rule, the escapes, the number and the integer
forms (written with `snprintf`), and a SHA-256 from FIPS 180-4 of the harness's own. The words
of a verdict and a pool are written out in `fuzz_pin.c` from `data/pin.json`.

`fuzz_layout` writes nothing itself but reads what the library wrote, with a cursor: each check
eats the text the harness expects at that place, so the first field that differs aborts in the
check of its own property.

## The checks and their records

Every property is an `if (...) abort();`, never `assert()`, inside a `noinline` function
`check_<property>`. The comment of each check says what it checks, names its `contract:`, and
carries one or more records, as OSS Verify's `tools/check` reads them:

```
/* A digest of any other length is never a digest.
 * contract: include/plugin-db/pin.h:48
 * mutant: include/plugin-db/pin.h:142 `s[n] == '\0'` -> `1`
 * input: digest-65 */
```

A record is a one-line change of `include/plugin-db/pin.h` and the seed it shows on: with the
change, that seed makes THAT check abort; without it, the seed passes. The one stop that is not
a property, the harness's own failed allocation, carries `not a property:` in place of a record.

`make fuzz-records` (`fuzz/records.sh`) proves every record at this commit. First the ratchet:
`fuzz/checks.awk` reads every `abort()` of `fuzz/` and its comment, and a check with no record
and no `not a property:` line is red before anything is built. Then each record alone: it makes
the change (the old text must occur once on that line), builds the plain native harness
(`fuzz/standalone.c` as the driver, every `abort()` naming its own line), runs the named seed,
and requires exit 134 from the record's own check. Every named seed exits 0 on the unchanged
tree.

## The sabotage patches

`fuzz/sabotage/<harness>-<property>.patch` is a bug in the header, one for each property, as
`git diff` writes it; above its `diff --git` line it names the check it breaks and the seed
that shows it (OSS Verify's `mutants:` directory). `make fuzz-sabotage` applies each, rebuilds
the sanitizer harness and runs it over every seed of its corpus: the patch is found when the
seed it names aborts inside the `check_<property>` function that holds the property, and every
seed that fails fails in a check, never in a sanitizer report. `make fuzz-records` proves the
same patches on the exact abort line.

## The coverage witness

`make fuzz-coverage` runs each harness once over its seed corpus without sanitizers, with
llvm-cov. It prints the line coverage of the functions the harness exists to drive and the lines
of them the corpus never reaches, holds the covered lines to the floor in
`fuzz/coverage-floor.txt`, and requires every property check of the harness to be reached by a
seed. The lines no seed reaches are the library's `malloc` failures; the harnesses do not fail
an allocation.

## The seed corpus

`fuzz/corpus/<harness>/` is written by `fuzz/gen-corpus.c` (`make fuzz-corpus`) and committed;
`make fuzz-corpus-check` is red when the directory is not what it writes. A file is named for
what it shows.

## Running it

```
make fuzz                    # build the three harnesses
make fuzz-corpus-check
make fuzz-records
make fuzz-sabotage
make fuzz-coverage           # LLVM_COV=llvm-cov-18 LLVM_PROFDATA=llvm-profdata-18 on a Debian
make fuzz-run FUZZ_TIME=60   # a finding is left in fuzz/findings/
```
