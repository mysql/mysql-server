/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

/*
  This file is used to dump DUCET 9.0.0 to table we use in MySQL collations.
  It is created on the basis of uca-dump.cc file for 5.2.0. It is changed to
  dump all 3 levels into one table.
  How to use:
    1. g++ uca9-dump.cc -o ucadump
    2. ucadump < /path/to/allkeys.txt > /path/to/youfile

  This can also be used to dump weight table of Japanese Han characters.
  How to use:
    1. Copy the line of Han characters in CLDR file ja.xml to a seperate file,
       e.g. ja_han.txt.
    2. Make sure the file is saved in UTF-8 (use 'file' command to check), or
       use iconv to convert.
    3. ucadump ja < /path/to/ja_han.txt > /path/to/yourfile
*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "m_string.h"

typedef unsigned char uchar;
typedef unsigned short uint16;
typedef unsigned int uint;
typedef unsigned long my_wc_t;

#define MY_UCA_MAXWEIGHT_TO_PARSE 64
#define MY_UCA_MAXCE_TO_PARSE 18
#define MY_UCA_MAXWEIGHT_TO_DUMP 24
#define MY_UCA_MAXCE_TO_DUMP 8
#define MY_UCA_VERSION_SIZE 32
#define MY_UCA_CE_SIZE 3
#define MY_UCA_MAX_CONTRACTION 6

#define MY_UCA_MAXCHAR (0x10FFFF + 1)
#define MY_UCA_CHARS_PER_PAGE 256
#define MY_UCA_PSHIFT 8
#define MY_UCA_NPAGES MY_UCA_MAXCHAR / MY_UCA_CHARS_PER_PAGE

struct MY_UCA_ITEM {
  int num_of_ce; /* Number of collation elements */
  uint16 weight[MY_UCA_MAXWEIGHT_TO_DUMP + 1];
  /* +1 for trailing num_of_ce */
};

struct MY_UCA {
  char version[MY_UCA_VERSION_SIZE];
  MY_UCA_ITEM item[MY_UCA_MAXCHAR];  // Weight info of all characters
};

static int load_uca_file(MY_UCA *uca, int maxchar, int *pageloaded) {
  char str[512];
  int out_of_range_chars = 0;

  for (int lineno = 0; fgets(str, sizeof(str), stdin); lineno++) {
    /* Skip comment lines */
    if (*str == '\r' || *str == '\n' || *str == '#') continue;

    /* Detect version */
    if (*str == '@') {
      if (!strncmp(str, "@version ", 9)) {
        const char *value;
        if (strtok(str, " \r\n\t") && (value = strtok(nullptr, " \r\n\t")))
          snprintf(uca->version, MY_UCA_VERSION_SIZE, "%s", value);
      }
      continue;
    }

    int code;
    /* Skip big characters */
    if ((code = strtol(str, nullptr, 16)) > maxchar) {
      out_of_range_chars++;
      continue;
    }

    char *comment;
    if (!(comment = strchr(str, '#'))) {
      fprintf(stderr, "Warning: could not parse line #%d:\n'%s'\n", lineno,
              str);
      continue;
    }
    *comment = '\0';

    char *weight;
    if ((weight = strchr(str, ';'))) {
      *weight++ = '\0';
      weight += strspn(weight, " ");
    } else {
      fprintf(stderr, "Warning: could not parse line #%d:\n%s\n", lineno, str);
      continue;
    }

    char *s;
    int codenum;
    for (codenum = 0, s = strtok(str, " \t"); s;
         codenum++, s = strtok(nullptr, " \t")) {
      /* Meet a contraction. To handle in the future. */
      if (codenum >= 1) {
        codenum++;
        break;
      }
    }

    MY_UCA_ITEM *item = nullptr;
    if (codenum > 1) {
      /* Contractions we don't support. */
      continue;
    } else {
      item = &uca->item[code];
    }

    /*
      Split weight string into separate weights

      "[p1.s1.t1.q1][p2.s2.t2.q2][p3.s3.t3.q3]" ->

      "p1.s1.t1.q1" "p2.s2.t2.q2" "p3.s3.t3.q3"
    */
    item->num_of_ce = 0;
    s = strtok(weight, " []");
    char *weights[MY_UCA_MAXWEIGHT_TO_PARSE];
    while (s) {
      if (item->num_of_ce >= MY_UCA_MAXCE_TO_PARSE) {
        fprintf(stderr, "Line #%d has more than %d collation elements\n",
                lineno, MY_UCA_MAXCE_TO_PARSE);
        fprintf(stderr, "Can't continue.\n");
        exit(1);
      }
      weights[item->num_of_ce] = s;
      s = strtok(nullptr, " []");
      item->num_of_ce++;
    }

    for (int i = 0; i < item->num_of_ce; i++) {
      /*
        The longest collation element in DUCET is assigned to 0xFDFA. It
        has 18 collation elements. The second longest is 8. Because 8
        collation elements is enough to distict 0xFDFA from other
        characters, we skip the extra weights and only use 8 here.
      */
      if (i >= MY_UCA_MAXCE_TO_DUMP) {
        fprintf(stderr,
                "Warning: at line %d: character %04X has"
                " more than %d collation elements (%d). "
                "Skipping the extra weights.\n",
                lineno, code, MY_UCA_MAXCE_TO_DUMP, item->num_of_ce);
        item->num_of_ce = MY_UCA_MAXCE_TO_DUMP;
        break;
      }

      int weight_of_ce = 0;
      for (s = weights[i]; *s;) {
        char *endptr;
        int part = strtol(s + 1, &endptr, 16);
        if (i < MY_UCA_MAXCE_TO_DUMP) {
          item->weight[i * MY_UCA_CE_SIZE + weight_of_ce] = part;
        } else {
          fprintf(stderr, "Too many weights (%d) at line %d\n", i, lineno);
          exit(1);
        }
        s = endptr;
        weight_of_ce++;
      }
    }
    /* Mark that a character from this page was loaded */
    pageloaded[code >> MY_UCA_PSHIFT]++;
  }

  if (out_of_range_chars)
    fprintf(stderr, "%d out-of-range characters skipped\n", out_of_range_chars);

  return 0;
}

#define HANGUL_JAMO_MAX_LENGTH 3

static int my_decompose_hangul_syllable(my_wc_t syllable, my_wc_t *jamo) {
  if (syllable < 0xAC00 || syllable > 0xD7AF) return 0;
  constexpr int syllable_base = 0xAC00;
  constexpr int leadingjamo_base = 0x1100;
  constexpr int voweljamo_base = 0x1161;
  constexpr int trailingjamo_base = 0x11A7;
  constexpr int voweljamo_cnt = 21;
  constexpr int trailingjamo_cnt = 28;
  int syllable_index = syllable - syllable_base;
  int v_t_combination = voweljamo_cnt * trailingjamo_cnt;
  int leadingjamo_index = syllable_index / v_t_combination;
  int voweljamo_index = (syllable_index % v_t_combination) / trailingjamo_cnt;
  int trailingjamo_index = syllable_index % trailingjamo_cnt;
  jamo[0] = leadingjamo_base + leadingjamo_index;
  jamo[1] = voweljamo_base + voweljamo_index;
  jamo[2] = trailingjamo_index ? (trailingjamo_base + trailingjamo_index) : 0;
  return trailingjamo_index ? 3 : 2;
}

void my_put_jamo_weights(const my_wc_t *hangul_jamo, int jamo_cnt,
                         MY_UCA_ITEM *item, const MY_UCA *uca) {
  for (int jamoind = 0; jamoind < jamo_cnt; jamoind++) {
    uint16 *implicit_weight = item->weight + jamoind * MY_UCA_CE_SIZE;
    const uint16 *jamo_weight = uca->item[hangul_jamo[jamoind]].weight;
    *implicit_weight = *jamo_weight;
    *(implicit_weight + 1) = *(jamo_weight + 1);
    *(implicit_weight + 2) = *(jamo_weight + 2) + 1;
  }
  item->num_of_ce = jamo_cnt;
}

static void set_implicit_weights(MY_UCA_ITEM *item, int code) {
  int base, aaaa, bbbb;
  if (code >= 0x17000 && code <= 0x18AFF)  // Tangut character
  {
    aaaa = 0xFB00;
    bbbb = (code - 0x17000) | 0x8000;
  } else {
    /* non-Core Han Unified Ideographs */
    if ((code >= 0x3400 && code <= 0x4DB5) ||
        (code >= 0x20000 && code <= 0x2A6D6) ||
        (code >= 0x2A700 && code <= 0x2B734) ||
        (code >= 0x2B740 && code <= 0x2B81D) ||
        (code >= 0x2B820 && code <= 0x2CEA1))
      base = 0xFB80;
    /* Core Han Unified Ideographs */
    else if ((code >= 0x4E00 && code <= 0x9FD5) ||
             (code >= 0xFA0E && code <= 0xFA29))
      base = 0xFB40;
    /* All other characters whose weight is unassigned */
    else
      base = 0xFBC0;
    aaaa = base + (code >> 15);
    bbbb = (code & 0x7FFF) | 0x8000;
  }

  item->weight[0] = aaaa;
  item->weight[1] = 0x0020;
  item->weight[2] = 0x0002;
  item->weight[3] = bbbb;
  item->weight[4] = 0x0000;
  item->weight[5] = 0x0000;

  item->num_of_ce = 2;
}
/*
  We need to initialize implicit weights because
  some pages have both implicit and explicit weights:
  0x4D??, 0x9F??
*/
static void set_implicit_weights(MY_UCA *uca, const int *pageloaded) {
  for (int page = 0; page < MY_UCA_NPAGES; page++) {
    if (pageloaded[page] == MY_UCA_CHARS_PER_PAGE) continue;
    /* Now set implicit weights */
    for (int code = page * MY_UCA_CHARS_PER_PAGE;
         code < (page + 1) * MY_UCA_CHARS_PER_PAGE; code++) {
      MY_UCA_ITEM *item = &uca->item[code];

      if (item->num_of_ce) continue;

      int jamo_cnt = 0;
      my_wc_t hangul_jamo[HANGUL_JAMO_MAX_LENGTH];
      if ((jamo_cnt = my_decompose_hangul_syllable(code, hangul_jamo))) {
        my_put_jamo_weights(hangul_jamo, jamo_cnt, item, uca);
        continue;
      }

      set_implicit_weights(item, code);
    }
  }
}

static void get_page_statistics(const MY_UCA *uca, int page, int *maxnum) {
  for (int offs = 0; offs < MY_UCA_CHARS_PER_PAGE; offs++) {
    const MY_UCA_ITEM *item = &uca->item[page * MY_UCA_CHARS_PER_PAGE + offs];

    *maxnum = *maxnum < item->num_of_ce ? item->num_of_ce : *maxnum;
  }
}

/*
  Compose the prefix name of weight tables from the version number.
*/
static char *prefix_name(const MY_UCA *uca) {
  static char prefix[MY_UCA_VERSION_SIZE];
  const char *s;
  char *d;
  strcpy(prefix, "uca");
  for (s = uca->version, d = prefix + strlen(prefix); *s; s++) {
    if ((*s >= '0' && *s <= '9') || (*s >= 'a' && *s <= 'z')) *d++ = *s;
  }
  *d = '\0';
  return prefix;
}

static char *page_name(const MY_UCA *uca, int page, bool pageloaded) {
  static char page_name_buf[120];
  static char page_name_null[] = "NULL";

  if (pageloaded) {
    snprintf(page_name_buf, sizeof(page_name_buf), "%s_p%03X", prefix_name(uca),
             page);
    return page_name_buf;
  } else
    return page_name_null;
}

static void print_one_page(const MY_UCA *uca, int page, int maxnum) {
  printf("uint16 %s[]= {\n", page_name(uca, page, true));

  printf("  /* Number of CEs for each character. */\n");
  for (int offs = 0; offs < MY_UCA_CHARS_PER_PAGE; ++offs) {
    const int code = page * MY_UCA_CHARS_PER_PAGE + offs;
    const MY_UCA_ITEM *item = &uca->item[code];
    if ((offs % 16) == 0) printf("  ");
    printf("%d, ", item->num_of_ce);
    if ((offs % 16) == 15) printf("\n");
  }

  for (int i = 0; i < maxnum - 1; i++) {
    printf("\n");
    if ((i % 3) == 0) {
      printf("  /* Primary weight %d for each character. */\n", i / 3 + 1);
    } else if ((i % 3) == 1) {
      printf("  /* Secondary weight %d for each character. */\n", i / 3 + 1);
    } else {
      printf("  /* Tertiary weight %d for each character. */\n", i / 3 + 1);
    }
    for (int offs = 0; offs < MY_UCA_CHARS_PER_PAGE; offs++) {
      const int code = page * MY_UCA_CHARS_PER_PAGE + offs;
      const MY_UCA_ITEM *item = &uca->item[code];
      const uint16 *weight = item->weight;
      printf("  0x%04X,   /* U+%04X */\n", weight[i], code);
    }
  }
  printf("};\n\n");
}

/*
  This is a very simple conversion from utf8 to wide char, assuming
  the input is always legal 3 bytes encoded Japanese Han character.
  Because our input is from the CLDR file, ja.xml, we believe there
  is no problem.
*/
static void ja_han_u8_to_wc(const unsigned char *s, int *pwc) {
  assert(((s[0] == 0xE4 && s[1] >= 0xB8) ||
          (s[0] >= 0xE5 && s[0] <= 0xE9 && s[1] >= 0x80)) &&
         s[1] <= 0xBF && s[2] >= 0x80 && s[2] <= 0xBF);
  *pwc = ((my_wc_t)(s[0] & 0x0f) << 12) + ((my_wc_t)(s[1] & 0x3f) << 6) +
         (my_wc_t)(s[2] & 0x3f);
}

int dump_ja_hans() {
  // There are 6355 Japanese Han characters.
  unsigned char ja_u8_bytes[8000 * 3] = {0};
  // There are 20992 characters in range [4E00, 9FFF].
  MY_UCA_ITEM ja_han_items[25600];
  if (!fgets((char *)ja_u8_bytes, sizeof(ja_u8_bytes), stdin)) {
    fprintf(stderr, "Could not read Japanese Han characters.\n");
    return 1;
  }
  int ja_length = strlen((char *)ja_u8_bytes);
  if (ja_u8_bytes[ja_length - 1] == '\n') {
    ja_u8_bytes[ja_length - 1] = '\0';
    ja_length--;
  }
  // All these Japanese Han characters should be 3 bytes.
  if ((ja_length % 3)) {
    fprintf(stderr, "Wrong UTF8 Han character bytes.\n");
    return 1;
  }
  int han_cnt = ja_length / 3;
  const int JA_CORE_HAN_BASE_WT = 0x54A4;
  const int ja_han_page_cnt = 0x9F - 0x4E + 1;
  // Set weight for Japanese Han characters.
  unsigned char *ja_han = ja_u8_bytes;
  for (int i = 0; i < han_cnt; i++) {
    int ja_ch_u16 = 0;
    ja_han_u8_to_wc(ja_han, &ja_ch_u16);
    ja_han += 3;
    int page = ja_ch_u16 >> 8;
    assert(page >= 0x4E && page <= 0x9F);
    MY_UCA_ITEM *item = &ja_han_items[ja_ch_u16 - 0x4E00];
    item->num_of_ce = 1;
    item->weight[0] = JA_CORE_HAN_BASE_WT + i;
    item->weight[1] = 0x20;
    item->weight[2] = 0x02;
  }

  // Set implicit weight for non-Japanese characters.
  for (int page = 0x4E; page <= 0x9F; page++) {
    for (int offs = 0; offs < MY_UCA_CHARS_PER_PAGE; ++offs) {
      int code = (page << 8) + offs;
      int ind = code - 0x4E00;
      MY_UCA_ITEM *item = &ja_han_items[ind];
      if (item->num_of_ce == 0) set_implicit_weights(item, code);
    }
  }

  // Print weights.
  for (int page = 0; page < ja_han_page_cnt; page++) {
    printf("uint16 ja_han_page%2X[]= {\n", 0x4E + page);
    printf("  /* Number of CEs for each character. */\n");
    for (int offs = 0; offs < MY_UCA_CHARS_PER_PAGE; ++offs) {
      int ind = (page << 8) + offs;
      MY_UCA_ITEM *item = &ja_han_items[ind];
      if ((offs % 16) == 0) printf("  ");
      printf("%d, ", item->num_of_ce);
      if ((offs % 16) == 15) printf("\n");
    }
    for (int i = 0; i < 6; i++) {
      printf("\n");
      if ((i % 3) == 0) {
        printf("  /* Primary weight %d for each character. */\n", i / 3 + 1);
      } else if ((i % 3) == 1) {
        printf("  /* Secondary weight %d for each character. */\n", i / 3 + 1);
      } else {
        printf("  /* Tertiary weight %d for each character. */\n", i / 3 + 1);
      }
      for (int offs = 0; offs < MY_UCA_CHARS_PER_PAGE; offs++) {
        const int ind = page * MY_UCA_CHARS_PER_PAGE + offs;
        const int code = (page + 0x4E) * MY_UCA_CHARS_PER_PAGE + offs;
        const MY_UCA_ITEM *item = &ja_han_items[ind];
        const uint16 *weight = item->weight;
        printf("  0x%04X,   /* U+%04X */\n", weight[i], code);
      }
    }
    printf("};\n\n");
  }
  /* Print page index */
  printf("uint16* ja_han_pages[%d]= {\n", ja_han_page_cnt);
  for (int page = 0; page < ja_han_page_cnt; page++) {
    if (!(page % 5))
      printf("%13s%2X", "ja_han_page", page + 0x4E);
    else
      printf("%12s%2X", "ja_han_page", page + 0x4E);
    if ((page + 1) != ja_han_page_cnt) printf(",");
    if (!((page + 1) % 5) || (page + 1) == ja_han_page_cnt) printf("\n");
  }
  printf("};\n\n");

  return 0;
}

int main(int ac, char **av) {
  if (ac == 2 && !native_strcasecmp(av[1], "ja")) return dump_ja_hans();
  static MY_UCA uca;
  int maxchar = MY_UCA_MAXCHAR;
  static int pageloaded[MY_UCA_NPAGES];

  memset(&uca, 0, sizeof(uca));

  memset(pageloaded, 0, sizeof(pageloaded));

  load_uca_file(&uca, maxchar, pageloaded);

  set_implicit_weights(&uca, pageloaded);

  int pagemaxlen[MY_UCA_NPAGES];

  for (int page = 0; page < MY_UCA_NPAGES; page++) {
    int maxnum = 0;

    pagemaxlen[page] = 0;

    /* Skip this page if no weights were loaded */
    if (!pageloaded[page]) continue;

    /*
      Calculate number of weights per character
      and number of default weights.
    */
    get_page_statistics(&uca, page, &maxnum);

    maxnum = maxnum * MY_UCA_CE_SIZE + 1;

    pagemaxlen[page] = maxnum;

    print_one_page(&uca, page, maxnum);
  }

  /* Print page index */
  printf("uint16* %s_weight[%d]= {\n", prefix_name(&uca), MY_UCA_NPAGES);
  for (int page = 0; page < MY_UCA_NPAGES; page++) {
    if (!(page % 6))
      printf("%13s", page_name(&uca, page, pagemaxlen[page]));
    else
      printf("%12s", page_name(&uca, page, pagemaxlen[page]));
    if ((page + 1) != MY_UCA_NPAGES) printf(",");
    if (!((page + 1) % 6) || (page + 1) == MY_UCA_NPAGES) printf("\n");
  }
  printf("};\n\n");

  return 0;
}
