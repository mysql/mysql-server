/*
 * Copyright (c) 2019-2022 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _MUTATOR_AUX_H
#define _MUTATOR_AUX_H

#include <sys/types.h>

#include <stddef.h>
#include <stdint.h>
#include <cbor.h>

#include "../src/fido.h"
#include "../src/fido/bio.h"
#include "../src/fido/config.h"
#include "../src/fido/credman.h"
#include "../src/fido/eddsa.h"
#include "../src/fido/es256.h"
#include "../src/fido/es384.h"
#include "../src/fido/rs256.h"
#include "../src/netlink.h"

/*
 * As of LLVM 10.0.0, MSAN support in libFuzzer was still experimental.
 * We therefore have to be careful when using our custom mutator, or
 * MSAN will flag uninitialised reads on memory populated by libFuzzer.
 * Since there is no way to suppress MSAN without regenerating object
 * code (in which case you might as well rebuild libFuzzer with MSAN),
 * we adjust our mutator to make it less accurate while allowing
 * fuzzing to proceed.
 */

#if defined(__has_feature)
# if  __has_feature(memory_sanitizer)
#  include <sanitizer/msan_interface.h>
#  define NO_MSAN	__attribute__((no_sanitize("memory")))
#  define WITH_MSAN	1
# endif
#endif

#if !defined(WITH_MSAN)
# define NO_MSAN
#endif

#define MUTATE_SEED	0x01
#define MUTATE_PARAM	0x02
#define MUTATE_WIREDATA	0x04
#define MUTATE_ALL	(MUTATE_SEED | MUTATE_PARAM | MUTATE_WIREDATA)

#define MAXSTR		1024
#define MAXBLOB		3600
#define MAXCORPUS	8192

#define HID_DEV_HANDLE	0x68696421
#define NFC_DEV_HANDLE	0x6e666321

struct blob {
	uint8_t body[MAXBLOB];
	size_t len;
};

struct param;

struct param *unpack(const uint8_t *, size_t);
size_t pack(uint8_t *, size_t, const struct param *);
size_t pack_dummy(uint8_t *, size_t);
void mutate(struct param *, unsigned int, unsigned int);
void test(const struct param *);

void consume(const void *, size_t);
void consume_str(const char *);

int unpack_blob(cbor_item_t *, struct blob *);
int unpack_byte(cbor_item_t *, uint8_t *);
int unpack_int(cbor_item_t *, int *);
int unpack_string(cbor_item_t *, char *);

cbor_item_t *pack_blob(const struct blob *);
cbor_item_t *pack_byte(uint8_t);
cbor_item_t *pack_int(int);
cbor_item_t *pack_string(const char *);

void mutate_byte(uint8_t *);
void mutate_int(int *);
void mutate_blob(struct blob *);
void mutate_string(char *);

ssize_t fd_read(int, void *, size_t);
ssize_t fd_write(int, const void *, size_t);

int nfc_read(void *, unsigned char *, size_t, int);
int nfc_write(void *, const unsigned char *, size_t);

fido_dev_t *open_dev(int);
void set_wire_data(const uint8_t *, size_t);

void fuzz_clock_reset(void);
void prng_init(unsigned long);
unsigned long prng_uint32(void);

uint32_t uniform_random(uint32_t);

void set_pcsc_parameters(const struct blob *);
void set_pcsc_io_functions(int (*)(void *, u_char *, size_t, int),
    int (*)(void *, const u_char *, size_t), void (*)(const void *, size_t));

#endif /* !_MUTATOR_AUX_H */
