/*
 * This file is released under the terms of the Artistic License.  Please see
 * the file LICENSE, included in this package, for details.
 *
 * Copyright (C) 2005 Mark Wong & Open Source Development Labs, Inc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int main(int argc, char *argv[])
{
	int i;
	double power = 1;
	double exp = 1.0/24.0;

	if (argc < 3) {
		printf("usage: %s <scale factor> <q1> .. <q22> <rf1> <rf2>\n", argv[0]);
		return 1;
	}
	for (i = 2; i < argc; i++) {
		power *= atof(argv[i]);
	}
	printf("%0.2f", 3600 * atoi(argv[1]) / pow(power, exp));
	return 0;
}
