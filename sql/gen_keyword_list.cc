/*
   Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include <fstream>
#include <iostream>
#include <map>
#include <set>

#include "extra/regex/my_regex.h"       // my_regex_t
#include "lex.h"                        // symbols[]
#include "m_ctype.h"                    // CHARSET_INFO
#include "welcome_copyright_notice.h"   // ORACLE_WELCOME_COPYRIGHT_NOTICE


int main(int argc, const char *argv[])
{
  if (argc != 2)
  {
    fprintf(stderr, "Usage: %s <YACC file>\n", argv[0]);
    return EXIT_FAILURE;
  }

  const char *yacc_filename= argv[1];

  std::ifstream yacc(yacc_filename);
  if (!yacc.is_open())
  {
    fprintf(stderr, "Failed to open \"%s\"",  yacc_filename);
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
  size_t token_num= 257;
  while (getline(yacc, s))
  {
    if (!my_regexec(&rx, s.c_str(), sizeof(match) / sizeof(*match), match, 0))
    {
      token_num++;
      const char *semantic_type= s.c_str() + match[2].rm_so;
      const size_t semantic_type_sz= match[2].rm_eo - match[2].rm_so;
      if (semantic_type_sz != 0 &&
          strncmp(semantic_type, "<keyword>", semantic_type_sz) == 0)
        keyword_tokens.insert(token_num);
    }
  }

  my_regfree(&rx);

  std::map<std::string, bool> words;

  for (size_t i= 0; i < array_elements(symbols); i++)
  {
    const SYMBOL *sym= &symbols[i];

    if (sym->group != SG_KEYWORDS && sym->group != SG_HINTABLE_KEYWORDS)
      continue; // Function or optimizer hint name.

    if (!isalpha(sym->name[0]))
      continue; // Operator.

    bool is_reserved= keyword_tokens.count(sym->tok) == 0;
    if (!words.insert(std::make_pair(sym->name, is_reserved)).second)
    {
      fprintf(stderr, "This should not happen: \"%s\" has duplicates."
              " See symbols[] in lex.h", sym->name);
      DBUG_ASSERT(false);
      return EXIT_FAILURE;
    }
  }

  auto &out= std::cout;

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
