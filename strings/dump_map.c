#include <stdio.h>
#include <string.h>

static void print_short_array(unsigned short *a)
{
  int i;
  printf("{\n");
  for (i=0; i<=0xFF; i++)
  {
    printf("0x%04X%s%s",(int)a[i],i<0xFF?",":"",(i+1) % 8 ? "" :"\n");
  }
  printf("};\n");
  
}



int main(void)
{
  char str[160];
  unsigned short touni[256];
  unsigned short fromuni[65536];
  unsigned short fromstat[256];
  int i;
  
  bzero((void*)touni,sizeof(touni));
  bzero((void*)fromuni,sizeof(fromuni));
  bzero((void*)fromstat,sizeof(fromstat));
  
  while (fgets(str,sizeof(str),stdin))
  {
    unsigned int c,u;
    
    if ((str[0]=='#') || (2!=sscanf(str,"%x%x",&c,&u)))
      continue;
    if (c>0xFF || u>0xFFFF)
      continue;
    
    touni[c]= u;
    fromuni[u]= c;
  }
  
  printf("unsigned short cs_to_uni[256]=");
  print_short_array(touni);
  
  for (i=0;i<=0xFF;i++)
  {
    fromstat[touni[i]>>8]++;
  }
  
  for (i=0;i<=256;i++)
  {
    if (fromstat[i])
    { 
      printf("unsigned char pl%02X[256]=",i);
      print_short_array(fromuni+i*256);
    }
  }
  
  printf("unsigned short *uni_to_cs[256]={\n");
  for (i=0;i<=255;i++)
  {
    if (fromstat[i])
      printf("pl%02X",i);
    else
      printf("NULL");
    printf("%s%s",i<255?",":"",((i+1) % 8) ? "":"\n");
  }
  printf("};\n");
  
  return 0;
}
