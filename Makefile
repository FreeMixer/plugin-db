# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
CC ?= gcc
PKG_CONFIG ?= pkg-config

# the one version: VERSION holds it, and `make version-check` holds the spec's Version and the newest debian/changelog
# entry to it
VERSION := $(strip $(shell cat VERSION))
NAME = plugin-db

PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
INCLUDEDIR = $(PREFIX)/include
LIBDIR = $(PREFIX)/lib
DATADIR = $(PREFIX)/share

HEADERS = include/plugin-db/pin.h include/plugin-db/plugin_db_vocabulary.h include/plugin-db/db.h \
          include/plugin-db/plugin_db_tables.h

CFLAGS ?= -O2 -g
CFLAGS += -Wall -Wextra -std=gnu99 -D_GNU_SOURCE
CRYPTO_CFLAGS = $(shell $(PKG_CONFIG) --cflags libcrypto)
CRYPTO_LIBS = $(shell $(PKG_CONFIG) --libs libcrypto)

# libplugin-db.a and the command plugin-db; pin.h stays header-only
BUILD = build
LIB_SRC = src/address.c src/collect.c src/json.c src/manifest.c src/state.c src/store.c
LIB_OBJ = $(LIB_SRC:src/%.c=$(BUILD)/%.o)
LIB = $(BUILD)/lib$(NAME).a
BIN = $(BUILD)/$(NAME)
ALL_CFLAGS = -Iinclude -Isrc $(CRYPTO_CFLAGS) $(CFLAGS) -Werror

all: $(LIB) $(BIN)

# position-independent, so a shared object may link the archive too
$(BUILD)/%.o: src/%.c $(HEADERS) $(wildcard src/*.h)
	@mkdir -p $(BUILD)
	$(CC) $(ALL_CFLAGS) -fPIC -c -o $@ $<

$(LIB): $(LIB_OBJ)
	rm -f $@
	$(AR) rcs $@ $(LIB_OBJ)

$(BIN): src/cli.c $(LIB)
	$(CC) $(ALL_CFLAGS) $(LDFLAGS) -o $@ src/cli.c $(LIB) $(CRYPTO_LIBS)

install: all
	install -d $(DESTDIR)$(INCLUDEDIR)/$(NAME) $(DESTDIR)$(DATADIR)/pkgconfig $(DESTDIR)$(BINDIR) $(DESTDIR)$(LIBDIR) \
	  $(DESTDIR)$(DATADIR)/$(NAME)
	install -m 644 $(HEADERS) $(DESTDIR)$(INCLUDEDIR)/$(NAME)/
	install -m 644 $(LIB) $(DESTDIR)$(LIBDIR)/
	install -m 755 $(BIN) $(DESTDIR)$(BINDIR)/
	install -m 644 data/store.schema.json $(DESTDIR)$(DATADIR)/$(NAME)/
	sed -e 's,@PREFIX@,$(PREFIX),g' -e 's,@LIBDIR@,$(LIBDIR),g' -e 's,@INCLUDEDIR@,$(INCLUDEDIR),g' \
	  -e 's,@VERSION@,$(VERSION),g' $(NAME).pc.in > $(DESTDIR)$(DATADIR)/pkgconfig/$(NAME).pc
	chmod 644 $(DESTDIR)$(DATADIR)/pkgconfig/$(NAME).pc

# ---------------------------------------------------------------------------------------------------------------------
# tests: no plugin, no daemon

test: test-pin check-tables test-state test-collect test-store test-install test-sabotage test-db-sabotage test-abi version-check

tests/pin_test: tests/pin_test.c $(HEADERS)
	$(CC) -Iinclude $(CRYPTO_CFLAGS) $(CFLAGS) -Werror -o $@ tests/pin_test.c $(CRYPTO_LIBS)

test-pin: tests/pin_test
	./tests/pin_test

# plugin_db_tables.h is what tools/render-db.py derives from data/ (python3 only: part of make test)
check-tables:
	python3 tools/render-db.py --check data include/plugin-db/plugin_db_tables.h

render-tables:
	python3 tools/render-db.py data include/plugin-db/plugin_db_tables.h

$(BUILD)/%_test: tests/%_test.c $(LIB)
	$(CC) $(ALL_CFLAGS) $(LDFLAGS) -o $@ $< $(LIB) $(CRYPTO_LIBS)

# the state function by table: every state, reason, change, action, run and source of data/db.json reached
test-state: $(BUILD)/state_test
	$(BUILD)/state_test

# the address form, the bundle rule and the file collection, over fixture bundles
test-collect: $(BUILD)/collect_test
	$(BUILD)/collect_test

# the store, show and verify through the command
test-store: $(BIN)
	PDB=$(BIN) sh tests/store.sh

# every rule of the state function, the bundle rule, the collection and the store has an arm that must go red
test-db-sabotage:
	sh tests/db-sabotage.sh

# the library, the command and their tests under AddressSanitizer and UBSan, in a build directory of their own
test-asan:
	$(MAKE) --no-print-directory BUILD=build/asan CFLAGS="-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
	  -fno-sanitize-recover=all" LDFLAGS="-fsanitize=address,undefined" test-state test-collect test-store

# a consumer compiled and linked with what pkg-config plugin-db says of a scratch install, and nothing else
test-install:
	sh tests/install-check.sh

# every rule and every byte of the format has an arm that must go red
test-sabotage:
	sh tests/sabotage.sh

# VERSION, the spec's Version and the newest debian/changelog entry say the same (the shared release workflows require it)
version-check:
	@spec=$$(sed -n 's/^Version:[[:space:]]*//p' packaging/$(NAME).spec | head -1); \
	deb=$$(sed -n '1s/^[^ ]* (\([^)]*\)).*/\1/p' debian/changelog); deb=$${deb%%~git*}; \
	test "$$spec" = "$(VERSION)" && test "$$deb" = "$(VERSION)" || \
	  { echo "version-check: VERSION $(VERSION), spec $$spec, debian/changelog $$deb" >&2; exit 1; }; \
	echo "ok   version $(VERSION) in VERSION, the spec and debian/changelog"

# ---------------------------------------------------------------------------------------------------------------------
# The checks that need tools a package build does not: run by CI (.github/workflows/ci.yml, job words).
#   check-rendered      plugin_db_vocabulary.h is what omx-contract renders from data/
#   test-rendered-sabotage  check-rendered fails on a header changed by hand and on data/ not rendered again
#   check-contract-dir  data/ as a package validates with the installed contract directory
OMX_CONTRACT ?= omx-contract
CONTRACT_SCHEMA_MAJOR = 3
CONTRACTDIR = $(or $(shell $(PKG_CONFIG) --variable=contractdir omx-contract 2>/dev/null),$(DATADIR)/omx-contract/contract.d)
INSTALL_CONTRACT_DIR = $(firstword $(wildcard $(shell $(PKG_CONFIG) --variable=datadir omx-contract 2>/dev/null)/install-contract-dir.sh) $(wildcard $(OMX_CONTRACT_DIR)/tools/install-contract-dir.sh))

check-tools: check-rendered test-rendered-sabotage check-contract-dir

# data/store.schema.json is a JSON Schema of the store, not a file of omx-contract's: it is installed to
# $(DATADIR)/plugin-db/, never into the contract directory
CONTRACT_EXCLUDE = -x store.schema.json

# data/ as the package plugin-db of omx-contract's contract directory, <contractdir>/<schema major>/plugin-db/, with
# the MANIFEST.json omx-contract's script writes
install-contract:
	@test -n "$(INSTALL_CONTRACT_DIR)" || { echo "install-contract: no install-contract-dir.sh: install omx-contract-devel, or set OMX_CONTRACT_DIR to a checkout" >&2; exit 1; }
	sh $(INSTALL_CONTRACT_DIR) $(CONTRACT_EXCLUDE) $(NAME) $(VERSION) $(CONTRACT_SCHEMA_MAJOR) data $(DESTDIR)$(CONTRACTDIR) data

check-contract-dir:
	@command -v $(OMX_CONTRACT) >/dev/null || { echo "$@: no $(OMX_CONTRACT) command: install omx-contract" >&2; exit 1; }
	@set -e; tmp=$$(mktemp -d); trap 'rm -rf $$tmp' EXIT; \
	$(MAKE) --no-print-directory install-contract DESTDIR=$$tmp >/dev/null; \
	OMX_CONTRACT_PATH=$$tmp$(CONTRACTDIR)$(if $(OMX_CONTRACT_PATH),:$(OMX_CONTRACT_PATH)):$(CONTRACTDIR) $(OMX_CONTRACT) validate --installed

test-rendered-sabotage:
	sh tests/rendered-sabotage.sh

render-words check-rendered:
	@command -v $(OMX_CONTRACT) >/dev/null || { echo "$@: no $(OMX_CONTRACT) command: install omx-contract" >&2; exit 1; }
	@test -n "$(INSTALL_CONTRACT_DIR)" || { echo "$@: no install-contract-dir.sh: install omx-contract-devel, or set OMX_CONTRACT_DIR to a checkout" >&2; exit 1; }
	@set -e; tmp=$$(mktemp -d); trap 'rm -rf $$tmp' EXIT; \
	sh $(INSTALL_CONTRACT_DIR) $(CONTRACT_EXCLUDE) $(NAME) $(VERSION) $(CONTRACT_SCHEMA_MAJOR) data $$tmp data >/dev/null; \
	OMX_CONTRACT_PATH=$$tmp$(if $(OMX_CONTRACT_PATH),:$(OMX_CONTRACT_PATH)):$(CONTRACTDIR) \
	  $(OMX_CONTRACT) render --target c --package $(NAME) --installed --out include/plugin-db $(if $(filter check-rendered,$@),--check)

# ---------------------------------------------------------------------------------------------------------------------
# ABI. The library is header-only: its functions are static inline and compile into the consumer, so no symbol is
# exported. What a host and the library share at run time is the layout of the records a host fills (the digest context,
# the CLAP parameter, the LV2 port, the sink) and the constants beside them.
#   test-abi        abi/plugin-db.layout is what tests/abi-layout.c prints (needs the compiler only)
#   abi-layout      write abi/plugin-db.layout again
#   abi-baseline    write abi/plugin-db.abi again: abidw over tests/abi-probe.c, built into a shared object that
#                   exports one object of each of those types and is never installed
#   abi-check       abidiff of that baseline against the tree (needs libabigail); red on a change of a type
ABI_LAYOUT = abi/$(NAME).layout
ABI_BASELINE = abi/$(NAME).abi
ABI_PROBE = build/abi/libplugin-db-abi.so

build/abi/abi-layout: tests/abi-layout.c $(HEADERS)
	@mkdir -p $(@D)
	$(CC) -Iinclude $(CRYPTO_CFLAGS) $(CFLAGS) -Werror -o $@ tests/abi-layout.c

$(ABI_PROBE): tests/abi-probe.c $(HEADERS)
	@mkdir -p $(@D)
	$(CC) -Iinclude $(CRYPTO_CFLAGS) $(CFLAGS) -Werror -fPIC -shared -Wl,-soname,libplugin-db-abi.so -o $@ tests/abi-probe.c

test-abi: build/abi/abi-layout
	@build/abi/abi-layout | diff -u $(ABI_LAYOUT) - && echo "abi: the records are laid out as $(ABI_LAYOUT) says"

abi-layout: build/abi/abi-layout
	build/abi/abi-layout > $(ABI_LAYOUT)

abi-baseline: $(ABI_PROBE)
	abidw --headers-dir include/$(NAME) --out-file $(ABI_BASELINE) $(ABI_PROBE)

abi-check: $(ABI_PROBE)
	abidiff --headers-dir2 include/$(NAME) $(ABI_BASELINE) $(ABI_PROBE)

# ---------------------------------------------------------------------------------------------------------------------
# fuzz (fuzz/README.md): one libFuzzer harness per input surface of pin.h (the text of a pin, the bytes of a layout, the
# digest), built with clang, AddressSanitizer and UndefinedBehaviorSanitizer into build/fuzz. None is part of `make test`.
#   make fuzz                   build every harness
#   make fuzz-run FUZZ_TIME=60  fuzz each for the budget; red on any crash, the input left in fuzz/findings/
#   make fuzz-sabotage          each patch of fuzz/sabotage/ must make its harness abort on a seed, in the property its
#                               header names
#   make fuzz-records           each check's mutant: and input: record, alone, must make that check abort on that seed;
#                               red on a check with no record and no not-a-property line
#   make fuzz-coverage          line coverage of the header over each seed corpus, red below fuzz/coverage-floor.txt
#                               or on a property check no seed reaches
#   make fuzz-corpus            write fuzz/corpus/ again; fuzz-corpus-check is red when it is not what that writes
FUZZ_CC        ?= clang
FUZZ_TIME      ?= 60
FUZZ_BUILD     := build/fuzz
FUZZ_HARNESSES := pin layout digest
FUZZ_SAN       := -fsanitize=address,undefined -fno-sanitize-recover=all
FUZZ_CFLAGS    := -O1 -g -std=gnu99 -D_GNU_SOURCE -Wall -Wextra -fno-omit-frame-pointer -Iinclude -Ifuzz
FUZZ_DEPS      := fuzz/fuzz.h fuzz/spec.h $(HEADERS)

$(FUZZ_BUILD)/fuzz_%: fuzz/fuzz_%.c $(FUZZ_DEPS)
	@mkdir -p $(@D)
	$(FUZZ_CC) $(FUZZ_CFLAGS) $(CRYPTO_CFLAGS) $(FUZZ_SAN) -fsanitize=fuzzer $< -o $@ $(CRYPTO_LIBS)

fuzz: $(FUZZ_HARNESSES:%=$(FUZZ_BUILD)/fuzz_%)

fuzz-run: fuzz
	FUZZ_BUILD=$(FUZZ_BUILD) FUZZ_TIME=$(FUZZ_TIME) fuzz/run.sh $(FUZZ_HARNESSES)

fuzz-sabotage:
	FUZZ_BUILD=$(FUZZ_BUILD) MAKE="$(MAKE)" fuzz/sabotage.sh

# each record built plain and native with fuzz/standalone.c for its driver, no sanitizer, as the OSS Verify coverage
# build is
fuzz-records:
	FUZZ_BUILD=$(FUZZ_BUILD) FUZZ_CC=$(FUZZ_CC) fuzz/records.sh $(FUZZ_HARNESSES)

# the same harnesses without sanitizers, instrumented for llvm-cov, each run once over its seed corpus (-runs=0 runs the
# corpus and nothing else); the llvm-cov and llvm-profdata of FUZZ_CC's own LLVM
FUZZ_COV_BUILD := $(FUZZ_BUILD)/cov
FUZZ_COV_FLAGS := -O0 -g -std=gnu99 -D_GNU_SOURCE -Iinclude -Ifuzz -fprofile-instr-generate -fcoverage-mapping
LLVM_COV       ?= llvm-cov
LLVM_PROFDATA  ?= llvm-profdata

$(FUZZ_COV_BUILD)/fuzz_%: fuzz/fuzz_%.c $(FUZZ_DEPS)
	@mkdir -p $(@D)
	$(FUZZ_CC) $(FUZZ_COV_FLAGS) $(CRYPTO_CFLAGS) -fsanitize=fuzzer $< -o $@ $(CRYPTO_LIBS)

fuzz-coverage: $(FUZZ_HARNESSES:%=$(FUZZ_COV_BUILD)/fuzz_%)
	@set -e; for h in $(FUZZ_HARNESSES); do \
	  rm -f $(FUZZ_COV_BUILD)/$$h.profraw; \
	  LLVM_PROFILE_FILE=$(FUZZ_COV_BUILD)/$$h.profraw $(FUZZ_COV_BUILD)/fuzz_$$h -runs=0 fuzz/corpus/$$h/ \
	      >$(FUZZ_COV_BUILD)/$$h.log 2>&1 || \
	    { cat $(FUZZ_COV_BUILD)/$$h.log; echo "fuzz-coverage: fuzz_$$h failed on its own corpus"; exit 1; }; \
	  $(LLVM_PROFDATA) merge -sparse $(FUZZ_COV_BUILD)/$$h.profraw -o $(FUZZ_COV_BUILD)/$$h.profdata; \
	done
	@LLVM_COV=$(LLVM_COV) fuzz/coverage.sh $(FUZZ_COV_BUILD) $(FUZZ_HARNESSES)

# the seed corpus is written by fuzz/gen-corpus.c, never taken from a capture, and committed
FUZZ_GEN := build/fuzz-gen-corpus
$(FUZZ_GEN): fuzz/gen-corpus.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -o $@ $<

fuzz-corpus: $(FUZZ_GEN)
	rm -rf fuzz/corpus
	$(FUZZ_GEN) fuzz/corpus

fuzz-corpus-check: $(FUZZ_GEN)
	@rm -rf build/fuzz-corpus-check
	@$(FUZZ_GEN) build/fuzz-corpus-check
	@diff -r build/fuzz-corpus-check fuzz/corpus && echo "fuzz/corpus is what fuzz-corpus writes"

clean:
	rm -rf build tests/pin_test

.PHONY: all install test test-pin check-tables render-tables test-state test-collect test-store test-db-sabotage test-asan \
        test-install test-sabotage test-abi abi-layout abi-baseline abi-check version-check check-tools install-contract check-contract-dir \
        test-rendered-sabotage render-words check-rendered fuzz fuzz-run fuzz-sabotage fuzz-records fuzz-coverage fuzz-corpus fuzz-corpus-check clean
