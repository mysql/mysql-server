/*
 * Copyright (c) 2018 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Example usage:
 *
 * $ echo credential challenge | openssl sha256 -binary | base64 > cred_param
 * $ echo relying party >> cred_param
 * $ echo user name >> cred_param
 * $ dd if=/dev/urandom bs=1 count=32 | base64 >> cred_param
 * $ fido2-cred -M -i cred_param /dev/hidraw5 | fido2-cred -V -o cred
 */

#include <fido.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../openbsd-compat/openbsd-compat.h"
#include "extern.h"

void
usage(void)
{
	fprintf(stderr,
"usage: fido2-cred -M [-bdhqruv] [-c cred_protect] [-i input_file] [-o output_file] device [type]\n"
"       fido2-cred -V [-dhv] [-c cred_protect] [-i input_file] [-o output_file] [type]\n"
	);

	exit(1);
}

int
main(int argc, char **argv)
{
	if (argc < 2 || strlen(argv[1]) != 2 || argv[1][0] != '-')
		usage();

	switch (argv[1][1]) {
	case 'M':
		return (cred_make(--argc, ++argv));
	case 'V':
		return (cred_verify(--argc, ++argv));
	}

	usage();

	/* NOTREACHED */
}
