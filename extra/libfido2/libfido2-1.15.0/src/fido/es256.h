/*
 * Copyright (c) 2018-2021 Yubico AB. All rights reserved.
 * SPDX-License-Identifier: BSD-2-Clause
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _FIDO_ES256_H
#define _FIDO_ES256_H

#include <openssl/ec.h>

#include <stdint.h>
#include <stdlib.h>

#ifdef _FIDO_INTERNAL
#include "types.h"
#else
#include <fido.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

es256_pk_t *es256_pk_new(void);
void es256_pk_free(es256_pk_t **);
EVP_PKEY *es256_pk_to_EVP_PKEY(const es256_pk_t *);

int es256_pk_from_EC_KEY(es256_pk_t *, const EC_KEY *);
int es256_pk_from_EVP_PKEY(es256_pk_t *, const EVP_PKEY *);
int es256_pk_from_ptr(es256_pk_t *, const void *, size_t);

#ifdef _FIDO_INTERNAL
es256_sk_t *es256_sk_new(void);
void es256_sk_free(es256_sk_t **);
EVP_PKEY *es256_sk_to_EVP_PKEY(const es256_sk_t *);

int es256_derive_pk(const es256_sk_t *, es256_pk_t *);
int es256_sk_create(es256_sk_t *);

int es256_pk_set_x(es256_pk_t *, const unsigned char *);
int es256_pk_set_y(es256_pk_t *, const unsigned char *);
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* !_FIDO_ES256_H */
