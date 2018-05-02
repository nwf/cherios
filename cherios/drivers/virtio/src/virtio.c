/*-
 * Copyright (c) 2018 Lawrence Esswood
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

#include "virtio.h"
#include "cheric.h"
#include "mman.h"

#define PADDR_LO(X) ((uint32_t)(X))
#define PADDR_HI(X) (uint32_t)((((uint64_t)(X) >> 32)) & 0xFFFFFFFF)

#define TOUCH(X) *((volatile char*)(X)) = *((volatile char*)(X))

int virtio_device_init(virtio_mmio_map* map,
                       enum virtio_devices device, u32 version, u32 vendor_id, u32 driver_features,
                       struct virtq* queue) {
    /* INIT1: reset device */
    map->status = 0;

    // Check device is what we expect
    if(map->magic_value != 0x74726976) return DRIVER_BAD_DEVICE;
    if(map->version != version) return DRIVER_BAD_DEVICE;
    if(map->device_id != (u32)device) return DRIVER_BAD_DEVICE;
    if(map->vendor_id != vendor_id) return DRIVER_BAD_DEVICE;

    /* INIT2: set ACKNOWLEDGE status bit */
    /* INIT3: set DRIVER status bit */

    map->status |= STATUS_ACKNOWLEDGE | STATUS_DRIVER;

    /* INIT4: select features */
    map->host_features_sel = 0;
    u32 device_features = map->host_features;
    map->guest_features_sel = 0;
    map->guest_features = device_features & driver_features;

    if((device_features & driver_features) != driver_features) return DRIVER_BAD_FEATURES;

    /* INIT5 INIT6: legacy device, skipped */

    /* INIT7: set virtqueues */

    if(queue->desc == NULL) return DRIVER_QUEUE_MISSING_FIELDS;
    if(queue->avail == NULL) return DRIVER_QUEUE_MISSING_FIELDS;
    if(queue->used == NULL) return DRIVER_QUEUE_MISSING_FIELDS;

    queue->avail->flags = 0;
    queue->avail->idx = 0;
    queue->used->idx = 0;
    queue->last_used_idx = 0;


    map->queue_sel = 0;
    if(queue->num > map->queue_num_max) return DRIVER_QUEUE_TOO_LONG;
    map->queue_num = queue->num;

#define P_FOR(X) 								\
        TOUCH(queue-> X); 						\
		size_t X ## _sz = X ## _size(queue);	\
		uint64_t X ## _phy = mem_paddr_for_vaddr((size_t)queue-> X); 							\
		uint64_t X ## _phy_end = mem_paddr_for_vaddr(((size_t)queue-> X) + X ## _sz -1); 		\
		if(X ## _phy + X ## _sz - 1 != X ## _phy_end) return DRIVER_QUEUE_CROSSES_PAGE_BOUNDRY;	\
		map->queue_ ## X ## _low = PADDR_LO(X ## _phy);												\
		map->queue_ ## X ## _high = PADDR_HI(X ## _phy);

    P_FOR(desc);
    P_FOR(avail);
    P_FOR(used);

    map->queue_ready = 1;

    map->status |= STATUS_DRIVER_OK;

    if(map->status & STATUS_DEVICE_NEEDS_RESET) return DRIVER_DEVICE_NEEDS_RESET;

    return 0;
}

void virtio_device_ack_used(virtio_mmio_map* map) {
    if(map->interrupt_status == 1) {
        map->interrupt_ack = 1;
    }
}


void virtio_device_notify(virtio_mmio_map* map) {
    HW_SYNC;
    map->queue_notify = 0;
}

void virtio_q_add_descs(struct virtq* queue, le16 head) {
    le16 ndx = queue->avail->idx;
    queue->avail->ring[ndx % queue->num] = head;
    HW_SYNC;
    queue->avail->idx = ndx+1;
    return;
}

void virtio_q_init_free(struct virtq* queue, le16* free_head, le16 start) {
    for(le16 i = start; i != queue->num; i++) {
        queue->desc[i].next = i+1;
    }
    *free_head = start;
}

le16 virtio_q_alloc(struct virtq* queue, le16* free_head) {
    le16 head = *free_head;
    if(head != queue->num) {
        *free_head = queue->desc[head].next;
    }
    return head;
}

void virtio_q_free(struct virtq* queue, le16* free_head, le16 head, le16 tail) {
    queue->desc[tail].next = *free_head;
    *free_head = head;
}

int virito_q_chain_add(struct virtq* queue, le16* free_head, le16* tail, le64 addr, le16 length, le16 flags) {
    le16 new = virtio_q_alloc(queue, free_head);
    if(new == queue->num) return -1;
    queue->desc[*tail].next = new;

    *tail = new;
    struct virtq_desc* desc = queue->desc + new;
    desc->len = length;
    desc->addr = addr;
    desc->flags = flags;

    return 0;
}
