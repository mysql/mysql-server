/*
   Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <fstream>
#include <iostream>
#include <map>
#include <set>

#include "extra/regex/my_regex.h"  // my_regex_t
#include "lex.h"                   // symbols[]
#include "m_ctype.h"               // CHARSET_INFO
#include "my_dbug.h"
#include "welcome_copyright_notice.h"  // ORACLE_WELCOME_COPYRIGHT_NOTICE

int main(int argc, const char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <YACC file>\n", argv[0]);
    return EXIT_FAILURE;
  }

  const char *yacc_filename = argv[1];

  std::ifstream yacc(yacc_filename);
  if (!yacc.is_open()) {
    fprintf(stderr, "Failed to open \"%s\"", yacc_filename);
    return EXIT_FAILURE;
  }

  my_regex_t rx;
  my_regmatch_t match[4];
  if (my_regcomp(&rx,
                 "^%(token|left|right|nonassoc)[[:space:]]*"
                 "(<[_[:alnum:]]+>)?[[:space:]]*([_[:alnum:]]+)",
                 MY_REG_EXTENDED, &my_charset_utf8_general_ci))
    return EXIT_FAILURE;

  std::set<size_t> keyword_tokens;
  std::string s;
  size_t token_num = 257;
  while (getline(yacc, s)) {
    if (!my_regexec(&rx, s.c_str(), sizeof(match) / sizeof(*match), match, 0)) {
      token_num++;
      const char *semantic_type = s.c_str() + match[2].rm_so;
      const size_t semantic_type_sz = match[2].rm_eo - match[2].rm_so;
      if (semantic_type_sz != 0 &&
          strncmp(semantic_type, "<keyword>", semantic_type_sz) == 0)
        keyword_tokens.insert(token_num);
    }
  }

  my_regfree(&rx);

  std::map<std::string, bool> words;

  for (size_t i = 0; i < array_elements(symbols); i++) {
    const SYMBOL *sym = &symbols[i];

    if (sym->group != SG_KEYWORDS && sym->group != SG_HINTABLE_KEYWORDS)
      continue;  // Function or optimizer hint name.

    if (!isalpha(sym->name[0])) continue;  // Operator.

    bool is_reserved = keyword_tokens.count(sym->tok) == 0;
    if (!words.insert(std::make_pair(sym->name, is_reserved)).second) {
      fprintf(stderr,
              "This should not happen: \"%s\" has duplicates."
              " See symbols[] in lex.h",
              sym->name);
      DBUG_ASSERT(false);
      return EXIT_FAILURE;
    }
  }

  auto &out = std::cout;

  out << ORACLE_GPL_COPYRIGHT_NOTICE("2017") << std::endl;

  out << "#ifndef GEN_KEYWORD_LIST_H__INCLUDED\n";
  out << "#define GEN_KEYWORD_LIST_H__INCLUDED\n\n";
  out << "/*\n";
  out << "  This file is generated, do not edit.\n";
  out << "  See file sql/gen_keyword_list.cc.\n";
  out << "*/\n\n";

  out << "typedef struct { const char *word; int reserved; } keyword_t;\n\n";

  out << "static const keyword_t keyword_list[]= {\n";
  for (auto p : words)
    out << "  { \"" << p.first << "\", " << (p.second ? 1 : 0) << " },\n";
  out << "};/*keyword_list*/\n\n";

  out << "#endif/*GEN_KEYWORD_LIST_H__INCLUDED*/\n";

  my_regex_end();

  return 0;
}
