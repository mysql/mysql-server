#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char uchar;
typedef unsigned short uint16;

struct uca_item_st
{
  uchar  num;
  uint16 weight[4][9];
};

#if 0
#define MY_UCA_NPAGES	1024
#define MY_UCA_NCHARS	64
#define MY_UCA_CMASK	63
#define MY_UCA_PSHIFT	6
#else
#define MY_UCA_NPAGES	256
#define MY_UCA_NCHARS	256
#define MY_UCA_CMASK	255
#define MY_UCA_PSHIFT	8
#endif

static char *pname[]= {"", "2", "3"};

int main(int ac, char **av)
{
  char str[256];
  char *weights[64];
  struct uca_item_st uca[64*1024];
  size_t code, w;
  int pageloaded[MY_UCA_NPAGES];
  
  bzero(uca, sizeof(uca));
  bzero(pageloaded, sizeof(pageloaded));
  
  while (fgets(str,sizeof(str),stdin))
  {
    char *comment;
    char *weight;
    char *s;
    size_t codenum;
    
    code= strtol(str,NULL,16);
    
    if (str[0]=='#' || (code > 0xFFFF))
      continue;
    if ((comment=strchr(str,'#')))
    {
      *comment++= '\0';
      for ( ; *comment==' ' ; comment++);
    }else
      continue;
    
    if ((weight=strchr(str,';')))
    {
      *weight++= '\0';
      for ( ; *weight==' ' ; weight++);
    }
    else
      continue;
    
    codenum= 0;
    s= strtok(str, " \t");
    while (s)
    {
      s= strtok(NULL, " \t");
      codenum++;
    }
    
    if (codenum>1)
    {
      /* Multi-character weight, 
         i.e. contraction. 
         Not supported yet.
      */
      continue;
    }
    
    uca[code].num= 0;
    s= strtok(weight, " []");
    while (s)
    {
      weights[uca[code].num]= s;
      s= strtok(NULL, " []");
      uca[code].num++;
    }
    
    for (w=0; w < uca[code].num; w++)
    {
      size_t partnum;
      
      partnum= 0;
      s= weights[w];
      while (*s)
      {
        char *endptr;
        size_t part;
        part= strtol(s+1,&endptr,16);
        uca[code].weight[partnum][w]= part;
        s= endptr;
        partnum++;
      }
    }
    /* Mark that a character from this page was loaded */
    pageloaded[code >> MY_UCA_PSHIFT]++;
  }
  
  
  
  /* Now set implicit weights */
  for (code=0; code <= 0xFFFF; code++)
  {
    size_t base, aaaa, bbbb;
    
    if (uca[code].num)
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
    uca[code].weight[0][0]= aaaa;
    uca[code].weight[0][1]= bbbb;
    
    uca[code].weight[1][0]= 0x0020;
    uca[code].weight[1][1]= 0x0000;
    
    uca[code].weight[2][0]= 0x0002;
    uca[code].weight[2][1]= 0x0000;
    
    uca[code].weight[3][0]= 0x0001;
    uca[code].weight[3][2]= 0x0000;
    
    uca[code].num= 2;
  }
  
  printf("#include \"my_uca.h\"\n");
  
  printf("#define MY_UCA_NPAGES %d\n",MY_UCA_NPAGES);
  printf("#define MY_UCA_NCHARS %d\n",MY_UCA_NCHARS);
  printf("#define MY_UCA_CMASK  %d\n",MY_UCA_CMASK);
  printf("#define MY_UCA_PSHIFT %d\n",MY_UCA_PSHIFT);
  
  for (w=0; w<3; w++)
  {
    size_t page;
    int pagemaxlen[MY_UCA_NPAGES];

    for (page=0; page < MY_UCA_NPAGES; page++)
    {
      size_t offs;
      size_t maxnum= 0;
      size_t nchars= 0;
      size_t mchars;
      size_t ndefs= 0;
      
      pagemaxlen[page]= 0;
      
      /*
        Skip this page if no weights were loaded
      */
      
      if (!pageloaded[page])
        continue;
      
      /* 
        Calculate maximum weight
        length for this page
      */
      
      for (offs=0; offs < MY_UCA_NCHARS; offs++)
      {
        size_t i, num;
        
        code= page*MY_UCA_NCHARS+offs;
        
        /* Calculate only non-zero weights */
        for (num=0, i=0; i < uca[code].num; i++)
          if (uca[code].weight[w][i])
            num++;
        
        maxnum= maxnum < num ? num : maxnum;
        
        /* Check if default weight */
        if (w == 1 && num == 1)
        {
          /* 0020 0000 ... */
          if (uca[code].weight[w][0] == 0x0020)
            ndefs++;
        }
        else if (w == 2 && num == 1)
        {
          /* 0002 0000 ... */
          if (uca[code].weight[w][0] == 0x0002)
            ndefs++;
        }
      } 
      maxnum++;
      
      /*
        If the page have only default weights
        then no needs to dump it, skip.
      */
      if (ndefs == MY_UCA_NCHARS)
      {
        continue;
      }
      switch (maxnum)
      {
        case 0: mchars= 8; break;
        case 1: mchars= 8; break;
        case 2: mchars= 8; break;
        case 3: mchars= 9; break;
        case 4: mchars= 8; break;
        default: mchars= uca[code].num;
      }
      
      pagemaxlen[page]= maxnum;


      /*
        Now print this page
      */
      
      
      printf("uint16 page%03Xdata%s[]= { /* %04X (%d weights per char) */\n",
              page, pname[w], page*MY_UCA_NCHARS, maxnum);
      
      for (offs=0; offs < MY_UCA_NCHARS; offs++)
      {
        uint16 weight[8];
        size_t num, i;
        
        code= page*MY_UCA_NCHARS+offs;
        
        bzero(weight,sizeof(weight));
        
        /* Copy non-zero weights */
        for (num=0, i=0; i < uca[code].num; i++)
        {
          if (uca[code].weight[w][i])
          {
            weight[num]= uca[code].weight[w][i];
            num++;
          }
        }
        
        for (i=0; i < maxnum; i++)
        {
          /* 
            Invert weights for secondary level to
            sort upper case letters before their
            lower case counter part.
          */
          int tmp= weight[i];
          if (w == 2 && tmp)
            tmp= (int)(0x100 - weight[i]);
          
          
          printf("0x%04X", tmp);
          if ((offs+1 != MY_UCA_NCHARS) || (i+1!=maxnum))
            printf(",");
          nchars++;
        }
        if (nchars >=mchars)
        {
          printf("\n");
          nchars=0;
        }
        else
        {
          printf(" ");
        }
      }
      printf("};\n\n");
    }

    printf("uchar uca_length%s[%d]={\n", pname[w], MY_UCA_NPAGES);
    for (page=0; page < MY_UCA_NPAGES; page++)
    {
      printf("%d%s%s",pagemaxlen[page],page<MY_UCA_NPAGES-1?",":"",(page+1) % 16 ? "":"\n");
    }
    printf("};\n");


    printf("uint16 *uca_weight%s[%d]={\n", pname[w], MY_UCA_NPAGES);
    for (page=0; page < MY_UCA_NPAGES; page++)
    {
      const char *comma= page < MY_UCA_NPAGES-1 ? "," : "";
      const char *nline= (page+1) % 4 ? "" : "\n";
      if (!pagemaxlen[page])
        printf("NULL        %s%s", comma , nline);
      else
        printf("page%03Xdata%s%s%s", page, pname[w], comma, nline);
    }
    printf("};\n");
  }

  
  printf("int main(void){ return 0;};\n");
  return 0;
}
