/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com> */

/* plugin-db is header-only: its functions are static inline and compile into the consumer, so no symbol of it is
 * exported. What a consumer and a host share at run time is the layout of the records a host fills and passes on:
 * the digest context, the CLAP parameter, the LV2 port and the sink. This file is built into a shared object that
 * exports one object of each of those types (make abi-baseline), so that abidw records their layout in
 * abi/plugin-db.abi, and abidiff (make abi-check) compares a later layout to it. The object is never installed. */

#include <plugin-db/pin.h>

#define ANCHOR __attribute__((visibility("default")))

ANCHOR const phd_sha256_t plugin_db_abi_sha256;
ANCHOR const phd_layout_clap_param_t plugin_db_abi_clap_param;
ANCHOR const phd_layout_lv2_port_t plugin_db_abi_lv2_port;
ANCHOR const phd_layout_sink_t plugin_db_abi_sink;
