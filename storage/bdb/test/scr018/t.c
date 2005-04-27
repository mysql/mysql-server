#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <strings.h>

int
chk(f)
	char *f;
{
	int ch, l, r;

	if (freopen(f, "r", stdin) == NULL) {
		fprintf(stderr, "%s: %s\n", f, strerror(errno));
		exit (1);
	}
	for (l = 1, r = 0; (ch = getchar()) != EOF;) {
		if (ch != ',')
			goto next;
		do { ch = getchar(); } while (isblank(ch));
		if (ch != '\n')
			goto next;
		++l;
		do { ch = getchar(); } while (isblank(ch));
		if (ch != '}')
			goto next;
		r = 1;
		printf("%s: line %d\n", f, l);

next:		if (ch == '\n')
			++l;
	}
	return (r);
}

int
main(int argc, char *argv[])
{
	int r;

	for (r = 0; *++argv != NULL;)
		if (chk(*argv))
			r = 1;
	return (r);
}
