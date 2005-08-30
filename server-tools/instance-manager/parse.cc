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

#include <string.h>


enum Token
{
  TOK_ERROR= 0, /* Encodes the "ERROR" word, it doesn't indicate error. */
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


int get_text_id(const char **text, uint *word_len, const char **id)
{
  get_word(text, word_len);
  if (word_len == 0)
    return 1;
  *id= *text;
  return 0;
}


Command *parse_command(Instance_map *map, const char *text)
{
  uint word_len;
  const char *instance_name;
  uint instance_name_len;
  const char *option;
  uint option_len;
  const char *option_value;
  uint option_value_len;
  const char *log_size;
  Command *command;
  const char *saved_text= text;
  bool skip= false;
  const char *tmp;

  Token tok1= shift_token(&text, &word_len);

  switch (tok1) {
  case TOK_START:                               // fallthrough
  case TOK_STOP:
    if (shift_token(&text, &word_len) != TOK_INSTANCE)
      goto syntax_error;
    get_word(&text, &word_len);
    if (word_len == 0)
      goto syntax_error;
    instance_name= text;
    instance_name_len= word_len;
    text+= word_len;
    /* it should be the end of command */
    get_word(&text, &word_len);
    if (word_len)
      goto syntax_error;

    if (tok1 == TOK_START)
      command= new Start_instance(map, instance_name, instance_name_len);
    else
      command= new Stop_instance(map, instance_name, instance_name_len);
    break;
  case TOK_FLUSH:
    if (shift_token(&text, &word_len) != TOK_INSTANCES)
      goto syntax_error;

    get_word(&text, &word_len);
    if (word_len)
      goto syntax_error;

    command= new Flush_instances(map);
    break;
  case TOK_UNSET:
    skip= true;
  case TOK_SET:

    get_text_id(&text, &instance_name_len, &instance_name);
    text+= instance_name_len;

   /* the next token should be a dot */
    get_word(&text, &word_len);
    if (*text != '.')
      goto syntax_error;
    text++;

    get_word(&text, &option_len, NONSPACE);
    option= text;
    if ((tmp= strchr(text, '=')) != NULL)
      option_len= tmp - text;
    text+= option_len;

    get_word(&text, &word_len);
    if (*text == '=')
    {
      text++;                                   /* skip '=' */
      get_word(&text, &option_value_len, NONSPACE);
      option_value= text;
      text+= option_value_len;
    }
    else
    {
      option_value= "";
      option_value_len= 0;
    }

    /* should be empty */
    get_word(&text, &word_len);
    if (word_len)
      goto syntax_error;

    if (skip)
      command= new  Unset_option(map, instance_name, instance_name_len,
                                 option, option_len, option_value,
                                 option_value_len);
    else
      command= new Set_option(map, instance_name, instance_name_len,
                              option, option_len, option_value,
                              option_value_len);
    break;
  case TOK_SHOW:
    switch (shift_token(&text, &word_len)) {
    case TOK_INSTANCES:
      get_word(&text, &word_len);
      if (word_len)
        goto syntax_error;
      command= new Show_instances(map);
      break;
    case TOK_INSTANCE:
      switch (Token tok2= shift_token(&text, &word_len)) {
      case TOK_OPTIONS:
      case TOK_STATUS:
        get_text_id(&text, &instance_name_len, &instance_name);
        text+= instance_name_len;
        /* check that this is the end of the command */
        get_word(&text, &word_len);
        if (word_len)
          goto syntax_error;
        if (tok2 == TOK_STATUS)
          command= new Show_instance_status(map, instance_name,
                                            instance_name_len);
        else
          command= new Show_instance_options(map, instance_name,
                                            instance_name_len);
        break;
      default:
        goto syntax_error;
      }
      break;
    default:
      instance_name= text - word_len;
      instance_name_len= word_len;
      if (instance_name_len)
      {
        Log_type log_type;
        switch (shift_token(&text, &word_len)) {
        case TOK_LOG:
          switch (Token tok3= shift_token(&text, &word_len)) {
          case TOK_FILES:
            get_word(&text, &word_len);
            /* check that this is the end of the command */
            if (word_len)
              goto syntax_error;
            command= new Show_instance_log_files(map, instance_name,
                                                 instance_name_len);
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
            get_text_id(&text, &word_len, &log_size);
            text+= word_len;
            /* this parameter is required */
            if (!word_len)
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
                command= new Show_instance_log(map, instance_name,
                                               instance_name_len, log_type,
                                               log_size, text);

                //get_text_id(&text, &log_size_len, &log_size);
                break;
              case '\0':
                command= new Show_instance_log(map, instance_name,
                                               instance_name_len, log_type,
                                               log_size, NULL);
                break; /* this is ok */
              default:
              goto syntax_error;
            }
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
