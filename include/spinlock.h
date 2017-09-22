/*-
 * Copyright (c) 2017 Lawrence Esswood
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
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

#ifndef CHERIOS_SPINLOCK_H
#define CHERIOS_SPINLOCK_H

/* TODO align this nicely */
#define CACHE_LINE_SIZE 64;

typedef struct spinlock_t{
    volatile char lock;
} spinlock_t;


static inline void spinlock_init(spinlock_t* lock) {
    lock->lock = 0;
}
static inline void spinlock_acquire(spinlock_t* lock) {
    __asm__ volatile (
    "start:"
            "cllb   $t0, %[lock]\n"
            "check:"
            "bnez   $t0, start\n"
            "li     $t0, 1\n"
            "cscb   $t0, $t0, %[lock]\n"
            "beqz   $t0, check\n"
            "cllb   $t0, %[lock]\n"
    :
    : [lock]"C"(lock)
    : "t0"
    );
}

static inline int spinlock_try_acquire(spinlock_t* lock) {
    int result;
    __asm__ volatile (
    "start:"
            "li     %[result], 0\n"
            "cllb   $t0, %[lock]\n"
            "check: "
            "bnez   $t0, 1f\n"
            "li     $t0, 1\n"
            "cscb   %[result], $t0, %[lock]\n"
            "beqz   %[result], check\n"
            "cllb   $t0, %[lock]\n"
            "1:"
    : [result]"=r"(result)
    : [lock]"C"(lock)
    : "t0"
    );

    return result;
}

static inline void spinlock_release(spinlock_t* lock) {
    lock->lock = 0;
}

#endif //CHERIOS_SPINLOCK_H