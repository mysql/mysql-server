/*
 * Analyze the profile file (cmon.out) written out by the dbug
 * routines with profiling enabled.
 *
 * Copyright June 1987, Binayak Banerjee
 * All rights reserved.
 *
 * This program may be freely distributed under the same terms and
 * conditions as Fred Fish's Dbug package.
 *
 * Compile with -- cc -O -s -o %s analyze.c
 *
 * Analyze will read an trace file created by the dbug package
 * (when run with traceing enabled).  It will then produce a
 * summary on standard output listing the name of each traced
 * function, the number of times it was called, the percentage
 * of total calls, the time spent executing the function, the
 * proportion of the total time and the 'importance'.  The last
 * is a metric which is obtained by multiplying the proportions
 * of calls and the proportions of time for each function.  The
 * greater the importance, the more likely it is that a speedup
 * could be obtained by reducing the time taken by that function.
 *
 * Note that the timing values that you obtain are only rough
 * measures.  The overhead of the dbug package is included
 * within.  However, there is no need to link in special profiled
 * libraries and the like.
 *
 * CHANGES:
 *
 *	2-Mar-89: fnf
 *	Changes to support tracking of stack usage.  This required
 *	reordering the fields in the profile log file to make
 *	parsing of different record types easier.  Corresponding
 *	changes made in dbug runtime library.  Also used this
 *	opportunity to reformat the code more to my liking (my
 *	apologies to Binayak Banerjee for "uglifying" his code).
 *
 *	24-Jul-87: fnf
 *	Because I tend to use functions names like
 *	"ExternalFunctionDoingSomething", I've rearranged the
 *	printout to put the function name last in each line, so
 *	long names don't screw up the formatting unless they are
 *	*very* long and wrap around the screen width...
 *
 *	24-Jul-87: fnf
 *	Modified to put out table very similar to Unix profiler
 *	by default, but also puts out original verbose table
 *	if invoked with -v flag.
 */

#include <m_string.h>
#include <my_thread.h>

static char *my_name;
static int verbose;

/*
 * Structure of the stack.
 */

#define PRO_FILE "dbugmon.out" /* Default output file name */
#define STACKSIZ 100           /* Maximum function nesting */
#define MAXPROCS 10000         /* Maximum number of function calls */

#ifdef BSD
#include <sysexits.h>
#else
#define EX_SOFTWARE 1
#define EX_DATAERR 1
#define EX_USAGE 1
#define EX_OSERR 1
#define EX_IOERR 1
#ifndef EX_OK
#define EX_OK 0
#endif
#endif

#define __MERF_OO_ "%s: Malloc Failed in %s: %d\n"

#define MALLOC(Ptr, Num, Typ)                                   \
  do /* Malloc w/error checking & exit */                       \
    if (!(Ptr = (Typ *)malloc((Num) * (sizeof(Typ))))) {        \
      fprintf(stderr, __MERF_OO_, my_name, __FILE__, __LINE__); \
      exit(EX_OSERR);                                           \
    }                                                           \
  while (0)

#define Malloc(Ptr, Num, Typ)                                   \
  do /* Weaker version of above */                              \
    if (!(Ptr = (Typ *)malloc((Num) * (sizeof(Typ)))))          \
      fprintf(stderr, __MERF_OO_, my_name, __FILE__, __LINE__); \
  while (0)

#define FILEOPEN(Fp, Fn, Mod)                                 \
  do /* File open with error exit */                          \
    if (!(Fp = fopen(Fn, Mod))) {                             \
      fprintf(stderr, "%s: Couldn't open %s\n", my_name, Fn); \
      exit(EX_IOERR);                                         \
    }                                                         \
  while (0)

#define Fileopen(Fp, Fn, Mod)                                 \
  do /* Weaker version of above */                            \
    if (!(Fp = fopen(Fn, Mod)))                               \
      fprintf(stderr, "%s: Couldn't open %s\n", my_name, Fn); \
  while (0)

struct stack_t {
  unsigned int pos;       /* which function? */
  unsigned long time;     /* Time that this was entered */
  unsigned long children; /* Time spent in called funcs */
};

static struct stack_t fn_stack[STACKSIZ + 1];

static unsigned int stacktop = 0; /* Lowest stack position is a dummy */

static unsigned long tot_time = 0;
static unsigned long tot_calls = 0;
static unsigned long highstack = 0;
static unsigned long lowstack = (ulong)~0;

/*
 * top() returns a pointer to the top item on the stack.
 * (was a function, now a macro)
 */

#define top() &fn_stack[stacktop]

/*
 * Push - Push the given record on the stack.
 */

void push(name_pos, time_entered) unsigned int name_pos;
unsigned long time_entered;
{
  struct stack_t *t;

  DBUG_ENTER("push");
  if (++stacktop > STACKSIZ) {
    fprintf(DBUG_FILE, "%s: stack overflow (%s:%d)\n", my_name, __FILE__,
            __LINE__);
    exit(EX_SOFTWARE);
  }
  DBUG_PRINT("push", ("%d %ld", name_pos, time_entered));
  t = &fn_stack[stacktop];
  t->pos = name_pos;
  t->time = time_entered;
  t->children = 0;
  DBUG_VOID_RETURN;
}

/*
 * Pop - pop the top item off the stack, assigning the field values
 * to the arguments. Returns 0 on stack underflow, or on popping first
 * item off stack.
 */

unsigned int pop(name_pos, time_entered, child_time) unsigned int *name_pos;
unsigned long *time_entered;
unsigned long *child_time;
{
  struct stack_t *temp;
  unsigned int rtnval;

  DBUG_ENTER("pop");

  if (stacktop < 1) {
    rtnval = 0;
  } else {
    temp = &fn_stack[stacktop];
    *name_pos = temp->pos;
    *time_entered = temp->time;
    *child_time = temp->children;
    DBUG_PRINT("pop", ("%d %lu %lu", *name_pos, *time_entered, *child_time));
    rtnval = stacktop--;
  }
  DBUG_RETURN(rtnval);
}

/*
 * We keep the function info in another array (serves as a simple
 * symbol table)
 */

struct module_t {
  char *name;
  unsigned long m_time;
  unsigned long m_calls;
  unsigned long m_stkuse;
};

static struct module_t modules[MAXPROCS];

/*
 * We keep a binary search tree in order to look up function names
 * quickly (and sort them at the end.
 */

struct bnode {
  unsigned int lchild; /* Index of left subtree */
  unsigned int rchild; /* Index of right subtree */
  unsigned int pos;    /* Index of module_name entry */
};

static struct bnode s_table[MAXPROCS];

static unsigned int n_items = 0; /* No. of items in the array so far */

/*
 * Need a function to allocate space for a string and squirrel it away.
 */

char *strsave(s) char *s;
{
  char *retval;
  unsigned int len;

  DBUG_ENTER("strsave");
  DBUG_PRINT("strsave", ("%s", s));
  if (!s || (len = strlen(s)) == 0) {
    DBUG_RETURN(0);
  }
  MALLOC(retval, ++len, char);
  strcpy(retval, s);
  DBUG_RETURN(retval);
}

/*
 * add() - adds m_name to the table (if not already there), and returns
 * the index of its location in the table.  Checks s_table (which is a
 * binary search tree) to see whether or not it should be added.
 */

unsigned int add(m_name) char *m_name;
{
  unsigned int ind = 0;
  int cmp;

  DBUG_ENTER("add");
  if (n_items == 0) { /* First item to be added */
    s_table[0].pos = ind;
    s_table[0].lchild = s_table[0].rchild = MAXPROCS;
  addit:
    modules[n_items].name = strsave(m_name);
    modules[n_items].m_time = 0;
    modules[n_items].m_calls = 0;
    modules[n_items].m_stkuse = 0;
    DBUG_RETURN(n_items++);
  }
  while ((cmp = strcmp(m_name, modules[ind].name))) {
    if (cmp < 0) { /* In left subtree */
      if (s_table[ind].lchild == MAXPROCS) {
        /* Add as left child */
        if (n_items >= MAXPROCS) {
          fprintf(DBUG_FILE, "%s: Too many functions being profiled\n",
                  my_name);
          exit(EX_SOFTWARE);
        }
        s_table[n_items].pos = s_table[ind].lchild = n_items;
        s_table[n_items].lchild = s_table[n_items].rchild = MAXPROCS;
#ifdef notdef
        modules[n_items].name = strsave(m_name);
        modules[n_items].m_time = modules[n_items].m_calls = 0;
        DBUG_RETURN(n_items++);
#else
        goto addit;
#endif
      }
      ind = s_table[ind].lchild; /* else traverse l-tree */
    } else {
      if (s_table[ind].rchild == MAXPROCS) {
        /* Add as right child */
        if (n_items >= MAXPROCS) {
          fprintf(DBUG_FILE, "%s: Too many functions being profiled\n",
                  my_name);
          exit(EX_SOFTWARE);
        }
        s_table[n_items].pos = s_table[ind].rchild = n_items;
        s_table[n_items].lchild = s_table[n_items].rchild = MAXPROCS;
#ifdef notdef
        modules[n_items].name = strsave(m_name);
        modules[n_items].m_time = modules[n_items].m_calls = 0;
        DBUG_RETURN(n_items++);
#else
        goto addit;
#endif
      }
      ind = s_table[ind].rchild; /* else traverse r-tree */
    }
  }
  DBUG_RETURN(ind);
}

/*
 * process() - process the input file, filling in the modules table.
 */

void process(inf) FILE *inf;
{
  char buf[BUFSIZ];
  char fn_name[64]; /* Max length of fn_name */
  unsigned long fn_time;
  unsigned long fn_sbot;
  unsigned long fn_ssz;
  unsigned long lastuse;
  unsigned int pos;
  unsigned long local_time;
  unsigned int oldpos;
  unsigned long oldtime;
  unsigned long oldchild;
  struct stack_t *t;

  DBUG_ENTER("process");
  while (fgets(buf, BUFSIZ, inf) != NULL) {
    switch (buf[0]) {
      case 'E':
        sscanf(buf + 2, "%ld %64s", &fn_time, fn_name);
        DBUG_PRINT("erec", ("%ld %s", fn_time, fn_name));
        pos = add(fn_name);
        push(pos, fn_time);
        break;
      case 'X':
        sscanf(buf + 2, "%ld %64s", &fn_time, fn_name);
        DBUG_PRINT("xrec", ("%ld %s", fn_time, fn_name));
        pos = add(fn_name);
        /*
         * An exited function implies that all stacked
         * functions are also exited, until the matching
         * function is found on the stack.
         */
        while (pop(&oldpos, &oldtime, &oldchild)) {
          DBUG_PRINT("popped", ("%lu %lu", oldtime, oldchild));
          local_time = fn_time - oldtime;
          t = top();
          t->children += local_time;
          DBUG_PRINT("update", ("%s", modules[t->pos].name));
          DBUG_PRINT("update", ("%lu", t->children));
          local_time -= oldchild;
          modules[oldpos].m_time += local_time;
          modules[oldpos].m_calls++;
          tot_time += local_time;
          tot_calls++;
          if (pos == oldpos) {
            goto next_line; /* Should be a break2 */
          }
        }
        /*
         * Assume that item seen started at time 0.
         * (True for function main).  But initialize
         * it so that it works the next time too.
         */
        t = top();
        local_time = fn_time - t->time - t->children;
        t->time = fn_time;
        t->children = 0;
        modules[pos].m_time += local_time;
        modules[pos].m_calls++;
        tot_time += local_time;
        tot_calls++;
        break;
      case 'S':
        sscanf(buf + 2, "%lx %lx %64s", &fn_sbot, &fn_ssz, fn_name);
        DBUG_PRINT("srec", ("%lx %lx %s", fn_sbot, fn_ssz, fn_name));
        pos = add(fn_name);
        lastuse = modules[pos].m_stkuse;
#if 0
      /*
       *  Needs further thought.  Stack use is determined by
       *  difference in stack between two functions with DBUG_ENTER
       *  macros.  If A calls B calls C, where A and C have the
       *  macros, and B doesn't, then B's stack use will be lumped
       *  in with either A's or C's.  If somewhere else A calls
       *  C directly, the stack use will seem to change.  Just
       *  take the biggest for now...
       */
      if (lastuse > 0 && lastuse != fn_ssz) {
	fprintf (stderr,
		 "warning - %s stack use changed (%lx to %lx)\n",
		 fn_name, lastuse, fn_ssz);
      }
#endif
        if (fn_ssz > lastuse) {
          modules[pos].m_stkuse = fn_ssz;
        }
        if (fn_sbot > highstack) {
          highstack = fn_sbot;
        } else if (fn_sbot < lowstack) {
          lowstack = fn_sbot;
        }
        break;
      default:
        fprintf(stderr, "unknown record type '%c'\n", buf[0]);
        break;
    }
  next_line:;
  }

  /*
   * Now, we've hit eof.  If we still have stuff stacked, then we
   * assume that the user called exit, so give everything the exited
   * time of fn_time.
   */
  while (pop(&oldpos, &oldtime, &oldchild)) {
    local_time = fn_time - oldtime;
    t = top();
    t->children += local_time;
    local_time -= oldchild;
    modules[oldpos].m_time += local_time;
    modules[oldpos].m_calls++;
    tot_time += local_time;
    tot_calls++;
  }
  DBUG_VOID_RETURN;
}

/*
 * out_header () -- print out the header of the report.
 */

void out_header(outf) FILE *outf;
{
  DBUG_ENTER("out_header");
  if (verbose) {
    fprintf(outf, "Profile of Execution\n");
    fprintf(outf, "Execution times are in milliseconds\n\n");
    fprintf(outf, "    Calls\t\t\t    Time\n");
    fprintf(outf, "    -----\t\t\t    ----\n");
    fprintf(outf, "Times\tPercentage\tTime Spent\tPercentage\n");
    fprintf(
        outf,
        "Called\tof total\tin Function\tof total    Importance\tFunction\n");
    fprintf(outf,
            "======\t==========\t===========\t==========  "
            "==========\t========\t\n");
  } else {
    fprintf(outf, "%ld bytes of stack used, from %lx down to %lx\n\n",
            highstack - lowstack, highstack, lowstack);
    fprintf(
        outf,
        "   %%time     sec   #call ms/call  %%calls  weight   stack  name\n");
  }
  DBUG_VOID_RETURN;
}

/*
 * out_trailer () - writes out the summary line of the report.
 */

void out_trailer(outf, sum_calls, sum_time) FILE *outf;
unsigned long int sum_calls, sum_time;
{
  DBUG_ENTER("out_trailer");
  if (verbose) {
    fprintf(outf, "======\t==========\t===========\t==========\t========\n");
    fprintf(outf, "%6ld\t%10.2f\t%11ld\t%10.2f\t\t%-15s\n", sum_calls, 100.0,
            sum_time, 100.0, "Totals");
  }
  DBUG_VOID_RETURN;
}

/*
 * out_item () - prints out the output line for a single entry,
 * and sets the calls and time fields appropriately.
 */

void out_item(outf, m, called, timed) FILE *outf;
struct module_t *m;
unsigned long int *called, *timed;
{
  char *name = m->name;
  unsigned int calls = m->m_calls;
  unsigned long local_time = m->m_time;
  unsigned long stkuse = m->m_stkuse;
  unsigned int import;
  double per_time = 0.0;
  double per_calls = 0.0;
  double ms_per_call, local_ftime;

  DBUG_ENTER("out_item");

  if (tot_time > 0) {
    per_time = (double)(local_time * 100) / (double)tot_time;
  }
  if (tot_calls > 0) {
    per_calls = (double)(calls * 100) / (double)tot_calls;
  }
  import = (unsigned int)(per_time * per_calls);

  if (verbose) {
    fprintf(outf, "%6d\t%10.2f\t%11ld\t%10.2f  %10d\t%-15s\n", calls, per_calls,
            local_time, per_time, import, name);
  } else {
    ms_per_call = local_time;
    ms_per_call /= calls;
    local_ftime = local_time;
    local_ftime /= 1000;
    fprintf(outf, "%8.2f%8.3f%8u%8.3f%8.2f%8u%8lu  %-s\n", per_time,
            local_ftime, calls, ms_per_call, per_calls, import, stkuse, name);
  }
  *called = calls;
  *timed = local_time;
  DBUG_VOID_RETURN;
}

/*
 * out_body (outf, root,s_calls,s_time) -- Performs an inorder traversal
 * on the binary search tree (root).  Calls out_item to actually print
 * the item out.
 */

void out_body(outf, root, s_calls, s_time) FILE *outf;
unsigned int root;
unsigned long int *s_calls, *s_time;
{
  unsigned long int calls, local_time;

  DBUG_ENTER("out_body");
  DBUG_PRINT("out_body", ("%lu,%lu", *s_calls, *s_time));
  if (root == MAXPROCS) {
    DBUG_PRINT("out_body", ("%lu,%lu", *s_calls, *s_time));
  } else {
    while (root != MAXPROCS) {
      out_body(outf, s_table[root].lchild, s_calls, s_time);
      out_item(outf, &modules[s_table[root].pos], &calls, &local_time);
      DBUG_PRINT("out_body", ("-- %lu -- %lu --", calls, local_time));
      *s_calls += calls;
      *s_time += local_time;
      root = s_table[root].rchild;
    }
    DBUG_PRINT("out_body", ("%lu,%lu", *s_calls, *s_time));
  }
  DBUG_VOID_RETURN;
}

/*
 * output () - print out a nice sorted output report on outf.
 */

void output(outf) FILE *outf;
{
  unsigned long int sum_calls = 0;
  unsigned long int sum_time = 0;

  DBUG_ENTER("output");
  if (n_items == 0) {
    fprintf(outf, "%s: No functions to trace\n", my_name);
    exit(EX_DATAERR);
  }
  out_header(outf);
  out_body(outf, 0, &sum_calls, &sum_time);
  out_trailer(outf, sum_calls, sum_time);
  DBUG_VOID_RETURN;
}

#define usage() fprintf(DBUG_FILE, "Usage: %s [-v] [prof-file]\n", my_name)

extern int optind;
extern char *optarg;

int main(int argc, char **argv) {
  int c;
  int badflg = 0;
  FILE *infile;
  FILE *outfile = {stdout};

  my_thread_global_init();
  {
    DBUG_ENTER("main");
    DBUG_PROCESS(argv[0]);
    my_name = argv[0];
    while ((c = getopt(argc, argv, "#:v")) != EOF) {
      switch (c) {
        case '#': /* Debugging Macro enable */
          DBUG_PUSH(optarg);
          break;
        case 'v': /* Verbose mode */
          verbose++;
          break;
        default:
          badflg++;
          break;
      }
    }
    if (badflg) {
      usage();
      DBUG_RETURN(EX_USAGE);
    }
    if (optind < argc) {
      FILEOPEN(infile, argv[optind], "r");
    } else {
      FILEOPEN(infile, PRO_FILE, "r");
    }
    process(infile);
    output(outfile);
    DBUG_RETURN(EX_OK);
  }
}
