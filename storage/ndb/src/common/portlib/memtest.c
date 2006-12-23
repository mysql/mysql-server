/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */



#include <ndb_global.h>

long long getMilli();
long long getMicro();
void malloctest(int loopcount, int memsize, int touch);
void freetest(int loopcount, int memsize);
void mmaptest(int loopcount, int memsize, int touch);
void unmaptest(int loopcount, int memsize);


main(int argc, char ** argv)
{

  int loopcount;
  int memsize;
  if(argc < 4) {
    printf("Usage:  memtest X loopcount memsize(MB)\n");
    printf("where X = \n");
    printf("1 : malloc test \n");
    printf("2 : mmap test \n");
    printf("3 : malloc test + touch pages\n");
    printf("4 : mmap test + touch pages\n");
    printf("5 : malloc/free test \n");
    printf("6 : mmap/munmap test \n");
    printf("loopcount - number of loops\n");
    printf("memsize - memory segment size to allocate in MB.\n");
    exit(1);
  }
  

  loopcount = atoi(argv[2]);
  memsize = atoi(argv[3]);
  switch(atoi(argv[1])) {
  case 1: malloctest(loopcount, memsize , 0 );
    break;
  case 2: mmaptest(loopcount, memsize,0);
    break;
  case 3: malloctest(loopcount, memsize,1);
    break;
  case 4: mmaptest(loopcount, memsize,1);
    break;
  case 5: freetest(loopcount, memsize);
    break;
  case 6: unmaptest(loopcount, memsize);
    break;
  default:
    break;
  }
}
  
long long getMilli() {
  struct timeval tick_time;
  gettimeofday(&tick_time, 0);

  return 
    ((long long)tick_time.tv_sec)  * ((long long)1000) +
    ((long long)tick_time.tv_usec) / ((long long)1000);
}

long long getMicro(){
  struct timeval tick_time;
  int res = gettimeofday(&tick_time, 0);

  long long secs   = tick_time.tv_sec;
  long long micros = tick_time.tv_usec;
  
  micros = secs*1000000+micros;
  return micros;
}

void malloctest(int loopcount, int memsize, int touch) {
  long long start=0;
  int total=0;
  int i=0, j=0;
  int size=memsize*1024*1024; /*bytes*/;
  float mean;
  char * ptr =0;
  
  printf("Staring malloctest ");
  if(touch)
    printf("with touch\n");
  else
    printf("\n");
  
  start=getMicro();
  
  for(i=0; i<loopcount; i++){
    ptr=(char *)malloc((size_t)(size));
    if(ptr==0) {
      printf("failed to malloc!\n");
      return;
    }    
    if(touch) {
      for(j=0; j<size; j=j+4096)
	ptr[j]=1;
    }
  }
  total=(int)(getMicro()-start);
  
  mean=(float)((float)total/(float)loopcount);
  printf("Total time malloc %d bytes: %2.3f microsecs  loopcount %d touch %d \n",
	 size, mean,loopcount, touch);  
}


void mmaptest(int loopcount, int memsize, int touch) {
  long long start=0;
  int total=0;
  int i=0, j=0;
  char * ptr;
  int size=memsize*1024*1024; /*bytes*/;
  float mean;

  printf("Staring mmaptest ");
  if(touch)
    printf("with touch \n");
  else
    printf("\n");

  start=getMicro();  
  for(i=0; i<loopcount; i++){
    ptr = mmap(0, 
	       size, 
	       PROT_READ|PROT_WRITE, 
	       MAP_PRIVATE|MAP_ANONYMOUS, 
	       0,
	       0);
    if(ptr<0) {
      printf("failed to mmap!\n");
      return;
    }

    if(touch) {
      for(j=0; j<size; j=j+4096)
	ptr[j]=1;
    }  
  }
  total=(int)(getMicro()-start);
  mean=(float)((float)total/(float)loopcount);
  printf("Total time mmap %d bytes: %2.3f microsecs  \n",size, mean);  
}


void unmaptest(loopcount, memsize) 
{
  long long start=0;
  int total=0;
  int i=0, j=0;
  char * ptr;
  int size=memsize*1024*1024; /*bytes*/;
  float mean;

  printf("Staring munmap test (loopcount = 1 no matter what you prev. set)\n");

  loopcount = 1;


  for(i=0; i<loopcount; i++){
    ptr =(char*) mmap(0, 
		      size, 
		      PROT_READ|PROT_WRITE, 
		      MAP_PRIVATE|MAP_ANONYMOUS, 
		      0,
		      0);
    if(ptr<0) {
      printf("failed to mmap!\n");
      return;
    }


    for(j=0; j<size; j=j+1)
      ptr[j]='1';
    start=getMicro(); 
    if(munmap(ptr, size)<0) {
      printf("failed to munmap!\n");
      return;
    }
   
    total=(int)(getMicro()-start);
    /*
    for(j=8192; j<size; j=j+4096) {

	*(ptr+j)='1';
    }
    
    for(j=0; j<4096; j=j+4096) { 
      *(ptr+j)='1';
    }
    
    */
  }
  mean=(float)((float)total/(float)loopcount);
  printf("Total time unmap %d bytes: %2.3f microsecs  \n",size, mean);  
}

void freetest(int loopcount, int memsize) {
  long long start=0;
  int total=0;
  int i=0, j=0;
  int size=memsize*1024*1024; /*bytes*/;
  float mean;
  char * ptr =0;

  loopcount = 1;
  printf("Staring free test (loopcount = 1 no matter what you prev. set)\n");

  
  for(i=0; i<loopcount; i++){
    ptr=(char*)malloc((size_t)(size));
    if(ptr==0) {
      printf("failed to malloc!\n");
      return;
    }
    for(j=0; j<size; j=j+4096)
      ptr[j]='1';
    start=getMicro();     
    free(ptr);
    total=(int)(getMicro()-start);
  }

  
  mean=(float)((float)total/(float)loopcount);
  printf("Total time free %d bytes: %2.3f microsecs  loopcount %d \n",
	 size, mean,loopcount);  
}
