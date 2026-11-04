/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com> */

/* Prints the size, alignment and member offsets of the records of include/plugin-db/pin.h that a host fills, one line
 * each, as abi/plugin-db.layout holds them (make test-abi compares, make abi-layout writes). It needs no tool beyond
 * the compiler, so it holds where abidw is not installed. */

#include <stddef.h>
#include <stdio.h>

#include <plugin-db/pin.h>

#define TYPE(t) printf("%s size %zu align %zu\n", #t, sizeof(t), _Alignof(t))
#define MEMBER(t, m) printf("%s.%s offset %zu size %zu\n", #t, #m, offsetof(t, m), sizeof(((t *)0)->m))

int main(void)
{
    TYPE(phd_sha256_t);
    MEMBER(phd_sha256_t, ctx);
    MEMBER(phd_sha256_t, ok);
    TYPE(phd_layout_clap_param_t);
    MEMBER(phd_layout_clap_param_t, id);
    MEMBER(phd_layout_clap_param_t, name);
    MEMBER(phd_layout_clap_param_t, min);
    MEMBER(phd_layout_clap_param_t, max);
    MEMBER(phd_layout_clap_param_t, def);
    MEMBER(phd_layout_clap_param_t, flags);
    TYPE(phd_layout_lv2_port_t);
    MEMBER(phd_layout_lv2_port_t, index);
    MEMBER(phd_layout_lv2_port_t, symbol);
    MEMBER(phd_layout_lv2_port_t, output);
    MEMBER(phd_layout_lv2_port_t, has);
    MEMBER(phd_layout_lv2_port_t, min);
    MEMBER(phd_layout_lv2_port_t, max);
    MEMBER(phd_layout_lv2_port_t, def);
    MEMBER(phd_layout_lv2_port_t, properties);
    MEMBER(phd_layout_lv2_port_t, nproperties);
    TYPE(phd_layout_sink_t);
    printf("PHD_SHA256_HEX_LEN %d\n", PHD_SHA256_HEX_LEN);
    printf("PHD_LAYOUT_HAS_MIN %u\nPHD_LAYOUT_HAS_MAX %u\nPHD_LAYOUT_HAS_DEF %u\n", PHD_LAYOUT_HAS_MIN, PHD_LAYOUT_HAS_MAX,
           PHD_LAYOUT_HAS_DEF);
    return 0;
}
