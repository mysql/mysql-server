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

/* Prints case-convert and sort-convert tabell on stdout. This is used to
   make _ctype.c easyer */

#ifdef DBUG_OFF
#undef DBUG_OFF
#endif

#include <global.h>
#include <ctype.h>
#include <my_sys.h>
#include "m_string.h"

uchar NEAR to_upper[256];
uchar NEAR to_lower[256],NEAR sort_order[256];

static int	ascii_output=1;
static string	tab_names[]={ "to_lower[]={","to_upper[]={","sort_order[]={" };
static uchar*	tabell[]= {to_lower,to_upper,sort_order};

void	get_options(),init_case_convert();

main(argc,argv)
int argc;
char *argv[];
{
  int i,j,ch;
  DBUG_ENTER ("main");
  DBUG_PROCESS (argv[0]);

  get_options(&argc,&argv);
  init_case_convert();
  puts("Tabells for caseconverts and sorttest of characters\n");
  for (i=0 ; i < 3 ; i++)
  {
    printf("uchar NEAR %s\n",tab_names[i]);
    for (j=0 ; j <= 255 ; j++)
    {
      ch=(int) tabell[i][j];
      if (ascii_output && isprint(ch) && ! (ch & 128))
      {
	if (strchr("\\'",(char) ch))
	  printf("'\\%c',  ",ch);
	else
	  printf("'%c',   ",ch);
      }
      else
	printf("'\\%03o',",ch);
      if ((j+1 & 7) == 0)
	puts("");
    }
    puts("};\n");
  }
  DBUG_RETURN(0);
} /* main */

	/* Read options */

void get_options(argc,argv)
register int *argc;
register char **argv[];
{
  int help,version;
  char *pos,*progname;

  progname= (*argv)[0];
  help=0; ascii_output=1;
  while (--*argc >0 && *(pos = *(++*argv)) == '-' )
  {
    while (*++pos)
    {
      version=0;
      switch(*pos) {
      case 'n':					/* Numeric output */
	ascii_output=0;
	break;
      case '#':
	DBUG_PUSH (++pos);
      *(pos--) = '\0';			/* Skippa argument */
      break;
      case 'V':
	version=1;
      case 'I':
      case '?':
	printf("%s  Ver 1.0\n",progname);
	if (version)
	  break;
	puts("Output tabells of to_lower[], to_upper[] and sortorder[]\n");
	printf("Usage: %s [-n?I]\n",progname);
	puts("Options: -? or -I \"Info\" -n \"numeric output\"");
	break;
      default:
	fprintf(stderr,"illegal option: -%c\n",*pos);
	break;
      }
    }
  }
  return;
} /* get_options */


	/* set up max character for which isupper() and toupper() gives */
	/* right answer. Is usually 127 or 255 */

#ifdef USE_INTERNAL_CTYPE
#define MAX_CHAR_OK	CHAR_MAX		/* All chars is right */
#else
#define MAX_CHAR_OK	127			/* 7 Bit ascii */
#endif

	/* Initiate arrays for case-conversation */

void init_case_convert()
{
  reg1 int16 i;
  reg2 uchar *higher_pos,*lower_pos;
  DBUG_ENTER("init_case_convert");

  for (i=0 ; i <= MAX_CHAR_OK ; i++)
  {
    to_upper[i]= sort_order[i]= (islower(i) ? toupper(i) : (char) i);
    to_lower[i]=  (isupper(i) ? tolower(i) : (char) i);
  }
#if MAX_CHAR_OK != 255
  for (i--; i++ < 255 ;)
    to_upper[i]= sort_order[i]= to_lower[i]= (char) i;
#endif

#ifdef MSDOS
  higher_pos= (uchar * ) "\217\216\231\232\220"; /* Extra chars to konv. */
  lower_pos=  (uchar * ) "\206\204\224\201\202";
#else
#if defined(HPUX) && ASCII_BITS_USED == 8
  higher_pos= (uchar * ) "\xd0\xd8\xda\xdb\xdc\xd3";
  lower_pos=  (uchar * ) "\xd4\xcc\xce\xdf\xc9\xd7";
#else
#ifdef USE_INTERNAL_CTYPE
  higher_pos=lower_pos= (uchar* ) "";		/* System converts chars */
#else
#if defined(DEC_MULTINATIONAL_CHAR) || defined(HP_MULTINATIONAL_CHAR)
  higher_pos= (uchar * ) "\305\304\326\311\334";
  lower_pos=  (uchar * ) "\345\344\366\351\374";
#else
  higher_pos= (uchar * ) "[]\\@^";
  lower_pos=  (uchar * ) "{}|`~";
#endif
#endif /* USE_INTERNAL_CTYPE */
#endif /* HPUX */
#endif /* MSDOS */

  while (*higher_pos)
  {
    to_upper[ *lower_pos ] = sort_order[ *lower_pos ] = (char) *higher_pos;
    to_lower[ *higher_pos++ ] = (char) *lower_pos++;
  }

	/* sets upp sortorder; higer_pos character (upper and lower) is */
	/* changed to lower_pos character */

#ifdef MSDOS
  higher_pos= (uchar *) "\217\216\231\232\220";
  lower_pos=  (uchar *) "\216\217\231YE";
#else
#if defined(HPUX) && ASCII_BITS_USED == 8
  higher_pos= lower_pos= (uchar *) "";		/* Tecknen i r{tt ordning */
#else
#ifdef USE_ISO_8859_1				/* As in USG5 ICL-386 */
  higher_pos= (uchar *) "\305\304\326\334\311";
  lower_pos=  (uchar *) "\304\305\326YE";
#else
  higher_pos= (uchar *) "][\\~`";		/* R{tt ordning p} tecknen */
  lower_pos= (uchar *)	"[\\]YE";		/* Ordning enligt ascii */
#endif /* USE_ISO_8859_1 */
#endif /* HPUX */
#endif /* MSDOS */

  while (*higher_pos)
  {
    sort_order[ *higher_pos ] =
      sort_order[(uchar)to_lower[*higher_pos]] = *lower_pos;
    higher_pos++; lower_pos++;
  }
  DBUG_VOID_RETURN;
} /* init_case_convert */
