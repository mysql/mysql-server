/*
** An alternative implemtation of "strtod()" that is both
** simplier, and thread-safe.
*/
#include <pthread.h>
#include <ctype.h>
#include <math.h>

#ifdef TEST
#  define strtod NewStrtod
#include <stdio.h>
#endif

static double scaler10[] = {
  1.0, 1e10, 1e20, 1e30, 1e40, 1e50, 1e60, 1e70, 1e80, 1e90
};
static double scaler1[] = {
  1.0, 10.0, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9
};
static double pastpoint[] = {
  1e-1,  1e-2,  1e-3,  1e-4,  1e-5,  1e-6,  1e-7,  1e-8,  1e-9,
  1e-10, 1e-11, 1e-12, 1e-13, 1e-14, 1e-15, 1e-16, 1e-17, 1e-18,  1e-19,
  1e-20, 1e-21, 1e-22, 1e-23, 1e-24, 1e-25, 1e-26, 1e-27, 1e-28,  1e-29,
  1e-30, 1e-31, 1e-32, 1e-33, 1e-34, 1e-35, 1e-36, 1e-37, 1e-38,  1e-39,
  1e-40, 1e-41, 1e-42, 1e-43, 1e-44, 1e-45, 1e-46, 1e-47, 1e-48,  1e-49,
  1e-50, 1e-51, 1e-52, 1e-53, 1e-54, 1e-55, 1e-56, 1e-57, 1e-58,  1e-59,
};

#ifndef DBL_MAX
#define DBL_MAX 1.7976931348623157e+308
#endif

double strtod(const char *zNum, char **pzEnd){
  double rResult = 0.0;
  int isNegative = 0;

  while( isspace(*zNum) ){
    zNum++;
			}
  if( *zNum=='-' ){
    zNum++;
    isNegative = 1;
  }else if( *zNum=='+' ){
    zNum++;
			}
  while( isdigit(*zNum) ){
    rResult = rResult*10.0 + (*zNum - '0');
    zNum++;
			}
  if( *zNum=='.' ){
    int n = 0;
    zNum++;
    while( isdigit(*zNum) ){
      if( n<sizeof(pastpoint)/sizeof(pastpoint[0]) ){
        rResult += pastpoint[n] * (*zNum - '0');
        n++;
					}
      zNum++;
				}
				}
  if( *zNum=='e' || *zNum=='E' ){
    int expVal = 0;
    int isNegExp = 0;
    const char *zExpStart = zNum;
    zNum++;
    if( *zNum=='-' ){
      isNegExp = 1;
      zNum++;
    }else if( *zNum=='+' ){
      zNum++;
				}
    if( !isdigit(*zNum) ){
      zNum = zExpStart;
    }else{
      double scaler = 1.0;
      while( isdigit(*zNum) ){
        expVal = expVal*10 + *zNum - '0';
        zNum++;
			}
      if( expVal >= 1000 ){
        if( isNegExp ){
          rResult = 0.0;
        }else{
          rResult = DBL_MAX;
				}
        goto done;
				}
      while( expVal >= 100 ){
        scaler *= 1.0e100;
        expVal -= 100;
			}
      scaler *= scaler10[expVal/10]*scaler1[expVal%10];
      if( isNegExp ){
        scaler = 1.0/scaler;
				}
      rResult *= scaler;
			}

	}

done:
  if( pzEnd ){
    *pzEnd = (char *)zNum;
		}
  if( isNegative && rResult!=0.0 ){
    rResult = -rResult;
	}
  return rResult;
}

double atof(const char *nptr)
{
  return (strtod(nptr, 0));
}

#ifdef TEST
#undef strtod

double strtod(const char*,char**);
double NewStrtod(const char*,char**);

int main(int argc, char **argv){
  int nTest = 0;
  int nFail = 0;
  int nBigFail = 0;
  char zBuf[1000];

  while( fgets(zBuf,sizeof(zBuf),stdin) ){
    double old, new;
    char *zTailOld, *zTailNew;
    int i;

    for(i=0; zBuf[i] && zBuf[i]!='\n'; i++){}
    zBuf[i] = 0;

#if TEST==1
    printf("Input line: [%s]\n",zBuf);
    old = strtod(zBuf,&zTailOld);
    printf("value=%g\n",old);
    printf("Old:        0x%08x%08x  tail=[%s]\n",
           ((int*)&old)[1], ((int*)&old)[0], zTailOld);
    new = NewStrtod(zBuf,&zTailNew);
    printf("value=%g\n",new);
    printf("New:        0x%08x%08x  tail=[%s]\n\n",
           ((int*)&new)[1], ((int*)&new)[0], zTailNew);
#else
    old = strtod(zBuf,&zTailOld);
    new = NewStrtod(zBuf,&zTailNew);
    nTest++;
    if( strcmp(zTailOld,zTailNew) 
        || ((int*)&old)[0]!=((int*)&new)[0]
        || ((int*)&old)[1]!=((int*)&new)[1]
        ){
      int olda, oldb, newa, newb;

      nFail++;
      olda = ((int*)&old)[1];
      oldb = ((int*)&old)[0];
      newa = ((int*)&new)[1];
      newb = ((int*)&new)[0];

      if( olda!=newa || abs(oldb-newb)>2 ){
        nBigFail++;
        printf("******* Big failure \n");
			}
      printf("Input = [%s]\n",zBuf);
      printf("old:   val=%g   0x%08x%08x  tail=[%s]\n",
             old, olda, oldb, zTailOld);
      printf("new:   val=%g   0x%08x%08x  tail=[%s]\n\n",
             new, newa, newb, zTailNew);
				}
#endif
			}

  printf("Out of %d tests, %d failures and %d big failurs\n",
         nTest,nFail, nBigFail);
}
#endif
