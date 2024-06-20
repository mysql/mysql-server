#!/bin/sh

# Copyright (c) 2020 Yubico AB. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.
# SPDX-License-Identifier: BSD-2-Clause

DEV=""

while [ -z "${DEV}" ]; do
	sleep .5
	DEV="$(fido2-token -L | sed 's/^\(.*\): .*$/\1/;q')"
done

printf '%s\n' "${DEV}"
