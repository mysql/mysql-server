/*
   Copyright (c) 2000, 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; version 2 of
   the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
   02110-1301  USA */

/*
  Replace strings in textfile

  This program replaces strings in files or from stdin to stdout.
  It accepts a list of from-string/to-string pairs and replaces
  each occurrence of a from-string with the corresponding to-string.
  The first occurrence of a found string is matched. If there is more
  than one possibility for the string to replace, longer matches
  are preferred before shorter matches.

  Special characters in from string:
  \^    Match start of line.
  \$	Match end of line.
  \b	Match space-character, start of line or end of line.
        For end \b the next replace starts locking at the end space-character.
        An \b alone or in a string matches only a space-character.
  \r, \t, \v as in C.
  The programs make a DFA-state-machine of the strings and the speed isn't
  dependent on the count of replace-strings (only of the number of replaces).
  A line is assumed ending with \n or \0.
  There are no limit exept memory on length of strings.

  Written by Monty.
  fill_buffer_retaining() is taken from gnu-grep and modified.
*/

#include <my_global.h>
#include <m_ctype.h>
#include <my_sys.h>
#include <m_string.h>
#include <errno.h>

#define PC_MALLOC		256	/* Bytes for pointers */
#define PS_MALLOC		512	/* Bytes for data */

typedef struct st_pointer_array {		/* when using array-strings */
  TYPELIB typelib;				/* Pointer to strings */
  uchar *str;					/* Strings is here */
  uint8	*flag;					/* Flag about each var. */
  uint  array_allocs,max_count,length,max_length;
} POINTER_ARRAY;

#define SPACE_CHAR	256
#define START_OF_LINE	257
#define END_OF_LINE	258
#define LAST_CHAR_CODE	259

typedef struct st_replace {
  my_bool   found;
  struct st_replace *next[256];
} REPLACE;

typedef struct st_replace_found {
  my_bool found;
  char *replace_string;
  uint to_offset;
  int from_offset;
} REPLACE_STRING;

#ifndef WORD_BIT
#define WORD_BIT (8*sizeof(uint))
#endif

	/* functions defined in this file */

static int static_get_options(int *argc,char * * *argv);
static int get_replace_strings(int *argc,char * * *argv,
				   POINTER_ARRAY *from_array,
				   POINTER_ARRAY *to_array);
static int insert_pointer_name(POINTER_ARRAY *pa, char * name);
static void free_pointer_array(POINTER_ARRAY *pa);
static int convert_pipe(REPLACE *,FILE *,FILE *);
static int convert_file(REPLACE *, char *);
static REPLACE *init_replace(char * *from, char * *to,uint count,
                             char * word_end_chars);
static uint replace_strings(REPLACE *rep, char * *start,uint *max_length,
                            char * from);
static int initialize_buffer(void);
static void reset_buffer(void);
static void free_buffer(void);

static int silent=0,verbose=0,updated=0;

	/* The main program */

int main(int argc, char *argv[])
{
  int i,error;
  char word_end_chars[256],*pos;
  POINTER_ARRAY from,to;
  REPLACE *replace;
  MY_INIT(argv[0]);

  if (static_get_options(&argc,&argv))
    exit(1);
  if (get_replace_strings(&argc,&argv,&from,&to))
    exit(1);

  for (i=1,pos=word_end_chars ; i < 256 ; i++)
    if (my_isspace(&my_charset_latin1,i))
      *pos++= (char) i;
  *pos=0;
  if (!(replace=init_replace((char**) from.typelib.type_names,
			     (char**) to.typelib.type_names,
			     (uint) from.typelib.count,word_end_chars)))
    exit(1);
  free_pointer_array(&from);
  free_pointer_array(&to);
  if (initialize_buffer())
    return 1;

  error=0;
  if (argc == 0)
    error=convert_pipe(replace,stdin,stdout);
  else
  {
    while (argc--)
    {
      error=convert_file(replace,*(argv++));
    }
  }
  free_buffer();
  my_end(verbose ? MY_CHECK_ERROR | MY_GIVE_INFO : MY_CHECK_ERROR);
  exit(error ? 2 : 0);
  return 0;					/* No compiler warning */
} /* main */


	/* reads options */
	/* Initiates DEBUG - but no debugging here ! */

static int static_get_options(argc,argv)
int *argc;
char **argv[];
{
  int help,version;
  char *pos;

  silent=verbose=help=0;

  while (--*argc > 0 && *(pos = *(++*argv)) == '-' && pos[1] != '-') {
    while (*++pos)
    {
      version=0;
      switch((*pos)) {
      case 's':
	silent=1;
	break;
      case 'v':
	verbose=1;
	break;
      case '#':
	DBUG_PUSH (++pos);
	pos= (char*) " ";			/* Skip rest of arguments */
	break;
      case 'V':
	version=1;
      case 'I':
      case '?':
	help=1;					/* Help text written */
	printf("%s  Ver 1.4 for %s at %s\n",my_progname,SYSTEM_TYPE,
	       MACHINE_TYPE);
	if (version)
	  break;
	puts("This software comes with ABSOLUTELY NO WARRANTY. This is free software,\nand you are welcome to modify and redistribute it under the GPL license\n");
	puts("This program replaces strings in files or from stdin to stdout.\n"
	     "It accepts a list of from-string/to-string pairs and replaces\n"
	     "each occurrence of a from-string with the corresponding to-string.\n"
         "The first occurrence of a found string is matched. If there is\n"
         "more than one possibility for the string to replace, longer\n"
         "matches are preferred before shorter matches.\n\n"
	     "A from-string can contain these special characters:\n"
	     "  \\^      Match start of line.\n"
	     "  \\$      Match end of line.\n"
	     "  \\b      Match space-character, start of line or end of line.\n"
	     "          For a end \\b the next replace starts locking at the end\n"
	     "          space-character. A \\b alone in a string matches only a\n"
	     "          space-character.\n");
	  printf("Usage: %s [-?svIV] from to from to ... -- [files]\n", my_progname);
	puts("or");
	  printf("Usage: %s [-?svIV] from to from to ... < fromfile > tofile\n", my_progname);
	puts("");
	puts("Options: -? or -I \"Info\"  -s \"silent\"      -v \"verbose\"");
	break;
      default:
	fprintf(stderr,"illegal option: -%c\n",*pos);
	break;
      }
    }
  }
  if (*argc == 0)
  {
    if (!help)
      my_message(0,"No replace options given",MYF(ME_BELL));
    exit(0);					/* Don't use as pipe */
  }
  return(0);
} /* static_get_options */


static int get_replace_strings(argc,argv,from_array,to_array)
int *argc;
char **argv[];
POINTER_ARRAY *from_array,*to_array;
{
  char *pos;

  memset(from_array, 0, sizeof(from_array[0]));
  memset(to_array, 0, sizeof(to_array[0]));
  while (*argc > 0 && (*(pos = *(*argv)) != '-' || pos[1] != '-' || pos[2]))
  {
    insert_pointer_name(from_array,pos);
    (*argc)--;
    (*argv)++;
    if (!*argc || !strcmp(**argv,"--"))
    {
      my_message(0,"No to-string for last from-string",MYF(ME_BELL));
      return 1;
    }
    insert_pointer_name(to_array,**argv);
    (*argc)--;
    (*argv)++;
  }
  if (*argc)
  {					/* Skip "--" argument */
    (*argc)--;
    (*argv)++;
  }
  return 0;
}

static int insert_pointer_name(POINTER_ARRAY *pa,char * name)
{
  uint i,length,old_count;
  uchar *new_pos;
  const char **new_array;
  DBUG_ENTER("insert_pointer_name");

  if (! pa->typelib.count)
  {
    if (!(pa->typelib.type_names=(const char **)
	  my_malloc(((PC_MALLOC-MALLOC_OVERHEAD)/
		     (sizeof(char *)+sizeof(*pa->flag))*
		     (sizeof(char *)+sizeof(*pa->flag))),MYF(MY_WME))))
      DBUG_RETURN(-1);
    if (!(pa->str= (uchar*) my_malloc((uint) (PS_MALLOC-MALLOC_OVERHEAD),
				     MYF(MY_WME))))
    {
      my_free(pa->typelib.type_names);
      DBUG_RETURN (-1);
    }
    pa->max_count=(PC_MALLOC-MALLOC_OVERHEAD)/(sizeof(uchar*)+
					       sizeof(*pa->flag));
    pa->flag= (uint8*) (pa->typelib.type_names+pa->max_count);
    pa->length=0;
    pa->max_length=PS_MALLOC-MALLOC_OVERHEAD;
    pa->array_allocs=1;
  }
  length=(uint) strlen(name)+1;
  if (pa->length+length >= pa->max_length)
  {
    pa->max_length=(pa->length+length+MALLOC_OVERHEAD+PS_MALLOC-1)/PS_MALLOC;
    pa->max_length=pa->max_length*PS_MALLOC-MALLOC_OVERHEAD;
    if (!(new_pos= (uchar*) my_realloc((uchar*) pa->str,
				      (uint) pa->max_length,
				      MYF(MY_WME))))
      DBUG_RETURN(1);
    if (new_pos != pa->str)
    {
      my_ptrdiff_t diff=PTR_BYTE_DIFF(new_pos,pa->str);
      for (i=0 ; i < pa->typelib.count ; i++)
	pa->typelib.type_names[i]= ADD_TO_PTR(pa->typelib.type_names[i],diff,
					      char*);
      pa->str=new_pos;
    }
  }
  if (pa->typelib.count >= pa->max_count-1)
  {
    int len;
    pa->array_allocs++;
    len=(PC_MALLOC*pa->array_allocs - MALLOC_OVERHEAD);
    if (!(new_array=(const char **) my_realloc((uchar*) pa->typelib.type_names,
					       (uint) len/
					 (sizeof(uchar*)+sizeof(*pa->flag))*
					 (sizeof(uchar*)+sizeof(*pa->flag)),
					 MYF(MY_WME))))
      DBUG_RETURN(1);
    pa->typelib.type_names=new_array;
    old_count=pa->max_count;
    pa->max_count=len/(sizeof(uchar*) + sizeof(*pa->flag));
    pa->flag= (uint8*) (pa->typelib.type_names+pa->max_count);
    memcpy((uchar*) pa->flag,(char *) (pa->typelib.type_names+old_count),
	   old_count*sizeof(*pa->flag));
  }
  pa->flag[pa->typelib.count]=0;			/* Reset flag */
  pa->typelib.type_names[pa->typelib.count++]= (char*) (pa->str+pa->length);
  pa->typelib.type_names[pa->typelib.count]= NullS;	/* Put end-mark */
  (void) strmov((char*) pa->str + pa->length, name);
  pa->length+=length;
  DBUG_RETURN(0);
} /* insert_pointer_name */


	/* free pointer array */

static void free_pointer_array(POINTER_ARRAY *pa)
{
  if (pa->typelib.count)
  {
    pa->typelib.count=0;
    my_free(pa->typelib.type_names);
    pa->typelib.type_names=0;
    my_free(pa->str);
  }
  return;
} /* free_pointer_array */


	/* Code for replace rutines */

#define SET_MALLOC_HUNC 64

typedef struct st_rep_set {
  uint  *bits;				/* Pointer to used sets */
  short	next[LAST_CHAR_CODE];		/* Pointer to next sets */
  uint	found_len;			/* Best match to date */
  int	found_offset;
  uint  table_offset;
  uint  size_of_bits;			/* For convinience */
} REP_SET;

typedef struct st_rep_sets {
  uint		count;			/* Number of sets */
  uint		extra;			/* Extra sets in buffer */
  uint		invisible;		/* Sets not chown */
  uint		size_of_bits;
  REP_SET	*set,*set_buffer;
  uint		*bit_buffer;
} REP_SETS;

typedef struct st_found_set {
  uint table_offset;
  int found_offset;
} FOUND_SET;

typedef struct st_follow {
  int chr;
  uint table_offset;
  uint len;
} FOLLOWS;


static int init_sets(REP_SETS *sets,uint states);
static REP_SET *make_new_set(REP_SETS *sets);
static void make_sets_invisible(REP_SETS *sets);
static void free_last_set(REP_SETS *sets);
static void free_sets(REP_SETS *sets);
static void internal_set_bit(REP_SET *set, uint bit);
static void internal_clear_bit(REP_SET *set, uint bit);
static void or_bits(REP_SET *to,REP_SET *from);
static void copy_bits(REP_SET *to,REP_SET *from);
static int cmp_bits(REP_SET *set1,REP_SET *set2);
static int get_next_bit(REP_SET *set,uint lastpos);
static short find_set(REP_SETS *sets,REP_SET *find);
static short find_found(FOUND_SET *found_set,uint table_offset,
                        int found_offset);
static uint start_at_word(char * pos);
static uint end_of_word(char * pos);
static uint replace_len(char * pos);

static uint found_sets=0;


	/* Init a replace structure for further calls */

static REPLACE *init_replace(char * *from, char * *to,uint count,
                             char * word_end_chars)
{
  uint i,j,states,set_nr,len,result_len,max_length,found_end,bits_set,bit_nr;
  int used_sets,chr;
  short default_state;
  char used_chars[LAST_CHAR_CODE],is_word_end[256];
  char * pos, *to_pos, **to_array;
  REP_SETS sets;
  REP_SET *set,*start_states,*word_states,*new_set;
  FOLLOWS *follow,*follow_ptr;
  REPLACE *replace;
  FOUND_SET *found_set;
  REPLACE_STRING *rep_str;
  DBUG_ENTER("init_replace");

  /* Count number of states */
  for (i=result_len=max_length=0 , states=2 ; i < count ; i++)
  {
    len=replace_len(from[i]);
    if (!len)
    {
      errno=EINVAL;
      my_message(0,"No to-string for last from-string",MYF(ME_BELL));
      DBUG_RETURN(0);
    }
    states+=len+1;
    result_len+=(uint) strlen(to[i])+1;
    if (len > max_length)
      max_length=len;
  }
  memset(is_word_end, 0, sizeof(is_word_end));
  for (i=0 ; word_end_chars[i] ; i++)
    is_word_end[(uchar) word_end_chars[i]]=1;

  if (init_sets(&sets,states))
    DBUG_RETURN(0);
  found_sets=0;
  if (!(found_set= (FOUND_SET*) my_malloc(sizeof(FOUND_SET)*max_length*count,
					  MYF(MY_WME))))
  {
    free_sets(&sets);
    DBUG_RETURN(0);
  }
  (void) make_new_set(&sets);			/* Set starting set */
  make_sets_invisible(&sets);			/* Hide previus sets */
  used_sets=-1;
  word_states=make_new_set(&sets);		/* Start of new word */
  start_states=make_new_set(&sets);		/* This is first state */
  if (!(follow=(FOLLOWS*) my_malloc((states+2)*sizeof(FOLLOWS),MYF(MY_WME))))
  {
    free_sets(&sets);
    my_free(found_set);
    DBUG_RETURN(0);
  }

	/* Init follow_ptr[] */
  for (i=0, states=1, follow_ptr=follow+1 ; i < count ; i++)
  {
    if (from[i][0] == '\\' && from[i][1] == '^')
    {
      internal_set_bit(start_states,states+1);
      if (!from[i][2])
      {
	start_states->table_offset=i;
	start_states->found_offset=1;
      }
    }
    else if (from[i][0] == '\\' && from[i][1] == '$')
    {
      internal_set_bit(start_states,states);
      internal_set_bit(word_states,states);
      if (!from[i][2] && start_states->table_offset == (uint) ~0)
      {
	start_states->table_offset=i;
	start_states->found_offset=0;
      }
    }
    else
    {
      internal_set_bit(word_states,states);
      if (from[i][0] == '\\' && (from[i][1] == 'b' && from[i][2]))
	internal_set_bit(start_states,states+1);
      else
	internal_set_bit(start_states,states);
    }
    for (pos=from[i], len=0; *pos ; pos++)
    {
      if (*pos == '\\' && *(pos+1))
      {
	pos++;
	switch (*pos) {
	case 'b':
	  follow_ptr->chr = SPACE_CHAR;
	  break;
	case '^':
	  follow_ptr->chr = START_OF_LINE;
	  break;
	case '$':
	  follow_ptr->chr = END_OF_LINE;
	  break;
	case 'r':
	  follow_ptr->chr = '\r';
	  break;
	case 't':
	  follow_ptr->chr = '\t';
	  break;
	case 'v':
	  follow_ptr->chr = '\v';
	  break;
	default:
	  follow_ptr->chr = (uchar) *pos;
	  break;
	}
      }
      else
	follow_ptr->chr= (uchar) *pos;
      follow_ptr->table_offset=i;
      follow_ptr->len= ++len;
      follow_ptr++;
    }
    follow_ptr->chr=0;
    follow_ptr->table_offset=i;
    follow_ptr->len=len;
    follow_ptr++;
    states+=(uint) len+1;
  }


  for (set_nr=0,pos=0 ; set_nr < sets.count ; set_nr++)
  {
    set=sets.set+set_nr;
    default_state= 0;				/* Start from beginning */

    /* If end of found-string not found or start-set with current set */

    for (i= (uint) ~0; (i=get_next_bit(set,i)) ;)
    {
      if (!follow[i].chr)
      {
	if (! default_state)
	  default_state= find_found(found_set,set->table_offset,
				    set->found_offset+1);
      }
    }
    copy_bits(sets.set+used_sets,set);		/* Save set for changes */
    if (!default_state)
      or_bits(sets.set+used_sets,sets.set);	/* Can restart from start */

    /* Find all chars that follows current sets */
    memset(used_chars, 0, sizeof(used_chars));
    for (i= (uint) ~0; (i=get_next_bit(sets.set+used_sets,i)) ;)
    {
      used_chars[follow[i].chr]=1;
      if ((follow[i].chr == SPACE_CHAR && !follow[i+1].chr &&
	   follow[i].len > 1) || follow[i].chr == END_OF_LINE)
	used_chars[0]=1;
    }

    /* Mark word_chars used if \b is in state */
    if (used_chars[SPACE_CHAR])
      for (pos= word_end_chars ; *pos ; pos++)
	used_chars[(int) (uchar) *pos] = 1;

    /* Handle other used characters */
    for (chr= 0 ; chr < 256 ; chr++)
    {
      if (! used_chars[chr])
	set->next[chr]= (short) (chr ? default_state : -1);
      else
      {
	new_set=make_new_set(&sets);
	set=sets.set+set_nr;			/* if realloc */
	new_set->table_offset=set->table_offset;
	new_set->found_len=set->found_len;
	new_set->found_offset=set->found_offset+1;
	found_end=0;

	for (i= (uint) ~0 ; (i=get_next_bit(sets.set+used_sets,i)) ; )
	{
	  if (!follow[i].chr || follow[i].chr == chr ||
	      (follow[i].chr == SPACE_CHAR &&
	       (is_word_end[chr] ||
		(!chr && follow[i].len > 1 && ! follow[i+1].chr))) ||
	      (follow[i].chr == END_OF_LINE && ! chr))
	  {
	    if ((! chr || (follow[i].chr && !follow[i+1].chr)) &&
		follow[i].len > found_end)
	      found_end=follow[i].len;
	    if (chr && follow[i].chr)
	      internal_set_bit(new_set,i+1);		/* To next set */
	    else
	      internal_set_bit(new_set,i);
	  }
	}
	if (found_end)
	{
	  new_set->found_len=0;			/* Set for testing if first */
	  bits_set=0;
	  for (i= (uint) ~0; (i=get_next_bit(new_set,i)) ;)
	  {
	    if ((follow[i].chr == SPACE_CHAR ||
		 follow[i].chr == END_OF_LINE) && ! chr)
	      bit_nr=i+1;
	    else
	      bit_nr=i;
	    if (follow[bit_nr-1].len < found_end ||
		(new_set->found_len &&
		 (chr == 0 || !follow[bit_nr].chr)))
	      internal_clear_bit(new_set,i);
	    else
	    {
	      if (chr == 0 || !follow[bit_nr].chr)
	      {					/* best match  */
		new_set->table_offset=follow[bit_nr].table_offset;
		if (chr || (follow[i].chr == SPACE_CHAR ||
			    follow[i].chr == END_OF_LINE))
		  new_set->found_offset=found_end;	/* New match */
		new_set->found_len=found_end;
	      }
	      bits_set++;
	    }
	  }
	  if (bits_set == 1)
	  {
	    set->next[chr] = find_found(found_set,
					new_set->table_offset,
					new_set->found_offset);
	    free_last_set(&sets);
	  }
	  else
	    set->next[chr] = find_set(&sets,new_set);
	}
	else
	  set->next[chr] = find_set(&sets,new_set);
      }
    }
  }

	/* Alloc replace structure for the replace-state-machine */

  if ((replace=(REPLACE*) my_malloc(sizeof(REPLACE)*(sets.count)+
				    sizeof(REPLACE_STRING)*(found_sets+1)+
				    sizeof(char *)*count+result_len,
				    MYF(MY_WME | MY_ZEROFILL))))
  {
    rep_str=(REPLACE_STRING*) (replace+sets.count);
    to_array=(char **) (rep_str+found_sets+1);
    to_pos=(char *) (to_array+count);
    for (i=0 ; i < count ; i++)
    {
      to_array[i]=to_pos;
      to_pos=strmov(to_pos,to[i])+1;
    }
    rep_str[0].found=1;
    rep_str[0].replace_string=0;
    for (i=1 ; i <= found_sets ; i++)
    {
      pos=from[found_set[i-1].table_offset];
      rep_str[i].found= (my_bool) (!memcmp(pos,"\\^",3) ? 2 : 1);
      rep_str[i].replace_string=to_array[found_set[i-1].table_offset];
      rep_str[i].to_offset=found_set[i-1].found_offset-start_at_word(pos);
      rep_str[i].from_offset=found_set[i-1].found_offset-replace_len(pos)+
	end_of_word(pos);
    }
    for (i=0 ; i < sets.count ; i++)
    {
      for (j=0 ; j < 256 ; j++)
	if (sets.set[i].next[j] >= 0)
	  replace[i].next[j]=replace+sets.set[i].next[j];
	else
	  replace[i].next[j]=(REPLACE*) (rep_str+(-sets.set[i].next[j]-1));
    }
  }
  my_free(follow);
  free_sets(&sets);
  my_free(found_set);
  DBUG_PRINT("exit",("Replace table has %d states",sets.count));
  DBUG_RETURN(replace);
}


static int init_sets(REP_SETS *sets,uint states)
{
  memset(sets, 0, sizeof(*sets));
  sets->size_of_bits=((states+7)/8);
  if (!(sets->set_buffer=(REP_SET*) my_malloc(sizeof(REP_SET)*SET_MALLOC_HUNC,
					      MYF(MY_WME))))
    return 1;
  if (!(sets->bit_buffer=(uint*) my_malloc(sizeof(uint)*sets->size_of_bits*
					   SET_MALLOC_HUNC,MYF(MY_WME))))
  {
    my_free(sets->set);
    return 1;
  }
  return 0;
}

	/* Make help sets invisible for nicer codeing */

static void make_sets_invisible(REP_SETS *sets)
{
  sets->invisible=sets->count;
  sets->set+=sets->count;
  sets->count=0;
}

static REP_SET *make_new_set(REP_SETS *sets)
{
  uint i,count,*bit_buffer;
  REP_SET *set;
  if (sets->extra)
  {
    sets->extra--;
    set=sets->set+ sets->count++;
    memset(set->bits, 0, sizeof(uint)*sets->size_of_bits);
    memset(&set->next[0], 0, sizeof(set->next[0])*LAST_CHAR_CODE);
    set->found_offset=0;
    set->found_len=0;
    set->table_offset= (uint) ~0;
    set->size_of_bits=sets->size_of_bits;
    return set;
  }
  count=sets->count+sets->invisible+SET_MALLOC_HUNC;
  if (!(set=(REP_SET*) my_realloc((uchar*) sets->set_buffer,
				   sizeof(REP_SET)*count,
				  MYF(MY_WME))))
    return 0;
  sets->set_buffer=set;
  sets->set=set+sets->invisible;
  if (!(bit_buffer=(uint*) my_realloc((uchar*) sets->bit_buffer,
				      (sizeof(uint)*sets->size_of_bits)*count,
				      MYF(MY_WME))))
    return 0;
  sets->bit_buffer=bit_buffer;
  for (i=0 ; i < count ; i++)
  {
    sets->set_buffer[i].bits=bit_buffer;
    bit_buffer+=sets->size_of_bits;
  }
  sets->extra=SET_MALLOC_HUNC;
  return make_new_set(sets);
}

static void free_last_set(REP_SETS *sets)
{
  sets->count--;
  sets->extra++;
  return;
}

static void free_sets(REP_SETS *sets)
{
  my_free(sets->set_buffer);
  my_free(sets->bit_buffer);
  return;
}

static void internal_set_bit(REP_SET *set, uint bit)
{
  set->bits[bit / WORD_BIT] |= 1 << (bit % WORD_BIT);
  return;
}

static void internal_clear_bit(REP_SET *set, uint bit)
{
  set->bits[bit / WORD_BIT] &= ~ (1 << (bit % WORD_BIT));
  return;
}


static void or_bits(REP_SET *to,REP_SET *from)
{
  uint i;
  for (i=0 ; i < to->size_of_bits ; i++)
    to->bits[i]|=from->bits[i];
  return;
}

static void copy_bits(REP_SET *to,REP_SET *from)
{
  memcpy((uchar*) to->bits,(uchar*) from->bits,
	 (size_t) (sizeof(uint) * to->size_of_bits));
}

static int cmp_bits(REP_SET *set1,REP_SET *set2)
{
  return memcmp(set1->bits, set2->bits,
                sizeof(uint) * set1->size_of_bits);
}


	/* Get next set bit from set. */

static int get_next_bit(REP_SET *set,uint lastpos)
{
  uint pos,*start,*end,bits;

  start=set->bits+ ((lastpos+1) / WORD_BIT);
  end=set->bits + set->size_of_bits;
  bits=start[0] & ~((1 << ((lastpos+1) % WORD_BIT)) -1);

  while (! bits && ++start < end)
    bits=start[0];
  if (!bits)
    return 0;
  pos=(uint) (start-set->bits)*WORD_BIT;
  while (! (bits & 1))
  {
    bits>>=1;
    pos++;
  }
  return pos;
}

	/* find if there is a same set in sets. If there is, use it and
	   free given set, else put in given set in sets and return it's
	   position */

static short find_set(REP_SETS *sets,REP_SET *find)
{
  uint i;
  for (i=0 ; i < sets->count-1 ; i++)
  {
    if (!cmp_bits(sets->set+i,find))
    {
      free_last_set(sets);
      return (short) i;
    }
  }
  return (short) i;			/* return new position */
}


/*
  find if there is a found_set with same table_offset & found_offset
  If there is return offset to it, else add new offset and return pos.
  Pos returned is -offset-2 in found_set_structure because it's is
  saved in set->next and set->next[] >= 0 points to next set and
  set->next[] == -1 is reserved for end without replaces.
*/

static short find_found(FOUND_SET *found_set,uint table_offset,
                        int found_offset)
{
  int i;
  for (i=0 ; (uint) i < found_sets ; i++)
    if (found_set[i].table_offset == table_offset &&
	found_set[i].found_offset == found_offset)
      return (short) (-i-2);
  found_set[i].table_offset=table_offset;
  found_set[i].found_offset=found_offset;
  found_sets++;
  return (short) (-i-2);			/* return new position */
}

	/* Return 1 if regexp starts with \b or ends with \b*/

static uint start_at_word(char * pos)
{
  return (((!memcmp(pos,"\\b",2) && pos[2]) || !memcmp(pos,"\\^",2)) ? 1 : 0);
}

static uint end_of_word(char * pos)
{
  char * end=strend(pos);
  return ((end > pos+2 && !memcmp(end-2,"\\b",2)) ||
	  (end >= pos+2 && !memcmp(end-2,"\\$",2))) ?
	    1 : 0;
}


static uint replace_len(char * str)
{
  uint len=0;
  while (*str)
  {
    if (str[0] == '\\' && str[1])
      str++;
    str++;
    len++;
  }
  return len;
}


	/* The actual loop */

static uint replace_strings(REPLACE *rep, char **start, uint *max_length,
                            char *from)
{
  REPLACE *rep_pos;
  REPLACE_STRING *rep_str;
  char *to, *end, *pos, *new;

  end=(to= *start) + *max_length-1;
  rep_pos=rep+1;
  for(;;)
  {
    while (!rep_pos->found)
    {
      rep_pos= rep_pos->next[(uchar) *from];
      if (to == end)
      {
	(*max_length)+=8192;
	if (!(new=my_realloc(*start,*max_length,MYF(MY_WME))))
	  return (uint) -1;
	to=new+(to - *start);
	end=(*start=new)+ *max_length-1;
      }
      *to++= *from++;
    }
    if (!(rep_str = ((REPLACE_STRING*) rep_pos))->replace_string)
      return (uint) (to - *start)-1;
    updated=1;			/* Some char * is replaced */
    to-=rep_str->to_offset;
    for (pos=rep_str->replace_string; *pos ; pos++)
    {
      if (to == end)
      {
	(*max_length)*=2;
	if (!(new=my_realloc(*start,*max_length,MYF(MY_WME))))
	  return (uint) -1;
	to=new+(to - *start);
	end=(*start=new)+ *max_length-1;
      }
      *to++= *pos;
    }
    if (!*(from-=rep_str->from_offset) && rep_pos->found != 2)
      return (uint) (to - *start);
    rep_pos=rep;
  }
}

static char *buffer;		/* The buffer itself, grown as needed. */
static int bufbytes;		/* Number of bytes in the buffer. */
static int bufread,my_eof;		/* Number of bytes to get with each read(). */
static uint bufalloc;
static char *out_buff;
static uint out_length;

static int initialize_buffer()
{
  bufread = 8192;
  bufalloc = bufread + bufread / 2;
  if (!(buffer = my_malloc(bufalloc+1,MYF(MY_WME))))
    return 1;
  bufbytes=my_eof=0;
  out_length=bufread;
  if (!(out_buff=my_malloc(out_length,MYF(MY_WME))))
    return(1);
  return 0;
}

static void reset_buffer()
{
  bufbytes=my_eof=0;
}

static void free_buffer()
{
  my_free(buffer);
  my_free(out_buff);
}


/*
  Fill the buffer retaining the last n bytes at the beginning of the
  newly filled buffer (for backward context).  Returns the number of new
  bytes read from disk.
*/

static int fill_buffer_retaining(fd,n)
File fd;
int n;
{
  int i;

  /* See if we need to grow the buffer. */
  if ((int) bufalloc - n <= bufread)
  {
    while ((int) bufalloc - n <= bufread)
    {
      bufalloc *= 2;
      bufread *= 2;
    }
    buffer = my_realloc(buffer, bufalloc+1, MYF(MY_WME));
    if (! buffer)
      return(-1);
  }

  /* Shift stuff down. */
  bmove(buffer,buffer+bufbytes-n,(uint) n);
  bufbytes = n;

  if (my_eof)
    return 0;

  /* Read in new stuff. */
  if ((i=(int) my_read(fd, (uchar*) buffer + bufbytes,
                       (size_t) bufread, MYF(MY_WME))) < 0)
    return -1;

  /* Kludge to pretend every nonempty file ends with a newline. */
  if (i == 0 && bufbytes > 0 && buffer[bufbytes - 1] != '\n')
  {
    my_eof = i = 1;
    buffer[bufbytes] = '\n';
  }

  bufbytes += i;
  return i;
}

	/* Return 0 if convert is ok */
	/* Global variable update is set if something was changed */

static int convert_pipe(rep,in,out)
REPLACE *rep;
FILE *in,*out;
{
  int retain,error;
  uint length;
  char save_char,*end_of_line,*start_of_line;
  DBUG_ENTER("convert_pipe");

  updated=retain=0;
  reset_buffer();

  while ((error=fill_buffer_retaining(fileno(in),retain)) > 0)
  {
    end_of_line=buffer ;
    buffer[bufbytes]=0;			/* Sentinel  */
    for (;;)
    {
      start_of_line=end_of_line;
      while (end_of_line[0] != '\n' && end_of_line[0])
	end_of_line++;
      if (end_of_line == buffer+bufbytes)
      {
	retain= (int) (end_of_line - start_of_line);
	break;				/* No end of line, read more */
      }
      save_char=end_of_line[0];
      end_of_line[0]=0;
      end_of_line++;
      if ((length=replace_strings(rep,&out_buff,&out_length,start_of_line)) ==
	  (uint) -1)
	return 1;
      if (!my_eof)
	out_buff[length++]=save_char;	/* Don't write added newline */
      if (my_fwrite(out, (uchar*) out_buff, length, MYF(MY_WME | MY_NABP)))
	DBUG_RETURN(1);
    }
  }
  DBUG_RETURN(error);
}


static int convert_file(REPLACE *rep, char * name)
{
  int error;
  FILE *in,*out;
  char dir_buff[FN_REFLEN], tempname[FN_REFLEN], *org_name = name;
#ifdef HAVE_READLINK
  char link_name[FN_REFLEN];
#endif
  File temp_file;
  size_t dir_buff_length;
  DBUG_ENTER("convert_file");

  /* check if name is a symlink */
#ifdef HAVE_READLINK  
  org_name= (my_enable_symlinks &&
             !my_readlink(link_name, name, MYF(0))) ? link_name : name;
#endif
  if (!(in= my_fopen(org_name,O_RDONLY,MYF(MY_WME))))
    DBUG_RETURN(1);
  dirname_part(dir_buff, org_name, &dir_buff_length);
  if ((temp_file= create_temp_file(tempname, dir_buff, "PR", O_WRONLY,
                                   MYF(MY_WME))) < 0)
  {
    my_fclose(in,MYF(0));
    DBUG_RETURN(1);
  }    
  if (!(out= my_fdopen(temp_file, tempname, O_WRONLY, MYF(MY_WME))))
  {
    my_fclose(in,MYF(0));
    DBUG_RETURN(1);
  }

  error=convert_pipe(rep,in,out);
  my_fclose(in,MYF(0)); my_fclose(out,MYF(0));

  if (updated && ! error)
    my_redel(org_name,tempname,MYF(MY_WME | MY_LINK_WARNING));
  else
    my_delete(tempname,MYF(MY_WME));
  if (!silent && ! error)
  {
    if (updated)
      printf("%s converted\n",name);
    else if (verbose)
      printf("%s left unchanged\n",name);
  }
  DBUG_RETURN(error);
}
