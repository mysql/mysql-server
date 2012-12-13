#include <my_global.h>
#include <m_string.h>
#include <sys/types.h>
#include <assert.h>

#include "my_regex.h"
#include "main.ih"
#include "tests_include.h"

char *progname;
int debug = 0;
int line = 0;
int status = 0;

int copts = MY_REG_EXTENDED;
int eopts = 0;
my_regoff_t startoff = 0;
my_regoff_t endoff = 0;


extern int split(char *string, char *fields[], int nfields, char *sep);
extern void regprint(my_regex_t *r, FILE *d);


#ifdef __WIN__
char *optarg= "";
int optind= 1;
int opterr;

/* A (very) simplified version of getopt, enough to run the registered tests. */
int getopt(int argc, char *argv[], const char *optstring)
{
  char *opt= NULL;
  int retval= -1;
  if (optind >= argc)
    return retval;

  opt= argv[optind];
  if (*opt != '-')
    return retval;

  retval= *(opt+1);
  if (*(opt+1) && *(opt+2))
    optarg= opt + 2;
  else
    optarg= "";

  ++optind;
  return retval;
}
#endif


/*
 - main - do the simple case, hand off to regress() for regression
 */
int main(argc, argv)
int argc;
char *argv[];
{
	my_regex_t re;
#	define	NS	10
	my_regmatch_t subs[NS];
	char erbuf[100];
	int err;
	size_t len;
	int c;
	int errflg = 0;
	int opt_inline = 0;
	int i;
        char *input_file_name= NULL;
	extern int optind;
	extern char *optarg;

	progname = argv[0];

	while ((c = getopt(argc, argv, "c:e:i:S:E:xI")) != EOF)
		switch (c) {
		case 'c':	/* compile options */
			copts = options('c', optarg);
			break;
		case 'e':	/* execute options */
			eopts = options('e', optarg);
			break;
                case 'i':
                        input_file_name= optarg;
			break;
		case 'S':	/* start offset */
			startoff = (my_regoff_t)atoi(optarg);
			break;
		case 'E':	/* end offset */
			endoff = (my_regoff_t)atoi(optarg);
			break;
		case 'x':	/* Debugging. */
			debug++;
			break;
		case 'I':	/* Inline. */
			opt_inline= 1;
			break;
		case '?':
		default:
			errflg++;
			break;
		}
	if (errflg) {
		fprintf(stderr, "usage: %s ", progname);
		fprintf(stderr,
                        "[-c copt][-e eopt][-i filename][-S][-E][-x][-I] [re]\n");
		exit(2);
	}

        if (opt_inline) {
          regress(NULL);
          exit(status);
        }

	if (optind >= argc && !input_file_name) {
		regress(stdin);
		exit(status);
	}

        if (input_file_name) {
          FILE *input_file= fopen(input_file_name, "r");
          if (!input_file) {
            fprintf(stderr, "Could not open '%s' : ", input_file_name);
            perror(NULL);
            exit(EXIT_FAILURE);
          }
          regress(input_file);
          fclose(input_file);
          exit(status);
        }

	err = my_regcomp(&re, argv[optind++], copts, &my_charset_latin1);
	if (err) {
		len = my_regerror(err, &re, erbuf, sizeof(erbuf));
		fprintf(stderr, "error %s, %d/%d `%s'\n",
			eprint(err), (int) len, (int) sizeof(erbuf), erbuf);
		exit(status);
	}
	regprint(&re, stdout);

	if (optind >= argc) {
		my_regfree(&re);
		exit(status);
	}

	if (eopts&MY_REG_STARTEND) {
		subs[0].rm_so = startoff;
		subs[0].rm_eo = strlen(argv[optind]) - endoff;
	}
	err = my_regexec(&re, argv[optind], (size_t)NS, subs, eopts);
	if (err) {
		len = my_regerror(err, &re, erbuf, sizeof(erbuf));
		fprintf(stderr, "error %s, %d/%d `%s'\n",
			eprint(err), (int) len, (int) sizeof(erbuf), erbuf);
		exit(status);
	}
	if (!(copts&MY_REG_NOSUB)) {
		len = (int)(subs[0].rm_eo - subs[0].rm_so);
		if (subs[0].rm_so != -1) {
			if (len != 0)
				printf("match `%.*s'\n", (int)len,
					argv[optind] + subs[0].rm_so);
			else
				printf("match `'@%.1s\n",
					argv[optind] + subs[0].rm_so);
		}
		for (i = 1; i < NS; i++)
			if (subs[i].rm_so != -1)
				printf("(%d) `%.*s'\n", i,
					(int)(subs[i].rm_eo - subs[i].rm_so),
					argv[optind] + subs[i].rm_so);
	}
	exit(status);
}

char*
get_next_line(s, size, stream)
char *s;
int size;
FILE *stream;
{
  if (stream)
    return fgets(s, size, stream);
  if (test_array[line])
    return strncpy(s, test_array[line], size);
  return NULL;
}

/*
 - regress - main loop of regression test
 == void regress(FILE *in);
    Reads file, line-by-line.
    If in == NULL, we read data from test_array instead.
 */
void
regress(in)
FILE *in;
{
	char inbuf[1000];
#	define	MAXF	10
	char *f[MAXF];
	int nf;
	int i;
	char erbuf[100];
	size_t ne;
	const char *badpat = "invalid regular expression";
#	define	SHORT	10
	const char *bpname = "MY_REG_BADPAT";
	my_regex_t re;

	while (get_next_line(inbuf, sizeof(inbuf), in) != NULL) {
		line++;
		if (inbuf[0] == '#' || inbuf[0] == '\n' || inbuf[0] == '\0')
			continue;			/* NOTE CONTINUE */
		if (inbuf[strlen(inbuf)-1] == '\n')
		  inbuf[strlen(inbuf)-1] = '\0';  /* get rid of stupid \n */
		if (debug)
                  fprintf(stdout, "%d: <%s>\n", line, inbuf);
		nf = split(inbuf, f, MAXF, (char*) "\t\t");
		if (nf < 3) {
			fprintf(stderr, "bad input, line %d\n", line);
			exit(1);
		}
		for (i = 0; i < nf; i++)
			if (strcmp(f[i], "\"\"") == 0)
				f[i] = (char*) "";
		if (nf <= 3)
			f[3] = NULL;
		if (nf <= 4)
			f[4] = NULL;
		rx_try(f[0], f[1], f[2], f[3], f[4], options('c', f[1]));
		if (opt('&', f[1]))	/* try with either type of RE */
			rx_try(f[0], f[1], f[2], f[3], f[4],
					options('c', f[1]) &~ MY_REG_EXTENDED);
	}

	ne = my_regerror(MY_REG_BADPAT, (my_regex_t *)NULL, erbuf, sizeof(erbuf));
	if (strcmp(erbuf, badpat) != 0 || ne != strlen(badpat)+1) {
		fprintf(stderr, "end: regerror() test gave `%s' not `%s'\n",
							erbuf, badpat);
		status = 1;
	}
	ne = my_regerror(MY_REG_BADPAT, (my_regex_t *)NULL, erbuf, (size_t)SHORT);
	if (strncmp(erbuf, badpat, SHORT-1) != 0 || erbuf[SHORT-1] != '\0' ||
						ne != strlen(badpat)+1) {
		fprintf(stderr, "end: regerror() short test gave `%s' not `%.*s'\n",
						erbuf, SHORT-1, badpat);
		status = 1;
	}
	ne = my_regerror(MY_REG_ITOA|MY_REG_BADPAT, (my_regex_t *)NULL, erbuf, sizeof(erbuf));
	if (strcmp(erbuf, bpname) != 0 || ne != strlen(bpname)+1) {
		fprintf(stderr, "end: regerror() ITOA test gave `%s' not `%s'\n",
						erbuf, bpname);
		status = 1;
	}
	re.re_endp = bpname;
	ne = my_regerror(MY_REG_ATOI, &re, erbuf, sizeof(erbuf));
	if (atoi(erbuf) != (int)MY_REG_BADPAT) {
		fprintf(stderr, "end: regerror() ATOI test gave `%s' not `%ld'\n",
						erbuf, (long)MY_REG_BADPAT);
		status = 1;
	} else if (ne != strlen(erbuf)+1) {
		fprintf(stderr, "end: regerror() ATOI test len(`%s') = %ld\n",
						erbuf, (long)MY_REG_BADPAT);
		status = 1;
	}
}

/*
 - rx_try - try it, and report on problems
 == void rx_try(char *f0, char *f1, char *f2, char *f3, char *f4, int opts);
 */
void
rx_try(f0, f1, f2, f3, f4, opts)
char *f0;
char *f1;
char *f2;
char *f3;
char *f4;
int opts;			/* may not match f1 */
{
	my_regex_t re;
#	define	NSUBS	10
	my_regmatch_t subs[NSUBS];
#	define	NSHOULD	15
	char *should[NSHOULD];
	int nshould;
	char erbuf[100];
	int err;
	int len;
	const char *type = (opts & MY_REG_EXTENDED) ? "ERE" : "BRE";
	int i;
	char *grump;
	char f0copy[1000];
	char f2copy[1000];

	strcpy(f0copy, f0);
	re.re_endp = (opts&MY_REG_PEND) ? f0copy + strlen(f0copy) : NULL;
	fixstr(f0copy);
	err = my_regcomp(&re, f0copy, opts, &my_charset_latin1);
	if (err != 0 && (!opt('C', f1) || err != efind(f2))) {
		/* unexpected error or wrong error */
		len = my_regerror(err, &re, erbuf, sizeof(erbuf));
		fprintf(stderr, "%d: %s error %s, %d/%d `%s'\n",
					line, type, eprint(err), len,
					(int) sizeof(erbuf), erbuf);
		status = 1;
	} else if (err == 0 && opt('C', f1)) {
		/* unexpected success */
		fprintf(stderr, "%d: %s should have given MY_REG_%s\n",
						line, type, f2);
		status = 1;
		err = 1;	/* so we won't try regexec */
	}

	if (err != 0) {
		my_regfree(&re);
		return;
	}

	strcpy(f2copy, f2);
	fixstr(f2copy);

	if (options('e', f1)&MY_REG_STARTEND) {
		if (strchr(f2, '(') == NULL || strchr(f2, ')') == NULL)
			fprintf(stderr, "%d: bad STARTEND syntax\n", line);
		subs[0].rm_so = strchr(f2, '(') - f2 + 1;
		subs[0].rm_eo = strchr(f2, ')') - f2;
	}
	err = my_regexec(&re, f2copy, NSUBS, subs, options('e', f1));

	if (err != 0 && (f3 != NULL || err != MY_REG_NOMATCH)) {
		/* unexpected error or wrong error */
		len = my_regerror(err, &re, erbuf, sizeof(erbuf));
		fprintf(stderr, "%d: %s exec error %s, %d/%d `%s'\n",
					line, type, eprint(err), len,
					(int) sizeof(erbuf), erbuf);
		status = 1;
	} else if (err != 0) {
		/* nothing more to check */
	} else if (f3 == NULL) {
		/* unexpected success */
		fprintf(stderr, "%d: %s exec should have failed\n",
						line, type);
		status = 1;
		err = 1;		/* just on principle */
	} else if (opts&MY_REG_NOSUB) {
		/* nothing more to check */
	} else if ((grump = check(f2, subs[0], f3)) != NULL) {
		fprintf(stderr, "%d: %s %s\n", line, type, grump);
		status = 1;
		err = 1;
	}

	if (err != 0 || f4 == NULL) {
		my_regfree(&re);
		return;
	}

	for (i = 1; i < NSHOULD; i++)
		should[i] = NULL;
	nshould = split(f4, should+1, NSHOULD-1, (char*) ",");
	if (nshould == 0) {
		nshould = 1;
		should[1] = (char*) "";
	}
	for (i = 1; i < NSUBS; i++) {
		grump = check(f2, subs[i], should[i]);
		if (grump != NULL) {
			fprintf(stderr, "%d: %s $%d %s\n", line,
							type, i, grump);
			status = 1;
			err = 1;
		}
	}

	my_regfree(&re);
}

/*
 - options - pick options out of a regression-test string
 == int options(int type, char *s);
 */
int
options(type, s)
int type;			/* 'c' compile, 'e' exec */
char *s;
{
	char *p;
	int o = (type == 'c') ? copts : eopts;
	const char *legal = (type == 'c') ? "bisnmp" : "^$#tl";

	for (p = s; *p != '\0'; p++)
		if (strchr(legal, *p) != NULL)
			switch (*p) {
			case 'b':
				o &= ~MY_REG_EXTENDED;
				break;
			case 'i':
				o |= MY_REG_ICASE;
				break;
			case 's':
				o |= MY_REG_NOSUB;
				break;
			case 'n':
				o |= MY_REG_NEWLINE;
				break;
			case 'm':
				o &= ~MY_REG_EXTENDED;
				o |= MY_REG_NOSPEC;
				break;
			case 'p':
				o |= MY_REG_PEND;
				break;
			case '^':
				o |= MY_REG_NOTBOL;
				break;
			case '$':
				o |= MY_REG_NOTEOL;
				break;
			case '#':
				o |= MY_REG_STARTEND;
				break;
			case 't':	/* trace */
				o |= MY_REG_TRACE;
				break;
			case 'l':	/* force long representation */
				o |= MY_REG_LARGE;
				break;
			case 'r':	/* force backref use */
				o |= MY_REG_BACKR;
				break;
			}
	return(o);
}

/*
 - opt - is a particular option in a regression string?
 == int opt(int c, char *s);
 */
int				/* predicate */
opt(c, s)
int c;
char *s;
{
	return(strchr(s, c) != NULL);
}

/*
 - fixstr - transform magic characters in strings
 == void fixstr(register char *p);
 */
void
fixstr(p)
char *p;
{
	if (p == NULL)
		return;

	for (; *p != '\0'; p++)
		if (*p == 'N')
			*p = '\n';
		else if (*p == 'T')
			*p = '\t';
		else if (*p == 'S')
			*p = ' ';
		else if (*p == 'Z')
			*p = '\0';
}

/*
 - check - check a substring match
 == char *check(char *str, regmatch_t sub, char *should);
 */
char *				/* NULL or complaint */
check(str, sub, should)
char *str;
my_regmatch_t sub;
char *should;
{
	int len;
	int shlen;
	char *p;
	static char grump[500];
	char *at = NULL;

	if (should != NULL && strcmp(should, "-") == 0)
		should = NULL;
	if (should != NULL && should[0] == '@') {
		at = should + 1;
		should = (char*) "";
	}

	/* check rm_so and rm_eo for consistency */
	if (sub.rm_so > sub.rm_eo || (sub.rm_so == -1 && sub.rm_eo != -1) ||
				(sub.rm_so != -1 && sub.rm_eo == -1) ||
				(sub.rm_so != -1 && sub.rm_so < 0) ||
				(sub.rm_eo != -1 && sub.rm_eo < 0) ) {
		sprintf(grump, "start %ld end %ld", (long)sub.rm_so,
							(long)sub.rm_eo);
		return(grump);
	}

	/* check for no match */
	if (sub.rm_so == -1 && should == NULL)
		return(NULL);
	if (sub.rm_so == -1)
		return((char*) "did not match");

	/* check for in range */
	if ((int) sub.rm_eo > (int) strlen(str)) {
		sprintf(grump, "start %ld end %ld, past end of string",
					(long)sub.rm_so, (long)sub.rm_eo);
		return(grump);
	}

	len = (int)(sub.rm_eo - sub.rm_so);
	shlen = (int)strlen(should);
	p = str + sub.rm_so;

	/* check for not supposed to match */
	if (should == NULL) {
		sprintf(grump, "matched `%.*s'", len, p);
		return(grump);
	}

	/* check for wrong match */
	if (len != shlen || strncmp(p, should, (size_t)shlen) != 0) {
		sprintf(grump, "matched `%.*s' instead", len, p);
		return(grump);
	}
	if (shlen > 0)
		return(NULL);

	/* check null match in right place */
	if (at == NULL)
		return(NULL);
	shlen = strlen(at);
	if (shlen == 0)
		shlen = 1;	/* force check for end-of-string */
	if (strncmp(p, at, shlen) != 0) {
		sprintf(grump, "matched null at `%.20s'", p);
		return(grump);
	}
	return(NULL);
}

/*
 - eprint - convert error number to name
 == static char *eprint(int err);
 */
static char *
eprint(err)
int err;
{
	static char epbuf[100];
	size_t len;

	len = my_regerror(MY_REG_ITOA|err, (my_regex_t *)NULL, epbuf, sizeof(epbuf));
	assert(len <= sizeof(epbuf));
	return(epbuf);
}

/*
 - efind - convert error name to number
 == static int efind(char *name);
 */
static int
efind(name)
char *name;
{
	static char efbuf[100];
	my_regex_t re;

	sprintf(efbuf, "MY_REG_%s", name);
	assert(strlen(efbuf) < sizeof(efbuf));
	re.re_endp = efbuf;
	(void) my_regerror(MY_REG_ATOI, &re, efbuf, sizeof(efbuf));
	return(atoi(efbuf));
}
