/* $Id: sisci_demolib.h,v 1.1 2002/12/13 12:17:23 hin Exp $  */

/*******************************************************************************
 *                                                                             *
 * Copyright (C) 1993 - 2000                                                   * 
 *         Dolphin Interconnect Solutions AS                                   *
 *                                                                             *
 * This program is free software; you can redistribute it and/or modify        * 
 * it under the terms of the GNU General Public License as published by        *
 * the Free Software Foundation; either version 2 of the License,              *
 * or (at your option) any later version.                                      *
 *                                                                             *
 * This program is distributed in the hope that it will be useful,             *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the               *
 * GNU General Public License for more details.                                *
 *                                                                             *
 * You should have received a copy of the GNU General Public License           *
 * along with this program; if not, write to the Free Software                 *
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA. *
 *                                                                             *
 *                                                                             *
 *******************************************************************************/ 

#ifndef _SISCI_DEMOLIB_H
#define _SISCI_DEMOLIB_H


#if defined(_REENTRANT)

#define _SISCI_DEMOLIB_EXPAND_NAME(name)  _SISCI_DEMOLIB_MT_ ## name

#else

#define _SISCI_DEMOLIB_EXPAND_NAME(name)  _SISCI_DEMOLIB_ST_ ## name

#endif

/*********************************************************************************/
/*                     Q U E R Y   A D A P T E R                                 */
/*                                                                               */
/*********************************************************************************/

#define QueryAdapter              _SISCI_DEMOLIB_EXPAND_NAME(QueryAdapter)

sci_error_t QueryAdapter(
                    unsigned int subcommand,
                    unsigned int localAdapterNo,
                    unsigned int portNo,
                    unsigned int *data);


/*********************************************************************************/
/*                     Q U E R Y   S Y S T E M                                   */
/*                                                                               */
/*********************************************************************************/

#define QuerySystem               _SISCI_DEMOLIB_EXPAND_NAME(QuerySystem)

sci_error_t QuerySystem(
                    unsigned int subcommand,
                    unsigned int *data);


/*********************************************************************************/
/*             D E T E C T   F I R S T   A D A P T E R   C A R D                 */
/*                                                                               */
/*********************************************************************************/

#define DetectFirstAdapterCard    _SISCI_DEMOLIB_EXPAND_NAME(DetectFirstAdapterCard)

sci_error_t DetectFirstAdapterCard(
                    unsigned int *localAdapterNo,
                    unsigned int *localNodeId);


/*********************************************************************************/
/*                   G E T   A D A P T E R   T Y P E                             */
/*                                                                               */
/*********************************************************************************/

#define GetAdapterType    _SISCI_DEMOLIB_EXPAND_NAME(GetAdapterType)

sci_error_t GetAdapterType(unsigned int localAdapterNo, 
                           unsigned int *adapterType);


/*********************************************************************************/
/*                      G E T   L O C A L   N O D E I D                          */
/*                                                                               */
/*********************************************************************************/

#define GetLocalNodeId            _SISCI_DEMOLIB_EXPAND_NAME(GetLocalNodeId)

sci_error_t GetLocalNodeId(
                    unsigned int localAdapterNo,
                    unsigned int *localNodeId);


/*********************************************************************************/
/*            G E T   A D A P T E R   S E R I A L   N U M B E R                  */
/*                                                                               */
/*********************************************************************************/

#define GetAdapterSerialNumber    _SISCI_DEMOLIB_EXPAND_NAME(GetAdapterSerialNumber)

sci_error_t GetAdapterSerialNumber(
                     unsigned int localAdapterNo, 
                     unsigned int *serialNo);



/*********************************************************************************/
/*                  G E T   H O S T B R I D G E   T Y P E                        */
/*                                                                               */
/*********************************************************************************/

#define GetHostbridgeType       _SISCI_DEMOLIB_EXPAND_NAME(GetHostbridgeType)

sci_error_t GetHostbridgeType(unsigned int *hostbridgeType);



/*********************************************************************************/
/*                P R I N T   H O S T B R I D G E   T Y P E                      */
/*                                                                               */
/*********************************************************************************/

#define PrintHostbridgeType       _SISCI_DEMOLIB_EXPAND_NAME(PrintHostbridgeType)

void PrintHostbridgeType(unsigned int hostbridge);



/*********************************************************************************/
/*               G E T   A P I   V E R S I O N   S T R I N G                     */
/*                                                                               */
/*********************************************************************************/

#define GetAPIVersionString       _SISCI_DEMOLIB_EXPAND_NAME(GetAPIVersionString)

sci_error_t GetAPIVersionString(char str[], unsigned int strLength);



/*********************************************************************************/
/*         G E T   A D A P T E R   I O   B U S    F R E Q U E N C Y              */
/*                                                                               */
/*********************************************************************************/

sci_error_t GetAdapterIoBusFrequency(unsigned int localAdapterNo,
                                     unsigned int *ioBusFrequency);



/*********************************************************************************/
/*      G E T   A D A P T E R   S C I   L I N K   F R E Q U E N C Y              */
/*                                                                               */
/*********************************************************************************/

sci_error_t GetAdapterSciLinkFrequency(unsigned int localAdapterNo,
                                       unsigned int *sciLinkFrequency);



/*********************************************************************************/
/*          G E T   A D A P T E R   B L I N K   F R E Q U E N C Y                */
/*                                                                               */
/*********************************************************************************/

sci_error_t GetAdapterBlinkFrequency(unsigned int localAdapterNo,
                                     unsigned int *bLinkFrequency);


/*********************************************************************************/
/*                       S E N D   I N T E R R U P T                             */
/*                                                                               */
/*********************************************************************************/

#define SendInterrupt             _SISCI_DEMOLIB_EXPAND_NAME(SendInterrupt)

sci_error_t SendInterrupt(
                    sci_desc_t   sd,
                    unsigned int localAdapterNo, 
                    unsigned int localNodeId, 
                    unsigned int remoteNodeId,
                    unsigned int interruptNo);


/*********************************************************************************/
/*                    R E C E I V E   I N T E R R U P T                          */
/*                                                                               */
/*********************************************************************************/

#define ReceiveInterrupt          _SISCI_DEMOLIB_EXPAND_NAME(ReceiveInterrupt)

sci_error_t ReceiveInterrupt(
                    sci_desc_t   sd,
                    unsigned int localAdapterNo,
                    unsigned int localNodeId,
                    unsigned int interruptNo);


/*********************************************************************************/
/*                           E N D I A N   S W A P                               */
/*                                                                               */
/*********************************************************************************/

#define EndianSwap                _SISCI_DEMOLIB_EXPAND_NAME(EndianSwap)

unsigned int EndianSwap (unsigned int  value);


/*********************************************************************************/
/*                    S L E E P   M I L L I S E C O N D S                        */
/*                                                                               */
/*********************************************************************************/

#define SleepMilliseconds         _SISCI_DEMOLIB_EXPAND_NAME(SleepMilliseconds)

void SleepMilliseconds(int  milliseconds);




#endif
