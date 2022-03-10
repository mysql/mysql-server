/*
 * Copyright (c) 2018 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#include <fido.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../openbsd-compat/openbsd-compat.h"
#include "extern.h"

static int action;

void
usage(void)
{
	fprintf(stderr,
"usage: fido2-token [-CR] [-d] device\n"
"       fido2-token -D [-de] -i id device\n"
"       fido2-token -I [-cd] [-k rp_id -i cred_id] device\n"
"       fido2-token -L [-der] [-k rp_id] [device]\n"
"       fido2-token -S [-de] [-i template_id -n template_name] device\n"
"       fido2-token -V\n"
	);

	exit(1);
}

static void
setaction(int ch)
{
	if (action)
		usage();
	action = ch;
}

int
main(int argc, char **argv)
{
	int ch;
	int flags = 0;
	char *device;

	while ((ch = getopt(argc, argv, TOKEN_OPT)) != -1) {
		switch (ch) {
		case 'b':
		case 'c':
		case 'e':
		case 'i':
		case 'k':
		case 'n':
		case 'r':
			break; /* ignore */
		case 'd':
			flags = FIDO_DEBUG;
			break;
		default:
			setaction(ch);
			break;
		}
	}

	if (argc - optind == 1)
		device = argv[optind];
	else
		device = NULL;

	fido_init(flags);

	switch (action) {
	case 'C':
		return (pin_change(device));
	case 'D':
		return (token_delete(argc, argv, device));
	case 'I':
		return (token_info(argc, argv, device));
	case 'L':
		return (token_list(argc, argv, device));
	case 'R':
		return (token_reset(device));
	case 'S':
		return (token_set(argc, argv, device));
	case 'V':
		fprintf(stderr, "%d.%d.%d\n", _FIDO_MAJOR, _FIDO_MINOR,
		    _FIDO_PATCH);
		exit(0);
	}

	usage();

	/* NOTREACHED */
}
