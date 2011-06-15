/* Init cclasses array from ctypes */

#include <my_global.h>
#include <m_ctype.h>
#include <m_string.h>
#include "cclass.h"
#include "my_regex.h"

static my_bool regex_inited=0;
extern my_regex_stack_check_t my_regex_enough_mem_in_stack;

void my_regex_init(const CHARSET_INFO *cs, my_regex_stack_check_t func)
{
  char buff[CCLASS_LAST][256];
  int  count[CCLASS_LAST];
  uint i;

  if (!regex_inited)
  {
    regex_inited=1;
    my_regex_enough_mem_in_stack= func;
    memset(&count, 0, sizeof(count));

    for (i=1 ; i<= 255; i++)
    {
      if (my_isalnum(cs,i))
	buff[CCLASS_ALNUM][count[CCLASS_ALNUM]++]=(char) i;
      if (my_isalpha(cs,i))
	buff[CCLASS_ALPHA][count[CCLASS_ALPHA]++]=(char) i;
      if (my_iscntrl(cs,i))
	buff[CCLASS_CNTRL][count[CCLASS_CNTRL]++]=(char) i;
      if (my_isdigit(cs,i))
	buff[CCLASS_DIGIT][count[CCLASS_DIGIT]++]=(char) i;
      if (my_isgraph(cs,i))
	buff[CCLASS_GRAPH][count[CCLASS_GRAPH]++]=(char) i;
      if (my_islower(cs,i))
	buff[CCLASS_LOWER][count[CCLASS_LOWER]++]=(char) i;
      if (my_isprint(cs,i))
	buff[CCLASS_PRINT][count[CCLASS_PRINT]++]=(char) i;
      if (my_ispunct(cs,i))
	buff[CCLASS_PUNCT][count[CCLASS_PUNCT]++]=(char) i;
      if (my_isspace(cs,i))
	buff[CCLASS_SPACE][count[CCLASS_SPACE]++]=(char) i;
      if (my_isupper(cs,i))
	buff[CCLASS_UPPER][count[CCLASS_UPPER]++]=(char) i;
      if (my_isxdigit(cs,i))
	buff[CCLASS_XDIGIT][count[CCLASS_XDIGIT]++]=(char) i;
    }
    buff[CCLASS_BLANK][0]=' ';
    buff[CCLASS_BLANK][1]='\t';
    count[CCLASS_BLANK]=2;
    for (i=0; i < CCLASS_LAST ; i++)
    {
      char *tmp=(char*) malloc(count[i]+1);
      if (!tmp)
      {
	/*
	  This is very unlikely to happen as this function is called once
	  at program startup
	*/
	fprintf(stderr,
		"Fatal error: Can't allocate memory in regex_init\n");
	exit(1);
      }
      memcpy(tmp,buff[i],count[i]*sizeof(char));
      tmp[count[i]]=0;
      cclasses[i].chars=tmp;
    }
  }
  return;
}

void my_regex_end()
{
  if (regex_inited)
  {
    int i;
    for (i=0; i < CCLASS_LAST ; i++)
      free((char*) cclasses[i].chars);
    my_regex_enough_mem_in_stack= NULL;
    regex_inited=0;
  }
}


