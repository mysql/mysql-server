/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

/*
  This file is used to dump DUCET 9.0.0 to table we use in MySQL collations.
  It is created on the basis of uca-dump.cc file for 5.2.0. It is changed to
  dump all 3 levels into one table.
  How to use:
    1. g++ uca9-dump.cc -o ucadump
    2. ucadump < /path/to/allkeys.txt > /path/to/youfile
*/
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char  uchar;
typedef unsigned short uint16;
typedef unsigned int   uint;
typedef unsigned long  my_wc_t;

#define MY_UCA_MAXWEIGHT_TO_PARSE 64
#define MY_UCA_MAXCE_TO_PARSE     18
#define MY_UCA_MAXWEIGHT_TO_DUMP  24
#define MY_UCA_MAXCE_TO_DUMP      8
#define MY_UCA_VERSION_SIZE       32
#define MY_UCA_CE_SIZE            3
#define MY_UCA_MAX_CONTRACTION    6

#define MY_UCA_MAXCHAR            (0x10FFFF+1)
#define MY_UCA_CHARS_PER_PAGE     256
#define MY_UCA_PSHIFT             8
#define MY_UCA_NPAGES             MY_UCA_MAXCHAR/MY_UCA_CHARS_PER_PAGE

struct MY_UCA_ITEM
{
  int    num_of_ce; /* Number of collation elements */
  uint16 weight[MY_UCA_MAXWEIGHT_TO_DUMP + 1];
  /* +1 for trailing num_of_ce */
};

struct MY_UCA
{
  char version[MY_UCA_VERSION_SIZE];
  MY_UCA_ITEM item[MY_UCA_MAXCHAR]; // Weight info of all characters
};

static int load_uca_file(MY_UCA *uca,
                         int maxchar, int *pageloaded)
{
  char str[512];
  int out_of_range_chars= 0;

  for (int lineno= 0; fgets(str, sizeof(str), stdin); lineno++)
  {
    /* Skip comment lines */
    if (*str == '\r' || *str == '\n' || *str == '#')
      continue;

    /* Detect version */
    if (*str == '@')
    {
      if (!strncmp(str, "@version ", 9))
      {
        const char *value;
        if (strtok(str, " \r\n\t") && (value= strtok(nullptr, " \r\n\t")))
          snprintf(uca->version, MY_UCA_VERSION_SIZE, "%s", value);
      }
      continue;
    }

    int code;
    /* Skip big characters */
    if ((code= strtol(str, nullptr, 16)) > maxchar)
    {
      out_of_range_chars++;
      continue;
    }

    char *comment;
    if (!(comment= strchr(str,'#')))
    {
      fprintf(stderr, "Warning: could not parse line #%d:\n'%s'\n",
              lineno, str);
      continue;
    }
    *comment= '\0';

    char *weight;
    if ((weight= strchr(str,';')))
    {
      *weight++= '\0';
      weight+= strspn(weight, " ");
    }
    else
    {
      fprintf(stderr, "Warning: could not parse line #%d:\n%s\n", lineno, str);
      continue;
    }

    char *s;
    int codenum;
    for (codenum= 0, s= strtok(str, " \t"); s;
         codenum++, s= strtok(nullptr, " \t"))
    {
      /* Meet a contraction. To handle in the future. */
      if (codenum >= 1)
      {
        codenum++;
        break;
      }
    }

    MY_UCA_ITEM *item= nullptr;
    if (codenum > 1)
    {
      /* Contractions we don't support. */
      continue;
    }
    else
    {
      item= &uca->item[code];
    }

    /*
      Split weight string into separate weights

      "[p1.s1.t1.q1][p2.s2.t2.q2][p3.s3.t3.q3]" ->

      "p1.s1.t1.q1" "p2.s2.t2.q2" "p3.s3.t3.q3"
    */
    item->num_of_ce= 0;
    s= strtok(weight, " []");
    char *weights[MY_UCA_MAXWEIGHT_TO_PARSE];
    while (s)
    {
      if (item->num_of_ce >= MY_UCA_MAXCE_TO_PARSE)
      {
        fprintf(stderr, "Line #%d has more than %d collation elements\n",
                lineno, MY_UCA_MAXCE_TO_PARSE);
        fprintf(stderr, "Can't continue.\n");
        exit(1);
      }
      weights[item->num_of_ce]= s;
      s= strtok(nullptr, " []");
      item->num_of_ce++;
    }

    for (int i= 0; i < item->num_of_ce; i++)
    {
      /*
        The longest collation element in DUCET is assigned to 0xFDFA. It
        has 18 collation elements. The second longest is 8. Because 8
        collation elements is enough to distict 0xFDFA from other
        characters, we skip the extra weights and only use 8 here.
      */
      if (i >= MY_UCA_MAXCE_TO_DUMP)
      {
        fprintf(stderr,
                "Warning: at line %d: character %04X has"
                " more than %d collation elements (%d). "
                "Skipping the extra weights.\n",
                lineno, code, MY_UCA_MAXCE_TO_DUMP, item->num_of_ce);
        item->num_of_ce= MY_UCA_MAXCE_TO_DUMP;
        break;
      }

      int weight_of_ce= 0;
      for (s= weights[i]; *s; )
      {
        char *endptr;
        int part= strtol(s + 1, &endptr, 16);
        if (i < MY_UCA_MAXCE_TO_DUMP)
        {
          item->weight[i * MY_UCA_CE_SIZE + weight_of_ce]= part;
        }
        else
        {
          fprintf(stderr, "Too many weights (%d) at line %d\n", i, lineno);
          exit(1);
        }
        s= endptr;
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

static int
my_decompose_hangul_syllable(my_wc_t syllable, my_wc_t* jamo)
{
  if (syllable < 0xAC00 || syllable > 0xD7AF)
    return 0;
  constexpr int syllable_base= 0xAC00;
  constexpr int leadingjamo_base= 0x1100;
  constexpr int voweljamo_base= 0x1161;
  constexpr int trailingjamo_base= 0x11A7;
  constexpr int voweljamo_cnt= 21;
  constexpr int trailingjamo_cnt= 28;
  int syllable_index= syllable - syllable_base;
  int v_t_combination= voweljamo_cnt * trailingjamo_cnt;
  int leadingjamo_index= syllable_index / v_t_combination;
  int voweljamo_index= (syllable_index % v_t_combination) / trailingjamo_cnt;
  int trailingjamo_index= syllable_index % trailingjamo_cnt;
  jamo[0]= leadingjamo_base + leadingjamo_index;
  jamo[1]= voweljamo_base + voweljamo_index;
  jamo[2]= trailingjamo_index ? (trailingjamo_base + trailingjamo_index) : 0;
  return trailingjamo_index ? 3 : 2;
}

void my_put_jamo_weights(const my_wc_t *hangul_jamo, int jamo_cnt,
                         MY_UCA_ITEM *item, const MY_UCA *uca)
{
  for (int jamoind= 0; jamoind < jamo_cnt; jamoind++)
  {
    uint16 *implicit_weight= item->weight + jamoind * MY_UCA_CE_SIZE;
    const uint16 *jamo_weight= uca->item[hangul_jamo[jamoind]].weight;
    *implicit_weight= *jamo_weight;
    *(implicit_weight + 1) = *(jamo_weight + 1);
    *(implicit_weight + 2) = *(jamo_weight + 2) + 1;
  }
  item->num_of_ce= jamo_cnt;
}
/*
  We need to initialize implicit weights because
  some pages have both implicit and explicit weights:
  0x4D??, 0x9F??
*/
static void
set_implicit_weights(MY_UCA *uca, const int *pageloaded)
{
  for (int page= 0; page < MY_UCA_NPAGES; page++)
  {
    if (pageloaded[page] == MY_UCA_CHARS_PER_PAGE)
      continue;
    /* Now set implicit weights */
    for (int code= page * MY_UCA_CHARS_PER_PAGE;
         code < (page + 1) * MY_UCA_CHARS_PER_PAGE; code++)
    {
      int base, aaaa, bbbb;
      MY_UCA_ITEM *item= &uca->item[code];

      if (item->num_of_ce)
        continue;

      int jamo_cnt= 0;
      my_wc_t hangul_jamo[HANGUL_JAMO_MAX_LENGTH];
      if ((jamo_cnt= my_decompose_hangul_syllable(code, hangul_jamo)))
      {
        my_put_jamo_weights(hangul_jamo, jamo_cnt, item, uca);
        continue;
      }
      else if (code >= 0x17000 && code <= 0x18AFF) //Tangut character
      {
        aaaa= 0xFB00;
        bbbb= (code - 0x17000) | 0x8000;
      }
      else
      {
        /* non-Core Han Unified Ideographs */
        if ((code >= 0x3400 && code <= 0x4DB5) ||
            (code >= 0x20000 && code <= 0x2A6D6) ||
            (code >= 0x2A700 && code <= 0x2B734) ||
            (code >= 0x2B740 && code <= 0x2B81D) ||
            (code >= 0x2B820 && code <= 0x2CEA1))
          base= 0xFB80;
        /* Core Han Unified Ideographs */
        else if ((code >= 0x4E00 && code <= 0x9FD5) ||
                 (code >= 0xFA0E && code <= 0xFA29))
          base= 0xFB40;
        /* All other characters whose weight is unassigned */
        else
          base= 0xFBC0;
        aaaa= base +  (code >> 15);
        bbbb= (code & 0x7FFF) | 0x8000;
      }

      item->weight[0]= aaaa;
      item->weight[1]= 0x0020;
      item->weight[2]= 0x0002;
      item->weight[3]= bbbb;
      item->weight[4]= 0x0000;
      item->weight[5]= 0x0000;

      item->num_of_ce= 2;
    }
  }
}

/*
  Move SPACE to the lowest possible weight, such that:

   1. Everything that is equal to SPACE (on a given level) keeps being
      equal to it.
   2. Everything else compares higher to SPACE on the first nonequal level.
   3. Space gets the weight [.0001.0001.0001].

  For why this is necessary, see the comments on my_strnxfrm_uca_900_tmpl()
  in ctype-uca.cc.
*/
static void
move_space_weights(MY_UCA *uca)
{
  const uint16 space_weights[3]= {
    uca->item[0x20].weight[0],
    uca->item[0x20].weight[1],
    uca->item[0x20].weight[2]
  };

  for (int code= 0; code < MY_UCA_MAXCHAR; ++code)
  {
    MY_UCA_ITEM *item= &uca->item[code];
    for (int ce= 0; ce < item->num_of_ce; ++ce)
    {
      uint16 *weight= item->weight + ce * MY_UCA_CE_SIZE;
      assert(weight[0] != 0x0001);  // Must be free.
      if (weight[0] == space_weights[0])
      {
        weight[0]= 0x0001;

        /*
          #2 is now met for the primary level. For the secondary and tertiary
          level, it is already so in UCA 9.0.0, but we verify it here.
          Also, we take care of demand #3, while making sure not to disturb
          demand #1.
        */
        for (int level= 1; level < 3; ++level)
        {
          assert(weight[level] != 0x0001);  // Must be free.
          assert(weight[level] >= space_weights[level]);
          if (weight[level] != space_weights[level])
            break;  // No need to modify the next levels.

          weight[level]= 0x0001;
        }
      }
    }
  }
}

static void
get_page_statistics(const MY_UCA *uca, int page, int *maxnum)
{
  for (int offs= 0; offs < MY_UCA_CHARS_PER_PAGE; offs++)
  {
    const MY_UCA_ITEM *item= &uca->item[page * MY_UCA_CHARS_PER_PAGE + offs];

    *maxnum= *maxnum < item->num_of_ce ? item->num_of_ce : *maxnum;
  }
}

/*
  Compose the prefix name of weight tables from the version number.
*/
static char *
prefix_name(const MY_UCA *uca)
{
  static char prefix[MY_UCA_VERSION_SIZE];
  const char *s;
  char *d;
  strcpy(prefix, "uca");
  for (s= uca->version, d= prefix + strlen(prefix); *s; s++)
  {
    if ((*s >= '0' && *s <= '9') || (*s >= 'a' && *s <= 'z'))
      *d++= *s;
  }
  *d= '\0';
  return prefix;
}


static char *
page_name(const MY_UCA *uca, int page, bool pageloaded)
{
  static char page_name_buf[120];
  static char page_name_null[]= "NULL";

  if (pageloaded)
  {
    snprintf(page_name_buf, sizeof(page_name_buf),
             "%s_p%03X",
             prefix_name(uca),
             page);
    return page_name_buf;
  }
  else
    return page_name_null;
}


static void
print_one_page(const MY_UCA *uca, int page, int maxnum)
{
  printf("uint16 %s[]= {\n", page_name(uca, page, true));

  printf("  /* Number of CEs for each character. */\n");
  for (int offs= 0; offs < MY_UCA_CHARS_PER_PAGE; ++offs)
  {
    const int code= page * MY_UCA_CHARS_PER_PAGE + offs;
    const MY_UCA_ITEM *item= &uca->item[code];
    if ((offs % 16) == 0)
      printf("  ");
    printf("%d, ", item->num_of_ce);
    if ((offs % 16) == 15)
      printf("\n");
  }

  for (int i= 0; i < maxnum - 1; i++)
  {
    printf("\n");
    if ((i % 3) == 0)
    {
      printf("  /* Primary weight %d for each character. */\n", i / 3 + 1);
    }
    else if ((i % 3) == 1)
    {
      printf("  /* Secondary weight %d for each character. */\n", i / 3 + 1);
    }
    else
    {
      printf("  /* Tertiary weight %d for each character. */\n", i / 3 + 1);
    }
    for (int offs= 0; offs < MY_UCA_CHARS_PER_PAGE; offs++)
    {
      const int code= page * MY_UCA_CHARS_PER_PAGE + offs;
      const MY_UCA_ITEM *item= &uca->item[code];
      const uint16 *weight= item->weight;
      printf("  0x%04X,   /* U+%04X */\n", weight[i], code);
    }
  }
  printf("};\n\n");
}

int
main(int ac, char **av)
{
  static MY_UCA uca;
  int maxchar= MY_UCA_MAXCHAR;
  static int pageloaded[MY_UCA_NPAGES];

  memset(&uca, 0, sizeof(uca));

  memset(pageloaded, 0, sizeof(pageloaded));

  load_uca_file(&uca, maxchar, pageloaded);

  set_implicit_weights(&uca, pageloaded);

  move_space_weights(&uca);

  int pagemaxlen[MY_UCA_NPAGES];

  for (int page= 0; page < MY_UCA_NPAGES; page++)
  {
    int maxnum= 0;

    pagemaxlen[page]= 0;

    /* Skip this page if no weights were loaded */
    if (!pageloaded[page])
      continue;

    /*
      Calculate number of weights per character
      and number of default weights.
    */
    get_page_statistics(&uca, page, &maxnum);

    maxnum= maxnum * MY_UCA_CE_SIZE + 1;

    pagemaxlen[page]= maxnum;

    print_one_page(&uca, page, maxnum);
  }

  /* Print page index */
  printf("uint16* %s_weight[%d]= {\n",
         prefix_name(&uca), MY_UCA_NPAGES);
  for (int page= 0; page < MY_UCA_NPAGES; page++)
  {
    if (!(page % 6))
      printf("%13s", page_name(&uca, page, pagemaxlen[page]));
    else
      printf("%12s", page_name(&uca, page, pagemaxlen[page]));
    if ((page + 1) != MY_UCA_NPAGES)
      printf(",");
    if (!((page + 1) % 6) || (page + 1) == MY_UCA_NPAGES)
      printf("\n");
  }
  printf("};\n\n");

  return 0;
}
