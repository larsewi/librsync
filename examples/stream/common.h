/*= -*- c-basic-offset: 4; indent-tabs-mode: nil; -*-
 *
 * librsync -- the library for network deltas
 *
 * Copyright (C) 2024 by Lars Erik Wik <lars.erik.wik@northern.tech>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/** \file common.h
 * Simple network protocol on top of TCP. Used for client-server communication
 * in stream example code.
 *
 * Header format:
 *   +----------+----------|
 *   | SDU Len. | EoF Flag |
 *   +----------+----------+
 *   | 15 bits  | 1 bit    |
 *   +----------+----------|
 *
 * The header fields are defined as follows:
 * SDU Length           Length of the SDU (i.e. payload) encapsulated within
 *                      this datagram.
 * End-of-File Flag     Determines whether or not the receiver should expect
 *                      to receive more datagrams.
 * */
#ifndef COMMON_H
#define COMMON_H

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <arpa/inet.h>
#include <librsync.h>

/** The port the server will be listening on */
#define PORT 5612

/** The largest buffer accepted the network protocol (i.e., 32767 Bytes) */
#define BUFFER_SIZE (UINT16_MAX >> 1)


static int send_message(int sock, const char *msg, size_t len, int eof) {
    /* Make sure we have one bit to signal End-of-File */
    assert(len <= (UINT16_MAX >> 1));

    /* Make space for flags */
    uint16_t header = len << 1;

    /* Set EOF flag */
    if (eof != 0) {
        header |= 1;
    }

    /* Send header */
    header = htons(header);
    ssize_t ret = write(sock, &header, sizeof(header));
    if (ret != sizeof(header)) {
        perror("Failed to send message header");
        return -1;
    }

    if (len > 0) {
        /* Send payload */
        ret = write(sock, msg, len);
        if (ret != len) {
            perror("Failed to send message payload");
            return -1;
        }
    }

    return 0;
}

static int recv_message(int sock, char *msg, size_t *len, int *eof) {
    /* Receive header */
    uint16_t header;
    ssize_t ret = read(sock, &header, sizeof(header));
    if (ret != sizeof(header)) {
        perror("Failed to receive message header");
        return -1;
    }
    header = ntohs(header);

    /* Extract EOF flag */
    *eof = header & 1;

    /* Extract message length */
    *len = header >> 1;

    if (*len > 0) {
        /* Read payload */
        ret = read(sock, msg, *len);
        if (ret != *len) {
            perror("Failed to receive message payload");
            return -1;
        }
    }

    return 0;
}

#endif                          /* !COMMON_H */
