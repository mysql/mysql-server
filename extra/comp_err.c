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

/* Saves all errmesg in a header file, updated by me, in a compact file  */

#include <my_global.h>
#include <m_ctype.h>
#include <my_sys.h>
#include <m_string.h>

#define MAXLENGTH 1000
#define MAX_ROWS  1000
#define MAX_FILES 10

int	row_count;
uint	file_pos[MAX_ROWS],file_row_pos[MAX_FILES];
my_string saved_row[MAX_ROWS];
uchar   file_head[]= { 254,254,2,1 };

static void get_options(int *argc,char **argv[]);
static int count_rows(FILE *from,pchar c, pchar c2);
static int remember_rows(FILE *from,pchar c);
static int copy_rows(FILE *to);


	/* Functions defined in this file */

int main(int argc,char *argv[])
{
  int i,error,files,length;
  uchar head[32];
  FILE *from,*to;
  MY_INIT(argv[0]);

  get_options(&argc,&argv);
  error=1;
  row_count=files=0;

  to=0;
  for ( ; argc-- > 1 ; argv++)
  {
    file_row_pos[files++] = row_count;

    if ((from = fopen(*argv,"r")) == NULL)
    {
      fprintf(stderr,"Can't open file '%s'\n",*argv);
      return(1);
    }

    VOID(count_rows(from,'"','}'));	/* Calculate start-info */
    if (remember_rows(from,'}') < 0)	/* Remember rows */
    {
      fprintf(stderr,"Can't find textrows in '%s'\n",*argv);
      fclose(from);
      goto end;
    }
    fclose(from);
  }

  if ((to=my_fopen(*argv,O_WRONLY | FILE_BINARY,MYF(0))) == NULL)
  {
    fprintf(stderr,"Can't create file '%s'\n",*argv);
    return(1);
  }

  fseek(to,(long) (32+row_count*2),0);
  if (copy_rows(to))
    goto end;

  length=ftell(to)-32-row_count*2;

  bzero((gptr) head,32);		/* Save Header & pointers */
  bmove((byte*) head,(byte*) file_head,4);
  head[4]=files;
  int2store(head+6,length);
  int2store(head+8,row_count);
  for (i=0 ; i<files ; i++)
  {
    int2store(head+10+i+i,file_row_pos[i]);
  }

  fseek(to,0l,0);
  if (fwrite(head,1,32,to) != 32)
    goto end;

  for (i=0 ; i<row_count ; i++)
  {
    int2store(head,file_pos[i]);
    if (fwrite(head,1,2,to) != 2)
      goto end;
  }
  error=0;
  printf("Found %d messages in language file %s\n",row_count,*argv);

 end:
  if (to)
    fclose(to);
  if (error)
    fprintf(stderr,"Can't uppdate messagefile %s, errno: %d\n",*argv,errno);

  exit(error);
  return(0);
} /* main */


	/* Read options */

static void get_options(register int *argc,register char **argv[])
{
  int help=0;
  char *pos,*progname;

  progname= (*argv)[0];
  while (--*argc >0 && *(pos = *(++*argv)) == '-' ) {
    while (*++pos)
    switch(*pos) {
    case '#':
      DBUG_PUSH (++pos);
      *(pos--) = '\0';			/* Skippa argument */
      break;
    case 'V':
      printf("%s  (Compile errormessage)  Ver 1.3\n",progname);
      break;
    case 'I':
    case '?':
      printf("         %s  (Compile errormessage)  Ver 1.3\n",progname);
      puts("This software comes with ABSOLUTELY NO WARRANTY. This is free software,\nand you are welcome to modify and redistribute it under the GPL license\n");
      printf("Usage:   %s [-?] [-I] [-V] fromfile[s] tofile\n",progname);
      puts("Options: -Info -Version\n");
      help=1;
      break;
   default:
     fprintf(stderr,"illegal option: -%c\n",*pos);
     fprintf(stderr,"legal options:  -?IV\n");
     break;
    }
  }
  if (*argc < 2)
  {
    if (!help)
      printf("Usage: %s [-?] [-I] [-V] fromfile[s] tofile\n",progname);
    exit(-1);
  }
  return;
} /* get_options */


	/* Count rows in from-file until row that start with char is found */

static int count_rows(FILE *from, pchar c, pchar c2)
{
  int count;
  long pos;
  char rad[MAXLENGTH];
  DBUG_ENTER("count_rows");

  pos=ftell(from); count=0;
  while (fgets(rad,MAXLENGTH,from) != NULL)
  {
    if (rad[0] == c || rad[0] == c2)
      break;
    count++;
    pos=ftell(from);
  }
  fseek(from,pos,0);		/* Position to beginning of last row */
  DBUG_PRINT("exit",("count: %d",count));
  DBUG_RETURN(count);
} /* count_rows */


	/* Read rows and remember them until row that start with char */
	/* Converts row as a C-compiler would convert a textstring */

static int remember_rows(FILE* from, pchar c)
{
  int i,nr,start_count,found_end;
  char row[MAXLENGTH],*pos;
  DBUG_ENTER("remember_rows");

  start_count=row_count; found_end=0;
  while (fgets(row,MAXLENGTH,from) != NULL)
  {
    if (row[0] == c)
    {
      found_end=1;
      break;
    }
    for (pos=row ; *pos ;)
    {
      if (*pos == '\\')
      {
	switch (*++pos) {
	case '\\':
	case '"':
	  VOID(strmov(pos-1,pos));
	  break;
	case 'n':
	  pos[-1]='\n';
	  VOID(strmov(pos,pos+1));
	  break;
	default:
	  if (*pos >= '0' && *pos <'8')
	  {
	    nr=0;
	    for (i=0 ; i<3 && (*pos >= '0' && *pos <'8' ) ; i++)
	      nr=nr*8+ (*(pos++) -'0');
	    pos-=i;
	    pos[-1]=nr;
	    VOID(strmov(pos,pos+i));
	  }
	  else if (*pos)
	    VOID(strmov(pos-1,pos));			/* Remove '\' */
	}
      }
      else pos++;
    }
    while (pos >row+1 && *pos != '"')
      pos--;

    if (!(saved_row[row_count] = (my_string) my_malloc((uint) (pos-row),
						       MYF(MY_WME))))
      DBUG_RETURN(-1);
    *pos=0;
    VOID(strmov(saved_row[row_count],row+1));
    row_count++;
  }
  if (row_count-start_count == 0 && ! found_end)
    DBUG_RETURN(-1);				/* Found nothing */
  DBUG_RETURN(row_count-start_count);
} /* remember_rows */


	/* Copy rows from memory to file and remember position */


static int copy_rows(FILE *to)
{
  int row_nr;
  long start_pos;
  DBUG_ENTER("copy_rows");

  start_pos=ftell(to);
  for (row_nr =0 ; row_nr < row_count; row_nr++)
  {
    file_pos[row_nr]= (int) (ftell(to)-start_pos);
    if (fputs(saved_row[row_nr],to) == EOF || fputc('\0',to) == EOF)
    {
      fprintf(stderr,"Can't write to outputfile\n");
      DBUG_RETURN(1);
    }
    my_free((gptr) saved_row[row_nr],MYF(0));
  }
  DBUG_RETURN(0);
} /* copy_rows */
