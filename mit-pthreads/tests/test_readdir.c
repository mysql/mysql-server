/* ==== test_readdir.c ========================================================
 * Copyright (c) 1993, 1994 by Chris Provenzano, proven@athena.mit.edu
 *
 * Description : Test pthread_create() and pthread_exit() calls.
 *
 *  1.00 94/05/19 proven
 *      -Started coding this file.
 */

#include <pthread.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>

main()
{
	struct dirent * file;
	DIR * dot_dir;
	int i, found = 0;

	pthread_init(); 

	if (dot_dir = opendir(".")) {
		while (file = readdir(dot_dir)) {
			if (!strcmp("test_readdir", file->d_name)) {
				found = 1;
			}
		}
		closedir(dot_dir);
		if (found) {
			printf("test_readdir PASSED\n");
			exit(0);
		} else {
			printf("Couldn't find file test_readdir ERROR\n");
		}
	} else {
		printf("opendir() ERROR\n");
	}
	printf("test_readdir FAILED\n");
	exit(1);
}

