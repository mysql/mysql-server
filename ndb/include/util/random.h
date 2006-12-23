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

#ifndef RANDOM_H
#define RANDOM_H

/***************************************************************
* I N C L U D E D   F I L E S                                  *
***************************************************************/

/***************************************************************
* M A C R O S                                                  *
***************************************************************/

/***************************************************************/
/* C O N S T A N T S                                           */
/***************************************************************/


/***************************************************************
* D A T A   S T R U C T U R E S                                *
***************************************************************/

typedef struct {
   unsigned int  length;
   unsigned int *values;
   unsigned int  currentIndex;
}RandomSequence;

typedef struct {
   unsigned int length;
   unsigned int value;
}SequenceValues;

/***************************************************************
* P U B L I C    F U N C T I O N S                             *
***************************************************************/

#ifdef __cplusplus
extern "C" {
#endif


extern double getTps(unsigned int count, double timeValue);

/*----------------------------*/
/* Random Sequences Functions */
/*----------------------------*/
extern int  initSequence(RandomSequence *seq, SequenceValues *inputValues);
extern unsigned int getNextRandom(RandomSequence *seq);
extern void printSequence(RandomSequence *seq, unsigned int numPerRow);

/*---------------------------------------------------*/
/* Code from the glibc, to make sure the same random */
/* number generator is used by all                   */
/*---------------------------------------------------*/
extern void myRandom48Init(long int seedval);
extern long int myRandom48(unsigned int maxValue);

#ifdef __cplusplus
}
#endif

/***************************************************************
* E X T E R N A L   D A T A                                    *
***************************************************************/



#endif /* RANDOM_H */

