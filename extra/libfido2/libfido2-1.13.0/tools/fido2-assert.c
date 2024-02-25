/*
 * Copyright (c) 2018 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Example usage:
 *
 * $ echo assertion challenge | openssl sha256 -binary | base64 > assert_param
 * $ echo relying party >> assert_param
 * $ head -1 cred >> assert_param # credential id
 * $ tail -n +2 cred > pubkey # credential pubkey
 * $ fido2-assert -G -i assert_param /dev/hidraw5 | fido2-assert -V pubkey rs256
 *
 * See blurb in fido2-cred.c on how to obtain cred.
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
"usage: fido2-assert -G [-bdhpruv] [-t option] [-i input_file] [-o output_file] device\n"
"       fido2-assert -V [-dhpv] [-i input_file] key_file [type]\n"
	);

	exit(1);
}

int
main(int argc, char **argv)
{
	if (argc < 2 || strlen(argv[1]) != 2 || argv[1][0] != '-')
		usage();

	switch (argv[1][1]) {
	case 'G':
		return (assert_get(--argc, ++argv));
	case 'V':
		return (assert_verify(--argc, ++argv));
	}

	usage();

	/* NOTREACHED */
}
