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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <limits.h>
#include <librsync.h>

#include "common.h"

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#define IP_ADDRESS "127.0.0.1"


static char in_buf[BUFFER_SIZE], out_buf[BUFFER_SIZE];

static int connect_to_server(const char *ip_addr);
static int send_signature(int sock, const char *fname);
static int recv_delta_and_patch_file(int sock, const char *fname);

int main(int argc, char *argv[]) {
    /* Parse arguments */
    if (argc < 2) {
        printf("USAGE: %s <FILENAME>", argv[0]);
        return EXIT_FAILURE;
    }
    const char *fname = argv[1];

    puts("Connecting to server...");
    int sock = connect_to_server(IP_ADDRESS);
    if (sock == -1) {
        return EXIT_FAILURE;
    }

    puts("Sending signature...");
    int ret = send_signature(sock, fname);
    if (ret == -1) {
        close(sock);
        return EXIT_FAILURE;
    }

    puts("Receiving delta and patching file...");
    ret = recv_delta_and_patch_file(sock, fname);
    if (ret == -1) {
        close(sock);
        return EXIT_FAILURE;
    }

    puts("Success!");
    return EXIT_SUCCESS;
}

static int connect_to_server(const char *ip_addr) {
    /* Create socket */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Failed to create socket");
        return -1;
    }

    /* Assign IP address and port */
    struct sockaddr_in addr = { 0 };
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip_addr);
    addr.sin_port = htons(PORT);

    /* Connect to server */
    int ret = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    if (ret == -1) {
        perror("Failed to connect");
        close(sock);
        return -1;
    }

    return sock;
}

static int send_signature(int sock, const char *fname) {
    /* Open basis file */
    FILE *file = rs_file_open(fname, "r", 0);
    assert(file != NULL);

    /* Get file size */
    rs_long_t fsize = rs_file_size(file);

    /* Get recommended arguments */
    rs_magic_number sig_magic = 0;
    size_t block_len = 0, strong_len = 0;
    rs_result res = rs_sig_args(fsize, &sig_magic, &block_len, &strong_len);
    if (res != RS_DONE) {
        rs_file_close(file);
        return -1;
    }

    /* Start generating signature */
    rs_job_t *job = rs_sig_begin(block_len, strong_len, sig_magic);
    assert(job != NULL);

    /* Setup buffers */
    rs_buffers_t bufs = { 0 };
    bufs.next_in = in_buf;
    bufs.next_out = out_buf;
    bufs.avail_out = sizeof(out_buf);

    /* Generate signature */
    do {
        if (bufs.eof_in == 0) {
            if (bufs.avail_in > 0) {
                /* Leftover tail data, move it to front */
                memmove(in_buf, bufs.next_in, bufs.avail_in);
            }

            /* Fill input buffer */
            size_t n_bytes = fread(in_buf + bufs.avail_in, 1, sizeof(in_buf) - bufs.avail_in, file);
            if (n_bytes == 0) {
                if (ferror(file)) {
                    perror("Failed to read file");
                    rs_file_close(file);
                    rs_job_free(job);
                    return -1;
                }

                /* End-of-File reached */
                bufs.eof_in = feof(file);
                assert(bufs.eof_in != 0);
            }

            bufs.next_in = in_buf;
            bufs.avail_in += n_bytes;
        }

        /* Iterate job */
        res = rs_job_iter(job, &bufs);
        if (res != RS_DONE && res != RS_BLOCKED) {
            rs_file_close(file);
            rs_job_free(job);
            return -1;
        }

        size_t present = bufs.next_out - out_buf;
        if (present > 0) {
            /* Drain output buffer */
            int ret = send_message(sock, out_buf, present, bufs.eof_in);
            if (ret == -1) {
                rs_file_close(file);
                rs_job_free(job);
                return -1;
            }

            bufs.next_out = out_buf;
            bufs.avail_out = sizeof(out_buf);
        }
    } while (res != RS_DONE);

    rs_file_close(file);
    rs_job_free(job);

    return 0;
}

static int recv_delta_and_patch_file(int sock, const char *fname) {
    char fname_new[PATH_MAX];
    int ret = snprintf(fname_new, sizeof(fname_new), "%s.new", fname);
    if (ret < 0 || (size_t)ret >= sizeof(fname_new)) {
        fputs("Filename too long", stderr);
        return -1;
    }

    FILE *new = rs_file_open(fname_new, "w", 1);
    assert(new != NULL);

    FILE *old = old = rs_file_open(fname, "r", 0);
    assert(old != NULL);

    rs_job_t *job = rs_patch_begin(rs_file_copy_cb, old);
    assert(job != NULL);

    /* Setup RSYNC buffers */
    rs_buffers_t bufs = { 0 };
    bufs.next_in = in_buf;
    bufs.next_out = out_buf;
    bufs.avail_out = sizeof(out_buf);

    rs_result res;
    do {
        if (bufs.eof_in == 0) {
            if (bufs.avail_in > 0) {
                /* Left over tail data, move to front */
                memmove(in_buf, bufs.next_in, bufs.avail_in);
            }

            size_t n_bytes;
            int ret = recv_message(sock, in_buf + bufs.avail_in, &n_bytes, &bufs.eof_in);
            if (ret == -1) {
                rs_file_close(new);
                rs_file_close(old);
                rs_job_free(job);
                return -1;
            }

            bufs.next_in = in_buf;
            bufs.avail_in += n_bytes;
        }

        res = rs_job_iter(job, &bufs);
        if (res != RS_DONE && res != RS_BLOCKED) {
            rs_file_close(new);
            rs_file_close(old);
            rs_job_free(job);
            return -1;
        }

        /* Drain output buffer, if there is data */
        size_t present = bufs.next_out - out_buf;
        if (present > 0) {
            size_t n_bytes = fwrite(out_buf, 1, present, new);
            if (n_bytes == 0) {
                perror("Failed to write to file");
                rs_file_close(new);
                rs_file_close(old);
                rs_job_free(job);
                return -1;
            }

            bufs.next_out = out_buf;
            bufs.avail_out = sizeof(out_buf);
        }
    } while (res != RS_DONE);

    rs_file_close(new);
    rs_file_close(old);
    rs_job_free(job);
    return 0;
}
