/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/***************************************************************
* I N C L U D E D   F I L E S                                  *
***************************************************************/

#include <ndb_global.h>

#include <NdbOut.hpp>

#include <random.h>

/***************************************************************
* L O C A L   C O N S T A N T S                                *
***************************************************************/

/***************************************************************
* L O C A L   D A T A   S T R U C T U R E S                    *
***************************************************************/

typedef struct {
    unsigned short int x[3];	/* Current state.  */
    unsigned short int a[3];	/* Factor in congruential formula.  */
    unsigned short int c;	/* Additive const. in congruential formula.  */
    int init;			/* Flag for initializing.  */
}DRand48Data;

/***************************************************************
* L O C A L   F U N C T I O N S                                *
***************************************************************/

static void shuffleSequence(RandomSequence *seq);

/***************************************************************
* L O C A L   D A T A                                          *
***************************************************************/

static DRand48Data dRand48Data;

/***************************************************************
* P U B L I C   D A T A                                        *
***************************************************************/


/***************************************************************
****************************************************************
* L O C A L   F U N C T I O N S   C O D E   S E C T I O N      *
****************************************************************
***************************************************************/

static void localRandom48Init(long int seedval, DRand48Data *buffer)
{
   /* The standards say we only have 32 bits.  */
   if (sizeof (long int) > 4)
      seedval &= 0xffffffffl;

#if USHRT_MAX == 0xffffU
  buffer->x[2] = seedval >> 16;
  buffer->x[1] = seedval & 0xffffl;
  buffer->x[0] = 0x330e;

  buffer->a[2] = 0x5;
  buffer->a[1] = 0xdeec;
  buffer->a[0] = 0xe66d;
#else
  buffer->x[2] = seedval;
  buffer->x[1] = 0x330e0000UL;
  buffer->x[0] = 0;

  buffer->a[2] = 0x5deecUL;
  buffer->a[1] = 0xe66d0000UL;
  buffer->a[0] = 0;
#endif

  buffer->c    = 0xb;
  buffer->init = 1;
}

static void localRandom48(DRand48Data *buffer, long int *result)
{
   Uint64 X;
   Uint64 a;
   Uint64 loc_result;

   /*--------------------------------------*/
   /* Initialize buffer, if not yet done.  */
   /*--------------------------------------*/
   if (!buffer->init) {
#if (USHRT_MAX == 0xffffU)
      buffer->a[2] = 0x5;
      buffer->a[1] = 0xdeec;
      buffer->a[0] = 0xe66d;
#else
      buffer->a[2] = 0x5deecUL;
      buffer->a[1] = 0xe66d0000UL;
      buffer->a[0] = 0;
#endif
      buffer->c    = 0xb;
      buffer->init = 1;
   }

   /* Do the real work.  We choose a data type which contains at least
      48 bits.  Because we compute the modulus it does not care how
      many bits really are computed.  */

   if (sizeof (unsigned short int) == 2) {
      X = (Uint64)buffer->x[2] << 32 | 
          (Uint64)buffer->x[1] << 16 | 
           buffer->x[0];
      a = ((Uint64)buffer->a[2] << 32 |
           (Uint64)buffer->a[1] << 16 |
	    buffer->a[0]);

      loc_result = X * a + buffer->c;

      buffer->x[0] = loc_result & 0xffff;
      buffer->x[1] = (loc_result >> 16) & 0xffff;
      buffer->x[2] = (loc_result >> 32) & 0xffff;
   }
   else {
      X = (Uint64)buffer->x[2] << 16 | 
           buffer->x[1] >> 16;
      a = (Uint64)buffer->a[2] << 16 |
           buffer->a[1] >> 16;

      loc_result = X * a + buffer->c;

      buffer->x[0] = loc_result >> 16 & 0xffffffffl;
      buffer->x[1] = loc_result << 16 & 0xffff0000l;
   }

   /*--------------------*/
   /* Store the result.  */
   /*--------------------*/
   if (sizeof (unsigned short int) == 2)
      *result = buffer->x[2] << 15 | buffer->x[1] >> 1;
   else
      *result = buffer->x[2] >> 1;
}

static void shuffleSequence(RandomSequence *seq)
{
   unsigned int i;
   unsigned int j;
   unsigned int tmp;

   if( !seq ) return;

   for(i = 0; i < seq->length; i++ ) {
      j = myRandom48(seq->length);
      if( i != j ) {
         tmp = seq->values[i];
         seq->values[i] = seq->values[j];
         seq->values[j] = tmp;
      }
   }
}


/***************************************************************
****************************************************************
* P U B L I C   F U N C T I O N S   C O D E   S E C T I O N    *
****************************************************************
***************************************************************/


double getTps(unsigned int count, double timeValue)
{
   double f;

   if( timeValue != 0.0 )
      f = count / timeValue;
   else
      f = 0.0;

   return(f);
}

/*----------------------------*/
/* Random Sequences Functions */
/*----------------------------*/
int initSequence(RandomSequence *seq, SequenceValues *inputValues)
{
   unsigned int i;
   unsigned int j;
   unsigned int totalLength;
   unsigned int index;

   if( !seq || !inputValues ) return(-1);

   /*------------------------------------*/
   /* Find the total length of the array */
   /*------------------------------------*/
   totalLength = 0;

   for(i = 0; inputValues[i].length != 0; i++)
      totalLength += inputValues[i].length;

   if( totalLength == 0 ) return(-1);

   seq->length = totalLength;
   seq->values = calloc(totalLength, sizeof(unsigned int));

   if( seq->values == 0 ) return(-1);

   /*----------------------*/
   /* set the array values */
   /*----------------------*/
   index = 0;

   for(i = 0; inputValues[i].length != 0; i++) {
      for(j = 0; j < inputValues[i].length; j++ ) {
         seq->values[index] = inputValues[i].value;
         index++;
      }
   }

   shuffleSequence(seq);

   seq->currentIndex = 0;

   return(0);
}

unsigned int getNextRandom(RandomSequence *seq)
{
  unsigned int nextValue;
  
  nextValue = seq->values[seq->currentIndex];

  seq->currentIndex++;

  if(seq->currentIndex == seq->length){
    seq->currentIndex = 0;
    shuffleSequence(seq);
  }

  return nextValue;
}

void printSequence(RandomSequence *seq, unsigned int numPerRow)
{
   unsigned int i;

   if( !seq ) return;

   for(i = 0; i<seq->length; i++) {
      ndbout_c("%d ", seq->values[i]);

      if((i+1) % numPerRow == 0)
         ndbout_c("");
   }

   if(i % numPerRow != 0)
      ndbout_c("");
}

void myRandom48Init(long int seedval)
{
   localRandom48Init(seedval, &dRand48Data);
}

long int myRandom48(unsigned int maxValue)
{
   long int result;

   localRandom48(&dRand48Data, &result);

   return(result % maxValue);
}
