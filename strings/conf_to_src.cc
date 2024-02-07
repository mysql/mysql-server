/* Copyright (c) 2000, 2024, Oracle and/or its affiliates.

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

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>

#include "mysql/my_loglevel.h"
#include "mysql/strings/collations.h"
#include "mysql/strings/m_ctype.h"
#include "strings/collations_internal.h"
#include "strings/m_ctype_internals.h"
#include "welcome_copyright_notice.h"

constexpr int ROW_LEN = 16;
constexpr int ROW16_LEN = 8;
constexpr int MAX_BUF = 64 * 1024;

static void print_array(FILE *f, const char *set, const char *name,
                        const uint8_t *a, int n) {
  fprintf(f, "static const uint8_t %s_%s[] = {\n", name, set);

  for (int i = 0; i < n; i++) {
    fprintf(f, "0x%02X", a[i]);
    fprintf(f, (i + 1 < n) ? "," : "");
    fprintf(f, ((i + 1) % ROW_LEN == n % ROW_LEN) ? "\n" : "");
  }
  fprintf(f, "};\n\n");
}

static void print_array16(FILE *f, const char *set, const char *name,
                          const uint16_t *a, int n) {
  fprintf(f, "static const uint16_t %s_%s[] = {\n", name, set);

  for (int i = 0; i < n; i++) {
    fprintf(f, "0x%04X", a[i]);
    fprintf(f, (i + 1 < n) ? "," : "");
    fprintf(f, ((i + 1) % ROW16_LEN == n % ROW16_LEN) ? "\n" : "");
  }
  fprintf(f, "};\n\n");
}

static bool simple_cs_is_full(const CHARSET_INFO *cs) {
  return ((cs->csname && cs->tab_to_uni && cs->ctype && cs->to_upper &&
           cs->to_lower) &&
          (cs->number && cs->m_coll_name &&
           (cs->sort_order || (cs->state & MY_CS_BINSORT))));
}

static char buf[MAX_BUF];

class Loader : public MY_CHARSET_LOADER {
 public:
  void reporter(loglevel, unsigned /* errcode */, ...) override {}
  void *read_file(const char *path, size_t *size) override;
};

void *Loader::read_file(const char *path, size_t *size) {
  buf[0] = '\0';
  static bool first_call = true;
  if (!first_call) {
    return buf;
  }
  first_call = false;

  FILE *fd = fopen(path, "rb");
  if (fd == nullptr) {
    fprintf(stderr, "Can't open '%s'\n", path);
    return nullptr;
  }

  unsigned len = fread(buf, 1, sizeof(buf), fd);
  fclose(fd);

  *size = len;
  // Caller will free() result, so strdup our static buffer.
  return strdup(buf);
}

static int is_case_sensitive(const CHARSET_INFO *cs) {
  return (cs->sort_order &&
          cs->sort_order[static_cast<int>('A')] <
              cs->sort_order[static_cast<int>('a')] &&
          cs->sort_order[static_cast<int>('a')] <
              cs->sort_order[static_cast<int>('B')])
             ? 1
             : 0;
}

static void dispcset(FILE *f, const CHARSET_INFO *cs) {
  fprintf(f, "{\n");
  fprintf(f, "  %d,%d,%d,\n", cs->number, 0, 0);
  fprintf(f, "  MY_CS_COMPILED%s%s%s%s%s,\n",
          cs->state & MY_CS_BINSORT ? "|MY_CS_BINSORT" : "",
          cs->state & MY_CS_PRIMARY ? "|MY_CS_PRIMARY" : "",
          is_case_sensitive(cs) ? "|MY_CS_CSSORT" : "",
          my_charset_is_8bit_pure_ascii(cs) ? "|MY_CS_PUREASCII" : "",
          !my_charset_is_ascii_compatible(cs) ? "|MY_CS_NONASCII" : "");

  if (cs->m_coll_name) {
    fprintf(f, "  \"%s\",                     /* csname */\n", cs->csname);
    fprintf(f, "  \"%s\",                    /* m_collname */\n",
            cs->m_coll_name);
    if (cs->comment) {
      fprintf(f, "  \"%s\",                   /* comment */\n", cs->comment);
    } else {
      fprintf(f, "  \"\",                     /* comment */\n");
    }
    fprintf(f, "  nullptr,                    /* tailoring */\n");
    fprintf(f, "  nullptr,                    /* coll_param */\n");
    fprintf(f, "  ctype_%s,                   /* ctype         */\n",
            cs->m_coll_name);
    fprintf(f, "  to_lower_%s,                /* to_lower */\n",
            cs->m_coll_name);
    fprintf(f, "  to_upper_%s,                /* to_upper */\n",
            cs->m_coll_name);
    if (cs->sort_order)
      fprintf(f, "  sort_order_%s,            /* sort_order */\n",
              cs->m_coll_name);
    else
      fprintf(f, "  nullptr,                     /* sort_order */\n");

    fprintf(f, "  nullptr,                    /* uca */\n");
    fprintf(f, "  to_uni_%s,                  /* to_uni        */\n",
            cs->m_coll_name);
  } else {
    fprintf(f, "  nullptr,                    /* cset name     */\n");
    fprintf(f, "  nullptr,                    /* coll name     */\n");
    fprintf(f, "  nullptr,                    /* comment       */\n");
    fprintf(f, "  nullptr,                    /* tailoring     */\n");
    fprintf(f, "  nullptr,                    /* coll_param    */\n");
    fprintf(f, "  nullptr,                    /* ctype         */\n");
    fprintf(f, "  nullptr,                    /* lower         */\n");
    fprintf(f, "  nullptr,                    /* upper         */\n");
    fprintf(f, "  nullptr,                    /* sort order    */\n");
    fprintf(f, "  nullptr,                    /* uca           */\n");
    fprintf(f, "  nullptr,                    /* to_uni        */\n");
  }

  fprintf(f, "  nullptr,                    /* from_uni         */\n");
  fprintf(f, "  &my_unicase_default,        /* caseinfo         */\n");
  fprintf(f, "  nullptr,                    /* state map        */\n");
  fprintf(f, "  nullptr,                    /* ident map        */\n");
  fprintf(f, "  1,                          /* strxfrm_multiply */\n");
  fprintf(f, "  1,                          /* caseup_multiply  */\n");
  fprintf(f, "  1,                          /* casedn_multiply  */\n");
  fprintf(f, "  1,                          /* mbminlen         */\n");
  fprintf(f, "  1,                          /* mbmaxlen         */\n");
  fprintf(f, "  1,                          /* mbmaxlenlen      */\n");
  fprintf(f, "  0,                          /* min_sort_char    */\n");
  fprintf(f, "  255,                        /* max_sort_char    */\n");
  fprintf(f, "  ' ',                        /* pad_char         */\n");
  fprintf(f,
          "  false,                      /* escape_with_backslash_is_dangerous "
          "*/\n");
  fprintf(f, "  1,                          /* levels_for_compare */\n");

  if (my_charset_is_8bit_pure_ascii(cs))
    fprintf(f, "  &my_charset_ascii_handler,\n");
  else
    fprintf(f, "  &my_charset_8bit_handler,\n");
  if (cs->state & MY_CS_BINSORT)
    fprintf(f, "  &my_collation_8bit_bin_handler,\n");
  else
    fprintf(f, "  &my_collation_8bit_simple_ci_handler,\n");
  fprintf(f, "  PAD_SPACE                   /* pad_attribute */\n");
  fprintf(f, "}\n");
}

int main(int argc, char **argv) {
  CHARSET_INFO ncs{};
  FILE *f = stdout;

  if (argc < 2) {
    fprintf(stderr, "usage: %s source-dir\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  mysql::collation::initialize(argv[1], new Loader);

  std::deque<const CHARSET_INFO *> sorted_by_number;
  mysql::collation_internals::entry->iterate(
      [&sorted_by_number](const CHARSET_INFO *cs) {
        if (!(cs->state & MY_CS_INLINE)) {
          sorted_by_number.push_back(cs);
        }
      });
  std::sort(sorted_by_number.begin(), sorted_by_number.end(),
            [](auto a, auto b) { return a->number < b->number; });

  fprintf(f, "/*\n");
  fprintf(f,
          "  This file was generated by the conf_to_src utility. "
          "Do not edit it directly,\n");
  fprintf(f, "  edit the XML definitions in share/charsets/ instead.\n\n");
  fprintf(f,
          "  To re-generate, run the following in the build "
          "directory:\n");
  fprintf(f,
          "    ./bin/conf_to_src ${CMAKE_SOURCE_DIR}/share/charsets/ >\n"
          "    ${CMAKE_SOURCE_DIR}/strings/ctype-extra.cc\n");
  fprintf(f, "*/\n\n");
  fprintf(f, ORACLE_GPL_FOSS_COPYRIGHT_NOTICE("2003"));
  fprintf(f, "\n");
  fprintf(f, "#include <cstdint>\n\n");
  fprintf(f, "#include \"mysql/strings/m_ctype.h\"\n");
  fprintf(f, "#include \"strings/m_ctype_internals.h\"\n\n");

  fprintf(f, "/* clang-format off */\n\n");

  for (const CHARSET_INFO *cs : sorted_by_number) {
    if (cs == nullptr) continue;
    if (simple_cs_is_full(cs)) {
      print_array(f, cs->m_coll_name, "ctype", cs->ctype,
                  MY_CS_CTYPE_TABLE_SIZE);
      print_array(f, cs->m_coll_name, "to_lower", cs->to_lower,
                  MY_CS_TO_LOWER_TABLE_SIZE);
      print_array(f, cs->m_coll_name, "to_upper", cs->to_upper,
                  MY_CS_TO_UPPER_TABLE_SIZE);
      if (cs->sort_order)
        print_array(f, cs->m_coll_name, "sort_order", cs->sort_order,
                    MY_CS_SORT_ORDER_TABLE_SIZE);
      print_array16(f, cs->m_coll_name, "to_uni", cs->tab_to_uni,
                    MY_CS_TO_UNI_TABLE_SIZE);
      fprintf(f, "\n");
    }
  }

  fprintf(f, "CHARSET_INFO compiled_charsets[] = {\n");
  for (const CHARSET_INFO *cs : sorted_by_number) {
    if (cs == nullptr) continue;
    if (simple_cs_is_full(cs)) {
      dispcset(f, cs);
      fprintf(f, ",\n");
    }
  }

  dispcset(f, &ncs);
  fprintf(f, "};\n");
  mysql::collation::shutdown();

  return 0;
}
