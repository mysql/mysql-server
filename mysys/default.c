/* Copyright (C) 2000-2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/****************************************************************************
 Add all options from files named "group".cnf from the default_directories
 before the command line arguments.
 On Windows defaults will also search in the Windows directory for a file
 called 'group'.ini
 As long as the program uses the last argument for conflicting
 options one only have to add a call to "load_defaults" to enable
 use of default values.
 pre- and end 'blank space' are removed from options and values. The
 following escape sequences are recognized in values:  \b \t \n \r \\

 The following arguments are handled automaticly;  If used, they must be
 first argument on the command line!
 --no-defaults	; no options are read.
 --defaults-file=full-path-to-default-file	; Only this file will be read.
 --defaults-extra-file=full-path-to-default-file ; Read this file before ~/
 --print-defaults	; Print the modified command line and exit
****************************************************************************/

#include "mysys_priv.h"
#include "m_string.h"
#include "m_ctype.h"
#include <my_dir.h>

char *defaults_extra_file=0;

/* Which directories are searched for options (and in which order) */

const char *default_directories[]= {
#ifdef __WIN__
"C:/",
#elif defined(__NETWARE__)
"sys:/etc/",
#else
"/etc/",
#endif
#ifdef DATADIR
DATADIR,
#endif
"",					/* Place for defaults_extra_dir */
#if !defined(__WIN__) && !defined(__NETWARE__)
"~/",
#endif
NullS,
};

#define default_ext	".cnf"		/* extension for config file */
#ifdef __WIN__
#include <winbase.h>
#define windows_ext	".ini"
#endif

/*
   This structure defines the context that we pass to callback
   function 'handle_default_option' used in search_default_file
   to process each option. This context is used if search_default_file
   was called from load_defaults.
*/

struct handle_option_ctx
{
   MEM_ROOT *alloc;
   DYNAMIC_ARRAY *args;
   TYPELIB *group;
};

static int search_default_file(Process_option_func func, void *func_ctx,
			       const char *dir, const char *config_file,
			       const char *ext);

static char *remove_end_comment(char *ptr);


/*
  Process config files in default directories.

  SYNOPSIS
  search_files()
  conf_file                   Basename for configuration file to search for.
                              If this is a path, then only this file is read.
  argc                        Pointer to argc of original program
  argv                        Pointer to argv of original program
  args_used                   Pointer to variable for storing the number of
                              arguments used.
  func                        Pointer to the function to process options
  func_ctx                    It's context. Usually it is the structure to
                              store additional options.
  DESCRIPTION

  This function looks for config files in default directories. Then it
  travesrses each of the files and calls func to process each option.

  RETURN
    0  ok
    1  given cinf_file doesn't exist
*/

static int search_files(const char *conf_file, int *argc, char ***argv,
                        uint *args_used, Process_option_func func,
                        void *func_ctx)
{
  const char **dirs, *forced_default_file;
  int error= 0;
  DBUG_ENTER("search_files");

  /* Check if we want to force the use a specific default file */
  forced_default_file= 0;
  if (*argc >= 2)
  {
    if (is_prefix(argv[0][1],"--defaults-file="))
    {
      forced_default_file= strchr(argv[0][1],'=') + 1;
      (*args_used)++;
    }
    else if (is_prefix(argv[0][1],"--defaults-extra-file="))
    {
      defaults_extra_file= strchr(argv[0][1],'=') + 1;
      (*args_used)++;
    }
  }

  if (forced_default_file)
  {
    if ((error= search_default_file(func, func_ctx, "",
                                    forced_default_file, "")) < 0)
      goto err;
    if (error > 0)
    {
      fprintf(stderr, "Could not open required defaults file: %s\n",
              forced_default_file);
      goto err;
    }
  }
  else if (dirname_length(conf_file))
  {
    if ((error= search_default_file(func, func_ctx, NullS, conf_file,
                                    default_ext)) < 0)
      goto err;
  }
  else
  {
#ifdef __WIN__
    char system_dir[FN_REFLEN];
    GetWindowsDirectory(system_dir,sizeof(system_dir));
    if ((search_default_file(func, func_ctx, system_dir, conf_file,
                             windows_ext)))
      goto err;
#endif
#if defined(__EMX__) || defined(OS2)
    if (getenv("ETC") &&
        (search_default_file(func, func_ctx, getenv("ETC"), conf_file,
			     default_ext)) < 0)
      goto err;
#endif
    for (dirs= default_directories ; *dirs; dirs++)
    {
      if (**dirs)
      {
	if (search_default_file(func, func_ctx, *dirs, conf_file, default_ext) < 0)
	  goto err;
      }
      else if (defaults_extra_file)
      {
	if (search_default_file(func, func_ctx, NullS, defaults_extra_file,
				default_ext) < 0)
	  goto err;				/* Fatal error */
      }
    }
  }

  DBUG_RETURN(error);

err:
  fprintf(stderr,"Fatal error in defaults handling. Program aborted\n");
  exit(1);
  return 0;					/* Keep compiler happy */
}


/*
  Simplified version of search_files (no argv, argc to process).

  SYNOPSIS
  process_default_option_files()
  conf_file                   Basename for configuration file to search for.
                              If this is a path, then only this file is read.
  func                        Pointer to the function to process options
  func_ctx                    It's context. Usually it is the structure to
                              store additional options.

  DESCRIPTION

  Often we want only to get options from default config files. In this case we
  don't want to provide any argc and argv parameters. This function is a
  simplified variant of search_files which allows us to forget about
  argc, argv.

  RETURN
    0  ok
    1  given cinf_file doesn't exist
*/

int process_default_option_files(const char *conf_file,
                                 Process_option_func func, void *func_ctx)
{
  int argc= 1;
  /* this is a dummy variable for search_files() */
  uint args_used;

  return search_files(conf_file, &argc, NULL, &args_used, func, func_ctx);
}

/*
  The option handler for load_defaults.

  SYNOPSIS
  handle_deault_option()
  in_ctx                    Handler context. In this case it is a
                            handle_option_ctx structure.
  group_name                The name of the group the option belongs to.
  option                    The very option to be processed. It is already
                            prepared to be used in argv (has -- prefix)

  DESCRIPTION

  This handler checks whether a group is one of the listed and adds an option
  to the array if yes. Some other handler can record, for instance, all groups
  and their options, not knowing in advance the names and amount of groups.

  RETURN
    0 - ok
    1 - error occured
*/

static int handle_default_option(void *in_ctx, const char *group_name,
                                   const char *option)
{
  char *tmp;
  struct handle_option_ctx *ctx;
  ctx= (struct handle_option_ctx *) in_ctx;
  if(find_type((char *)group_name, ctx->group, 3))
  {
    if (!(tmp= alloc_root(ctx->alloc, (uint) strlen(option) + 1)))
      return 1;
    if (insert_dynamic(ctx->args, (gptr) &tmp))
      return 1;
    strmov(tmp, option);
  }

  return 0;
}


/*
  Read options from configurations files

  SYNOPSIS
    load_defaults()
    conf_file			Basename for configuration file to search for.
    				If this is a path, then only this file is read.
    groups			Which [group] entrys to read.
				Points to an null terminated array of pointers
    argc			Pointer to argc of original program
    argv			Pointer to argv of original program

  IMPLEMENTATION

   Read options from configuration files and put them BEFORE the arguments
   that are already in argc and argv.  This way the calling program can
   easily command line options override options in configuration files

   NOTES
    In case of fatal error, the function will print a warning and do
    exit(1)
 
    To free used memory one should call free_defaults() with the argument
    that was put in *argv

   RETURN
     0	ok
     1	The given conf_file didn't exists
*/


int load_defaults(const char *conf_file, const char **groups,
		   int *argc, char ***argv)
{
  DYNAMIC_ARRAY args;
  TYPELIB group;
  my_bool found_print_defaults=0;
  uint args_used=0;
  int error= 0;
  MEM_ROOT alloc;
  char *ptr,**res;
  struct handle_option_ctx ctx;
  DBUG_ENTER("load_defaults");

  init_alloc_root(&alloc,512,0);
  if (*argc >= 2 && !strcmp(argv[0][1],"--no-defaults"))
  {
    /* remove the --no-defaults argument and return only the other arguments */
    uint i;
    if (!(ptr=(char*) alloc_root(&alloc,sizeof(alloc)+
				 (*argc + 1)*sizeof(char*))))
      goto err;
    res= (char**) (ptr+sizeof(alloc));
    res[0]= **argv;				/* Copy program name */
    for (i=2 ; i < (uint) *argc ; i++)
      res[i-1]=argv[0][i];
    res[i-1]=0;					/* End pointer */
    (*argc)--;
    *argv=res;
    *(MEM_ROOT*) ptr= alloc;			/* Save alloc root for free */
    DBUG_RETURN(0);
  }

  group.count=0;
  group.name= "defaults";
  group.type_names= groups;

  for (; *groups ; groups++)
    group.count++;

  if (my_init_dynamic_array(&args, sizeof(char*),*argc, 32))
    goto err;

  ctx.alloc= &alloc;
  ctx.args= &args;
  ctx.group= &group;
  
  error= search_files(conf_file, argc, argv, &args_used,
                      handle_default_option, (void *) &ctx);
  /*
    Here error contains <> 0 only if we have a fully specified conf_file
    or a forced default file
  */
  if (!(ptr=(char*) alloc_root(&alloc,sizeof(alloc)+
			       (args.elements + *argc +1) *sizeof(char*))))
    goto err;
  res= (char**) (ptr+sizeof(alloc));

  /* copy name + found arguments + command line arguments to new array */
  res[0]= argv[0][0];  /* Name MUST be set, even by embedded library */
  memcpy((gptr) (res+1), args.buffer, args.elements*sizeof(char*));
  /* Skip --defaults-file and --defaults-extra-file */
  (*argc)-= args_used;
  (*argv)+= args_used;

  /* Check if we wan't to see the new argument list */
  if (*argc >= 2 && !strcmp(argv[0][1],"--print-defaults"))
  {
    found_print_defaults=1;
    --*argc; ++*argv;				/* skip argument */
  }

  if (*argc)
    memcpy((gptr) (res+1+args.elements), (char*) ((*argv)+1),
	   (*argc-1)*sizeof(char*));
  res[args.elements+ *argc]=0;			/* last null */

  (*argc)+=args.elements;
  *argv= (char**) res;
  *(MEM_ROOT*) ptr= alloc;			/* Save alloc root for free */
  delete_dynamic(&args);
  if (found_print_defaults)
  {
    int i;
    printf("%s would have been started with the following arguments:\n",
	   **argv);
    for (i=1 ; i < *argc ; i++)
      printf("%s ", (*argv)[i]);
    puts("");
    exit(0);
  }
  DBUG_RETURN(error);

 err:
  fprintf(stderr,"Fatal error in defaults handling. Program aborted\n");
  exit(1);
  return 0;					/* Keep compiler happy */
}


void free_defaults(char **argv)
{
  MEM_ROOT ptr;
  memcpy_fixed((char*) &ptr,(char *) argv - sizeof(ptr), sizeof(ptr));
  free_root(&ptr,MYF(0));
}


/*
  Open a configuration file (if exists) and read given options from it
  
  SYNOPSIS
    search_default_file()
    opt_handler                 Option handler function. It is used to process
                                every separate option.
    handler_ctx                 Pointer to the structure to store actual 
                                parameters of the function.
    dir				directory to read
    config_file			Name of configuration file
    ext				Extension for configuration file
    group			groups to read

  RETURN
    0   Success
    -1	Fatal error, abort
     1	File not found (Warning)
*/

static int search_default_file(Process_option_func opt_handler, void *handler_ctx,
			       const char *dir, const char *config_file,
			       const char *ext)
{
  char name[FN_REFLEN+10], buff[4096], curr_gr[4096], *ptr, *end;
  char *value, option[4096];
  FILE *fp;
  uint line=0;
  my_bool found_group=0;

  if ((dir ? strlen(dir) : 0 )+strlen(config_file) >= FN_REFLEN-3)
    return 0;					/* Ignore wrong paths */
  if (dir)
  {
    end=convert_dirname(name, dir, NullS);
    if (dir[0] == FN_HOMELIB)		/* Add . to filenames in home */
      *end++='.';
    strxmov(end,config_file,ext,NullS);
  }
  else
  {
    strmov(name,config_file);
  }
  fn_format(name,name,"","",4);
#if !defined(__WIN__) && !defined(OS2) && !defined(__NETWARE__)
  {
    MY_STAT stat_info;
    if (!my_stat(name,&stat_info,MYF(0)))
      return 1;
    /*
      Ignore world-writable regular files.
      This is mainly done to protect us to not read a file created by
      the mysqld server, but the check is still valid in most context. 
    */
    if ((stat_info.st_mode & S_IWOTH) &&
	(stat_info.st_mode & S_IFMT) == S_IFREG)
    {
      fprintf(stderr, "warning: World-writeable config file %s is ignored\n",
              name);
      return 0;
    }
  }
#endif
  if (!(fp = my_fopen(fn_format(name,name,"","",4),O_RDONLY,MYF(0))))
    return 0;					/* Ignore wrong files */

  while (fgets(buff,sizeof(buff)-1,fp))
  {
    line++;
    /* Ignore comment and empty lines */
    for (ptr=buff ; my_isspace(&my_charset_latin1,*ptr) ; ptr++ ) ;
    if (*ptr == '#' || *ptr == ';' || !*ptr)
      continue;
    if (*ptr == '[')				/* Group name */
    {
      found_group=1;
      if (!(end=(char *) strchr(++ptr,']')))
      {
	fprintf(stderr,
		"error: Wrong group definition in config file: %s at line %d\n",
		name,line);
	goto err;
      }
      for ( ; my_isspace(&my_charset_latin1,end[-1]) ; end--) ;/* Remove end space */
      end[0]=0;

      strnmov(curr_gr, ptr, min((uint) (end-ptr)+1, 4096));
      continue;
    }
    if (!found_group)
    {
      fprintf(stderr,
	      "error: Found option without preceding group in config file: %s at line: %d\n",
	      name,line);
      goto err;
    }
    
   
    end= remove_end_comment(ptr);
    if ((value= strchr(ptr, '=')))
      end= value;				/* Option without argument */
    for ( ; my_isspace(&my_charset_latin1,end[-1]) ; end--) ;
    if (!value)
    {
      strmake(strmov(option,"--"),ptr,(uint) (end-ptr));
      if (opt_handler(handler_ctx, curr_gr, option))
        goto err;
    }
    else
    {
      /* Remove pre- and end space */
      char *value_end;
      for (value++ ; my_isspace(&my_charset_latin1,*value); value++) ;
      value_end=strend(value);
      /*
	We don't have to test for value_end >= value as we know there is
	an '=' before
      */
      for ( ; my_isspace(&my_charset_latin1,value_end[-1]) ; value_end--) ;
      if (value_end < value)			/* Empty string */
	value_end=value;

      /* remove quotes around argument */
      if ((*value == '\"' || *value == '\'') && *value == value_end[-1])
      {
	value++;
	value_end--;
      }
      ptr=strnmov(strmov(option,"--"),ptr,(uint) (end-ptr));
      *ptr++= '=';

      for ( ; value != value_end; value++)
      {
	if (*value == '\\' && value != value_end-1)
	{
	  switch(*++value) {
	  case 'n':
	    *ptr++='\n';
	    break;
	  case 't':
	    *ptr++= '\t';
	    break;
	  case 'r':
	    *ptr++ = '\r';
	    break;
	  case 'b':
	    *ptr++ = '\b';
	    break;
	  case 's':
	    *ptr++= ' ';			/* space */
	    break;
	  case '\"':
	    *ptr++= '\"';
	    break;
	  case '\'':
	    *ptr++= '\'';
	    break;
	  case '\\':
	    *ptr++= '\\';
	    break;
	  default:				/* Unknown; Keep '\' */
	    *ptr++= '\\';
	    *ptr++= *value;
	    break;
	  }
	}
	else
	  *ptr++= *value;
      }
      *ptr=0;
      if (opt_handler(handler_ctx, curr_gr, option))
        goto err;
    }
  }
  my_fclose(fp,MYF(0));
  return(0);

 err:
  my_fclose(fp,MYF(0));
  return -1;					/* Fatal error */
}


static char *remove_end_comment(char *ptr)
{
  char quote= 0;	/* we are inside quote marks */
  char escape= 0;	/* symbol is protected by escape chagacter */

  for (; *ptr; ptr++)
  {
    if ((*ptr == '\'' || *ptr == '\"') && !escape)
    {
      if (!quote)
	quote= *ptr;
      else if (quote == *ptr)
	quote= 0;
    }
    /* We are not inside a string */
    if (!quote && *ptr == '#')
    {
      *ptr= 0;
      return ptr;
    }
    escape= (quote && *ptr == '\\' && !escape);
  }
  return ptr;
}

#include <help_start.h>

void print_defaults(const char *conf_file, const char **groups)
{
#ifdef __WIN__
  bool have_ext=fn_ext(conf_file)[0] != 0;
#endif
  char name[FN_REFLEN];
  const char **dirs;
  puts("\nDefault options are read from the following files in the given order:");

  if (dirname_length(conf_file))
    fputs(conf_file,stdout);
  else
  {
#ifdef __WIN__
    GetWindowsDirectory(name,sizeof(name));
    printf("%s\\%s%s ",name,conf_file,have_ext ? "" : windows_ext);
#endif
#if defined(__EMX__) || defined(OS2)
    if (getenv("ETC"))
      printf("%s\\%s%s ", getenv("ETC"), conf_file, default_ext);
#endif
    for (dirs=default_directories ; *dirs; dirs++)
    {
      const char *pos;
      char *end;
      if (**dirs)
	pos= *dirs;
      else if (defaults_extra_file)
	pos= defaults_extra_file;
      else
	continue;
      end=convert_dirname(name, pos, NullS);
      if (name[0] == FN_HOMELIB)	/* Add . to filenames in home */
	*end++='.';
      strxmov(end,conf_file,default_ext," ",NullS);
      fputs(name,stdout);
    }
    puts("");
  }
  fputs("The following groups are read:",stdout);
  for ( ; *groups ; groups++)
  {
    fputc(' ',stdout);
    fputs(*groups,stdout);
  }
  puts("\nThe following options may be given as the first argument:\n\
--print-defaults	Print the program argument list and exit\n\
--no-defaults		Don't read default options from any options file\n\
--defaults-file=#	Only read default options from the given file #\n\
--defaults-extra-file=# Read this file after the global files are read");
}

#include <help_end.h>
