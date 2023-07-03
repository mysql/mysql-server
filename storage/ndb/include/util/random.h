/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.
    Use is subject to license terms.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

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

