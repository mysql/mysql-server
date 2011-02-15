/* Copyright (c) 2004, 2011, Oracle and/or its affiliates. All rights reserved.

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char uchar;
typedef unsigned short uint16;

#define MY_UCA_MAXWEIGHT_TO_PARSE 64
#define MY_UCA_MAXWEIGHT_TO_DUMP  8
#define MY_UCA_MAXLEVEL           4
#define MY_UCA_VERSION_SIZE       32

#define MY_UCA_MAXCHAR  (0x10FFFF+1)
#define MY_UCA_NCHARS	256
#define MY_UCA_CMASK	255
#define MY_UCA_PSHIFT	8
#define MY_UCA_NPAGES	MY_UCA_MAXCHAR/MY_UCA_NCHARS
typedef struct uca_item_st
{
  uchar  num; /* Number of weights */
  uint16 weight[MY_UCA_MAXLEVEL+1][MY_UCA_MAXWEIGHT_TO_DUMP];
  /* +1 for trailing zero */
} MY_UCA_ITEM;


typedef struct uca_info_st
{
  char version[MY_UCA_VERSION_SIZE];
  MY_UCA_ITEM item[MY_UCA_MAXCHAR];
} MY_UCA;





static int load_uca_file(MY_UCA *uca,
                         size_t maxchar, int *pageloaded)
{
  char str[512];
  size_t lineno, out_of_range_chars= 0;
  char *weights[MY_UCA_MAXWEIGHT_TO_PARSE];
  
  for (lineno= 0; fgets(str, sizeof(str), stdin); lineno++)
  {
    char *comment;
    char *weight;
    char *s;
    size_t codenum, i, code;
    MY_UCA_ITEM *item= NULL;
    
    
    /* Skip comment lines */
    if (*str== '\r' || *str == '\n' || *str == '#')
      continue;
    
    /* Detect version */
    if (*str == '@' && !strncmp(str, "@version ", 9))
    {
      const char *value;
      if (strtok(str, " \r\n\t") && (value= strtok(NULL, " \r\n\t")))
        snprintf(uca->version, MY_UCA_VERSION_SIZE, value);
      continue;
    }
    
    /* Skip big characters */
    if ((code= strtol(str,NULL,16)) > maxchar)
    {
      out_of_range_chars++;
      continue;
    }
    item= &uca->item[code];
    
    
    if ((comment= strchr(str,'#')))
    {
      *comment++= '\0';
      for ( ; *comment == ' '; comment++);
    }
    else
    {
      fprintf(stderr, "Warning: could not parse line #%d:\n'%s'\n",
              lineno, str);
      continue;
    }
    
    
    if ((weight= strchr(str,';')))
    {
      *weight++= '\0';
      for ( ; *weight==' '; weight++);
    }
    else
    {
      fprintf(stderr, "Warning: could not parse line #%d:\n%s\n", lineno, str);
      continue;
    }
    
    codenum= 0;
    s= strtok(str, " \t");
    while (s)
    {
      s= strtok(NULL, " \t");
      codenum++;
    }
    
    if (codenum > 1)
    {
      /* Multi-character weight (contraction) - not supported yet. */
      continue;
    }
    
    
    /*
      Split weight string into separate weights

      "[p1.s1.t1.q1][p2.s2.t2.q2][p3.s3.t3.q3]" ->
      
      "p1.s1.t1.q1" "p2.s2.t2.q2" "p3.s3.t3.q3"
    */
    item->num= 0;
    s= strtok(weight, " []");
    while (s)
    {
      if (item->num >= MY_UCA_MAXWEIGHT_TO_PARSE)
      {
        fprintf(stderr, "Line #%d has more than %d weights\n",
                lineno, MY_UCA_MAXWEIGHT_TO_PARSE);
        fprintf(stderr, "Can't continue.\n");
        exit(1);
      }
      weights[item->num]= s;
      s= strtok(NULL, " []");
      item->num++;
    }
    
    for (i= 0; i < item->num; i++)
    {
      size_t level= 0;
      
      if (i >= MY_UCA_MAXWEIGHT_TO_DUMP)
      {
        fprintf(stderr,
                "Warning: at line %d: character %04X has"
                " more than %d many weights (%d). "
                "Skipping the extra weights.\n",
                lineno, code, MY_UCA_MAXWEIGHT_TO_DUMP, item->num);
        item->num= MY_UCA_MAXWEIGHT_TO_DUMP;
        break;
      }
      
      for (s= weights[i]; *s; )
      {
        char *endptr;
        size_t part= strtol(s + 1, &endptr, 16);
        if (i < MY_UCA_MAXWEIGHT_TO_DUMP)
        {
          item->weight[level][i]= part;
        }
        else
        {
          fprintf(stderr, "Too many weights: %d\n", i);
        }
        s= endptr;
        level++;
      }
    }
    /* Mark that a character from this page was loaded */
    pageloaded[code >> MY_UCA_PSHIFT]++;
  }

  if (out_of_range_chars)
    fprintf(stderr, "%d out-of-range characters skipped\n", out_of_range_chars);

  return 0;
}  


/*
  We need to initialize implicit weights because
  some pages have both implicit and explicit weights:
  0x4D??, 0x9F??
*/
static void
set_implicit_weights(MY_UCA *uca, size_t maxchar)
{
  size_t code;
  /* Now set implicit weights */
  for (code= 0; code < maxchar; code++)
  {
    size_t base, aaaa, bbbb;
    MY_UCA_ITEM *item= &uca->item[code];
    
    if (item->num)
      continue;
    
    /*
    3400;<CJK Ideograph Extension A, First>
    4DB5;<CJK Ideograph Extension A, Last>
    4E00;<CJK Ideograph, First>
    9FA5;<CJK Ideograph, Last>
    */
    
    if (code >= 0x3400 && code <= 0x4DB5)
      base= 0xFB80;
    else if (code >= 0x4E00 && code <= 0x9FA5)
      base= 0xFB40;
    else
      base= 0xFBC0;
    
    aaaa= base +  (code >> 15);
    bbbb= (code & 0x7FFF) | 0x8000;
    item->weight[0][0]= aaaa;
    item->weight[0][1]= bbbb;
    
    item->weight[1][0]= 0x0020;
    item->weight[1][1]= 0x0000;
    
    item->weight[2][0]= 0x0002;
    item->weight[2][1]= 0x0000;
    
    item->weight[3][0]= 0x0001;
    item->weight[3][2]= 0x0000;
    
    item->num= 2;
  }
}


static void
get_page_statistics(MY_UCA *uca, size_t page, size_t level,
                    size_t *maxnum, size_t *ndefs)
{
  size_t offs;
  
  for (offs= 0; offs < MY_UCA_NCHARS; offs++)
  {
    size_t i, num;
    MY_UCA_ITEM *item= &uca->item[page * MY_UCA_NCHARS + offs];
    
    /* Calculate only non-zero weights */
    for (num= 0, i= 0; i < item->num; i++)
    {
      if (item->weight[level][i])
        num++;
    }
    *maxnum= *maxnum < num ? num : *maxnum;
    
    /* Check if default weight */
    if (level == 1 && num == 1)
    {
      /* 0020 0000 ... */
      if (item->weight[level][0] == 0x0020)
        (*ndefs)++;
    }
    else if (level == 2 && num == 1)
    {
      /* 0002 0000 ... */
      if (item->weight[level][0] == 0x0002)
        (*ndefs)++;
    }
  } 
}


static const char *pname[]= {"", "l2", "l3", "l4"};


static char *
prefix_name(MY_UCA *uca)
{
  static char prefix[MY_UCA_VERSION_SIZE];
  char *s, *d;
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
page_name(MY_UCA *uca, size_t page, size_t level)
{
  static char page_name_buf[120];

  snprintf(page_name_buf, sizeof(page_name_buf),
           "%s_p%03X%s",
           prefix_name(uca),
           page, pname[level]);
  return page_name_buf;
}



static void
print_one_page(MY_UCA *uca, size_t level,
               size_t page, size_t maxnum)
{
  size_t offs, mchars, nchars= 0, chars_per_line;
  
  printf("uint16 %s[]= { /* %04X (%d weights per char) */\n",
         page_name(uca, page, level), page * MY_UCA_NCHARS, maxnum);
  
  /* Calculate how many wights to print per line */
  switch (maxnum)
  {
    case 0: mchars= 8; chars_per_line= 8; break;
    case 1: mchars= 8; chars_per_line= 8; break;
    case 2: mchars= 8; chars_per_line= 4; break;
    case 3: mchars= 9; chars_per_line= 3; break;
    case 4: mchars= 8; chars_per_line= 2; break;
    default:
      mchars= uca->item[page * MY_UCA_NCHARS + offs].num;
      chars_per_line= 1;
  }
  
  
  /* Print the page */
  for (offs=0; offs < MY_UCA_NCHARS; offs++)
  {
    uint16 weight[9];
    size_t num, i, code= page * MY_UCA_NCHARS + offs;
    MY_UCA_ITEM *item= &uca->item[code];
    
    bzero(weight, sizeof(weight));
    
    /* Copy non-zero weights */
    for (num= 0, i= 0; i < item->num && i < MY_UCA_MAXWEIGHT_TO_DUMP; i++)
    {
      if (item->weight[level][i])
      {
        weight[num]= item->weight[level][i];
        num++;
      }
    }
    
    /* Print weights */
    for (i= 0; i < maxnum; i++)
    {
      /* 
        Invert weights for secondary level to
        sort upper case letters before their
        lower case counter part.
      */
      int tmp= weight[i];
      if (level == 2 && tmp)
        tmp= (int) (0x20 - weight[i]);

      printf("0x%04X", tmp);
      
      
      if (tmp > 0xFFFF || tmp < 0)
      {
        fprintf(stderr,
                "Error: Too big weight for code point %04X level %d: %08X\n",
                code, level, tmp);
        exit(1);
      }
      
      
      if ((offs + 1 != MY_UCA_NCHARS) || (i + 1 != maxnum))
        printf(",");
      else
        printf(" ");
      nchars++;
    }
    
    if (nchars >=mchars)
    {
      /* Print "\n" with a comment telling the first code on this line. */
      printf(" /* %04X */\n", (code + 1) - chars_per_line);
      nchars= 0;
    }
    else
    {
      printf(" ");
    }
  }
  printf("};\n\n");
}


int
main(int ac __attribute__((unused)), char **av __attribute__((unused)))
{
  static MY_UCA uca;
  size_t level, maxchar= MY_UCA_MAXCHAR;
  static int pageloaded[MY_UCA_NPAGES];
  size_t nlevels= 1;
  
  bzero(&uca, sizeof(uca));
  bzero(pageloaded, sizeof(pageloaded));
  
  load_uca_file(&uca, maxchar, pageloaded);
  
  /* Now set implicit weights */
  set_implicit_weights(&uca, maxchar);
  
  
  printf("#include \"my_uca.h\"\n");
  printf("\n\n");
  printf("#define MY_UCA_NPAGES %d\n", MY_UCA_NPAGES);
  printf("#define MY_UCA_NCHARS %d\n", MY_UCA_NCHARS);
  printf("#define MY_UCA_CMASK  %d\n", MY_UCA_CMASK);
  printf("#define MY_UCA_PSHIFT %d\n", MY_UCA_PSHIFT);
  printf("\n\n");
  printf("/* Created from allkeys.txt. Unicode version '%s'. */\n\n", uca.version);
  
  for (level= 0; level < nlevels; level++)
  {
    size_t page;
    int pagemaxlen[MY_UCA_NPAGES];

    for (page=0; page < MY_UCA_NPAGES; page++)
    {
      size_t maxnum= 0;
      size_t ndefs= 0;
      
      pagemaxlen[page]= 0;
      
      /* Skip this page if no weights were loaded */
      if (!pageloaded[page])
        continue;
      
      /*
        Calculate number of weights per character
        and number of default weights.
      */
      get_page_statistics(&uca, page, level, &maxnum, &ndefs);
      
      
      maxnum++; /* For zero terminator */
      
      
      /*
        If the page have only default weights
        then no needs to dump it, skip.
      */
      if (ndefs == MY_UCA_NCHARS)
        continue;
      
      pagemaxlen[page]= maxnum;
      
      
      /* Now print this page */
      print_one_page(&uca, level, page, maxnum);
    }
    
    
    /* Print page lengths */
    printf("uchar %s_length%s[%d]={\n",
           prefix_name(&uca), pname[level], MY_UCA_NPAGES);
    for (page=0; page < MY_UCA_NPAGES; page++)
    {
      printf("%d%s%s",
             pagemaxlen[page],
             page < MY_UCA_NPAGES - 1 ? "," : "" ,
             (page + 1) % 16 ? "" : "\n");
    }
    printf("};\n");


    /* Print page index */
    printf("uint16 *%s_weight%s[%d]={\n",
           prefix_name(&uca), pname[level], MY_UCA_NPAGES);
    for (page=0; page < MY_UCA_NPAGES; page++)
    {
      const char *comma= page < MY_UCA_NPAGES - 1 ? "," : "";
      const char *nline= (page + 1) % 4 ? "" : "\n";
      if (!pagemaxlen[page])
        printf("NULL       %s%s%s", level ? " ": "", comma , nline);
      else
        printf("%s%s%s", page_name(&uca, page, level), comma, nline);
    }
    printf("};\n");
  }

  
  printf("int main(void){ return 0;};\n");
  return 0;
}
