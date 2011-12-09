/*
   Copyright (C) 2005, 2006 MySQL AB
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef DBPOPULATE_H
#define DBPOPULATE_H

/***************************************************************
* I N C L U D E D   F I L E S                                  *
***************************************************************/

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

extern void dbPopulate(UserHandle *uh);

#ifdef __cplusplus
}
#endif

/***************************************************************
* E X T E R N A L   D A T A                                    *
***************************************************************/



#endif /* DBPOPULATE_H */

