/* Init cclasses array from ctypes */

#include <my_global.h>
#include <m_ctype.h>
#include <m_string.h>
#include "cclass.h"

static bool regex_inited=0;

void regex_init()
{
  char buff[CCLASS_LAST][256];
  int  count[CCLASS_LAST];
  uint i;

  if (!regex_inited)
  {
    regex_inited=1;
    bzero((gptr) &count,sizeof(count));

    for (i=1 ; i<= 255; i++)
    {
      if (isalnum(i))
	buff[CCLASS_ALNUM][count[CCLASS_ALNUM]++]=(char) i;
      if (isalpha(i))
	buff[CCLASS_ALPHA][count[CCLASS_ALPHA]++]=(char) i;
      if (iscntrl(i))
	buff[CCLASS_CNTRL][count[CCLASS_CNTRL]++]=(char) i;
      if (isdigit(i))
	buff[CCLASS_DIGIT][count[CCLASS_DIGIT]++]=(char) i;
      if (isgraph(i))
	buff[CCLASS_GRAPH][count[CCLASS_GRAPH]++]=(char) i;
      if (islower(i))
	buff[CCLASS_LOWER][count[CCLASS_LOWER]++]=(char) i;
      if (isprint(i))
	buff[CCLASS_PRINT][count[CCLASS_PRINT]++]=(char) i;
      if (ispunct(i))
	buff[CCLASS_PUNCT][count[CCLASS_PUNCT]++]=(char) i;
      if (isspace(i))
	buff[CCLASS_SPACE][count[CCLASS_SPACE]++]=(char) i;
      if (isupper(i))
	buff[CCLASS_UPPER][count[CCLASS_UPPER]++]=(char) i;
      if (isxdigit(i))
	buff[CCLASS_XDIGIT][count[CCLASS_XDIGIT]++]=(char) i;
    }
    buff[CCLASS_BLANK][0]=' ';
    buff[CCLASS_BLANK][1]='\t';
    count[CCLASS_BLANK]=2;
    for (i=0; i < CCLASS_LAST ; i++)
    {
      char *tmp=(char*) malloc(count[i]+1);
      memcpy(tmp,buff[i],count[i]*sizeof(char));
      tmp[count[i]]=0;
      cclasses[i].chars=tmp;
    }
  }
  return;
}

void regex_end()
{
  if (regex_inited)
  {
    int i;
    for (i=0; i < CCLASS_LAST ; i++)
      free(cclasses[i].chars);
    regex_inited=0;
  }
}


