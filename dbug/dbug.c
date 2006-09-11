/******************************************************************************
 *                                                                            *
 *                                 N O T I C E                                *
 *                                                                            *
 *                    Copyright Abandoned, 1987, Fred Fish                    *
 *                                                                            *
 *                                                                            *
 *      This previously copyrighted work has been placed into the  public     *
 *      domain  by  the  author  and  may be freely used for any purpose,     *
 *      private or commercial.                                                *
 *                                                                            *
 *      Because of the number of inquiries I was receiving about the  use     *
 *      of this product in commercially developed works I have decided to     *
 *      simply make it public domain to further its unrestricted use.   I     *
 *      specifically  would  be  most happy to see this material become a     *
 *      part of the standard Unix distributions by AT&T and the  Berkeley     *
 *      Computer  Science  Research Group, and a standard part of the GNU     *
 *      system from the Free Software Foundation.                             *
 *                                                                            *
 *      I would appreciate it, as a courtesy, if this notice is  left  in     *
 *      all copies and derivative works.  Thank you.                          *
 *                                                                            *
 *      The author makes no warranty of any kind  with  respect  to  this     *
 *      product  and  explicitly disclaims any implied warranties of mer-     *
 *      chantability or fitness for any particular purpose.                   *
 *                                                                            *
 ******************************************************************************
 */

/*
 *  FILE
 *
 *      dbug.c   runtime support routines for dbug package
 *
 *  SCCS
 *
 *      @(#)dbug.c      1.25    7/25/89
 *
 *  DESCRIPTION
 *
 *      These are the runtime support routines for the dbug package.
 *      The dbug package has two main components; the user include
 *      file containing various macro definitions, and the runtime
 *      support routines which are called from the macro expansions.
 *
 *      Externally visible functions in the runtime support module
 *      use the naming convention pattern "_db_xx...xx_", thus
 *      they are unlikely to collide with user defined function names.
 *
 *  AUTHOR(S)
 *
 *      Fred Fish               (base code)
 *      Enhanced Software Technologies, Tempe, AZ
 *      asuvax!mcdphx!estinc!fnf
 *
 *      Binayak Banerjee        (profiling enhancements)
 *      seismo!bpa!sjuvax!bbanerje
 *
 *      Michael Widenius:
 *      DBUG_DUMP       - To dump a block of memory.
 *      PUSH_FLAG "O"   - To be used insted of "o" if we
 *                        want flushing after each write
 *      PUSH_FLAG "A"   - as 'O', but we will append to the out file instead
 *                        of creating a new one.
 *      Check of malloc on entry/exit (option "S")
 *
 *      DBUG_EXECUTE_IF
 *      incremental mode (-#+t:-d,info ...)
 *      DBUG_SET, _db_explain_
 *      thread-local settings
 *
 */


#include <my_global.h>
#include <m_string.h>
#include <errno.h>
#if defined(MSDOS) || defined(__WIN__)
#include <process.h>
#endif


#ifndef DBUG_OFF


/*
 *            Manifest constants which may be "tuned" if desired.
 */

#define PRINTBUF              1024    /* Print buffer size */
#define INDENT                2       /* Indentation per trace level */
#define MAXDEPTH              200     /* Maximum trace depth default */

/*
 *      The following flags are used to determine which
 *      capabilities the user has enabled with the settings
 *      push macro.
 */

#define TRACE_ON        000001  /* Trace enabled */
#define DEBUG_ON        000002  /* Debug enabled */
#define FILE_ON         000004  /* File name print enabled */
#define LINE_ON         000010  /* Line number print enabled */
#define DEPTH_ON        000020  /* Function nest level print enabled */
#define PROCESS_ON      000040  /* Process name print enabled */
#define NUMBER_ON       000100  /* Number each line of output */
#define PROFILE_ON      000200  /* Print out profiling code */
#define PID_ON          000400  /* Identify each line with process id */
#define TIMESTAMP_ON    001000  /* timestamp every line of output */
#define SANITY_CHECK_ON 002000  /* Check safemalloc on DBUG_ENTER */
#define FLUSH_ON_WRITE  004000  /* Flush on every write */
#define OPEN_APPEND     010000  /* Open for append      */

#define TRACING (cs->stack->flags & TRACE_ON)
#define DEBUGGING (cs->stack->flags & DEBUG_ON)
#define PROFILING (cs->stack->flags & PROFILE_ON)

/*
 *      Typedefs to make things more obvious.
 */

#ifndef __WIN__
typedef int BOOLEAN;
#else
#define BOOLEAN BOOL
#endif

/*
 *      Make it easy to change storage classes if necessary.
 */

#define IMPORT extern           /* Names defined externally */
#define EXPORT                  /* Allocated here, available globally */
#define AUTO auto               /* Names to be allocated on stack */
#define REGISTER register       /* Names to be placed in registers */

/*
 * The default file for profiling.  Could also add another flag
 * (G?) which allowed the user to specify this.
 *
 * If the automatic variables get allocated on the stack in
 * reverse order from their declarations, then define AUTOS_REVERSE.
 * This is used by the code that keeps track of stack usage.  For
 * forward allocation, the difference in the dbug frame pointers
 * represents stack used by the callee function.  For reverse allocation,
 * the difference represents stack used by the caller function.
 *
 */

#define PROF_FILE       "dbugmon.out"
#define PROF_EFMT       "E\t%ld\t%s\n"
#define PROF_SFMT       "S\t%lx\t%lx\t%s\n"
#define PROF_XFMT       "X\t%ld\t%s\n"

#ifdef M_I386           /* predefined by xenix 386 compiler */
#define AUTOS_REVERSE 1
#endif

/*
 *      Externally supplied functions.
 */

#ifndef HAVE_PERROR
static void perror();          /* Fake system/library error print routine */
#endif

IMPORT int _sanity(const char *file,uint line); /* safemalloc sanity checker */

/*
 *      The user may specify a list of functions to trace or
 *      debug.  These lists are kept in a linear linked list,
 *      a very simple implementation.
 */

struct link {
    struct link *next_link;   /* Pointer to the next link */
    char   str[1];        /* Pointer to link's contents */
};

/*
 *      Debugging settings can be pushed or popped off of a
 *      stack which is implemented as a linked list.  Note
 *      that the head of the list is the current settings and the
 *      stack is pushed by adding a new settings to the head of the
 *      list or popped by removing the first link.
 *
 *      Note: if out_file is NULL, the other fields are not initialized at all!
 */

struct settings {
  int flags;                    /* Current settings flags */
  int maxdepth;                 /* Current maximum trace depth */
  uint delay;                   /* Delay after each output line */
  int sub_level;                /* Sub this from code_state->level */
  FILE *out_file;               /* Current output stream */
  FILE *prof_file;              /* Current profiling stream */
  char name[FN_REFLEN];         /* Name of output file */
  struct link *functions;       /* List of functions */
  struct link *p_functions;     /* List of profiled functions */
  struct link *keywords;        /* List of debug keywords */
  struct link *processes;       /* List of process names */
  struct settings *next;        /* Next settings in the list */
};

#define is_shared(S, V) ((S)->next && (S)->next->V == (S)->V)

/*
 *      Local variables not seen by user.
 */


static BOOLEAN init_done= FALSE; /* Set to TRUE when initialization done */
static struct settings init_settings;
static const char *db_process= 0;/* Pointer to process name; argv[0] */

typedef struct _db_code_state_ {
  const char *process;          /* Pointer to process name; usually argv[0] */
  const char *func;             /* Name of current user function */
  const char *file;             /* Name of current user file */
  char **framep;                /* Pointer to current frame */
  struct settings *stack;       /* debugging settings */
  const char *jmpfunc;          /* Remember current function for setjmp */
  const char *jmpfile;          /* Remember current file for setjmp */
  int lineno;                   /* Current debugger output line number */
  int level;                    /* Current function nesting level */
  int jmplevel;                 /* Remember nesting level at setjmp() */

/*
 *      The following variables are used to hold the state information
 *      between the call to _db_pargs_() and _db_doprnt_(), during
 *      expansion of the DBUG_PRINT macro.  This is the only macro
 *      that currently uses these variables.
 *
 *      These variables are currently used only by _db_pargs_() and
 *      _db_doprnt_().
 */

  uint u_line;                  /* User source code line number */
  int  locked;                  /* If locked with _db_lock_file_ */
  const char *u_keyword;        /* Keyword for current macro */
} CODE_STATE;

/*
  The test below is so we could call functions with DBUG_ENTER before
  my_thread_init().
*/
#define get_code_state_or_return if (!cs && !((cs=code_state()))) return

        /* Handling lists */
static struct link *ListAdd(struct link *, const char *, const char *);
static struct link *ListDel(struct link *, const char *, const char *);
static struct link *ListCopy(struct link *);
static void FreeList(struct link *linkp);

        /* OpenClose debug output stream */
static void DBUGOpenFile(CODE_STATE *,const char *, const char *, int);
static void DBUGCloseFile(CODE_STATE *cs, FILE *fp);
        /* Push current debug settings */
static void PushState(CODE_STATE *cs);
	/* Free memory associated with debug state. */
static void FreeState (CODE_STATE *cs, struct settings *state);
        /* Test for tracing enabled */
static BOOLEAN DoTrace(CODE_STATE *cs);

        /* Test to see if file is writable */
#if !(!defined(HAVE_ACCESS) || defined(MSDOS))
static BOOLEAN Writable(char *pathname);
        /* Change file owner and group */
static void ChangeOwner(CODE_STATE *cs, char *pathname);
        /* Allocate memory for runtime support */
#endif

static void DoPrefix(CODE_STATE *cs, uint line);

static char *DbugMalloc(size_t size);
static const char *BaseName(const char *pathname);
static void Indent(CODE_STATE *cs, int indent);
static BOOLEAN InList(struct link *linkp,const char *cp);
static void dbug_flush(CODE_STATE *);
static void DbugExit(const char *why);
static const char *DbugStrTok(const char *s);

#ifndef THREAD
        /* Open profile output stream */
static FILE *OpenProfile(CODE_STATE *cs, const char *name);
        /* Profile if asked for it */
static BOOLEAN DoProfile(CODE_STATE *);
        /* Return current user time (ms) */
static unsigned long Clock(void);
#endif

/*
 *      Miscellaneous printf format strings.
 */

#define ERR_MISSING_RETURN "%s: missing DBUG_RETURN or DBUG_VOID_RETURN macro in function \"%s\"\n"
#define ERR_OPEN "%s: can't open debug output stream \"%s\": "
#define ERR_CLOSE "%s: can't close debug file: "
#define ERR_ABORT "%s: debugger aborting because %s\n"
#define ERR_CHOWN "%s: can't change owner/group of \"%s\": "

/*
 *      Macros and defines for testing file accessibility under UNIX and MSDOS.
 */

#undef EXISTS
#if !defined(HAVE_ACCESS) || defined(MSDOS)
#define EXISTS(pathname) (FALSE)        /* Assume no existance */
#define Writable(name) (TRUE)
#else
#define EXISTS(pathname)         (access(pathname, F_OK) == 0)
#define WRITABLE(pathname)       (access(pathname, W_OK) == 0)
#endif
#ifndef MSDOS
#define ChangeOwner(cs,name)
#endif


/*
** Macros to allow dbugging with threads
*/

#ifdef THREAD
#include <my_pthread.h>
pthread_mutex_t THR_LOCK_dbug;

static CODE_STATE *code_state(void)
{
  CODE_STATE *cs=0;
  struct st_my_thread_var *tmp;

  if (!init_done)
  {
    pthread_mutex_init(&THR_LOCK_dbug,MY_MUTEX_INIT_FAST);
    bzero(&init_settings, sizeof(init_settings));
    init_settings.out_file=stderr;
    init_settings.flags=OPEN_APPEND;
    init_done=TRUE;
  }

  if ((tmp=my_thread_var))
  {
    if (!(cs=(CODE_STATE *) tmp->dbug))
    {
      cs=(CODE_STATE*) DbugMalloc(sizeof(*cs));
      bzero((char*) cs,sizeof(*cs));
      cs->process= db_process ? db_process : "dbug";
      cs->func="?func";
      cs->file="?file";
      cs->stack=&init_settings;
      tmp->dbug=(gptr) cs;
    }
  }
  return cs;
}

#else /* !THREAD */

static CODE_STATE static_code_state=
{
  "dbug", "?func", "?file", NULL, &init_settings,
  NullS, NullS, 0,0,0,0,0,NullS
};

static CODE_STATE *code_state(void)
{
  if (!init_done)
  {
    bzero(&init_settings, sizeof(init_settings));
    init_settings.out_file=stderr;
    init_settings.flags=OPEN_APPEND;
    init_done=TRUE;
  }
  return &static_code_state;
}

#define pthread_mutex_lock(A) {}
#define pthread_mutex_unlock(A) {}
#endif

/*
 *      Translate some calls among different systems.
 */

#ifdef HAVE_SLEEP
/* sleep() wants seconds */
#define Delay(A) sleep(((uint) A)/10)
#else
#define Delay(A) (0)
#endif

/*
 *  FUNCTION
 *
 *      _db_process_       give the name to the current process/thread
 *
 *  SYNOPSIS
 *
 *      VOID _db_push_(name)
 *      char *name;
 *
 */

void _db_process_(const char *name)
{
  CODE_STATE *cs=0;

  if (!db_process)
    db_process= name;
  
  get_code_state_or_return;
  cs->process= name;
}

/*
 *  FUNCTION
 *
 *      _db_set_       set current debugger settings
 *
 *  SYNOPSIS
 *
 *      VOID _db_set_(control)
 *      char *control;
 *
 *  DESCRIPTION
 *
 *      Given pointer to a debug control string in "control",
 *      parses the control string, and sets
 *      up a current debug settings.
 *
 *      The debug control string is a sequence of colon separated fields
 *      as follows:
 *
 *              [+]<field_1>:<field_2>:...:<field_N>
 *
 *      Each field consists of a mandatory flag character followed by
 *      an optional "," and comma separated list of modifiers:
 *
 *              [sign]flag[,modifier,modifier,...,modifier]
 *
 *      See the manual for the list of supported signs, flags, and modifiers
 *
 *      For convenience, any leading "-#" is stripped off.
 *
 */

void _db_set_(CODE_STATE *cs, const char *control)
{
  const char *end;
  int rel=0;

  get_code_state_or_return;

  if (control[0] == '-' && control[1] == '#')
    control+=2;

  rel= control[0] == '+' || control[0] == '-';
  if (!rel || (!cs->stack->out_file && !cs->stack->next))
  {
    cs->stack->flags= 0;
    cs->stack->delay= 0;
    cs->stack->maxdepth= 0;
    cs->stack->sub_level= 0;
    cs->stack->out_file= stderr;
    cs->stack->prof_file= NULL;
    cs->stack->functions= NULL;
    cs->stack->p_functions= NULL;
    cs->stack->keywords= NULL;
    cs->stack->processes= NULL;
  }
  else if (!cs->stack->out_file)
  {
    cs->stack->flags= cs->stack->next->flags;
    cs->stack->delay= cs->stack->next->delay;
    cs->stack->maxdepth= cs->stack->next->maxdepth;
    cs->stack->sub_level= cs->stack->next->sub_level;
    strcpy(cs->stack->name, cs->stack->next->name);
    cs->stack->out_file= cs->stack->next->out_file;
    cs->stack->prof_file= cs->stack->next->prof_file;
    if (cs->stack->next == &init_settings)
    {
      /* never share with the global parent - it can change under your feet */
      cs->stack->functions= ListCopy(init_settings.functions);
      cs->stack->p_functions= ListCopy(init_settings.p_functions);
      cs->stack->keywords= ListCopy(init_settings.keywords);
      cs->stack->processes= ListCopy(init_settings.processes);
    }
    else
    {
      cs->stack->functions= cs->stack->next->functions;
      cs->stack->p_functions= cs->stack->next->p_functions;
      cs->stack->keywords= cs->stack->next->keywords;
      cs->stack->processes= cs->stack->next->processes;
    }
  }

  end= DbugStrTok(control);
  while (1)
  {
    int c, sign= (*control == '+') ? 1 : (*control == '-') ? -1 : 0;
    if (sign) control++;
    if (!rel) sign=0;
    c= *control++;
    if (*control == ',') control++;
    /* XXX when adding new cases here, don't forget _db_explain_ ! */
    switch (c) {
    case 'd':
      if (sign < 0 && control == end)
      {
        if (!is_shared(cs->stack, keywords))
          FreeList(cs->stack->keywords);
        cs->stack->keywords=NULL;
        cs->stack->flags &= ~DEBUG_ON;
        break;
      }
      if (rel && is_shared(cs->stack, keywords))
        cs->stack->keywords= ListCopy(cs->stack->keywords);
      if (sign < 0)
      {
        if (DEBUGGING)
          cs->stack->keywords= ListDel(cs->stack->keywords, control, end);
      break;
      }
      cs->stack->keywords= ListAdd(cs->stack->keywords, control, end);
      cs->stack->flags |= DEBUG_ON;
      break;
    case 'D':
      cs->stack->delay= atoi(control);
      break;
    case 'f':
      if (sign < 0 && control == end)
      {
        if (!is_shared(cs->stack,functions))
          FreeList(cs->stack->functions);
        cs->stack->functions=NULL;
        break;
      }
      if (rel && is_shared(cs->stack,functions))
        cs->stack->functions= ListCopy(cs->stack->functions);
      if (sign < 0)
        cs->stack->functions= ListDel(cs->stack->functions, control, end);
      else
        cs->stack->functions= ListAdd(cs->stack->functions, control, end);
      break;
    case 'F':
      if (sign < 0)
        cs->stack->flags &= ~FILE_ON;
      else
        cs->stack->flags |= FILE_ON;
      break;
    case 'i':
      if (sign < 0)
        cs->stack->flags &= ~PID_ON;
      else
        cs->stack->flags |= PID_ON;
      break;
#ifndef THREAD
    case 'g':
      if (OpenProfile(cs, PROF_FILE))
      {
        cs->stack->flags |= PROFILE_ON;
        cs->stack->p_functions= ListAdd(cs->stack->p_functions, control, end);
      }
      break;
#endif
    case 'L':
      if (sign < 0)
        cs->stack->flags &= ~LINE_ON;
      else
        cs->stack->flags |= LINE_ON;
      break;
    case 'n':
      if (sign < 0)
        cs->stack->flags &= ~DEPTH_ON;
      else
        cs->stack->flags |= DEPTH_ON;
      break;
    case 'N':
      if (sign < 0)
        cs->stack->flags &= ~NUMBER_ON;
      else
        cs->stack->flags |= NUMBER_ON;
      break;
    case 'A':
    case 'O':
      cs->stack->flags |= FLUSH_ON_WRITE;
      /* fall through */
    case 'a':
    case 'o':
      if (sign < 0)
      {
        if (!is_shared(cs->stack, out_file))
          DBUGCloseFile(cs, cs->stack->out_file);
        cs->stack->flags &= ~FLUSH_ON_WRITE;
        cs->stack->out_file= stderr;
        break;
      }
      if (c == 'a' || c == 'A')
        cs->stack->flags |= OPEN_APPEND;
      else
        cs->stack->flags &= ~OPEN_APPEND;
      if (control != end)
        DBUGOpenFile(cs, control, end, cs->stack->flags & OPEN_APPEND);
      else
        DBUGOpenFile(cs, "-",0,0);
      break;
    case 'p':
      if (sign < 0 && control == end)
      {
        if (!is_shared(cs->stack,processes))
          FreeList(cs->stack->processes);
        cs->stack->processes=NULL;
        break;
      }
      if (rel && is_shared(cs->stack, processes))
        cs->stack->processes= ListCopy(cs->stack->processes);
      if (sign < 0)
        cs->stack->processes= ListDel(cs->stack->processes, control, end);
      else
        cs->stack->processes= ListAdd(cs->stack->processes, control, end);
      break;
    case 'P':
      if (sign < 0)
        cs->stack->flags &= ~PROCESS_ON;
      else
        cs->stack->flags |= PROCESS_ON;
      break;
    case 'r':
      cs->stack->sub_level= cs->level;
      break;
    case 't':
      if (sign < 0)
      {
        if (control != end)
          cs->stack->maxdepth-= atoi(control);
        else
          cs->stack->maxdepth= 0;
      }
      else
      {
        if (control != end)
          cs->stack->maxdepth+= atoi(control);
        else
          cs->stack->maxdepth= MAXDEPTH;
      }
      if (cs->stack->maxdepth > 0)
        cs->stack->flags |= TRACE_ON;
      else
        cs->stack->flags &= ~TRACE_ON;
      break;
    case 'T':
      if (sign < 0)
        cs->stack->flags &= ~TIMESTAMP_ON;
      else
        cs->stack->flags |= TIMESTAMP_ON;
      break;
    case 'S':
      if (sign < 0)
        cs->stack->flags &= ~SANITY_CHECK_ON;
      else
        cs->stack->flags |= SANITY_CHECK_ON;
      break;
    }
    if (!*end)
      break;
    control=end+1;
    end= DbugStrTok(control);
  }
}


/*
 *  FUNCTION
 *
 *      _db_push_       push current debugger settings and set up new one
 *
 *  SYNOPSIS
 *
 *      VOID _db_push_(control)
 *      char *control;
 *
 *  DESCRIPTION
 *
 *      Given pointer to a debug control string in "control", pushes
 *      the current debug settings, parses the control string, and sets
 *      up a new debug settings with _db_set_()
 *
 */

void _db_push_(const char *control)
{
  CODE_STATE *cs=0;
  get_code_state_or_return;
  PushState(cs);
  _db_set_(cs, control);
}

/*
 *  FUNCTION
 *
 *      _db_set_init_       set initial debugger settings
 *
 *  SYNOPSIS
 *
 *      VOID _db_set_init_(control)
 *      char *control;
 *
 *  DESCRIPTION
 *      see _db_set_
 *
 */

void _db_set_init_(const char *control)
{
  CODE_STATE cs;
  bzero((char*) &cs,sizeof(cs));
  cs.stack=&init_settings;
  _db_set_(&cs, control);
}

/*
 *  FUNCTION
 *
 *      _db_pop_    pop the debug stack
 *
 *  DESCRIPTION
 *
 *      Pops the debug stack, returning the debug settings to its
 *      condition prior to the most recent _db_push_ invocation.
 *      Note that the pop will fail if it would remove the last
 *      valid settings from the stack.  This prevents user errors
 *      in the push/pop sequence from screwing up the debugger.
 *      Maybe there should be some kind of warning printed if the
 *      user tries to pop too many states.
 *
 */

void _db_pop_()
{
  struct settings *discard;
  CODE_STATE *cs=0;

  get_code_state_or_return;

  discard= cs->stack;
  if (discard->next != NULL)
  {
    cs->stack= discard->next;
    FreeState(cs, discard);
  }
}

/*
 *  FUNCTION
 *
 *      _db_explain_    generates 'control' string for the current settings
 *
 *  RETURN
 *      0 - ok
 *      1  - buffer too short, output truncated
 *
 */

/* helper macros */
#define char_to_buf(C)    do {                  \
        *buf++=(C);                             \
        if (buf >= end) goto overflow;          \
      } while (0)
#define str_to_buf(S)    do {                   \
        char_to_buf(',');                       \
        buf=strnmov(buf, (S), len+1);           \
        if (buf >= end) goto overflow;          \
      } while (0)
#define list_to_buf(l)  do {                    \
        struct link *listp=(l);                 \
        while (listp)                           \
        {                                       \
          str_to_buf(listp->str);               \
          listp=listp->next_link;               \
        }                                       \
      } while (0)
#define int_to_buf(i)  do {                     \
        char b[50];                             \
        int10_to_str((i), b, 10);               \
        str_to_buf(b);                          \
      } while (0)
#define colon_to_buf   do {                     \
        if (buf != start) char_to_buf(':');     \
      } while(0)
#define op_int_to_buf(C, val, def) do {         \
        if ((val) != (def))                     \
        {                                       \
          colon_to_buf;                         \
          char_to_buf((C));                     \
          int_to_buf(val);                      \
        }                                       \
      } while (0)
#define op_intf_to_buf(C, val, def, cond) do {  \
        if ((cond))                             \
        {                                       \
          colon_to_buf;                         \
          char_to_buf((C));                     \
          if ((val) != (def)) int_to_buf(val);  \
        }                                       \
      } while (0)
#define op_str_to_buf(C, val, cond) do {        \
        if ((cond))                             \
        {                                       \
          char *s=(val);                        \
          colon_to_buf;                         \
          char_to_buf((C));                     \
          if (*s) str_to_buf(s);                \
        }                                       \
      } while (0)
#define op_list_to_buf(C, val, cond) do {       \
        if ((cond))                             \
        {                                       \
          colon_to_buf;                         \
          char_to_buf((C));                     \
          list_to_buf(val);                     \
        }                                       \
      } while (0)
#define op_bool_to_buf(C, cond) do {            \
        if ((cond))                             \
        {                                       \
          colon_to_buf;                         \
          char_to_buf((C));                     \
        }                                       \
      } while (0)

int _db_explain_ (CODE_STATE *cs, char *buf, int len)
{
  char *start=buf, *end=buf+len-4;

  get_code_state_or_return *buf=0;

  op_list_to_buf('d', cs->stack->keywords, DEBUGGING);
  op_int_to_buf ('D', cs->stack->delay, 0);
  op_list_to_buf('f', cs->stack->functions, cs->stack->functions);
  op_bool_to_buf('F', cs->stack->flags & FILE_ON);
  op_bool_to_buf('i', cs->stack->flags & PID_ON);
  op_list_to_buf('g', cs->stack->p_functions, PROFILING);
  op_bool_to_buf('L', cs->stack->flags & LINE_ON);
  op_bool_to_buf('n', cs->stack->flags & DEPTH_ON);
  op_bool_to_buf('N', cs->stack->flags & NUMBER_ON);
  op_str_to_buf(
    ((cs->stack->flags & FLUSH_ON_WRITE ? 0 : 32) |
     (cs->stack->flags & OPEN_APPEND ? 'A' : 'O')),
    cs->stack->name, cs->stack->out_file != stderr);
  op_list_to_buf('p', cs->stack->processes, cs->stack->processes);
  op_bool_to_buf('P', cs->stack->flags & PROCESS_ON);
  op_bool_to_buf('r', cs->stack->sub_level != 0);
  op_intf_to_buf('t', cs->stack->maxdepth, MAXDEPTH, TRACING);
  op_bool_to_buf('T', cs->stack->flags & TIMESTAMP_ON);
  op_bool_to_buf('S', cs->stack->flags & SANITY_CHECK_ON);

  *buf= '\0';
  return 0;

overflow:
  *end++= '.';
  *end++= '.';
  *end++= '.';
  *end=   '\0';
  return 1;
}

#undef char_to_buf
#undef str_to_buf
#undef list_to_buf
#undef int_to_buf
#undef colon_to_buf
#undef op_int_to_buf
#undef op_intf_to_buf
#undef op_str_to_buf
#undef op_list_to_buf
#undef op_bool_to_buf

/*
 *  FUNCTION
 *
 *      _db_explain_init_       explain initial debugger settings
 *
 *  DESCRIPTION
 *      see _db_explain_
 */

int _db_explain_init_(char *buf, int len)
{
  CODE_STATE cs;
  bzero((char*) &cs,sizeof(cs));
  cs.stack=&init_settings;
  return _db_explain_(&cs, buf, len);
}

/*
 *  FUNCTION
 *
 *      _db_enter_    process entry point to user function
 *
 *  SYNOPSIS
 *
 *      VOID _db_enter_(_func_, _file_, _line_,
 *                       _sfunc_, _sfile_, _slevel_, _sframep_)
 *      char *_func_;           points to current function name
 *      char *_file_;           points to current file name
 *      int _line_;             called from source line number
 *      char **_sfunc_;         save previous _func_
 *      char **_sfile_;         save previous _file_
 *      int *_slevel_;          save previous nesting level
 *      char ***_sframep_;      save previous frame pointer
 *
 *  DESCRIPTION
 *
 *      Called at the beginning of each user function to tell
 *      the debugger that a new function has been entered.
 *      Note that the pointers to the previous user function
 *      name and previous user file name are stored on the
 *      caller's stack (this is why the ENTER macro must be
 *      the first "executable" code in a function, since it
 *      allocates these storage locations).  The previous nesting
 *      level is also stored on the callers stack for internal
 *      self consistency checks.
 *
 *      Also prints a trace line if tracing is enabled and
 *      increments the current function nesting depth.
 *
 *      Note that this mechanism allows the debugger to know
 *      what the current user function is at all times, without
 *      maintaining an internal stack for the function names.
 *
 */

void _db_enter_(const char *_func_, const char *_file_,
                uint _line_, const char **_sfunc_, const char **_sfile_,
                uint *_slevel_, char ***_sframep_ __attribute__((unused)))
{
  int save_errno=errno;
  CODE_STATE *cs=0;
  get_code_state_or_return;

  *_sfunc_= cs->func;
  *_sfile_= cs->file;
  cs->func=  _func_;
  cs->file=  _file_;
  *_slevel_=  ++cs->level;
#ifndef THREAD
  *_sframep_= cs->framep;
  cs->framep= (char **) _sframep_;
  if (DoProfile(cs))
  {
    long stackused;
    if (*cs->framep == NULL)
      stackused= 0;
    else
    {
      stackused= ((long)(*cs->framep)) - ((long)(cs->framep));
      stackused= stackused > 0 ? stackused : -stackused;
    }
    (void) fprintf(cs->stack->prof_file, PROF_EFMT , Clock(), cs->func);
#ifdef AUTOS_REVERSE
    (void) fprintf(cs->stack->prof_file, PROF_SFMT, cs->framep, stackused, *_sfunc_);
#else
    (void) fprintf(cs->stack->prof_file, PROF_SFMT, (ulong) cs->framep, stackused,
                    cs->func);
#endif
    (void) fflush(cs->stack->prof_file);
  }
#endif
  if (DoTrace(cs))
  {
    if (!cs->locked)
      pthread_mutex_lock(&THR_LOCK_dbug);
    DoPrefix(cs, _line_);
    Indent(cs, cs->level);
    (void) fprintf(cs->stack->out_file, ">%s\n", cs->func);
    dbug_flush(cs);                       /* This does a unlock */
  }
#ifdef SAFEMALLOC
  if (cs->stack->flags & SANITY_CHECK_ON)
    if (_sanity(_file_,_line_))               /* Check of safemalloc */
      cs->stack->flags &= ~SANITY_CHECK_ON;
#endif
  errno=save_errno;
}

/*
 *  FUNCTION
 *
 *      _db_return_    process exit from user function
 *
 *  SYNOPSIS
 *
 *      VOID _db_return_(_line_, _sfunc_, _sfile_, _slevel_)
 *      int _line_;             current source line number
 *      char **_sfunc_;         where previous _func_ is to be retrieved
 *      char **_sfile_;         where previous _file_ is to be retrieved
 *      int *_slevel_;          where previous level was stashed
 *
 *  DESCRIPTION
 *
 *      Called just before user function executes an explicit or implicit
 *      return.  Prints a trace line if trace is enabled, decrements
 *      the current nesting level, and restores the current function and
 *      file names from the defunct function's stack.
 *
 */

/* helper macro */
void _db_return_(uint _line_, const char **_sfunc_,
                 const char **_sfile_, uint *_slevel_)
{
  int save_errno=errno;
  CODE_STATE *cs=0;
  get_code_state_or_return;

  if (cs->level != (int) *_slevel_)
  {
    if (!cs->locked)
      pthread_mutex_lock(&THR_LOCK_dbug);
    (void) fprintf(cs->stack->out_file, ERR_MISSING_RETURN, cs->process,
                   cs->func);
    dbug_flush(cs);
  }
  else
  {
#ifdef SAFEMALLOC
    if (cs->stack->flags & SANITY_CHECK_ON)
    {
      if (_sanity(*_sfile_,_line_))
        cs->stack->flags &= ~SANITY_CHECK_ON;
    }
#endif
#ifndef THREAD
    if (DoProfile(cs))
      (void) fprintf(cs->stack->prof_file, PROF_XFMT, Clock(), cs->func);
#endif
    if (DoTrace(cs))
    {
      if (!cs->locked)
        pthread_mutex_lock(&THR_LOCK_dbug);
      DoPrefix(cs, _line_);
      Indent(cs, cs->level);
      (void) fprintf(cs->stack->out_file, "<%s\n", cs->func);
      dbug_flush(cs);
    }
  }
  cs->level= *_slevel_-1;
  cs->func= *_sfunc_;
  cs->file= *_sfile_;
#ifndef THREAD
  if (cs->framep != NULL)
    cs->framep= (char **) *cs->framep;
#endif
  errno=save_errno;
}


/*
 *  FUNCTION
 *
 *      _db_pargs_    log arguments for subsequent use by _db_doprnt_()
 *
 *  SYNOPSIS
 *
 *      VOID _db_pargs_(_line_, keyword)
 *      int _line_;
 *      char *keyword;
 *
 *  DESCRIPTION
 *
 *      The new universal printing macro DBUG_PRINT, which replaces
 *      all forms of the DBUG_N macros, needs two calls to runtime
 *      support routines.  The first, this function, remembers arguments
 *      that are used by the subsequent call to _db_doprnt_().
 *
 */

void _db_pargs_(uint _line_, const char *keyword)
{
  CODE_STATE *cs=0;
  get_code_state_or_return;
  cs->u_line= _line_;
  cs->u_keyword= (char*) keyword;
}


/*
 *  FUNCTION
 *
 *      _db_doprnt_    handle print of debug lines
 *
 *  SYNOPSIS
 *
 *      VOID _db_doprnt_(format, va_alist)
 *      char *format;
 *      va_dcl;
 *
 *  DESCRIPTION
 *
 *      When invoked via one of the DBUG macros, tests the current keyword
 *      set by calling _db_pargs_() to see if that macro has been selected
 *      for processing via the debugger control string, and if so, handles
 *      printing of the arguments via the format string.  The line number
 *      of the DBUG macro in the source is found in u_line.
 *
 *      Note that the format string SHOULD NOT include a terminating
 *      newline, this is supplied automatically.
 *
 */

#include <stdarg.h>

void _db_doprnt_(const char *format,...)
{
  va_list args;

  CODE_STATE *cs=0;
  get_code_state_or_return;

  va_start(args,format);

  if (_db_keyword_(cs, cs->u_keyword))
  {
    int save_errno=errno;
    if (!cs->locked)
      pthread_mutex_lock(&THR_LOCK_dbug);
    DoPrefix(cs, cs->u_line);
    if (TRACING)
      Indent(cs, cs->level + 1);
    else
      (void) fprintf(cs->stack->out_file, "%s: ", cs->func);
    (void) fprintf(cs->stack->out_file, "%s: ", cs->u_keyword);
    (void) vfprintf(cs->stack->out_file, format, args);
    (void) fputc('\n',cs->stack->out_file);
    dbug_flush(cs);
    errno=save_errno;
  }
  va_end(args);
}


/*
 *  FUNCTION
 *
 *            _db_dump_    dump a string in hex
 *
 *  SYNOPSIS
 *
 *            void _db_dump_(_line_,keyword,memory,length)
 *            int _line_;               current source line number
 *            char *keyword;
 *            char *memory;             Memory to print
 *            int length;               Bytes to print
 *
 *  DESCRIPTION
 *  Dump N characters in a binary array.
 *  Is used to examine corrputed memory or arrays.
 */

void _db_dump_(uint _line_, const char *keyword, const char *memory, uint length)
{
  int pos;
  char dbuff[90];

  CODE_STATE *cs=0;
  get_code_state_or_return;

  if (_db_keyword_(cs, (char*) keyword))
  {
    if (!cs->locked)
      pthread_mutex_lock(&THR_LOCK_dbug);
    DoPrefix(cs, _line_);
    if (TRACING)
    {
      Indent(cs, cs->level + 1);
      pos= min(max(cs->level-cs->stack->sub_level,0)*INDENT,80);
    }
    else
    {
      fprintf(cs->stack->out_file, "%s: ", cs->func);
    }
    sprintf(dbuff,"%s: Memory: 0x%lx  Bytes: (%d)\n",
            keyword,(ulong) memory, length);
    (void) fputs(dbuff,cs->stack->out_file);

    pos=0;
    while (length-- > 0)
    {
      uint tmp= *((unsigned char*) memory++);
      if ((pos+=3) >= 80)
      {
        fputc('\n',cs->stack->out_file);
        pos=3;
      }
      fputc(_dig_vec_upper[((tmp >> 4) & 15)], cs->stack->out_file);
      fputc(_dig_vec_upper[tmp & 15], cs->stack->out_file);
      fputc(' ',cs->stack->out_file);
    }
    (void) fputc('\n',cs->stack->out_file);
    dbug_flush(cs);
  }
}


/*
 *  FUNCTION
 *
 *      ListAdd    add to the list modifiers from debug control string
 *
 *  SYNOPSIS
 *
 *      static struct link *ListAdd(listp, ctlp, end)
 *      struct link *listp;
 *      char *ctlp;
 *      char *end;
 *
 *  DESCRIPTION
 *
 *      Given pointer to a comma separated list of strings in "cltp",
 *      parses the list, and adds it to listp, returning a pointer
 *      to the new list
 *
 *      Note that since each link is added at the head of the list,
 *      the final list will be in "reverse order", which is not
 *      significant for our usage here.
 *
 */

static struct link *ListAdd(struct link *head,
                             const char *ctlp, const char *end)
{
  const char *start;
  struct link *new_malloc;
  int len;

  while (ctlp < end)
  {
    start= ctlp;
    while (ctlp < end && *ctlp != ',')
      ctlp++;
    len=ctlp-start;
    new_malloc= (struct link *) DbugMalloc(sizeof(struct link)+len);
    memcpy(new_malloc->str, start, len);
    new_malloc->str[len]=0;
    new_malloc->next_link= head;
    head= new_malloc;
    ctlp++;
  }
  return head;
}

/*
 *  FUNCTION
 *
 *      ListDel    remove from the list modifiers in debug control string
 *
 *  SYNOPSIS
 *
 *      static struct link *ListDel(listp, ctlp, end)
 *      struct link *listp;
 *      char *ctlp;
 *      char *end;
 *
 *  DESCRIPTION
 *
 *      Given pointer to a comma separated list of strings in "cltp",
 *      parses the list, and removes these strings from the listp,
 *      returning a pointer to the new list.
 *
 */

static struct link *ListDel(struct link *head,
                             const char *ctlp, const char *end)
{
  const char *start;
  struct link **cur;
  int len;

  while (ctlp < end)
  {
    start= ctlp;
    while (ctlp < end && *ctlp != ',')
      ctlp++;
    len=ctlp-start;
    cur=&head;
    do
    {
      while (*cur && !strncmp((*cur)->str, start, len))
      {
        struct link *delme=*cur;
        *cur=(*cur)->next_link;
        free((char*)delme);
      }
    } while (*cur && *(cur=&((*cur)->next_link)));
  }
  return head;
}

/*
 *  FUNCTION
 *
 *      ListCopy    make a copy of the list
 *
 *  SYNOPSIS
 *
 *      static struct link *ListCopy(orig)
 *      struct link *orig;
 *
 *  DESCRIPTION
 *
 *      Given pointer to list, which contains a copy of every element from
 *      the original list.
 *
 *      the orig pointer can be NULL
 *
 *      Note that since each link is added at the head of the list,
 *      the final list will be in "reverse order", which is not
 *      significant for our usage here.
 *
 */

static struct link *ListCopy(struct link *orig)
{
  struct link *new_malloc;
  struct link *head;
  int len;

  head= NULL;
  while (orig != NULL)
  {
    len= strlen(orig->str);
    new_malloc= (struct link *) DbugMalloc(sizeof(struct link)+len);
    memcpy(new_malloc->str, orig->str, len);
    new_malloc->str[len]= 0;
    new_malloc->next_link= head;
    head= new_malloc;
    orig= orig->next_link;
  }
  return head;
}

/*
 *  FUNCTION
 *
 *      InList    test a given string for member of a given list
 *
 *  SYNOPSIS
 *
 *      static BOOLEAN InList(linkp, cp)
 *      struct link *linkp;
 *      char *cp;
 *
 *  DESCRIPTION
 *
 *      Tests the string pointed to by "cp" to determine if it is in
 *      the list pointed to by "linkp".  Linkp points to the first
 *      link in the list.  If linkp is NULL then the string is treated
 *      as if it is in the list (I.E all strings are in the null list).
 *      This may seem rather strange at first but leads to the desired
 *      operation if no list is given.  The net effect is that all
 *      strings will be accepted when there is no list, and when there
 *      is a list, only those strings in the list will be accepted.
 *
 */

static BOOLEAN InList(struct link *linkp, const char *cp)
{
  REGISTER struct link *scan;
  REGISTER BOOLEAN result;

  if (linkp == NULL)
    result= TRUE;
  else
  {
    result= FALSE;
    for (scan= linkp; scan != NULL; scan= scan->next_link)
    {
      if (!strcmp(scan->str, cp))
      {
        result= TRUE;
        break;
      }
    }
  }
  return result;
}


/*
 *  FUNCTION
 *
 *      PushState    push current settings onto stack and set up new one
 *
 *  SYNOPSIS
 *
 *      static VOID PushState()
 *
 *  DESCRIPTION
 *
 *      Pushes the current settings on the settings stack, and creates
 *      a new settings. The new settings is NOT initialized
 *
 *      The settings stack is a linked list of settings, with the new
 *      settings added at the head.  This allows the stack to grow
 *      to the limits of memory if necessary.
 *
 */

static void PushState(CODE_STATE *cs)
{
  struct settings *new_malloc;

  new_malloc= (struct settings *) DbugMalloc(sizeof(struct settings));
  new_malloc->next= cs->stack;
  new_malloc->out_file= NULL;
  cs->stack= new_malloc;
}

/*
 *  FUNCTION
 *
 *	FreeState    Free memory associated with a struct state.
 *
 *  SYNOPSIS
 *
 *	static void FreeState (state)
 *	struct state *state;
 *
 *  DESCRIPTION
 *
 *	Deallocates the memory allocated for various information in a
 *	state.
 *
 */
static void FreeState (
CODE_STATE *cs,
struct settings *state)
{
  if (!is_shared(state, keywords))
    FreeList(state->keywords);
  if (!is_shared(state, functions))
    FreeList(state->functions);
  if (!is_shared(state, processes))
    FreeList(state->processes);
  if (!is_shared(state, p_functions))
    FreeList(state->p_functions);
  if (!is_shared(state, out_file))
    DBUGCloseFile(cs, state->out_file);
  if (state->prof_file)
    DBUGCloseFile(cs, state->prof_file);
  free((char *) state);
}


/*
 *  FUNCTION
 *
 *	_db_end_    End debugging, freeing state stack memory.
 *
 *  SYNOPSIS
 *
 *	static VOID _db_end_ ()
 *
 *  DESCRIPTION
 *
 *	Ends debugging, de-allocating the memory allocated to the
 *	state stack.
 *
 *	To be called at the very end of the program.
 *
 */
void _db_end_ ()
{
  struct settings *discard;
  CODE_STATE *cs=0;

  get_code_state_or_return;

  while((discard= cs->stack) != NULL) {
    if(discard == &init_settings)
      break;
    cs->stack= discard->next;
    FreeState (cs, discard);
  }
}


/*
 *  FUNCTION
 *
 *      DoTrace    check to see if tracing is current enabled
 *
 *  SYNOPSIS
 *
 *      static BOOLEAN DoTrace(stack)
 *
 *  DESCRIPTION
 *
 *      Checks to see if tracing is enabled based on whether the
 *      user has specified tracing, the maximum trace depth has
 *      not yet been reached, the current function is selected,
 *      and the current process is selected.  Returns TRUE if
 *      tracing is enabled, FALSE otherwise.
 *
 */

static BOOLEAN DoTrace(CODE_STATE *cs)
{
  return (TRACING && cs->level <= cs->stack->maxdepth &&
          InList(cs->stack->functions, cs->func) &&
          InList(cs->stack->processes, cs->process));
}


/*
 *  FUNCTION
 *
 *      DoProfile    check to see if profiling is current enabled
 *
 *  SYNOPSIS
 *
 *      static BOOLEAN DoProfile()
 *
 *  DESCRIPTION
 *
 *      Checks to see if profiling is enabled based on whether the
 *      user has specified profiling, the maximum trace depth has
 *      not yet been reached, the current function is selected,
 *      and the current process is selected.  Returns TRUE if
 *      profiling is enabled, FALSE otherwise.
 *
 */

#ifndef THREAD
static BOOLEAN DoProfile(CODE_STATE *cs)
{
  return PROFILING &&
         cs->level <= cs->stack->maxdepth &&
         InList(cs->stack->p_functions, cs->func) &&
         InList(cs->stack->processes, cs->process);
}
#endif

FILE *_db_fp_(void)
{
  CODE_STATE *cs=0;
  get_code_state_or_return NULL;
  return cs->stack->out_file;
}


/*
 *  FUNCTION
 *
 *      _db_strict_keyword_     test keyword for member of keyword list
 *
 *  SYNOPSIS
 *
 *      BOOLEAN _db_strict_keyword_(keyword)
 *      char *keyword;
 *
 *  DESCRIPTION
 *
 *      Similar to _db_keyword_, but keyword is NOT accepted if keyword list
 *      is empty. Used in DBUG_EXECUTE_IF() - for actions that must not be
 *      executed by default.
 *
 *      Returns TRUE if keyword accepted, FALSE otherwise.
 *
 */

BOOLEAN _db_strict_keyword_(const char *keyword)
{
  CODE_STATE *cs=0;
  get_code_state_or_return FALSE;
  if (!DEBUGGING || cs->stack->keywords == NULL)
    return FALSE;
  return _db_keyword_(cs, keyword);
}

/*
 *  FUNCTION
 *
 *      _db_keyword_    test keyword for member of keyword list
 *
 *  SYNOPSIS
 *
 *      BOOLEAN _db_keyword_(keyword)
 *      char *keyword;
 *
 *  DESCRIPTION
 *
 *      Test a keyword to determine if it is in the currently active
 *      keyword list.  As with the function list, a keyword is accepted
 *      if the list is null, otherwise it must match one of the list
 *      members.  When debugging is not on, no keywords are accepted.
 *      After the maximum trace level is exceeded, no keywords are
 *      accepted (this behavior subject to change).  Additionally,
 *      the current function and process must be accepted based on
 *      their respective lists.
 *
 *      Returns TRUE if keyword accepted, FALSE otherwise.
 *
 */

BOOLEAN _db_keyword_(CODE_STATE *cs, const char *keyword)
{
  get_code_state_or_return FALSE;

  return (DEBUGGING &&
          (!TRACING || cs->level <= cs->stack->maxdepth) &&
          InList(cs->stack->functions, cs->func) &&
          InList(cs->stack->keywords, keyword) &&
          InList(cs->stack->processes, cs->process));
}

/*
 *  FUNCTION
 *
 *      Indent    indent a line to the given indentation level
 *
 *  SYNOPSIS
 *
 *      static VOID Indent(indent)
 *      int indent;
 *
 *  DESCRIPTION
 *
 *      Indent a line to the given level.  Note that this is
 *      a simple minded but portable implementation.
 *      There are better ways.
 *
 *      Also, the indent must be scaled by the compile time option
 *      of character positions per nesting level.
 *
 */

static void Indent(CODE_STATE *cs, int indent)
{
  REGISTER int count;

  indent= max(indent-1-cs->stack->sub_level,0)*INDENT;
  for (count= 0; count < indent ; count++)
  {
    if ((count % INDENT) == 0)
      fputc('|',cs->stack->out_file);
    else
      fputc(' ',cs->stack->out_file);
  }
}


/*
 *  FUNCTION
 *
 *      FreeList    free all memory associated with a linked list
 *
 *  SYNOPSIS
 *
 *      static VOID FreeList(linkp)
 *      struct link *linkp;
 *
 *  DESCRIPTION
 *
 *      Given pointer to the head of a linked list, frees all
 *      memory held by the list and the members of the list.
 *
 */

static void FreeList(struct link *linkp)
{
  REGISTER struct link *old;

  while (linkp != NULL)
  {
    old= linkp;
    linkp= linkp->next_link;
    free((char *) old);
  }
}


/*
 *  FUNCTION
 *
 *      DoPrefix    print debugger line prefix prior to indentation
 *
 *  SYNOPSIS
 *
 *      static VOID DoPrefix(_line_)
 *      int _line_;
 *
 *  DESCRIPTION
 *
 *      Print prefix common to all debugger output lines, prior to
 *      doing indentation if necessary.  Print such information as
 *      current process name, current source file name and line number,
 *      and current function nesting depth.
 *
 */

static void DoPrefix(CODE_STATE *cs, uint _line_)
{
  cs->lineno++;
  if (cs->stack->flags & PID_ON)
  {
#ifdef THREAD
    (void) fprintf(cs->stack->out_file, "%-7s: ", my_thread_name());
#else
    (void) fprintf(cs->stack->out_file, "%5d: ", (int) getpid());
#endif
  }
  if (cs->stack->flags & NUMBER_ON)
    (void) fprintf(cs->stack->out_file, "%5d: ", cs->lineno);
  if (cs->stack->flags & TIMESTAMP_ON)
  {
#ifdef __WIN__
    /* FIXME This doesn't give microseconds as in Unix case, and the resolution is
       in system ticks, 10 ms intervals. See my_getsystime.c for high res */
    SYSTEMTIME loc_t;
    GetLocalTime(&loc_t);
    (void) fprintf (cs->stack->out_file,
                    /* "%04d-%02d-%02d " */
                    "%02d:%02d:%02d.%06d ",
                    /*tm_p->tm_year + 1900, tm_p->tm_mon + 1, tm_p->tm_mday,*/
                    loc_t.wHour, loc_t.wMinute, loc_t.wSecond, loc_t.wMilliseconds);
#else
    struct timeval tv;
    struct tm *tm_p;
    if (gettimeofday(&tv, NULL) != -1)
    {
      if ((tm_p= localtime((const time_t *)&tv.tv_sec)))
      {
        (void) fprintf (cs->stack->out_file,
                        /* "%04d-%02d-%02d " */
                        "%02d:%02d:%02d.%06d ",
                        /*tm_p->tm_year + 1900, tm_p->tm_mon + 1, tm_p->tm_mday,*/
                        tm_p->tm_hour, tm_p->tm_min, tm_p->tm_sec,
                        (int) (tv.tv_usec));
      }
    }
#endif
  }
  if (cs->stack->flags & PROCESS_ON)
    (void) fprintf(cs->stack->out_file, "%s: ", cs->process);
  if (cs->stack->flags & FILE_ON)
    (void) fprintf(cs->stack->out_file, "%14s: ", BaseName(cs->file));
  if (cs->stack->flags & LINE_ON)
    (void) fprintf(cs->stack->out_file, "%5d: ", _line_);
  if (cs->stack->flags & DEPTH_ON)
    (void) fprintf(cs->stack->out_file, "%4d: ", cs->level);
}


/*
 *  FUNCTION
 *
 *      DBUGOpenFile    open new output stream for debugger output
 *
 *  SYNOPSIS
 *
 *      static VOID DBUGOpenFile(name)
 *      char *name;
 *
 *  DESCRIPTION
 *
 *      Given name of a new file (or "-" for stdout) opens the file
 *      and sets the output stream to the new file.
 *
 */

static void DBUGOpenFile(CODE_STATE *cs,
                         const char *name,const char *end,int append)
{
  REGISTER FILE *fp;
  REGISTER BOOLEAN newfile;

  if (name != NULL)
  {
    if (end)
    {
      int len=end-name;
      memcpy(cs->stack->name, name, len);
      cs->stack->name[len]=0;
    }
    else
    strmov(cs->stack->name,name);
    name=cs->stack->name;
    if (strcmp(name, "-") == 0)
    {
      cs->stack->out_file= stdout;
      cs->stack->flags |= FLUSH_ON_WRITE;
      cs->stack->name[0]=0;
    }
    else
    {
      if (!Writable((char*)name))
      {
        (void) fprintf(stderr, ERR_OPEN, cs->process, name);
        perror("");
        fflush(stderr);
      }
      else
      {
        newfile= !EXISTS(name);
        if (!(fp= fopen(name, append ? "a+" : "w")))
        {
          (void) fprintf(stderr, ERR_OPEN, cs->process, name);
          perror("");
          fflush(stderr);
        }
        else
        {
          cs->stack->out_file= fp;
          if (newfile)
          {
            ChangeOwner(cs, name);
          }
        }
      }
    }
  }
}


/*
 *  FUNCTION
 *
 *      OpenProfile    open new output stream for profiler output
 *
 *  SYNOPSIS
 *
 *      static FILE *OpenProfile(name)
 *      char *name;
 *
 *  DESCRIPTION
 *
 *      Given name of a new file, opens the file
 *      and sets the profiler output stream to the new file.
 *
 *      It is currently unclear whether the prefered behavior is
 *      to truncate any existing file, or simply append to it.
 *      The latter behavior would be desirable for collecting
 *      accumulated runtime history over a number of separate
 *      runs.  It might take some changes to the analyzer program
 *      though, and the notes that Binayak sent with the profiling
 *      diffs indicated that append was the normal mode, but this
 *      does not appear to agree with the actual code. I haven't
 *      investigated at this time [fnf; 24-Jul-87].
 */

#ifndef THREAD
static FILE *OpenProfile(CODE_STATE *cs, const char *name)
{
  REGISTER FILE *fp;
  REGISTER BOOLEAN newfile;

  fp=0;
  if (!Writable(name))
  {
    (void) fprintf(cs->stack->out_file, ERR_OPEN, cs->process, name);
    perror("");
    dbug_flush(0);
    (void) Delay(cs->stack->delay);
  }
  else
  {
    newfile= !EXISTS(name);
    if (!(fp= fopen(name, "w")))
    {
      (void) fprintf(cs->stack->out_file, ERR_OPEN, cs->process, name);
      perror("");
      dbug_flush(0);
    }
    else
    {
      cs->stack->prof_file= fp;
      if (newfile)
      {
        ChangeOwner(cs, name);
      }
    }
  }
  return fp;
}
#endif

/*
 *  FUNCTION
 *
 *      DBUGCloseFile    close the debug output stream
 *
 *  SYNOPSIS
 *
 *      static VOID DBUGCloseFile(fp)
 *      FILE *fp;
 *
 *  DESCRIPTION
 *
 *      Closes the debug output stream unless it is standard output
 *      or standard error.
 *
 */

static void DBUGCloseFile(CODE_STATE *cs, FILE *fp)
{
  if (fp != stderr && fp != stdout && fclose(fp) == EOF)
  {
    pthread_mutex_lock(&THR_LOCK_dbug);
    (void) fprintf(cs->stack->out_file, ERR_CLOSE, cs->process);
    perror("");
    dbug_flush(0);
  }
}


/*
 *  FUNCTION
 *
 *      DbugExit    print error message and exit
 *
 *  SYNOPSIS
 *
 *      static VOID DbugExit(why)
 *      char *why;
 *
 *  DESCRIPTION
 *
 *      Prints error message using current process name, the reason for
 *      aborting (typically out of memory), and exits with status 1.
 *      This should probably be changed to use a status code
 *      defined in the user's debugger include file.
 *
 */

static void DbugExit(const char *why)
{
  CODE_STATE *cs=code_state();
  (void) fprintf(stderr, ERR_ABORT, cs ? cs->process : "(null)", why);
  (void) fflush(stderr);
  exit(1);
}


/*
 *  FUNCTION
 *
 *      DbugMalloc    allocate memory for debugger runtime support
 *
 *  SYNOPSIS
 *
 *      static long *DbugMalloc(size)
 *      int size;
 *
 *  DESCRIPTION
 *
 *      Allocate more memory for debugger runtime support functions.
 *      Failure to to allocate the requested number of bytes is
 *      immediately fatal to the current process.  This may be
 *      rather unfriendly behavior.  It might be better to simply
 *      print a warning message, freeze the current debugger cs,
 *      and continue execution.
 *
 */

static char *DbugMalloc(size_t size)
{
  register char *new_malloc;

  if (!(new_malloc= (char*) malloc((size_t) size)))
    DbugExit("out of memory");
  return new_malloc;
}


/*
 *     strtok lookalike - splits on ':', magically handles :\ and :/
 */

static const char *DbugStrTok(const char *s)
{
  while (s[0] && (s[0] != ':' || (s[1] == '\\' || s[1] == '/')))
    s++;
  return s;
}


/*
 *  FUNCTION
 *
 *      BaseName    strip leading pathname components from name
 *
 *  SYNOPSIS
 *
 *      static char *BaseName(pathname)
 *      char *pathname;
 *
 *  DESCRIPTION
 *
 *      Given pointer to a complete pathname, locates the base file
 *      name at the end of the pathname and returns a pointer to
 *      it.
 *
 */

static const char *BaseName(const char *pathname)
{
  register const char *base;

  base= strrchr(pathname, FN_LIBCHAR);
  if (base++ == NullS)
    base= pathname;
  return base;
}


/*
 *  FUNCTION
 *
 *      Writable    test to see if a pathname is writable/creatable
 *
 *  SYNOPSIS
 *
 *      static BOOLEAN Writable(pathname)
 *      char *pathname;
 *
 *  DESCRIPTION
 *
 *      Because the debugger might be linked in with a program that
 *      runs with the set-uid-bit (suid) set, we have to be careful
 *      about opening a user named file for debug output.  This consists
 *      of checking the file for write access with the real user id,
 *      or checking the directory where the file will be created.
 *
 *      Returns TRUE if the user would normally be allowed write or
 *      create access to the named file.  Returns FALSE otherwise.
 *
 */


#ifndef Writable

static BOOLEAN Writable(char *pathname)
{
  REGISTER BOOLEAN granted;
  REGISTER char *lastslash;

  granted= FALSE;
  if (EXISTS(pathname))
  {
    if (WRITABLE(pathname))
      granted= TRUE;
  }
  else
  {
    lastslash= strrchr(pathname, '/');
    if (lastslash != NULL)
      *lastslash= '\0';
    else
      pathname= ".";
    if (WRITABLE(pathname))
      granted= TRUE;
    if (lastslash != NULL)
      *lastslash= '/';
  }
  return granted;
}
#endif


/*
 *  FUNCTION
 *
 *      ChangeOwner    change owner to real user for suid programs
 *
 *  SYNOPSIS
 *
 *      static VOID ChangeOwner(pathname)
 *
 *  DESCRIPTION
 *
 *      For unix systems, change the owner of the newly created debug
 *      file to the real owner.  This is strictly for the benefit of
 *      programs that are running with the set-user-id bit set.
 *
 *      Note that at this point, the fact that pathname represents
 *      a newly created file has already been established.  If the
 *      program that the debugger is linked to is not running with
 *      the suid bit set, then this operation is redundant (but
 *      harmless).
 *
 */

#ifndef ChangeOwner
static void ChangeOwner(CODE_STATE *cs, char *pathname)
{
  if (chown(pathname, getuid(), getgid()) == -1)
  {
    (void) fprintf(stderr, ERR_CHOWN, cs->process, pathname);
    perror("");
    (void) fflush(stderr);
  }
}
#endif


/*
 *  FUNCTION
 *
 *      _db_setjmp_    save debugger environment
 *
 *  SYNOPSIS
 *
 *      VOID _db_setjmp_()
 *
 *  DESCRIPTION
 *
 *      Invoked as part of the user's DBUG_SETJMP macro to save
 *      the debugger environment in parallel with saving the user's
 *      environment.
 *
 */

#ifdef HAVE_LONGJMP

EXPORT void _db_setjmp_()
{
  CODE_STATE *cs=0;
  get_code_state_or_return;

  cs->jmplevel= cs->level;
  cs->jmpfunc= cs->func;
  cs->jmpfile= cs->file;
}

/*
 *  FUNCTION
 *
 *      _db_longjmp_    restore previously saved debugger environment
 *
 *  SYNOPSIS
 *
 *      VOID _db_longjmp_()
 *
 *  DESCRIPTION
 *
 *      Invoked as part of the user's DBUG_LONGJMP macro to restore
 *      the debugger environment in parallel with restoring the user's
 *      previously saved environment.
 *
 */

EXPORT void _db_longjmp_()
{
  CODE_STATE *cs=0;
  get_code_state_or_return;

  cs->level= cs->jmplevel;
  if (cs->jmpfunc)
    cs->func= cs->jmpfunc;
  if (cs->jmpfile)
    cs->file= cs->jmpfile;
}
#endif

/*
 *  FUNCTION
 *
 *      perror    perror simulation for systems that don't have it
 *
 *  SYNOPSIS
 *
 *      static VOID perror(s)
 *      char *s;
 *
 *  DESCRIPTION
 *
 *      Perror produces a message on the standard error stream which
 *      provides more information about the library or system error
 *      just encountered.  The argument string s is printed, followed
 *      by a ':', a blank, and then a message and a newline.
 *
 *      An undocumented feature of the unix perror is that if the string
 *      's' is a null string (NOT a NULL pointer!), then the ':' and
 *      blank are not printed.
 *
 *      This version just complains about an "unknown system error".
 *
 */

#ifndef HAVE_PERROR
static void perror(s)
char *s;
{
  if (s && *s != '\0')
    (void) fprintf(stderr, "%s: ", s);
  (void) fprintf(stderr, "<unknown system error>\n");
}
#endif /* HAVE_PERROR */


        /* flush dbug-stream, free mutex lock & wait delay */
        /* This is because some systems (MSDOS!!) dosn't flush fileheader */
        /* and dbug-file isn't readable after a system crash !! */

static void dbug_flush(CODE_STATE *cs)
{
#ifndef THREAD
  if (cs->stack->flags & FLUSH_ON_WRITE)
#endif
  {
#if defined(MSDOS) || defined(__WIN__)
    if (cs->stack->out_file != stdout && cs->stack->out_file != stderr)
    {
      if (!(freopen(cs->stack->name,"a",cs->stack->out_file)))
      {
        (void) fprintf(stderr, ERR_OPEN, cs->process, cs->stack->name);
        fflush(stderr);
        cs->stack->out_file= stderr;
      }
    }
    else
#endif
    {
      (void) fflush(cs->stack->out_file);
      if (cs->stack->delay)
        (void) Delay(cs->stack->delay);
    }
  }
  if (!cs->locked)
    pthread_mutex_unlock(&THR_LOCK_dbug);
} /* dbug_flush */


void _db_lock_file_()
{
  CODE_STATE *cs=0;
  get_code_state_or_return;
  pthread_mutex_lock(&THR_LOCK_dbug);
  cs->locked=1;
}

void _db_unlock_file_()
{
  CODE_STATE *cs=0;
  get_code_state_or_return;
  cs->locked=0;
  pthread_mutex_unlock(&THR_LOCK_dbug);
}

/*
 * Here we need the definitions of the clock routine.  Add your
 * own for whatever system that you have.
 */

#ifndef THREAD
#if defined(HAVE_GETRUSAGE)

#include <sys/param.h>
#include <sys/resource.h>

/* extern int getrusage(int, struct rusage *); */

/*
 * Returns the user time in milliseconds used by this process so
 * far.
 */

static unsigned long Clock()
{
    struct rusage ru;

    (void) getrusage(RUSAGE_SELF, &ru);
    return ru.ru_utime.tv_sec*1000 + ru.ru_utime.tv_usec/1000;
}

#elif defined(MSDOS) || defined(__WIN__)

static ulong Clock()
{
  return clock()*(1000/CLOCKS_PER_SEC);
}
#elif defined(amiga)

struct DateStamp {              /* Yes, this is a hack, but doing it right */
        long ds_Days;           /* is incredibly ugly without splitting this */
        long ds_Minute;         /* off into a separate file */
        long ds_Tick;
};

static int first_clock= TRUE;
static struct DateStamp begin;
static struct DateStamp elapsed;

static unsigned long Clock()
{
    register struct DateStamp *now;
    register unsigned long millisec= 0;
    extern VOID *AllocMem();

    now= (struct DateStamp *) AllocMem((long) sizeof(struct DateStamp), 0L);
    if (now != NULL)
    {
        if (first_clock == TRUE)
        {
            first_clock= FALSE;
            (void) DateStamp(now);
            begin= *now;
        }
        (void) DateStamp(now);
        millisec= 24 * 3600 * (1000 / HZ) * (now->ds_Days - begin.ds_Days);
        millisec += 60 * (1000 / HZ) * (now->ds_Minute - begin.ds_Minute);
        millisec += (1000 / HZ) * (now->ds_Tick - begin.ds_Tick);
        (void) FreeMem(now, (long) sizeof(struct DateStamp));
    }
    return millisec;
}
#else
static unsigned long Clock()
{
    return 0;
}
#endif /* RUSAGE */
#endif /* THREADS */

#ifdef NO_VARARGS

/*
 *      Fake vfprintf for systems that don't support it.  If this
 *      doesn't work, you are probably SOL...
 */

static int vfprintf(stream, format, ap)
FILE *stream;
char *format;
va_list ap;
{
    int rtnval;
    ARGS_DCL;

    ARG0=  va_arg(ap, ARGS_TYPE);
    ARG1=  va_arg(ap, ARGS_TYPE);
    ARG2=  va_arg(ap, ARGS_TYPE);
    ARG3=  va_arg(ap, ARGS_TYPE);
    ARG4=  va_arg(ap, ARGS_TYPE);
    ARG5=  va_arg(ap, ARGS_TYPE);
    ARG6=  va_arg(ap, ARGS_TYPE);
    ARG7=  va_arg(ap, ARGS_TYPE);
    ARG8=  va_arg(ap, ARGS_TYPE);
    ARG9=  va_arg(ap, ARGS_TYPE);
    rtnval= fprintf(stream, format, ARGS_LIST);
    return rtnval;
}

#endif  /* NO_VARARGS */

#endif
