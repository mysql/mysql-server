#!/bin/sh

# Copyright (c) 2020 Yubico AB. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

DEV="$(fido2-token -L | sed 's/^\(.*\): .*$/\1/;q')"

while [ -n "${DEV}" ]; do
	sleep .5
	DEV="$(fido2-token -L | sed 's/^\(.*\): .*$/\1/;q')"
done
