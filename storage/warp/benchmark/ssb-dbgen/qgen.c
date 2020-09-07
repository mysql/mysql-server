/*
 * Sccsid:     @(#)qgen.c	2.1.8.2
 * qgen.c -- routines to convert query templates to executable query
 *           text for TPC-H and TPC-R
 */
#define DECLARER

#include <stdio.h>
#include <string.h>
#if (defined(_POSIX_)||!defined(WIN32))
/*
#include <unistd.h>
*/
#else
#include "process.h"
#endif /* WIN32 */
#include <ctype.h>
#include <time.h>
#include "config.h"
#include "dss.h"
#include "tpcd.h"
#include "permute.h"


#define LINE_SIZE 512

/*
 * Function Protoypes
 */
void varsub PROTO((int qnum, int vnum, int flags));
int strip_comments PROTO((char *line));
void usage PROTO((void));
int process_options PROTO((int cnt, char **args));
int setup PROTO((void));
void qsub PROTO((char *qtag, int flags));



extern char *optarg;
extern int optind;
char **mk_ascdate(void);
extern seed_t Seed[];

char **asc_date;
int snum = -1;
char *prog;
tdef tdefs = { NULL };
long rndm;
double flt_scale;
distribution q13a, q13b;
int qnum;


/*
 * FUNCTION strip_comments(line)
 *
 * remove all comments from 'line'; recognizes both {} and -- comments
 */
int
strip_comments(char *line)
{
    static int in_comment = 0;
    char *cp1, *cp2;

    cp1 = line;
    
    while (1)   /* traverse the entire string */
        {
        if (in_comment)
            {
            if ((cp2 = strchr(cp1, '}')) != NULL) /* comment ends */
                {
                strcpy(cp1, cp2 + 1);
                in_comment = 0;
                continue;
                }
            else 
                {
                *cp1 = '\0';
                break;
                }
            }
        else    /* not in_comment */
            {
            if ((cp2 = strchr(cp1, '-')) != NULL)
                {
                if (*(cp2 + 1) == '-')  /* found a '--' comment */
                    {
                    *cp2 = '\0';
                    break;
                    }
                }
            if ((cp2 = strchr(cp1, '{')) != NULL) /* comment starts */
                {
                in_comment = 1;
                *cp2 = ' ';
                continue;
                }
            else break;
            }
        }
    return(0);
}

/*
 * FUNCTION qsub(char *qtag, int flags)
 *
 * based on the settings of flags, and the template file $QDIR/qtag.sql
 * make the following substitutions to turn a query template into EQT
 *
 *  String      Converted to            Based on
 *  ======      ============            ===========
 *  first line  database <db_name>;      -n from command line
 *  second line set explain on;         -x from command line
 *   :<number>  parameter <number>
 *  :k          set number
 *  :o          output to outpath/qnum.snum    
 *                                      -o from command line, SET_OUTPUT
 *  :s          stream number
 *  :b          BEGIN WORK;             -a from command line, START_TRAN
 *  :e          COMMIT WORK;            -a from command line, END_TRAN
 *  :q          query number
 *  :n<number>                          sets rowcount to be returned
 */
void
qsub(char *qtag, int flags)
{
static char *line = NULL,
    *qpath = NULL;
FILE *qfp;
char *cptr,
    *mark,
    *qroot = NULL;

    qnum = atoi(qtag);
    if (line == NULL)
        {
        line = malloc(BUFSIZ);
        qpath = malloc(BUFSIZ);
        MALLOC_CHECK(line);
        MALLOC_CHECK(qpath);
        }

    qroot = env_config(QDIR_TAG, QDIR_DFLT);
    sprintf(qpath, "%s%c%s.sql", 
		qroot, PATH_SEP, qtag);
    qfp = fopen(qpath, "r");
    OPEN_CHECK(qfp, qpath);

    rowcnt = rowcnt_dflt[qnum];
    varsub(qnum, 0, flags); /* set the variables */
    if (flags & DFLT_NUM)
        fprintf(ofp, SET_ROWCOUNT, rowcnt);
    while (fgets(line, BUFSIZ, qfp) != NULL)
        {
        if (!(flags & COMMENT))
            strip_comments(line);
        mark = line;
        while ((cptr = strchr(mark, VTAG)) != NULL)
            {
            *cptr = '\0';
             cptr++;
            fprintf(ofp,"%s", mark);
            switch(*cptr)
                {
                case 'b':
                case 'B':
                    if (!(flags & ANSI))
                        fprintf(ofp,"%s\n", START_TRAN);
                    cptr++;
                    break;
                case 'c':
                case 'C':
                    if (flags & DBASE)
                        fprintf(ofp, SET_DBASE, db_name);
                    cptr++;
                    break;
                case 'e':
                case 'E':
                    if (!(flags & ANSI))
                        fprintf(ofp,"%s\n", END_TRAN);
                    cptr++;
                    break;
                case 'n':
                case 'N':
                    if (!(flags & DFLT_NUM))
                        {
                        rowcnt=atoi(++cptr);
                        while (isdigit(*cptr) || *cptr == ' ') cptr++;
                        fprintf(ofp, SET_ROWCOUNT, rowcnt);
                        }
                    continue;
                case 'o':
                case 'O':
                    if (flags & OUTPUT)
                        fprintf(ofp,"%s '%s/%s.%d'", SET_OUTPUT, osuff, 
                            qtag, (snum < 0)?0:snum);
                    cptr++;
                    break;
                case 'q':
                case 'Q':
                    fprintf(ofp,"%s", qtag);
                    cptr++;
                    break;
                case 's':
                case 'S':
                    fprintf(ofp,"%d", (snum < 0)?0:snum);
                    cptr++;
                    break;
                case 'X':
                case 'x':
                    if (flags & EXPLAIN)
                        fprintf(ofp, "%s\n", GEN_QUERY_PLAN);
                    cptr++;
                    break;
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
                    varsub(qnum, atoi(cptr), flags & DFLT);
                    while (isdigit(*++cptr));
                    break;
                default:
		    fprintf(stderr, "-- unknown flag '%c%c' ignored\n", 
                        VTAG, *cptr);
		    cptr++;
		    break;
                }
            mark=cptr;
            }
        fprintf(ofp,"%s", mark);
        }
    fclose(qfp);
    fflush(stdout);
    return;
}

void
usage(void)
{
printf("%s Parameter Substitution (v. %d.%d.%d%s)\n", 
          NAME, VERSION,RELEASE,
            MODIFICATION,PATCH);
printf("Copyright %s %s\n", TPC, C_DATES);
printf("USAGE: %s <options> [ queries ]\n", prog);
printf("Options:\n");
printf("\t-a\t\t-- use ANSI semantics.\n");
printf("\t-b <str>\t-- load distributions from <str>\n");
printf("\t-c\t\t-- retain comments found in template.\n");
printf("\t-d\t\t-- use default substitution values.\n");
printf("\t-h\t\t-- print this usage summary.\n");
printf("\t-i <str>\t-- use the contents of file <str> to begin a query.\n");
printf("\t-l <str>\t-- log parameters to <str>.\n");
printf("\t-n <str>\t-- connect to database <str>.\n");
printf("\t-N\t\t-- use default rowcounts and ignore :n directive.\n");
printf("\t-o <str>\t-- set the output file base path to <str>.\n");
printf("\t-p <n>\t\t-- use the query permutation for stream <n>\n");
printf("\t-r <n>\t\t-- seed the random number generator with <n>\n");
printf("\t-s <n>\t\t-- base substitutions on an SF of <n>\n");
printf("\t-v\t\t-- verbose.\n");
printf("\t-t <str>\t-- use the contents of file <str> to complete a query\n");
printf("\t-x\t\t-- enable SET EXPLAIN in each query.\n");
}

int
process_options(int cnt, char **args)
{
    int flag;

    while((flag = getopt(cnt, args, "ab:cdhi:n:Nl:o:p:r:s:t:vx")) != -1)
        switch(flag)
            {
            case 'a':   /* use ANSI semantics */
                flags |= ANSI;
                break;
			case 'b':               /* load distributions from named file */
				d_path = (char *)malloc(strlen(optarg) + 1);
				MALLOC_CHECK(d_path);
				strcpy(d_path, optarg);
				break;
			case 'c':   /* retain comments in EQT */
                flags |= COMMENT;
                break;
            case 'd':   /* use default substitution values */
                flags |= DFLT;
                break;
            case 'h':   /* just generate the usage summary */
                usage();
                exit(0);
                break;
            case 'i':   /* set stream initialization file name */
                ifile = malloc(strlen(optarg) + 1);
                MALLOC_CHECK(ifile);
                strcpy(ifile, optarg);
                flags |= INIT;
                break;
            case 'l':   /* log parameter usages */
                lfile = malloc(strlen(optarg) + 1);
                MALLOC_CHECK(lfile);
                strcpy(lfile, optarg);
                flags |= LOG;
                break;
            case 'N':   /* use default rowcounts */
                flags |= DFLT_NUM;
                break;
            case 'n':   /* set database name */
                db_name = malloc(strlen(optarg) + 1);
                MALLOC_CHECK(db_name);
                strcpy(db_name, optarg);
                flags |= DBASE;
                break;
            case 'o':   /* set the output path */
                osuff = malloc(strlen(optarg) + 1);
                MALLOC_CHECK(osuff);
                strcpy(osuff, optarg);
                flags |=OUTPUT;
                break;
            case 'p':   /* permutation for a given stream */
                snum = atoi(optarg);
                break;
            case 'r':   /* set random number seed for parameter gen */
                flags |= SEED;
                rndm = atol(optarg);
                break;
            case 's':   /* scale of data set to run against */
                flt_scale = atof(optarg);
				if (scale > MAX_SCALE)
					fprintf(stderr, "%s %5.0f %s\n%s\n",
						"WARNING: Support for scale factors >",
						MAX_SCALE,
						"GB is still in development.",
						"Data set integrity is not guaranteed.\n");
                break;
            case 't':   /* set termination file name */
                tfile = malloc(strlen(optarg) + 1);
                MALLOC_CHECK(tfile);
                strcpy(tfile, optarg);
                flags |= TERMINATE;
                break;
            case 'v':   /* verbose */
                flags |= VERBOSE;
                break;
            case 'x':   /* set explain in the queries */
                flags |= EXPLAIN;
                break;
            default:
                printf("unknown option '%s' ignored\n", args[optind]);
                usage();
                exit(1);
                break;
            }
    return(0);
}

int
setup(void)
{

    asc_date = mk_ascdate();

    read_dist(env_config(DIST_TAG, DIST_DFLT), "p_cntr", &p_cntr_set);
    read_dist(env_config(DIST_TAG, DIST_DFLT), "colors", &colors);
    read_dist(env_config(DIST_TAG, DIST_DFLT), "p_types", &p_types_set);
    read_dist(env_config(DIST_TAG, DIST_DFLT), "nations", &nations);
    read_dist(env_config(DIST_TAG, DIST_DFLT), "nations2", &nations2);
    read_dist(env_config(DIST_TAG, DIST_DFLT), "regions", &regions);
    read_dist(env_config(DIST_TAG, DIST_DFLT), "o_oprio", 
        &o_priority_set);
    read_dist(env_config(DIST_TAG, DIST_DFLT), "instruct", 
        &l_instruct_set);
    read_dist(env_config(DIST_TAG, DIST_DFLT), "smode", &l_smode_set);
    read_dist(env_config(DIST_TAG, DIST_DFLT), "category", 
        &l_category_set);
    read_dist(env_config(DIST_TAG, DIST_DFLT), "rflag", &l_rflag_set);
    read_dist(env_config(DIST_TAG, DIST_DFLT), "msegmnt", &c_mseg_set);
	read_dist(env_config(DIST_TAG, DIST_DFLT), "Q13a", &q13a);
	read_dist(env_config(DIST_TAG, DIST_DFLT), "Q13b", &q13b);

    return(0);
}


main(int ac, char **av)
{
    int i;
    FILE *ifp;
    char line[LINE_SIZE];

    prog = av[0];
    flt_scale = (double)1.0;
    flags = 0;
	d_path = NULL;
    process_options(ac, av);
    if (flags & VERBOSE)
        fprintf(ofp, 
	    "-- TPC %s Parameter Substitution (Version %d.%d.%d%s)\n",
            NAME, VERSION, RELEASE, MODIFICATION, PATCH);

    setup();

    if (!(flags & DFLT))        /* perturb the RNG */
	    {
	    if (!(flags & SEED))
                rndm = (long)((unsigned)time(NULL) * DSS_PROC);
		if (rndm < 0)
			rndm += 2147483647;
		Seed[0].value = rndm;
		for (i=1; i <= QUERIES_PER_SET; i++)
			{
			Seed[0].value = NextRand(Seed[0].value);
			Seed[i].value = Seed[0].value;
			}
		printf("-- using %ld as a seed to the RNG\n", rndm);
		}
    else
        printf("-- using default substitutions\n");
    
    if (flags & INIT)           /* init stream with ifile */
        {
        ifp = fopen(ifile, "r");
	OPEN_CHECK(ifp, ifile);
        while (fgets(line, LINE_SIZE, ifp) != NULL)
            fprintf(stdout, "%s", line);
        }

    if (snum >= 0)
        if (optind < ac)
            for (i=optind; i < ac; i++)
                {
                char qname[10];
                sprintf(qname, "%d", SEQUENCE(snum, atoi(av[i])));
                qsub(qname, flags);
                }
        else
            for (i=1; i <= QUERIES_PER_SET; i++)
                {
                char qname[10];
                sprintf(qname, "%d", SEQUENCE(snum, i));
                qsub(qname, flags);
                }
    else
        if (optind < ac)
            for (i=optind; i < ac; i++)
                qsub(av[i], flags);   
        else
            for (i=1; i <= QUERIES_PER_SET; i++)
                {
                char qname[10];
                sprintf(qname, "%d", i);
                qsub(qname, flags);
                }
    
    if (flags & TERMINATE)      /* terminate stream with tfile */
        {
        ifp = fopen(tfile, "r");
        if (ifp == NULL)
	OPEN_CHECK(ifp, tfile);
        while (fgets(line, LINE_SIZE, ifp) != NULL)
            fprintf(stdout, "%s", line);
        }

    return(0);
}

