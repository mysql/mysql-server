/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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


/* A lexical scanner on a temporary buffer with a yacc interface */

#include "mysql_priv.h"
#include "item_create.h"
#include <m_ctype.h>
#include <hash.h>

LEX_STRING tmp_table_alias= {(char*) "tmp-table",8};

/* Macros to look like lex */

#define yyGet()		*(lex->ptr++)
#define yyGetLast()	lex->ptr[-1]
#define yyPeek()	lex->ptr[0]
#define yyPeek2()	lex->ptr[1]
#define yyUnget()	lex->ptr--
#define yySkip()	lex->ptr++
#define yyLength()	((uint) (lex->ptr - lex->tok_start)-1)

#if MYSQL_VERSION_ID < 32300
#define FLOAT_NUM	REAL_NUM
#endif

pthread_key(LEX*,THR_LEX);

#define TOCK_NAME_LENGTH 24

/*
  The following is based on the latin1 character set, and is only
  used when comparing keywords
*/

uchar to_upper_lex[] = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
   16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
   32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
   48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
   64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
   80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
   96, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
   80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90,123,124,125,126,127,
  128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
  144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
  160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
  176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
  192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
  208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
  192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
  208,209,210,211,212,213,214,247,216,217,218,219,220,221,222,255
};

inline int lex_casecmp(const char *s, const char *t, uint len)
{
  while (len-- != 0 &&
	 to_upper_lex[(uchar) *s++] == to_upper_lex[(uchar) *t++]) ;
  return (int) len+1;
}

#include "lex_hash.h"

static uchar state_map[256];


void lex_init(void)
{
  uint i;
  DBUG_ENTER("lex_init");
  for (i=0 ; i < array_elements(symbols) ; i++)
    symbols[i].length=(uchar) strlen(symbols[i].name);
  for (i=0 ; i < array_elements(sql_functions) ; i++)
    sql_functions[i].length=(uchar) strlen(sql_functions[i].name);

  VOID(pthread_key_create(&THR_LEX,NULL));

  /* Fill state_map with states to get a faster parser */
  for (i=0; i < 256 ; i++)
  {
    if (my_isalpha(system_charset_info,i))
      state_map[i]=(uchar) STATE_IDENT;
    else if (my_isdigit(system_charset_info,i))
      state_map[i]=(uchar) STATE_NUMBER_IDENT;
#if defined(USE_MB) && defined(USE_MB_IDENT)
    else if (use_mb(system_charset_info) && my_ismbhead(system_charset_info, i))
      state_map[i]=(uchar) STATE_IDENT;
#endif
    else if (!my_isgraph(system_charset_info,i))
      state_map[i]=(uchar) STATE_SKIP;      
    else
      state_map[i]=(uchar) STATE_CHAR;
  }
  state_map[(uchar)'_']=state_map[(uchar)'$']=(uchar) STATE_IDENT;
  state_map[(uchar)'\'']=state_map[(uchar)'"']=(uchar) STATE_STRING;
  state_map[(uchar)'-']=state_map[(uchar)'+']=(uchar) STATE_SIGNED_NUMBER;
  state_map[(uchar)'.']=(uchar) STATE_REAL_OR_POINT;
  state_map[(uchar)'>']=state_map[(uchar)'=']=state_map[(uchar)'!']= (uchar) STATE_CMP_OP;
  state_map[(uchar)'<']= (uchar) STATE_LONG_CMP_OP;
  state_map[(uchar)'&']=state_map[(uchar)'|']=(uchar) STATE_BOOL;
  state_map[(uchar)'#']=(uchar) STATE_COMMENT;
  state_map[(uchar)';']=(uchar) STATE_COLON;
  state_map[(uchar)':']=(uchar) STATE_SET_VAR;
  state_map[0]=(uchar) STATE_EOL;
  state_map[(uchar)'\\']= (uchar) STATE_ESCAPE;
  state_map[(uchar)'/']= (uchar) STATE_LONG_COMMENT;
  state_map[(uchar)'*']= (uchar) STATE_END_LONG_COMMENT;
  state_map[(uchar)'@']= (uchar) STATE_USER_END;
  state_map[(uchar) '`']= (uchar) STATE_USER_VARIABLE_DELIMITER;
  if (opt_sql_mode & MODE_ANSI_QUOTES)
  {
    state_map[(uchar) '"'] = STATE_USER_VARIABLE_DELIMITER;
  }
  DBUG_VOID_RETURN;
}


void lex_free(void)
{					// Call this when daemon ends
  DBUG_ENTER("lex_free");
  DBUG_VOID_RETURN;
}


LEX *lex_start(THD *thd, uchar *buf,uint length)
{
  LEX *lex= &thd->lex;
  lex->next_state=STATE_START;
  lex->end_of_query=(lex->ptr=buf)+length;
  lex->yylineno = 1;
  lex->select->create_refs=lex->in_comment=0;
  lex->length=0;
  lex->select->in_sum_expr=0;
  lex->select->expr_list.empty();
  lex->select->ftfunc_list.empty();
  lex->convert_set=(lex->thd=thd)->convert_set;
  lex->yacc_yyss=lex->yacc_yyvs=0;
  lex->ignore_space=test(thd->sql_mode & MODE_IGNORE_SPACE);
  lex->slave_thd_opt=0;
  bzero(&lex->mi,sizeof(lex->mi));
  return lex;
}

void lex_end(LEX *lex)
{
  lex->select->expr_list.delete_elements();	// If error when parsing sql-varargs
  x_free(lex->yacc_yyss);
  x_free(lex->yacc_yyvs);
}


static int find_keyword(LEX *lex, uint len, bool function)
{
  uchar *tok=lex->tok_start;

  SYMBOL *symbol = get_hash_symbol((const char *)tok,len,function);
  if (symbol)
  {
    lex->yylval->symbol.symbol=symbol;
    lex->yylval->symbol.str= (char*) tok;
    lex->yylval->symbol.length=len;
    return symbol->tok;
  }
#ifdef HAVE_DLOPEN
  udf_func *udf;
  if (function && using_udf_functions && (udf=find_udf((char*) tok, len)))
  {
    switch (udf->returns) {
    case STRING_RESULT:
      lex->yylval->udf=udf;
      return (udf->type == UDFTYPE_FUNCTION) ? UDF_CHAR_FUNC : UDA_CHAR_SUM;
    case REAL_RESULT:
      lex->yylval->udf=udf;
      return (udf->type == UDFTYPE_FUNCTION) ? UDF_FLOAT_FUNC : UDA_FLOAT_SUM;
    case INT_RESULT:
      lex->yylval->udf=udf;
      return (udf->type == UDFTYPE_FUNCTION) ? UDF_INT_FUNC : UDA_INT_SUM;
    }
  }
#endif
  return 0;
}


/* make a copy of token before ptr and set yytoklen */

LEX_STRING get_token(LEX *lex,uint length)
{
  LEX_STRING tmp;
  yyUnget();			// ptr points now after last token char
  tmp.length=lex->yytoklen=length;
  tmp.str=(char*) lex->thd->strmake((char*) lex->tok_start,tmp.length);
  return tmp;
}

/* Return an unescaped text literal without quotes */
/* Fix sometimes to do only one scan of the string */

static char *get_text(LEX *lex)
{
  reg1 uchar c,sep;
  uint found_escape=0;

  sep= yyGetLast();			// String should end with this
  //lex->tok_start=lex->ptr-1;		// Remember '
  while (lex->ptr != lex->end_of_query)
  {
    c = yyGet();
#ifdef USE_MB
    int l;
    if (use_mb(system_charset_info) &&
        (l = my_ismbchar(system_charset_info,
                         (const char *)lex->ptr-1,
                         (const char *)lex->end_of_query))) {
	lex->ptr += l-1;
	continue;
    }
#endif
    if (c == '\\')
    {					// Escaped character
      found_escape=1;
      if (lex->ptr == lex->end_of_query)
	return 0;
      yySkip();
    }
    else if (c == sep)
    {
      if (c == yyGet())			// Check if two separators in a row
      {
	found_escape=1;			// dupplicate. Remember for delete
	continue;
      }
      else
	yyUnget();

      /* Found end. Unescape and return string */
      uchar *str,*end,*start;

      str=lex->tok_start+1;
      end=lex->ptr-1;
      if (!(start=(uchar*) lex->thd->alloc((uint) (end-str)+1)))
	return (char*) "";		// Sql_alloc has set error flag
      if (!found_escape)
      {
	lex->yytoklen=(uint) (end-str);
	memcpy(start,str,lex->yytoklen);
	start[lex->yytoklen]=0;
      }
      else
      {
	uchar *to;
	for (to=start ; str != end ; str++)
	{
#ifdef USE_MB
	  int l;
	  if (use_mb(system_charset_info) &&
              (l = my_ismbchar(system_charset_info,
                               (const char *)str, (const char *)end))) {
	      while (l--)
		  *to++ = *str++;
	      str--;
	      continue;
	  }
#endif
	  if (*str == '\\' && str+1 != end)
	  {
	    switch(*++str) {
	    case 'n':
	      *to++='\n';
	      break;
	    case 't':
	      *to++= '\t';
	      break;
	    case 'r':
	      *to++ = '\r';
	      break;
	    case 'b':
	      *to++ = '\b';
	      break;
	    case '0':
	      *to++= 0;			// Ascii null
	      break;
	    case 'Z':			// ^Z must be escaped on Win32
	      *to++='\032';
	      break;
	    case '_':
	    case '%':
	      *to++= '\\';		// remember prefix for wildcard
	      /* Fall through */
	    default:
	      *to++ = *str;
	      break;
	    }
	  }
	  else if (*str == sep)
	    *to++= *str++;		// Two ' or "
	  else
	    *to++ = *str;

	}
	*to=0;
	lex->yytoklen=(uint) (to-start);
      }
      if (lex->convert_set)
	lex->convert_set->convert((char*) start,lex->yytoklen);
      return (char*) start;
    }
  }
  return 0;					// unexpected end of query
}


/*
** Calc type of integer; long integer, longlong integer or real.
** Returns smallest type that match the string.
** When using unsigned long long values the result is converted to a real
** because else they will be unexpected sign changes because all calculation
** is done with longlong or double.
*/

static const char *long_str="2147483647";
static const uint long_len=10;
static const char *signed_long_str="-2147483648";
static const char *longlong_str="9223372036854775807";
static const uint longlong_len=19;
static const char *signed_longlong_str="-9223372036854775808";
static const uint signed_longlong_len=19;
static const char *unsigned_longlong_str="18446744073709551615";
static const uint unsigned_longlong_len=20;

inline static uint int_token(const char *str,uint length)
{
  if (length < long_len)			// quick normal case
    return NUM;
  bool neg=0;

  if (*str == '+')				// Remove sign and pre-zeros
  {
    str++; length--;
  }
  else if (*str == '-')
  {
    str++; length--;
    neg=1;
  }
  while (*str == '0' && length)
  {
    str++; length --;
  }
  if (length < long_len)
    return NUM;

  uint smaller,bigger;
  const char *cmp;
  if (neg)
  {
    if (length == long_len)
    {
      cmp= signed_long_str+1;
      smaller=NUM;				// If <= signed_long_str
      bigger=LONG_NUM;				// If >= signed_long_str
    }
    else if (length < signed_longlong_len)
      return LONG_NUM;
    else if (length > signed_longlong_len)
      return REAL_NUM;
    else
    {
      cmp=signed_longlong_str+1;
      smaller=LONG_NUM;				// If <= signed_longlong_str
      bigger=REAL_NUM;
    }
  }
  else
  {
    if (length == long_len)
    {
      cmp= long_str;
      smaller=NUM;
      bigger=LONG_NUM;
    }
    else if (length < longlong_len)
      return LONG_NUM;
    else if (length > longlong_len)
    {
      if (length > unsigned_longlong_len)
	return REAL_NUM;
      cmp=unsigned_longlong_str;
      smaller=ULONGLONG_NUM;
      bigger=REAL_NUM;
    }
    else
    {
      cmp=longlong_str;
      smaller=LONG_NUM;
      bigger=REAL_NUM;
    }
  }
  while (*cmp && *cmp++ == *str++) ;
  return ((uchar) str[-1] <= (uchar) cmp[-1]) ? smaller : bigger;
}


// yylex remember the following states from the following yylex()
// STATE_EOQ ; found end of query
// STATE_OPERATOR_OR_IDENT ; last state was an ident, text or number
// 			     (which can't be followed by a signed number)

int yylex(void *arg)
{
  reg1	uchar c;
  int	tokval;
  uint length;
  enum lex_states state,prev_state;
  LEX	*lex=current_lex;
  YYSTYPE *yylval=(YYSTYPE*) arg;

  lex->yylval=yylval;			// The global state
  lex->tok_start=lex->tok_end=lex->ptr;
  prev_state=state=lex->next_state;
  lex->next_state=STATE_OPERATOR_OR_IDENT;
  LINT_INIT(c);
  for (;;)
  {
    switch(state) {
    case STATE_OPERATOR_OR_IDENT:	// Next is operator or keyword
    case STATE_START:			// Start of token
      // Skip startspace
      for (c=yyGet() ; (state_map[c] == STATE_SKIP) ; c= yyGet())
      {
	if (c == '\n')
	  lex->yylineno++;
      }
      lex->tok_start=lex->ptr-1;	// Start of real token
      state= (enum lex_states) state_map[c];
      break;
    case STATE_ESCAPE:
      if (yyGet() == 'N')
      {					// Allow \N as shortcut for NULL
	yylval->lex_str.str=(char*) "\\N";
	yylval->lex_str.length=2;
	return NULL_SYM;
      }
    case STATE_CHAR:			// Unknown or single char token
    case STATE_SKIP:			// This should not happen
      yylval->lex_str.str=(char*) (lex->ptr=lex->tok_start);// Set to first char
      yylval->lex_str.length=1;
      c=yyGet();
      if (c != ')')
	lex->next_state= STATE_START;	// Allow signed numbers
      if (c == ',')
	lex->tok_start=lex->ptr;	// Let tok_start point at next item
      return((int) c);

    case STATE_IDENT:			// Incomplete keyword or ident
      if ((c == 'x' || c == 'X') && yyPeek() == '\'')
      {					// Found x'hex-number'
	state=STATE_HEX_NUMBER;
	break;
      }
#if defined(USE_MB) && defined(USE_MB_IDENT)
      if (use_mb(system_charset_info))
      {
        if (my_ismbhead(system_charset_info, yyGetLast()))
        {
          int l = my_ismbchar(system_charset_info,
                              (const char *)lex->ptr-1,
                              (const char *)lex->end_of_query);
          if (l == 0) {
            state = STATE_CHAR;
            continue;
          }
          lex->ptr += l - 1;
        }
        while (state_map[c=yyGet()] == STATE_IDENT ||
               state_map[c] == STATE_NUMBER_IDENT)
        {
          if (my_ismbhead(system_charset_info, c))
          {
            int l;
            if ((l = my_ismbchar(system_charset_info,
                              (const char *)lex->ptr-1,
                              (const char *)lex->end_of_query)) == 0)
              break;
            lex->ptr += l-1;
          }
        }
      }
      else
#endif
        while (state_map[c=yyGet()] == STATE_IDENT ||
               state_map[c] == STATE_NUMBER_IDENT) ;
      length= (uint) (lex->ptr - lex->tok_start)-1;
      if (lex->ignore_space)
      {
	for ( ; state_map[c] == STATE_SKIP ; c= yyGet());
      }
      if (c == '.' && (state_map[yyPeek()] == STATE_IDENT ||
		       state_map[yyPeek()] == STATE_NUMBER_IDENT))
	lex->next_state=STATE_IDENT_SEP;
      else
      {					// '(' must follow directly if function
	yyUnget();
	if ((tokval = find_keyword(lex,length,c == '(')))
	{
	  lex->next_state= STATE_START;	// Allow signed numbers
	  return(tokval);		// Was keyword
	}
	yySkip();			// next state does a unget
      }
      yylval->lex_str=get_token(lex,length);
      if (lex->convert_set)
	    lex->convert_set->convert((char*) yylval->lex_str.str,lex->yytoklen);
      return(IDENT);

    case STATE_IDENT_SEP:		// Found ident and now '.'
      lex->next_state=STATE_IDENT_START;// Next is an ident (not a keyword)
      yylval->lex_str.str=(char*) lex->ptr;
      yylval->lex_str.length=1;
      c=yyGet();			// should be '.'
      return((int) c);

    case STATE_NUMBER_IDENT:		// number or ident which num-start
      while (my_isdigit(system_charset_info,(c = yyGet()))) ;
      if (state_map[c] != STATE_IDENT)
      {					// Can't be identifier
	state=STATE_INT_OR_REAL;
	break;
      }
      if (c == 'e' || c == 'E')
      {
	// The following test is written this way to allow numbers of type 1e1
	if (my_isdigit(system_charset_info,yyPeek()) || 
            (c=(yyGet())) == '+' || c == '-')
	{				// Allow 1E+10
	  if (my_isdigit(system_charset_info,yyPeek()))	// Number must have digit after sign
	  {
	    yySkip();
	    while (my_isdigit(system_charset_info,yyGet())) ;
	    yylval->lex_str=get_token(lex,yyLength());
	    return(FLOAT_NUM);
	  }
	}
	yyUnget(); /* purecov: inspected */
      }
      else if (c == 'x' && (lex->ptr - lex->tok_start) == 2 &&
	  lex->tok_start[0] == '0' )
      {						// Varbinary
	while (my_isxdigit(system_charset_info,(c = yyGet()))) ;
	if ((lex->ptr - lex->tok_start) >= 4 && state_map[c] != STATE_IDENT)
	{
	  yylval->lex_str=get_token(lex,yyLength());
	  yylval->lex_str.str+=2;		// Skip 0x
	  yylval->lex_str.length-=2;
	  lex->yytoklen-=2;
	  return (HEX_NUM);
	}
	yyUnget();
      }
      // fall through
    case STATE_IDENT_START:		// Incomplete ident
#if defined(USE_MB) && defined(USE_MB_IDENT)
      if (use_mb(system_charset_info))
      {
        if (my_ismbhead(system_charset_info, yyGetLast()))
        {
          int l = my_ismbchar(system_charset_info,
                              (const char *)lex->ptr-1,
                              (const char *)lex->end_of_query);
          if (l == 0)
          {
            state = STATE_CHAR;
            continue;
          }
          lex->ptr += l - 1;
        }
        while (state_map[c=yyGet()] == STATE_IDENT ||
               state_map[c] == STATE_NUMBER_IDENT)
        {
          if (my_ismbhead(system_charset_info, c))
          {
            int l;
            if ((l = my_ismbchar(system_charset_info,
                                 (const char *)lex->ptr-1,
                                 (const char *)lex->end_of_query)) == 0)
              break;
            lex->ptr += l-1;
          }
        }
      }
      else
#endif
        while (state_map[c = yyGet()] == STATE_IDENT ||
               state_map[c] == STATE_NUMBER_IDENT) ;

      if (c == '.' && (state_map[yyPeek()] == STATE_IDENT ||
		       state_map[yyPeek()] == STATE_NUMBER_IDENT))
	lex->next_state=STATE_IDENT_SEP;// Next is '.'
      // fall through

    case STATE_FOUND_IDENT:		// Complete ident
      yylval->lex_str=get_token(lex,yyLength());
      if (lex->convert_set)
        lex->convert_set->convert((char*) yylval->lex_str.str,lex->yytoklen);
      return(IDENT);

    case STATE_USER_VARIABLE_DELIMITER:
      lex->tok_start=lex->ptr;			// Skip first `
#ifdef USE_MB
      if (use_mb(system_charset_info))
      {
	while ((c=yyGet()) && state_map[c] != STATE_USER_VARIABLE_DELIMITER &&
	       c != (uchar) NAMES_SEP_CHAR)
	{
          if (my_ismbhead(system_charset_info, c))
          {
            int l;
            if ((l = my_ismbchar(system_charset_info,
                                 (const char *)lex->ptr-1,
                                 (const char *)lex->end_of_query)) == 0)
              break;
            lex->ptr += l-1;
          }
        }
      }
      else
#endif
      {
	while ((c=yyGet()) && state_map[c] != STATE_USER_VARIABLE_DELIMITER &&
	       c != (uchar) NAMES_SEP_CHAR) ;
      }
      yylval->lex_str=get_token(lex,yyLength());
      if (lex->convert_set)
        lex->convert_set->convert((char*) yylval->lex_str.str,lex->yytoklen);
      if (state_map[c] == STATE_USER_VARIABLE_DELIMITER)
	yySkip();			// Skip end `
      return(IDENT);

    case STATE_SIGNED_NUMBER:		// Incomplete signed number
      if (prev_state == STATE_OPERATOR_OR_IDENT)
      {
	if (c == '-' && yyPeek() == '-' &&
	    (my_isspace(system_charset_info,yyPeek2()) || 
             my_iscntrl(system_charset_info,yyPeek2())))
	  state=STATE_COMMENT;
	else
	  state= STATE_CHAR;		// Must be operator
	break;
      }
      if (!my_isdigit(system_charset_info,c=yyGet()) || yyPeek() == 'x')
      {
	if (c != '.')
	{
	  if (c == '-' && my_isspace(system_charset_info,yyPeek()))
	    state=STATE_COMMENT;
	  else
	    state = STATE_CHAR;		// Return sign as single char
	  break;
	}
	yyUnget();			// Fix for next loop
      }
      while (my_isdigit(system_charset_info,c=yyGet())) ;	// Incomplete real or int number
      if ((c == 'e' || c == 'E') &&
	  (yyPeek() == '+' || yyPeek() == '-' || my_isdigit(system_charset_info,yyPeek())))
      {					// Real number
	yyUnget();
	c= '.';				// Fool next test
      }
      // fall through
    case STATE_INT_OR_REAL:		// Compleat int or incompleat real
      if (c != '.')
      {					// Found complete integer number.
	yylval->lex_str=get_token(lex,yyLength());
	return int_token(yylval->lex_str.str,yylval->lex_str.length);
      }
      // fall through
    case STATE_REAL:			// Incomplete real number
      while (my_isdigit(system_charset_info,c = yyGet())) ;

      if (c == 'e' || c == 'E')
      {
	c = yyGet();
	if (c == '-' || c == '+')
	  c = yyGet();			// Skip sign
	if (!my_isdigit(system_charset_info,c))
	{				// No digit after sign
	  state= STATE_CHAR;
	  break;
	}
	while (my_isdigit(system_charset_info,yyGet())) ;
	yylval->lex_str=get_token(lex,yyLength());
	return(FLOAT_NUM);
      }
      yylval->lex_str=get_token(lex,yyLength());
      return(REAL_NUM);

    case STATE_HEX_NUMBER:		// Found x'hexstring'
      yyGet();				// Skip '
      while (my_isxdigit(system_charset_info,(c = yyGet()))) ;
      length=(lex->ptr - lex->tok_start);	// Length of hexnum+3
      if (!(length & 1) || c != '\'')
      {
	return(ABORT_SYM);		// Illegal hex constant
      }
      yyGet();				// get_token makes an unget
      yylval->lex_str=get_token(lex,length);
      yylval->lex_str.str+=2;		// Skip x'
      yylval->lex_str.length-=3;	// Don't count x' and last '
      lex->yytoklen-=3;
      return (HEX_NUM);

    case STATE_CMP_OP:			// Incomplete comparison operator
      if (state_map[yyPeek()] == STATE_CMP_OP ||
	  state_map[yyPeek()] == STATE_LONG_CMP_OP)
	yySkip();
      if ((tokval = find_keyword(lex,(uint) (lex->ptr - lex->tok_start),0)))
      {
	lex->next_state= STATE_START;	// Allow signed numbers
	return(tokval);
      }
      state = STATE_CHAR;		// Something fishy found
      break;

    case STATE_LONG_CMP_OP:		// Incomplete comparison operator
      if (state_map[yyPeek()] == STATE_CMP_OP ||
	  state_map[yyPeek()] == STATE_LONG_CMP_OP)
      {
	yySkip();
	if (state_map[yyPeek()] == STATE_CMP_OP)
	  yySkip();
      }
      if ((tokval = find_keyword(lex,(uint) (lex->ptr - lex->tok_start),0)))
      {
	lex->next_state= STATE_START;	// Found long op
	return(tokval);
      }
      state = STATE_CHAR;		// Something fishy found
      break;

    case STATE_BOOL:
      if (c != yyPeek())
      {
	state=STATE_CHAR;
	break;
      }
      yySkip();
      tokval = find_keyword(lex,2,0);	// Is a bool operator
      lex->next_state= STATE_START;	// Allow signed numbers
      return(tokval);

    case STATE_STRING:			// Incomplete text string
      if (!(yylval->lex_str.str = get_text(lex)))
      {
	state= STATE_CHAR;		// Read char by char
	break;
      }
      yylval->lex_str.length=lex->yytoklen;
      return(TEXT_STRING);

    case STATE_COMMENT:			//  Comment
      lex->select_lex.options|= OPTION_FOUND_COMMENT;
      while ((c = yyGet()) != '\n' && c) ;
      yyUnget();			// Safety against eof
      state = STATE_START;		// Try again
      break;
    case STATE_LONG_COMMENT:		/* Long C comment? */
      if (yyPeek() != '*')
      {
	state=STATE_CHAR;		// Probable division
	break;
      }
      yySkip();				// Skip '*'
      lex->select_lex.options|= OPTION_FOUND_COMMENT;
      if (yyPeek() == '!')		// MySQL command in comment
      {
	ulong version=MYSQL_VERSION_ID;
	yySkip();
	state=STATE_START;
	if (my_isdigit(system_charset_info,yyPeek()))
	{				// Version number
	  version=strtol((char*) lex->ptr,(char**) &lex->ptr,10);
	}
	if (version <= MYSQL_VERSION_ID)
	{
	  lex->in_comment=1;
	  break;
	}
      }
      while (lex->ptr != lex->end_of_query &&
	     ((c=yyGet()) != '*' || yyPeek() != '/'))
      {
	if (c == '\n')
	  lex->yylineno++;
      }
      if (lex->ptr != lex->end_of_query)
	yySkip();			// remove last '/'
      state = STATE_START;		// Try again
      break;
    case STATE_END_LONG_COMMENT:
      if (lex->in_comment && yyPeek() == '/')
      {
	yySkip();
	lex->in_comment=0;
	state=STATE_START;
      }
      else
	state=STATE_CHAR;		// Return '*'
      break;
    case STATE_SET_VAR:			// Check if ':='
      if (yyPeek() != '=')
      {
	state=STATE_CHAR;		// Return ':'
	break;
      }
      yySkip();
      return (SET_VAR);
    case STATE_COLON:			// optional line terminator
      if (yyPeek())
      {
	state=STATE_CHAR;		// Return ';'
	break;
      }
      /* fall true */
    case STATE_EOL:
      lex->next_state=STATE_END;	// Mark for next loop
      return(END_OF_INPUT);
    case STATE_END:
      lex->next_state=STATE_END;
      return(0);			// We found end of input last time

      // Actually real shouldn't start
      // with . but allow them anyhow
    case STATE_REAL_OR_POINT:
      if (my_isdigit(system_charset_info,yyPeek()))
	state = STATE_REAL;		// Real
      else
      {
	state = STATE_CHAR;		// return '.'
	lex->next_state=STATE_IDENT_START;// Next is an ident (not a keyword)
      }
      break;
    case STATE_USER_END:		// end '@' of user@hostname
      switch (state_map[yyPeek()])
      {
      case STATE_STRING:
      case STATE_USER_VARIABLE_DELIMITER:
	break;
      case STATE_USER_END:
	lex->next_state=STATE_USER_END;
	yySkip();
	break;
      default:
	lex->next_state=STATE_HOSTNAME;
	break;
      }
      yylval->lex_str.str=(char*) lex->ptr;
      yylval->lex_str.length=1;
      return((int) '@');
    case STATE_HOSTNAME:		// end '@' of user@hostname
      for (c=yyGet() ;
	   my_isalnum(system_charset_info,c) || c == '.' || c == '_' || c == '$';
	   c= yyGet()) ;
      yylval->lex_str=get_token(lex,yyLength());
      return(LEX_HOSTNAME);
    }
  }
}

/*
  st_select_lex structures initialisations
*/

void st_select_lex_node::init_query()
{
  next= master= slave= link_next= 0;
  prev= link_prev= 0;
}

void st_select_lex_node::init_select()
{
  order_list.elements= 0;
  order_list.first= 0;
  order_list.next= (byte**) &order_list.first;
  select_limit= HA_POS_ERROR;
  offset_limit= 0; 
}

void st_select_lex_unit::init_query()
{
  linkage= GLOBAL_OPTIONS_TYPE;
  st_select_lex_node::init_query();
  global_parameters= this;
  select_limit_cnt= HA_POS_ERROR;
  offset_limit_cnt= 0;
}

void st_select_lex::init_query()
{
  st_select_lex_node::init_query();
  table_list.elements= 0;
  table_list.first= 0; 
  table_list.next= (byte**) &table_list.first;
  item_list.empty();
}

void st_select_lex::init_select()
{
  st_select_lex_node::init_select();
  group_list.elements= 0;
  group_list.first= 0;
  group_list.next= (byte**) &group_list.first;
  options= 0;
  where= having= 0;
  when_list.empty(); 
  expr_list.empty();
  interval_list.empty(); 
  use_index.empty();
  ftfunc_list.empty();
  linkage=UNSPECIFIED_TYPE;
}

/*
  st_select_lex structures linking
*/

/* include on level down */
void st_select_lex_node::include_down(st_select_lex_node *upper)
{
  if ((next= upper->slave))
    next->prev= &next;
  prev= &upper->slave;
  upper->slave= this;
  master= upper;
}

/* include neighbour (on same level) */
void st_select_lex_node::include_neighbour(st_select_lex_node *before)
{
  if ((next= before->next))
    next->prev= &next;
  prev= &before->next;
  before->next= this;
  master= before->master;
}

/* including in global SELECT_LEX list */
void st_select_lex_node::include_global(st_select_lex_node **plink)
{
  if ((link_next= *plink))
    link_next->link_prev= &link_next;
  link_prev= plink;
  *plink= this;
}

//excluding from global list (internal function)
void st_select_lex_node::fast_exclude()
{
  if(link_prev)
  {
    if ((*link_prev= link_next))
      link_next->link_prev= link_prev;
    // Remove slave structure
    for (; slave; slave= slave->next)
      slave->fast_exclude();
  }
}

/*
  excluding select_lex structure (except first (first select can't be
  deleted, because it is most upper select))
*/
void st_select_lex_node::exclude()
{
  //exclude from global list
  fast_exclude();
  //exclude from other structures
  if ((*prev= next))
    next->prev= prev;
  /* 
     We do not need following statements, because prev pointer of first 
     list element point to master->slave
     if (master->slave == this)
       master->slave= next;
  */
}

/*
  This is used for UNION & subselect to create a new table list of all used 
  tables.
  The table_list->table entry in all used tables are set to point
  to the entries in this list.
*/

// interface
bool st_select_lex_unit::create_total_list(THD *thd, st_lex *lex,
					   TABLE_LIST **result)
{
  *result= 0;
  return create_total_list_n_last_return(thd, lex, &result);
}

// list creator
bool st_select_lex_unit::create_total_list_n_last_return(THD *thd, st_lex *lex,
							 TABLE_LIST ***result)
{
  TABLE_LIST *slave_list_first=0, **slave_list_last= &slave_list_first;
  TABLE_LIST **new_table_list= *result, *aux;
  SELECT_LEX *sl= (SELECT_LEX*)slave;
  for (; sl; sl= (SELECT_LEX*)sl->next)
  {
    // check usage of ORDER BY in union
    if (sl->order_list.first && sl->next && !sl->braces)
    {
      net_printf(&thd->net,ER_WRONG_USAGE,"UNION","ORDER BY");
      return 1;
    }
    if (sl->slave)
      if (((SELECT_LEX_UNIT *)
	   sl->slave)->create_total_list_n_last_return(thd, lex,
						       &slave_list_last))
	return 1;
    if ((aux= (TABLE_LIST*) sl->table_list.first))
    {
      TABLE_LIST *next;
      for (; aux; aux= next)
      {
	TABLE_LIST *cursor;
	next= aux->next;
	for (cursor= **result; cursor; cursor= cursor->next)
	  if (!strcmp(cursor->db, aux->db) &&
	      !strcmp(cursor->real_name, aux->real_name) &&
	      !strcmp(cursor->name, aux->name))
	    break;
	if (!cursor)
	{
	  /* Add not used table to the total table list */
	  aux->lock_type= lex->lock_option;
	  if (!(cursor= (TABLE_LIST *) thd->memdup((char*) aux,
						   sizeof(*aux))))
	  {
	    send_error(&thd->net,0);
	    return 1;
	  }
	  *new_table_list= cursor;
	  new_table_list= &cursor->next;
	  *new_table_list= 0;			// end result list
	}
	else
	  aux->shared= 1;			// Mark that it's used twice
	aux->table_list= cursor;
      }
    }
  }
  if (slave_list_first)
  {
    *new_table_list= slave_list_first;
    new_table_list= slave_list_last;
  }
  *result= new_table_list;
  return 0;
}
