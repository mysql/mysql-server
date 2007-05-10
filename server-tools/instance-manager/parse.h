#ifndef INCLUDES_MYSQL_INSTANCE_MANAGER_PARSE_H
#define INCLUDES_MYSQL_INSTANCE_MANAGER_PARSE_H
/* Copyright (C) 2004 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <my_global.h>
#include <my_sys.h>
#include <m_string.h>

class Command;

enum Log_type
{
  IM_LOG_ERROR= 0,
  IM_LOG_GENERAL,
  IM_LOG_SLOW
};

Command *parse_command(const char *text);

bool parse_option_value(const char *text, size_t *text_len, char **value);

void skip_spaces(const char **text);

/* define kinds of the word seek method */
enum enum_seek_method { ALPHANUM= 1, NONSPACE, OPTION_NAME };

/************************************************************************/

class Named_value
{
public:
  /*
    The purpose of these methods is just to have one method for
    allocating/deallocating memory for strings for Named_value.
  */

  static inline char *alloc_str(const LEX_STRING *str);
  static inline char *alloc_str(const char *str);
  static inline void free_str(char **str);

public:
  inline Named_value();
  inline Named_value(char *name_arg, char *value_arg);

  inline char *get_name();
  inline char *get_value();

  inline void free();

private:
  char *name;
  char *value;
};

inline char *Named_value::alloc_str(const LEX_STRING *str)
{
  return my_strndup(str->str, str->length, MYF(0));
}

inline char *Named_value::alloc_str(const char *str)
{
  return my_strdup(str, MYF(0));
}

inline void Named_value::free_str(char **str)
{
  my_free(*str, MYF(MY_ALLOW_ZERO_PTR));
  *str= NULL;
}

inline Named_value::Named_value()
  :name(NULL), value(NULL)
{ }

inline Named_value::Named_value(char *name_arg, char *value_arg)
  :name(name_arg), value(value_arg)
{ }

inline char *Named_value::get_name()
{
  return name;
}

inline char *Named_value::get_value()
{
  return value;
}

void Named_value::free()
{
  free_str(&name);
  free_str(&value);
}

/************************************************************************/

class Named_value_arr
{
public:
  Named_value_arr();
  ~Named_value_arr();

  bool init();

  inline int get_size() const;
  inline Named_value get_element(int idx) const;
  inline void remove_element(int idx);
  inline bool add_element(Named_value *option);
  inline bool replace_element(int idx, Named_value *option);

private:
  bool initialized;
  DYNAMIC_ARRAY arr;
};


inline int Named_value_arr::get_size() const
{
  return arr.elements;
}


inline Named_value Named_value_arr::get_element(int idx) const
{
  DBUG_ASSERT(0 <= idx && (uint) idx < arr.elements);

  Named_value option;
  get_dynamic((DYNAMIC_ARRAY *) &arr, (uchar*) &option, idx);

  return option;
}


inline void Named_value_arr::remove_element(int idx)
{
  DBUG_ASSERT(0 <= idx && (uint) idx < arr.elements);

  get_element(idx).free();

  delete_dynamic_element(&arr, idx);
}


inline bool Named_value_arr::add_element(Named_value *option)
{
  return insert_dynamic(&arr, (uchar*) option);
}


inline bool Named_value_arr::replace_element(int idx, Named_value *option)
{
  DBUG_ASSERT(0 <= idx && (uint) idx < arr.elements);

  get_element(idx).free();

  return set_dynamic(&arr, (uchar*) option, idx);
}

/************************************************************************/

/*
  tries to find next word in the text
  if found, returns the beginning and puts word length to word_len argument.
  if not found returns pointer to first non-space or to '\0', word_len == 0
*/

inline void get_word(const char **text, size_t *word_len,
                     enum_seek_method seek_method= ALPHANUM)
{
  const char *word_end;

  /* skip space */
  while (my_isspace(default_charset_info, **text))
    ++(*text);

  word_end= *text;

  switch (seek_method) {
  case ALPHANUM:
    while (my_isalnum(default_charset_info, *word_end))
      ++word_end;
    break;
  case NONSPACE:
    while (!my_isspace(default_charset_info, *word_end) &&
           (*word_end != '\0'))
      ++word_end;
    break;
  case OPTION_NAME:
    while (my_isalnum(default_charset_info, *word_end) ||
           *word_end == '-' ||
           *word_end == '_')
      ++word_end;
    break;
  }

  *word_len= word_end - *text;
}

#endif /* INCLUDES_MYSQL_INSTANCE_MANAGER_PARSE_H */
