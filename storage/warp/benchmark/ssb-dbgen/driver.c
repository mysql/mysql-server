/* @(#)driver.c	2.1.8.4 */
/* main driver for dss banchmark */

#define DECLARER				/* EXTERN references get defined here */
#define NO_FUNC (int (*) ()) NULL	/* to clean up tdefs */
#define NO_LFUNC (long (*) ()) NULL		/* to clean up tdefs */

#include "config.h"
#include <stdlib.h>
#if (defined(_POSIX_)||!defined(WIN32))		/* Change for Windows NT */
#ifndef DOS
#include <unistd.h>
#include <sys/wait.h>
#endif

#endif /* WIN32 */
#include <stdio.h>				/* */
#include <limits.h>
#include <math.h>
#include <ctype.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#ifdef HP
#include <strings.h>
#endif
#if (defined(WIN32)&&!defined(_POSIX_))
#include <process.h>
#pragma warning(disable:4201)
#pragma warning(disable:4214)
#pragma warning(disable:4514)
#define WIN32_LEAN_AND_MEAN
#define NOATOM
#define NOGDICAPMASKS
#define NOMETAFILE
#define NOMINMAX
#define NOMSG
#define NOOPENFILE
#define NORASTEROPS
#define NOSCROLL
#define NOSOUND
#define NOSYSMETRICS
#define NOTEXTMETRIC
#define NOWH
#define NOCOMM
#define NOKANJI
#define NOMCX

#include "windows.h"

#pragma warning(default:4201)
#pragma warning(default:4214)
#endif

#include "dss.h"
#include "dsstypes.h"
#include "bcd2.h"

/*
* Function prototypes
*/
void	usage (void);
int		prep_direct (char *);
int		close_direct (void);
void	kill_load (void);
int		pload (int tbl);
void	gen_tbl (int tnum, long start, long count, long upd_num);
int		pr_drange (int tbl, long min, long cnt, long num);
int		set_files (int t, int pload);
int		partial (int, int);


extern int optind, opterr;
extern char *optarg;
long rowcnt = 0, minrow = 0, upd_num = 0;
double flt_scale;
#if (defined(WIN32)&&!defined(_POSIX_))
char *spawn_args[25];
#endif


/*
* general table descriptions. See dss.h for details on structure
* NOTE: tables with no scaling info are scaled according to
* another table
*
*
* the following is based on the tdef structure defined in dss.h as:
* typedef struct
* {
* char     *name;            -- name of the table; 
*                               flat file output in <name>.tbl
* long      base;            -- base scale rowcount of table; 
*                               0 if derived
* int       (*header) ();    -- function to prep output
* int       (*loader[2]) (); -- functions to present output
* long      (*gen_seed) ();  -- functions to seed the RNG
* int       (*verify) ();    -- function to verfiy the data set without building it
* int       child;           -- non-zero if there is an associated detail table
* unsigned long vtotal;      -- "checksum" total 
* }         tdef;
*
*/

/*
* flat file print functions; used with -F(lat) option
*/
#ifdef SSBM
int pr_cust (customer_t * c, int mode);
int pr_part (part_t * p, int mode);
int pr_supp (supplier_t * s, int mode);
int pr_line (order_t * o, int mode);
#else
int pr_cust (customer_t * c, int mode);
int pr_line (order_t * o, int mode);
int pr_order (order_t * o, int mode);
int pr_part (part_t * p, int mode);
int pr_psupp (part_t * p, int mode);
int pr_supp (supplier_t * s, int mode);
int pr_order_line (order_t * o, int mode);
int pr_part_psupp (part_t * p, int mode);
int pr_nation (code_t * c, int mode);
int pr_region (code_t * c, int mode);
#endif

/*
* inline load functions; used with -D(irect) option
*/
#ifdef SSBM
int ld_cust (customer_t * c, int mode);
int ld_part (part_t * p, int mode);
int ld_supp (supplier_t * s, int mode);

/*todo: get rid of ld_order*/
int ld_line (order_t * o, int mode);
int ld_order (order_t * o, int mode);

#else
int ld_cust (customer_t * c, int mode);
int ld_line (order_t * o, int mode);
int ld_order (order_t * o, int mode);
int ld_part (part_t * p, int mode);
int ld_psupp (part_t * p, int mode);
int ld_supp (supplier_t * s, int mode);
int ld_order_line (order_t * o, int mode);
int ld_part_psupp (part_t * p, int mode);
int ld_nation (code_t * c, int mode);
int ld_region (code_t * c, int mode);
#endif

/*
* seed generation functions; used with '-O s' option
*/
#ifdef SSBM
long sd_cust (int child, long skip_count);
long sd_part (int child, long skip_count);
long sd_supp (int child, long skip_count);

long sd_line (int child, long skip_count);
long sd_order (int child, long skip_count);

#else
long sd_cust (int child, long skip_count);
long sd_line (int child, long skip_count);
long sd_order (int child, long skip_count);
long sd_part (int child, long skip_count);
long sd_psupp (int child, long skip_count);
long sd_supp (int child, long skip_count);
long sd_order_line (int child, long skip_count);
long sd_part_psupp (int child, long skip_count);
#endif

/*
* header output functions); used with -h(eader) option
*/
#ifdef SSBM
int hd_cust (FILE * f);
int hd_part (FILE * f);
int hd_supp (FILE * f);
int hd_line (FILE * f);

#else
int hd_cust (FILE * f);
int hd_line (FILE * f);
int hd_order (FILE * f);
int hd_part (FILE * f);
int hd_psupp (FILE * f);
int hd_supp (FILE * f);
int hd_order_line (FILE * f);
int hd_part_psupp (FILE * f);
int hd_nation (FILE * f);
int hd_region (FILE * f);
#endif

/*
* data verfication functions; used with -O v option
*/
#ifdef SSBM
int vrf_cust (customer_t * c, int mode);
int vrf_part (part_t * p, int mode);
int vrf_supp (supplier_t * s, int mode);
int vrf_line (order_t * o, int mode);
int vrf_order (order_t * o, int mode);
int vrf_date (date_t,int mode);
#else
int vrf_cust (customer_t * c, int mode);
int vrf_line (order_t * o, int mode);
int vrf_order (order_t * o, int mode);
int vrf_part (part_t * p, int mode);
int vrf_psupp (part_t * p, int mode);
int vrf_supp (supplier_t * s, int mode);
int vrf_order_line (order_t * o, int mode);
int vrf_part_psupp (part_t * p, int mode);
int vrf_nation (code_t * c, int mode);
int vrf_region (code_t * c, int mode);
#endif


#ifdef SSBM
tdef tdefs[] =
{
   
    	{"part.tbl", "part table", 200000, hd_part,
		{pr_part, ld_part}, sd_part, vrf_part, PSUPP, 0},
	{0,0,0,0,{0,0}, 0,0,0,0},
	{"supplier.tbl", "suppliers table", 2000, hd_supp,
	        {pr_supp, ld_supp}, sd_supp, vrf_supp, NONE, 0},
    
	{"customer.tbl", "customers table", 30000, hd_cust,
		{pr_cust, ld_cust}, sd_cust, vrf_cust, NONE, 0},
	{"date.tbl","date table",2556,0,{pr_date,ld_date}, 0,vrf_date, NONE,0},
	/*line order is SF*1,500,000, however due to the implementation
	  the base here is 150,000 instead if 1500,000*/
	{"lineorder.tbl", "lineorder table", 150000, hd_line,
		{pr_line, ld_line}, sd_line, vrf_line, NONE, 0},
	{0,0,0,0,{0,0}, 0,0,0,0},
	{0,0,0,0,{0,0}, 0,0,0,0},
	{0,0,0,0,{0,0}, 0,0,0,0},
	{0,0,0,0,{0,0}, 0,0,0,0},
};

#else

tdef tdefs[] =
{
	{"part.tbl", "part table", 200000, hd_part,
		{pr_part, ld_part}, sd_part, vrf_part, PSUPP, 0},
	{"partsupp.tbl", "partsupplier table", 200000, hd_psupp,
		{pr_psupp, ld_psupp}, sd_psupp, vrf_psupp, NONE, 0},
	{"supplier.tbl", "suppliers table", 10000, hd_supp,
		{pr_supp, ld_supp}, sd_supp, vrf_supp, NONE, 0},
	{"customer.tbl", "customers table", 150000, hd_cust,
		{pr_cust, ld_cust}, sd_cust, vrf_cust, NONE, 0},
	{"orders.tbl", "order table", 150000, hd_order,
		{pr_order, ld_order}, sd_order, vrf_order, LINE, 0},
	{"lineitem.tbl", "lineitem table", 150000, hd_line,
		{pr_line, ld_line}, sd_line, vrf_line, NONE, 0},
	{"orders.tbl", "orders/lineitem tables", 150000, hd_order_line,
		{pr_order_line, ld_order_line}, sd_order, vrf_order_line, LINE, 0},
	{"part.tbl", "part/partsupplier tables", 200000, hd_part_psupp,
		{pr_part_psupp, ld_part_psupp}, sd_part, vrf_part_psupp, PSUPP, 0},
	{"nation.tbl", "nation table", NATIONS_MAX, hd_nation,
		{pr_nation, ld_nation}, NO_LFUNC, vrf_nation, NONE, 0},
	{"region.tbl", "region table", NATIONS_MAX, hd_region,
		{pr_region, ld_region}, NO_LFUNC, vrf_region, NONE, 0},
};
#endif
int *pids;


/*
* routines to handle the graceful cleanup of multi-process loads
*/

void
stop_proc (int signum)
{
	exit (0);
}

void
kill_load (void)
{
	int i;
	
#if !defined(U2200) && !defined(DOS)
	for (i = 0; i < children; i++)
		if (pids[i])
			KILL (pids[i]);
#endif /* !U2200 && !DOS */
		return;
}

/*
* re-set default output file names 
*/
int
set_files (int i, int pload)
{
	char line[80], *new_name;
	
	if (table & (1 << i))
child_table:
	{
		if (pload != -1)
			sprintf (line, "%s.%d", tdefs[i].name, pload);
		else
		{
			printf ("Enter new destination for %s data: ",
				tdefs[i].name);
			if (fgets (line, sizeof (line), stdin) == NULL)
				return (-1);;
			if ((new_name = strchr (line, '\n')) != NULL)
				*new_name = '\0';
			if (strlen (line) == 0)
				return (0);
		}
		new_name = (char *) malloc (strlen (line) + 1);
		MALLOC_CHECK (new_name);
		strcpy (new_name, line);
		tdefs[i].name = new_name;
		if (tdefs[i].child != NONE)
		{
			i = tdefs[i].child;
			tdefs[i].child = NONE;
			goto child_table;
		}
	}
	
	return (0);
}



/*
* read the distributions needed in the benchamrk
*/
void
load_dists (void)
{
	read_dist (env_config (DIST_TAG, DIST_DFLT), "p_cntr", &p_cntr_set);
	read_dist (env_config (DIST_TAG, DIST_DFLT), "colors", &colors);
	read_dist (env_config (DIST_TAG, DIST_DFLT), "p_types", &p_types_set);
	read_dist (env_config (DIST_TAG, DIST_DFLT), "nations", &nations);
	read_dist (env_config (DIST_TAG, DIST_DFLT), "regions", &regions);
	read_dist (env_config (DIST_TAG, DIST_DFLT), "o_oprio",
		&o_priority_set);
	read_dist (env_config (DIST_TAG, DIST_DFLT), "instruct",
		&l_instruct_set);
	read_dist (env_config (DIST_TAG, DIST_DFLT), "smode", &l_smode_set);
	read_dist (env_config (DIST_TAG, DIST_DFLT), "category",
		&l_category_set);
	read_dist (env_config (DIST_TAG, DIST_DFLT), "rflag", &l_rflag_set);
	read_dist (env_config (DIST_TAG, DIST_DFLT), "msegmnt", &c_mseg_set);

	/* load the distributions that contain text generation */
	read_dist (env_config (DIST_TAG, DIST_DFLT), "nouns", &nouns);
	read_dist (env_config (DIST_TAG, DIST_DFLT), "verbs", &verbs);
	read_dist (env_config (DIST_TAG, DIST_DFLT), "adjectives", &adjectives);
	read_dist (env_config (DIST_TAG, DIST_DFLT), "adverbs", &adverbs);
	read_dist (env_config (DIST_TAG, DIST_DFLT), "auxillaries", &auxillaries);
	read_dist (env_config (DIST_TAG, DIST_DFLT), "terminators", &terminators);
	read_dist (env_config (DIST_TAG, DIST_DFLT), "articles", &articles);
	read_dist (env_config (DIST_TAG, DIST_DFLT), "prepositions", &prepositions);
	read_dist (env_config (DIST_TAG, DIST_DFLT), "grammar", &grammar);
	read_dist (env_config (DIST_TAG, DIST_DFLT), "np", &np);
	read_dist (env_config (DIST_TAG, DIST_DFLT), "vp", &vp);
	
}

/*
* generate a particular table
*/
void
gen_tbl (int tnum, long start, long count, long upd_num)
{
	static order_t o;
	supplier_t supp;
	customer_t cust;
	part_t part;
#ifdef SSBM
	date_t dt;
#else
	code_t code;
#endif
	static int completed = 0;
	static int init = 0;
	long i;

	int rows_per_segment=0;
	int rows_this_segment=-1;
	int residual_rows=0;

	if (insert_segments)
		{
		rows_per_segment = count / insert_segments;
		residual_rows = count - (rows_per_segment * insert_segments);
		}

	if (init == 0)
	{
		INIT_HUGE(o.okey);
		for (i=0; i < O_LCNT_MAX; i++)
#ifdef SSBM
			INIT_HUGE(o.lineorders[i].okey);	
#else
			INIT_HUGE(o.l[i].okey);
#endif
		init = 1;
	}

	for (i = start; count; count--, i++)
	{
		LIFENOISE (1000, i);
		row_start(tnum);

		switch (tnum)
		{
		case LINE:
#ifdef SSBM
#else
		case ORDER:
  		case ORDER_LINE: 
#endif
			mk_order (i, &o, upd_num % 10000);

		  if (insert_segments  && (upd_num > 0))
			if((upd_num / 10000) < residual_rows)
				{
				if((++rows_this_segment) > rows_per_segment) 
					{						
					rows_this_segment=0;
					upd_num += 10000;					
					}
				}
			else
				{
				if((++rows_this_segment) >= rows_per_segment) 
					{
					rows_this_segment=0;
					upd_num += 10000;
					}
				}

			if (set_seeds == 0)
				if (validate)
					tdefs[tnum].verify(&o, 0);
				else
					tdefs[tnum].loader[direct] (&o, upd_num);
			break;
		case SUPP:
			mk_supp (i, &supp);
			if (set_seeds == 0)
				if (validate)
					tdefs[tnum].verify(&supp, 0);
				else
					tdefs[tnum].loader[direct] (&supp, upd_num);
			break;
		case CUST:
			mk_cust (i, &cust);
			if (set_seeds == 0)
				if (validate)
					tdefs[tnum].verify(&cust, 0);
				else
					tdefs[tnum].loader[direct] (&cust, upd_num);
			break;
#ifdef SSBM
		case PART:
#else
		case PSUPP:
		case PART:
  		case PART_PSUPP:
#endif 
			mk_part (i, &part);
			if (set_seeds == 0)
				if (validate)
					tdefs[tnum].verify(&part, 0);
				else
					tdefs[tnum].loader[direct] (&part, upd_num);
			break;
#ifdef SSBM
		case DATE:
			mk_date (i, &dt);
			if (set_seeds == 0)
				if (validate)
					tdefs[tnum].verify(&dt, 0);
				else
					tdefs[tnum].loader[direct] (&dt, 0);
			break;
#else
		case NATION:
			mk_nation (i, &code);
			if (set_seeds == 0)
				if (validate)
					tdefs[tnum].verify(&code, 0);
				else
					tdefs[tnum].loader[direct] (&code, 0);
			break;
		case REGION:
			mk_region (i, &code);
			if (set_seeds == 0)
				if (validate)
					tdefs[tnum].verify(&code, 0);
				else
					tdefs[tnum].loader[direct] (&code, 0);
			break;
#endif
		}
		row_stop(tnum);
		if (set_seeds && (i % tdefs[tnum].base) < 2)
		{
			printf("\nSeeds for %s at rowcount %ld\n", tdefs[tnum].comment, i);
			dump_seeds(tnum);
		}
	}
	completed |= 1 << tnum;
}



void
usage (void)
{
#ifdef SSBM
	fprintf (stderr, "%s\n%s\n\t%s\n%s %s\n\n",
		"USAGE:",
		"dbgen [-{vfFD}] [-O {fhmsv}][-T {pcsdla}]",
		"[-s <scale>][-C <procs>][-S <step>]",
		"dbgen [-v] [-O {dfhmr}] [-s <scale>]",
		"[-U <updates>] [-r <percent>]");

#else
	fprintf (stderr, "%s\n%s\n\t%s\n%s %s\n\n",
		"USAGE:",
		"dbgen [-{vfFD}] [-O {fhmsv}][-T {pcsoPSOL}]",
		"[-s <scale>][-C <procs>][-S <step>]",
		"dbgen [-v] [-O {dfhmr}] [-s <scale>]",
		"[-U <updates>] [-r <percent>]");
#endif
	fprintf (stderr, "-b <s> -- load distributions for <s>\n");
	fprintf (stderr, "-C <n> -- use <n> processes to generate data\n");
	fprintf (stderr, "          [Under DOS, must be used with -S]\n");
	fprintf (stderr, "-D     -- do database load in line\n");
	fprintf (stderr, "-d <n> -- split deletes between <n> files\n");
	fprintf (stderr, "-f     -- force. Overwrite existing files\n");
	fprintf (stderr, "-F     -- generate flat files output\n");
	fprintf (stderr, "-h     -- display this message\n");
	fprintf (stderr, "-i <n> -- split inserts between <n> files\n");
	fprintf (stderr, "-n <s> -- inline load into database <s>\n");
	fprintf (stderr, "-O d   -- generate SQL syntax for deletes\n");
	fprintf (stderr, "-O f   -- over-ride default output file names\n");
	fprintf (stderr, "-O h   -- output files with headers\n");
	fprintf (stderr, "-O m   -- produce columnar output\n");
	fprintf (stderr, "-O r   -- generate key ranges for deletes.\n");
	fprintf (stderr, "-O v   -- Verify data set without generating it.\n");
	fprintf (stderr, "-q     -- enable QUIET mode\n");
	fprintf (stderr, "-r <n> -- updates refresh (n/100)%% of the\n");
	fprintf (stderr, "          data set\n");
	fprintf (stderr, "-s <n> -- set Scale Factor (SF) to  <n> \n");
	fprintf (stderr, "-S <n> -- build the <n>th step of the data/update set\n");

#ifdef SSBM
	fprintf (stderr, "-T c   -- generate cutomers dimension table ONLY\n");
	fprintf (stderr, "-T p   -- generate parts dimension table ONLY\n");
	fprintf (stderr, "-T s   -- generate suppliers dimension table ONLY\n");
	fprintf (stderr, "-T d   -- generate date dimension table ONLY\n");
	fprintf (stderr, "-T l   -- generate lineorder fact table ONLY\n");
#else
	fprintf (stderr, "-T c   -- generate cutomers ONLY\n");
	fprintf (stderr, "-T l   -- generate nation/region ONLY\n");
	fprintf (stderr, "-T L   -- generate lineitem ONLY\n");
	fprintf (stderr, "-T n   -- generate nation ONLY\n");
	fprintf (stderr, "-T o   -- generate orders/lineitem ONLY\n");
	fprintf (stderr, "-T O   -- generate orders ONLY\n");
	fprintf (stderr, "-T p   -- generate parts/partsupp ONLY\n");
	fprintf (stderr, "-T P   -- generate parts ONLY\n");
	fprintf (stderr, "-T r   -- generate region ONLY\n");
	fprintf (stderr, "-T s   -- generate suppliers ONLY\n");
	fprintf (stderr, "-T S   -- generate partsupp ONLY\n");
#endif

	fprintf (stderr, "-U <s> -- generate <s> update sets\n");
	fprintf (stderr, "-v     -- enable VERBOSE mode\n");
	fprintf (stderr,
		"\nTo generate the SF=1 (1GB), validation database population, use:\n");
	fprintf (stderr, "\tdbgen -vfF -s 1\n");
	fprintf (stderr, "\nTo generate updates for a SF=1 (1GB), use:\n");
	fprintf (stderr, "\tdbgen -v -U 1 -s 1\n");
}

/*
* pload() -- handle the parallel loading of tables
*/
/*
* int partial(int tbl, int s) -- generate the s-th part of the named tables data
*/
int
partial (int tbl, int s)
{
	long rowcnt;
	long extra;
	
	if (verbose > 0)
	{
		fprintf (stderr, "\tStarting to load stage %d of %d for %s...",
			s, children, tdefs[tbl].comment);
	}
	
	if (direct == 0)
		set_files (tbl, s);
	
	rowcnt = set_state(tbl, scale, children, s, &extra);

	if (s == children)
		gen_tbl (tbl, rowcnt * (s - 1) + 1, rowcnt + extra, upd_num);
	else
		gen_tbl (tbl, rowcnt * (s - 1) + 1, rowcnt, upd_num);
	
	if (verbose > 0)
		fprintf (stderr, "done.\n");
	
	return (0);
}

#ifndef DOS

int
pload (int tbl)
{
	int c = 0, i, status;
	
	if (verbose > 0)
	{
		fprintf (stderr, "Starting %d children to load %s",
			children, tdefs[tbl].comment);
	}
	for (c = 0; c < children; c++)
	{
		pids[c] = SPAWN ();
		if (pids[c] == -1)
		{
			perror ("Child loader not created");
			kill_load ();
			exit (-1);
		}
		else if (pids[c] == 0)	/* CHILD */
		{
			SET_HANDLER (stop_proc);
			verbose = 0;
			partial (tbl, c+1);
			exit (0);
		}
		else if (verbose > 0)			/* PARENT */
			fprintf (stderr, ".");
	}
	
	if (verbose > 0)
		fprintf (stderr, "waiting...");

	c = children;
	while (c)
	{
		i = WAIT (&status, pids[c - 1]);
		if (i == -1 && children)
		{
			if (errno == ECHILD)
				fprintf (stderr, "\nCould not wait on pid %d\n", pids[c - 1]);
			else if (errno == EINTR)
				fprintf (stderr, "\nProcess %d stopped abnormally\n", pids[c - 1]);
			else if (errno == EINVAL)
				fprintf (stderr, "\nProgram bug\n");
		}
		if (! WIFEXITED(status)) {
			(void) fprintf(stderr, "\nProcess %d: ", i);
			if (WIFSIGNALED(status)) {
				(void) fprintf(stderr, "rcvd signal %d\n",
					WTERMSIG(status));
				} else if (WIFSTOPPED(status)) {
				(void) fprintf(stderr, "stopped, signal %d\n",
					WSTOPSIG(status));
					}
				
			}
		c--;
	}

	if (verbose > 0)
		fprintf (stderr, "done\n");
	return (0);
}
#endif


void
process_options (int count, char **vector)
{
	int option;
	
	while ((option = getopt (count, vector,
		"b:C:Dd:Ffi:hn:O:P:qr:s:S:T:U:v")) != -1)
	switch (option)
		{
		case 'b':				/* load distributions from named file */
			d_path = (char *)malloc(strlen(optarg) + 1);
			MALLOC_CHECK(d_path);
			strcpy(d_path, optarg);
			break;
		case 'q':				/* all prompts disabled */
			verbose = -1;
			break;
		case 'i':
			insert_segments = atoi (optarg);
			break;
		case 'd':
			delete_segments = atoi (optarg);
			break;
	  case 'S':				/* generate a particular STEP */
		  step = atoi (optarg);
		  break;
	  case 'v':				/* life noises enabled */
		  verbose = 1;
		  break;
	  case 'f':				/* blind overwrites; Force */
		  force = 1;
		  break;
	  case 'T':				/* generate a specifc table */
		  switch (*optarg)
		  {
#ifdef SSBM
		  case 'c':			/* generate customer ONLY */
			  table = 1 << CUST;
			  break;
		  case 'p':			/* generate part ONLY */
			  table = 1 << PART;
			  break;
		  case 's':			/* generate partsupp ONLY */
			  table = 1 << SUPP;
			  break;
		  case 'd':			/* generate date ONLY */
			  table = 1 << DATE;
			  break;  
		  case 'l':			/* generate lineorder table ONLY */
			  table = 1 << LINE;
			  break;
		  case 'a':
		          table = 1 << CUST;
			  table |= 1 << PART;
			  table |= 1 << SUPP;
			  table |= 1 << DATE;
			  table |= 1 << LINE;
			  break;
#else
		  case 'c':			/* generate customer ONLY */
			  table = 1 << CUST;
			  break;
		  case 'L':			/* generate lineitems ONLY */
			  table = 1 << LINE;
			  break;
		  case 'l':			/* generate code table ONLY */
			  table = 1 << NATION;
			  table |= 1 << REGION;
			  break;
		  case 'n':			/* generate nation table ONLY */
			  table = 1 << NATION;
			  break;
		  case 'O':			/* generate orders ONLY */
			  table = 1 << ORDER;
			  break;
		  case 'o':			/* generate orders/lineitems ONLY */
			  table = 1 << ORDER_LINE;
			  break;
		  case 'P':			/* generate part ONLY */
			  table = 1 << PART;
			  break;
		  case 'p':			/* generate part/partsupp ONLY */
			  table = 1 << PART_PSUPP;
			  break;
		  case 'r':			/* generate region table ONLY */
			  table = 1 << REGION;
			  break;
		  case 'S':			/* generate partsupp ONLY */
			  table = 1 << PSUPP;
			  break;
		  case 's':			/* generate suppliers ONLY */
			  table = 1 << SUPP;
			  break;			  
#endif
		  default:
			  fprintf (stderr, "Unknown table name %s\n",
				  optarg);
			  usage ();
			  exit (1);
		  }
		  break;
		  case 's':				/* scale by Percentage of base rowcount */
		  case 'P':				/* for backward compatibility */
			  flt_scale = atof (optarg);
			  if (flt_scale < MIN_SCALE)
			  {
				  int i;
				  
				  scale = 1;
				  for (i = PART; i < REGION; i++)
				  {
					  tdefs[i].base *= flt_scale;
					  if (tdefs[i].base < 1)
						  tdefs[i].base = 1;
				  }
			  }
			  else
				  scale = (long) flt_scale;
			  if (scale > MAX_SCALE)
			  {
				  fprintf (stderr, "%s %5.0f %s\n\t%s\n\n",
					  "NOTE: Data generation for scale factors >",
					  MAX_SCALE,
					  "GB is still in development,",
					  "and is not yet supported.\n");
				  fprintf (stderr,
					  "Your resulting data set MAY NOT BE COMPLIANT!\n");
			  }
			  break;
		  case 'O':				/* optional actions */
			  switch (tolower (*optarg))
			  {
			  case 'd':			/* generate SQL for deletes */
				  gen_sql = 1;
				  break;
			  case 'f':			/* over-ride default file names */
				  fnames = 1;
				  break;
			  case 'h':			/* generate headers */
				  header = 1;
				  break;
			  case 'm':			/* generate columnar output */
				  columnar = 1;
				  break;
			  case 'r':			/* generate key ranges for delete */
				  gen_rng = 1;
				  break;
			  case 's':			/* calibrate the RNG usage */
				  set_seeds = 1;
				  break;
			  case 'v':			/* validate the data set */
				  validate = 1;
				  break;
			  default:
				  fprintf (stderr, "Unknown option name %s\n",
					  optarg);
				  usage ();
				  exit (1);
			  }
			  break;
			  case 'D':				/* direct load of generated data */
				  direct = 1;
				  break;
			  case 'F':				/* generate flat files for later loading */
				  direct = 0;
				  break;
			  case 'U':				/* generate flat files for update stream */
				  updates = atoi (optarg);
				  break;
			  case 'r':				/* set the refresh (update) percentage */
				  refresh = atoi (optarg);
				  break;
#ifndef DOS
			  case 'C':
				  children = atoi (optarg);
				  break;
#endif /* !DOS */
			  case 'n':				/* set name of database for direct load */
				  db_name = (char *) malloc (strlen (optarg) + 1);
				  MALLOC_CHECK (db_name);
				  strcpy (db_name, optarg);
				  break;
			  default:
				  printf ("ERROR: option '%c' unknown.\n",
					  *(vector[optind] + 1));
			  case 'h':				/* something unexpected */
				  fprintf (stderr,
					  "%s Population Generator (Version %d.%d.%d%s)\n",
					  NAME, VERSION, RELEASE,
					  MODIFICATION, PATCH);
				  fprintf (stderr, "Copyright %s %s\n", TPC, C_DATES);
				  usage ();
				  exit (1);
	  }

#ifndef DOS
	if (children != 1 && step == -1)
		{
		pids = malloc(children * sizeof(pid_t));
		MALLOC_CHECK(pids)
		}
#else
	if (children != 1 && step < 0)
		{
		fprintf(stderr, "ERROR: -C must be accompanied by -S on this platform\n");
		exit(1);
		}
#endif /* DOS */

	return;
}

/*
* MAIN
*
* assumes the existance of getopt() to clean up the command 
* line handling
*/
int
main (int ac, char **av)
{
	int i;
	
	table = (1 << CUST) |
		(1 << SUPP) |
		(1 << NATION) |
		(1 << REGION) |
		(1 << PART_PSUPP) |
		(1 << ORDER_LINE);
	force = 0;
	insert_segments=0;
	delete_segments=0;
	insert_orders_segment=0;
	insert_lineitem_segment=0;
	delete_segment=0;
	verbose = 0;
	columnar = 0;
	set_seeds = 0;
	header = 0;
	direct = 0;
	scale = 1;
	flt_scale = 1.0;
	updates = 0;
	refresh = UPD_PCT;
	step = -1;
#ifdef SSBM
	tdefs[LINE].base *=
		ORDERS_PER_CUST;			/* have to do this after init */
#else
	tdefs[ORDER].base *=
		ORDERS_PER_CUST;			/* have to do this after init */
	tdefs[LINE].base *=
		ORDERS_PER_CUST;			/* have to do this after init */
	tdefs[ORDER_LINE].base *=
		ORDERS_PER_CUST;			/* have to do this after init */
#endif
	fnames = 0;
	db_name = NULL;
	gen_sql = 0;
	gen_rng = 0;
	children = 1;
	d_path = NULL;
	
#ifdef NO_SUPPORT
	signal (SIGINT, exit);
#endif /* NO_SUPPORT */
	process_options (ac, av);
#if (defined(WIN32)&&!defined(_POSIX_))
	for (i = 0; i < ac; i++)
	{
		spawn_args[i] = malloc ((strlen (av[i]) + 1) * sizeof (char));
		MALLOC_CHECK (spawn_args[i]);
		strcpy (spawn_args[i], av[i]);
	}
	spawn_args[ac] = NULL;
#endif
	
	if (verbose >= 0)
		{
		fprintf (stderr,
			"%s Population Generator (Version %d.%d.%d%s)\n",
			NAME, VERSION, RELEASE, MODIFICATION, PATCH);
		fprintf (stderr, "Copyright %s %s\n", TPC, C_DATES);
		}
	
	load_dists ();
	/* have to do this after init */
	tdefs[NATION].base = nations.count;
	tdefs[REGION].base = regions.count;
	
	/* 
	* updates are never parallelized 
	*/
	if (updates)
		{
		/* 
		 * set RNG to start generating rows beyond SF=scale
		 */
		double fix1;

#ifdef SSBM
		set_state (LINE, scale, 1, 2, (long *)&i); 
		fix1 = (double)tdefs[LINE].base / (double)10000; /*represent the %% percentage (n/100)%*/
#else
		set_state (ORDER, scale, 1, 2, (long *)&i); 
		fix1 = (double)tdefs[ORDER_LINE].base / (double)10000;
#endif		
		rowcnt = (int)(fix1 * scale * refresh);
		if (step > 0)
			{
			/* 
			 * adjust RNG for any prior update generation
			 */
			sd_order(0, rowcnt * (step - 1));
			sd_line(0, rowcnt * (step - 1));
			upd_num = step - 1;
			}
		else
			upd_num = 0;

		while (upd_num < updates)
			{
			if (verbose > 0)
#ifdef SSBM
				fprintf (stderr,
				"Generating update pair #%d for %s [pid: %d]",
				upd_num + 1, tdefs[LINE].comment, DSS_PROC);
#else
				fprintf (stderr,
				"Generating update pair #%d for %s [pid: %d]",
				upd_num + 1, tdefs[ORDER_LINE].comment, DSS_PROC);

#endif
			insert_orders_segment=0;
			insert_lineitem_segment=0;
			delete_segment=0;
			minrow = upd_num * rowcnt + 1;
#ifdef SSBM
			gen_tbl (LINE, minrow, rowcnt, upd_num + 1);
#else
			gen_tbl (ORDER_LINE, minrow, rowcnt, upd_num + 1);
#endif
			if (verbose > 0)
				fprintf (stderr, "done.\n");
#ifdef SSBM
			pr_drange (LINE, minrow, rowcnt, upd_num + 1);
#else
			pr_drange (ORDER_LINE, minrow, rowcnt, upd_num + 1);
#endif
			upd_num++;
			}

		exit (0);
		}
	
	/**
	** actual data generation section starts here
	**/
/*
 * open database connection or set all the file names, as appropriate
 */
	if (direct)
		prep_direct ((db_name) ? db_name : DBNAME);
	else if (fnames)
		for (i = PART; i <= REGION; i++)
		{
			if (table & (1 << i))
				if (set_files (i, -1))
				{
					fprintf (stderr, "Load aborted!\n");
					exit (1);
				}
		}
		
/*
 * traverse the tables, invoking the appropriate data generation routine for any to be built
 */
	for (i = PART; i <= REGION; i++)
		if (table & (1 << i))
		{
			if (children > 1 && i < NATION)
				if (step >= 0)
				{
					if (validate)
					{
						INTERNAL_ERROR("Cannot validate parallel data generation");
					}
					else
						partial (i, step);
				}
#ifdef DOS
				else
				{
					fprintf (stderr,
						"Parallel load is not supported on your platform.\n");
					exit (1);
				}
#else
				else
				{
					if (validate)
					{
						INTERNAL_ERROR("Cannot validate parallel data generation");
					}
					else
						pload (i);
				}
#endif /* DOS */
				else
				{
					minrow = 1;
					if (i < NATION)
						rowcnt = tdefs[i].base * scale;
					else
						rowcnt = tdefs[i].base;
#ifdef SSBM
					if(i==PART){
					    rowcnt = tdefs[i].base * (floor(1+log((double)(scale))/(log(2))));
					}
					if(i==DATE){
					    rowcnt = tdefs[i].base;
					}
#endif
					if (verbose > 0)
						fprintf (stderr, "%s data for %s [pid: %ld]",
						(validate)?"Validating":"Generating", tdefs[i].comment, DSS_PROC);
					gen_tbl (i, minrow, rowcnt, upd_num);
					if (verbose > 0)
						fprintf (stderr, "done.\n");
				}
				if (validate)
					printf("Validation checksum for %s at %d GB: %0x\n", 
						 tdefs[i].name, scale, tdefs[i].vtotal);
		}
			
		if (direct)
			close_direct ();
			
		return (0);
}











