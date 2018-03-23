/*-
 * Copyright (c) 2017 Lawrence Esswood
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "cheric.h"
#include "sockets.h"
#include "thread.h"
#include "assert.h"
#include "stdio.h"
#include "string.h"

#define DATA_SIZE 0x800
#define INDIR_SIZE 16
#define PORT 777

#define BIG_TEST_SIZE (1 << 12)

struct stack_request {
    uni_dir_socket_requester r;
    request_t pad[INDIR_SIZE];
};

const char* str1 = "Hello World!\n";
const int size1 = 14;
const char* str2 = "Some";
const int size2 = 4;
const char* str3 = " more bytes in parts\n";
const int size3 = 22;

const char* str4 = "Some more bytes in parts\n";

void connector_start(register_t arg, capability carg) {
    char data_buffer[DATA_SIZE];

    unix_like_socket socket;

    unix_like_socket* sock = &socket;

    int res;

    res = socket_internal_fulfiller_init(&sock->write.pull_writer, SOCK_TYPE_PULL);
    assert_int_ex(res, ==, 0);
    res = socket_internal_fulfiller_init(&sock->read.push_reader, SOCK_TYPE_PUSH);
    assert_int_ex(res, ==, 0);
    res = socket_init(sock, MSG_NONE, data_buffer, DATA_SIZE, CONNECT_PUSH_READ | CONNECT_PULL_WRITE);
    assert_int_ex(res, ==, 0);


    int result = socket_internal_connect(carg, PORT, NULL, &sock->read.push_reader);
    assert_int_ex(result, ==, 0);
    result = socket_internal_connect(carg, PORT+1, NULL, &sock->write.pull_writer);
    assert_int_ex(result, ==, 0);

    char buf[100];

    ssize_t rec = socket_recv(sock, buf, size1, MSG_NONE);
    assert_int_ex(rec, ==, size1);

    // Test a basic send
    assert(strcmp(buf, str1) == 0);

    // Test sending a message in parts

    ssize_t sent = socket_send(sock, str2, size2, MSG_NONE);

    assert(sent == size2);
    sent = socket_send(sock, str3, size3, MSG_NONE);

    assert(sent == size3);

    // Test sending a large amount of data in small parts
    for(size_t i = 0; i != BIG_TEST_SIZE; i++) {
        rec = socket_recv(sock, buf, 3, MSG_NONE);
        assert(rec == 3);
        assert(buf[0] == (char)(i & 0xFF));
        assert(buf[1] == (char)((i>>8) & 0xFF));
        assert(buf[2] == (char)((i>>16) & 0xFF));
    }

    // Test sending a large amount of data in large parts

    char big_buf[DATA_SIZE/2];
    // Test sending a large number of bytes in large chunks

    for(size_t i = 0; i != 3; i++) {
        sent = socket_recv(sock, big_buf, DATA_SIZE/2, MSG_NONE);
        assert(sent == DATA_SIZE/2);
        for(size_t j = 0; j < DATA_SIZE/2; j++) {
            assert(big_buf[j] == (char)j);
        }
    }

    // Test copying capabilities

    capability cap_rec;

    rec = socket_recv(sock, &cap_rec, sizeof(capability), MSG_NONE);
    assert(rec = sizeof(capability));
    assert(cheri_gettag(cap_rec));
    rec = socket_recv(sock, buf, 1, MSG_NONE);
    assert(rec == 1);
    rec = socket_recv(sock, &cap_rec, sizeof(capability), MSG_NONE);
    assert(rec = sizeof(capability));
    assert(cheri_gettag(cap_rec));
    rec = socket_recv(sock, &cap_rec, sizeof(capability), MSG_NONE);
    assert(rec = sizeof(capability));
    assert(!cheri_gettag(cap_rec));
    rec = socket_recv(sock, &cap_rec, sizeof(capability), MSG_NO_CAPS);
    assert(rec = sizeof(capability));
    assert(!cheri_gettag(cap_rec));

    // Test the closing mechanic

    res = socket_internal_close_fulfiller(&sock->write.pull_writer);
    assert_int_ex(res, ==, 0);
    res = socket_internal_close_fulfiller(&sock->read.push_reader);
    assert_int_ex(res, ==, 0);

    printf("Socket test part2 finished\n");
}

int main(register_t arg, capability carg) {

    printf("Socket test Hello World!\n");

    thread t = thread_new("socket_part2", 0, act_self_ref, &connector_start);

    char data_buffer[DATA_SIZE];
    struct stack_request on_stack1;
    struct stack_request on_stack2;
    unix_like_socket socket;

    unix_like_socket* sock = &socket;
    sock->write.push_writer = &on_stack1.r;
    sock->read.pull_reader = &on_stack2.r;

    int res = socket_internal_requester_init(sock->write.push_writer, INDIR_SIZE, SOCK_TYPE_PUSH, &sock->write_copy_buffer);
    assert_int_ex(res, ==, 0);
    socket_internal_requester_init(sock->read.pull_reader, INDIR_SIZE, SOCK_TYPE_PULL, NULL);
    assert_int_ex(res, ==, 0);
    socket_init(sock, MSG_NONE, data_buffer, DATA_SIZE, CONNECT_PUSH_WRITE | CONNECT_PULL_READ);
    assert_int_ex(res, ==, 0);

    int result = socket_internal_listen(PORT, sock->write.push_writer,NULL);
    assert_int_ex(res, ==, 0);
    result = socket_internal_listen(PORT+1, sock->read.pull_reader,NULL);
    assert_int_ex(res, ==, 0);

    ssize_t sent = socket_send(sock, str1, size1, MSG_NONE);
    assert(sent == size1);

    char buf[100];

    size_t p1 = 13;
    size_t p2 = size2 + size3 - p1;

    ssize_t rec = socket_recv(sock, buf, p1, MSG_NO_COPY);
    assert_int_ex(rec, ==, p1);
    rec = socket_recv(sock, buf + p1, p2, MSG_NO_COPY);
    assert_int_ex(rec, ==, p2);

    // Test multiple sends with partial reads
    assert(strcmp(buf, str4) == 0);

    // Test sending a large number bytes in small chunks

    for(size_t i = 0; i != BIG_TEST_SIZE; i++) {
        buf[0] = (char)(i & 0xFF);
        buf[1] = (char)((i >> 8) & 0xFF);
        buf[2] = (char)((i >> 16) & 0xFF);
        sent = socket_send(sock, buf, 3, MSG_NONE);
        assert(sent == 3);
    }

    char big_buf[DATA_SIZE/2];
    // Test sending a large number of bytes in large chunks
    for(size_t i = 0; i < DATA_SIZE/2; i++) {
        big_buf[i] = (char)i;
    }

    for(size_t i = 0; i != 3; i++) {
        sent = socket_send(sock, big_buf, DATA_SIZE/2, MSG_NONE);
        assert(sent == DATA_SIZE/2);
    }

    // Test copying capabilities

    capability cap = act_self_ref;
    sent = socket_send(sock, &cap, sizeof(capability), MSG_NONE);
    assert(sent == sizeof(capability));
    sent = socket_send(sock, buf, 1, MSG_NONE);
    assert(sent == 1);
    sent = socket_send(sock, &cap, sizeof(capability), MSG_NONE);
    assert(sent == sizeof(capability));
    sent = socket_send(sock, &cap, sizeof(capability), MSG_NO_CAPS);
    assert(sent == sizeof(capability));
    sent = socket_send(sock, &cap, sizeof(capability), MSG_NONE);
    assert(sent == sizeof(capability));

    // Test the closing mechanic

    rec = socket_recv(sock, buf, 1, MSG_NO_COPY);
    assert_int_ex(rec, ==, E_SOCKET_CLOSED);
    rec = socket_send(sock, buf, 1, MSG_NONE);
    assert_int_ex(rec, ==, E_SOCKET_CLOSED);

    printf("Socket test finished\n");
}
