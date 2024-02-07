/*
   Copyright (c) 2015, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "mysql/strings/m_ctype.h"
#include "sql_chars.h"

static void hint_lex_init_maps(CHARSET_INFO *cs,
                               enum hint_lex_char_classes *hint_map) {
  size_t i;
  for (i = 0; i < 256; i++) {
    if (my_ismb1st(cs, i))
      hint_map[i] = HINT_CHR_MB;
    else if (my_isalpha(cs, i))
      hint_map[i] = HINT_CHR_IDENT;
    else if (my_isdigit(cs, i))
      hint_map[i] = HINT_CHR_DIGIT;
    else if (my_isspace(cs, i)) {
      assert(!my_ismb1st(cs, i));
      hint_map[i] = HINT_CHR_SPACE;
    } else
      hint_map[i] = HINT_CHR_CHAR;
  }
  hint_map[u'*'] = HINT_CHR_ASTERISK;
  hint_map[u'@'] = HINT_CHR_AT;
  hint_map[u'`'] = HINT_CHR_BACKQUOTE;
  hint_map[u'.'] = HINT_CHR_DOT;
  hint_map[u'"'] = HINT_CHR_DOUBLEQUOTE;
  hint_map[u'$'] = HINT_CHR_IDENT;
  hint_map[u'_'] = HINT_CHR_IDENT;
  hint_map[u'\n'] = HINT_CHR_NL;
  hint_map[u'\''] = HINT_CHR_QUOTE;
  hint_map[u'/'] = HINT_CHR_SLASH;
}

bool init_state_maps(MY_CHARSET_LOADER *loader, CHARSET_INFO *cs) {
  uint8_t *ident_map = nullptr;
  enum my_lex_states *state_map = nullptr;

  // This character set has already been initialized.
  if (cs->state_maps != nullptr && cs->ident_map != nullptr) return false;

  lex_state_maps_st *lex_state_maps = static_cast<lex_state_maps_st *>(
      loader->once_alloc(sizeof(lex_state_maps_st)));

  if (lex_state_maps == nullptr) return true;  // OOM

  cs->state_maps = lex_state_maps;
  state_map = lex_state_maps->main_map;

  if (!(cs->ident_map = ident_map =
            static_cast<uint8_t *>(loader->once_alloc(256))))
    return true;  // OOM

  hint_lex_init_maps(cs, lex_state_maps->hint_map);

  /* Fill state_map with states to get a faster parser */
  for (unsigned i = 0; i < 256; i++) {
    if (my_isalpha(cs, i))
      state_map[i] = MY_LEX_IDENT;
    else if (my_isdigit(cs, i))
      state_map[i] = MY_LEX_NUMBER_IDENT;
    else if (my_ismb1st(cs, i))
      /* To get whether it's a possible leading byte for a charset. */
      state_map[i] = MY_LEX_IDENT;
    else if (my_isspace(cs, i))
      state_map[i] = MY_LEX_SKIP;
    else
      state_map[i] = MY_LEX_CHAR;
  }
  state_map[u'_'] = state_map[u'$'] = MY_LEX_IDENT;
  state_map[u'\''] = MY_LEX_STRING;
  state_map[u'.'] = MY_LEX_REAL_OR_POINT;
  state_map[u'>'] = state_map[u'='] = state_map[u'!'] = MY_LEX_CMP_OP;
  state_map[u'<'] = MY_LEX_LONG_CMP_OP;
  state_map[u'&'] = state_map[u'|'] = MY_LEX_BOOL;
  state_map[u'#'] = MY_LEX_COMMENT;
  state_map[u';'] = MY_LEX_SEMICOLON;
  state_map[u':'] = MY_LEX_SET_VAR;
  state_map[0] = MY_LEX_EOL;
  state_map[u'/'] = MY_LEX_LONG_COMMENT;
  state_map[u'*'] = MY_LEX_END_LONG_COMMENT;
  state_map[u'@'] = MY_LEX_USER_END;
  state_map[u'`'] = MY_LEX_USER_VARIABLE_DELIMITER;
  state_map[u'"'] = MY_LEX_STRING_OR_DELIMITER;

  /*
    Create a second map to make it faster to find identifiers
  */
  for (unsigned i = 0; i < 256; i++) {
    ident_map[i] = static_cast<uint8_t>(state_map[i] == MY_LEX_IDENT ||
                                        state_map[i] == MY_LEX_NUMBER_IDENT);
  }

  /* Special handling of hex and binary strings */
  state_map[u'x'] = state_map[u'X'] = MY_LEX_IDENT_OR_HEX;
  state_map[u'b'] = state_map[u'B'] = MY_LEX_IDENT_OR_BIN;
  state_map[u'n'] = state_map[u'N'] = MY_LEX_IDENT_OR_NCHAR;

  /* Special handling of '$' for dollar quoted strings */
  state_map[u'$'] = MY_LEX_IDENT_OR_DOLLAR_QUOTED_TEXT;

  return false;
}
