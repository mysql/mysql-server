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

#ifndef DBGENERATOR_H
#define DBGENERATOR_H

/***************************************************************
* I N C L U D E D   F I L E S                                  *
***************************************************************/

#include "testData.h"
#include "userInterface.h"

/***************************************************************
* M A C R O S                                                  *
***************************************************************/

/***************************************************************/
/* C O N S T A N T S                                           */
/***************************************************************/

/***************************************************************
* D A T A   S T R U C T U R E S                                *
***************************************************************/

/***************************************************************
* P U B L I C    F U N C T I O N S                             *
***************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

extern void asyncGenerator(ThreadData *d, int parallellism,
			   int millisSendPoll, 
			   int minEventSendPoll, 
			   int forceSendPoll);

#ifdef __cplusplus
}
#endif

/***************************************************************
* E X T E R N A L   D A T A                                    *
***************************************************************/



#endif /* DBGENERATOR_H */

