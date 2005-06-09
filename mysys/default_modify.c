/*
  Add/remove option to the option file section.

  SYNOPSYS
    modify_defaults_file()
    file_location     The location of configuration file to edit
    option            option to look for
    option value      The value of the option we would like to set
    section_name      the name of the section
    remove_option     This is true if we want to remove the option.
                      False otherwise.
  IMPLEMENTATION
    We open the option file first, then read the file line-by-line,
    looking for the section we need. At the same time we put these lines
    into a buffer. Then we look for the option within this section and
    change/remove it. In the end we get a buffer with modified version of the
    file. Then we write it to the file, truncate it if needed and close it.

  RETURN
    0 - ok
    1 - some error has occured. Probably due to the lack of resourses
    2 - cannot open the file
*/

#include "my_global.h"
#include "mysys_priv.h"
#include "m_string.h"
#include <my_dir.h>

#define BUFF_SIZE 1024

#ifdef __WIN__
#define NEWLINE "\r\n"
#define NEWLINE_LEN 2
#else
#define NEWLINE "\n"
#define NEWLINE_LEN 1
#endif

static char *add_option(char *dst, const char *option_value,
			const char *option, int remove_option);

int modify_defaults_file(const char *file_location, const char *option,
                         const char *option_value,
                         const char *section_name, int remove_option)
{
  FILE *cnf_file;
  MY_STAT file_stat;
  char linebuff[BUFF_SIZE], *src_ptr, *dst_ptr, *file_buffer;
  uint optlen, optval_len, sect_len, nr_newlines= 0;
  my_bool in_section= FALSE, opt_applied= 0;
  DBUG_ENTER("modify_defaults_file");

  if (!(cnf_file= my_fopen(file_location, O_RDWR | O_BINARY, MYF(0))))
    DBUG_RETURN(2);

  /* my_fstat doesn't use the flag parameter */
  if (my_fstat(fileno(cnf_file), &file_stat, MYF(0)))
    goto err;

  optlen= strlen(option);
  optval_len= strlen(option_value);

  /*
    Reserve space to read the contents of the file and some more
    for the option we want to add.
  */
  if (!(file_buffer= (char*) my_malloc(sizeof(char) *
				       (file_stat.st_size + 
					/* option name len */
					optlen +
					/* reserve space for newline */
					NEWLINE_LEN +
					/* reserve for '=' char */
					1 +           
					/* option value len */
					optval_len +
					/* The ending zero plus some safety */
					FN_REFLEN), MYF(MY_WME))))
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

    if (!opt_applied && in_section && !strncmp(src_ptr, option, optlen) &&
	(*(src_ptr + optlen) == '=' ||
	 my_isspace(&my_charset_latin1, *(src_ptr + optlen)) ||
	 *(src_ptr + optlen) == '\0'))
    {
      dst_ptr= add_option(dst_ptr, option_value, option, remove_option);
      opt_applied= 1;
    }
    else
    {
      /* If going to new group and we have option to apply, do it now */
      if (in_section && !opt_applied && *src_ptr == '[')
      {
	dst_ptr= add_option(dst_ptr, option_value, option, remove_option);

      }
      for (; nr_newlines; nr_newlines--)
	dst_ptr= strmov(dst_ptr, NEWLINE);	
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
	  continue; /* Missing closing parenthesis. Assume this was no group */
        in_section= TRUE;
      }
      else
        in_section= FALSE; /* mark that this section is of no interest to us */
    }
  }
  /* File ended. */
  if (!opt_applied && !remove_option && in_section)
  {
    /* New option still remains to apply at the end */
    if (*(dst_ptr - 1) != '\n')
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
	my_fwrite(cnf_file, file_buffer, (uint) (dst_ptr - file_buffer),
		  MYF(MY_NABP)))
      goto err;
  }
  if (my_fclose(cnf_file, MYF(MY_WME)))
    goto err;

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
