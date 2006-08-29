/* Copyright (C) 2004 MySQL AB

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

#include "parse.h"
#include "commands.h"


enum Token
{
  TOK_CREATE= 0,
  TOK_DROP,
  TOK_ERROR, /* Encodes the "ERROR" word, it doesn't indicate error. */
  TOK_FILES,
  TOK_FLUSH,
  TOK_GENERAL,
  TOK_INSTANCE,
  TOK_INSTANCES,
  TOK_LOG,
  TOK_OPTIONS,
  TOK_SET,
  TOK_SLOW,
  TOK_START,
  TOK_STATUS,
  TOK_STOP,
  TOK_SHOW,
  TOK_UNSET,
  TOK_NOT_FOUND, // must be after all tokens
  TOK_END
};


struct tokens_st
{
  uint length;
  const char *tok_name;
};


static struct tokens_st tokens[]= {
  {6, "CREATE"},
  {4, "DROP"},
  {5, "ERROR"},
  {5, "FILES"},
  {5, "FLUSH"},
  {7, "GENERAL"},
  {8, "INSTANCE"},
  {9, "INSTANCES"},
  {3, "LOG"},
  {7, "OPTIONS"},
  {3, "SET"},
  {4, "SLOW"},
  {5, "START"},
  {6, "STATUS"},
  {4, "STOP"},
  {4, "SHOW"},
  {5, "UNSET"}
};

/************************************************************************/

Named_value_arr::Named_value_arr() :
  initialized(FALSE)
{
}


bool Named_value_arr::init()
{
  if (my_init_dynamic_array(&arr, sizeof(Named_value), 0, 32))
    return TRUE;

  initialized= TRUE;

  return FALSE;
}


Named_value_arr::~Named_value_arr()
{
  if (!initialized)
    return;

  for (int i= 0; i < get_size(); ++i)
    get_element(i).free();

  delete_dynamic(&arr);
}

/************************************************************************/

/*
  Returns token no if word corresponds to some token, otherwise returns
  TOK_NOT_FOUND
*/

inline Token find_token(const char *word, uint word_len)
{
  int i= 0;
  do
  {
    if (my_strnncoll(default_charset_info, (const uchar *) tokens[i].tok_name,
                     tokens[i].length, (const uchar *) word, word_len) == 0)
      break;
  }
  while (++i < TOK_NOT_FOUND);
  return (Token) i;
}


Token get_token(const char **text, uint *word_len)
{
  get_word(text, word_len);
  if (*word_len)
    return find_token(*text, *word_len);
  return TOK_END;
}


Token shift_token(const char **text, uint *word_len)
{
  Token save= get_token(text, word_len);
  (*text)+= *word_len;
  return save;
}


int get_text_id(const char **text, LEX_STRING *token)
{
  get_word(text, &token->length);
  if (token->length == 0)
    return 1;
  token->str= (char *) *text;
  return 0;
}


static bool parse_long(const LEX_STRING *token, long *value)
{
  int err_code;
  char *end_ptr= token->str + token->length;

  *value= (long)my_strtoll10(token->str, &end_ptr, &err_code);

  return err_code != 0;
}


bool parse_option_value(const char *text, uint *text_len, char **value)
{
  char beginning_quote;
  const char *text_start_ptr;
  char *v;
  bool escape_mode= FALSE;

  if (!*text || (*text != '\'' && *text != '"'))
    return TRUE; /* syntax error: string expected. */

  beginning_quote= *text;

  ++text; /* skip the beginning quote. */

  text_start_ptr= text;

  if (!(v= Named_value::alloc_str(text)))
    return TRUE;

  *value= v;

  while (TRUE)
  {
    if (!*text)
    {
      Named_value::free_str(value);
      return TRUE; /* syntax error: missing terminating ' character. */
    }

    if (*text == '\n' || *text == '\r')
    {
      Named_value::free_str(value);
      return TRUE; /* syntax error: option value should be a single line. */
    }

    if (!escape_mode && *text == beginning_quote)
      break;

    if (escape_mode)
    {
      switch (*text)
      {
        case 'b': /* \b -- backspace */
          if (v > *value)
            --v;
          break;

        case 't': /* \t -- tab */
          *v= '\t';
          ++v;
          break;

        case 'n': /* \n -- newline */
          *v= '\n';
          ++v;
          break;

        case 'r': /* \r -- carriage return */
          *v= '\r';
          ++v;
          break;

        case '\\': /* \\ -- back slash */
          *v= '\\';
          ++v;
          break;

        case 's': /* \s -- space */
          *v= ' ';
          ++v;
          break;

        default: /* Unknown escape sequence. Treat as error. */
          Named_value::free_str(value);
          return TRUE;
      }

      escape_mode= FALSE;
    }
    else
    {
      if (*text == '\\')
      {
        escape_mode= TRUE;
      }
      else
      {
        *v= *text;
        ++v;
      }
    }

    ++text;
  }

  *v= 0;

  /* "2" below stands for beginning and ending quotes. */
  *text_len= text - text_start_ptr + 2;

  return FALSE;
}


void skip_spaces(const char **text)
{
  while (**text && my_isspace(default_charset_info, **text))
    ++(*text);
}


Command *parse_command(Instance_map *map, const char *text)
{
  uint word_len;
  LEX_STRING instance_name;
  Command *command;
  const char *saved_text= text;

  Token tok1= shift_token(&text, &word_len);

  switch (tok1) {
  case TOK_START:                               // fallthrough
  case TOK_STOP:
  case TOK_CREATE:
  case TOK_DROP:
    if (shift_token(&text, &word_len) != TOK_INSTANCE)
      goto syntax_error;
    get_word(&text, &word_len);
    if (word_len == 0)
      goto syntax_error;
    instance_name.str= (char *) text;
    instance_name.length= word_len;
    text+= word_len;

    if (tok1 == TOK_CREATE)
    {
      Create_instance *cmd= new Create_instance(map, &instance_name);

      if (!cmd)
        return NULL; /* Report ER_OUT_OF_RESOURCES. */

      if (cmd->init(&text))
      {
        delete cmd;
        goto syntax_error;
      }

      command= cmd;
    }
    else
    {
      /* it should be the end of command */
      get_word(&text, &word_len, NONSPACE);
      if (word_len)
        goto syntax_error;
    }

    switch (tok1) {
    case TOK_START:
      command= new Start_instance(map, &instance_name);
      break;
    case TOK_STOP:
      command= new Stop_instance(map, &instance_name);
      break;
    case TOK_CREATE:
      ; /* command already initialized. */
      break;
    case TOK_DROP:
      command= new Drop_instance(map, &instance_name);
      break;
    default: /* this is impossible, but nevertheless... */
      DBUG_ASSERT(0);
    }
    break;
  case TOK_FLUSH:
    if (shift_token(&text, &word_len) != TOK_INSTANCES)
      goto syntax_error;

    get_word(&text, &word_len, NONSPACE);
    if (word_len)
      goto syntax_error;

    command= new Flush_instances(map);
    break;
  case TOK_UNSET:
  case TOK_SET:
    {
      Abstract_option_cmd *cmd;

      if (tok1 == TOK_SET)
        cmd= new Set_option(map);
      else
        cmd= new Unset_option(map);

      if (!cmd)
        return NULL; /* Report ER_OUT_OF_RESOURCES. */

      if (cmd->init(&text))
      {
        delete cmd;
        goto syntax_error;
      }

      command= cmd;

      break;
    }
  case TOK_SHOW:
    switch (shift_token(&text, &word_len)) {
    case TOK_INSTANCES:
      get_word(&text, &word_len, NONSPACE);
      if (word_len)
        goto syntax_error;
      command= new Show_instances(map);
      break;
    case TOK_INSTANCE:
      switch (Token tok2= shift_token(&text, &word_len)) {
      case TOK_OPTIONS:
      case TOK_STATUS:
        if (get_text_id(&text, &instance_name))
          goto syntax_error;
        text+= instance_name.length;
        /* check that this is the end of the command */
        get_word(&text, &word_len, NONSPACE);
        if (word_len)
          goto syntax_error;
        if (tok2 == TOK_STATUS)
          command= new Show_instance_status(map, &instance_name);
        else
          command= new Show_instance_options(map, &instance_name);
        break;
      default:
        goto syntax_error;
      }
      break;
    default:
      instance_name.str= (char *) text - word_len;
      instance_name.length= word_len;
      if (instance_name.length)
      {
        Log_type log_type;

        long log_size;
        LEX_STRING log_size_str;

        long log_offset= 0;
        LEX_STRING log_offset_str= { NULL, 0 };

        switch (shift_token(&text, &word_len)) {
        case TOK_LOG:
          switch (Token tok3= shift_token(&text, &word_len)) {
          case TOK_FILES:
            get_word(&text, &word_len, NONSPACE);
            /* check that this is the end of the command */
            if (word_len)
              goto syntax_error;
            command= new Show_instance_log_files(map, &instance_name);
            break;
          case TOK_ERROR:
          case TOK_GENERAL:
          case TOK_SLOW:
            /* define a log type */
            switch (tok3) {
            case TOK_ERROR:
              log_type= IM_LOG_ERROR;
              break;
            case TOK_GENERAL:
              log_type= IM_LOG_GENERAL;
              break;
            case TOK_SLOW:
              log_type= IM_LOG_SLOW;
              break;
            default:
              goto syntax_error;
            }
            /* get the size of the log we want to retrieve */
            if (get_text_id(&text, &log_size_str))
              goto syntax_error;
            text+= log_size_str.length;

            /* this parameter is required */
            if (!log_size_str.length)
              goto syntax_error;

            /* the next token should be comma, or nothing */
            get_word(&text, &word_len);
            switch (*text) {
              case ',':
                text++; /* swallow the comma */
                /* read the next word */
                get_word(&text, &word_len);
                if (!word_len)
                  goto syntax_error;
                log_offset_str.str= (char *) text;
                log_offset_str.length= word_len;
                text+= word_len;
                get_word(&text, &word_len, NONSPACE);
                /* check that this is the end of the command */
                if (word_len)
                  goto syntax_error;
                break;
              case '\0':
                break; /* this is ok */
              default:
                goto syntax_error;
            }

            /* Parse size parameter. */

            if (parse_long(&log_size_str, &log_size))
              goto syntax_error;

            if (log_size <= 0)
              goto syntax_error;

            /* Parse offset parameter (if specified). */

            if (log_offset_str.length)
            {
              if (parse_long(&log_offset_str, &log_offset))
                goto syntax_error;

              if (log_offset <= 0)
                goto syntax_error;
            }

            command= new Show_instance_log(map, &instance_name,
                                           log_type, log_size, log_offset);
          break;
          default:
            goto syntax_error;
          }
        break;
        default:
          goto syntax_error;
        }
      }
      else
        goto syntax_error;
      break;
    }
    break;
  default:
syntax_error:
    command= new Syntax_error();
  }
  return command;
}
