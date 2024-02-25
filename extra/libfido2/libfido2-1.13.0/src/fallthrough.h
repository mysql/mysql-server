/*
 * Copyright (c) 2022 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _FALLTHROUGH_H
#define _FALLTHROUGH_H

#if defined(__GNUC__)
#if __has_attribute(fallthrough)
#define FALLTHROUGH	__attribute__((fallthrough));
#endif
#endif /* __GNUC__ */

#ifndef FALLTHROUGH
#define FALLTHROUGH	/* FALLTHROUGH */
#endif

#endif /* !_FALLTHROUGH_H */
