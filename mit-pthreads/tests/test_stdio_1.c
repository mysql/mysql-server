#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

char * base_name = "test_stdio_1.c";
char * dir_name = SRCDIR;
char * fullname;

#define OK 		0
#define NOTOK  -1

/* Test fopen()/ftell()/getc() */
int test_1(void)
{
	struct stat statbuf;
    FILE * fp;
	int i;

	if (stat(fullname, &statbuf) < OK) {
		printf("ERROR: Couldn't stat %s\n", fullname);
		return(NOTOK);
	}

	if ((fp = fopen(fullname, "r")) == NULL) {
		printf("ERROR: Couldn't open %s\n", fullname);
		return(NOTOK);
	}

	/* Get the entire file */
	while ((i = getc(fp)) != EOF);

	if (ftell(fp) != statbuf.st_size) {
		printf("ERROR: ftell() and stat() don't agree.");
		return(NOTOK);
	}

	if (fclose(fp) < OK) {
		printf("ERROR: fclose() failed.");
		return(NOTOK);
	}
	return(OK);
}

/* Test fopen()/fclose() */
int test_2(void)
{
	FILE *fp1, *fp2;

	if ((fp1 = fopen(fullname, "r")) == NULL) {
		printf("ERROR: Couldn't fopen %s\n", fullname);
		return(NOTOK);
	}

	if (fclose(fp1) < OK) {
		printf("ERROR: fclose() failed.");
		return(NOTOK);
	}

	if ((fp2 = fopen(fullname, "r")) == NULL) {
		printf("ERROR: Couldn't fopen %s\n", fullname);
		return(NOTOK);
	}

	if (fclose(fp2) < OK) {
		printf("ERROR: fclose() failed.");
		return(NOTOK);
	}

	if (fp1 != fp2) {
		printf("ERROR: FILE table leak.\n");
		return(NOTOK);
	}

	return(OK);
}

/* Test sscanf()/sprintf() */
int test_3(void)
{
    char * str = "10 4.53";
	char buf[64];
    double d;
    int    i;

    if (sscanf(str, "%d %lf", &i, &d) != 2) {
		printf("ERROR: sscanf didn't parse input string correctly\n");
		return(NOTOK);
	}

	/* Should have a check */
	sprintf(buf, "%d %2.2lf", i, d);

	if (strcmp(buf, str)) {
		printf("ERROR: sscanf()/sprintf() didn't parse unparse correctly\n");
		return(NOTOK);
	}
	return(OK);
}

main()
{

	printf("test_stdio_1 START\n");

	if (fullname = malloc (strlen (dir_name) + strlen (base_name) + 2)) {
		sprintf (fullname, "%s/%s", dir_name, base_name);
	} else {
		perror ("malloc");
		exit(1);
	}

	if (test_1() || test_2() || test_3()) {
		printf("test_stdio_1 FAILED\n");
		exit(1);
	}

	printf("test_stdio_1 PASSED\n");
	exit(0);
}


