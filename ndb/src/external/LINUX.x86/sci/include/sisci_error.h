/* $Id: sisci_error.h,v 1.1 2002/12/13 12:17:21 hin Exp $  */

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




#ifndef _SISCI_ERROR_H_
#define _SISCI_ERROR_H_


/* SCI Error return values always have 30 bit  set */
#define SCI_ERR_MASK                0x40000000
#define SCI_ERR_REMOTE_MASK            0x01        /* Remote errors should have bit 0 set */

#define SCI_ERR(u) ((unsigned32)(u)&0x7FFFFFFF )

/* Error codes */
typedef enum {
   SCI_ERR_OK                         = 0x000,


   SCI_ERR_BUSY              	      = (0x900  | SCI_ERR_MASK), 
   SCI_ERR_FLAG_NOT_IMPLEMENTED       = (0x901  | SCI_ERR_MASK),  
   SCI_ERR_ILLEGAL_FLAG               = (0x902  | SCI_ERR_MASK),  
   SCI_ERR_NOSPC                      = (0x904  | SCI_ERR_MASK),  
   SCI_ERR_API_NOSPC                  = (0x905  | SCI_ERR_MASK),         
   SCI_ERR_HW_NOSPC                   = (0x906  | SCI_ERR_MASK),  
   SCI_ERR_NOT_IMPLEMENTED            = (0x907  | SCI_ERR_MASK),  
   SCI_ERR_ILLEGAL_ADAPTERNO          = (0x908  | SCI_ERR_MASK),   
   SCI_ERR_NO_SUCH_ADAPTERNO          = (0x909  | SCI_ERR_MASK),
   SCI_ERR_TIMEOUT                    = (0x90A  | SCI_ERR_MASK),
   SCI_ERR_OUT_OF_RANGE               = (0x90B  | SCI_ERR_MASK),
   SCI_ERR_NO_SUCH_SEGMENT            = (0x90C  | SCI_ERR_MASK),
   SCI_ERR_ILLEGAL_NODEID             = (0x90D  | SCI_ERR_MASK),
   SCI_ERR_CONNECTION_REFUSED         = (0x90E  | SCI_ERR_MASK),
   SCI_ERR_SEGMENT_NOT_CONNECTED      = (0x90F  | SCI_ERR_MASK),
   SCI_ERR_SIZE_ALIGNMENT             = (0x910  | SCI_ERR_MASK),
   SCI_ERR_OFFSET_ALIGNMENT           = (0x911  | SCI_ERR_MASK),
   SCI_ERR_ILLEGAL_PARAMETER          = (0x912  | SCI_ERR_MASK),
   SCI_ERR_MAX_ENTRIES                = (0x913  | SCI_ERR_MASK),   
   SCI_ERR_SEGMENT_NOT_PREPARED       = (0x914  | SCI_ERR_MASK),
   SCI_ERR_ILLEGAL_ADDRESS            = (0x915  | SCI_ERR_MASK),
   SCI_ERR_ILLEGAL_OPERATION          = (0x916  | SCI_ERR_MASK),
   SCI_ERR_ILLEGAL_QUERY              = (0x917  | SCI_ERR_MASK),
   SCI_ERR_SEGMENTID_USED             = (0x918  | SCI_ERR_MASK),
   SCI_ERR_SYSTEM                     = (0x919  | SCI_ERR_MASK),
   SCI_ERR_CANCELLED                  = (0x91A  | SCI_ERR_MASK),
   SCI_ERR_NOT_CONNECTED              = (0x91B  | SCI_ERR_MASK),
   SCI_ERR_NOT_AVAILABLE              = (0x91C  | SCI_ERR_MASK),
   SCI_ERR_INCONSISTENT_VERSIONS      = (0x91D  | SCI_ERR_MASK),
   SCI_ERR_COND_INT_RACE_PROBLEM      = (0x91E  | SCI_ERR_MASK),
   SCI_ERR_OVERFLOW                   = (0x91F  | SCI_ERR_MASK),
   SCI_ERR_NOT_INITIALIZED            = (0x920  | SCI_ERR_MASK),

   SCI_ERR_ACCESS                     = (0x921  | SCI_ERR_MASK),

   SCI_ERR_NO_SUCH_NODEID             = (0xA00  | SCI_ERR_MASK),
   SCI_ERR_NODE_NOT_RESPONDING        = (0xA02  | SCI_ERR_MASK),  
   SCI_ERR_NO_REMOTE_LINK_ACCESS      = (0xA04  | SCI_ERR_MASK),
   SCI_ERR_NO_LINK_ACCESS             = (0xA05  | SCI_ERR_MASK),
   SCI_ERR_TRANSFER_FAILED            = (0xA06  | SCI_ERR_MASK)
} sci_error_t;


#endif /* _SCI_ERROR_H_ */



