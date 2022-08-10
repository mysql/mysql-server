/* Copyright (c) 2014, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <assert.h>
#include <stddef.h>

#include "my_inttypes.h"
#include "mysql/plugin.h"
#include "storage/innobase/include/fts0tokenize.h"

/* We are following InnoDB coding guidelines. */

/** Ngram token size, by default bigram. */
static int ngram_token_size;

#define RETURN_IF_ERROR(ret) \
  if (ret != 0) return ret;

/** Parse a document into ngram.
@param[in]	param		plugin parser param
@param[in]	doc		document to parse
@param[in]	len		document length in bytes
@param[in,out]	bool_info	boolean info
@retval	0	on success
@retval	1	on failure. */
static int ngram_parse(MYSQL_FTPARSER_PARAM *param, const char *doc, int len,
                       MYSQL_FTPARSER_BOOLEAN_INFO *bool_info) {
  const CHARSET_INFO *cs = param->cs;
  char *start;
  char *next;
  char *end;
  int char_len;
  int n_chars;
  int ret = 0;
  bool is_first = true;

  assert(cs->mbminlen == 1);

  start = const_cast<char *>(doc);
  next = start;
  end = start + len;
  n_chars = 0;

  while (next < end) {
    char_len = my_mbcharlen_ptr(cs, next, end);

    /* Skip the rest of the doc if invalid char. */
    if (next + char_len > end || char_len == 0) {
      break;
    } else {
      /* Skip SPACE or ","/"." etc as they are not words*/
      int ctype;
      cs->cset->ctype(cs, &ctype, (uchar *)next, (uchar *)end);

      if (char_len == 1 && (*next == ' ' || !true_word_char(ctype, *next))) {
        start = next + 1;
        next = start;
        n_chars = 0;

        continue;
      }

      next += char_len;
      n_chars++;
    }

    if (n_chars == ngram_token_size) {
      /* Add a ngram */
      bool_info->position = start - doc;
      ret = param->mysql_add_word(param, start, next - start, bool_info);
      RETURN_IF_ERROR(ret);

      /* Move a char forward */
      start += my_mbcharlen_ptr(cs, start, end);
      n_chars = ngram_token_size - 1;
      is_first = false;
    }
  }

  /* We handle unigram in cases below:
  1. BOOLEAN MODE: suppose we have phrase search like ('"a bc"');
  2. STOPWORD MODE: we should handle unigram when matching phrase.
  Note: only when the document char len is less than ngram_token_size. */
  switch (param->mode) {
    case MYSQL_FTPARSER_FULL_BOOLEAN_INFO:
    case MYSQL_FTPARSER_WITH_STOPWORDS:
      if (n_chars > 0 && is_first) {
        assert(next > start);
        assert(n_chars < ngram_token_size);

        ret = param->mysql_add_word(param, start, next - start, bool_info);
      }
      break;

    default:
      break;
  }

  return (ret);
}

/** Get token char size by charset
@param[in]	cs	charset
@param[in]	token	token
@param[in]	len	token length in bytes
@retval	size in number of chars */
static int ngram_get_token_size(const CHARSET_INFO *cs, const char *token,
                                int len) {
  const char *start;
  const char *end;
  int size = 0;
  int char_len;

  start = token;
  end = start + len;
  while (start < end) {
    char_len = my_mbcharlen_ptr(cs, start, end);

    size++;
    start += char_len;
  }

  return (size);
}

/** Convert term into phrase and handle wildcard.
@param[in]	param		plugin parser param
@param[in]	token		token to parse
@param[in]	len		token length in bytes
@param[in,out]	bool_info	boolean info
@retval	0	on success
@retval	1	on failure. */
static int ngram_term_convert(MYSQL_FTPARSER_PARAM *param, const char *token,
                              int len, MYSQL_FTPARSER_BOOLEAN_INFO *bool_info) {
  MYSQL_FTPARSER_BOOLEAN_INFO token_info = {FT_TOKEN_WORD, 0,      0, 0, 0, 0,
                                            ' ',           nullptr};
  const CHARSET_INFO *cs = param->cs;
  int token_size;
  int ret = 0;

  assert(bool_info->type == FT_TOKEN_WORD);
  assert(bool_info->quot == nullptr);
  assert(cs->mbminlen == 1);

  /* Convert rules:
  1. if term with wildcard and term length is less than ngram_token_size,
  we keep it as normal term search.
  2. otherwise, term is converted to phrase and wildcard is ignored.
  e.g. 'abc' and 'abc*' are both equivalent to '"ab bc"'.	*/

  token_size = ngram_get_token_size(cs, token, len);
  if (bool_info->trunc && token_size < ngram_token_size) {
    ret =
        param->mysql_add_word(param, const_cast<char *>(token), len, bool_info);
  } else {
    bool_info->type = FT_TOKEN_LEFT_PAREN;
    bool_info->quot = reinterpret_cast<char *>(1);

    ret = param->mysql_add_word(param, nullptr, 0, bool_info);
    RETURN_IF_ERROR(ret);

    ret = ngram_parse(param, token, len, &token_info);
    RETURN_IF_ERROR(ret);

    bool_info->type = FT_TOKEN_RIGHT_PAREN;
    ret = param->mysql_add_word(param, nullptr, 0, bool_info);

    assert(bool_info->quot == nullptr);
    bool_info->type = FT_TOKEN_WORD;
  }

  return (ret);
}

/** Ngram parser parse document.
@param[in]	param	plugin parser param
@retval	0	on success
@retval	1	on failure. */
static int ngram_parser_parse(MYSQL_FTPARSER_PARAM *param) {
  MYSQL_FTPARSER_BOOLEAN_INFO bool_info = {FT_TOKEN_WORD, 0,      0, 0, 0, 0,
                                           ' ',           nullptr};
  const CHARSET_INFO *cs = param->cs;
  uchar **start = reinterpret_cast<uchar **>(&param->doc);
  uchar *end = *start + param->length;
  FT_WORD word = {nullptr, 0, 0};
  int ret = 0;

  switch (param->mode) {
    case MYSQL_FTPARSER_SIMPLE_MODE:
    case MYSQL_FTPARSER_WITH_STOPWORDS:
      ret = ngram_parse(param, param->doc, param->length, &bool_info);

      break;

    case MYSQL_FTPARSER_FULL_BOOLEAN_INFO:
      /* Ngram parser cannot handle query in boolean mode, so we
      first parse query into words with boolean info, then we parse
      the words into ngram. */
      while (fts_get_word(cs, start, end, &word, &bool_info)) {
        if (bool_info.type == FT_TOKEN_WORD) {
          if (bool_info.quot != nullptr) {
            /* Phrase search */
            ret = ngram_parse(param, reinterpret_cast<char *>(word.pos),
                              word.len, &bool_info);
          } else {
            /* Term search */
            ret = ngram_term_convert(param, reinterpret_cast<char *>(word.pos),
                                     word.len, &bool_info);
            assert(bool_info.quot == nullptr);
            assert(bool_info.type == FT_TOKEN_WORD);
          }
        } else {
          ret = param->mysql_add_word(param, reinterpret_cast<char *>(word.pos),
                                      word.len, &bool_info);
        }

        RETURN_IF_ERROR(ret);
      }

      break;
  }

  return (ret);
}

/** Fulltext ngram parser */
static struct st_mysql_ftparser ngram_parser_descriptor = {
    MYSQL_FTPARSER_INTERFACE_VERSION, ngram_parser_parse, nullptr, nullptr};

static MYSQL_SYSVAR_INT(
    token_size, ngram_token_size, PLUGIN_VAR_READONLY,
    "InnoDB ngram full text plugin parser token size in characters", nullptr,
    nullptr, 2, 1, 10, 0);

/** Ngram plugin system variables */
static SYS_VAR *ngram_system_variables[] = {MYSQL_SYSVAR(token_size), nullptr};

/** Ngram plugin descriptor */
mysql_declare_plugin(ngram_parser){
    MYSQL_FTPARSER_PLUGIN,    /*!< type	*/
    &ngram_parser_descriptor, /*!< descriptor	*/
    "ngram",                  /*!< name	*/
    PLUGIN_AUTHOR_ORACLE,     /*!< author	*/
    "Ngram Full-Text Parser", /*!< description*/
    PLUGIN_LICENSE_GPL,
    nullptr,                /*!< init function (when loaded)*/
    nullptr,                /*!< check uninstall function*/
    nullptr,                /*!< deinit function (when unloaded)*/
    0x0001,                 /*!< version	*/
    nullptr,                /*!< status variables	*/
    ngram_system_variables, /*!< system variables	*/
    nullptr,
    0,
} mysql_declare_plugin_end;
