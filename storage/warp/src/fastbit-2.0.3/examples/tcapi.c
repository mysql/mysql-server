/*
 File: $Id$
 Author: John Wu <John.Wu at acm.org>
      Lawrence Berkeley National Laboratory
 Copyright (c) 2006-2016 the Regents of the University of California
*/
/**
   @file tcapi.c

   A simple test program for functions defined in capi.h.

   The basic command line options are
   @code
   datadir selection-conditions [<column type> <column type>...]
   @endcode

   Types recognized are: i (for integers), u (for unsigned integers), l
   (for long integers), f (for floats) and d for (doubles).  Unrecognized
   types are treated as integers.

    @ingroup FastBitExamples
*/
#include <capi.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

void usage(const char *name) {
    fprintf(stdout, "A simple tester for the C API of %s\n\nusage\n"
	    "%s [-c conffile] [-v [verboseness-level]] "
#ifdef TCAPI_USE_LOGFILE
	    " [-l logfile]"
#endif
	    "datadir [conditions] [<column type> ...]\n"
            "In SQL this is equivalent to\n\tFROM datadir "
            "[WHERE conditions [SELECT column type ...]]\n\n"
	    "If only datadir is present, %s indexes all columns in "
	    "the named directory.\n"
	    "If conditions are provided without columns to print, "
	    "%s will print the number of hits.\n"
	    "If any variable is to be printed, it must be specified as "
            "a <name type> pair, where the type must be one of i, u, l, f, d, or s.\n"
#if defined(TCAPI_USE_LOGFILE)
	    "NOTE: the option -l is only available if this program is compiled with TCAPI_USE_LOGFILE\n\n"
#endif
	    "Example:\n"
	    "%s dir 'c1 = 15 and c2 > 23' c1 i c3 u\n\n",
            fastbit_get_version_string(), name, name, name, name);
} /* usage */

/** Create a set of sample data and run some canned queries.

    The sample data contains 100 rows and 3 columns.  The columns are named
    'a', 'b', and 'c'.  They are of types 'int', 'short', and 'float'
    respectively.   The columns a and b have values 0, ..., 99 and column c
    has values 100, 99, ..., 1.
 */
static void builtin(const char *nm, FILE* output) {
    int nerrors = 0;
    int i, mult;
    int msglvl=fastbit_get_verbose_level();
    const char *dir = "tmp";
    int counts[] = {5, 24, 19, 10, 50};
    const char* conditions[] =
	{"a<5", "a+b>150", "a < 60 and c < 60", "c > 90", "c > a"};
    int32_t ivals[100];
    int16_t svals[100];
    float fvals[100];

    /* prepare a sample data */
    for (i = 0; i < 100; ++ i) {
	ivals[i] = i;
	svals[i] = (int16_t) i;
	fvals[i] = (float) (1e2 - i);
    }
    fastbit_add_values("a", "int", ivals, 100, 0);
    fastbit_add_values("b", "short", svals, 100, 0);
    fastbit_add_values("c", "float", fvals, 100, 0);
    fastbit_flush_buffer(dir);
    /* test the queries */
    mult = fastbit_rows_in_partition(dir);
    if (mult % 100 != 0) { /* no an exact multiple */
	fprintf(output, "Directory %s contains %d rows, but expected 100, "
		"remove the directory and try again\n", dir, mult);
	return;
    }

    mult /= 100;
    if (mult > 0) {
	int nh1, nh2;
	FastBitQueryHandle h1, h2;
	for (i = 0; i < 5; ++ i) {
	    h1 = fastbit_build_query(0, dir, conditions[i]);
	    nh1 = fastbit_get_result_rows(h1);
	    if (nh1 != mult * counts[i]) {
		++ nerrors;
		fprintf(output, "%s: query \"%s\" on %d built-in records found "
			"%d hits, but %d were expected\n", nm, conditions[i],
			(int)(mult*100), nh1, (int)(mult*counts[i]));
	    }
            else if (msglvl > 1) {
                int j;
                uint32_t *rids = malloc(sizeof(uint32_t)*nh1);
                int ierr = fastbit_get_result_row_ids(h1, rids);
                fprintf(output, "%s: fastbit_get_result_ids returned %u, "
                        "expected %d\n", nm, ierr, nh1);
                for (j = 0; j < nh1; ++ j)
                    (void) fprintf(output, "  rid[%u] = %u\n",
                                   j, (unsigned int)rids[j]);
                free(rids);
            }
	    fastbit_destroy_query(h1);
	}

	/* try the empty where clause */
	h2 = fastbit_build_query(0, dir, 0);
	nh2 = fastbit_get_result_rows(h2);
	if (nh2 != 100 * mult) {
	    ++ nerrors;
	    fprintf(output, "%s: query expected to return %d rows, "
		    "but got %d instead\n", nm, 100*mult, nh2);
	}
	fastbit_destroy_query(h2);
    }

    /* try to append the same data again */
    fastbit_add_values("a", "int", ivals, 100, 0);
    fastbit_add_values("b", "short", svals, 100, 0);
    fastbit_add_values("c", "float", fvals, 100, 0);
    fastbit_flush_buffer(dir);
    /* test the same queries once more */
    ++ mult;
    for (i = 0; i < 5; ++ i) {
	FastBitQueryHandle h = fastbit_build_query(0, dir, conditions[i]);
	int nhits = fastbit_get_result_rows(h);
	if (nhits != mult * counts[i]) {
	    ++ nerrors;
	    fprintf(output, "%s: query \"%s\" on %d built-in records found "
		    "%d hits, but %d were expected\n", nm, conditions[i],
		    (int)(mult*100), nhits, (int)(mult*counts[i]));
	}
        else if (msglvl > 1) {
            int j;
            uint32_t *rids = malloc(sizeof(uint32_t)*nhits);
            int ierr = fastbit_get_result_row_ids(h, rids);
            fprintf(output, "%s: fastbit_get_result_ids returned %u, "
                    "expected %d\n", nm, ierr, nhits);
            for (j = 0; j < nhits; ++ j)
                (void) fprintf(output, "  rid[%u] = %u\n",
                               j, (unsigned int)rids[j]);
            free(rids);
        }
	fastbit_destroy_query(h);
    }
    fprintf(output, "%s: built-in tests finished with nerrors = %d\n",
	    nm, nerrors);
} /* builtin */

int main(int argc, char **argv) {
    int ierr, msglvl, nhits, vselect;
    const char *conffile;
    const char *logfile;
    FILE* output;
    FastBitQueryHandle qh;
    FastBitResultSetHandle rh;

    ierr = 0;
    msglvl = 0;
    vselect = 1;
    logfile = 0;
    conffile = 0;
#if defined(DEBUG) || defined(_DEBUG)
#if DEBUG + 0 > 10 || _DEBUG + 0 > 10
    msglvl = INT_MAX;
#elif DEBUG + 0 > 0
    msglvl += 7 * DEBUG;
#elif _DEBUG + 0 > 0
    msglvl += 5 * _DEBUG;
#else
    msglvl += 3;
#endif
#endif
    /* process arguments started with - */
    while (vselect < argc && argv[vselect][0] == '-') {
	if (argv[vselect][1] == 'c' || argv[vselect][1] == 'C') {
	    if (vselect+1 < argc) {
		conffile = argv[vselect+1];
		vselect += 2;
	    }
	    else {
		vselect += 1;
	    }
	}
	else if (argv[vselect][1] == 'h' || argv[vselect][1] == 'H') {
	    usage(*argv);
	    vselect += 1;
	}
#if defined(TCAPI_USE_LOGFILE)
	else if (argv[vselect][1] == 'l' || argv[vselect][1] == 'L') {
	    if (vselect+1 < argc) {
		logfile = argv[vselect+1];
		vselect += 2;
	    }
	    else {
		vselect += 1;
	    }
	}
#endif
	else if (argv[vselect][1] == 'm' || argv[vselect][1] == 'M' ||
		 argv[vselect][1] == 'v' || argv[vselect][1] == 'V') {
	    if (vselect+1 < argc &&
		(isdigit(argv[vselect+1][0]) != 0 ||
		 (argv[vselect+1][0] == '-' &&
		  isdigit(argv[vselect+1][1]) != 0))) {
		msglvl += atoi(argv[vselect+1]);
		vselect += 2;
	    }
	    else {
		msglvl += 1;
		vselect += 1;
	    }
	}
	else {
	    fprintf(stderr, "%s: unknown option %s\n", *argv, argv[vselect]);
	    ++ vselect;
	}
    }

    fastbit_init((const char*)conffile);
    fastbit_set_verbose_level(msglvl);
    fastbit_set_logfile(logfile);
#if defined(TCAPI_USE_LOGFILE)
    output = fastbit_get_logfilepointer();
    printf("%s: output=0x%8.8x, stdout=0x%8.8x\n", *argv, output, stdout);
#else
    output = stdout;
#endif
    if (argc <= vselect) {
	builtin(*argv, output);
	return -1;
    }

    if (argc == vselect+1) /* buld indexes */
	return fastbit_build_indexes(argv[vselect], (const char*)0);

    qh = fastbit_build_query(0, argv[vselect], argv[vselect+1]);
    if (qh == 0) {
	fprintf(output, "%s failed to process query \"%s\" on data in %s\n",
		argv[0], argv[vselect+1], argv[vselect]);
	fastbit_cleanup();
	return -2;
    }

    nhits = fastbit_get_result_rows(qh);
    fprintf(output, "%s: applying \"%s\" on data in %s produced %d hit%s\n",
	    argv[0], argv[vselect+1], argv[vselect], nhits,
	    (nhits>1 ? "s" : ""));
    if (nhits <= 0)
	return 0;
    if (msglvl > 1) {
        int j;
        uint32_t *rids = malloc(sizeof(uint32_t)*nhits);
        int ierr = fastbit_get_result_row_ids(qh, rids);
        fprintf(output, "%s: fastbit_get_result_ids returned %u, expected %d\n",
                *argv, ierr, nhits);
        for (j = 0; j < nhits; ++ j)
            (void) fprintf(output, "  rid[%u] = %u\n",
                           j, (unsigned int)rids[j]);
        free(rids);
    }

    /* print the selected values specified in the select clause.  Since the
       select clause was nil in the call to fastbit_build_query, there
       would be nothing to print here! */
    rh = fastbit_build_result_set(qh);
    if (rh != 0) {
	int ncols = fastbit_get_result_columns(qh);
	fprintf(output, "%s\n", fastbit_get_select_clause(qh));
	while (fastbit_result_set_next(rh) == 0) {
	    int i;
	    fprintf(output, "%s", fastbit_result_set_getString(rh, 0));
	    for (i = 1; i < ncols; ++ i)
		fprintf(output, ", %s", fastbit_result_set_getString(rh, i));
	    fprintf(output, "\n");
	}
	fastbit_destroy_result_set(rh);
    }
    fflush(output);

    vselect += 2;
    /* print attributes explicitly specified on the command line */
    if (argc > vselect) {
	int i, j;
	for (i = vselect; i < argc; i += 2) {
	    char t = (i+1<argc ? argv[i+1][0] : 'i');
	    switch (t) {
	    default:
	    case 'i':
	    case 'I': {
		const int32_t *tmp = fastbit_get_qualified_ints(qh, argv[i]);
		if (tmp != 0) {
		    fprintf(output, "%s[%d]=", argv[i], nhits);
		    for (j = 0; j < nhits; ++ j)
			fprintf(output, "%d ", (int)tmp[j]);
		    fprintf(output, "\n");
		}
		else {
		    fprintf(output, "%s: failed to retrieve values for "
			    "column %s (requested type %c)\n",
			    argv[0], argv[i], t);
		}
		break;}
	    case 'u':
	    case 'U': {
		const uint32_t *tmp = fastbit_get_qualified_uints(qh, argv[i]);
		if (tmp != 0) {
		    fprintf(output, "%s[%d]=", argv[i], nhits);
		    for (j = 0; j < nhits; ++ j)
			fprintf(output, "%u ", (unsigned)tmp[j]);
		    fprintf(output, "\n");
		}
		else {
		    fprintf(output, "%s: failed to retrieve value for "
			    "column %s (requested type %c)\n",
			    argv[0], argv[i], t);
		}
		break;}
	    case 'l':
	    case 'L': {
		const int64_t *tmp = fastbit_get_qualified_longs(qh, argv[i]);
		if (tmp != 0) {
		    fprintf(output, "%s[%d]=", argv[i], nhits);
		    for (j = 0; j < nhits; ++ j)
			fprintf(output, "%lld ", (long long)tmp[j]);
		    fprintf(output, "\n");
		}
		else {
		    fprintf(output, "%s: failed to retrieve value for "
			    "column %s (requested type %c)\n",
			    argv[0], argv[i], t);
		}
		break;}
	    case 'r':
	    case 'R':
	    case 'f':
	    case 'F': {
		const float *tmp = fastbit_get_qualified_floats(qh, argv[i]);
		if (tmp != 0) {
		    fprintf(output, "%s[%d]=", argv[i], nhits);
		    for (j = 0; j < nhits; ++ j)
			fprintf(output, "%g ", tmp[j]);
		    fprintf(output, "\n");
		}
		else {
		    fprintf(output, "%s: failed to retrieve value for "
			    "column %s (requested type %c)\n",
			    argv[0], argv[i], t);
		}
		break;}
	    case 'd':
	    case 'D': {
		const double *tmp = fastbit_get_qualified_doubles(qh, argv[i]);
		if (tmp != 0) {
		    fprintf(output, "%s[%d]=", argv[i], nhits);
		    for (j = 0; j < nhits; ++ j)
			fprintf(output, "%lG ", tmp[j]);
		    fprintf(output, "\n");
		}
		else {
		    fprintf(output, "%s: failed to retrieve value for "
			    "column %s (requested type %c)\n",
			    argv[0], argv[i], t);
		}
		break;}
	    case 's':
	    case 'S':
	    case 't':
	    case 'T': {
		const char **tmp = fastbit_get_qualified_strings(qh, argv[i]);
		if (tmp != 0) {
		    fprintf(output, "%s[%d]=", argv[i], nhits);
		    for (j = 0; j < nhits; ++ j)
			fprintf(output, "\"%s\" ", tmp[j]);
		    fprintf(output, "\n");
		}
		else {
		    fprintf(output, "%s: failed to retrieve value for "
			    "column %s (requested type %c)\n",
			    argv[0], argv[i], t);
		}
		break;}
	    }
	}
    }

    ierr = fastbit_destroy_query(qh);
    fastbit_cleanup();
    return ierr;
} /* main */
