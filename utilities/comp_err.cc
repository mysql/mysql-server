/*
   Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/*
  Written by Anjuta Widenius
*/

/*
  Creates one include file and multiple language-error message files from one
  multi-language text file.
*/

#include <assert.h>
#include <fcntl.h>
#include <mysql_version.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <new>
#include <set>
#include <utility>

#include "m_ctype.h"
#include "m_string.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_dir.h"
#include "my_getopt.h"
#include "my_io.h"
#include "my_sys.h"
#include "mysql/service_mysql_alloc.h"
#include "prealloced_array.h"
#include "print_version.h"
#include "welcome_copyright_notice.h"

#define MAX_ERROR_NAME_LENGTH 64
#define MAX_ROWS 5000
#define HEADER_LENGTH 32 /* Length of header in errmsg.sys */
#define ERRMSG_VERSION 3 /* Version number of errmsg.sys */
#define DEFAULT_CHARSET_DIR "../share/charsets"
#define ER_PREFIX "ER_"
#define WARN_PREFIX "WARN_"
#define OBSOLETE_ER_PREFIX "OBSOLETE_ER_"
#define OBSOLETE_WARN_PREFIX "OBSOLETE_WARN_"
static char *OUTFILE = (char *)"errmsg.sys";
static char *HEADERFILE = (char *)"mysqld_error.h";
static char *NAMEFILE = (char *)"mysqld_ername.h";
static char *TXTFILE = (char *)"../share/errmsg-utf8.txt";
static char *DATADIRECTORY = (char *)"../share/";
#ifndef DBUG_OFF
static char *default_dbug_option = (char *)"d:t:O,/tmp/comp_err.trace";
#endif

/*
  Header for errmsg.sys files
  Byte 3 is treated as version number for errmsg.sys
    With this version ERRMSG_VERSION = 3, number of bytes
    used for the length, count and offset are increased
    from 2 bytes to 4 bytes.
*/
uchar file_head[] = {254, 254, ERRMSG_VERSION, 1};
/* Store positions to each error message row to store in errmsg.sys header */
uint file_pos[MAX_ROWS];

const char *empty_string = ""; /* For empty states */
/*
  Default values for command line options. See getopt structure for definitions
  for these.
*/

const char *default_language = "eng";
int er_offset = 1000;
bool info_flag = 0;

bool isObsolete(const char *error_name) {
  return is_prefix(error_name, OBSOLETE_ER_PREFIX);
}

/* Storage of one error message row (for one language) */

struct message {
  char *lang_short_name;
  char *text;
};

/* Storage for languages and charsets (from start of error text file) */

struct languages {
  char *lang_long_name;        /* full name of the language */
  char *lang_short_name;       /* abbreviation of the lang. */
  char *charset;               /* Character set name */
  struct languages *next_lang; /* Pointer to next language */
};

/* Name, code and  texts (for all lang) for one error message */

struct errors {
  const char *er_name;               /* Name of the error (ER_HASHCK) */
  int d_code;                        /* Error code number */
  const char *sql_code1;             /* sql state */
  const char *sql_code2;             /* ODBC state */
  struct errors *next_error;         /* Pointer to next error */
  Prealloced_array<message, 10> msg; /* All language texts for this error */

  errors() : msg(PSI_NOT_INSTRUMENTED) {}
};

// Set of reserved section ranges.
using err_range = std::pair<uint, uint>;
std::set<err_range> reserved_sections;

static struct my_option my_long_options[] = {
#ifdef DBUG_OFF
    {"debug", '#', "This is a non-debug version. Catch this and exit", 0, 0, 0,
     GET_DISABLED, OPT_ARG, 0, 0, 0, 0, 0, 0},
#else
    {"debug", '#', "Output debug log", &default_dbug_option,
     &default_dbug_option, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#endif
    {"debug-info", 'T', "Print some debug info at exit.", &info_flag,
     &info_flag, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
    {"help", '?', "Displays this help and exits.", 0, 0, 0, GET_NO_ARG, NO_ARG,
     0, 0, 0, 0, 0, 0},
    {"version", 'V', "Prints version", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0,
     0, 0},
    {"charset", 'C', "Charset dir", &charsets_dir, &charsets_dir, 0, GET_STR,
     REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"in_file", 'F', "Input file", &TXTFILE, &TXTFILE, 0, GET_STR, REQUIRED_ARG,
     0, 0, 0, 0, 0, 0},
    {"out_dir", 'D', "Output base directory", &DATADIRECTORY, &DATADIRECTORY, 0,
     GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"out_file", 'O', "Output filename (errmsg.sys)", &OUTFILE, &OUTFILE, 0,
     GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"header_file", 'H', "mysqld_error.h file ", &HEADERFILE, &HEADERFILE, 0,
     GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"name_file", 'N', "mysqld_ername.h file ", &NAMEFILE, &NAMEFILE, 0,
     GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}};

static struct languages *parse_charset_string(char *str);
static struct errors *parse_error_string(char *ptr, int er_count);
static struct message *parse_message_string(struct message *new_message,
                                            char *str);
static struct message *find_message(struct errors *err, const char *lang,
                                    bool no_default);
static int check_message_format(struct errors *err, const char *mess);
static int parse_input_file(const char *file_name, struct errors **top_error,
                            struct languages **top_language);
static int get_options(int *argc, char ***argv);
static void usage(void);
static bool get_one_option(int optid, const struct my_option *opt,
                           char *argument);
static char *parse_text_line(char *pos);
static int copy_rows(FILE *to, char *row, int row_nr, long start_pos);
static char *parse_default_language(char *str);
static uint parse_error_offset(char *str);
static bool parse_reserved_error_section(char *str);

static char *skip_delimiters(char *str);
static char *get_word(char **str);
static char *find_end_of_word(char *str);
static void clean_up(struct languages *lang_head, struct errors *error_head);
static int create_header_files(struct errors *error_head);
static int create_sys_files(struct languages *lang_head,
                            struct errors *error_head, uint row_count);

int main(int argc, char *argv[]) {
  MY_INIT(argv[0]);
  {
    uint row_count;
    struct errors *error_head;
    struct languages *lang_head;
    DBUG_ENTER("main");

    charsets_dir = DEFAULT_CHARSET_DIR;
    my_umask_dir = 0777;
    if (get_options(&argc, &argv)) DBUG_RETURN(1);
    if (!(row_count = parse_input_file(TXTFILE, &error_head, &lang_head))) {
      fprintf(stderr, "Failed to parse input file %s\n", TXTFILE);
      DBUG_RETURN(1);
    }
    if (row_count > MAX_ROWS) {
      fprintf(stderr, "Found too many error messages. ");
      fprintf(stderr, "Increase MAX_ROWS in utilities/comp_err.cc.\n");
      DBUG_RETURN(1);
    }
#if MYSQL_VERSION_ID >= 50100 && MYSQL_VERSION_ID < 50500
/* Number of error messages in 5.1 - do not change this number! */
#define MYSQL_OLD_GA_ERROR_MESSAGE_COUNT 641
#elif MYSQL_VERSION_ID >= 50500 && MYSQL_VERSION_ID < 50600
/* Number of error messages in 5.5 - do not change this number! */
#define MYSQL_OLD_GA_ERROR_MESSAGE_COUNT 728
#endif
#ifdef MYSQL_OLD_GA_ERROR_MESSAGE_COUNT
    if (row_count != MYSQL_OLD_GA_ERROR_MESSAGE_COUNT) {
      fprintf(stderr, "Can only add new error messages to latest GA. ");
      fprintf(stderr, "Use ER_UNKNOWN_ERROR instead.\n");
      fprintf(stderr, "Expected %u messages, found %u.\n",
              MYSQL_OLD_GA_ERROR_MESSAGE_COUNT, row_count);
      DBUG_RETURN(1);
    }
#endif
    if (lang_head == NULL || error_head == NULL) {
      fprintf(stderr, "Failed to parse input file %s\n", TXTFILE);
      DBUG_RETURN(1);
    }

    if (create_header_files(error_head)) {
      fprintf(stderr, "Failed to create header files\n");
      DBUG_RETURN(1);
    }
    if (create_sys_files(lang_head, error_head, row_count)) {
      fprintf(stderr, "Failed to create sys files\n");
      DBUG_RETURN(1);
    }
    clean_up(lang_head, error_head);
    DBUG_LEAVE; /* Can't use dbug after my_end() */
    my_end(info_flag ? MY_CHECK_ERROR | MY_GIVE_INFO : 0);
    return 0;
  }
}

static void print_escaped_string(FILE *f, const char *str) {
  const char *tmp = str;

  while (tmp[0] != 0) {
    switch (tmp[0]) {
      case '\\':
        fprintf(f, "\\\\");
        break;
      case '\'':
        fprintf(f, "\\\'");
        break;
      case '\"':
        fprintf(f, "\\\"");
        break;
      case '\n':
        fprintf(f, "\\n");
        break;
      case '\r':
        fprintf(f, "\\r");
        break;
      default:
        fprintf(f, "%c", tmp[0]);
    }
    tmp++;
  }
}

static int create_header_files(struct errors *error_head) {
  FILE *er_definef, *er_namef;
  struct errors *tmp_error;
  struct message *er_msg;
  const char *er_text;

  DBUG_ENTER("create_header_files");

  if (!(er_definef = my_fopen(HEADERFILE, O_WRONLY, MYF(MY_WME)))) {
    DBUG_RETURN(1);
  }
  if (!(er_namef = my_fopen(NAMEFILE, O_WRONLY, MYF(MY_WME)))) {
    my_fclose(er_definef, MYF(0));
    DBUG_RETURN(1);
  }

  fprintf(er_definef, ORACLE_GPL_COPYRIGHT_NOTICE("2000"));
  fprintf(er_definef, "/* Autogenerated file, please don't edit */\n\n");
  fprintf(er_namef, ORACLE_GPL_COPYRIGHT_NOTICE("2000"));
  fprintf(er_namef, "/* Autogenerated file, please don't edit */\n\n");

  fprintf(er_definef, "#ifndef MYSQLD_ERROR_INCLUDED\n");
  fprintf(er_definef, "#define MYSQLD_ERROR_INCLUDED\n\n");

  /*
    Find out how many sections of error messages we have, what the first
    number in each section is and the number of messages in each section.
  */
  {
    int error_code = error_head->d_code;
    int section_size[100]; /* Assume that 100 sections is enough. */
    int current_section = 0;
    int section_nr;
    int total_error_count = 0;
    section_size[0] = 0;
    fprintf(er_definef, "static const int errmsg_section_start[] = { %d",
            error_code);
    for (tmp_error = error_head; tmp_error; tmp_error = tmp_error->next_error) {
      if (tmp_error->d_code != error_code) /* Starting new section? */
      {
        fprintf(er_definef, ", %d", tmp_error->d_code);
        error_code = tmp_error->d_code;
        section_size[++current_section] = 0;
      }
      error_code++;
      section_size[current_section]++;
    }
    fprintf(er_definef, " };\n");

    fprintf(er_definef, "static const int errmsg_section_size[] = { %d",
            section_size[0]);
    total_error_count = section_size[0];
    for (section_nr = 1; section_nr <= current_section; section_nr++) {
      fprintf(er_definef, ", %d", section_size[section_nr]);
      total_error_count += section_size[section_nr];
    }
    fprintf(er_definef, " };\n\n");

    fprintf(er_definef, "static const int total_error_count = %d;\n\n",
            total_error_count);
  }

  int error_index = 1, temp_error_index;
  int obsolete_error_count = 0;
  for (tmp_error = error_head; tmp_error; tmp_error = tmp_error->next_error) {
    if (strlen(tmp_error->er_name) > MAX_ERROR_NAME_LENGTH) {
      fprintf(stderr, "Error name [%s] too long.\n", tmp_error->er_name);
      DBUG_RETURN(1);
    }

    /*
       generating mysqld_error.h
       fprintf() will automatically add \r on windows
    */
    if (!isObsolete(tmp_error->er_name)) {
      fprintf(er_definef, "#define %s %d\n", tmp_error->er_name,
              tmp_error->d_code);
      temp_error_index = error_index++;
    } else {
      fprintf(er_definef, "//#define %s %d\n", tmp_error->er_name,
              tmp_error->d_code);
      temp_error_index = 0;
      obsolete_error_count++;
    }

    /*generating er_name file */
    er_msg = find_message(tmp_error, default_language, 0);
    er_text = (er_msg ? er_msg->text : "");
    fprintf(er_namef, "{ \"%s\", %d, \"", tmp_error->er_name,
            tmp_error->d_code);
    print_escaped_string(er_namef, er_text);
    if (tmp_error->sql_code1[0] || tmp_error->sql_code2[0])
      fprintf(er_namef, "\",\"%s\", \"%s\"", tmp_error->sql_code1,
              tmp_error->sql_code2);
    else
      fprintf(er_namef, "\",\"HY000\", \"\""); /* General Error */

    fprintf(er_namef, ", %d },\n", temp_error_index);
  }

  fprintf(er_definef, "static const int obsolete_error_count = %d;\n\n",
          obsolete_error_count);
  /* finishing off with mysqld_error.h */
  fprintf(er_definef, "#endif\n");
  my_fclose(er_definef, MYF(0));
  my_fclose(er_namef, MYF(0));
  DBUG_RETURN(0);
}

static int create_sys_files(struct languages *lang_head,
                            struct errors *error_head, uint row_count) {
  FILE *to;
  uint csnum = 0, length, i, row_nr;
  uchar head[32];
  char outfile[FN_REFLEN], *outfile_end;
  long start_pos;
  struct message *tmp;
  struct languages *tmp_lang;
  struct errors *tmp_error;

  MY_STAT stat_info;
  DBUG_ENTER("create_sys_files");

  /*
     going over all languages and assembling corresponding error messages
  */
  for (tmp_lang = lang_head; tmp_lang; tmp_lang = tmp_lang->next_lang) {
    /* setting charset name */
    if (!(csnum = get_charset_number(tmp_lang->charset, MY_CS_PRIMARY))) {
      fprintf(stderr, "Unknown charset '%s' in '%s'\n", tmp_lang->charset,
              TXTFILE);
      DBUG_RETURN(1);
    }

    outfile_end =
        strxmov(outfile, DATADIRECTORY, tmp_lang->lang_long_name, NullS);
    if (!my_stat(outfile, &stat_info, MYF(0))) {
      if (my_mkdir(outfile, 0777, MYF(0)) < 0) {
        fprintf(stderr, "Can't create output directory for %s\n", outfile);
        DBUG_RETURN(1);
      }
    }

    strxmov(outfile_end, FN_ROOTDIR, OUTFILE, NullS);

    if (!(to = my_fopen(outfile, O_WRONLY | MY_FOPEN_BINARY, MYF(MY_WME))))
      DBUG_RETURN(1);

    /* 4 is for 4 bytes to store row position / error message */
    start_pos = (long)(HEADER_LENGTH + row_count * 4);
    fseek(to, start_pos, 0);
    row_nr = 0;
    for (tmp_error = error_head; tmp_error; tmp_error = tmp_error->next_error) {
      /* dealing with messages */
      tmp = find_message(tmp_error, tmp_lang->lang_short_name, false);

      if (!tmp) {
        fprintf(stderr,
                "Did not find message for %s neither in %s nor in default "
                "language\n",
                tmp_error->er_name, tmp_lang->lang_short_name);
        goto err;
      }
      if (copy_rows(to, tmp->text, row_nr, start_pos)) {
        fprintf(stderr, "Failed to copy rows to %s\n", outfile);
        goto err;
      }
      row_nr++;
    }

    /* continue with header of the errmsg.sys file */
    length = ftell(to) - HEADER_LENGTH - row_count * 4;
    memset(head, 0, HEADER_LENGTH);
    memmove(head, file_head, 4);
    head[4] = 1;
    int4store(head + 6, length);
    int4store(head + 10, row_count);
    head[30] = csnum;

    my_fseek(to, 0l, MY_SEEK_SET);
    if (my_fwrite(to, (uchar *)head, HEADER_LENGTH, MYF(MY_WME | MY_FNABP)))
      goto err;

    for (i = 0; i < row_count; i++) {
      int4store(head, file_pos[i]);
      if (my_fwrite(to, (uchar *)head, 4, MYF(MY_WME | MY_FNABP))) goto err;
    }
    my_fclose(to, MYF(0));
  }
  DBUG_RETURN(0);

err:
  my_fclose(to, MYF(0));
  DBUG_RETURN(1);
}

static void clean_up(struct languages *lang_head, struct errors *error_head) {
  struct languages *tmp_lang, *next_language;
  struct errors *tmp_error, *next_error;

  my_free((void *)default_language);

  for (tmp_lang = lang_head; tmp_lang; tmp_lang = next_language) {
    next_language = tmp_lang->next_lang;
    my_free(tmp_lang->lang_short_name);
    my_free(tmp_lang->lang_long_name);
    my_free(tmp_lang->charset);
    my_free(tmp_lang);
  }

  for (tmp_error = error_head; tmp_error; tmp_error = next_error) {
    next_error = tmp_error->next_error;
    size_t count = (tmp_error->msg).size();
    for (size_t i = 0; i < count; i++) {
      struct message *tmp;
      tmp = &tmp_error->msg[i];
      my_free(tmp->lang_short_name);
      my_free(tmp->text);
    }

    if (tmp_error->sql_code1[0]) my_free((void *)tmp_error->sql_code1);
    if (tmp_error->sql_code2[0]) my_free((void *)tmp_error->sql_code2);
    my_free((void *)tmp_error->er_name);
    tmp_error->~errors();
    my_free(tmp_error);
  }
}

static int parse_input_file(const char *file_name, struct errors **top_error,
                            struct languages **top_lang) {
  FILE *file;
  char *str, buff[1000];
  struct errors *current_error = 0, **tail_error = top_error;
  struct message current_message;
  int rcount = 0; /* Number of error codes in current section. */
  int ecount = 0; /* Number of error codes in total. */
  DBUG_ENTER("parse_input_file");

  *top_error = 0;
  *top_lang = 0;
  if (!(file = my_fopen(file_name, O_RDONLY, MYF(MY_WME)))) DBUG_RETURN(0);

  while ((str = fgets(buff, sizeof(buff), file))) {
    if (is_prefix(str, "language")) {
      if (!(*top_lang = parse_charset_string(str))) {
        fprintf(stderr, "Failed to parse the charset string!\n");
        DBUG_RETURN(0);
      }
      continue;
    }
    if (is_prefix(str, "start-error-number")) {
      if (!(er_offset = parse_error_offset(str))) {
        fprintf(stderr, "Failed to parse the error offset string!\n");
        DBUG_RETURN(0);
      }
      rcount = 0; /* Reset count if a fixed number is set. */
      continue;
    }
    if (is_prefix(str, "reserved-error-section")) {
      if (parse_reserved_error_section(str)) {
        fprintf(stderr, "Failed to parse the reserved error section string.\n");
        DBUG_RETURN(0);
      }
      continue;
    }
    if (is_prefix(str, "default-language")) {
      if (!(default_language = parse_default_language(str))) {
        DBUG_PRINT("info", ("default_slang: %s", default_language));
        fprintf(stderr,
                "Failed to parse the default language line. Aborting\n");
        DBUG_RETURN(0);
      }
      continue;
    }

    if (*str == '\t' || *str == ' ') {
      /* New error message in another language for previous error */
      if (!current_error) {
        fprintf(stderr, "Error in the input file format\n");
        DBUG_RETURN(0);
      }
      if (!parse_message_string(&current_message, str)) {
        fprintf(stderr, "Failed to parse message string for error '%s'",
                current_error->er_name);
        DBUG_RETURN(0);
      }
      if (find_message(current_error, current_message.lang_short_name, true)) {
        fprintf(stderr,
                "Duplicate message string for error '%s'"
                " in language '%s'\n",
                current_error->er_name, current_message.lang_short_name);
        DBUG_RETURN(0);
      }
      if (check_message_format(current_error, current_message.text)) {
        fprintf(stderr,
                "Wrong formatspecifier of error message string"
                " for error '%s' in language '%s'\n",
                current_error->er_name, current_message.lang_short_name);
        DBUG_RETURN(0);
      }
      if (current_error->msg.push_back(current_message)) DBUG_RETURN(0);
      continue;
    }
    if (is_prefix(str, ER_PREFIX) || is_prefix(str, WARN_PREFIX) ||
        is_prefix(str, OBSOLETE_ER_PREFIX) ||
        is_prefix(str, OBSOLETE_WARN_PREFIX)) {
      if (!(current_error = parse_error_string(str, rcount))) {
        fprintf(stderr, "Failed to parse the error name string\n");
        DBUG_RETURN(0);
      }
      rcount++;
      ecount++; /* Count number of unique errors */

      /* add error to the list */
      *tail_error = current_error;
      tail_error = &current_error->next_error;
      continue;
    }
    if (*str == '#' || *str == '\n') continue; /* skip comment or empty lines */

    fprintf(stderr, "Wrong input file format. Stop!\nLine: %s\n", str);
    DBUG_RETURN(0);
  }
  *tail_error = 0; /* Mark end of list */

  my_fclose(file, MYF(0));
  DBUG_RETURN(ecount);
}

static uint parse_error_offset(char *str) {
  char *soffset, *end;
  int error;
  uint ioffset;

  DBUG_ENTER("parse_error_offset");
  /* skipping the "start-error-number" keyword and spaces after it */
  str = find_end_of_word(str);
  str = skip_delimiters(str);

  if (!*str)
    DBUG_RETURN(0); /* Unexpected EOL: No error number after the keyword */

  /* reading the error offset */
  if (!(soffset = get_word(&str))) DBUG_RETURN(0); /* OOM: Fatal error */
  DBUG_PRINT("info", ("default_error_offset: %s", soffset));

  /* skipping space(s) and/or tabs after the error offset */
  str = skip_delimiters(str);
  DBUG_PRINT("info", ("str: %s", str));
  if (*str) {
    /* The line does not end with the error offset -> error! */
    fprintf(stderr, "The error offset line does not end with an error offset");
    DBUG_RETURN(0);
  }
  DBUG_PRINT("info", ("str: %s", str));

  end = 0;
  ioffset = (uint)my_strtoll10(soffset, &end, &error);
  for (auto section : reserved_sections) {
    if (ioffset >= section.first && ioffset <= section.second) {
      fprintf(stderr,
              "start-error-number %u overlaps with the reserved section (%u - "
              "%u).\n",
              ioffset, section.first, section.second);
      DBUG_RETURN(0);
    }
  }

  my_free(soffset);
  DBUG_RETURN(ioffset);
}

/*
  Method to parse "reserved-error-section <sec_start> <sec_end>" from the "str".

  @param   str    A line from error message file to parse.

  @retval  false  SUCCESS.
  @retval  true   FAILURE.
*/
static bool parse_reserved_error_section(char *str) {
  char *offset;
  char *end;
  int error;

  DBUG_ENTER("parse_reserved_error_section");

  /* skipping the "reserved-error-section" keyword and spaces after it */
  str = find_end_of_word(str);
  str = skip_delimiters(str);

  if (!*str)
    DBUG_RETURN(true); /* Unexpected EOL: No error number after the keyword */

  /* reading the section start number */
  if (!(offset = get_word(&str))) DBUG_RETURN(true); /* OOM: Fatal error */
  uint sec_start = static_cast<uint>(my_strtoll10(offset, &end, &error));
  my_free(offset);
  DBUG_PRINT("info", ("reserved_range_start: %u", sec_start));

  /* skipping space(s) and/or tabs after the section start number */
  str = skip_delimiters(str);
  if (!*str) DBUG_RETURN(true); /* Unexpected EOL: No section end number */
  DBUG_PRINT("info", ("str: %s", str));

  /* reading the section end number */
  if (!(offset = get_word(&str))) DBUG_RETURN(true); /* OOM: Fatal error */
  uint sec_end = static_cast<uint>(my_strtoll10(offset, &end, &error));
  my_free(offset);
  DBUG_PRINT("info", ("reserved_range_end: %u", sec_end));

  /* skipping space(s) and/or tabs after the error offset */
  str = skip_delimiters(str);
  DBUG_PRINT("info", ("str: %s", str));
  if (*str) {
    fprintf(stderr, "The line does not end with an error number.\n");
    DBUG_RETURN(true);
  }

  if (sec_start >= sec_end) {
    fprintf(stderr,
            "Section start %u should be smaller than the Section end %u.\n",
            sec_start, sec_end);
    DBUG_RETURN(true);
  }

  /* Check if section overlaps with existing reserved sections */
  for (auto section : reserved_sections) {
    if (sec_start <= section.second && section.first <= sec_end) {
      fprintf(
          stderr,
          "Section (%u - %u) overlaps with the reserved section (%u - %u).\n",
          sec_start, sec_end, section.first, section.second);
      DBUG_RETURN(true);
    }
  }

  auto ret = reserved_sections.insert(err_range(sec_start, sec_end));
  if (!ret.second) DBUG_RETURN(true);

  DBUG_RETURN(false);
}

/* Parsing of the default language line. e.g. "default-language eng" */

static char *parse_default_language(char *str) {
  char *slang;

  DBUG_ENTER("parse_default_language");
  /* skipping the "default-language" keyword */
  str = find_end_of_word(str);
  /* skipping space(s) and/or tabs after the keyword */
  str = skip_delimiters(str);
  if (!*str) {
    fprintf(stderr,
            "Unexpected EOL: No short language name after the keyword\n");
    DBUG_RETURN(0);
  }

  /* reading the short language tag */
  if (!(slang = get_word(&str))) DBUG_RETURN(0); /* OOM: Fatal error */
  DBUG_PRINT("info", ("default_slang: %s", slang));

  str = skip_delimiters(str);
  DBUG_PRINT("info", ("str: %s", str));
  if (*str) {
    fprintf(stderr,
            "The default language line does not end with short language "
            "name\n");
    DBUG_RETURN(0);
  }
  DBUG_PRINT("info", ("str: %s", str));
  DBUG_RETURN(slang);
}

/*
  Find the message in a particular language

  SYNOPSIS
    find_message()
    err             Error to find message for
    lang            Language of message to find
    no_default      Don't return default (English) if does not exit

  RETURN VALUE
    Returns the message structure if one is found, or NULL if not.
*/
static struct message *find_message(struct errors *err, const char *lang,
                                    bool no_default) {
  struct message *tmp, *return_val = 0;
  DBUG_ENTER("find_message");

  size_t count = (err->msg).size();
  for (size_t i = 0; i < count; i++) {
    tmp = &err->msg[i];

    if (!strcmp(tmp->lang_short_name, lang)) DBUG_RETURN(tmp);
    if (!strcmp(tmp->lang_short_name, default_language)) {
      DBUG_ASSERT(tmp->text[0] != 0);
      return_val = tmp;
    }
  }
  DBUG_RETURN(no_default ? NULL : return_val);
}

/*
  Check message format specifiers against error message for
  previous language

  SYNOPSIS
    checksum_format_specifier()
    msg            String for which to generate checksum
                   for the format specifiers

  RETURN VALUE
    Returns the checksum for all the characters of the
    format specifiers

    Ex.
     "text '%-64.s' text part 2 %d'"
            ^^^^^^              ^^
            characters will be xored to form checksum

    NOTE:
      Does not support format specifiers with positional args
      like "%2$s" but that is not yet supported by my_vsnprintf
      either.
*/

static ha_checksum checksum_format_specifier(const char *msg) {
  ha_checksum chksum = 0;
  const uchar *p = (const uchar *)msg;
  const uchar *start = NULL;
  uint32 num_format_specifiers = 0;
  while (*p) {
    if (*p == '%') {
      start = p + 1; /* Entering format specifier */
      num_format_specifiers++;
    } else if (start) {
      switch (*p) {
        case 'd':
        case 'u':
        case 'x':
        case 's':
          chksum = my_checksum(chksum, (uchar *)start, (uint)(p + 1 - start));
          start = 0; /* Not in format specifier anymore */
          break;

        default:
          break;
      }
    }

    p++;
  }

  if (start) {
    /* Still inside a format specifier after end of string */

    fprintf(stderr,
            "Still inside formatspecifier after end of string"
            " in'%s'\n",
            msg);
    DBUG_ASSERT(start == 0);
  }

  /* Add number of format specifiers to checksum as extra safeguard */
  chksum += num_format_specifiers;

  return chksum;
}

/*
  Check message format specifiers against error message for
  previous language

  SYNOPSIS
    check_message_format()
    err             Error to check message for
    mess            Message to check

  RETURN VALUE
    Returns 0 if no previous error message or message format is ok
*/
static int check_message_format(struct errors *err, const char *mess) {
  struct message *first;
  DBUG_ENTER("check_message_format");

  /*  Get first message(if any) */
  if ((err->msg).empty())
    DBUG_RETURN(0); /* No previous message to compare against */

  first = err->msg.begin();
  DBUG_ASSERT(first != NULL);

  if (checksum_format_specifier(first->text) !=
      checksum_format_specifier(mess)) {
    /* Check sum of format specifiers failed, they should be equal */
    DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}

/*
  Skips spaces and or tabs till the beginning of the next word
  Returns pointer to the beginning of the first character of the word
*/

static char *skip_delimiters(char *str) {
  DBUG_ENTER("skip_delimiters");
  for (; *str == ' ' || *str == ',' || *str == '\t' || *str == '\r' ||
         *str == '\n' || *str == '=';
       str++)
    ;
  DBUG_RETURN(str);
}

/*
  Skips all characters till meets with space, or tab, or EOL
*/

static char *find_end_of_word(char *str) {
  DBUG_ENTER("find_end_of_word");
  for (; *str != ' ' && *str != '\t' && *str != '\n' && *str != '\r' && *str &&
         *str != ',' && *str != ';' && *str != '=';
       str++)
    ;
  DBUG_RETURN(str);
}

/* Read the word starting from *str */

static char *get_word(char **str) {
  char *start = *str;
  DBUG_ENTER("get_word");

  *str = find_end_of_word(start);
  DBUG_RETURN(my_strndup(PSI_NOT_INSTRUMENTED, start, (uint)(*str - start),
                         MYF(MY_WME | MY_FAE)));
}

/*
  Parsing the string with short_lang - message text. Code - to
  remember to which error does the text belong
*/

static struct message *parse_message_string(struct message *new_message,
                                            char *str) {
  char *start;

  DBUG_ENTER("parse_message_string");
  DBUG_PRINT("enter", ("str: %s", str));

  /*skip space(s) and/or tabs in the beginning */
  while (*str == ' ' || *str == '\t' || *str == '\n') str++;

  if (!*str) {
    /* It was not a message line, but an empty line. */
    DBUG_PRINT("info", ("str: %s", str));
    DBUG_RETURN(0);
  }

  /* reading the short lang */
  start = str;
  while (*str != ' ' && *str != '\t' && *str) str++;
  if (!(new_message->lang_short_name =
            my_strndup(PSI_NOT_INSTRUMENTED, start, (uint)(str - start),
                       MYF(MY_WME | MY_FAE))))
    DBUG_RETURN(0); /* Fatal error */
  DBUG_PRINT("info", ("msg_slang: %s", new_message->lang_short_name));

  /*skip space(s) and/or tabs after the lang */
  while (*str == ' ' || *str == '\t' || *str == '\n') str++;

  if (*str != '"') {
    fprintf(stderr, "Unexpected EOL");
    DBUG_PRINT("info", ("str: %s", str));
    DBUG_RETURN(0);
  }

  /* reading the text */
  start = str + 1;
  str = parse_text_line(start);

  if (!(new_message->text =
            my_strndup(PSI_NOT_INSTRUMENTED, start, (uint)(str - start),
                       MYF(MY_WME | MY_FAE))))
    DBUG_RETURN(0); /* Fatal error */
  DBUG_PRINT("info", ("msg_text: %s", new_message->text));

  DBUG_RETURN(new_message);
}

/*
  Parsing the string with error name and codes; returns the pointer to
  the errors struct
*/

static struct errors *parse_error_string(char *str, int er_count) {
  struct errors *new_error;
  DBUG_ENTER("parse_error_string");
  DBUG_PRINT("enter", ("str: %s", str));

  /* create a new element */
  void *rawmem =
      my_malloc(PSI_NOT_INSTRUMENTED, sizeof(*new_error), MYF(MY_WME));
  new_error = new (rawmem) errors();

  /* getting the error name */
  str = skip_delimiters(str);

  if (!(new_error->er_name = get_word(&str)))
    DBUG_RETURN(0); /* OOM: Fatal error */
  DBUG_PRINT("info", ("er_name: %s", new_error->er_name));

  str = skip_delimiters(str);

  /* getting the error code 1 */

  /* Check if code overlaps with any of the reserved error sections */
  for (auto section : reserved_sections) {
    uint new_code = static_cast<uint>(er_offset + er_count);
    if (new_code >= section.first && new_code <= section.second) {
      fprintf(
          stderr,
          "er_name %s overlaps with the reserved error section (%u - %u).\n",
          new_error->er_name, section.first, section.second);
      DBUG_RETURN(0);
    }
  }

  new_error->d_code = er_offset + er_count;
  DBUG_PRINT("info", ("d_code: %d", new_error->d_code));

  str = skip_delimiters(str);

  /* if we reached EOL => no more codes, but this can happen */
  if (!*str) {
    new_error->sql_code1 = empty_string;
    new_error->sql_code2 = empty_string;
    DBUG_PRINT("info", ("str: %s", str));
    DBUG_RETURN(new_error);
  }

  /* getting the sql_code 1 */

  if (!(new_error->sql_code1 = get_word(&str)))
    DBUG_RETURN(0); /* OOM: Fatal error */
  DBUG_PRINT("info", ("sql_code1: %s", new_error->sql_code1));

  str = skip_delimiters(str);

  /* if we reached EOL => no more codes, but this can happen */
  if (!*str) {
    new_error->sql_code2 = empty_string;
    DBUG_PRINT("info", ("str: %s", str));
    DBUG_RETURN(new_error);
  }

  /* getting the sql_code 2 */
  if (!(new_error->sql_code2 = get_word(&str)))
    DBUG_RETURN(0); /* OOM: Fatal error */
  DBUG_PRINT("info", ("sql_code2: %s", new_error->sql_code2));

  str = skip_delimiters(str);
  if (*str) {
    fprintf(stderr, "The error line did not end with sql/odbc code: '%s'\n",
            str);
    DBUG_RETURN(0);
  }

  DBUG_RETURN(new_error);
}

/*
  Parsing the string with full lang name/short lang name/charset;
  returns pointer to the language structure
*/

static struct languages *parse_charset_string(char *str) {
  struct languages *head = 0, *new_lang;
  DBUG_ENTER("parse_charset_string");
  DBUG_PRINT("enter", ("str: %s", str));

  /* skip over keyword */
  str = find_end_of_word(str);
  if (!*str) {
    /* unexpected EOL */
    DBUG_PRINT("info", ("str: %s", str));
    DBUG_RETURN(0);
  }

  str = skip_delimiters(str);
  if (!(*str != ';' && *str)) DBUG_RETURN(0);

  do {
    /*creating new element of the linked list */
    new_lang = (struct languages *)my_malloc(PSI_NOT_INSTRUMENTED,
                                             sizeof(*new_lang), MYF(MY_WME));
    new_lang->next_lang = head;
    head = new_lang;

    /* get the full language name */

    if (!(new_lang->lang_long_name = get_word(&str)))
      DBUG_RETURN(0); /* OOM: Fatal error */

    DBUG_PRINT("info", ("long_name: %s", new_lang->lang_long_name));

    /* getting the short name for language */
    str = skip_delimiters(str);
    if (!*str) DBUG_RETURN(0); /* Error: No space or tab */

    if (!(new_lang->lang_short_name = get_word(&str)))
      DBUG_RETURN(0); /* OOM: Fatal error */
    DBUG_PRINT("info", ("short_name: %s", new_lang->lang_short_name));

    /* getting the charset name */
    str = skip_delimiters(str);
    if (!(new_lang->charset = get_word(&str))) DBUG_RETURN(0); /* Fatal error */
    DBUG_PRINT("info", ("charset: %s", new_lang->charset));

    /* skipping space, tab or "," */
    str = skip_delimiters(str);
  } while (*str != ';' && *str);

  DBUG_PRINT("info", ("long name: %s", new_lang->lang_long_name));
  DBUG_RETURN(head);
}

/* Read options */

static bool get_one_option(int optid,
                           const struct my_option *opt MY_ATTRIBUTE((unused)),
                           char *argument MY_ATTRIBUTE((unused))) {
  DBUG_ENTER("get_one_option");
  switch (optid) {
    case 'V':
      print_version();
      exit(0);
    case '?':
      usage();
      exit(0);
    case '#':
      DBUG_PUSH(argument ? argument : default_dbug_option);
      break;
  }
  DBUG_RETURN(0);
}

static void usage(void) {
  DBUG_ENTER("usage");
  print_version();
  printf(
      "This software comes with ABSOLUTELY NO WARRANTY. "
      "This is free software,\n"
      "and you are welcome to modify and redistribute it under the GPL "
      "license.\n"
      "Usage:\n");
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
  DBUG_VOID_RETURN;
}

static int get_options(int *argc, char ***argv) {
  int ho_error;
  DBUG_ENTER("get_options");

  if ((ho_error = handle_options(argc, argv, my_long_options, get_one_option)))
    DBUG_RETURN(ho_error);
  DBUG_RETURN(0);
}

/*
  Read rows and remember them until row that start with char Converts
  row as a C-compiler would convert a textstring
*/

static char *parse_text_line(char *pos) {
  int i, nr;
  char *row = pos;
  size_t len;
  DBUG_ENTER("parse_text_line");

  len = strlen(pos);
  while (*pos) {
    if (*pos == '\\') {
      switch (*++pos) {
        case '\\':
        case '"':
          (void)memmove(pos - 1, pos, len - (row - pos));
          break;
        case 'n':
          pos[-1] = '\n';
          (void)memmove(pos, pos + 1, len - (row - pos));
          break;
        default:
          if (*pos >= '0' && *pos < '8') {
            nr = 0;
            for (i = 0; i < 3 && (*pos >= '0' && *pos < '8'); i++)
              nr = nr * 8 + (*(pos++) - '0');
            pos -= i;
            pos[-1] = nr;
            (void)memmove(pos, pos + i, len - (row - pos));
          } else if (*pos)
            (void)memmove(pos - 1, pos, len - (row - pos)); /* Remove '\' */
      }
    } else
      pos++;
  }
  while (pos > row + 1 && *pos != '"') pos--;
  *pos = 0;
  DBUG_RETURN(pos);
}

/* Copy rows from memory to file and remember position */

static int copy_rows(FILE *to, char *row, int row_nr, long start_pos) {
  DBUG_ENTER("copy_rows");

  file_pos[row_nr] = (int)(ftell(to) - start_pos);
  if (fputs(row, to) == EOF || fputc('\0', to) == EOF) {
    fprintf(stderr, "Can't write to outputfile\n");
    DBUG_RETURN(1);
  }

  DBUG_RETURN(0);
}
