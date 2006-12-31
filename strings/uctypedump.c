/* Copyright (C) 2006 MySQL AB

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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
*/
#include <my_global.h>
#include <m_string.h>
#include <m_ctype.h>
#include "m_ctype.h"


typedef struct my_ctype_name_st
{
  const char *name;
  int val;
} MY_CTYPE_NAME_ST;


static MY_CTYPE_NAME_ST my_ctype_name[]=
{
  {"Lu", _MY_U},                /* Letter, Uppercase          */
  {"Ll", _MY_L},                /* Letter, Lowercase          */
  {"Lt", _MY_U},                /* Letter, Titlecase          */
  {"Lm", _MY_L},                /* Letter, Modifier           */
  {"Lo", _MY_L},                /* Letter, other              */
  
  {"Nd", _MY_NMR},              /* Number, Decimal Digit      */
  {"Nl", _MY_NMR|_MY_U|_MY_L},  /* Number, Letter             */
  {"No", _MY_NMR|_MY_PNT},      /* Number, Other              */
  
  {"Mn", _MY_L|_MY_PNT},        /* Mark, Nonspacing           */
  {"Mc", _MY_L|_MY_PNT},        /* Mark, Spacing Combining    */
  {"Me", _MY_L|_MY_PNT},        /* Mark, Enclosing            */
  
  {"Pc", _MY_PNT},              /* Punctuation, Connector     */
  {"Pd", _MY_PNT},              /* Punctuation, Dash          */
  {"Ps", _MY_PNT},              /* Punctuation, Open          */
  {"Pe", _MY_PNT},              /* Punctuation, Close         */
  {"Pi", _MY_PNT},              /* Punctuation, Initial quote */
  {"Pf", _MY_PNT},              /* Punctuation, Final quote   */
  {"Po", _MY_PNT},              /* Punctuation, Other         */
  
  {"Sm", _MY_PNT},              /* Symbol, Math               */
  {"Sc", _MY_PNT},              /* Symbol, Currency           */
  {"Sk", _MY_PNT},              /* Symbol, Modifier           */
  {"So", _MY_PNT},              /* Symbol, Other              */
  
  {"Zs", _MY_SPC},              /* Separator, Space           */
  {"Zl", _MY_SPC},              /* Separator, Line            */
  {"Zp", _MY_SPC},              /* Separator, Paragraph       */
  
  {"Cc", _MY_CTR},              /* Other, Control             */
  {"Cf", _MY_CTR},              /* Other, Format              */
  {"Cs", _MY_CTR},              /* Other, Surrogate           */
  {"Co", _MY_CTR},              /* Other, Private Use         */
  {"Cn", _MY_CTR},              /* Other, Not Assigned        */
  {NULL, 0}
};


static int
ctypestr2num(const char *tok)
{
  MY_CTYPE_NAME_ST *p;
  for (p= my_ctype_name; p->name; p++)
  {
    if (!strncasecmp(p->name, tok, 2))
      return p->val;
  }
  return 0;
}


int main(int ac, char ** av)
{
  char str[1024];
  unsigned char ctypea[64*1024];
  size_t i;
  size_t plane;
  MY_UNI_CTYPE uctype[256];
  FILE *f= stdin;

  if (ac > 1 && av[1] && !(f= fopen(av[1],"r")))
  {
    fprintf(stderr, "Can't open file %s\n", av[1]);
    exit(1);
  }
  bzero(&ctypea,sizeof(ctypea));
  bzero(&uctype, sizeof(uctype));
  
  printf("/*\n");
  printf("  Unicode ctype data\n");
  printf("  Generated from %s\n", av[1] ? av[1] : "stdin");
  printf("*/\n");
  
  while(fgets(str, sizeof(str), f))
  {
    size_t n= 0, code= 0;
    char *s,*e;
    int ctype= 0;
    
    for(s= str; s; )
    {
      char *end;
      char tok[1024]="";
      e=strchr(s,';');
      if(e)
      {
        strncpy(tok,s,(unsigned int)(e-s));
        tok[e-s]=0;
      }
      else
      {
        strcpy(tok,s);
      }
      
      end=tok+strlen(tok);
      
      switch(n)
      {
        case 0: code= strtol(tok,&end,16);break;
        case 2: ctype= ctypestr2num(tok);break;
      }
      
      n++;
      if(e)  s=e+1;
      else  s=e;
    }
    if(code<=0xFFFF)
    {
      ctypea[code]= ctype;
    }
  }
  
  /* Fill digits */
  for (i= '0'; i <= '9'; i++)
    ctypea[i]= _MY_NMR;
    
  for (i= 'a'; i <= 'z'; i++)
    ctypea[i]|= _MY_X;
  for (i= 'A'; i <= 'Z'; i++)
    ctypea[i]|= _MY_X;
  
  
  /* Fill ideographs  */
  
  /* CJK Ideographs Extension A (U+3400 - U+4DB5) */
  for(i=0x3400;i<=0x4DB5;i++)
  {
    ctypea[i]= _MY_L | _MY_U;
  }
  
  /* CJK Ideographs (U+4E00 - U+9FA5) */
  for(i=0x4E00;i<=0x9FA5;i++){
    ctypea[i]= _MY_L | _MY_U;
  }
  
  /* Hangul Syllables (U+AC00 - U+D7A3)  */
  for(i=0xAC00;i<=0xD7A3;i++)
  {
    ctypea[i]= _MY_L | _MY_U;
  }
  
  
  /* Calc plane parameters */
  for(plane=0;plane<256;plane++)
  {
    size_t character;
    uctype[plane].ctype= ctypea+plane*256;
    
    uctype[plane].pctype= uctype[plane].ctype[0];
    for(character=1;character<256;character++)
    {
      if (uctype[plane].ctype[character] != uctype[plane].pctype)
      {
        uctype[plane].pctype= 0; /* Mixed plane */
        break;
      }
    }
    if (character==256)	/* All the same, no needs to dump whole plane */
      uctype[plane].ctype= NULL; 
  }
  
  /* Dump mixed planes */
  for(plane=0;plane<256;plane++)
  {
    if(uctype[plane].ctype)
    {
      int charnum=0;
      int num=0;
      
      printf("static unsigned char uctype_page%02X[256]=\n{\n",plane);
      
      for(charnum=0;charnum<256;charnum++)
      {
        int cod;
        
        cod=(plane<<8)+charnum;
        printf(" %2d%s",uctype[plane].ctype[charnum],charnum<255?",":"");
      
        num++;
        if(num==16)
        {
          printf("\n");
          num=0;
        }
      }
      printf("};\n\n");
    }
  }
  
  
  /* Dump plane index */
  printf("MY_UNI_CTYPE my_uni_ctype[256]={\n");
  for(plane=0;plane<256;plane++)
  {
    char plane_name[128]="NULL";
    if(uctype[plane].ctype){
      sprintf(plane_name,"uctype_page%02X",plane);
    }
    printf("\t{%d,%s}%s\n",uctype[plane].pctype,plane_name,plane<255?",":"");
  }
  printf("};\n");
  
  return 0;
}
