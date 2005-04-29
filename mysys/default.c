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

#ifdef __WIN__
static const char *f_extensions[]= { ".ini", ".cnf", 0 };
#else
static const char *f_extensions[]= { ".cnf", 0 };
#endif

static int search_default_file(DYNAMIC_ARRAY *args,MEM_ROOT *alloc,
			       const char *dir, const char *config_file,
			       TYPELIB *group);

static int search_default_file_with_ext(DYNAMIC_ARRAY *args, MEM_ROOT *alloc,
					const char *dir, const char *ext,
					const char *config_file,
					TYPELIB *group, int recursion_level);

static char *remove_end_comment(char *ptr);


/*
  Gets --defaults-file and --defaults-extra-file options from command line.

  SYNOPSIS
    get_defaults_files()
    argc			Pointer to argc of original program
    argv			Pointer to argv of original program
    defaults                    --defaults-file option
    extra_defaults              --defaults-extra-file option

  RETURN
    defaults and extra_defaults will be set to appropriate items
    of argv array, or to NULL if there are no such options
*/

void get_defaults_files(int argc, char **argv,
                        char **defaults, char **extra_defaults)
{
  *defaults=0;
  *extra_defaults=0;
  if (argc >= 2)
  {
    if (is_prefix(argv[1],"--defaults-file="))
      *defaults= argv[1];
    else if (is_prefix(argv[1],"--defaults-extra-file="))
      *extra_defaults= argv[1];
  }
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
     2	The given conf_file was not a normal readable file
*/


int load_defaults(const char *conf_file, const char **groups,
                  int *argc, char ***argv)
{
  DYNAMIC_ARRAY args;
  const char **dirs, *forced_default_file;
  TYPELIB group;
  my_bool found_print_defaults=0;
  uint args_used=0;
  int error= 0;
  MEM_ROOT alloc;
  char *ptr, **res;

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

  get_defaults_files(*argc, *argv,
                      (char **)&forced_default_file, &defaults_extra_file);
  if (forced_default_file)
    forced_default_file= strchr(forced_default_file,'=')+1;
  if (defaults_extra_file)
    defaults_extra_file= strchr(defaults_extra_file,'=')+1;

  args_used+= (forced_default_file ? 1 : 0) + (defaults_extra_file ? 1 : 0);

  group.count=0;
  group.name= "defaults";
  group.type_names= groups;
  for (; *groups ; groups++)
    group.count++;

  if (my_init_dynamic_array(&args, sizeof(char*),*argc, 32))
    goto err;
  if (forced_default_file)
  {
    if ((error= search_default_file_with_ext(&args, &alloc, "", "",
					     forced_default_file,
					     &group, 0)) < 0)
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
    if ((error= search_default_file(&args, &alloc, NullS, conf_file,
				    &group)) < 0)
      goto err;
  }
  else
  {
#ifdef __WIN__
    char system_dir[FN_REFLEN];
    GetWindowsDirectory(system_dir,sizeof(system_dir));
    if ((search_default_file(&args, &alloc, system_dir, conf_file, &group)))
      goto err;
#endif
#if defined(__EMX__) || defined(OS2)
    {
      const char *etc;
      if ((etc= getenv("ETC")) &&
	  (search_default_file(&args, &alloc, etc, conf_file, 
			       &group)) < 0)
      goto err;
    }
#endif
    for (dirs=default_directories ; *dirs; dirs++)
    {
      if (**dirs)
      {
	if (search_default_file(&args, &alloc, *dirs, conf_file,
				&group) < 0)
	  goto err;
      }
      else if (defaults_extra_file)
      {
	if (search_default_file(&args, &alloc, NullS, defaults_extra_file,
				&group) < 0)
	  goto err;				/* Fatal error */
      }
    }
  }
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


static int search_default_file(DYNAMIC_ARRAY *args, MEM_ROOT *alloc,
			       const char *dir,
			       const char *config_file, TYPELIB *group)
{
  char **ext;

  for (ext= (char**) f_extensions; *ext; *ext++)
  {
    int error;
    if ((error= search_default_file_with_ext(args, alloc, dir, *ext,
					     config_file, group, 0)) < 0)
      return error;
  }
  return 0;
}


/*
  Skip over keyword and get argument after keyword

  SYNOPSIS
   get_argument()
   keyword		Include directive keyword
   kwlen		Length of keyword
   ptr			Pointer to the keword in the line under process
   line			line number

  RETURN
   0	error
   #	Returns pointer to the argument after the keyword.
*/

static char *get_argument(const char *keyword, uint kwlen,
                          char *ptr, char *name, uint line)
{
  char *end;

  /* Skip over "include / includedir keyword" and following whitespace */

  for (ptr+= kwlen - 1;
       my_isspace(&my_charset_latin1, ptr[0]);
       ptr++)
  {}

  /*
    Trim trailing whitespace from directory name
    The -1 below is for the newline added by fgets()
    Note that my_isspace() is true for \r and \n
  */
  for (end= ptr + strlen(ptr) - 1;
       my_isspace(&my_charset_latin1, *(end - 1));
       end--)
  {}
  end[0]= 0;                                    /* Cut off end space */

  /* Print error msg if there is nothing after !include* directive */
  if (end <= ptr)
  {
    fprintf(stderr,
	    "error: Wrong '!%s' directive in config file: %s at line %d\n",
	    keyword, name, line);
    return 0;
  }
  return ptr;
}


/*
  Open a configuration file (if exists) and read given options from it

  SYNOPSIS
    search_default_file_with_ext()
    args			Store pointer to found options here
    alloc			Allocate strings in this object
    dir				directory to read
    ext				Extension for configuration file
    config_file                 Name of configuration file
    group			groups to read
    recursion_level             the level of recursion, got while processing
                                "!include" or "!includedir"

  RETURN
    0   Success
    -1	Fatal error, abort
     1	File not found (Warning)
     2  File is not a regular file (Warning)
*/

static int search_default_file_with_ext(DYNAMIC_ARRAY *args, MEM_ROOT *alloc,
					const char *dir, const char *ext,
					const char *config_file,
					TYPELIB *group, int recursion_level)
{
  char name[FN_REFLEN + 10], buff[4096], *ptr, *end, *value, *tmp, **tmp_ext;
  static const char includedir_keyword[]= "includedir";
  static const char include_keyword[]= "include";
  const int max_recursion_level= 10;
  FILE *fp;
  uint line= 0;
  my_bool read_values= 0, found_group= 0;
  uint i;
  MY_DIR *search_dir;
  FILEINFO *search_file;

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
      fprintf(stderr, "warning: World-writable config file %s is ignored\n",
              name);
      return 0;
    }
  }
#endif
  if (!(fp= my_fopen(fn_format(name, name, "", "", 4), O_RDONLY, MYF(0))))
    return 0;					/* Ignore wrong files */

  while (fgets(buff, sizeof(buff) - 1, fp))
  {
    line++;
    /* Ignore comment and empty lines */
    for (ptr= buff; my_isspace(&my_charset_latin1, *ptr); ptr++)
    {}

    if (*ptr == '#' || *ptr == ';' || !*ptr)
      continue;

    /* Configuration File Directives */
    if ((*ptr == '!'))
    {
      if (recursion_level >= max_recursion_level)
      {
        for (end= ptr + strlen(ptr) - 1; 
             my_isspace(&my_charset_latin1, *(end - 1));
             end--)
        {}
        end[0]= 0;
        fprintf(stderr,
                "Warning: skipping '%s' directive as maximum include"
                "recursion level was reached in file %s at line %d\n",
                ptr, name, line);
        continue;
      }

      /* skip over `!' and following whitespace */
      for (++ptr; my_isspace(&my_charset_latin1, ptr[0]); ptr++)
      {}

      if ((!strncmp(ptr, includedir_keyword,
                    sizeof(includedir_keyword) - 1)) &&
          my_isspace(&my_charset_latin1, ptr[sizeof(includedir_keyword) - 1]))
      {
	if (!(ptr= get_argument(includedir_keyword,
                                sizeof(includedir_keyword),
                                ptr, name, line)))
	  goto err;

        if (!(search_dir= my_dir(ptr, MYF(MY_WME))))
          goto err;

        for (i= 0; i < (uint) search_dir->number_off_files; i++)
        {
          search_file= search_dir->dir_entry + i;
          ext= fn_ext(search_file->name);

          /* check extension */
          for (tmp_ext= (char**) f_extensions; *tmp_ext; *tmp_ext++)
          {
            if (!strcmp(ext, *tmp_ext))
              break;
          }

          if (*tmp_ext)
          {
            if (!(tmp= alloc_root(alloc, 2 + strlen(search_file->name)
                                          + strlen(ptr))))
              goto err;

            fn_format(tmp, search_file->name, ptr, "",
                      MY_UNPACK_FILENAME | MY_SAFE_PATH);

            search_default_file_with_ext(args, alloc, "", "", tmp, group,
                                         recursion_level + 1);
          }
        }

        my_dirend(search_dir);
      }
      else if ((!strncmp(ptr, include_keyword, sizeof(include_keyword) - 1)) &&
               my_isspace(&my_charset_latin1, ptr[sizeof(include_keyword)-1]))
      {
	if (!(ptr= get_argument(include_keyword,
                                sizeof(include_keyword), ptr,
                                name, line)))
	  goto err;

        search_default_file_with_ext(args, alloc, "", "", ptr, group,
                                     recursion_level + 1);
      }

      continue;
    }

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
      read_values=find_type(ptr,group,3) > 0;
      continue;
    }
    if (!found_group)
    {
      fprintf(stderr,
	      "error: Found option without preceding group in config file: %s at line: %d\n",
	      name,line);
      goto err;
    }
    if (!read_values)
      continue;
    end= remove_end_comment(ptr);
    if ((value= strchr(ptr, '=')))
      end= value;				/* Option without argument */
    for ( ; my_isspace(&my_charset_latin1,end[-1]) ; end--) ;
    if (!value)
    {
      if (!(tmp=alloc_root(alloc,(uint) (end-ptr)+3)))
	goto err;
      strmake(strmov(tmp,"--"),ptr,(uint) (end-ptr));
      if (insert_dynamic(args,(gptr) &tmp))
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
      if (!(tmp=alloc_root(alloc,(uint) (end-ptr)+3 +
			   (uint) (value_end-value)+1)))
	goto err;
      if (insert_dynamic(args,(gptr) &tmp))
	goto err;
      ptr=strnmov(strmov(tmp,"--"),ptr,(uint) (end-ptr));
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
  my_bool have_ext= fn_ext(conf_file)[0] != 0;
#endif
  char name[FN_REFLEN], **ext;
  const char **dirs;

  puts("\nDefault options are read from the following files in the given order:");

  if (dirname_length(conf_file))
    fputs(conf_file,stdout);
  else
  {
#ifdef __WIN__
    GetWindowsDirectory(name,sizeof(name));
    if (!have_ext)
    {
      for (ext= (char**) f_extensions; *ext; *ext++)
        printf("%s\\%s%s ", name, conf_file, *ext);
    }
    else
        printf("%s\\%s ", name, conf_file);
#endif
#if defined(__EMX__) || defined(OS2)
    {
      const char *etc;

      if ((etc= getenv("ETC")))
      {
	for (ext= (char**) f_extensions; *ext; *ext++)
	  printf("%s\\%s%s ", etc, conf_file, *ext);
      }
    }
#endif
    for (dirs=default_directories ; *dirs; dirs++)
    {
      for (ext= (char**) f_extensions; *ext; *ext++)
      {
	const char *pos;
	char *end;
	if (**dirs)
	  pos= *dirs;
	else if (defaults_extra_file)
	  pos= defaults_extra_file;
	else
	  continue;
	end= convert_dirname(name, pos, NullS);
	if (name[0] == FN_HOMELIB)	/* Add . to filenames in home */
	  *end++='.';
	strxmov(end, conf_file, *ext, " ", NullS);
	fputs(name,stdout);
      }
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
