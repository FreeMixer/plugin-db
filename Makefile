# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
CC ?= gcc
PKG_CONFIG ?= pkg-config

# the one version: VERSION holds it, and `make version-check` holds the spec's Version and the newest debian/changelog
# entry to it
VERSION := $(strip $(shell cat VERSION))
NAME = plugin-db

PREFIX = /usr/local
INCLUDEDIR = $(PREFIX)/include
DATADIR = $(PREFIX)/share

HEADERS = include/plugin-db/pin.h include/plugin-db/plugin_db_vocabulary.h

CFLAGS += -O2 -g -Wall -Wextra -std=gnu99 -D_GNU_SOURCE
CRYPTO_CFLAGS = $(shell $(PKG_CONFIG) --cflags libcrypto)
CRYPTO_LIBS = $(shell $(PKG_CONFIG) --libs libcrypto)

# header-only: nothing to build
all:

install:
	install -d $(DESTDIR)$(INCLUDEDIR)/$(NAME) $(DESTDIR)$(DATADIR)/pkgconfig
	install -m 644 $(HEADERS) $(DESTDIR)$(INCLUDEDIR)/$(NAME)/
	sed -e 's,@PREFIX@,$(PREFIX),g' -e 's,@VERSION@,$(VERSION),g' $(NAME).pc.in > $(DESTDIR)$(DATADIR)/pkgconfig/$(NAME).pc
	chmod 644 $(DESTDIR)$(DATADIR)/pkgconfig/$(NAME).pc

# ---------------------------------------------------------------------------------------------------------------------
# tests: no plugin, no daemon

test: test-pin test-install test-sabotage version-check

tests/pin_test: tests/pin_test.c $(HEADERS)
	$(CC) -Iinclude $(CRYPTO_CFLAGS) $(CFLAGS) -Werror -o $@ tests/pin_test.c $(CRYPTO_LIBS)

test-pin: tests/pin_test
	./tests/pin_test

# a consumer compiled and linked with what pkg-config plugin-db says of a scratch install, and nothing else
test-install:
	sh tests/install-check.sh

# every rule and every byte of the format has an arm that must go red
test-sabotage:
	sh tests/sabotage.sh

# VERSION, the spec's Version and the newest debian/changelog entry say the same (the shared release workflows require it)
version-check:
	@spec=$$(sed -n 's/^Version:[[:space:]]*//p' packaging/$(NAME).spec | head -1); \
	deb=$$(sed -n '1s/^[^ ]* (\([^)]*\)).*/\1/p' debian/changelog); \
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

# data/ as the package plugin-db of omx-contract's contract directory, <contractdir>/<schema major>/plugin-db/, with
# the MANIFEST.json omx-contract's script writes
install-contract:
	@test -n "$(INSTALL_CONTRACT_DIR)" || { echo "install-contract: no install-contract-dir.sh: install omx-contract-devel, or set OMX_CONTRACT_DIR to a checkout" >&2; exit 1; }
	sh $(INSTALL_CONTRACT_DIR) $(NAME) $(VERSION) $(CONTRACT_SCHEMA_MAJOR) data $(DESTDIR)$(CONTRACTDIR) data

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
	sh $(INSTALL_CONTRACT_DIR) $(NAME) $(VERSION) $(CONTRACT_SCHEMA_MAJOR) data $$tmp data >/dev/null; \
	OMX_CONTRACT_PATH=$$tmp$(if $(OMX_CONTRACT_PATH),:$(OMX_CONTRACT_PATH)):$(CONTRACTDIR) \
	  $(OMX_CONTRACT) render --target c --package $(NAME) --installed --out include/plugin-db $(if $(filter check-rendered,$@),--check)

clean:
	rm -rf build tests/pin_test

.PHONY: all install test test-pin test-install test-sabotage version-check check-tools install-contract check-contract-dir \
        test-rendered-sabotage render-words check-rendered clean
