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
#include <string.h>

enum Token
{
  TOK_FLUSH = 0,
  TOK_INSTANCE,
  TOK_INSTANCES,
  TOK_OPTIONS,
  TOK_START,
  TOK_STATUS,
  TOK_STOP,
  TOK_SHOW,
  TOK_NOT_FOUND, // must be after all tokens
  TOK_END
};

static const char *tokens[]= {
  "FLUSH",
  "INSTANCE",
  "INSTANCES",
  "OPTIONS",
  "START",
  "STATUS",
  "STOP",
  "SHOW",
};


/*
  tries to find next word in the text
  if found, returns the beginning and puts word length to word_len argument.
  if not found returns pointer to first non-space or to '\0', word_len == 0
*/

inline void get_word(const char **text, uint *word_len)
{
  const char *word_end;

  /* skip space */
  while (my_isspace(default_charset_info, **text))
    ++(*text);

  word_end= *text;

  while (my_isalnum(default_charset_info, *word_end))
    ++word_end;

  *word_len= word_end - *text;
}


/*
  Returns token no if word corresponds to some token, otherwise returns
  TOK_NOT_FOUND
*/

inline Token find_token(const char *word, uint word_len)
{
  int i= 0;
  do
  {
    if (strncasecmp(tokens[i], word, word_len) == 0)
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


void print_token(const char *token, uint tok_len)
{
  for (uint i= 0; i < tok_len; ++i)
    printf("%c", token[i]);
}


int get_text_id(const char **text, uint *word_len, const char **id)
{
  get_word(text, word_len);
  if (word_len == 0)
    return 1;
  *id= *text;
  return 0;
}


Command *parse_command(Command_factory *factory, const char *text)
{
  uint word_len;
  const char *instance_name;
  uint instance_name_len;
  Command *command;
  const char *saved_text= text;

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

    command= (tok1 == TOK_START) ? (Command *)
              factory->new_Start_instance(instance_name, instance_name_len):
              (Command *)
              factory->new_Stop_instance(instance_name, instance_name_len);
    break;
  case TOK_FLUSH:
    if (shift_token(&text, &word_len) != TOK_INSTANCES)
      goto syntax_error;

    get_word(&text, &word_len);
    if (word_len)
      goto syntax_error;

    command= factory->new_Flush_instances();
    break;
  case TOK_SHOW:
    switch (shift_token(&text, &word_len)) {
    case TOK_INSTANCES:
      get_word(&text, &word_len);
      if (word_len)
        goto syntax_error;
      command= factory->new_Show_instances();
      break;
    case TOK_INSTANCE:
      switch (Token tok2= shift_token(&text, &word_len)) {
      case TOK_OPTIONS:
      case TOK_STATUS:
        get_text_id(&text, &instance_name_len, &instance_name);
        text+= instance_name_len;
        get_word(&text, &word_len);
        if (word_len)
          goto syntax_error;
        command= (tok2 == TOK_STATUS) ? (Command *)
                  factory->new_Show_instance_status(instance_name,
                                                    instance_name_len):
                  (Command *)
                  factory->new_Show_instance_options(instance_name,
                                                     instance_name_len);
        break;
      default:
        goto syntax_error;
      }
      break;
    default:
      goto syntax_error;
    }
    break;
  default:
syntax_error:
    command= factory->new_Syntax_error();
  }
  return command;
}

