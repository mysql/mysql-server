/*
 pstack.c -- asynchronous stack trace of a running process
 Copyright (c) 1999 Ross Thompson
 Author: Ross Thompson <ross@whatsis.com>
 Critical bug fix: Tim Waugh
*/

/*
 This file is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

/* RESTRICTIONS:

   pstack currently works only on Linux, only on an x86 machine running
   32 bit ELF binaries (64 bit not supported).  Also, for symbolic
   information, you need to use a GNU compiler to generate your
   program, and you can't strip symbols from the binaries.  For thread
   information to be dumped, you have to use the debug-aware version
   of libpthread.so.  (To check, run 'nm' on your libpthread.so, and
   make sure that the symbol "__pthread_threads_debug" is defined.)

   The details of pulling stuff out of ELF files and running through
   program images is very platform specific, and I don't want to
   try to support modes or machine types I can't test in or on.
   If someone wants to generalize this to other architectures, I would
   be happy to help and coordinate the activity.  Please send me whatever
   changes you make to support these machines, so that I can own the
   central font of all truth (at least as regards this program).

   Thanks 
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <asm/ptrace.h>

#include <assert.h>
#include <fcntl.h>
#include <link.h>
#include <malloc.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <limits.h>		/* PTHREAD_THREADS_MAX */


#include <bfd.h>

#include "libiberty.h"

#include "pstack.h"		/* just one function */
#include "budbg.h"		/* binutils stuff related to debugging symbols. */
#include "bucomm.h"		/* some common stuff */
#include "debug.h"		/* and more binutils stuff... */
#include "budbg.h"
#include "linuxthreads.h"	/* LinuxThreads specific stuff... */


/*
 * fprintf for file descriptors :) NOTE: we have to use fixed-size buffer :)(
 * due to malloc's unavalaibility.
 */
int
fdprintf(	int		fd,
		const char*	fmt,...)
{
	char	xbuf[2048];// FIXME: enough?
	va_list	ap;
	int	r;
	if (fd<0)
		return -1;
	va_start(ap, fmt);
	r = vsnprintf(xbuf, sizeof(xbuf), fmt, ap);
	va_end(ap);
	return write(fd, xbuf, r);
}

int
fdputc(		char		c,
		int		fd)
{
	if (fd<0)
		return -1;
	return write(fd, &c, sizeof(c));
}

int
fdputs(		const char*	s,
		int		fd)
{
	if (fd<0)
		return -1;
	return write(fd, s, strlen(s));
}

/*
 * Use this function to open log file.
 * Flags: truncate on opening.
 */
static	const char*		path_format = "stack-trace-on-segv-%d.txt";
static int
open_log_file(	const pthread_t	tid,
		const pid_t	pid)
{
	char	fname[PATH_MAX];
	int	r;
	snprintf(fname, sizeof(fname), path_format, tid, pid);
	r = open(fname, O_WRONLY|O_CREAT|O_TRUNC,
			S_IRUSR|S_IWUSR);
	if (r<0)
		perror("open");
	return r;
}
/*
 * Add additional debugging information for functions.
 */

/*
 * Lineno
 */
typedef struct {
	int			lineno;
	bfd_vma			addr;
} debug_lineno_t;

/*
 * Block - a {} pair.
 */
typedef struct debug_block_st {
	bfd_vma			begin_addr;	/* where did it start */
	bfd_vma			end_addr;	/* where did it end */
	struct debug_block_st*	parent;
	struct debug_block_st*	childs;
	int			childs_count;
} debug_block_t;

/*
 * Function parameter.
 */
typedef struct {
	bfd_vma			offset;	/* Offset in the stack */
	const char*		name;	/* And name. */
} debug_parameter_t;

/*
 * Extra information about functions.
 */
typedef struct {
	asymbol*		symbol;	/* mangled function name, addr */
	debug_lineno_t*		lines;
	int			lines_count;
	int			max_lines_count;
	const char*		name;
	const char*		filename;/* a file name it occured in... */
	debug_block_t*		block;	/* each function has a block, or not, you know */
	debug_parameter_t*	argv;	/* argument types. */
	int			argc;
	int			max_argc;
} debug_function_t;

/* This is the structure we use as a handle for these routines.  */
struct pr_handle
{
  /* File to print information to.  */
  FILE *f;
  /* Current indentation level.  */
  unsigned int indent;
  /* Type stack.  */
  struct pr_stack *stack;
  /* Parameter number we are about to output.  */
  int parameter;
  debug_block_t*	block;			/* current block */
  debug_function_t*	function;		/* current function */
  debug_function_t*	functions;		/* all functions */
  int			functions_size;		/* current size */
  int			functions_maxsize;	/* maximum size */
};

/* The type stack.  */

struct pr_stack
{
  /* Next element on the stack.  */
  struct pr_stack *next;
  /* This element.  */
  char *type;
  /* Current visibility of fields if this is a class.  */
  enum debug_visibility visibility;
  /* Name of the current method we are handling.  */
  const char *method;
};

static void indent PARAMS ((struct pr_handle *));
static boolean push_type PARAMS ((struct pr_handle *, const char *));
static boolean prepend_type PARAMS ((struct pr_handle *, const char *));
static boolean append_type PARAMS ((struct pr_handle *, const char *));
static boolean substitute_type PARAMS ((struct pr_handle *, const char *));
static boolean indent_type PARAMS ((struct pr_handle *));
static char *pop_type PARAMS ((struct pr_handle *));
static void print_vma PARAMS ((bfd_vma, char *, boolean, boolean));
static boolean pr_fix_visibility
  PARAMS ((struct pr_handle *, enum debug_visibility));

static boolean pr_start_compilation_unit PARAMS ((PTR, const char *));
static boolean pr_start_source PARAMS ((PTR, const char *));
static boolean pr_empty_type PARAMS ((PTR));
static boolean pr_void_type PARAMS ((PTR));
static boolean pr_int_type PARAMS ((PTR, unsigned int, boolean));
static boolean pr_float_type PARAMS ((PTR, unsigned int));
static boolean pr_complex_type PARAMS ((PTR, unsigned int));
static boolean pr_bool_type PARAMS ((PTR, unsigned int));
static boolean pr_enum_type
  PARAMS ((PTR, const char *, const char **, bfd_signed_vma *));
static boolean pr_pointer_type PARAMS ((PTR));
static boolean pr_function_type PARAMS ((PTR, int, boolean));
static boolean pr_reference_type PARAMS ((PTR));
static boolean pr_range_type PARAMS ((PTR, bfd_signed_vma, bfd_signed_vma));
static boolean pr_array_type
  PARAMS ((PTR, bfd_signed_vma, bfd_signed_vma, boolean));
static boolean pr_set_type PARAMS ((PTR, boolean));
static boolean pr_offset_type PARAMS ((PTR));
static boolean pr_method_type PARAMS ((PTR, boolean, int, boolean));
static boolean pr_const_type PARAMS ((PTR));
static boolean pr_volatile_type PARAMS ((PTR));
static boolean pr_start_struct_type
  PARAMS ((PTR, const char *, unsigned int, boolean, unsigned int));
static boolean pr_struct_field
  PARAMS ((PTR, const char *, bfd_vma, bfd_vma, enum debug_visibility));
static boolean pr_end_struct_type PARAMS ((PTR));
static boolean pr_start_class_type
  PARAMS ((PTR, const char *, unsigned int, boolean, unsigned int, boolean,
	   boolean));
static boolean pr_class_static_member
  PARAMS ((PTR, const char *, const char *, enum debug_visibility));
static boolean pr_class_baseclass
  PARAMS ((PTR, bfd_vma, boolean, enum debug_visibility));
static boolean pr_class_start_method PARAMS ((PTR, const char *));
static boolean pr_class_method_variant
  PARAMS ((PTR, const char *, enum debug_visibility, boolean, boolean,
	   bfd_vma, boolean));
static boolean pr_class_static_method_variant
  PARAMS ((PTR, const char *, enum debug_visibility, boolean, boolean));
static boolean pr_class_end_method PARAMS ((PTR));
static boolean pr_end_class_type PARAMS ((PTR));
static boolean pr_typedef_type PARAMS ((PTR, const char *));
static boolean pr_tag_type
  PARAMS ((PTR, const char *, unsigned int, enum debug_type_kind));
static boolean pr_typdef PARAMS ((PTR, const char *));
static boolean pr_tag PARAMS ((PTR, const char *));
static boolean pr_int_constant PARAMS ((PTR, const char *, bfd_vma));
static boolean pr_float_constant PARAMS ((PTR, const char *, double));
static boolean pr_typed_constant PARAMS ((PTR, const char *, bfd_vma));
static boolean pr_variable
  PARAMS ((PTR, const char *, enum debug_var_kind, bfd_vma));
static boolean pr_start_function PARAMS ((PTR, const char *, boolean));
static boolean pr_function_parameter
  PARAMS ((PTR, const char *, enum debug_parm_kind, bfd_vma));
static boolean pr_start_block PARAMS ((PTR, bfd_vma));
static boolean pr_end_block PARAMS ((PTR, bfd_vma));
static boolean pr_end_function PARAMS ((PTR));
static boolean pr_lineno PARAMS ((PTR, const char *, unsigned long, bfd_vma));

static const struct debug_write_fns pr_fns =
{
  pr_start_compilation_unit,
  pr_start_source,
  pr_empty_type,
  pr_void_type,
  pr_int_type,
  pr_float_type,
  pr_complex_type,
  pr_bool_type,
  pr_enum_type,
  pr_pointer_type,
  pr_function_type,
  pr_reference_type,
  pr_range_type,
  pr_array_type,
  pr_set_type,
  pr_offset_type,
  pr_method_type,
  pr_const_type,
  pr_volatile_type,
  pr_start_struct_type,
  pr_struct_field,
  pr_end_struct_type,
  pr_start_class_type,
  pr_class_static_member,
  pr_class_baseclass,
  pr_class_start_method,
  pr_class_method_variant,
  pr_class_static_method_variant,
  pr_class_end_method,
  pr_end_class_type,
  pr_typedef_type,
  pr_tag_type,
  pr_typdef,
  pr_tag,
  pr_int_constant,
  pr_float_constant,
  pr_typed_constant,
  pr_variable,
  pr_start_function,
  pr_function_parameter,
  pr_start_block,
  pr_end_block,
  pr_end_function,
  pr_lineno
};


/* Indent to the current indentation level.  */

static void
indent (info)
     struct pr_handle *info;
{
  unsigned int i;

  for (i = 0; i < info->indent; i++)
    TRACE_PUTC ((' ', info->f));
}

/* Push a type on the type stack.  */

static boolean
push_type (info, type)
     struct pr_handle *info;
     const char *type;
{
  struct pr_stack *n;

  if (type == NULL)
    return false;

  n = (struct pr_stack *) xmalloc (sizeof *n);
  memset (n, 0, sizeof *n);

  n->type = xstrdup (type);
  n->visibility = DEBUG_VISIBILITY_IGNORE;
  n->method = NULL;
  n->next = info->stack;
  info->stack = n;

  return true;
}

/* Prepend a string onto the type on the top of the type stack.  */

static boolean
prepend_type (info, s)
     struct pr_handle *info;
     const char *s;
{
  char *n;

  assert (info->stack != NULL);

  n = (char *) xmalloc (strlen (s) + strlen (info->stack->type) + 1);
  sprintf (n, "%s%s", s, info->stack->type);
  free (info->stack->type);
  info->stack->type = n;

  return true;
}

/* Append a string to the type on the top of the type stack.  */

static boolean
append_type (info, s)
     struct pr_handle *info;
     const char *s;
{
  unsigned int len;

  if (s == NULL)
    return false;

  assert (info->stack != NULL);

  len = strlen (info->stack->type);
  info->stack->type = (char *) xrealloc (info->stack->type,
					 len + strlen (s) + 1);
  strcpy (info->stack->type + len, s);

  return true;
}

/* We use an underscore to indicate where the name should go in a type
   string.  This function substitutes a string for the underscore.  If
   there is no underscore, the name follows the type.  */

static boolean
substitute_type (info, s)
     struct pr_handle *info;
     const char *s;
{
  char *u;

  assert (info->stack != NULL);

  u = strchr (info->stack->type, '|');
  if (u != NULL)
    {
      char *n;

      n = (char *) xmalloc (strlen (info->stack->type) + strlen (s));

      memcpy (n, info->stack->type, u - info->stack->type);
      strcpy (n + (u - info->stack->type), s);
      strcat (n, u + 1);

      free (info->stack->type);
      info->stack->type = n;

      return true;
    }

  if (strchr (s, '|') != NULL
      && (strchr (info->stack->type, '{') != NULL
	  || strchr (info->stack->type, '(') != NULL))
    {
      if (! prepend_type (info, "(")
	  || ! append_type (info, ")"))
	return false;
    }

  if (*s == '\0')
    return true;

  return (append_type (info, " ")
	  && append_type (info, s));
}

/* Indent the type at the top of the stack by appending spaces.  */

static boolean
indent_type (info)
     struct pr_handle *info;
{
  unsigned int i;

  for (i = 0; i < info->indent; i++)
    {
      if (! append_type (info, " "))
	return false;
    }

  return true;
}

/* Pop a type from the type stack.  */

static char *
pop_type (info)
     struct pr_handle *info;
{
  struct pr_stack *o;
  char *ret;

  assert (info->stack != NULL);

  o = info->stack;
  info->stack = o->next;
  ret = o->type;
  free (o);

  return ret;
}

/* Print a VMA value into a string.  */

static void
print_vma (vma, buf, unsignedp, hexp)
     bfd_vma vma;
     char *buf;
     boolean unsignedp;
     boolean hexp;
{
  if (sizeof (vma) <= sizeof (unsigned long))
    {
      if (hexp)
	sprintf (buf, "0x%lx", (unsigned long) vma);
      else if (unsignedp)
	sprintf (buf, "%lu", (unsigned long) vma);
      else
	sprintf (buf, "%ld", (long) vma);
    }
  else
    {
      buf[0] = '0';
      buf[1] = 'x';
      sprintf_vma (buf + 2, vma);
    }
}

/* Start a new compilation unit.  */

static boolean
pr_start_compilation_unit (p, filename)
     PTR p;
     const char *filename;
{
  struct pr_handle *info = (struct pr_handle *) p;

  assert (info->indent == 0);
/*
  TRACE_FPRINTF( (info->f, "%s:\n", filename));
*/
  return true;
}

/* Start a source file within a compilation unit.  */

static boolean
pr_start_source (p, filename)
     PTR p;
     const char *filename;
{
  struct pr_handle *info = (struct pr_handle *) p;

  assert (info->indent == 0);
/*
  TRACE_FPRINTF( (info->f, " %s:\n", filename));
*/
  return true;
}

/* Push an empty type onto the type stack.  */

static boolean
pr_empty_type (p)
     PTR p;
{
  struct pr_handle *info = (struct pr_handle *) p;

  return push_type (info, "<undefined>");
}

/* Push a void type onto the type stack.  */

static boolean
pr_void_type (p)
     PTR p;
{
  struct pr_handle *info = (struct pr_handle *) p;

  return push_type (info, "void");
}

/* Push an integer type onto the type stack.  */

static boolean
pr_int_type (p, size, unsignedp)
     PTR p;
     unsigned int size;
     boolean unsignedp;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char ab[10];

  sprintf (ab, "%sint%d", unsignedp ? "u" : "", size * 8);
  return push_type (info, ab);
}

/* Push a floating type onto the type stack.  */

static boolean
pr_float_type (p, size)
     PTR p;
     unsigned int size;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char ab[10];

  if (size == 4)
    return push_type (info, "float");
  else if (size == 8)
    return push_type (info, "double");

  sprintf (ab, "float%d", size * 8);
  return push_type (info, ab);
}

/* Push a complex type onto the type stack.  */

static boolean
pr_complex_type (p, size)
     PTR p;
     unsigned int size;
{
  struct pr_handle *info = (struct pr_handle *) p;

  if (! pr_float_type (p, size))
    return false;

  return prepend_type (info, "complex ");
}

/* Push a boolean type onto the type stack.  */

static boolean
pr_bool_type (p, size)
     PTR p;
     unsigned int size;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char ab[10];

  sprintf (ab, "bool%d", size * 8);

  return push_type (info, ab);
}

/* Push an enum type onto the type stack.  */

static boolean
pr_enum_type (p, tag, names, values)
     PTR p;
     const char *tag;
     const char **names;
     bfd_signed_vma *values;
{
  struct pr_handle *info = (struct pr_handle *) p;
  unsigned int i;
  bfd_signed_vma val;

  if (! push_type (info, "enum "))
    return false;
  if (tag != NULL)
    {
      if (! append_type (info, tag)
	  || ! append_type (info, " "))
	return false;
    }
  if (! append_type (info, "{ "))
    return false;

  if (names == NULL)
    {
      if (! append_type (info, "/* undefined */"))
	return false;
    }
  else
    {
      val = 0;
      for (i = 0; names[i] != NULL; i++)
	{
	  if (i > 0)
	    {
	      if (! append_type (info, ", "))
		return false;
	    }

	  if (! append_type (info, names[i]))
	    return false;

	  if (values[i] != val)
	    {
	      char ab[20];

	      print_vma (values[i], ab, false, false);
	      if (! append_type (info, " = ")
		  || ! append_type (info, ab))
		return false;
	      val = values[i];
	    }

	  ++val;
	}
    }

  return append_type (info, " }");
}

/* Turn the top type on the stack into a pointer.  */

static boolean
pr_pointer_type (p)
     PTR p;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char *s;

  assert (info->stack != NULL);

  s = strchr (info->stack->type, '|');
  if (s != NULL && s[1] == '[')
    return substitute_type (info, "(*|)");
  return substitute_type (info, "*|");
}

/* Turn the top type on the stack into a function returning that type.  */

static boolean
pr_function_type (p, argcount, varargs)
     PTR p;
     int argcount;
     boolean varargs;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char **arg_types;
  unsigned int len;
  char *s;

  assert (info->stack != NULL);

  len = 10;

  if (argcount <= 0)
    {
      arg_types = NULL;
      len += 15;
    }
  else
    {
      int i;

      arg_types = (char **) xmalloc (argcount * sizeof *arg_types);
      for (i = argcount - 1; i >= 0; i--)
	{
	  if (! substitute_type (info, ""))
	    return false;
	  arg_types[i] = pop_type (info);
	  if (arg_types[i] == NULL)
	    return false;
	  len += strlen (arg_types[i]) + 2;
	}
      if (varargs)
	len += 5;
    }

  /* Now the return type is on the top of the stack.  */

  s = (char *) xmalloc (len);
  strcpy (s, "(|) (");

  if (argcount < 0)
    {
#if 0
      /* Turn off unknown arguments. */
      strcat (s, "/* unknown */");
#endif
    }
  else
    {
      int i;

      for (i = 0; i < argcount; i++)
	{
	  if (i > 0)
	    strcat (s, ", ");
	  strcat (s, arg_types[i]);
	}
      if (varargs)
	{
	  if (i > 0)
	    strcat (s, ", ");
	  strcat (s, "...");
	}
      if (argcount > 0)
	free (arg_types);
    }

  strcat (s, ")");

  if (! substitute_type (info, s))
    return false;

  free (s);

  return true;
}

/* Turn the top type on the stack into a reference to that type.  */

static boolean
pr_reference_type (p)
     PTR p;
{
  struct pr_handle *info = (struct pr_handle *) p;

  assert (info->stack != NULL);

  return substitute_type (info, "&|");
}

/* Make a range type.  */

static boolean
pr_range_type (p, lower, upper)
     PTR p;
     bfd_signed_vma lower;
     bfd_signed_vma upper;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char abl[20], abu[20];

  assert (info->stack != NULL);

  if (! substitute_type (info, ""))
    return false;

  print_vma (lower, abl, false, false);
  print_vma (upper, abu, false, false);

  return (prepend_type (info, "range (")
	  && append_type (info, "):")
	  && append_type (info, abl)
	  && append_type (info, ":")
	  && append_type (info, abu));
}

/* Make an array type.  */

/*ARGSUSED*/
static boolean
pr_array_type (p, lower, upper, stringp)
     PTR p;
     bfd_signed_vma lower;
     bfd_signed_vma upper;
     boolean stringp;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char *range_type;
  char abl[20], abu[20], ab[50];

  range_type = pop_type (info);
  if (range_type == NULL)
    return false;

  if (lower == 0)
    {
      if (upper == -1)
	sprintf (ab, "|[]");
      else
	{
	  print_vma (upper + 1, abu, false, false);
	  sprintf (ab, "|[%s]", abu);
	}
    }
  else
    {
      print_vma (lower, abl, false, false);
      print_vma (upper, abu, false, false);
      sprintf (ab, "|[%s:%s]", abl, abu);
    }

  if (! substitute_type (info, ab))
    return false;

  if (strcmp (range_type, "int") != 0)
    {
      if (! append_type (info, ":")
	  || ! append_type (info, range_type))
	return false;
    }

  if (stringp)
    {
      if (! append_type (info, " /* string */"))
	return false;
    }

  return true;
}

/* Make a set type.  */

/*ARGSUSED*/
static boolean
pr_set_type (p, bitstringp)
     PTR p;
     boolean bitstringp;
{
  struct pr_handle *info = (struct pr_handle *) p;

  if (! substitute_type (info, ""))
    return false;

  if (! prepend_type (info, "set { ")
      || ! append_type (info, " }"))
    return false;

  if (bitstringp)
    {
      if (! append_type (info, "/* bitstring */"))
	return false;
    }

  return true;
}

/* Make an offset type.  */

static boolean
pr_offset_type (p)
     PTR p;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char *t;

  if (! substitute_type (info, ""))
    return false;

  t = pop_type (info);
  if (t == NULL)
    return false;

  return (substitute_type (info, "")
	  && prepend_type (info, " ")
	  && prepend_type (info, t)
	  && append_type (info, "::|"));
}

/* Make a method type.  */

static boolean
pr_method_type (p, domain, argcount, varargs)
     PTR p;
     boolean domain;
     int argcount;
     boolean varargs;
{
  struct pr_handle *info = (struct pr_handle *) p;
  unsigned int len;
  char *domain_type;
  char **arg_types;
  char *s;

  len = 10;

  if (! domain)
    domain_type = NULL;
  else
    {
      if (! substitute_type (info, ""))
	return false;
      domain_type = pop_type (info);
      if (domain_type == NULL)
	return false;
      if (strncmp (domain_type, "class ", sizeof "class " - 1) == 0
	  && strchr (domain_type + sizeof "class " - 1, ' ') == NULL)
	domain_type += sizeof "class " - 1;
      else if (strncmp (domain_type, "union class ",
			sizeof "union class ") == 0
	       && (strchr (domain_type + sizeof "union class " - 1, ' ')
		   == NULL))
	domain_type += sizeof "union class " - 1;
      len += strlen (domain_type);
    }

  if (argcount <= 0)
    {
      arg_types = NULL;
      len += 15;
    }
  else
    {
      int i;

      arg_types = (char **) xmalloc (argcount * sizeof *arg_types);
      for (i = argcount - 1; i >= 0; i--)
	{
	  if (! substitute_type (info, ""))
	    return false;
	  arg_types[i] = pop_type (info);
	  if (arg_types[i] == NULL)
	    return false;
	  len += strlen (arg_types[i]) + 2;
	}
      if (varargs)
	len += 5;
    }

  /* Now the return type is on the top of the stack.  */

  s = (char *) xmalloc (len);
  if (! domain)
    *s = '\0';
  else
    strcpy (s, domain_type);
  strcat (s, "::| (");

  if (argcount < 0)
    strcat (s, "/* unknown */");
  else
    {
      int i;

      for (i = 0; i < argcount; i++)
	{
	  if (i > 0)
	    strcat (s, ", ");
	  strcat (s, arg_types[i]);
	}
      if (varargs)
	{
	  if (i > 0)
	    strcat (s, ", ");
	  strcat (s, "...");
	}
      if (argcount > 0)
	free (arg_types);
    }

  strcat (s, ")");

  if (! substitute_type (info, s))
    return false;

  free (s);

  return true;
}

/* Make a const qualified type.  */

static boolean
pr_const_type (p)
     PTR p;
{
  struct pr_handle *info = (struct pr_handle *) p;

  return substitute_type (info, "const |");
}

/* Make a volatile qualified type.  */

static boolean
pr_volatile_type (p)
     PTR p;
{
  struct pr_handle *info = (struct pr_handle *) p;

  return substitute_type (info, "volatile |");
}

/* Start accumulating a struct type.  */

static boolean
pr_start_struct_type (p, tag, id, structp, size)
     PTR p;
     const char *tag;
     unsigned int id;
     boolean structp;
     unsigned int size;
{
  struct pr_handle *info = (struct pr_handle *) p;

  info->indent += 2;

  if (! push_type (info, structp ? "struct " : "union "))
    return false;
  if (tag != NULL)
    {
      if (! append_type (info, tag))
	return false;
    }
  else
    {
      char idbuf[20];

      sprintf (idbuf, "%%anon%u", id);
      if (! append_type (info, idbuf))
	return false;
    }

  if (! append_type (info, " {"))
    return false;
  if (size != 0 || tag != NULL)
    {
      char ab[30];

      if (! append_type (info, " /*"))
	return false;

      if (size != 0)
	{
	  sprintf (ab, " size %u", size);
	  if (! append_type (info, ab))
	    return false;
	}
      if (tag != NULL)
	{
	  sprintf (ab, " id %u", id);
	  if (! append_type (info, ab))
	    return false;
	}
      if (! append_type (info, " */"))
	return false;
    }
  if (! append_type (info, "\n"))
    return false;

  info->stack->visibility = DEBUG_VISIBILITY_PUBLIC;

  return indent_type (info);
}

/* Output the visibility of a field in a struct.  */

static boolean
pr_fix_visibility (info, visibility)
     struct pr_handle *info;
     enum debug_visibility visibility;
{
  const char *s;
  char *t;
  unsigned int len;

  assert (info->stack != NULL);

  if (info->stack->visibility == visibility)
    return true;

  assert (info->stack->visibility != DEBUG_VISIBILITY_IGNORE);

  switch (visibility)
    {
    case DEBUG_VISIBILITY_PUBLIC:
      s = "public";
      break;
    case DEBUG_VISIBILITY_PRIVATE:
      s = "private";
      break;
    case DEBUG_VISIBILITY_PROTECTED:
      s = "protected";
      break;
    case DEBUG_VISIBILITY_IGNORE:
      s = "/* ignore */";
      break;
    default:
      abort ();
      return false;
    }

  /* Trim off a trailing space in the struct string, to make the
     output look a bit better, then stick on the visibility string.  */

  t = info->stack->type;
  len = strlen (t);
  assert (t[len - 1] == ' ');
  t[len - 1] = '\0';

  if (! append_type (info, s)
      || ! append_type (info, ":\n")
      || ! indent_type (info))
    return false;

  info->stack->visibility = visibility;

  return true;
}

/* Add a field to a struct type.  */

static boolean
pr_struct_field (p, name, bitpos, bitsize, visibility)
     PTR p;
     const char *name;
     bfd_vma bitpos;
     bfd_vma bitsize;
     enum debug_visibility visibility;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char ab[20];
  char *t;

  if (! substitute_type (info, name))
    return false;

  if (! append_type (info, "; /* "))
    return false;

  if (bitsize != 0)
    {
      print_vma (bitsize, ab, true, false);
      if (! append_type (info, "bitsize ")
	  || ! append_type (info, ab)
	  || ! append_type (info, ", "))
	return false;
    }

  print_vma (bitpos, ab, true, false);
  if (! append_type (info, "bitpos ")
      || ! append_type (info, ab)
      || ! append_type (info, " */\n")
      || ! indent_type (info))
    return false;

  t = pop_type (info);
  if (t == NULL)
    return false;

  if (! pr_fix_visibility (info, visibility))
    return false;

  return append_type (info, t);
}

/* Finish a struct type.  */

static boolean
pr_end_struct_type (p)
     PTR p;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char *s;

  assert (info->stack != NULL);
  assert (info->indent >= 2);

  info->indent -= 2;

  /* Change the trailing indentation to have a close brace.  */
  s = info->stack->type + strlen (info->stack->type) - 2;
  assert (s[0] == ' ' && s[1] == ' ' && s[2] == '\0');

  *s++ = '}';
  *s = '\0';

  return true;
}

/* Start a class type.  */

static boolean
pr_start_class_type (p, tag, id, structp, size, vptr, ownvptr)
     PTR p;
     const char *tag;
     unsigned int id;
     boolean structp;
     unsigned int size;
     boolean vptr;
     boolean ownvptr;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char *tv = NULL;

  info->indent += 2;

  if (vptr && ! ownvptr)
    {
      tv = pop_type (info);
      if (tv == NULL)
	return false;
    }

  if (! push_type (info, structp ? "class " : "union class "))
    return false;
  if (tag != NULL)
    {
      if (! append_type (info, tag))
	return false;
    }
  else
    {
      char idbuf[20];

      sprintf (idbuf, "%%anon%u", id);
      if (! append_type (info, idbuf))
	return false;
    }

  if (! append_type (info, " {"))
    return false;
  if (size != 0 || vptr || ownvptr || tag != NULL)
    {
      if (! append_type (info, " /*"))
	return false;

      if (size != 0)
	{
	  char ab[20];

	  sprintf (ab, "%u", size);
	  if (! append_type (info, " size ")
	      || ! append_type (info, ab))
	    return false;
	}

      if (vptr)
	{
	  if (! append_type (info, " vtable "))
	    return false;
	  if (ownvptr)
	    {
	      if (! append_type (info, "self "))
		return false;
	    }
	  else
	    {
	      if (! append_type (info, tv)
		  || ! append_type (info, " "))
		return false;
	    }
	}

      if (tag != NULL)
	{
	  char ab[30];

	  sprintf (ab, " id %u", id);
	  if (! append_type (info, ab))
	    return false;
	}

      if (! append_type (info, " */"))
	return false;
    }

  info->stack->visibility = DEBUG_VISIBILITY_PRIVATE;

  return (append_type (info, "\n")
	  && indent_type (info));
}

/* Add a static member to a class.  */

static boolean
pr_class_static_member (p, name, physname, visibility)
     PTR p;
     const char *name;
     const char *physname;
     enum debug_visibility visibility;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char *t;

  if (! substitute_type (info, name))
    return false;

  if (! prepend_type (info, "static ")
      || ! append_type (info, "; /* ")
      || ! append_type (info, physname)
      || ! append_type (info, " */\n")
      || ! indent_type (info))
    return false;

  t = pop_type (info);
  if (t == NULL)
    return false;

  if (! pr_fix_visibility (info, visibility))
    return false;

  return append_type (info, t);
}

/* Add a base class to a class.  */

static boolean
pr_class_baseclass (p, bitpos, virtual, visibility)
     PTR p;
     bfd_vma bitpos;
     boolean virtual;
     enum debug_visibility visibility;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char *t;
  const char *prefix;
  char ab[20];
  char *s, *l, *n;

  assert (info->stack != NULL && info->stack->next != NULL);

  if (! substitute_type (info, ""))
    return false;

  t = pop_type (info);
  if (t == NULL)
    return false;

  if (strncmp (t, "class ", sizeof "class " - 1) == 0)
    t += sizeof "class " - 1;

  /* Push it back on to take advantage of the prepend_type and
     append_type routines.  */
  if (! push_type (info, t))
    return false;

  if (virtual)
    {
      if (! prepend_type (info, "virtual "))
	return false;
    }

  switch (visibility)
    {
    case DEBUG_VISIBILITY_PUBLIC:
      prefix = "public ";
      break;
    case DEBUG_VISIBILITY_PROTECTED:
      prefix = "protected ";
      break;
    case DEBUG_VISIBILITY_PRIVATE:
      prefix = "private ";
      break;
    default:
      prefix = "/* unknown visibility */ ";
      break;
    }

  if (! prepend_type (info, prefix))
    return false;

  if (bitpos != 0)
    {
      print_vma (bitpos, ab, true, false);
      if (! append_type (info, " /* bitpos ")
	  || ! append_type (info, ab)
	  || ! append_type (info, " */"))
	return false;
    }

  /* Now the top of the stack is something like "public A / * bitpos
     10 * /".  The next element on the stack is something like "class
     xx { / * size 8 * /\n...".  We want to substitute the top of the
     stack in before the {.  */
  s = strchr (info->stack->next->type, '{');
  assert (s != NULL);
  --s;

  /* If there is already a ':', then we already have a baseclass, and
     we must append this one after a comma.  */
  for (l = info->stack->next->type; l != s; l++)
    if (*l == ':')
      break;
  if (! prepend_type (info, l == s ? " : " : ", "))
    return false;

  t = pop_type (info);
  if (t == NULL)
    return false;

  n = (char *) xmalloc (strlen (info->stack->type) + strlen (t) + 1);
  memcpy (n, info->stack->type, s - info->stack->type);
  strcpy (n + (s - info->stack->type), t);
  strcat (n, s);

  free (info->stack->type);
  info->stack->type = n;

  free (t);

  return true;
}

/* Start adding a method to a class.  */

static boolean
pr_class_start_method (p, name)
     PTR p;
     const char *name;
{
  struct pr_handle *info = (struct pr_handle *) p;

  assert (info->stack != NULL);
  info->stack->method = name;
  return true;
}

/* Add a variant to a method.  */

static boolean
pr_class_method_variant (p, physname, visibility, constp, volatilep, voffset,
			 context)
     PTR p;
     const char *physname;
     enum debug_visibility visibility;
     boolean constp;
     boolean volatilep;
     bfd_vma voffset;
     boolean context;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char *method_type;
  char *context_type;

  assert (info->stack != NULL);
  assert (info->stack->next != NULL);

  /* Put the const and volatile qualifiers on the type.  */
  if (volatilep)
    {
      if (! append_type (info, " volatile"))
	return false;
    }
  if (constp)
    {
      if (! append_type (info, " const"))
	return false;
    }

  /* Stick the name of the method into its type.  */
  if (! substitute_type (info,
			 (context
			  ? info->stack->next->next->method
			  : info->stack->next->method)))
    return false;

  /* Get the type.  */
  method_type = pop_type (info);
  if (method_type == NULL)
    return false;

  /* Pull off the context type if there is one.  */
  if (! context)
    context_type = NULL;
  else
    {
      context_type = pop_type (info);
      if (context_type == NULL)
	return false;
    }

  /* Now the top of the stack is the class.  */

  if (! pr_fix_visibility (info, visibility))
    return false;

  if (! append_type (info, method_type)
      || ! append_type (info, " /* ")
      || ! append_type (info, physname)
      || ! append_type (info, " "))
    return false;
  if (context || voffset != 0)
    {
      char ab[20];

      if (context)
	{
	  if (! append_type (info, "context ")
	      || ! append_type (info, context_type)
	      || ! append_type (info, " "))
	    return false;
	}
      print_vma (voffset, ab, true, false);
      if (! append_type (info, "voffset ")
	  || ! append_type (info, ab))
	return false;
    }

  return (append_type (info, " */;\n")
	  && indent_type (info));
}

/* Add a static variant to a method.  */

static boolean
pr_class_static_method_variant (p, physname, visibility, constp, volatilep)
     PTR p;
     const char *physname;
     enum debug_visibility visibility;
     boolean constp;
     boolean volatilep;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char *method_type;

  assert (info->stack != NULL);
  assert (info->stack->next != NULL);
  assert (info->stack->next->method != NULL);

  /* Put the const and volatile qualifiers on the type.  */
  if (volatilep)
    {
      if (! append_type (info, " volatile"))
	return false;
    }
  if (constp)
    {
      if (! append_type (info, " const"))
	return false;
    }

  /* Mark it as static.  */
  if (! prepend_type (info, "static "))
    return false;

  /* Stick the name of the method into its type.  */
  if (! substitute_type (info, info->stack->next->method))
    return false;

  /* Get the type.  */
  method_type = pop_type (info);
  if (method_type == NULL)
    return false;

  /* Now the top of the stack is the class.  */

  if (! pr_fix_visibility (info, visibility))
    return false;

  return (append_type (info, method_type)
	  && append_type (info, " /* ")
	  && append_type (info, physname)
	  && append_type (info, " */;\n")
	  && indent_type (info));
}

/* Finish up a method.  */

static boolean
pr_class_end_method (p)
     PTR p;
{
  struct pr_handle *info = (struct pr_handle *) p;

  info->stack->method = NULL;
  return true;
}

/* Finish up a class.  */

static boolean
pr_end_class_type (p)
     PTR p;
{
  return pr_end_struct_type (p);
}

/* Push a type on the stack using a typedef name.  */

static boolean
pr_typedef_type (p, name)
     PTR p;
     const char *name;
{
  struct pr_handle *info = (struct pr_handle *) p;

  return push_type (info, name);
}

/* Push a type on the stack using a tag name.  */

static boolean
pr_tag_type (p, name, id, kind)
     PTR p;
     const char *name;
     unsigned int id;
     enum debug_type_kind kind;
{
  struct pr_handle *info = (struct pr_handle *) p;
  const char *t, *tag;
  char idbuf[20];

  switch (kind)
    {
    case DEBUG_KIND_STRUCT:
      t = "struct ";
      break;
    case DEBUG_KIND_UNION:
      t = "union ";
      break;
    case DEBUG_KIND_ENUM:
      t = "enum ";
      break;
    case DEBUG_KIND_CLASS:
      t = "class ";
      break;
    case DEBUG_KIND_UNION_CLASS:
      t = "union class ";
      break;
    default:
      abort ();
      return false;
    }

  if (! push_type (info, t))
    return false;
  if (name != NULL)
    tag = name;
  else
    {
      sprintf (idbuf, "%%anon%u", id);
      tag = idbuf;
    }

  if (! append_type (info, tag))
    return false;
  if (name != NULL && kind != DEBUG_KIND_ENUM)
    {
      sprintf (idbuf, " /* id %u */", id);
      if (! append_type (info, idbuf))
	return false;
    }

  return true;
}

/* Output a typedef.  */

static boolean
pr_typdef (p, name)
     PTR p;
     const char *name;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char *s;

  if (! substitute_type (info, name))
    return false;

  s = pop_type (info);
  if (s == NULL)
    return false;
/*
  indent (info);
  TRACE_FPRINTF( (info->f, "typedef %s;\n", s));
*/
  free (s);

  return true;
}

/* Output a tag.  The tag should already be in the string on the
   stack, so all we have to do here is print it out.  */

/*ARGSUSED*/
static boolean
pr_tag (p, name)
     PTR p;
     const char *name;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char *t;

  t = pop_type (info);
  if (t == NULL)
    return false;
/*
  indent (info);
  TRACE_FPRINTF( (info->f, "%s;\n", t));
*/
  free (t);

  return true;
}

/* Output an integer constant.  */

static boolean
pr_int_constant (p, name, val)
     PTR p;
     const char *name;
     bfd_vma val;
{
/*
  struct pr_handle *info = (struct pr_handle *) p;
  char ab[20];
  indent (info);
  print_vma (val, ab, false, false);
  TRACE_FPRINTF( (info->f, "const int %s = %s;\n", name, ab));
 */
  return true;
}

/* Output a floating point constant.  */

static boolean
pr_float_constant (p, name, val)
     PTR p;
     const char *name;
     double val;
{
/*
  struct pr_handle *info = (struct pr_handle *) p;
  indent (info);
  TRACE_FPRINTF( (info->f, "const double %s = %g;\n", name, val));
 */
  return true;
}

/* Output a typed constant.  */

static boolean
pr_typed_constant (p, name, val)
     PTR p;
     const char *name;
     bfd_vma val;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char *t;

  t = pop_type (info);
  if (t == NULL)
    return false;
/*
  char ab[20];
  indent (info);
  print_vma (val, ab, false, false);
  TRACE_FPRINTF( (info->f, "const %s %s = %s;\n", t, name, ab));
*/
  free (t);

  return true;
}

/* Output a variable.  */

static boolean
pr_variable (p, name, kind, val)
     PTR p;
     const char *name;
     enum debug_var_kind kind;
     bfd_vma val;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char *t;
  char ab[20];
  (void)ab;

  if (! substitute_type (info, name))
    return false;

  t = pop_type (info);
  if (t == NULL)
    return false;

#if 0
  indent (info);
  switch (kind)
    {
    case DEBUG_STATIC:
    case DEBUG_LOCAL_STATIC:
      TRACE_FPRINTF( (info->f, "static "));
      break;
    case DEBUG_REGISTER:
      TRACE_FPRINTF( (info->f, "register "));
      break;
    default:
      break;
    }
  print_vma (val, ab, true, true);
  TRACE_FPRINTF( (info->f, "%s /* %s */;\n", t, ab));
#else /* 0 */
#if 0
	if (kind==DEBUG_STATIC || kind==DEBUG_LOCAL_STATIC) {
		print_vma (val, ab, true, true);
		TRACE_FPRINTF( (info->f, "STATIC_VAR: %s /* %s */;\n", t, ab));
	}
#endif /* 0 */
#endif /* !0 */

  free (t);

  return true;
}

/* Start outputting a function.  */

static boolean
pr_start_function (p, name, global)
     PTR p;
     const char *name;
     boolean global;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char *t;

  if (! substitute_type (info, name))
    return false;

  t = pop_type (info);
  if (t == NULL)
    return false;

#if 0
  indent (info);
  if (! global)
    TRACE_FPRINTF( (info->f, "static "));
  TRACE_FPRINTF( (info->f, "%s (", t));
  info->parameter = 1;
#else /* 0 */
	if (info->functions_size==info->functions_maxsize) {
		info->functions_maxsize *= 2;
		info->functions = xrealloc(info->functions,
			info->functions_maxsize*sizeof(debug_function_t));
		assert(info->functions!=0);
	}
	/* info->functions[info->functions_size] = xmalloc(sizeof(debug_function_t)); */
	info->function = &info->functions[info->functions_size];
	++info->functions_size;
	info->function->symbol = NULL;
	info->function->lines = NULL;
	info->function->lines_count = 0;
	info->function->max_lines_count = 0;
	info->function->name = t;
	info->function->filename = NULL;
	info->function->block = NULL;
	info->function->argv = NULL;
	info->function->argc = 0;
	info->function->max_argc = 0;
#endif /* !0 */
	return true;
}

/* Output a function parameter.  */

static boolean
pr_function_parameter (p, name, kind, val)
     PTR p;
     const char *name;
     enum debug_parm_kind kind;
     bfd_vma val;
{
  struct pr_handle *info = (struct pr_handle *) p;
  debug_function_t*	f = info->function;
  char *t;
  char ab[20];
  (void)ab;

  if (kind == DEBUG_PARM_REFERENCE
      || kind == DEBUG_PARM_REF_REG)
    {
      if (! pr_reference_type (p))
	return false;
    }

  if (! substitute_type (info, name))
    return false;

  t = pop_type (info);
  if (t == NULL)
    return false;

#if 0
  if (info->parameter != 1)
    TRACE_FPRINTF( (info->f, ", "));

  if (kind == DEBUG_PARM_REG || kind == DEBUG_PARM_REF_REG)
    TRACE_FPRINTF( (info->f, "register "));

  print_vma (val, ab, true, true);
  TRACE_FPRINTF( (info->f, "%s /* %s */", t, ab));
  free (t);
  ++info->parameter;
#else /* 0 */
	assert(f!=NULL);
	if (f->argv==NULL) {
		f->max_argc = 7; /* rarely anyone has more than that many args... */
		f->argv = xmalloc(sizeof(debug_parameter_t)*f->max_argc);
	} else if (f->argc==f->max_argc) {
		f->max_argc *= 2;
		f->argv = realloc(f->argv,sizeof(debug_parameter_t)*f->max_argc);
	}
	f->argv[f->argc].offset = val;
	f->argv[f->argc].name = t;
	++f->argc;
#endif /* !0 */
	return true;
}

/* Start writing out a block.  */

static boolean
pr_start_block (p, addr)
     PTR p;
     bfd_vma addr;
{
	struct pr_handle *info = (struct pr_handle *) p;
	char ab[20];
	debug_block_t*	block = 0;
	(void)ab;
#if 0
  if (info->parameter > 0)
    {
      TRACE_FPRINTF( (info->f, ")\n"));
      info->parameter = 0;
    }
  indent (info);
  print_vma (addr, ab, true, true);
  TRACE_FPRINTF( (info->f, "{ /* %s */\n", ab));
  info->indent += 2;
#else
	if (info->block) {
		if (info->block->childs_count==0)
			info->block->childs = xmalloc(sizeof(debug_block_t));
		else
			info->block->childs = xrealloc(info->block->childs,
						info->block->childs_count*sizeof(debug_block_t));
		block = &info->block->childs[info->block->childs_count];
	} else {
		block = xmalloc(sizeof(debug_block_t));
		info->function->block = block;
	}
	block->begin_addr = addr;
	block->end_addr = 0;
	block->parent = info->block;
	block->childs = NULL;
	block->childs_count = 0;
	info->block = block;
#endif
	return true;
}

/* Write out line number information.  */

static boolean
pr_lineno (p, filename, lineno, addr)
     PTR p;
     const char *filename;
     unsigned long lineno;
     bfd_vma addr;
{
	struct pr_handle *info = (struct pr_handle *) p;
	char ab[20];
	debug_function_t*	f = info->function;
	(void)ab;

#if 0
  indent (info);
  print_vma (addr, ab, true, true);
  TRACE_FPRINTF( (info->f, "/* file %s line %lu addr %s */\n", filename, lineno, ab));
#else /* 0 */
	if (f==NULL)	/* FIXME: skips junk silently. */
		return true;
	/* assert(f!=NULL); */
	if (f->filename==NULL) {
		f->filename = filename;
		assert(f->lines==0);
		f->max_lines_count = 4;
		f->lines = xmalloc(sizeof(debug_lineno_t)*f->max_lines_count);
	}
	if (f->lines_count==f->max_lines_count) {
		f->max_lines_count *= 2;
		f->lines = xrealloc(f->lines, sizeof(debug_lineno_t)*f->max_lines_count);
	}
	f->lines[f->lines_count].lineno = lineno;
	f->lines[f->lines_count].addr = addr;
	++f->lines_count;
#endif /* !0 */

  return true;
}

/* Finish writing out a block.  */

static boolean
pr_end_block (p, addr)
     PTR p;
     bfd_vma addr;
{
	struct pr_handle *info = (struct pr_handle *) p;

#if 0
  char ab[20];

  info->indent -= 2;
  indent (info);
  print_vma (addr, ab, true, true);
  TRACE_FPRINTF( (info->f, "} /* %s */\n", ab));
#else /* 0 */
	assert(info->block!=0);
	info->block->end_addr = addr;
	info->block = info->block->parent;
#endif /* !0 */

	return true;
}

/* Finish writing out a function.  */

/*ARGSUSED*/
static boolean
pr_end_function (p)
     PTR p;
{
	struct pr_handle *info = (struct pr_handle *) p;
	assert(info->block==0);
	info->function = NULL;
	return true;
}

/* third parameter to segv_action. */
/* Got it after a bit of head scratching and stack dumping. */
typedef struct {
	u_int32_t	foo1;	/* +0x00 */
	u_int32_t	foo2;
	u_int32_t	foo3;
	u_int32_t	foo4;	/* usually 2 */
	u_int32_t	foo5;	/* +0x10 */
	u_int32_t	xgs;	/* always zero */
	u_int32_t	xfs;	/* always zero */
	u_int32_t	xes;	/* always es=ds=ss */
	u_int32_t	xds;	/* +0x20 */
	u_int32_t	edi;
	u_int32_t	esi;
	u_int32_t	ebp;
	u_int32_t	esp;	/* +0x30 */
	u_int32_t	ebx;
	u_int32_t	edx;
	u_int32_t	ecx;
	u_int32_t	eax;	/* +0x40 */
	u_int32_t	foo11;	/* usually 0xe */
	u_int32_t	foo12;	/* usually 0x6 */
	u_int32_t	eip;	/* instruction pointer */
	u_int32_t	xcs;	/* +0x50 */
	u_int32_t	foo21;	/* usually 0x2 */
	u_int32_t	foo22;	/* second stack pointer?! Probably. */
	u_int32_t	xss;
	u_int32_t	foo31;	/* +0x60 */ /* usually 0x0 */
	u_int32_t	foo32;	/* usually 0x2 */
	u_int32_t	fault_addr;	/* Address which caused a fault */
	u_int32_t	foo41;	/* usually 0x2 */
} signal_regs_t;

signal_regs_t*	ptrace_regs = 0;	/* Tells my_ptrace to "ptrace" current process" */
/*
 * my_ptrace: small wrapper around ptrace.
 * Act as normal ptrace if ptrace_regs==0.
 * Read data from current process if ptrace_regs!=0.
 */
static int
my_ptrace(	int	request,
		int	pid,
		int	addr,
		int	data)
{
	if (ptrace_regs==0)
		return ptrace(request, pid, addr, data);
	/* we are tracing ourselves! */
	switch (request) {
	case PTRACE_ATTACH:	return 0;
	case PTRACE_CONT:	return 0;
	case PTRACE_DETACH:	return 0;
	case PTRACE_PEEKUSER:
				switch (addr / 4) {
				case EIP:	return ptrace_regs->eip;
				case EBP:	return ptrace_regs->ebp;
				default:	assert(0);
				}
	case PTRACE_PEEKTEXT:	/* FALLTHROUGH */
	case PTRACE_PEEKDATA:	return *(int*)(addr);
	default:		assert(0);
	}
	errno = 1;	/* what to do here? */
	return 1;	/* failed?! */
}

#define	MAXARGS	6

/*
 * To minimize the number of parameters.
 */
typedef struct {
	asymbol**		syms;	/* Sorted! */
	int			symcount;
	debug_function_t**	functions;
	int			functions_size;
} symbol_data_t;

/*
 * Perform a search. A binary search for a symbol.
 */
static void
decode_symbol(	symbol_data_t*		symbol_data,
		const unsigned long	addr,
		char*			buf,
		const int		bufsize)
{
	asymbol**	syms = symbol_data->syms;
	const int	symcount = symbol_data->symcount;
	int		bottom = 0;
	int		top = symcount - 1;
	int		i;
	if (symcount==0) {
		sprintf(buf, "????");
		return;
	}
	while (top>bottom+1) {
		i = (top+bottom) / 2;
		if (bfd_asymbol_value(syms[i])==addr) {
			sprintf(buf, "%s", syms[i]->name);
			return;
		} else if (bfd_asymbol_value(syms[i]) > addr)
			top = i;
		else
			bottom = i;
	}
	i = bottom;
	if (addr<bfd_asymbol_value(syms[i]) || addr>(syms[i]->section->vma+syms[i]->section->_cooked_size))
		sprintf(buf, "????");
	else
		sprintf(buf, "%s + 0x%lx", syms[i]->name, addr-bfd_asymbol_value(syms[i]));
}

/*
 * 1. Perform a binary search for an debug_function_t.
 * 2. Fill buf/bufsize with name, parameters and lineno, if found
 *    Or with '????' otherwise.
 */
static debug_function_t*
find_debug_function_t(	symbol_data_t*		symbol_data,
			const pid_t		pid,
			const unsigned long	fp,	/* frame pointer */
			const unsigned long	addr,
			char*			buf,	/* string buffer */
			const int		bufsize)/* FIXME: not used! */
{
	debug_function_t**	syms = symbol_data->functions;
	debug_function_t*	f = NULL;
	debug_block_t*		block = NULL;
	debug_lineno_t*		lineno = NULL;
	const int		symcount = symbol_data->functions_size;
	int			bottom = 0;
	int			top = symcount - 1;
	int			i;
	char*			bufptr = buf;

	if (symcount==0) {
		sprintf(buf, "????");
		return NULL;
	}
	while (top>bottom+1) {
		i = (top+bottom) / 2;
		if (syms[i]->block->begin_addr==addr) {
			f = syms[i];
			break;
		} else if (syms[i]->block->begin_addr > addr)
				top = i;
		else
			if (syms[i]->block->end_addr >= addr) {
				f = syms[i];
				break;
			} else
				bottom = i;
	}
	i = bottom;
	if (f!=0)
		block = f->block;
	else {
		block = syms[i]->block;
		if (block->begin_addr>=addr && block->end_addr<=addr)
			f = syms[i];
	}
	if (f==0)
		sprintf(buf, "????");
	else {
		/*
		 * Do the backtrace the GDB way...
		 */
		unsigned long	arg;
		/* assert(f->lines_count>0); */
		if (f->lines_count>0) {
			lineno = &f->lines[f->lines_count-1];
			for (i=1; i<f->lines_count; ++i)
				if (f->lines[i].addr>addr) {
					lineno = &f->lines[i-1];
					break;
				}
		}
		bufptr[0] = 0;
		bufptr += sprintf(bufptr, "%s+0x%lx (", f->name, addr-block->begin_addr);
		for (i=0; i<f->argc; ++i) {
			bufptr += sprintf(bufptr, "%s = ", f->argv[i].name);
			/* FIXME: better parameter printing */
			errno = 0;
			arg = my_ptrace(PTRACE_PEEKDATA, pid, fp+f->argv[i].offset, 0);
			assert(errno==0);
			bufptr += sprintf(bufptr, "0x%x", arg);
			if (i!=f->argc-1)
				bufptr += sprintf(bufptr, ", ");
		}
		if (lineno!=0)
			bufptr += sprintf(bufptr, ") at %s:%d", f->filename, lineno->lineno);
	}
	return f;
}

/*
 * Advance through the stacks and display frames as needed.
 */
static int
my_crawl(	int		pid,
		symbol_data_t*	symbol_data,
		int		fout)
{
	unsigned long		pc = 0;
	unsigned long		fp = 0;
	unsigned long		nextfp;
	unsigned long		nargs;
	unsigned long		i;
	unsigned long		arg;
	char			buf[8096];	// FIXME: enough?
	debug_function_t*	f = 0;

	errno = 0;

	pc = my_ptrace(PTRACE_PEEKUSER, pid, EIP * 4, 0);
	if (!errno)
		fp = my_ptrace(PTRACE_PEEKUSER, pid, EBP * 4, 0);

	if (!errno) {
#if 1
		f = find_debug_function_t(symbol_data, pid, fp, pc, buf, sizeof(buf));
		fdprintf(fout,"0x%08lx: %s", pc, buf);
		for ( ; !errno && fp; ) {
			nextfp = my_ptrace(PTRACE_PEEKDATA, pid, fp, 0);
			if (errno)
				break;

			if (f==0) {
				nargs = (nextfp - fp - 8) / 4;
				if (nargs > MAXARGS)
					nargs = MAXARGS;
				if (nargs > 0) {
					fdputs(" (", fout);
					for (i = 1; i <= nargs; i++) {
						arg = my_ptrace(PTRACE_PEEKDATA, pid, fp + 4 * (i + 1), 0);
						if (errno)
							break;
						fdprintf(fout,"%lx", arg);
						if (i < nargs)
							fdputs(", ", fout);
					}
					fdputc(')', fout);
					nargs = nextfp - fp - 8 - (4 * nargs);
					if (!errno && nargs > 0)
						fdprintf(fout," + %lx\n", nargs);
					else
						fdputc('\n', fout);
				} else
					fdputc('\n', fout);
			} else
				fdputc('\n', fout);

			if (errno || !nextfp)
				break;
			pc = my_ptrace(PTRACE_PEEKDATA, pid, fp + 4, 0);
			fp = nextfp;
			if (errno)
				break;
			f = find_debug_function_t(symbol_data, pid, fp, pc, buf, sizeof(buf));
			fdprintf(fout,"0x%08lx: %s", pc, buf);
		}
#else /* 1 */
		decode_symbol(symbol_data, pc, buf, sizeof(buf));
		fdprintf(fout,"0x%08lx: %s", pc, buf);
		for ( ; !errno && fp; ) {
			nextfp = my_ptrace(PTRACE_PEEKDATA, pid, fp, 0);
			if (errno)
				break;

			nargs = (nextfp - fp - 8) / 4;
			if (nargs > MAXARGS)
				nargs = MAXARGS;
			if (nargs > 0) {
				fputs(" (", fout);
				for (i = 1; i <= nargs; i++) {
					arg = my_ptrace(PTRACE_PEEKDATA, pid, fp + 4 * (i + 1), 0);
					if (errno)
						break;
					fdprintf(fout,"%lx", arg);
					if (i < nargs)
						fputs(", ", fout);
				}
				fdputc(')', fout);
				nargs = nextfp - fp - 8 - (4 * nargs);
				if (!errno && nargs > 0)
					fdprintf(fout," + %lx\n", nargs);
				else
					fdputc('\n', fout);
			} else
				fdputc('\n', fout);

			if (errno || !nextfp)
				break;
			pc = my_ptrace(PTRACE_PEEKDATA, pid, fp + 4, 0);
			fp = nextfp;
			if (errno)
				break;
			decode_symbol(symbol_data, pc, buf, sizeof(buf));
			fdprintf(fout,"0x%08lx: %s", pc, buf);
		}
#endif /* !1 */
	}
	if (errno)
		perror("my_crawl");
	return errno;
}

/* layout from /usr/src/linux/arch/i386/kernel/process.c */
static void
show_regs(	signal_regs_t*	regs,
		int		fd)
{
	/* long cr0 = 0L, cr2 = 0L, cr3 = 0L; */

	fdprintf(fd,"\n");
	fdprintf(fd,"FAULT ADDR: %08x\n", regs->fault_addr);
	fdprintf(fd,"EIP: %04x:[<%08x>]",0xffff & regs->xcs,regs->eip);
	if (regs->xcs & 3)
		fdprintf(fd," ESP: %04x:%08x",0xffff & regs->xss,regs->esp);
	/*fdprintf(fd," EFLAGS: %08lx\n",regs->eflags); */
	fdprintf(fd, "\n");
	fdprintf(fd,"EAX: %08x EBX: %08x ECX: %08x EDX: %08x\n",
		regs->eax,regs->ebx,regs->ecx,regs->edx);
	fdprintf(fd,"ESI: %08x EDI: %08x EBP: %08x",
		regs->esi, regs->edi, regs->ebp);
	fdprintf(fd," DS: %04x ES: %04x\n",
		0xffff & regs->xds,0xffff & regs->xes);
	/*
	__asm__("movl %%cr0, %0": "=r" (cr0));
	__asm__("movl %%cr2, %0": "=r" (cr2));
	__asm__("movl %%cr3, %0": "=r" (cr3));
	fprintf(stderr,"CR0: %08lx CR2: %08lx CR3: %08lx\n", cr0, cr2, cr3); */
}

/*
 * Load a BFD for an executable based on PID. Return 0 on failure.
 */
static bfd*
load_bfd(	const int	pid)
{
	char	filename[512];
	bfd*	abfd = 0;

	/* Get the contents from procfs. */
#if 1
	sprintf(filename, "/proc/%d/exe", pid);
#else
	sprintf(filename, "crashing");
#endif 

	if ((abfd = bfd_openr (filename, 0))== NULL)
		bfd_nonfatal (filename);
	else {
		char**	matching;
		assert(bfd_check_format(abfd, bfd_archive)!=true);

		/*
		 * There is no indication in BFD documentation that it should be done.
		 * God knows why...
		 */
		if (!bfd_check_format_matches (abfd, bfd_object, &matching)) {
			bfd_nonfatal (bfd_get_filename (abfd));
			if (bfd_get_error () == bfd_error_file_ambiguously_recognized) {
				list_matching_formats (matching);
				free (matching);
			}
		}
	}
	return abfd;
}

/*
 * Those are for qsort. We need only function addresses, so all the others don't count.
 */
/*
 * Compare two BFD::asymbol-s.
 */
static int 
compare_symbols(const void*	ap,
		const void*	bp)
{
	const asymbol *a = *(const asymbol **)ap;
	const asymbol *b = *(const asymbol **)bp;
	if (bfd_asymbol_value (a) > bfd_asymbol_value (b))
		return 1;
	else if (bfd_asymbol_value (a) < bfd_asymbol_value (b))
		return -1;
	return 0;
}

/*
 * Compare two debug_asymbol_t-s.
 */
static int 
compare_debug_function_t(const void*	ap,
			const void*	bp)
{
	const debug_function_t *a = *(const debug_function_t **)ap;
	const debug_function_t *b = *(const debug_function_t **)bp;
	assert(a->block!=0);
	assert(b->block!=0);
	{
		const bfd_vma	addr1 = a->block->begin_addr;
		const bfd_vma	addr2 = b->block->begin_addr;
		if (addr1 > addr2)
			return 1;
		else if (addr2 > addr1)
			return -1;
	}
	return 0;
}

/*
 * Filter out (in place) symbols that are useless for stack tracing.
 * COUNT is the number of elements in SYMBOLS.
 * Return the number of useful symbols.
 */

static long
remove_useless_symbols(	asymbol**	symbols,
			long		count)
{
	asymbol**	in_ptr = symbols;
	asymbol**	out_ptr = symbols;

	while (--count >= 0) {
		asymbol *sym = *in_ptr++;

		if (sym->name == NULL || sym->name[0] == '\0' || sym->value==0)
			continue;
		if (sym->flags & (BSF_DEBUGGING))
			continue;
		if (bfd_is_und_section (sym->section) || bfd_is_com_section (sym->section))
			continue;
		*out_ptr++ = sym;
	}
	return out_ptr - symbols;
}

/*
 * Debugging information.
 */
static	bfd*			abfd = 0;
static	PTR			dhandle = 0;
static	asymbol**		syms = 0;
static	long			symcount = 0;
static	asymbol**		sorted_syms = 0;
static	long			sorted_symcount = 0;
static	debug_function_t**	functions = 0;
static	int			functions_size = 0;
static	int			sigreport = SIGUSR1;
static	pthread_t		segv_tid;	/* What thread did SEGV? */
static	pid_t			segv_pid;

/*
 * We'll get here after a SIGSEGV. But you can install it on other signals, too :)
 * Because we are in the middle of the SIGSEGV, we are on our own. We can't do
 * any malloc(), any fopen(), nothing. The last is actually a sin. We event can't
 * fprintf(stderr,...)!!!
 */
static void
segv_action(int signo, siginfo_t* siginfo, void* ptr)
{
	symbol_data_t		symbol_data;
	int			fd = -1;

	segv_pid = getpid();
	segv_tid = pthread_self();
	fd = open_log_file(segv_tid, segv_pid);
	/* signal(SIGSEGV, SIG_DFL); */
	ptrace_regs = (signal_regs_t*)ptr;
	assert(ptrace_regs!=0);

	/* Show user how guilty we are. */
	fdprintf(fd,"--------- SEGV in PROCESS %d, THREAD %d ---------------\n", segv_pid, pthread_self());
	show_regs(ptrace_regs, fd);

	/* Some form of stack trace, too. */
	fdprintf(fd, "STACK TRACE:\n");

	symbol_data.syms = sorted_syms;
	symbol_data.symcount = sorted_symcount;
	symbol_data.functions = functions;
	symbol_data.functions_size = functions_size;
	my_crawl(segv_pid, &symbol_data, fd);
	//fflush(stdout);
	close(fd);
	linuxthreads_notify_others(sigreport);
}


static void
report_action(int signo, siginfo_t* siginfo, void* ptr)
{
	const int	pid = getpid();
	pthread_t	tid = pthread_self();
	symbol_data_t	symbol_data;
	int		fd;
	if (pthread_equal(tid, segv_tid)) {
		/* We have already printed our stack trace... */
		return;
	}

	fd = open_log_file(tid, pid);
	fdprintf(fd, "REPORT: CURRENT PROCESS:%d, THREAD:%d\n", getpid(), pthread_self());
	/* signal(SIGSEGV, SIG_DFL); */
	ptrace_regs = (signal_regs_t*)ptr;
	assert(ptrace_regs!=0);

	/* Show user how guilty we are. */
	fdprintf(fd,"--------- STACK TRACE FOR PROCESS %d, THREAD %d ---------------\n", pid, pthread_self());
	show_regs(ptrace_regs, fd);

	/* Some form of stack trace, too. */
	fdprintf(fd, "STACK TRACE:\n");

	symbol_data.syms = sorted_syms;
	symbol_data.symcount = sorted_symcount;
	symbol_data.functions = functions;
	symbol_data.functions_size = functions_size;
	my_crawl(pid, &symbol_data, fd);
	//fflush(stdout);
	close(fd);
	/* Tell segv_thread to proceed after pause(). */
	/*pthread_kill(segv_tid, sigreport);
	kill(segv_pid, sigreport);
	pthread_cancel(tid); */
}

/*
 * Main library routine. Just call it on your program.
 */
int
pstack_install_segv_action(	const char*	path_format_)
{
	const int		pid = getpid();
	struct sigaction	act;

	/* Store what we have to for later usage. */
	path_format = path_format_;

	/* We need a signal action for SIGSEGV and sigreport ! */
	sigreport = SIGUSR1;
	act.sa_handler = 0;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_SIGINFO|SA_ONESHOT;	/* Just one SIGSEGV. */
	act.sa_sigaction = segv_action;
	act.sa_restorer = NULL;
	if (sigaction(SIGSEGV, &act, NULL)!=0) {
		perror("sigaction");
		return 1;
	}
	act.sa_sigaction = report_action;
	act.sa_flags = SA_SIGINFO;		/* But many sigreports. */
	if (sigaction(sigreport, &act, NULL)!=0) {
		perror("sigaction");
		return 1;
	}

	/* And a little setup for libiberty. */
	program_name = "crashing";
	xmalloc_set_program_name (program_name);

	/* Umm, and initialize BFD, too */
	bfd_init();
#if 0
	list_supported_targets(0, stdout);
	set_default_bfd_target(); 
#endif /* 0 */

	if ((abfd = load_bfd(pid))==0)
		fprintf(stderr, "BFD load failed..\n");
	else {
		long	storage_needed = bfd_get_symtab_upper_bound (abfd);
		long	i;
		(void)i;

		if (storage_needed < 0)
			fprintf(stderr, "Symbol table size estimation failure.\n");
		else if (storage_needed > 0) {
			syms = (asymbol **) xmalloc (storage_needed);
			symcount = bfd_canonicalize_symtab (abfd, syms);

			TRACE_FPRINTF((stderr, "TOTAL: %ld SYMBOLS.\n", symcount));
			/* We need debugging info, too! */
			if (symcount==0 || (dhandle = read_debugging_info (abfd, syms, symcount))==0)
				fprintf(stderr, "NO DEBUGGING INFORMATION FOUND.\n");

			/* We make a copy of syms to sort.  We don't want to sort syms
			because that will screw up the relocs.  */
			sorted_syms = (asymbol **) xmalloc (symcount * sizeof (asymbol *));
			memcpy (sorted_syms, syms, symcount * sizeof (asymbol *));

#if 0
			for (i=0; i<symcount; ++i)
				if (syms[i]->name!=0 && strlen(syms[i]->name)>0 && syms[i]->value!=0)
					printf("%08lx T %s\n", syms[i]->section->vma + syms[i]->value, syms[i]->name);
#endif
			sorted_symcount = remove_useless_symbols (sorted_syms, symcount);
			TRACE_FPRINTF((stderr, "SORTED: %ld SYMBOLS.\n", sorted_symcount));

			/* Sort the symbols into section and symbol order */
			qsort (sorted_syms, sorted_symcount, sizeof (asymbol *), compare_symbols);
#if 0
			for (i=0; i<sorted_symcount; ++i)
				if (sorted_syms[i]->name!=0 && strlen(sorted_syms[i]->name)>0 && sorted_syms[i]->value!=0)
					printf("%08lx T %s\n", sorted_syms[i]->section->vma + sorted_syms[i]->value, sorted_syms[i]->name);
#endif
			/* We have symbols, we need debugging info somehow sorted out. */
			if (dhandle==0) {
				fprintf(stderr, "STACK TRACE WILL BE UNCOMFORTABLE.\n");
			} else {
				/* Start collecting the debugging information.... */
				struct pr_handle info;

				info.f = stdout;
				info.indent = 0;
				info.stack = NULL;
				info.parameter = 0;
				info.block = NULL;
				info.function = NULL;
				info.functions_size = 0;
				info.functions_maxsize = 1000;
				info.functions = (debug_function_t*)xmalloc(sizeof(debug_function_t)*info.functions_maxsize);
				debug_write (dhandle, &pr_fns, (PTR) &info);
				TRACE_FPRINTF((stdout, "\n%d DEBUG SYMBOLS\n", info.functions_size));
				assert(info.functions_size!=0);
				functions = xmalloc(sizeof(debug_function_t*)*info.functions_size);
				functions_size = info.functions_size;
				for (i=0; i<functions_size; ++i)
					functions[i] = &info.functions[i];
				/* Sort the symbols into section and symbol order */
				qsort (functions, functions_size, sizeof(debug_function_t*),
					compare_debug_function_t);
#if 0
				for (i=0; i<info.functions_size; ++i)
					fprintf(stdout, "%08lx T %s\n", info.functions[i].block->begin_addr, info.functions[i].name);
#endif
				fflush(stdout);
			}
		} else /* storage_needed == 0 */
			fprintf(stderr, "NO SYMBOLS FOUND.\n");
	}
	return 0;
}

/*********************************************************************/
/*********************************************************************/
/*********************************************************************/

