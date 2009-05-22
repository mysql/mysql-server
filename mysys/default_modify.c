/* Copyright (C) 2005 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "my_global.h"
#include "mysys_priv.h"
#include "m_string.h"
#include <my_dir.h>

#define BUFF_SIZE 1024
#define RESERVE 1024                   /* Extend buffer with this extent */

#ifdef __WIN__
#define NEWLINE "\r\n"
#define NEWLINE_LEN 2
#else
#define NEWLINE "\n"
#define NEWLINE_LEN 1
#endif

static char *add_option(char *dst, const char *option_value,
			const char *option, int remove_option);


/*
  Add/remove option to the option file section.

  SYNOPSYS
    modify_defaults_file()
    file_location     The location of configuration file to edit
    option            The name of the option to look for (can be NULL)
    option value      The value of the option we would like to set (can be NULL)
    section_name      The name of the section (must be NOT NULL)
    remove_option     This defines what we want to remove:
                        - MY_REMOVE_NONE -- nothing to remove;
                        - MY_REMOVE_OPTION -- remove the specified option;
                        - MY_REMOVE_SECTION -- remove the specified section;
  IMPLEMENTATION
    We open the option file first, then read the file line-by-line,
    looking for the section we need. At the same time we put these lines
    into a buffer. Then we look for the option within this section and
    change/remove it. In the end we get a buffer with modified version of the
    file. Then we write it to the file, truncate it if needed and close it.
    Note that there is a small time gap, when the file is incomplete,
    and this theoretically might introduce a problem.

  RETURN
    0 - ok
    1 - some error has occured. Probably due to the lack of resourses
    2 - cannot open the file
*/

int modify_defaults_file(const char *file_location, const char *option,
                         const char *option_value,
                         const char *section_name, int remove_option)
{
  FILE *cnf_file;
  MY_STAT file_stat;
  char linebuff[BUFF_SIZE], *src_ptr, *dst_ptr, *file_buffer;
  size_t opt_len= 0, optval_len= 0, sect_len, new_opt_len, reserve_extended;
  uint nr_newlines= 0, buffer_size;
  my_bool in_section= FALSE, opt_applied= 0;
  int reserve_occupied= 0;
  DBUG_ENTER("modify_defaults_file");

  if (!(cnf_file= my_fopen(file_location, O_RDWR | O_BINARY, MYF(0))))
    DBUG_RETURN(2);

  /* my_fstat doesn't use the flag parameter */
  if (my_fstat(fileno(cnf_file), &file_stat, MYF(0)))
    goto malloc_err;

  if (option && option_value)
  {
    opt_len= strlen(option);
    optval_len= strlen(option_value);
  }

  new_opt_len= opt_len + 1 + optval_len + NEWLINE_LEN;

  /* calculate the size of the buffer we need */
  reserve_extended= (opt_len +
                     1 +                        /* For '=' char */
                     optval_len +               /* Option value len */
                     NEWLINE_LEN +              /* Space for newline */
                     RESERVE);                  /* Some additional space */

  buffer_size= (file_stat.st_size +
                1);                             /* The ending zero */

  /*
    Reserve space to read the contents of the file and some more
    for the option we want to add.
  */
  if (!(file_buffer= (char*) my_malloc(buffer_size + reserve_extended,
                                       MYF(MY_WME))))
    goto malloc_err;

  sect_len= strlen(section_name);

  for (dst_ptr= file_buffer; fgets(linebuff, BUFF_SIZE, cnf_file); )
  {
    /* Skip over whitespaces */
    for (src_ptr= linebuff; my_isspace(&my_charset_latin1, *src_ptr);
	 src_ptr++)
    {}

    if (!*src_ptr) /* Empty line */
    {
      nr_newlines++;
      continue;
    }

    /* correct the option (if requested) */
    if (option && in_section && !strncmp(src_ptr, option, opt_len) &&
        (*(src_ptr + opt_len) == '=' ||
         my_isspace(&my_charset_latin1, *(src_ptr + opt_len)) ||
         *(src_ptr + opt_len) == '\0'))
    {
      char *old_src_ptr= src_ptr;
      src_ptr= strend(src_ptr+ opt_len);        /* Find the end of the line */

      /* could be negative */
      reserve_occupied+= (int) new_opt_len - (int) (src_ptr - old_src_ptr);
      if (reserve_occupied >= (int) reserve_extended)
      {
        reserve_extended= (uint) reserve_occupied + RESERVE;
        if (!(file_buffer= (char*) my_realloc(file_buffer, buffer_size +
                                              reserve_extended,
                                              MYF(MY_WME|MY_FREE_ON_ERROR))))
          goto malloc_err;
      }
      opt_applied= 1;
      dst_ptr= add_option(dst_ptr, option_value, option, remove_option);
    }
    else
    {
      /*
        If we are going to the new group and have an option to apply, do
        it now. If we are removing a single option or the whole section
        this will only trigger opt_applied flag.
      */

      if (in_section && !opt_applied && *src_ptr == '[')
      {
        dst_ptr= add_option(dst_ptr, option_value, option, remove_option);
        opt_applied= 1;           /* set the flag to do write() later */
        reserve_occupied= new_opt_len+ opt_len + 1 + NEWLINE_LEN;
      }

      for (; nr_newlines; nr_newlines--)
        dst_ptr= strmov(dst_ptr, NEWLINE);

      /* Skip the section if MY_REMOVE_SECTION was given */
      if (!in_section || remove_option != MY_REMOVE_SECTION)
        dst_ptr= strmov(dst_ptr, linebuff);
    }
    /* Look for a section */
    if (*src_ptr == '[')
    {
      /* Copy the line to the buffer */
      if (!strncmp(++src_ptr, section_name, sect_len))
      {
        src_ptr+= sect_len;
        /* Skip over whitespaces. They are allowed after section name */
        for (; my_isspace(&my_charset_latin1, *src_ptr); src_ptr++)
        {}

        if (*src_ptr != ']')
        {
          in_section= FALSE;
          continue; /* Missing closing parenthesis. Assume this was no group */
        }

        if (remove_option == MY_REMOVE_SECTION)
          dst_ptr= dst_ptr - strlen(linebuff);

        in_section= TRUE;
      }
      else
        in_section= FALSE; /* mark that this section is of no interest to us */
    }
  }

  /*
    File ended. Apply an option or set opt_applied flag (in case of
    MY_REMOVE_SECTION) so that the changes are saved. Do not do anything
    if we are removing non-existent option.
  */

  if (!opt_applied && in_section && (remove_option != MY_REMOVE_OPTION))
  {
    /* New option still remains to apply at the end */
    if (!remove_option && *(dst_ptr - 1) != '\n')
      dst_ptr= strmov(dst_ptr, NEWLINE);
    dst_ptr= add_option(dst_ptr, option_value, option, remove_option);
    opt_applied= 1;
  }
  for (; nr_newlines; nr_newlines--)
    dst_ptr= strmov(dst_ptr, NEWLINE);

  if (opt_applied)
  {
    /* Don't write the file if there are no changes to be made */
    if (my_chsize(fileno(cnf_file), (my_off_t) (dst_ptr - file_buffer), 0,
                  MYF(MY_WME)) ||
        my_fseek(cnf_file, 0, MY_SEEK_SET, MYF(0)) ||
        my_fwrite(cnf_file, (uchar*) file_buffer, (size_t) (dst_ptr - file_buffer),
                  MYF(MY_NABP)))
      goto err;
  }
  if (my_fclose(cnf_file, MYF(MY_WME)))
    DBUG_RETURN(1);

  my_free(file_buffer, MYF(0));
  DBUG_RETURN(0);

err:
  my_free(file_buffer, MYF(0));
malloc_err:
  my_fclose(cnf_file, MYF(0));
  DBUG_RETURN(1); /* out of resources */
}


static char *add_option(char *dst, const char *option_value,
			const char *option, int remove_option)
{
  if (!remove_option)
  {
    dst= strmov(dst, option);
    if (*option_value)
    {
      *dst++= '=';
      dst= strmov(dst, option_value);
    }
    /* add a newline */
    dst= strmov(dst, NEWLINE);
  }
  return dst;
}
