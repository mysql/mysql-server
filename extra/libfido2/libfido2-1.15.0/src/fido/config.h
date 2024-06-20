/*
 * Copyright (c) 2020 Yubico AB. All rights reserved.
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

#ifndef _FIDO_CONFIG_H
#define _FIDO_CONFIG_H

#ifdef _FIDO_INTERNAL
#include "blob.h"
#include "fido/err.h"
#include "fido/param.h"
#include "fido/types.h"
#else
#include <fido.h>
#include <fido/err.h>
#include <fido/param.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

int fido_dev_enable_entattest(fido_dev_t *, const char *);
int fido_dev_force_pin_change(fido_dev_t *, const char *);
int fido_dev_toggle_always_uv(fido_dev_t *, const char *);
int fido_dev_set_pin_minlen(fido_dev_t *, size_t, const char *);
int fido_dev_set_pin_minlen_rpid(fido_dev_t *, const char * const *, size_t,
    const char *);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* !_FIDO_CONFIG_H */
