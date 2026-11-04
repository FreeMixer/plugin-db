/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com> */
/* A program that knows of plugin-db only what pkg-config says: prints the digest of its one argument, a file, and of
 * one layout of each format, so install-check.sh can compare them with an implementation of its own. */

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include <plugin-db/pin.h>

int main(int argc, char **argv)
{
    static const char *const props[] = { "http://lv2plug.in/ns/lv2core#integer" };
    const phd_layout_clap_param_t param = { 1, "Gain", 0.0, 1.0, 0.5, 0x1 };
    const phd_layout_lv2_port_t port = { 0, "gain", 0, PHD_LAYOUT_HAS_MIN | PHD_LAYOUT_HAS_MAX, 0.0, 1.0, 0.0, props, 1 };
    char hex[PHD_SHA256_HEX_LEN + 1];
    int fd = argc > 1 ? open(argv[1], O_RDONLY) : -1;

    if (fd < 0 || phd_pin_fd_digest(fd, hex) != 0 || !phd_pin_hex_valid(hex))
        return 1;
    printf("%s\n", hex);
    close(fd);
    if (phd_layout_clap_digest(&param, 1, hex) != 0)
        return 1;
    printf("%s\n", hex);
    if (phd_layout_lv2_digest(&port, 1, hex) != 0)
        return 1;
    printf("%s\n", hex);
    return 0;
}
