/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

/* Install or upgrade MySQL server. By Sasha Pachev <sasha@mysql.com>
 */

#define INSTALL_VERSION "1.2"

#define DONT_USE_RAID
#include <my_global.h>
#include <m_ctype.h>
#include <my_sys.h>
#include <m_string.h>
#include <mysql_version.h>
#include <errno.h>
#include <my_getopt.h>

#define ANSWERS_CHUNCK 32

int have_gui=0;

static struct my_option my_long_options[] =
{
  {"help", '?', "Display this help and exit.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"version", 'V', "Output version information and exit.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

/* For now, not much exciting here, but we'll add more once
   we add GUI support
 */
typedef struct
{
  FILE* out;
  FILE* in;
  const char* question;
  int default_ind;
  DYNAMIC_ARRAY answers;
} QUESTION_WIDGET;

static void usage();
static void die(const char* fmt, ...);
static void print_version(void);
static char get_answer_char(int ans_ind);
static int ask_user(const char* question,int default_ind, ...);
static void add_answer(QUESTION_WIDGET* w, const char* ans);
static void display_question(QUESTION_WIDGET* w);
static int init_question_widget(QUESTION_WIDGET* w, const char* question,
				int default_ind);
static void end_question_widget(QUESTION_WIDGET* w);
static int get_answer(QUESTION_WIDGET* w);
static char answer_from_char(char c);
static void invalid_answer(QUESTION_WIDGET* w);

enum {IMODE_STANDARD=0,IMODE_CUSTOM,IMODE_UPGRAGE} install_mode
 = IMODE_STANDARD;

static char get_answer_char(int ans_ind)
{
  return 'a' + ans_ind;
}

static void invalid_answer(QUESTION_WIDGET* w)
{
  if (!have_gui)
  {
    fprintf(w->out, "ERROR: invalid answer, try again...\a\n");
  }
}

static char answer_from_char(char c)
{
  return c - 'a';
}

static void die(const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  fprintf(stderr, "%s: ", my_progname);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
  va_end(args);
  exit(1);
}

static void display_question(QUESTION_WIDGET* w)
{
  if (!have_gui)
  {
    uint i,num_answers=w->answers.elements;
    DYNAMIC_ARRAY* answers = &w->answers;
    fprintf(w->out,"\n%s\n\n",w->question);
    
    for (i=0; i<num_answers; i++)
    {
      char* ans;
      get_dynamic(answers,(gptr)&ans,i);
      fprintf(w->out,"%c - %s\n",get_answer_char(i),ans);
    }
    fprintf(w->out,"q - Abort Install/Upgrade\n\n");
  }
}

static void add_answer(QUESTION_WIDGET* w, const char* ans)
{
  insert_dynamic(&w->answers,(gptr)&ans);
}

static int init_question_widget(QUESTION_WIDGET* w, const char* question,
			       int default_ind)
{
  if (have_gui)
  {
    w->in = w->out = 0;
  }
  else
  {
    w->out = stdout;
    w->in = stdin;
  }
  w->question = question;
  w->default_ind = default_ind;
  if (my_init_dynamic_array(&w->answers,sizeof(char*),
			 ANSWERS_CHUNCK,ANSWERS_CHUNCK))
    die("Out of memory");
  return 0;
}

static void end_question_widget(QUESTION_WIDGET* w)
{
  delete_dynamic(&w->answers);
}

static int get_answer(QUESTION_WIDGET* w)
{
  if (!have_gui)
  {
    char buf[32];
    int ind;
    char c;
    if (!fgets(buf,sizeof(buf),w->in))
      die("Failed fgets on input stream");
    switch ((c=tolower(*buf)))
    {
    case '\n':
      return w->default_ind;
    case 'q':
      die("Install/Upgrade aborted");
    default:
      ind = answer_from_char(c);
      if (ind >= 0 && ind < (int)w->answers.elements)
	return ind;
    }
  }
  return -1;
}

static int ask_user(const char* question,int default_ind, ...)
{
  va_list args;
  char* opt;
  QUESTION_WIDGET w;
  int ans;
  
  va_start(args,default_ind);
  init_question_widget(&w,question,default_ind);
  for (;(opt=va_arg(args,char*));)
  {
    add_answer(&w,opt);
  }
  for (;;)
  {
    display_question(&w);
    if ((ans = get_answer(&w)) >= 0)
      break;
    invalid_answer(&w);
  }
  end_question_widget(&w);
  va_end(args);
  return ans;
}


static my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
	       char *argument __attribute__((unused)))
{
  switch(optid) {
  case 'V':
    print_version();
    exit(0);
  case '?':
    usage();
    exit(0);
  }
  return 0;
}


static int parse_args(int argc, char **argv)
{
  int ho_error;

  if ((ho_error=handle_options(&argc, &argv, my_long_options, get_one_option)))
    exit(ho_error);

  return 0;
}

static void print_version(void)
{
  printf("%s  Ver %s Distrib %s, for %s (%s)\n",my_progname,INSTALL_VERSION,
	 MYSQL_SERVER_VERSION,SYSTEM_TYPE,MACHINE_TYPE);
}

static void usage()
{
  print_version();
  printf("MySQL AB, by Sasha Pachev\n");
  printf("This software comes with ABSOLUTELY NO WARRANTY\n\n");
  printf("Install or upgrade MySQL server.\n\n");
  printf("Usage: %s [OPTIONS] \n", my_progname);
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
}

int main(int argc, char** argv)
{
  MY_INIT(argv[0]);
  parse_args(argc,argv);
  install_mode = ask_user("Please select install/upgrade mode",
			  install_mode, "Standard Install",
			  "Custom Install", "Upgrade",0);
  printf("mode=%d\n", install_mode);
  return 0;
}





