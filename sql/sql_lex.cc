/* Copyright (c) 2000, 2014, Oracle and/or its affiliates.
   Copyright (c) 2009, 2016, MariaDB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */


/* A lexical scanner on a temporary buffer with a yacc interface */

#define MYSQL_LEX 1
#include "sql_priv.h"
#include "unireg.h"                    // REQUIRED: for other includes
#include "sql_class.h"                          // sql_lex.h: SQLCOM_END
#include "sql_lex.h"
#include "sql_parse.h"                          // add_to_list
#include "item_create.h"
#include <m_ctype.h>
#include <hash.h>
#include "sp_head.h"
#include "sp.h"
#include "sql_select.h"

static int lex_one_token(YYSTYPE *yylval, THD *thd);

/*
  We are using pointer to this variable for distinguishing between assignment
  to NEW row field (when parsing trigger definition) and structured variable.
*/

sys_var *trg_new_row_fake_var= (sys_var*) 0x01;

/**
  LEX_STRING constant for null-string to be used in parser and other places.
*/
const LEX_STRING null_lex_str= {NULL, 0};
const LEX_STRING empty_lex_str= {(char *) "", 0};
/**
  @note The order of the elements of this array must correspond to
  the order of elements in enum_binlog_stmt_unsafe.
*/
const int
Query_tables_list::binlog_stmt_unsafe_errcode[BINLOG_STMT_UNSAFE_COUNT] =
{
  ER_BINLOG_UNSAFE_LIMIT,
  ER_BINLOG_UNSAFE_INSERT_DELAYED,
  ER_BINLOG_UNSAFE_SYSTEM_TABLE,
  ER_BINLOG_UNSAFE_AUTOINC_COLUMNS,
  ER_BINLOG_UNSAFE_UDF,
  ER_BINLOG_UNSAFE_SYSTEM_VARIABLE,
  ER_BINLOG_UNSAFE_SYSTEM_FUNCTION,
  ER_BINLOG_UNSAFE_NONTRANS_AFTER_TRANS,
  ER_BINLOG_UNSAFE_MULTIPLE_ENGINES_AND_SELF_LOGGING_ENGINE,
  ER_BINLOG_UNSAFE_MIXED_STATEMENT,
  ER_BINLOG_UNSAFE_INSERT_IGNORE_SELECT,
  ER_BINLOG_UNSAFE_INSERT_SELECT_UPDATE,
  ER_BINLOG_UNSAFE_WRITE_AUTOINC_SELECT,
  ER_BINLOG_UNSAFE_REPLACE_SELECT,
  ER_BINLOG_UNSAFE_CREATE_IGNORE_SELECT,
  ER_BINLOG_UNSAFE_CREATE_REPLACE_SELECT,
  ER_BINLOG_UNSAFE_CREATE_SELECT_AUTOINC,
  ER_BINLOG_UNSAFE_UPDATE_IGNORE,
  ER_BINLOG_UNSAFE_INSERT_TWO_KEYS,
  ER_BINLOG_UNSAFE_AUTOINC_NOT_FIRST
};


/* Longest standard keyword name */

#define TOCK_NAME_LENGTH 24

/*
  The following data is based on the latin1 character set, and is only
  used when comparing keywords
*/

static uchar to_upper_lex[]=
{
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

/* 
  Names of the index hints (for error messages). Keep in sync with 
  index_hint_type 
*/

const char * index_hint_type_name[] =
{
  "IGNORE INDEX", 
  "USE INDEX", 
  "FORCE INDEX"
};

inline int lex_casecmp(const char *s, const char *t, uint len)
{
  while (len-- != 0 &&
	 to_upper_lex[(uchar) *s++] == to_upper_lex[(uchar) *t++]) ;
  return (int) len+1;
}

#include <lex_hash.h>


void lex_init(void)
{
  uint i;
  DBUG_ENTER("lex_init");
  for (i=0 ; i < array_elements(symbols) ; i++)
    symbols[i].length=(uchar) strlen(symbols[i].name);
  for (i=0 ; i < array_elements(sql_functions) ; i++)
    sql_functions[i].length=(uchar) strlen(sql_functions[i].name);

  DBUG_VOID_RETURN;
}


void lex_free(void)
{					// Call this when daemon ends
  DBUG_ENTER("lex_free");
  DBUG_VOID_RETURN;
}

/**
  Initialize lex object for use in fix_fields and parsing.

  SYNOPSIS
    init_lex_with_single_table()
    @param thd                 The thread object
    @param table               The table object
  @return Operation status
    @retval TRUE                An error occurred, memory allocation error
    @retval FALSE               Ok

  DESCRIPTION
    This function is used to initialize a lex object on the
    stack for use by fix_fields and for parsing. In order to
    work properly it also needs to initialize the
    Name_resolution_context object of the lexer.
    Finally it needs to set a couple of variables to ensure
    proper functioning of fix_fields.
*/

int
init_lex_with_single_table(THD *thd, TABLE *table, LEX *lex)
{
  TABLE_LIST *table_list;
  Table_ident *table_ident;
  SELECT_LEX *select_lex= &lex->select_lex;
  Name_resolution_context *context= &select_lex->context;
  /*
    We will call the parser to create a part_info struct based on the
    partition string stored in the frm file.
    We will use a local lex object for this purpose. However we also
    need to set the Name_resolution_object for this lex object. We
    do this by using add_table_to_list where we add the table that
    we're working with to the Name_resolution_context.
  */
  thd->lex= lex;
  lex_start(thd);
  context->init();
  if ((!(table_ident= new Table_ident(thd,
                                      table->s->table_name,
                                      table->s->db, TRUE))) ||
      (!(table_list= select_lex->add_table_to_list(thd,
                                                   table_ident,
                                                   NULL,
                                                   0))))
    return TRUE;
  context->resolve_in_table_list_only(table_list);
  lex->use_only_table_context= TRUE;
  lex->context_analysis_only|= CONTEXT_ANALYSIS_ONLY_VCOL_EXPR;
  select_lex->cur_pos_in_select_list= UNDEF_POS;
  table->map= 1; //To ensure correct calculation of const item
  table->get_fields_in_item_tree= TRUE;
  table_list->table= table;
  table_list->cacheable_table= false;
  return FALSE;
}

/**
  End use of local lex with single table

  SYNOPSIS
    end_lex_with_single_table()
    @param thd               The thread object
    @param table             The table object
    @param old_lex           The real lex object connected to THD

  DESCRIPTION
    This function restores the real lex object after calling
    init_lex_with_single_table and also restores some table
    variables temporarily set.
*/

void
end_lex_with_single_table(THD *thd, TABLE *table, LEX *old_lex)
{
  LEX *lex= thd->lex;
  table->map= 0;
  table->get_fields_in_item_tree= FALSE;
  lex_end(lex);
  thd->lex= old_lex;
}


void
st_parsing_options::reset()
{
  allows_variable= TRUE;
  allows_select_into= TRUE;
  allows_select_procedure= TRUE;
  allows_derived= TRUE;
}


/**
  Perform initialization of Lex_input_stream instance.

  Basically, a buffer for pre-processed query. This buffer should be large
  enough to keep multi-statement query. The allocation is done once in
  Lex_input_stream::init() in order to prevent memory pollution when
  the server is processing large multi-statement queries.
*/

bool Lex_input_stream::init(THD *thd,
			    char* buff,
			    unsigned int length)
{
  DBUG_EXECUTE_IF("bug42064_simulate_oom",
                  DBUG_SET("+d,simulate_out_of_memory"););

  m_cpp_buf= (char*) thd->alloc(length + 1);

  DBUG_EXECUTE_IF("bug42064_simulate_oom",
                  DBUG_SET("-d,bug42064_simulate_oom");); 

  if (m_cpp_buf == NULL)
    return TRUE;

  m_thd= thd;
  reset(buff, length);

  return FALSE;
}


/**
  Prepare Lex_input_stream instance state for use for handling next SQL statement.

  It should be called between two statements in a multi-statement query.
  The operation resets the input stream to the beginning-of-parse state,
  but does not reallocate m_cpp_buf.
*/

void
Lex_input_stream::reset(char *buffer, unsigned int length)
{
  yylineno= 1;
  yytoklen= 0;
  yylval= NULL;
  lookahead_token= -1;
  lookahead_yylval= NULL;
  m_ptr= buffer;
  m_tok_start= NULL;
  m_tok_end= NULL;
  m_end_of_query= buffer + length;
  m_tok_start_prev= NULL;
  m_buf= buffer;
  m_buf_length= length;
  m_echo= TRUE;
  m_cpp_tok_start= NULL;
  m_cpp_tok_start_prev= NULL;
  m_cpp_tok_end= NULL;
  m_body_utf8= NULL;
  m_cpp_utf8_processed_ptr= NULL;
  next_state= MY_LEX_START;
  found_semicolon= NULL;
  ignore_space= test(m_thd->variables.sql_mode & MODE_IGNORE_SPACE);
  stmt_prepare_mode= FALSE;
  multi_statements= TRUE;
  in_comment=NO_COMMENT;
  m_underscore_cs= NULL;
  m_cpp_ptr= m_cpp_buf;
}


/**
  The operation is called from the parser in order to
  1) designate the intention to have utf8 body;
  1) Indicate to the lexer that we will need a utf8 representation of this
     statement;
  2) Determine the beginning of the body.

  @param thd        Thread context.
  @param begin_ptr  Pointer to the start of the body in the pre-processed
                    buffer.
*/

void Lex_input_stream::body_utf8_start(THD *thd, const char *begin_ptr)
{
  DBUG_ASSERT(begin_ptr);
  DBUG_ASSERT(m_cpp_buf <= begin_ptr && begin_ptr <= m_cpp_buf + m_buf_length);

  uint body_utf8_length=
    (m_buf_length / thd->variables.character_set_client->mbminlen) *
    my_charset_utf8_bin.mbmaxlen;

  m_body_utf8= (char *) thd->alloc(body_utf8_length + 1);
  m_body_utf8_ptr= m_body_utf8;
  *m_body_utf8_ptr= 0;

  m_cpp_utf8_processed_ptr= begin_ptr;
}

/**
  @brief The operation appends unprocessed part of pre-processed buffer till
  the given pointer (ptr) and sets m_cpp_utf8_processed_ptr to end_ptr.

  The idea is that some tokens in the pre-processed buffer (like character
  set introducers) should be skipped.

  Example:
    CPP buffer: SELECT 'str1', _latin1 'str2';
    m_cpp_utf8_processed_ptr -- points at the "SELECT ...";
    In order to skip "_latin1", the following call should be made:
      body_utf8_append(<pointer to "_latin1 ...">, <pointer to " 'str2'...">)

  @param ptr      Pointer in the pre-processed buffer, which specifies the
                  end of the chunk, which should be appended to the utf8
                  body.
  @param end_ptr  Pointer in the pre-processed buffer, to which
                  m_cpp_utf8_processed_ptr will be set in the end of the
                  operation.
*/

void Lex_input_stream::body_utf8_append(const char *ptr,
                                        const char *end_ptr)
{
  DBUG_ASSERT(m_cpp_buf <= ptr && ptr <= m_cpp_buf + m_buf_length);
  DBUG_ASSERT(m_cpp_buf <= end_ptr && end_ptr <= m_cpp_buf + m_buf_length);

  if (!m_body_utf8)
    return;

  if (m_cpp_utf8_processed_ptr >= ptr)
    return;

  int bytes_to_copy= ptr - m_cpp_utf8_processed_ptr;

  memcpy(m_body_utf8_ptr, m_cpp_utf8_processed_ptr, bytes_to_copy);
  m_body_utf8_ptr += bytes_to_copy;
  *m_body_utf8_ptr= 0;

  m_cpp_utf8_processed_ptr= end_ptr;
}

/**
  The operation appends unprocessed part of the pre-processed buffer till
  the given pointer (ptr) and sets m_cpp_utf8_processed_ptr to ptr.

  @param ptr  Pointer in the pre-processed buffer, which specifies the end
              of the chunk, which should be appended to the utf8 body.
*/

void Lex_input_stream::body_utf8_append(const char *ptr)
{
  body_utf8_append(ptr, ptr);
}

/**
  The operation converts the specified text literal to the utf8 and appends
  the result to the utf8-body.

  @param thd      Thread context.
  @param txt      Text literal.
  @param txt_cs   Character set of the text literal.
  @param end_ptr  Pointer in the pre-processed buffer, to which
                  m_cpp_utf8_processed_ptr will be set in the end of the
                  operation.
*/

void Lex_input_stream::body_utf8_append_literal(THD *thd,
                                                const LEX_STRING *txt,
                                                CHARSET_INFO *txt_cs,
                                                const char *end_ptr)
{
  if (!m_cpp_utf8_processed_ptr)
    return;

  LEX_STRING utf_txt;

  if (!my_charset_same(txt_cs, &my_charset_utf8_general_ci))
  {
    thd->convert_string(&utf_txt,
                        &my_charset_utf8_general_ci,
                        txt->str, (uint) txt->length,
                        txt_cs);
  }
  else
  {
    utf_txt.str= txt->str;
    utf_txt.length= txt->length;
  }

  /* NOTE: utf_txt.length is in bytes, not in symbols. */

  memcpy(m_body_utf8_ptr, utf_txt.str, utf_txt.length);
  m_body_utf8_ptr += utf_txt.length;
  *m_body_utf8_ptr= 0;

  m_cpp_utf8_processed_ptr= end_ptr;
}


/*
  This is called before every query that is to be parsed.
  Because of this, it's critical to not do too much things here.
  (We already do too much here)
*/

void lex_start(THD *thd)
{
  LEX *lex= thd->lex;
  DBUG_ENTER("lex_start");

  lex->thd= lex->unit.thd= thd;

  lex->context_stack.empty();
  lex->unit.init_query();
  lex->unit.init_select();
  /* 'parent_lex' is used in init_query() so it must be before it. */
  lex->select_lex.parent_lex= lex;
  lex->select_lex.init_query();
  lex->value_list.empty();
  lex->update_list.empty();
  lex->set_var_list.empty();
  lex->param_list.empty();
  lex->view_list.empty();
  lex->prepared_stmt_params.empty();
  lex->auxiliary_table_list.empty();
  lex->unit.next= lex->unit.master=
    lex->unit.link_next= lex->unit.return_to= 0;
  lex->unit.prev= lex->unit.link_prev= 0;
  lex->unit.slave= lex->unit.global_parameters= lex->current_select=
    lex->all_selects_list= &lex->select_lex;
  lex->select_lex.master= &lex->unit;
  lex->select_lex.prev= &lex->unit.slave;
  lex->select_lex.link_next= lex->select_lex.slave= lex->select_lex.next= 0;
  lex->select_lex.link_prev= (st_select_lex_node**)&(lex->all_selects_list);
  lex->select_lex.options= 0;
  lex->select_lex.sql_cache= SELECT_LEX::SQL_CACHE_UNSPECIFIED;
  lex->select_lex.init_order();
  lex->select_lex.group_list.empty();
  if (lex->select_lex.group_list_ptrs)
    lex->select_lex.group_list_ptrs->clear();
  lex->describe= 0;
  lex->subqueries= FALSE;
  lex->context_analysis_only= 0;
  lex->derived_tables= 0;
  lex->safe_to_cache_query= 1;
  lex->parsing_options.reset();
  lex->empty_field_list_on_rset= 0;
  lex->select_lex.select_number= 1;
  lex->length=0;
  lex->part_info= 0;
  lex->select_lex.in_sum_expr=0;
  lex->select_lex.ftfunc_list_alloc.empty();
  lex->select_lex.ftfunc_list= &lex->select_lex.ftfunc_list_alloc;
  lex->select_lex.group_list.empty();
  lex->select_lex.order_list.empty();
  lex->select_lex.gorder_list.empty();
  lex->duplicates= DUP_ERROR;
  lex->ignore= 0;
  lex->spname= NULL;
  lex->sphead= NULL;
  lex->spcont= NULL;
  lex->m_stmt= NULL;
  lex->proc_list.first= 0;
  lex->escape_used= FALSE;
  lex->query_tables= 0;
  lex->reset_query_tables_list(FALSE);
  lex->expr_allows_subselect= TRUE;
  lex->use_only_table_context= FALSE;
  lex->parse_vcol_expr= FALSE;

  lex->name.str= 0;
  lex->name.length= 0;
  lex->event_parse_data= NULL;
  lex->profile_options= PROFILE_NONE;
  lex->nest_level=0 ;
  lex->select_lex.nest_level_base= &lex->unit;
  lex->allow_sum_func= 0;
  lex->in_sum_func= NULL;
  /*
    ok, there must be a better solution for this, long-term
    I tried "bzero" in the sql_yacc.yy code, but that for
    some reason made the values zero, even if they were set
  */
  lex->server_options.server_name= 0;
  lex->server_options.server_name_length= 0;
  lex->server_options.host= 0;
  lex->server_options.db= 0;
  lex->server_options.username= 0;
  lex->server_options.password= 0;
  lex->server_options.scheme= 0;
  lex->server_options.socket= 0;
  lex->server_options.owner= 0;
  lex->server_options.port= -1;

  lex->is_lex_started= TRUE;
  lex->used_tables= 0;
  lex->only_view= FALSE;
  lex->reset_slave_info.all= false;
  lex->limit_rows_examined= 0;
  lex->limit_rows_examined_cnt= ULONGLONG_MAX;
  DBUG_VOID_RETURN;
}

void lex_end(LEX *lex)
{
  DBUG_ENTER("lex_end");
  DBUG_PRINT("enter", ("lex: 0x%lx", (long) lex));

  lex_end_stage1(lex);
  lex_end_stage2(lex);

  DBUG_VOID_RETURN;
}

void lex_end_stage1(LEX *lex)
{
  DBUG_ENTER("lex_end_stage1");

  /* release used plugins */
  if (lex->plugins.elements) /* No function call and no mutex if no plugins. */
  {
    plugin_unlock_list(0, (plugin_ref*)lex->plugins.buffer, 
                       lex->plugins.elements);
  }
  reset_dynamic(&lex->plugins);

  delete lex->sphead;
  lex->sphead= NULL;

  DBUG_VOID_RETURN;
}

/*
  MASTER INFO parameters (or state) is normally cleared towards the end
  of a statement. But in case of PS, the state needs to be preserved during
  its lifetime and should only be cleared on PS close or deallocation.
*/
void lex_end_stage2(LEX *lex)
{
  DBUG_ENTER("lex_end_stage2");

  /* Reset LEX_MASTER_INFO */
  lex->mi.reset();

  DBUG_VOID_RETURN;
}

Yacc_state::~Yacc_state()
{
  if (yacc_yyss)
  {
    my_free(yacc_yyss);
    my_free(yacc_yyvs);
  }
}

static int find_keyword(Lex_input_stream *lip, uint len, bool function)
{
  const char *tok= lip->get_tok_start();

  SYMBOL *symbol= get_hash_symbol(tok, len, function);
  if (symbol)
  {
    lip->yylval->symbol.symbol=symbol;
    lip->yylval->symbol.str= (char*) tok;
    lip->yylval->symbol.length=len;

    if ((symbol->tok == NOT_SYM) &&
        (lip->m_thd->variables.sql_mode & MODE_HIGH_NOT_PRECEDENCE))
      return NOT2_SYM;
    if ((symbol->tok == OR_OR_SYM) &&
	!(lip->m_thd->variables.sql_mode & MODE_PIPES_AS_CONCAT))
      return OR2_SYM;

    return symbol->tok;
  }
  return 0;
}

/*
  Check if name is a keyword

  SYNOPSIS
    is_keyword()
    name      checked name (must not be empty)
    len       length of checked name

  RETURN VALUES
    0         name is a keyword
    1         name isn't a keyword
*/

bool is_keyword(const char *name, uint len)
{
  DBUG_ASSERT(len != 0);
  return get_hash_symbol(name,len,0)!=0;
}

/**
  Check if name is a sql function

    @param name      checked name

    @return is this a native function or not
    @retval 0         name is a function
    @retval 1         name isn't a function
*/

bool is_lex_native_function(const LEX_STRING *name)
{
  DBUG_ASSERT(name != NULL);
  return (get_hash_symbol(name->str, (uint) name->length, 1) != 0);
}

/* make a copy of token before ptr and set yytoklen */

static LEX_STRING get_token(Lex_input_stream *lip, uint skip, uint length)
{
  LEX_STRING tmp;
  lip->yyUnget();                       // ptr points now after last token char
  tmp.length=lip->yytoklen=length;
  tmp.str= lip->m_thd->strmake(lip->get_tok_start() + skip, tmp.length);

  lip->m_cpp_text_start= lip->get_cpp_tok_start() + skip;
  lip->m_cpp_text_end= lip->m_cpp_text_start + tmp.length;

  return tmp;
}

/* 
 todo: 
   There are no dangerous charsets in mysql for function 
   get_quoted_token yet. But it should be fixed in the 
   future to operate multichar strings (like ucs2)
*/

static LEX_STRING get_quoted_token(Lex_input_stream *lip,
                                   uint skip,
                                   uint length, char quote)
{
  LEX_STRING tmp;
  const char *from, *end;
  char *to;
  lip->yyUnget();                       // ptr points now after last token char
  tmp.length= lip->yytoklen=length;
  tmp.str=(char*) lip->m_thd->alloc(tmp.length+1);
  from= lip->get_tok_start() + skip;
  to= tmp.str;
  end= to+length;

  lip->m_cpp_text_start= lip->get_cpp_tok_start() + skip;
  lip->m_cpp_text_end= lip->m_cpp_text_start + length;

  for ( ; to != end; )
  {
    if ((*to++= *from++) == quote)
    {
      from++;					// Skip double quotes
      lip->m_cpp_text_start++;
    }
  }
  *to= 0;					// End null for safety
  return tmp;
}


/*
  Return an unescaped text literal without quotes
  Fix sometimes to do only one scan of the string
*/

static char *get_text(Lex_input_stream *lip, int pre_skip, int post_skip)
{
  reg1 uchar c,sep;
  uint found_escape=0;
  CHARSET_INFO *cs= lip->m_thd->charset();

  lip->tok_bitmap= 0;
  sep= lip->yyGetLast();                        // String should end with this
  while (! lip->eof())
  {
    c= lip->yyGet();
    lip->tok_bitmap|= c;
#ifdef USE_MB
    {
      int l;
      if (use_mb(cs) &&
          (l = my_ismbchar(cs,
                           lip->get_ptr() -1,
                           lip->get_end_of_query()))) {
        lip->skip_binary(l-1);
        continue;
      }
    }
#endif
    if (c == '\\' &&
        !(lip->m_thd->variables.sql_mode & MODE_NO_BACKSLASH_ESCAPES))
    {					// Escaped character
      found_escape=1;
      if (lip->eof())
	return 0;
      lip->yySkip();
    }
    else if (c == sep)
    {
      if (c == lip->yyGet())            // Check if two separators in a row
      {
        found_escape=1;                 // duplicate. Remember for delete
	continue;
      }
      else
        lip->yyUnget();

      /* Found end. Unescape and return string */
      const char *str, *end;
      char *start;

      str= lip->get_tok_start();
      end= lip->get_ptr();
      /* Extract the text from the token */
      str += pre_skip;
      end -= post_skip;
      DBUG_ASSERT(end >= str);

      if (!(start= (char*) lip->m_thd->alloc((uint) (end-str)+1)))
	return (char*) "";		// Sql_alloc has set error flag

      lip->m_cpp_text_start= lip->get_cpp_tok_start() + pre_skip;
      lip->m_cpp_text_end= lip->get_cpp_ptr() - post_skip;

      if (!found_escape)
      {
	lip->yytoklen=(uint) (end-str);
	memcpy(start,str,lip->yytoklen);
	start[lip->yytoklen]=0;
      }
      else
      {
        char *to;

	for (to=start ; str != end ; str++)
	{
#ifdef USE_MB
	  int l;
	  if (use_mb(cs) &&
              (l = my_ismbchar(cs, str, end))) {
	      while (l--)
		  *to++ = *str++;
	      str--;
	      continue;
	  }
#endif
	  if (!(lip->m_thd->variables.sql_mode & MODE_NO_BACKSLASH_ESCAPES) &&
              *str == '\\' && str+1 != end)
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
              *to++= *str;
	      break;
	    }
	  }
	  else if (*str == sep)
	    *to++= *str++;		// Two ' or "
	  else
	    *to++ = *str;
	}
	*to=0;
	lip->yytoklen=(uint) (to-start);
      }
      return start;
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

static inline uint int_token(const char *str,uint length)
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
      return DECIMAL_NUM;
    else
    {
      cmp=signed_longlong_str+1;
      smaller=LONG_NUM;				// If <= signed_longlong_str
      bigger=DECIMAL_NUM;
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
        return DECIMAL_NUM;
      cmp=unsigned_longlong_str;
      smaller=ULONGLONG_NUM;
      bigger=DECIMAL_NUM;
    }
    else
    {
      cmp=longlong_str;
      smaller=LONG_NUM;
      bigger= ULONGLONG_NUM;
    }
  }
  while (*cmp && *cmp++ == *str++) ;
  return ((uchar) str[-1] <= (uchar) cmp[-1]) ? smaller : bigger;
}


/**
  Given a stream that is advanced to the first contained character in 
  an open comment, consume the comment.  Optionally, if we are allowed, 
  recurse so that we understand comments within this current comment.

  At this level, we do not support version-condition comments.  We might 
  have been called with having just passed one in the stream, though.  In 
  that case, we probably want to tolerate mundane comments inside.  Thus,
  the case for recursion.

  @retval  Whether EOF reached before comment is closed.
*/
bool consume_comment(Lex_input_stream *lip, int remaining_recursions_permitted)
{
  reg1 uchar c;
  while (! lip->eof())
  {
    c= lip->yyGet();

    if (remaining_recursions_permitted > 0)
    {
      if ((c == '/') && (lip->yyPeek() == '*'))
      {
        lip->yySkip(); /* Eat asterisk */
        consume_comment(lip, remaining_recursions_permitted-1);
        continue;
      }
    }

    if (c == '*')
    {
      if (lip->yyPeek() == '/')
      {
        lip->yySkip(); /* Eat slash */
        return FALSE;
      }
    }

    if (c == '\n')
      lip->yylineno++;
  }

  return TRUE;
}


/*
  MYSQLlex remember the following states from the following MYSQLlex()

  @param yylval         [out]  semantic value of the token being parsed (yylval)
  @param thd            THD

  - MY_LEX_EOQ			Found end of query
  - MY_LEX_OPERATOR_OR_IDENT	Last state was an ident, text or number
				(which can't be followed by a signed number)
*/

int MYSQLlex(YYSTYPE *yylval, THD *thd)
{
  Lex_input_stream *lip= & thd->m_parser_state->m_lip;
  int token;

  if (lip->lookahead_token >= 0)
  {
    /*
      The next token was already parsed in advance,
      return it.
    */
    token= lip->lookahead_token;
    lip->lookahead_token= -1;
    *yylval= *(lip->lookahead_yylval);
    lip->lookahead_yylval= NULL;
    return token;
  }

  token= lex_one_token(yylval, thd);

  switch(token) {
  case WITH:
    /*
      Parsing 'WITH' 'ROLLUP' or 'WITH' 'CUBE' requires 2 look ups,
      which makes the grammar LALR(2).
      Replace by a single 'WITH_ROLLUP' or 'WITH_CUBE' token,
      to transform the grammar into a LALR(1) grammar,
      which sql_yacc.yy can process.
    */
    token= lex_one_token(yylval, thd);
    switch(token) {
    case CUBE_SYM:
      return WITH_CUBE_SYM;
    case ROLLUP_SYM:
      return WITH_ROLLUP_SYM;
    default:
      /*
        Save the token following 'WITH'
      */
      lip->lookahead_yylval= lip->yylval;
      lip->yylval= NULL;
      lip->lookahead_token= token;
      return WITH;
    }
    break;
  default:
    break;
  }

  return token;
}

static int lex_one_token(YYSTYPE *yylval, THD *thd)
{
  reg1	uchar c;
  bool comment_closed;
  int	tokval, result_state;
  uint length;
  enum my_lex_states state;
  Lex_input_stream *lip= & thd->m_parser_state->m_lip;
  LEX *lex= thd->lex;
  CHARSET_INFO *const cs= thd->charset();
  const uchar *const state_map= cs->state_map;
  const uchar *const ident_map= cs->ident_map;

  LINT_INIT(c);
  lip->yylval=yylval;			// The global state

  lip->start_token();
  state=lip->next_state;
  lip->next_state=MY_LEX_OPERATOR_OR_IDENT;
  for (;;)
  {
    switch (state) {
    case MY_LEX_OPERATOR_OR_IDENT:	// Next is operator or keyword
    case MY_LEX_START:			// Start of token
      // Skip starting whitespace
      while(state_map[c= lip->yyPeek()] == MY_LEX_SKIP)
      {
	if (c == '\n')
	  lip->yylineno++;

        lip->yySkip();
      }

      /* Start of real token */
      lip->restart_token();
      c= lip->yyGet();
      state= (enum my_lex_states) state_map[c];
      break;
    case MY_LEX_ESCAPE:
      if (!lip->eof() && lip->yyGet() == 'N')
      {					// Allow \N as shortcut for NULL
	yylval->lex_str.str=(char*) "\\N";
	yylval->lex_str.length=2;
	return NULL_SYM;
      }
      /* Fall through */
    case MY_LEX_CHAR:			// Unknown or single char token
    case MY_LEX_SKIP:			// This should not happen
      if (c != ')')
	lip->next_state= MY_LEX_START;	// Allow signed numbers
      return((int) c);

    case MY_LEX_MINUS_OR_COMMENT:
      if (lip->yyPeek() == '-' &&
          (my_isspace(cs,lip->yyPeekn(1)) ||
           my_iscntrl(cs,lip->yyPeekn(1))))
      {
        state=MY_LEX_COMMENT;
        break;
      }
      lip->next_state= MY_LEX_START;	// Allow signed numbers
      return((int) c);

    case MY_LEX_PLACEHOLDER:
      /*
        Check for a placeholder: it should not precede a possible identifier
        because of binlogging: when a placeholder is replaced with
        its value in a query for the binlog, the query must stay
        grammatically correct.
      */
      lip->next_state= MY_LEX_START;	// Allow signed numbers
      if (lip->stmt_prepare_mode && !ident_map[(uchar) lip->yyPeek()])
        return(PARAM_MARKER);
      return((int) c);

    case MY_LEX_COMMA:
      lip->next_state= MY_LEX_START;	// Allow signed numbers
      /*
        Warning:
        This is a work around, to make the "remember_name" rule in
        sql/sql_yacc.yy work properly.
        The problem is that, when parsing "select expr1, expr2",
        the code generated by bison executes the *pre* action
        remember_name (see select_item) *before* actually parsing the
        first token of expr2.
      */
      lip->restart_token();
      return((int) c);

    case MY_LEX_IDENT_OR_NCHAR:
      if (lip->yyPeek() != '\'')
      {
	state= MY_LEX_IDENT;
	break;
      }
      /* Found N'string' */
      lip->yySkip();                         // Skip '
      if (!(yylval->lex_str.str = get_text(lip, 2, 1)))
      {
	state= MY_LEX_CHAR;             // Read char by char
	break;
      }
      yylval->lex_str.length= lip->yytoklen;
      lex->text_string_is_7bit= (lip->tok_bitmap & 0x80) ? 0 : 1;
      return(NCHAR_STRING);

    case MY_LEX_IDENT_OR_HEX:
      if (lip->yyPeek() == '\'')
      {					// Found x'hex-number'
	state= MY_LEX_HEX_NUMBER;
	break;
      }
    case MY_LEX_IDENT_OR_BIN:
      if (lip->yyPeek() == '\'')
      {                                 // Found b'bin-number'
        state= MY_LEX_BIN_NUMBER;
        break;
      }
    case MY_LEX_IDENT:
      const char *start;
#if defined(USE_MB) && defined(USE_MB_IDENT)
      if (use_mb(cs))
      {
	result_state= IDENT_QUOTED;
        if (my_mbcharlen(cs, lip->yyGetLast()) > 1)
        {
          int l = my_ismbchar(cs,
                              lip->get_ptr() -1,
                              lip->get_end_of_query());
          if (l == 0) {
            state = MY_LEX_CHAR;
            continue;
          }
          lip->skip_binary(l - 1);
        }
        while (ident_map[c=lip->yyGet()])
        {
          if (my_mbcharlen(cs, c) > 1)
          {
            int l;
            if ((l = my_ismbchar(cs,
                                 lip->get_ptr() -1,
                                 lip->get_end_of_query())) == 0)
              break;
            lip->skip_binary(l-1);
          }
        }
      }
      else
#endif
      {
        for (result_state= c;
             ident_map[(uchar) (c= lip->yyGet())];
             result_state|= c)
          ;
        /* If there were non-ASCII characters, mark that we must convert */
        result_state= result_state & 0x80 ? IDENT_QUOTED : IDENT;
      }
      length= lip->yyLength();
      start= lip->get_ptr();
      if (lip->ignore_space)
      {
        /*
          If we find a space then this can't be an identifier. We notice this
          below by checking start != lex->ptr.
        */
        for (; state_map[(uchar) c] == MY_LEX_SKIP ; c= lip->yyGet())
          ;
      }
      if (start == lip->get_ptr() && c == '.' &&
          ident_map[(uchar) lip->yyPeek()])
	lip->next_state=MY_LEX_IDENT_SEP;
      else
      {					// '(' must follow directly if function
        lip->yyUnget();
	if ((tokval = find_keyword(lip, length, c == '(')))
	{
	  lip->next_state= MY_LEX_START;	// Allow signed numbers
	  return(tokval);		// Was keyword
	}
        lip->yySkip();                  // next state does a unget
      }
      yylval->lex_str=get_token(lip, 0, length);

      /*
         Note: "SELECT _bla AS 'alias'"
         _bla should be considered as a IDENT if charset haven't been found.
         So we don't use MYF(MY_WME) with get_charset_by_csname to avoid
         producing an error.
      */

      if (yylval->lex_str.str[0] == '_')
      {
        CHARSET_INFO *cs= get_charset_by_csname(yylval->lex_str.str + 1,
                                                MY_CS_PRIMARY, MYF(0));
        if (cs)
        {
          yylval->charset= cs;
          lip->m_underscore_cs= cs;

          lip->body_utf8_append(lip->m_cpp_text_start,
                                lip->get_cpp_tok_start() + length);
          return(UNDERSCORE_CHARSET);
        }
      }

      lip->body_utf8_append(lip->m_cpp_text_start);

      lip->body_utf8_append_literal(thd, &yylval->lex_str, cs,
                                    lip->m_cpp_text_end);

      return(result_state);			// IDENT or IDENT_QUOTED

    case MY_LEX_IDENT_SEP:                  // Found ident and now '.'
      yylval->lex_str.str= (char*) lip->get_ptr();
      yylval->lex_str.length= 1;
      c= lip->yyGet();                          // should be '.'
      lip->next_state= MY_LEX_IDENT_START;      // Next is ident (not keyword)
      if (!ident_map[(uchar) lip->yyPeek()])    // Probably ` or "
	lip->next_state= MY_LEX_START;
      return((int) c);

    case MY_LEX_NUMBER_IDENT:		// number or ident which num-start
      if (lip->yyGetLast() == '0')
      {
        c= lip->yyGet();
        if (c == 'x')
        {
          while (my_isxdigit(cs,(c = lip->yyGet()))) ;
          if ((lip->yyLength() >= 3) && !ident_map[c])
          {
            /* skip '0x' */
            yylval->lex_str=get_token(lip, 2, lip->yyLength()-2);
            return (HEX_NUM);
          }
          lip->yyUnget();
          state= MY_LEX_IDENT_START;
          break;
        }
        else if (c == 'b')
        {
          while ((c= lip->yyGet()) == '0' || c == '1')
            ;
          if ((lip->yyLength() >= 3) && !ident_map[c])
          {
            /* Skip '0b' */
            yylval->lex_str= get_token(lip, 2, lip->yyLength()-2);
            return (BIN_NUM);
          }
          lip->yyUnget();
          state= MY_LEX_IDENT_START;
          break;
        }
        lip->yyUnget();
      }

      while (my_isdigit(cs, (c = lip->yyGet()))) ;
      if (!ident_map[c])
      {					// Can't be identifier
	state=MY_LEX_INT_OR_REAL;
	break;
      }
      if (c == 'e' || c == 'E')
      {
	// The following test is written this way to allow numbers of type 1e1
        if (my_isdigit(cs,lip->yyPeek()) ||
            (c=(lip->yyGet())) == '+' || c == '-')
	{				// Allow 1E+10
          if (my_isdigit(cs,lip->yyPeek()))     // Number must have digit after sign
	  {
            lip->yySkip();
            while (my_isdigit(cs,lip->yyGet())) ;
            yylval->lex_str=get_token(lip, 0, lip->yyLength());
	    return(FLOAT_NUM);
	  }
	}
        lip->yyUnget();
      }
      // fall through
    case MY_LEX_IDENT_START:			// We come here after '.'
      result_state= IDENT;
#if defined(USE_MB) && defined(USE_MB_IDENT)
      if (use_mb(cs))
      {
	result_state= IDENT_QUOTED;
        while (ident_map[c=lip->yyGet()])
        {
          if (my_mbcharlen(cs, c) > 1)
          {
            int l;
            if ((l = my_ismbchar(cs,
                                 lip->get_ptr() -1,
                                 lip->get_end_of_query())) == 0)
              break;
            lip->skip_binary(l-1);
          }
        }
      }
      else
#endif
      {
        for (result_state=0; ident_map[c= lip->yyGet()]; result_state|= c)
          ;
        /* If there were non-ASCII characters, mark that we must convert */
        result_state= result_state & 0x80 ? IDENT_QUOTED : IDENT;
      }
      if (c == '.' && ident_map[(uchar) lip->yyPeek()])
	lip->next_state=MY_LEX_IDENT_SEP;// Next is '.'

      yylval->lex_str= get_token(lip, 0, lip->yyLength());

      lip->body_utf8_append(lip->m_cpp_text_start);

      lip->body_utf8_append_literal(thd, &yylval->lex_str, cs,
                                    lip->m_cpp_text_end);

      return(result_state);

    case MY_LEX_USER_VARIABLE_DELIMITER:	// Found quote char
    {
      uint double_quotes= 0;
      char quote_char= c;                       // Used char
      while ((c=lip->yyGet()))
      {
	int var_length;
	if ((var_length= my_mbcharlen(cs, c)) == 1)
	{
	  if (c == quote_char)
	  {
            if (lip->yyPeek() != quote_char)
	      break;
            c=lip->yyGet();
	    double_quotes++;
	    continue;
	  }
	}
#ifdef USE_MB
        else if (use_mb(cs))
        {
          if ((var_length= my_ismbchar(cs, lip->get_ptr() - 1,
                                       lip->get_end_of_query())))
            lip->skip_binary(var_length-1);
        }
#endif
      }
      if (double_quotes)
	yylval->lex_str=get_quoted_token(lip, 1,
                                         lip->yyLength() - double_quotes -1,
					 quote_char);
      else
        yylval->lex_str=get_token(lip, 1, lip->yyLength() -1);
      if (c == quote_char)
        lip->yySkip();                  // Skip end `
      lip->next_state= MY_LEX_START;

      lip->body_utf8_append(lip->m_cpp_text_start);

      lip->body_utf8_append_literal(thd, &yylval->lex_str, cs,
                                    lip->m_cpp_text_end);

      return(IDENT_QUOTED);
    }
    case MY_LEX_INT_OR_REAL:		// Complete int or incomplete real
      if (c != '.')
      {					// Found complete integer number.
        yylval->lex_str=get_token(lip, 0, lip->yyLength());
	return int_token(yylval->lex_str.str, (uint) yylval->lex_str.length);
      }
      // fall through
    case MY_LEX_REAL:			// Incomplete real number
      while (my_isdigit(cs,c = lip->yyGet())) ;

      if (c == 'e' || c == 'E')
      {
        c = lip->yyGet();
	if (c == '-' || c == '+')
          c = lip->yyGet();                     // Skip sign
	if (!my_isdigit(cs,c))
	{				// No digit after sign
	  state= MY_LEX_CHAR;
	  break;
	}
        while (my_isdigit(cs,lip->yyGet())) ;
        yylval->lex_str=get_token(lip, 0, lip->yyLength());
	return(FLOAT_NUM);
      }
      yylval->lex_str=get_token(lip, 0, lip->yyLength());
      return(DECIMAL_NUM);

    case MY_LEX_HEX_NUMBER:		// Found x'hexstring'
      lip->yySkip();                    // Accept opening '
      while (my_isxdigit(cs, (c= lip->yyGet()))) ;
      if (c != '\'')
        return(ABORT_SYM);              // Illegal hex constant
      lip->yySkip();                    // Accept closing '
      length= lip->yyLength();          // Length of hexnum+3
      if ((length % 2) == 0)
        return(ABORT_SYM);              // odd number of hex digits
      yylval->lex_str=get_token(lip,
                                2,          // skip x'
                                length-3);  // don't count x' and last '
      return HEX_STRING;

    case MY_LEX_BIN_NUMBER:           // Found b'bin-string'
      lip->yySkip();                  // Accept opening '
      while ((c= lip->yyGet()) == '0' || c == '1')
        ;
      if (c != '\'')
        return(ABORT_SYM);            // Illegal hex constant
      lip->yySkip();                  // Accept closing '
      length= lip->yyLength();        // Length of bin-num + 3
      yylval->lex_str= get_token(lip,
                                 2,         // skip b'
                                 length-3); // don't count b' and last '
      return (BIN_NUM);

    case MY_LEX_CMP_OP:			// Incomplete comparison operator
      lip->next_state= MY_LEX_START;	// Allow signed numbers
      if (state_map[(uchar) lip->yyPeek()] == MY_LEX_CMP_OP ||
          state_map[(uchar) lip->yyPeek()] == MY_LEX_LONG_CMP_OP)
      {
        lip->yySkip();
        if ((tokval= find_keyword(lip, 2, 0)))
          return(tokval);
        lip->yyUnget();
      }
      return(c);

    case MY_LEX_LONG_CMP_OP:		// Incomplete comparison operator
      lip->next_state= MY_LEX_START;
      if (state_map[(uchar) lip->yyPeek()] == MY_LEX_CMP_OP ||
          state_map[(uchar) lip->yyPeek()] == MY_LEX_LONG_CMP_OP)
      {
        lip->yySkip();
        if (state_map[(uchar) lip->yyPeek()] == MY_LEX_CMP_OP)
        {
          lip->yySkip();
          if ((tokval= find_keyword(lip, 3, 0)))
            return(tokval);
          lip->yyUnget();
        }
        if ((tokval= find_keyword(lip, 2, 0)))
          return(tokval);
        lip->yyUnget();
      }
      return(c);

    case MY_LEX_BOOL:
      if (c != lip->yyPeek())
      {
	state=MY_LEX_CHAR;
	break;
      }
      lip->yySkip();
      tokval = find_keyword(lip,2,0);	// Is a bool operator
      lip->next_state= MY_LEX_START;	// Allow signed numbers
      return(tokval);

    case MY_LEX_STRING_OR_DELIMITER:
      if (thd->variables.sql_mode & MODE_ANSI_QUOTES)
      {
	state= MY_LEX_USER_VARIABLE_DELIMITER;
	break;
      }
      /* " used for strings */
    case MY_LEX_STRING:			// Incomplete text string
      if (!(yylval->lex_str.str = get_text(lip, 1, 1)))
      {
	state= MY_LEX_CHAR;		// Read char by char
	break;
      }
      yylval->lex_str.length=lip->yytoklen;

      lip->body_utf8_append(lip->m_cpp_text_start);

      lip->body_utf8_append_literal(thd, &yylval->lex_str,
        lip->m_underscore_cs ? lip->m_underscore_cs : cs,
        lip->m_cpp_text_end);

      lip->m_underscore_cs= NULL;

      lex->text_string_is_7bit= (lip->tok_bitmap & 0x80) ? 0 : 1;
      return(TEXT_STRING);

    case MY_LEX_COMMENT:			//  Comment
      lex->select_lex.options|= OPTION_FOUND_COMMENT;
      while ((c = lip->yyGet()) != '\n' && c) ;
      lip->yyUnget();                   // Safety against eof
      state = MY_LEX_START;		// Try again
      break;
    case MY_LEX_LONG_COMMENT:		/* Long C comment? */
      if (lip->yyPeek() != '*')
      {
	state=MY_LEX_CHAR;		// Probable division
	break;
      }
      lex->select_lex.options|= OPTION_FOUND_COMMENT;
      /* Reject '/' '*', since we might need to turn off the echo */
      lip->yyUnget();

      lip->save_in_comment_state();

      if (lip->yyPeekn(2) == '!' ||
          (lip->yyPeekn(2) == 'M' && lip->yyPeekn(3) == '!'))
      {
        bool maria_comment_syntax= lip->yyPeekn(2) == 'M';
        lip->in_comment= DISCARD_COMMENT;
        /* Accept '/' '*' '!', but do not keep this marker. */
        lip->set_echo(FALSE);
        lip->yySkipn(maria_comment_syntax ? 4 : 3);

        /*
          The special comment format is very strict:
          '/' '*' '!', followed by an optional 'M' and exactly
          1-2 digits (major), 2 digits (minor), then 2 digits (dot).
          32302  -> 3.23.02
          50032  -> 5.0.32
          50114  -> 5.1.14
          100000 -> 10.0.0
        */
        if (  my_isdigit(cs, lip->yyPeekn(0))
           && my_isdigit(cs, lip->yyPeekn(1))
           && my_isdigit(cs, lip->yyPeekn(2))
           && my_isdigit(cs, lip->yyPeekn(3))
           && my_isdigit(cs, lip->yyPeekn(4))
           )
        {
          ulong version;
          uint length= 5;
          char *end_ptr= (char*) lip->get_ptr()+length;
          int error;
          if (my_isdigit(cs, lip->yyPeekn(5)))
          {
            end_ptr++;                          // 6 digit number
            length++;
          }

          version= (ulong) my_strtoll10(lip->get_ptr(), &end_ptr, &error);

          if (version <= MYSQL_VERSION_ID)
          {
            /* Accept 'M' 'm' 'm' 'd' 'd' */
            lip->yySkipn(length);
            /* Expand the content of the special comment as real code */
            lip->set_echo(TRUE);
            state=MY_LEX_START;
            break;  /* Do not treat contents as a comment.  */
          }
          else
          {
            /*
              Patch and skip the conditional comment to avoid it
              being propagated infinitely (eg. to a slave).
            */
            char *pcom= lip->yyUnput(' ');
            comment_closed= ! consume_comment(lip, 1);
            if (! comment_closed)
            {
              *pcom= '!';
            }
            /* version allowed to have one level of comment inside. */
          }
        }
        else
        {
          /* Not a version comment. */
          state=MY_LEX_START;
          lip->set_echo(TRUE);
          break;
        }
      }
      else
      {
        lip->in_comment= PRESERVE_COMMENT;
        lip->yySkip();                  // Accept /
        lip->yySkip();                  // Accept *
        comment_closed= ! consume_comment(lip, 0);
        /* regular comments can have zero comments inside. */
      }
      /*
        Discard:
        - regular '/' '*' comments,
        - special comments '/' '*' '!' for a future version,
        by scanning until we find a closing '*' '/' marker.

        Nesting regular comments isn't allowed.  The first 
        '*' '/' returns the parser to the previous state.

        /#!VERSI oned containing /# regular #/ is allowed #/

		Inside one versioned comment, another versioned comment
		is treated as a regular discardable comment.  It gets
		no special parsing.
      */

      /* Unbalanced comments with a missing '*' '/' are a syntax error */
      if (! comment_closed)
        return (ABORT_SYM);
      state = MY_LEX_START;             // Try again
      lip->restore_in_comment_state();
      break;
    case MY_LEX_END_LONG_COMMENT:
      if ((lip->in_comment != NO_COMMENT) && lip->yyPeek() == '/')
      {
        /* Reject '*' '/' */
        lip->yyUnget();
        /* Accept '*' '/', with the proper echo */
        lip->set_echo(lip->in_comment == PRESERVE_COMMENT);
        lip->yySkipn(2);
        /* And start recording the tokens again */
        lip->set_echo(TRUE);
        lip->in_comment=NO_COMMENT;
        state=MY_LEX_START;
      }
      else
	state=MY_LEX_CHAR;		// Return '*'
      break;
    case MY_LEX_SET_VAR:		// Check if ':='
      if (lip->yyPeek() != '=')
      {
	state=MY_LEX_CHAR;		// Return ':'
	break;
      }
      lip->yySkip();
      return (SET_VAR);
    case MY_LEX_SEMICOLON:			// optional line terminator
      state= MY_LEX_CHAR;               // Return ';'
      break;
    case MY_LEX_EOL:
      if (lip->eof())
      {
        lip->yyUnget();                 // Reject the last '\0'
        lip->set_echo(FALSE);
        lip->yySkip();
        lip->set_echo(TRUE);
        /* Unbalanced comments with a missing '*' '/' are a syntax error */
        if (lip->in_comment != NO_COMMENT)
          return (ABORT_SYM);
        lip->next_state=MY_LEX_END;     // Mark for next loop
        return(END_OF_INPUT);
      }
      state=MY_LEX_CHAR;
      break;
    case MY_LEX_END:
      lip->next_state=MY_LEX_END;
      return(0);			// We found end of input last time

      /* Actually real shouldn't start with . but allow them anyhow */
    case MY_LEX_REAL_OR_POINT:
      if (my_isdigit(cs,lip->yyPeek()))
	state = MY_LEX_REAL;		// Real
      else
      {
	state= MY_LEX_IDENT_SEP;	// return '.'
        lip->yyUnget();                 // Put back '.'
      }
      break;
    case MY_LEX_USER_END:		// end '@' of user@hostname
      switch (state_map[(uchar) lip->yyPeek()]) {
      case MY_LEX_STRING:
      case MY_LEX_USER_VARIABLE_DELIMITER:
      case MY_LEX_STRING_OR_DELIMITER:
	break;
      case MY_LEX_USER_END:
	lip->next_state=MY_LEX_SYSTEM_VAR;
	break;
      default:
	lip->next_state=MY_LEX_HOSTNAME;
	break;
      }
      yylval->lex_str.str=(char*) lip->get_ptr();
      yylval->lex_str.length=1;
      return((int) '@');
    case MY_LEX_HOSTNAME:		// end '@' of user@hostname
      for (c=lip->yyGet() ;
	   my_isalnum(cs,c) || c == '.' || c == '_' ||  c == '$';
           c= lip->yyGet()) ;
      yylval->lex_str=get_token(lip, 0, lip->yyLength());
      return(LEX_HOSTNAME);
    case MY_LEX_SYSTEM_VAR:
      yylval->lex_str.str=(char*) lip->get_ptr();
      yylval->lex_str.length=1;
      lip->yySkip();                                    // Skip '@'
      lip->next_state= (state_map[(uchar) lip->yyPeek()] ==
			MY_LEX_USER_VARIABLE_DELIMITER ?
			MY_LEX_OPERATOR_OR_IDENT :
			MY_LEX_IDENT_OR_KEYWORD);
      return((int) '@');
    case MY_LEX_IDENT_OR_KEYWORD:
      /*
	We come here when we have found two '@' in a row.
	We should now be able to handle:
	[(global | local | session) .]variable_name
      */

      for (result_state= 0; ident_map[c= lip->yyGet()]; result_state|= c)
        ;
      /* If there were non-ASCII characters, mark that we must convert */
      result_state= result_state & 0x80 ? IDENT_QUOTED : IDENT;

      if (c == '.')
	lip->next_state=MY_LEX_IDENT_SEP;
      length= lip->yyLength();
      if (length == 0)
        return(ABORT_SYM);              // Names must be nonempty.
      if ((tokval= find_keyword(lip, length,0)))
      {
        lip->yyUnget();                         // Put back 'c'
	return(tokval);				// Was keyword
      }
      yylval->lex_str=get_token(lip, 0, length);

      lip->body_utf8_append(lip->m_cpp_text_start);

      lip->body_utf8_append_literal(thd, &yylval->lex_str, cs,
                                    lip->m_cpp_text_end);

      return(result_state);
    }
  }
}


/**
  Construct a copy of this object to be used for mysql_alter_table
  and mysql_create_table.

  Historically, these two functions modify their Alter_info
  arguments. This behaviour breaks re-execution of prepared
  statements and stored procedures and is compensated by always
  supplying a copy of Alter_info to these functions.

  @return You need to use check the error in THD for out
  of memory condition after calling this function.
*/

Alter_info::Alter_info(const Alter_info &rhs, MEM_ROOT *mem_root)
  :drop_list(rhs.drop_list, mem_root),
  alter_list(rhs.alter_list, mem_root),
  key_list(rhs.key_list, mem_root),
  create_list(rhs.create_list, mem_root),
  flags(rhs.flags),
  keys_onoff(rhs.keys_onoff),
  tablespace_op(rhs.tablespace_op),
  partition_names(rhs.partition_names, mem_root),
  num_parts(rhs.num_parts),
  change_level(rhs.change_level),
  datetime_field(rhs.datetime_field),
  error_if_not_empty(rhs.error_if_not_empty)
{
  /*
    Make deep copies of used objects.
    This is not a fully deep copy - clone() implementations
    of Alter_drop, Alter_column, Key, foreign_key, Key_part_spec
    do not copy string constants. At the same length the only
    reason we make a copy currently is that ALTER/CREATE TABLE
    code changes input Alter_info definitions, but string
    constants never change.
  */
  list_copy_and_replace_each_value(drop_list, mem_root);
  list_copy_and_replace_each_value(alter_list, mem_root);
  list_copy_and_replace_each_value(key_list, mem_root);
  list_copy_and_replace_each_value(create_list, mem_root);
  /* partition_names are not deeply copied currently */
}


void trim_whitespace(CHARSET_INFO *cs, LEX_STRING *str)
{
  /*
    TODO:
    This code assumes that there are no multi-bytes characters
    that can be considered white-space.
  */

  while ((str->length > 0) && (my_isspace(cs, str->str[0])))
  {
    str->length --;
    str->str ++;
  }

  /*
    FIXME:
    Also, parsing backward is not safe with multi bytes characters
  */
  while ((str->length > 0) && (my_isspace(cs, str->str[str->length-1])))
  {
    str->length --;
  }
}


/*
  st_select_lex structures initialisations
*/

void st_select_lex_node::init_query()
{
  options= 0;
  sql_cache= SQL_CACHE_UNSPECIFIED;
  linkage= UNSPECIFIED_TYPE;
  no_table_names_allowed= 0;
  uncacheable= 0;
}

void st_select_lex_node::init_select()
{
}

void st_select_lex_unit::init_query()
{
  st_select_lex_node::init_query();
  linkage= GLOBAL_OPTIONS_TYPE;
  global_parameters= first_select();
  select_limit_cnt= HA_POS_ERROR;
  offset_limit_cnt= 0;
  union_distinct= 0;
  prepared= optimized= executed= 0;
  item= 0;
  union_result= 0;
  table= 0;
  fake_select_lex= 0;
  cleaned= 0;
  item_list.empty();
  describe= 0;
  found_rows_for_union= 0;
  insert_table_with_stored_vcol= 0;
  derived= 0;
}

void st_select_lex::init_query()
{
  st_select_lex_node::init_query();
  table_list.empty();
  top_join_list.empty();
  join_list= &top_join_list;
  embedding= 0;
  leaf_tables_prep.empty();
  leaf_tables.empty();
  item_list.empty();
  join= 0;
  having= prep_having= where= prep_where= 0;
  olap= UNSPECIFIED_OLAP_TYPE;
  having_fix_field= 0;
  context.select_lex= this;
  context.init();
  /*
    Add the name resolution context of the current (sub)query to the
    stack of contexts for the whole query.
    TODO:
    push_context may return an error if there is no memory for a new
    element in the stack, however this method has no return value,
    thus push_context should be moved to a place where query
    initialization is checked for failure.
  */
  parent_lex->push_context(&context);
  cond_count= between_count= with_wild= 0;
  max_equal_elems= 0;
  ref_pointer_array= 0;
  ref_pointer_array_size= 0;
  select_n_where_fields= 0;
  select_n_having_items= 0;
  n_sum_items= 0;
  n_child_sum_items= 0;
  subquery_in_having= explicit_limit= 0;
  is_item_list_lookup= 0;
  first_execution= 1;
  first_natural_join_processing= 1;
  first_cond_optimization= 1;
  parsing_place= NO_MATTER;
  exclude_from_table_unique_test= no_wrap_view_item= FALSE;
  nest_level= 0;
  link_next= 0;
  prep_leaf_list_state= UNINIT;
  bzero((char*) expr_cache_may_be_used, sizeof(expr_cache_may_be_used));
  m_non_agg_field_used= false;
  m_agg_func_used= false;
}

void st_select_lex::init_select()
{
  st_select_lex_node::init_select();
  sj_nests.empty();
  sj_subselects.empty();
  group_list.empty();
  if (group_list_ptrs)
    group_list_ptrs->clear();
  type= db= 0;
  having= 0;
  table_join_options= 0;
  in_sum_expr= with_wild= 0;
  options= 0;
  sql_cache= SQL_CACHE_UNSPECIFIED;
  braces= 0;
  interval_list.empty();
  ftfunc_list_alloc.empty();
  inner_sum_func_list= 0;
  ftfunc_list= &ftfunc_list_alloc;
  linkage= UNSPECIFIED_TYPE;
  order_list.elements= 0;
  order_list.first= 0;
  order_list.next= &order_list.first;
  /* Set limit and offset to default values */
  select_limit= 0;      /* denotes the default limit = HA_POS_ERROR */
  offset_limit= 0;      /* denotes the default offset = 0 */
  with_sum_func= 0;
  is_correlated= 0;
  cur_pos_in_select_list= UNDEF_POS;
  cond_value= having_value= Item::COND_UNDEF;
  inner_refs_list.empty();
  insert_tables= 0;
  merged_into= 0;
  m_non_agg_field_used= false;
  m_agg_func_used= false;
  name_visibility_map= 0;
  join= 0;
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
  slave= 0;
}


void st_select_lex_node::add_slave(st_select_lex_node *slave_arg)
{
  for (; slave; slave= slave->next)
    if (slave == slave_arg)
      return;

  if (slave)
  {
    st_select_lex_node *slave_arg_slave= slave_arg->slave;
    /* Insert in the front of list of slaves if any. */
    slave_arg->include_neighbour(slave);
    /* include_neighbour() sets slave_arg->slave=0, restore it. */
    slave_arg->slave= slave_arg_slave;
    /* Count on include_neighbour() setting the master. */
    DBUG_ASSERT(slave_arg->master == this);
  }
  else
  {
    slave= slave_arg;
    slave_arg->master= this;
  }
}


/*
  include on level down (but do not link)

  SYNOPSYS
    st_select_lex_node::include_standalone()
    upper - reference on node underr which this node should be included
    ref - references on reference on this node
*/
void st_select_lex_node::include_standalone(st_select_lex_node *upper,
					    st_select_lex_node **ref)
{
  next= 0;
  prev= ref;
  master= upper;
  slave= 0;
}

/* include neighbour (on same level) */
void st_select_lex_node::include_neighbour(st_select_lex_node *before)
{
  if ((next= before->next))
    next->prev= &next;
  prev= &before->next;
  before->next= this;
  master= before->master;
  slave= 0;
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
  if (link_prev)
  {
    if ((*link_prev= link_next))
      link_next->link_prev= link_prev;
  }
  // Remove slave structure
  for (; slave; slave= slave->next)
    slave->fast_exclude();
  
}


/*
  Exclude a node from the tree lex structure, but leave it in the global
  list of nodes.
*/

void st_select_lex_node::exclude_from_tree()
{
  if ((*prev= next))
    next->prev= prev;
}


/*
  Exclude select_lex structure (except first (first select can't be
  deleted, because it is most upper select))
*/
void st_select_lex_node::exclude()
{
  /* exclude from global list */
  fast_exclude();
  /* exclude from other structures */
  exclude_from_tree();
  /* 
     We do not need following statements, because prev pointer of first 
     list element point to master->slave
     if (master->slave == this)
       master->slave= next;
  */
}


/*
  Exclude level of current unit from tree of SELECTs

  SYNOPSYS
    st_select_lex_unit::exclude_level()

  NOTE: units which belong to current will be brought up on level of
  currernt unit 
*/
void st_select_lex_unit::exclude_level()
{
  SELECT_LEX_UNIT *units= 0, **units_last= &units;
  for (SELECT_LEX *sl= first_select(); sl; sl= sl->next_select())
  {
    // unlink current level from global SELECTs list
    if (sl->link_prev && (*sl->link_prev= sl->link_next))
      sl->link_next->link_prev= sl->link_prev;

    // bring up underlay levels
    SELECT_LEX_UNIT **last= 0;
    for (SELECT_LEX_UNIT *u= sl->first_inner_unit(); u; u= u->next_unit())
    {
      u->master= master;
      last= (SELECT_LEX_UNIT**)&(u->next);
    }
    if (last)
    {
      (*units_last)= sl->first_inner_unit();
      units_last= last;
    }
  }
  if (units)
  {
    // include brought up levels in place of current
    (*prev)= units;
    (*units_last)= (SELECT_LEX_UNIT*)next;
    if (next)
      next->prev= (SELECT_LEX_NODE**)units_last;
    units->prev= prev;
  }
  else
  {
    // exclude currect unit from list of nodes
    (*prev)= next;
    if (next)
      next->prev= prev;
  }
}


/*
  Exclude subtree of current unit from tree of SELECTs

  SYNOPSYS
    st_select_lex_unit::exclude_tree()
*/
void st_select_lex_unit::exclude_tree()
{
  for (SELECT_LEX *sl= first_select(); sl; sl= sl->next_select())
  {
    // unlink current level from global SELECTs list
    if (sl->link_prev && (*sl->link_prev= sl->link_next))
      sl->link_next->link_prev= sl->link_prev;

    // unlink underlay levels
    for (SELECT_LEX_UNIT *u= sl->first_inner_unit(); u; u= u->next_unit())
    {
      u->exclude_level();
    }
  }
  // exclude currect unit from list of nodes
  (*prev)= next;
  if (next)
    next->prev= prev;
}


/*
  st_select_lex_node::mark_as_dependent mark all st_select_lex struct from 
  this to 'last' as dependent

  SYNOPSIS
    last - pointer to last st_select_lex struct, before which all 
           st_select_lex have to be marked as dependent

  NOTE
    'last' should be reachable from this st_select_lex_node
*/

bool st_select_lex::mark_as_dependent(THD *thd, st_select_lex *last,
                                      Item *dependency)
{

  DBUG_ASSERT(this != last);

  /*
    Mark all selects from resolved to 1 before select where was
    found table as depended (of select where was found table)
  */
  SELECT_LEX *s= this;
  do
  {
    if (!(s->uncacheable & UNCACHEABLE_DEPENDENT_GENERATED))
    {
      // Select is dependent of outer select
      s->uncacheable= (s->uncacheable & ~UNCACHEABLE_UNITED) |
                       UNCACHEABLE_DEPENDENT_GENERATED;
      SELECT_LEX_UNIT *munit= s->master_unit();
      munit->uncacheable= (munit->uncacheable & ~UNCACHEABLE_UNITED) |
                       UNCACHEABLE_DEPENDENT_GENERATED;
      for (SELECT_LEX *sl= munit->first_select(); sl ; sl= sl->next_select())
      {
        if (sl != s &&
            !(sl->uncacheable & (UNCACHEABLE_DEPENDENT_GENERATED |
                                 UNCACHEABLE_UNITED)))
          sl->uncacheable|= UNCACHEABLE_UNITED;
      }
    }

    Item_subselect *subquery_expr= s->master_unit()->item;
    if (subquery_expr && subquery_expr->mark_as_dependent(thd, last, 
                                                          dependency))
      return TRUE;
  } while ((s= s->outer_select()) != last && s != 0);
  is_correlated= TRUE;
  this->master_unit()->item->is_correlated= TRUE;
  return FALSE;
}

bool st_select_lex_node::set_braces(bool value)      { return 1; }
bool st_select_lex_node::inc_in_sum_expr()           { return 1; }
uint st_select_lex_node::get_in_sum_expr()           { return 0; }
TABLE_LIST* st_select_lex_node::get_table_list()     { return 0; }
List<Item>* st_select_lex_node::get_item_list()      { return 0; }
TABLE_LIST *st_select_lex_node::add_table_to_list (THD *thd, Table_ident *table,
						  LEX_STRING *alias,
						  ulong table_join_options,
						  thr_lock_type flags,
                                                  enum_mdl_type mdl_type,
						  List<Index_hint> *hints,
                                                  LEX_STRING *option)
{
  return 0;
}
ulong st_select_lex_node::get_table_join_options()
{
  return 0;
}

/*
  prohibit using LIMIT clause
*/
bool st_select_lex::test_limit()
{
  if (select_limit != 0)
  {
    my_error(ER_NOT_SUPPORTED_YET, MYF(0),
             "LIMIT & IN/ALL/ANY/SOME subquery");
    return(1);
  }
  return(0);
}


st_select_lex_unit* st_select_lex_unit::master_unit()
{
    return this;
}


st_select_lex* st_select_lex_unit::outer_select()
{
  return (st_select_lex*) master;
}


bool st_select_lex::add_order_to_list(THD *thd, Item *item, bool asc)
{
  return add_to_list(thd, order_list, item, asc);
}


bool st_select_lex::add_gorder_to_list(THD *thd, Item *item, bool asc)
{
  return add_to_list(thd, gorder_list, item, asc);
}

bool st_select_lex::add_item_to_list(THD *thd, Item *item)
{
  DBUG_ENTER("st_select_lex::add_item_to_list");
  DBUG_PRINT("info", ("Item: 0x%lx", (long) item));
  DBUG_RETURN(item_list.push_back(item));
}


bool st_select_lex::add_group_to_list(THD *thd, Item *item, bool asc)
{
  return add_to_list(thd, group_list, item, asc);
}


bool st_select_lex::add_ftfunc_to_list(Item_func_match *func)
{
  return !func || ftfunc_list->push_back(func); // end of memory?
}


st_select_lex_unit* st_select_lex::master_unit()
{
  return (st_select_lex_unit*) master;
}


st_select_lex* st_select_lex::outer_select()
{
  return (st_select_lex*) master->get_master();
}


bool st_select_lex::set_braces(bool value)
{
  braces= value;
  return 0; 
}


bool st_select_lex::inc_in_sum_expr()
{
  in_sum_expr++;
  return 0;
}


uint st_select_lex::get_in_sum_expr()
{
  return in_sum_expr;
}


TABLE_LIST* st_select_lex::get_table_list()
{
  return table_list.first;
}

List<Item>* st_select_lex::get_item_list()
{
  return &item_list;
}

ulong st_select_lex::get_table_join_options()
{
  return table_join_options;
}


bool st_select_lex::setup_ref_array(THD *thd, uint order_group_num)
{
  // find_order_in_list() may need some extra space, so multiply by two.
  order_group_num*= 2;

  /*
    We have to create array in prepared statement memory if it is a
    prepared statement
  */
  Query_arena *arena= thd->stmt_arena;
  const uint n_elems= (n_sum_items +
                       n_child_sum_items +
                       item_list.elements +
                       select_n_having_items +
                       select_n_where_fields +
                       order_group_num) * 5;
  if (ref_pointer_array != NULL)
  {
    /*
      We need to take 'n_sum_items' into account when allocating the array,
      and this may actually increase during the optimization phase due to
      MIN/MAX rewrite in Item_in_subselect::single_value_transformer.
      In the usual case we can reuse the array from the prepare phase.
      If we need a bigger array, we must allocate a new one.
    */
    if (ref_pointer_array_size >= n_elems)
    {
      DBUG_PRINT("info", ("reusing old ref_array"));
      return false;
    }
  }
  ref_pointer_array= static_cast<Item**>(arena->alloc(sizeof(Item*) * n_elems));
  if (ref_pointer_array != NULL)
    ref_pointer_array_size= n_elems;

  return ref_pointer_array == NULL;
}


void st_select_lex_unit::print(String *str, enum_query_type query_type)
{
  bool union_all= !union_distinct;
  for (SELECT_LEX *sl= first_select(); sl; sl= sl->next_select())
  {
    if (sl != first_select())
    {
      str->append(STRING_WITH_LEN(" union "));
      if (union_all)
	str->append(STRING_WITH_LEN("all "));
      else if (union_distinct == sl)
        union_all= TRUE;
    }
    if (sl->braces)
      str->append('(');
    sl->print(thd, str, query_type);
    if (sl->braces)
      str->append(')');
  }
  if (fake_select_lex == global_parameters)
  {
    if (fake_select_lex->order_list.elements)
    {
      str->append(STRING_WITH_LEN(" order by "));
      fake_select_lex->print_order(str,
        fake_select_lex->order_list.first,
        query_type);
    }
    fake_select_lex->print_limit(thd, str, query_type);
  }
}


void st_select_lex::print_order(String *str,
                                ORDER *order,
                                enum_query_type query_type)
{
  for (; order; order= order->next)
  {
    if (order->counter_used)
    {
      if (query_type != QT_VIEW_INTERNAL)
      {
        char buffer[20];
        size_t length= my_snprintf(buffer, 20, "%d", order->counter);
        str->append(buffer, (uint) length);
      }
      else
      {
        /* replace numeric reference with expression */
        if (order->item[0]->type() == Item::INT_ITEM &&
            order->item[0]->basic_const_item())
        {
          char buffer[20];
          size_t length= my_snprintf(buffer, 20, "%d", order->counter);
          str->append(buffer, (uint) length);
          /* make it expression instead of integer constant */
          str->append(STRING_WITH_LEN("+0"));
        }
        else
          (*order->item)->print(str, query_type);
      }
    }
    else
      (*order->item)->print(str, query_type);
    if (!order->asc)
      str->append(STRING_WITH_LEN(" desc"));
    if (order->next)
      str->append(',');
  }
}
 

void st_select_lex::print_limit(THD *thd,
                                String *str,
                                enum_query_type query_type)
{
  SELECT_LEX_UNIT *unit= master_unit();
  Item_subselect *item= unit->item;

  if (item && unit->global_parameters == this)
  {
    Item_subselect::subs_type subs_type= item->substype();
    if (subs_type == Item_subselect::EXISTS_SUBS ||
        subs_type == Item_subselect::IN_SUBS ||
        subs_type == Item_subselect::ALL_SUBS)
    {
      return;
    }
  }
  if (explicit_limit)
  {
    str->append(STRING_WITH_LEN(" limit "));
    if (offset_limit)
    {
      offset_limit->print(str, query_type);
      str->append(',');
    }
    select_limit->print(str, query_type);
  }
}


/**
  @brief Restore the LEX and THD in case of a parse error.

  This is a clean up call that is invoked by the Bison generated
  parser before returning an error from MYSQLparse. If your
  semantic actions manipulate with the global thread state (which
  is a very bad practice and should not normally be employed) and
  need a clean-up in case of error, and you can not use %destructor
  rule in the grammar file itself, this function should be used
  to implement the clean up.
*/

void LEX::cleanup_lex_after_parse_error(THD *thd)
{
  /*
    Delete sphead for the side effect of restoring of the original
    LEX state, thd->lex, thd->mem_root and thd->free_list if they
    were replaced when parsing stored procedure statements.  We
    will never use sphead object after a parse error, so it's okay
    to delete it only for the sake of the side effect.
    TODO: make this functionality explicit in sp_head class.
    Sic: we must nullify the member of the main lex, not the
    current one that will be thrown away
  */
  if (thd->lex->sphead)
  {
    thd->lex->sphead->restore_thd_mem_root(thd);
    delete thd->lex->sphead;
    thd->lex->sphead= NULL;
  }
}

/*
  Initialize (or reset) Query_tables_list object.

  SYNOPSIS
    reset_query_tables_list()
      init  TRUE  - we should perform full initialization of object with
                    allocating needed memory
            FALSE - object is already initialized so we should only reset
                    its state so it can be used for parsing/processing
                    of new statement

  DESCRIPTION
    This method initializes Query_tables_list so it can be used as part
    of LEX object for parsing/processing of statement. One can also use
    this method to reset state of already initialized Query_tables_list
    so it can be used for processing of new statement.
*/

void Query_tables_list::reset_query_tables_list(bool init)
{
  sql_command= SQLCOM_END;
  if (!init && query_tables)
  {
    TABLE_LIST *table= query_tables;
    for (;;)
    {
      delete table->view;
      if (query_tables_last == &table->next_global ||
          !(table= table->next_global))
        break;
    }
  }
  query_tables= 0;
  query_tables_last= &query_tables;
  query_tables_own_last= 0;
  if (init)
  {
    /*
      We delay real initialization of hash (and therefore related
      memory allocation) until first insertion into this hash.
    */
    my_hash_clear(&sroutines);
  }
  else if (sroutines.records)
  {
    /* Non-zero sroutines.records means that hash was initialized. */
    my_hash_reset(&sroutines);
  }
  sroutines_list.empty();
  sroutines_list_own_last= sroutines_list.next;
  sroutines_list_own_elements= 0;
  binlog_stmt_flags= 0;
  stmt_accessed_table_flag= 0;
}


/*
  Destroy Query_tables_list object with freeing all resources used by it.

  SYNOPSIS
    destroy_query_tables_list()
*/

void Query_tables_list::destroy_query_tables_list()
{
  my_hash_free(&sroutines);
}


/*
  Initialize LEX object.

  SYNOPSIS
    LEX::LEX()

  NOTE
    LEX object initialized with this constructor can be used as part of
    THD object for which one can safely call open_tables(), lock_tables()
    and close_thread_tables() functions. But it is not yet ready for
    statement parsing. On should use lex_start() function to prepare LEX
    for this.
*/

LEX::LEX()
  :result(0), option_type(OPT_DEFAULT), is_lex_started(0),
   limit_rows_examined_cnt(ULONGLONG_MAX)
{

  my_init_dynamic_array2(&plugins, sizeof(plugin_ref),
                         plugins_static_buffer,
                         INITIAL_LEX_PLUGIN_LIST_SIZE, 
                         INITIAL_LEX_PLUGIN_LIST_SIZE);
  reset_query_tables_list(TRUE);
  mi.init();
}


/*
  Check whether the merging algorithm can be used on this VIEW

  SYNOPSIS
    LEX::can_be_merged()

  DESCRIPTION
    We can apply merge algorithm if it is single SELECT view  with
    subqueries only in WHERE clause (we do not count SELECTs of underlying
    views, and second level subqueries) and we have not grpouping, ordering,
    HAVING clause, aggregate functions, DISTINCT clause, LIMIT clause and
    several underlying tables.

  RETURN
    FALSE - only temporary table algorithm can be used
    TRUE  - merge algorithm can be used
*/

bool LEX::can_be_merged()
{
  // TODO: do not forget implement case when select_lex.table_list.elements==0

  /* find non VIEW subqueries/unions */
  bool selects_allow_merge= (select_lex.next_select() == 0 &&
                             !(select_lex.uncacheable &
                               UNCACHEABLE_RAND));
  if (selects_allow_merge)
  {
    for (SELECT_LEX_UNIT *tmp_unit= select_lex.first_inner_unit();
         tmp_unit;
         tmp_unit= tmp_unit->next_unit())
    {
      if (tmp_unit->first_select()->parent_lex == this &&
          (tmp_unit->item == 0 ||
           (tmp_unit->item->place() != IN_WHERE &&
            tmp_unit->item->place() != IN_ON)))
      {
        selects_allow_merge= 0;
        break;
      }
    }
  }

  return (selects_allow_merge &&
	  select_lex.group_list.elements == 0 &&
	  select_lex.having == 0 &&
          select_lex.with_sum_func == 0 &&
	  select_lex.table_list.elements >= 1 &&
	  !(select_lex.options & SELECT_DISTINCT) &&
          select_lex.select_limit == 0);
}


/*
  check if command can use VIEW with MERGE algorithm (for top VIEWs)

  SYNOPSIS
    LEX::can_use_merged()

  DESCRIPTION
    Only listed here commands can use merge algorithm in top level
    SELECT_LEX (for subqueries will be used merge algorithm if
    LEX::can_not_use_merged() is not TRUE).

  RETURN
    FALSE - command can't use merged VIEWs
    TRUE  - VIEWs with MERGE algorithms can be used
*/

bool LEX::can_use_merged()
{
  switch (sql_command)
  {
  case SQLCOM_SELECT:
  case SQLCOM_CREATE_TABLE:
  case SQLCOM_UPDATE:
  case SQLCOM_UPDATE_MULTI:
  case SQLCOM_DELETE:
  case SQLCOM_DELETE_MULTI:
  case SQLCOM_INSERT:
  case SQLCOM_INSERT_SELECT:
  case SQLCOM_REPLACE:
  case SQLCOM_REPLACE_SELECT:
  case SQLCOM_LOAD:
    return TRUE;
  default:
    return FALSE;
  }
}

/*
  Check if command can't use merged views in any part of command

  SYNOPSIS
    LEX::can_not_use_merged()

  DESCRIPTION
    Temporary table algorithm will be used on all SELECT levels for queries
    listed here (see also LEX::can_use_merged()).

  RETURN
    FALSE - command can't use merged VIEWs
    TRUE  - VIEWs with MERGE algorithms can be used
*/

bool LEX::can_not_use_merged()
{
  switch (sql_command)
  {
  case SQLCOM_CREATE_VIEW:
  case SQLCOM_SHOW_CREATE:
  /*
    SQLCOM_SHOW_FIELDS is necessary to make 
    information schema tables working correctly with views.
    see get_schema_tables_result function
  */
  case SQLCOM_SHOW_FIELDS:
    return TRUE;
  default:
    return FALSE;
  }
}

/*
  Detect that we need only table structure of derived table/view

  SYNOPSIS
    only_view_structure()

  RETURN
    TRUE yes, we need only structure
    FALSE no, we need data
*/

bool LEX::only_view_structure()
{
  switch (sql_command) {
  case SQLCOM_SHOW_CREATE:
  case SQLCOM_SHOW_TABLES:
  case SQLCOM_SHOW_FIELDS:
  case SQLCOM_REVOKE_ALL:
  case SQLCOM_REVOKE:
  case SQLCOM_GRANT:
  case SQLCOM_CREATE_VIEW:
    return TRUE;
  default:
    return FALSE;
  }
}


/*
  Should Items_ident be printed correctly

  SYNOPSIS
    need_correct_ident()

  RETURN
    TRUE yes, we need only structure
    FALSE no, we need data
*/


bool LEX::need_correct_ident()
{
  switch(sql_command)
  {
  case SQLCOM_SHOW_CREATE:
  case SQLCOM_SHOW_TABLES:
  case SQLCOM_CREATE_VIEW:
    return TRUE;
  default:
    return FALSE;
  }
}

/*
  Get effective type of CHECK OPTION for given view

  SYNOPSIS
    get_effective_with_check()
    view    given view

  NOTE
    It have not sense to set CHECK OPTION for SELECT satement or subqueries,
    so we do not.

  RETURN
    VIEW_CHECK_NONE      no need CHECK OPTION
    VIEW_CHECK_LOCAL     CHECK OPTION LOCAL
    VIEW_CHECK_CASCADED  CHECK OPTION CASCADED
*/

uint8 LEX::get_effective_with_check(TABLE_LIST *view)
{
  if (view->select_lex->master_unit() == &unit &&
      which_check_option_applicable())
    return (uint8)view->with_check;
  return VIEW_CHECK_NONE;
}


/**
  This method should be called only during parsing.
  It is aware of compound statements (stored routine bodies)
  and will initialize the destination with the default
  database of the stored routine, rather than the default
  database of the connection it is parsed in.
  E.g. if one has no current database selected, or current database 
  set to 'bar' and then issues:

  CREATE PROCEDURE foo.p1() BEGIN SELECT * FROM t1 END//

  t1 is meant to refer to foo.t1, not to bar.t1.

  This method is needed to support this rule.

  @return TRUE in case of error (parsing should be aborted, FALSE in
  case of success
*/

bool
LEX::copy_db_to(char **p_db, size_t *p_db_length) const
{
  if (sphead)
  {
    DBUG_ASSERT(sphead->m_db.str && sphead->m_db.length);
    /*
      It is safe to assign the string by-pointer, both sphead and
      its statements reside in the same memory root.
    */
    *p_db= sphead->m_db.str;
    if (p_db_length)
      *p_db_length= sphead->m_db.length;
    return FALSE;
  }
  return thd->copy_db_to(p_db, p_db_length);
}

/*
  initialize limit counters

  SYNOPSIS
    st_select_lex_unit::set_limit()
    values	- SELECT_LEX with initial values for counters
*/

void st_select_lex_unit::set_limit(st_select_lex *sl)
{
  ha_rows select_limit_val;
  ulonglong val;

  DBUG_ASSERT(! thd->stmt_arena->is_stmt_prepare());
  if (sl->select_limit)
  {
    Item *item = sl->select_limit;
    /*
      fix_fields() has not been called for sl->select_limit. That's due to the
      historical reasons -- this item could be only of type Item_int, and
      Item_int does not require fix_fields(). Thus, fix_fields() was never
      called for sl->select_limit.

      Some time ago, Item_splocal was also allowed for LIMIT / OFFSET clauses.
      However, the fix_fields() behavior was not updated, which led to a crash
      in some cases.

      There is no single place where to call fix_fields() for LIMIT / OFFSET
      items during the fix-fields-phase. Thus, for the sake of readability,
      it was decided to do it here, on the evaluation phase (which is a
      violation of design, but we chose the lesser of two evils).

      We can call fix_fields() here, because sl->select_limit can be of two
      types only: Item_int and Item_splocal. Item_int::fix_fields() is trivial,
      and Item_splocal::fix_fields() (or rather Item_sp_variable::fix_fields())
      has the following specific:
        1) it does not affect other items;
        2) it does not fail.

      Nevertheless DBUG_ASSERT was added to catch future changes in
      fix_fields() implementation. Also added runtime check against a result
      of fix_fields() in order to handle error condition in non-debug build.
    */
    bool fix_fields_successful= true;
    if (!item->fixed)
    {
      fix_fields_successful= !item->fix_fields(thd, NULL);

      DBUG_ASSERT(fix_fields_successful);
    }
    val= fix_fields_successful ? item->val_uint() : HA_POS_ERROR;
  }
  else
    val= HA_POS_ERROR;

  select_limit_val= (ha_rows)val;
#ifndef BIG_TABLES
  /*
    Check for overflow : ha_rows can be smaller then ulonglong if
    BIG_TABLES is off.
    */
  if (val != (ulonglong)select_limit_val)
    select_limit_val= HA_POS_ERROR;
#endif
  if (sl->offset_limit)
  {
    Item *item = sl->offset_limit;
    // see comment for sl->select_limit branch.
    bool fix_fields_successful= true;
    if (!item->fixed)
    {
      fix_fields_successful= !item->fix_fields(thd, NULL);

      DBUG_ASSERT(fix_fields_successful);
    }
    val= fix_fields_successful ? item->val_uint() : 0;
  }
  else
    val= ULL(0);

  offset_limit_cnt= (ha_rows)val;
#ifndef BIG_TABLES
  /* Check for truncation. */
  if (val != (ulonglong)offset_limit_cnt)
    offset_limit_cnt= HA_POS_ERROR;
#endif
  select_limit_cnt= select_limit_val + offset_limit_cnt;
  if (select_limit_cnt < select_limit_val)
    select_limit_cnt= HA_POS_ERROR;		// no limit
}


/**
  @brief Set the initial purpose of this TABLE_LIST object in the list of used
    tables.

  We need to track this information on table-by-table basis, since when this
  table becomes an element of the pre-locked list, it's impossible to identify
  which SQL sub-statement it has been originally used in.

  E.g.:

  User request:                 SELECT * FROM t1 WHERE f1();
  FUNCTION f1():                DELETE FROM t2; RETURN 1;
  BEFORE DELETE trigger on t2:  INSERT INTO t3 VALUES (old.a);

  For this user request, the pre-locked list will contain t1, t2, t3
  table elements, each needed for different DML.

  The trigger event map is updated to reflect INSERT, UPDATE, DELETE,
  REPLACE, LOAD DATA, CREATE TABLE .. SELECT, CREATE TABLE ..
  REPLACE SELECT statements, and additionally ON DUPLICATE KEY UPDATE
  clause.
*/

void LEX::set_trg_event_type_for_tables()
{
  uint8 new_trg_event_map= 0;
  DBUG_ENTER("LEX::set_trg_event_type_for_tables");

  /*
    Some auxiliary operations
    (e.g. GRANT processing) create TABLE_LIST instances outside
    the parser. Additionally, some commands (e.g. OPTIMIZE) change
    the lock type for a table only after parsing is done. Luckily,
    these do not fire triggers and do not need to pre-load them.
    For these TABLE_LISTs set_trg_event_type is never called, and
    trg_event_map is always empty. That means that the pre-locking
    algorithm will ignore triggers defined on these tables, if
    any, and the execution will either fail with an assert in
    sql_trigger.cc or with an error that a used table was not
    pre-locked, in case of a production build.

    TODO: this usage pattern creates unnecessary module dependencies
    and should be rewritten to go through the parser.
    Table list instances created outside the parser in most cases
    refer to mysql.* system tables. It is not allowed to have
    a trigger on a system table, but keeping track of
    initialization provides extra safety in case this limitation
    is circumvented.
  */

  switch (sql_command) {
  case SQLCOM_LOCK_TABLES:
  /*
    On a LOCK TABLE, all triggers must be pre-loaded for this TABLE_LIST
    when opening an associated TABLE.
  */
    new_trg_event_map= static_cast<uint8>
                        (1 << static_cast<int>(TRG_EVENT_INSERT)) |
                      static_cast<uint8>
                        (1 << static_cast<int>(TRG_EVENT_UPDATE)) |
                      static_cast<uint8>
                        (1 << static_cast<int>(TRG_EVENT_DELETE));
    break;
  /*
    Basic INSERT. If there is an additional ON DUPLIATE KEY UPDATE
    clause, it will be handled later in this method.
  */
  case SQLCOM_INSERT:                           /* fall through */
  case SQLCOM_INSERT_SELECT:
  /*
    LOAD DATA ... INFILE is expected to fire BEFORE/AFTER INSERT
    triggers.
    If the statement also has REPLACE clause, it will be
    handled later in this method.
  */
  case SQLCOM_LOAD:                             /* fall through */
  /*
    REPLACE is semantically equivalent to INSERT. In case
    of a primary or unique key conflict, it deletes the old
    record and inserts a new one. So we also may need to
    fire ON DELETE triggers. This functionality is handled
    later in this method.
  */
  case SQLCOM_REPLACE:                          /* fall through */
  case SQLCOM_REPLACE_SELECT:
  /*
    CREATE TABLE ... SELECT defaults to INSERT if the table or
    view already exists. REPLACE option of CREATE TABLE ...
    REPLACE SELECT is handled later in this method.
  */
  case SQLCOM_CREATE_TABLE:
    new_trg_event_map|= static_cast<uint8>
                          (1 << static_cast<int>(TRG_EVENT_INSERT));
    break;
  /* Basic update and multi-update */
  case SQLCOM_UPDATE:                           /* fall through */
  case SQLCOM_UPDATE_MULTI:
    new_trg_event_map|= static_cast<uint8>
                          (1 << static_cast<int>(TRG_EVENT_UPDATE));
    break;
  /* Basic delete and multi-delete */
  case SQLCOM_DELETE:                           /* fall through */
  case SQLCOM_DELETE_MULTI:
    new_trg_event_map|= static_cast<uint8>
                          (1 << static_cast<int>(TRG_EVENT_DELETE));
    break;
  default:
    break;
  }

  switch (duplicates) {
  case DUP_UPDATE:
    new_trg_event_map|= static_cast<uint8>
                          (1 << static_cast<int>(TRG_EVENT_UPDATE));
    break;
  case DUP_REPLACE:
    new_trg_event_map|= static_cast<uint8>
                          (1 << static_cast<int>(TRG_EVENT_DELETE));
    break;
  case DUP_ERROR:
  default:
    break;
  }


  /*
    Do not iterate over sub-selects, only the tables in the outermost
    SELECT_LEX can be modified, if any.
  */
  TABLE_LIST *tables= select_lex.get_table_list();

  while (tables)
  {
    /*
      This is a fast check to filter out statements that do
      not change data, or tables  on the right side, in case of
      INSERT .. SELECT, CREATE TABLE .. SELECT and so on.
      Here we also filter out OPTIMIZE statement and non-updateable
      views, for which lock_type is TL_UNLOCK or TL_READ after
      parsing.
    */
    if (static_cast<int>(tables->lock_type) >=
        static_cast<int>(TL_WRITE_ALLOW_WRITE))
      tables->trg_event_map= new_trg_event_map;
    tables= tables->next_local;
  }
  DBUG_VOID_RETURN;
}


/*
  Unlink the first table from the global table list and the first table from
  outer select (lex->select_lex) local list

  SYNOPSIS
    unlink_first_table()
    link_to_local	Set to 1 if caller should link this table to local list

  NOTES
    We assume that first tables in both lists is the same table or the local
    list is empty.

  RETURN
    0	If 'query_tables' == 0
    unlinked table
      In this case link_to_local is set.

*/
TABLE_LIST *LEX::unlink_first_table(bool *link_to_local)
{
  TABLE_LIST *first;
  if ((first= query_tables))
  {
    /*
      Exclude from global table list
    */
    if ((query_tables= query_tables->next_global))
      query_tables->prev_global= &query_tables;
    else
      query_tables_last= &query_tables;
    first->next_global= 0;

    /*
      and from local list if it is not empty
    */
    if ((*link_to_local= test(select_lex.table_list.first)))
    {
      select_lex.context.table_list= 
        select_lex.context.first_name_resolution_table= first->next_local;
      select_lex.table_list.first= first->next_local;
      select_lex.table_list.elements--;	//safety
      first->next_local= 0;
      /*
        Ensure that the global list has the same first table as the local
        list.
      */
      first_lists_tables_same();
    }
  }
  return first;
}


/*
  Bring first local table of first most outer select to first place in global
  table list

  SYNOPSYS
     LEX::first_lists_tables_same()

  NOTES
    In many cases (for example, usual INSERT/DELETE/...) the first table of
    main SELECT_LEX have special meaning => check that it is the first table
    in global list and re-link to be first in the global list if it is
    necessary.  We need such re-linking only for queries with sub-queries in
    the select list, as only in this case tables of sub-queries will go to
    the global list first.
*/

void LEX::first_lists_tables_same()
{
  TABLE_LIST *first_table= select_lex.table_list.first;
  if (query_tables != first_table && first_table != 0)
  {
    TABLE_LIST *next;
    if (query_tables_last == &first_table->next_global)
      query_tables_last= first_table->prev_global;

    if ((next= *first_table->prev_global= first_table->next_global))
      next->prev_global= first_table->prev_global;
    /* include in new place */
    first_table->next_global= query_tables;
    /*
       We are sure that query_tables is not 0, because first_table was not
       first table in the global list => we can use
       query_tables->prev_global without check of query_tables
    */
    query_tables->prev_global= &first_table->next_global;
    first_table->prev_global= &query_tables;
    query_tables= first_table;
  }
}


/*
  Link table back that was unlinked with unlink_first_table()

  SYNOPSIS
    link_first_table_back()
    link_to_local	do we need link this table to local

  RETURN
    global list
*/

void LEX::link_first_table_back(TABLE_LIST *first,
				   bool link_to_local)
{
  if (first)
  {
    if ((first->next_global= query_tables))
      query_tables->prev_global= &first->next_global;
    else
      query_tables_last= &first->next_global;
    query_tables= first;

    if (link_to_local)
    {
      first->next_local= select_lex.table_list.first;
      select_lex.context.table_list= first;
      select_lex.table_list.first= first;
      select_lex.table_list.elements++;	//safety
    }
  }
}



/*
  cleanup lex for case when we open table by table for processing

  SYNOPSIS
    LEX::cleanup_after_one_table_open()

  NOTE
    This method is mostly responsible for cleaning up of selects lists and
    derived tables state. To rollback changes in Query_tables_list one has
    to call Query_tables_list::reset_query_tables_list(FALSE).
*/

void LEX::cleanup_after_one_table_open()
{
  /*
    thd->lex->derived_tables & additional units may be set if we open
    a view. It is necessary to clear thd->lex->derived_tables flag
    to prevent processing of derived tables during next open_and_lock_tables
    if next table is a real table and cleanup & remove underlying units
    NOTE: all units will be connected to thd->lex->select_lex, because we
    have not UNION on most upper level.
    */
  if (all_selects_list != &select_lex)
  {
    derived_tables= 0;
    select_lex.exclude_from_table_unique_test= false;
    /* cleunup underlying units (units of VIEW) */
    for (SELECT_LEX_UNIT *un= select_lex.first_inner_unit();
         un;
         un= un->next_unit())
      un->cleanup();
    /* reduce all selects list to default state */
    all_selects_list= &select_lex;
    /* remove underlying units (units of VIEW) subtree */
    select_lex.cut_subtree();
  }
}


/*
  Save current state of Query_tables_list for this LEX, and prepare it
  for processing of new statemnt.

  SYNOPSIS
    reset_n_backup_query_tables_list()
      backup  Pointer to Query_tables_list instance to be used for backup
*/

void LEX::reset_n_backup_query_tables_list(Query_tables_list *backup)
{
  backup->set_query_tables_list(this);
  /*
    We have to perform full initialization here since otherwise we
    will damage backed up state.
  */
  this->reset_query_tables_list(TRUE);
}


/*
  Restore state of Query_tables_list for this LEX from backup.

  SYNOPSIS
    restore_backup_query_tables_list()
      backup  Pointer to Query_tables_list instance used for backup
*/

void LEX::restore_backup_query_tables_list(Query_tables_list *backup)
{
  this->destroy_query_tables_list();
  this->set_query_tables_list(backup);
}


/*
  Checks for usage of routines and/or tables in a parsed statement

  SYNOPSIS
    LEX:table_or_sp_used()

  RETURN
    FALSE  No routines and tables used
    TRUE   Either or both routines and tables are used.
*/

bool LEX::table_or_sp_used()
{
  DBUG_ENTER("table_or_sp_used");

  if (sroutines.records || query_tables)
    DBUG_RETURN(TRUE);

  DBUG_RETURN(FALSE);
}


/*
  Do end-of-prepare fixup for list of tables and their merge-VIEWed tables

  SYNOPSIS
    fix_prepare_info_in_table_list()
      thd  Thread handle
      tbl  List of tables to process

  DESCRIPTION
    Perform end-end-of prepare fixup for list of tables, if any of the tables
    is a merge-algorithm VIEW, recursively fix up its underlying tables as
    well.

*/

static void fix_prepare_info_in_table_list(THD *thd, TABLE_LIST *tbl)
{
  for (; tbl; tbl= tbl->next_local)
  {
    if (tbl->on_expr && !tbl->prep_on_expr)
    {
      thd->check_and_register_item_tree(&tbl->prep_on_expr, &tbl->on_expr);
      tbl->on_expr= tbl->on_expr->copy_andor_structure(thd);
    }
    if (tbl->is_view_or_derived() && tbl->is_merged_derived())
    {
      SELECT_LEX *sel= tbl->get_single_select();
      fix_prepare_info_in_table_list(thd, sel->get_table_list());
    }
  }
}


/*
  Save WHERE/HAVING/ON clauses and replace them with disposable copies

  SYNOPSIS
    st_select_lex::fix_prepare_information
      thd          thread handler
      conds        in/out pointer to WHERE condition to be met at execution
      having_conds in/out pointer to HAVING condition to be met at execution
  
  DESCRIPTION
    The passed WHERE and HAVING are to be saved for the future executions.
    This function saves it, and returns a copy which can be thrashed during
    this execution of the statement. By saving/thrashing here we mean only
    We also save the chain of ORDER::next in group_list, in case
    the list is modified by remove_const().
    AND/OR trees.
    The function also calls fix_prepare_info_in_table_list that saves all
    ON expressions.    
*/

void st_select_lex::fix_prepare_information(THD *thd, Item **conds, 
                                            Item **having_conds)
{
  DBUG_ENTER("st_select_lex::fix_prepare_information");
  if (!thd->stmt_arena->is_conventional() && first_execution)
  {
    first_execution= 0;
    if (group_list.first)
    {
      if (!group_list_ptrs)
      {
        void *mem= thd->stmt_arena->alloc(sizeof(Group_list_ptrs));
        group_list_ptrs= new (mem) Group_list_ptrs(thd->stmt_arena->mem_root);
      }
      group_list_ptrs->reserve(group_list.elements);
      for (ORDER *order= group_list.first; order; order= order->next)
      {
        group_list_ptrs->push_back(order);
      }
    }
    if (*conds)
    {
      thd->check_and_register_item_tree(&prep_where, conds);
      *conds= where= prep_where->copy_andor_structure(thd);
    }
    if (*having_conds)
    {
      thd->check_and_register_item_tree(&prep_having, having_conds);
      *having_conds= having= prep_having->copy_andor_structure(thd);
    }
    fix_prepare_info_in_table_list(thd, table_list.first);
  }
  DBUG_VOID_RETURN;
}


/*
  There are st_select_lex::add_table_to_list &
  st_select_lex::set_lock_for_tables are in sql_parse.cc

  st_select_lex::print is in sql_select.cc

  st_select_lex_unit::prepare, st_select_lex_unit::exec,
  st_select_lex_unit::cleanup, st_select_lex_unit::reinit_exec_mechanism,
  st_select_lex_unit::change_result
  are in sql_union.cc
*/

/*
  Sets the kind of hints to be added by the calls to add_index_hint().

  SYNOPSIS
    set_index_hint_type()
      type_arg     The kind of hints to be added from now on.
      clause       The clause to use for hints to be added from now on.

  DESCRIPTION
    Used in filling up the tagged hints list.
    This list is filled by first setting the kind of the hint as a 
    context variable and then adding hints of the current kind.
    Then the context variable index_hint_type can be reset to the
    next hint type.
*/
void st_select_lex::set_index_hint_type(enum index_hint_type type_arg,
                                        index_clause_map clause)
{ 
  current_index_hint_type= type_arg;
  current_index_hint_clause= clause;
}


/*
  Makes an array to store index usage hints (ADD/FORCE/IGNORE INDEX).

  SYNOPSIS
    alloc_index_hints()
      thd         current thread.
*/

void st_select_lex::alloc_index_hints (THD *thd)
{ 
  index_hints= new (thd->mem_root) List<Index_hint>(); 
}



/*
  adds an element to the array storing index usage hints 
  (ADD/FORCE/IGNORE INDEX).

  SYNOPSIS
    add_index_hint()
      thd         current thread.
      str         name of the index.
      length      number of characters in str.

  RETURN VALUE
    0 on success, non-zero otherwise
*/
bool st_select_lex::add_index_hint (THD *thd, char *str, uint length)
{
  return index_hints->push_front (new (thd->mem_root) 
                                 Index_hint(current_index_hint_type,
                                            current_index_hint_clause,
                                            str, length));
}


/**
  Optimize all subqueries that have not been flattened into semi-joins.

  @details
  This functionality is a method of SELECT_LEX instead of JOIN because
  SQL statements as DELETE/UPDATE do not have a corresponding JOIN object.

  @see JOIN::optimize_unflattened_subqueries

  @param const_only  Restrict subquery optimization to constant subqueries

  @return Operation status
  @retval FALSE     success.
  @retval TRUE      error occurred.
*/

bool st_select_lex::optimize_unflattened_subqueries(bool const_only)
{
  SELECT_LEX_UNIT *next_unit= NULL;
  for (SELECT_LEX_UNIT *un= first_inner_unit();
       un;
       un= next_unit ? next_unit : un->next_unit())
  {
    Item_subselect *subquery_predicate= un->item;
    next_unit= NULL;

    if (subquery_predicate)
    {
      if (!subquery_predicate->fixed)
      {
	/*
	 This subquery was excluded as part of some expression so it is
	 invisible from all prepared expression.
       */
	next_unit= un->next_unit();
	un->exclude_level();
	if (next_unit)
	  continue;
	break;
      }
      if (subquery_predicate->substype() == Item_subselect::IN_SUBS)
      {
        Item_in_subselect *in_subs= (Item_in_subselect*) subquery_predicate;
        if (in_subs->is_jtbm_merged)
          continue;
      }

      if (const_only && !subquery_predicate->const_item())
      {
        /* Skip non-constant subqueries if the caller asked so. */
        continue;
      }

      bool empty_union_result= true;
      bool is_correlated_unit= false;
      /*
        If the subquery is a UNION, optimize all the subqueries in the UNION. If
        there is no UNION, then the loop will execute once for the subquery.
      */
      for (SELECT_LEX *sl= un->first_select(); sl; sl= sl->next_select())
      {
        JOIN *inner_join= sl->join;
        if (!inner_join)
          continue;
        SELECT_LEX *save_select= un->thd->lex->current_select;
        ulonglong save_options;
        int res;
        /* We need only 1 row to determine existence */
        un->set_limit(un->global_parameters);
        un->thd->lex->current_select= sl;
        save_options= inner_join->select_options;
        if (options & SELECT_DESCRIBE)
        {
          /* Optimize the subquery in the context of EXPLAIN. */
          sl->set_explain_type();
          sl->options|= SELECT_DESCRIBE;
          inner_join->select_options|= SELECT_DESCRIBE;
        }
        res= inner_join->optimize();
        sl->update_correlated_cache();
        is_correlated_unit|= sl->is_correlated;
        inner_join->select_options= save_options;
        un->thd->lex->current_select= save_select;
        if (empty_union_result)
        {
          /*
            If at least one subquery in a union is non-empty, the UNION result
            is non-empty. If there is no UNION, the only subquery is non-empy.
          */
          empty_union_result= inner_join->empty_result();
        }
        if (res)
          return TRUE;
      }
      if (empty_union_result)
        subquery_predicate->no_rows_in_result();
      if (!is_correlated_unit)
        un->uncacheable&= ~UNCACHEABLE_DEPENDENT;
      subquery_predicate->is_correlated= is_correlated_unit;
    }
  }
  return FALSE;
}



/**
  @brief Process all derived tables/views of the SELECT.

  @param lex    LEX of this thread
  @param phase  phases to run derived tables/views through

  @details
  This function runs specified 'phases' on all tables from the
  table_list of this select.

  @return FALSE ok.
  @return TRUE an error occur.
*/

bool st_select_lex::handle_derived(LEX *lex, uint phases)
{
  for (TABLE_LIST *cursor= (TABLE_LIST*) table_list.first;
       cursor;
       cursor= cursor->next_local)
  {
    if (cursor->is_view_or_derived() && cursor->handle_derived(lex, phases))
      return TRUE;
  }
  return FALSE;
}


/**
  @brief
  Returns first unoccupied table map and table number

  @param map     [out] return found map
  @param tablenr [out] return found tablenr

  @details
  Returns first unoccupied table map and table number in this select.
  Map and table are returned in *'map' and *'tablenr' accordingly.

  @retrun TRUE  no free table map/table number
  @return FALSE found free table map/table number
*/

bool st_select_lex::get_free_table_map(table_map *map, uint *tablenr)
{
  *map= 0;
  *tablenr= 0;
  TABLE_LIST *tl;
  List_iterator<TABLE_LIST> ti(leaf_tables);
  while ((tl= ti++))
  {
    if (tl->table->map > *map)
      *map= tl->table->map;
    if (tl->table->tablenr > *tablenr)
      *tablenr= tl->table->tablenr;
  }
  (*map)<<= 1;
  (*tablenr)++;
  if (*tablenr >= MAX_TABLES)
    return TRUE;
  return FALSE;
}


/**
  @brief
  Append given table to the leaf_tables list.

  @param link  Offset to which list in table structure to use
  @param table Table to append

  @details
  Append given 'table' to the leaf_tables list using the 'link' offset.
  If the 'table' is linked with other tables through next_leaf/next_local
  chains then whole list will be appended.
*/

void st_select_lex::append_table_to_list(TABLE_LIST *TABLE_LIST::*link,
                                         TABLE_LIST *table)
{
  TABLE_LIST *tl;
  for (tl= leaf_tables.head(); tl->*link; tl= tl->*link) ;
  tl->*link= table;
}


/*
  @brief
  Replace given table from the leaf_tables list for a list of tables 

  @param table Table to replace
  @param list  List to substititute the table for

  @details
  Replace 'table' from the leaf_tables list for a list of tables 'tbl_list'.
*/

void st_select_lex::replace_leaf_table(TABLE_LIST *table, List<TABLE_LIST> &tbl_list)
{
  TABLE_LIST *tl;
  List_iterator<TABLE_LIST> ti(leaf_tables);
  while ((tl= ti++))
  {
    if (tl == table)
    {
      ti.replace(tbl_list);
      break;
    }
  }
}


/**
  @brief
  Assigns new table maps to tables in the leaf_tables list

  @param derived    Derived table to take initial table map from
  @param map        table map to begin with
  @param tablenr    table number to begin with
  @param parent_lex new parent select_lex

  @details
  Assign new table maps/table numbers to all tables in the leaf_tables list.
  'map'/'tablenr' are used for the first table and shifted to left/
  increased for each consequent table in the leaf_tables list.
  If the 'derived' table is given then it's table map/number is used for the
  first table in the list and 'map'/'tablenr' are used for the second and
  all consequent tables.
  The 'parent_lex' is set as the new parent select_lex for all tables in the
  list.
*/

void st_select_lex::remap_tables(TABLE_LIST *derived, table_map map,
                                 uint tablenr, SELECT_LEX *parent_lex)
{
  bool first_table= TRUE;
  TABLE_LIST *tl;
  table_map first_map;
  uint first_tablenr;

  if (derived && derived->table)
  {
    first_map= derived->table->map;
    first_tablenr= derived->table->tablenr;
  }
  else
  {
    first_map= map;
    map<<= 1;
    first_tablenr= tablenr++;
  }
  /*
    Assign table bit/table number.
    To the first table of the subselect the table bit/tablenr of the
    derived table is assigned. The rest of tables are getting bits
    sequentially, starting from the provided table map/tablenr.
  */
  List_iterator<TABLE_LIST> ti(leaf_tables);
  while ((tl= ti++))
  {
    if (first_table)
    {
      first_table= FALSE;
      tl->table->set_table_map(first_map, first_tablenr);
    }
    else
    {
      tl->table->set_table_map(map, tablenr);
      tablenr++;
      map<<= 1;
    }
    SELECT_LEX *old_sl= tl->select_lex;
    tl->select_lex= parent_lex;
    for(TABLE_LIST *emb= tl->embedding;
        emb && emb->select_lex == old_sl;
        emb= emb->embedding)
      emb->select_lex= parent_lex;
  }
}

/**
  @brief
  Merge a subquery into this select.

  @param derived     derived table of the subquery to be merged
  @param subq_select select_lex of the subquery
  @param map         table map for assigning to merged tables from subquery
  @param table_no    table number for assigning to merged tables from subquery

  @details
  This function merges a subquery into its parent select. In short the
  merge operation appends the subquery FROM table list to the parent's
  FROM table list. In more details:
    .) the top_join_list of the subquery is wrapped into a join_nest
       and attached to 'derived'
    .) subquery's leaf_tables list  is merged with the leaf_tables
       list of this select_lex
    .) the table maps and table numbers of the tables merged from
       the subquery are adjusted to reflect their new binding to
       this select

  @return TRUE  an error occur
  @return FALSE ok
*/

bool SELECT_LEX::merge_subquery(THD *thd, TABLE_LIST *derived,
                                SELECT_LEX *subq_select,
                                uint table_no, table_map map)
{
  derived->wrap_into_nested_join(subq_select->top_join_list);

  ftfunc_list->concat(subq_select->ftfunc_list);
  if (join ||
      thd->lex->sql_command == SQLCOM_UPDATE_MULTI ||
      thd->lex->sql_command == SQLCOM_DELETE_MULTI)
  {
    List_iterator_fast<Item_in_subselect> li(subq_select->sj_subselects);
    Item_in_subselect *in_subq;
    while ((in_subq= li++))
    {
      sj_subselects.push_back(in_subq);
      if (in_subq->emb_on_expr_nest == NO_JOIN_NEST)
         in_subq->emb_on_expr_nest= derived;
    }
  }

  /* Walk through child's tables and adjust table map, tablenr,
   * parent_lex */
  subq_select->remap_tables(derived, map, table_no, this);
  subq_select->merged_into= this;

  replace_leaf_table(derived, subq_select->leaf_tables);

  return FALSE;
}


/**
  @brief
  Mark tables from the leaf_tables list as belong to a derived table.

  @param derived   tables will be marked as belonging to this derived

  @details
  Run through the leaf_list and mark all tables as belonging to the 'derived'.
*/

void SELECT_LEX::mark_as_belong_to_derived(TABLE_LIST *derived)
{
  /* Mark tables as belonging to this DT */
  TABLE_LIST *tl;
  List_iterator<TABLE_LIST> ti(leaf_tables);
  while ((tl= ti++))
    tl->belong_to_derived= derived;
}


/**
  @brief
  Update used_tables cache for this select

  @details
  This function updates used_tables cache of ON expressions of all tables
  in the leaf_tables list and of the conds expression (if any).
*/

void SELECT_LEX::update_used_tables()
{
  TABLE_LIST *tl;
  List_iterator<TABLE_LIST> ti(leaf_tables);

  while ((tl= ti++))
  {
    if (tl->table && !tl->is_view_or_derived())
    {
      TABLE_LIST *embedding= tl->embedding;
      for (embedding= tl->embedding; embedding; embedding=embedding->embedding)
      {
        if (embedding->is_view_or_derived())
	{
          DBUG_ASSERT(embedding->is_merged_derived());
          TABLE *tab= tl->table;
          tab->covering_keys= tab->s->keys_for_keyread;
          tab->covering_keys.intersect(tab->keys_in_use_for_query);
          tab->merge_keys.clear_all();
          bitmap_clear_all(tab->read_set);
          bitmap_clear_all(tab->vcol_set);
          break;
        }
      }
    }
  }

  ti.rewind();
  while ((tl= ti++))
  {
    TABLE_LIST *embedding= tl;
    do
    {
      bool maybe_null;
      if ((maybe_null= test(embedding->outer_join)))
      {
	tl->table->maybe_null= maybe_null;
        break;
      }
    }
    while ((embedding= embedding->embedding));
    if (tl->on_expr)
    {
      tl->on_expr->update_used_tables();
      tl->on_expr->walk(&Item::eval_not_null_tables, 0, NULL);
    }
    /*
      - There is no need to check sj_on_expr, because merged semi-joins inject
        sj_on_expr into the parent's WHERE clase.
      - For non-merged semi-joins (aka JTBMs), we need to check their
        left_expr. There is no need to check the rest of the subselect, we know
        it is uncorrelated and so cannot refer to any tables in this select.
    */
    if (tl->jtbm_subselect)
    {
      Item *left_expr= tl->jtbm_subselect->left_expr;
      left_expr->walk(&Item::update_table_bitmaps_processor, FALSE, NULL);
    }

    embedding= tl->embedding;
    while (embedding)
    {
      if (embedding->on_expr && 
          embedding->nested_join->join_list.head() == tl)
      {
        embedding->on_expr->update_used_tables();
        embedding->on_expr->walk(&Item::eval_not_null_tables, 0, NULL);
      }
      tl= embedding;
      embedding= tl->embedding;
    }
  }

  if (join->conds)
  {
    join->conds->update_used_tables();
    join->conds->walk(&Item::eval_not_null_tables, 0, NULL);
  }
  if (join->having)
  {
    join->having->update_used_tables();
  }

  Item *item;
  List_iterator_fast<Item> it(join->fields_list);
  while ((item= it++))
  {
    item->update_used_tables();
  }
  Item_outer_ref *ref;
  List_iterator_fast<Item_outer_ref> ref_it(inner_refs_list);
  while ((ref= ref_it++))
  {
    item= ref->outer_ref;
    item->update_used_tables();
  }
  for (ORDER *order= group_list.first; order; order= order->next)
    (*order->item)->update_used_tables();
  if (!master_unit()->is_union() || master_unit()->global_parameters != this)
  {
    for (ORDER *order= order_list.first; order; order= order->next)
      (*order->item)->update_used_tables();
  }
  join->result->update_used_tables();
}


/**
  @brief
  Update is_correlated cache for this select

  @details
*/

void st_select_lex::update_correlated_cache()
{
  TABLE_LIST *tl;
  List_iterator<TABLE_LIST> ti(leaf_tables);

  is_correlated= false;

  while ((tl= ti++))
  {
    if (tl->on_expr)
      is_correlated|= test(tl->on_expr->used_tables() & OUTER_REF_TABLE_BIT);
    for (TABLE_LIST *embedding= tl->embedding ; embedding ;
         embedding= embedding->embedding)
    {
      if (embedding->on_expr)
        is_correlated|= test(embedding->on_expr->used_tables() &
                            OUTER_REF_TABLE_BIT);
    }
  }

  if (join->conds)
    is_correlated|= test(join->conds->used_tables() & OUTER_REF_TABLE_BIT);

  if (join->having)
    is_correlated|= test(join->having->used_tables() & OUTER_REF_TABLE_BIT);

  if (join->tmp_having)
    is_correlated|= test(join->tmp_having->used_tables() & OUTER_REF_TABLE_BIT);

  Item *item;
  List_iterator_fast<Item> it(join->fields_list);
  while ((item= it++))
    is_correlated|= test(item->used_tables() & OUTER_REF_TABLE_BIT);

  for (ORDER *order= group_list.first; order; order= order->next)
    is_correlated|= test((*order->item)->used_tables() & OUTER_REF_TABLE_BIT);

  if (!master_unit()->is_union())
  {
    for (ORDER *order= order_list.first; order; order= order->next)
      is_correlated|= test((*order->item)->used_tables() & OUTER_REF_TABLE_BIT);
  }

  if (!is_correlated)
    uncacheable&= ~UNCACHEABLE_DEPENDENT;
}


/**
  Set the EXPLAIN type for this subquery.
*/

void st_select_lex::set_explain_type()
{
  bool is_primary= FALSE;
  if (next_select())
    is_primary= TRUE;

  if (!is_primary && first_inner_unit())
  {
    /*
      If there is at least one materialized derived|view then it's a PRIMARY select.
      Otherwise, all derived tables/views were merged and this select is a SIMPLE one.
    */
    for (SELECT_LEX_UNIT *un= first_inner_unit(); un; un= un->next_unit())
    {
      if ((!un->derived || un->derived->is_materialized_derived()))
      {
        is_primary= TRUE;
        break;
      }
    }
  }

  SELECT_LEX *first= master_unit()->first_select();
  /* drop UNCACHEABLE_EXPLAIN, because it is for internal usage only */
  uint8 is_uncacheable= (uncacheable & ~UNCACHEABLE_EXPLAIN);
  
  bool using_materialization= FALSE;
  Item_subselect *parent_item;
  if ((parent_item= master_unit()->item) &&
      parent_item->substype() == Item_subselect::IN_SUBS)
  {
    Item_in_subselect *in_subs= (Item_in_subselect*)parent_item;
    /*
      Surprisingly, in_subs->is_set_strategy() can return FALSE here,
      even for the last invocation of this function for the select.
    */
    if (in_subs->test_strategy(SUBS_MATERIALIZATION))
      using_materialization= TRUE;
  }

  if (&master_unit()->thd->lex->select_lex == this)
  {
     type= is_primary ? "PRIMARY" : "SIMPLE";
  }
  else
  {
    if (this == first)
    {
      /* If we're a direct child of a UNION, we're the first sibling there */
      if (linkage == DERIVED_TABLE_TYPE)
        type= "DERIVED";
      else if (using_materialization)
        type= "MATERIALIZED";
      else
      {
         if (is_uncacheable & UNCACHEABLE_DEPENDENT)
           type= "DEPENDENT SUBQUERY";
         else
         {
           type= is_uncacheable? "UNCACHEABLE SUBQUERY" :
                                 "SUBQUERY";
         }
      }
    }
    else
    {
      /* This a non-first sibling in UNION */
      if (is_uncacheable & UNCACHEABLE_DEPENDENT)
        type= "DEPENDENT UNION";
      else if (using_materialization)
        type= "MATERIALIZED UNION";
      else
      {
        type= is_uncacheable ? "UNCACHEABLE UNION": "UNION";
      }
    }
  }
  options|= SELECT_DESCRIBE;
}


/**
  @brief
  Increase estimated number of records for a derived table/view

  @param records  number of records to increase estimate by

  @details
  This function increases estimated number of records by the 'records'
  for the derived table to which this select belongs to.
*/

void SELECT_LEX::increase_derived_records(ha_rows records)
{
  SELECT_LEX_UNIT *unit= master_unit();
  DBUG_ASSERT(unit->derived);

  select_union *result= (select_union*)unit->result;
  result->records+= records;
}


/**
  @brief
  Mark select's derived table as a const one.

  @param empty Whether select has an empty result set

  @details
  Mark derived table/view of this select as a constant one (to
  materialize it at the optimization phase) unless this select belongs to a
  union. Estimated number of rows is incremented if this select has non empty
  result set.
*/

void SELECT_LEX::mark_const_derived(bool empty)
{
  TABLE_LIST *derived= master_unit()->derived;
  if (!join->thd->lex->describe && derived)
  {
    if (!empty)
      increase_derived_records(1);
    if (!master_unit()->is_union() && !derived->is_merged_derived())
      derived->fill_me= TRUE;
  }
}


bool st_select_lex::save_leaf_tables(THD *thd)
{
  Query_arena *arena, backup;
  arena= thd->activate_stmt_arena_if_needed(&backup);

  List_iterator_fast<TABLE_LIST> li(leaf_tables);
  TABLE_LIST *table;
  while ((table= li++))
  {
    if (leaf_tables_exec.push_back(table))
      return 1;
    table->tablenr_exec= table->get_tablenr();
    table->map_exec= table->get_map();
    if (join && (join->select_options & SELECT_DESCRIBE))
      table->maybe_null_exec= 0;
    else
      table->maybe_null_exec= table->table?  table->table->maybe_null: 0;
  }
  if (arena)
    thd->restore_active_arena(arena, &backup);

  return 0;
}


bool LEX::save_prep_leaf_tables()
{
  if (!thd->save_prep_leaf_list)
    return FALSE;

  Query_arena *arena= thd->stmt_arena, backup;
  arena= thd->activate_stmt_arena_if_needed(&backup);
  //It is used for DETETE/UPDATE so top level has only one SELECT
  DBUG_ASSERT(select_lex.next_select() == NULL);
  bool res= select_lex.save_prep_leaf_tables(thd);

  if (arena)
    thd->restore_active_arena(arena, &backup);

  if (res)
    return TRUE;

  thd->save_prep_leaf_list= FALSE;
  return FALSE;
}


bool st_select_lex::save_prep_leaf_tables(THD *thd)
{
  List_iterator_fast<TABLE_LIST> li(leaf_tables);
  TABLE_LIST *table;

  /*
    Check that the SELECT_LEX was really prepared and so tables are setup.

    It can be subquery in SET clause of UPDATE which was not prepared yet, so
    its tables are not yet setup and ready for storing.
  */
  if (prep_leaf_list_state != READY)
    return FALSE;

  while ((table= li++))
  {
    if (leaf_tables_prep.push_back(table))
      return TRUE;
  }
  prep_leaf_list_state= SAVED;
  for (SELECT_LEX_UNIT *u= first_inner_unit(); u; u= u->next_unit())
  {
    for (SELECT_LEX *sl= u->first_select(); sl; sl= sl->next_select())
    {
      if (sl->save_prep_leaf_tables(thd))
        return TRUE;
    }
  }

  return FALSE;
}


/*
  Return true if this select_lex has been converted into a semi-join nest
  within 'ancestor'.

  We need a loop to check this because there could be several nested
  subselects, like

    SELECT ... FROM grand_parent 
      WHERE expr1 IN (SELECT ... FROM parent 
                        WHERE expr2 IN ( SELECT ... FROM child)

  which were converted into:
  
    SELECT ... 
    FROM grand_parent SEMI_JOIN (parent JOIN child) 
    WHERE 
      expr1 AND expr2

  In this case, both parent and child selects were merged into the parent.
*/

bool st_select_lex::is_merged_child_of(st_select_lex *ancestor)
{
  bool all_merged= TRUE;
  for (SELECT_LEX *sl= this; sl && sl!=ancestor;
       sl=sl->outer_select())
  {
    Item *subs= sl->master_unit()->item;
    if (subs && subs->type() == Item::SUBSELECT_ITEM && 
        ((Item_subselect*)subs)->substype() == Item_subselect::IN_SUBS &&
        ((Item_in_subselect*)subs)->test_strategy(SUBS_SEMI_JOIN))
    {
      continue;
    }

    if (sl->master_unit()->derived &&
      sl->master_unit()->derived->is_merged_derived())
    {
      continue;
    }
    all_merged= FALSE;
    break;
  }
  return all_merged;
}


/**
  A routine used by the parser to decide whether we are specifying a full
  partitioning or if only partitions to add or to split.

  @note  This needs to be outside of WITH_PARTITION_STORAGE_ENGINE since it
  is used from the sql parser that doesn't have any ifdef's

  @retval  TRUE    Yes, it is part of a management partition command
  @retval  FALSE          No, not a management partition command
*/

bool LEX::is_partition_management() const
{
  return (sql_command == SQLCOM_ALTER_TABLE &&
          (alter_info.flags == ALTER_ADD_PARTITION ||
           alter_info.flags == ALTER_REORGANIZE_PARTITION));
}

#ifdef MYSQL_SERVER
uint binlog_unsafe_map[256];

#define UNSAFE(a, b, c) \
  { \
  DBUG_PRINT("unsafe_mixed_statement", ("SETTING BASE VALUES: %s, %s, %02X\n", \
    LEX::stmt_accessed_table_string(a), \
    LEX::stmt_accessed_table_string(b), \
    c)); \
  unsafe_mixed_statement(a, b, c); \
  }

/*
  Sets the combination given by "a" and "b" and automatically combinations
  given by other types of access, i.e. 2^(8 - 2), as unsafe.

  It may happen a colision when automatically defining a combination as unsafe.
  For that reason, a combination has its unsafe condition redefined only when
  the new_condition is greater then the old. For instance,
  
     . (BINLOG_DIRECT_ON & TRX_CACHE_NOT_EMPTY) is never overwritten by 
     . (BINLOG_DIRECT_ON | BINLOG_DIRECT_OFF).
*/
void unsafe_mixed_statement(LEX::enum_stmt_accessed_table a,
                            LEX::enum_stmt_accessed_table b, uint condition)
{
  int type= 0;
  int index= (1U << a) | (1U << b);
  
  
  for (type= 0; type < 256; type++)
  {
    if ((type & index) == index)
    {
      binlog_unsafe_map[type] |= condition;
    }
  }
}
/*
  The BINLOG_* AND TRX_CACHE_* values can be combined by using '&' or '|',
  which means that both conditions need to be satisfied or any of them is
  enough. For example, 
    
    . BINLOG_DIRECT_ON & TRX_CACHE_NOT_EMPTY means that the statment is
    unsafe when the option is on and trx-cache is not empty;

    . BINLOG_DIRECT_ON | BINLOG_DIRECT_OFF means the statement is unsafe
    in all cases.

    . TRX_CACHE_EMPTY | TRX_CACHE_NOT_EMPTY means the statement is unsafe
    in all cases. Similar as above.
*/
void binlog_unsafe_map_init()
{
  memset((void*) binlog_unsafe_map, 0, sizeof(uint) * 256);

  /*
    Classify a statement as unsafe when there is a mixed statement and an
    on-going transaction at any point of the execution if:

      1. The mixed statement is about to update a transactional table and
      a non-transactional table.

      2. The mixed statement is about to update a transactional table and
      read from a non-transactional table.

      3. The mixed statement is about to update a non-transactional table
      and temporary transactional table.

      4. The mixed statement is about to update a temporary transactional
      table and read from a non-transactional table.

      5. The mixed statement is about to update a transactional table and
      a temporary non-transactional table.
     
      6. The mixed statement is about to update a transactional table and
      read from a temporary non-transactional table.

      7. The mixed statement is about to update a temporary transactional
      table and temporary non-transactional table.

      8. The mixed statement is about to update a temporary transactional
      table and read from a temporary non-transactional table.

    After updating a transactional table if:

      9. The mixed statement is about to update a non-transactional table
      and read from a transactional table.

      10. The mixed statement is about to update a non-transactional table
      and read from a temporary transactional table.

      11. The mixed statement is about to update a temporary non-transactional
      table and read from a transactional table.
      
      12. The mixed statement is about to update a temporary non-transactional
      table and read from a temporary transactional table.

      13. The mixed statement is about to update a temporary non-transactional
      table and read from a non-transactional table.

    The reason for this is that locks acquired may not protected a concurrent
    transaction of interfering in the current execution and by consequence in
    the result.
  */
  /* Case 1. */
  UNSAFE(LEX::STMT_WRITES_TRANS_TABLE, LEX::STMT_WRITES_NON_TRANS_TABLE,
    BINLOG_DIRECT_ON | BINLOG_DIRECT_OFF);
  /* Case 2. */
  UNSAFE(LEX::STMT_WRITES_TRANS_TABLE, LEX::STMT_READS_NON_TRANS_TABLE,
    BINLOG_DIRECT_ON | BINLOG_DIRECT_OFF);
  /* Case 3. */
  UNSAFE(LEX::STMT_WRITES_NON_TRANS_TABLE, LEX::STMT_WRITES_TEMP_TRANS_TABLE,
    BINLOG_DIRECT_ON | BINLOG_DIRECT_OFF);
  /* Case 4. */
  UNSAFE(LEX::STMT_WRITES_TEMP_TRANS_TABLE, LEX::STMT_READS_NON_TRANS_TABLE,
    BINLOG_DIRECT_ON | BINLOG_DIRECT_OFF);
  /* Case 5. */
  UNSAFE(LEX::STMT_WRITES_TRANS_TABLE, LEX::STMT_WRITES_TEMP_NON_TRANS_TABLE,
    BINLOG_DIRECT_ON);
  /* Case 6. */
  UNSAFE(LEX::STMT_WRITES_TRANS_TABLE, LEX::STMT_READS_TEMP_NON_TRANS_TABLE,
    BINLOG_DIRECT_ON);
  /* Case 7. */
  UNSAFE(LEX::STMT_WRITES_TEMP_TRANS_TABLE, LEX::STMT_WRITES_TEMP_NON_TRANS_TABLE,
    BINLOG_DIRECT_ON);
  /* Case 8. */
  UNSAFE(LEX::STMT_WRITES_TEMP_TRANS_TABLE, LEX::STMT_READS_TEMP_NON_TRANS_TABLE,
    BINLOG_DIRECT_ON);
  /* Case 9. */
  UNSAFE(LEX::STMT_WRITES_NON_TRANS_TABLE, LEX::STMT_READS_TRANS_TABLE,
    (BINLOG_DIRECT_ON | BINLOG_DIRECT_OFF) & TRX_CACHE_NOT_EMPTY);
  /* Case 10 */
  UNSAFE(LEX::STMT_WRITES_NON_TRANS_TABLE, LEX::STMT_READS_TEMP_TRANS_TABLE,
    (BINLOG_DIRECT_ON | BINLOG_DIRECT_OFF) & TRX_CACHE_NOT_EMPTY);
  /* Case 11. */
  UNSAFE(LEX::STMT_WRITES_TEMP_NON_TRANS_TABLE, LEX::STMT_READS_TRANS_TABLE,
    BINLOG_DIRECT_ON & TRX_CACHE_NOT_EMPTY);
  /* Case 12. */
  UNSAFE(LEX::STMT_WRITES_TEMP_NON_TRANS_TABLE, LEX::STMT_READS_TEMP_TRANS_TABLE,
    BINLOG_DIRECT_ON & TRX_CACHE_NOT_EMPTY);
  /* Case 13. */
  UNSAFE(LEX::STMT_WRITES_TEMP_NON_TRANS_TABLE, LEX::STMT_READS_NON_TRANS_TABLE,
     BINLOG_DIRECT_OFF & TRX_CACHE_NOT_EMPTY);
}
#endif

#ifdef HAVE_EXPLICIT_TEMPLATE_INSTANTIATION
template class Mem_root_array<ORDER*, true>;
#endif
