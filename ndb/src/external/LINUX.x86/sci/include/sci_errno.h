/* $Id: sci_errno.h,v 1.1 2002/12/13 12:17:20 hin Exp $  */

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



#ifndef _SCI_ERRNO_H_
#define _SCI_ERRNO_H_


/*
 * SCI Error return values always have 30 bit set
 * Remote errors should have bit 0 set 
 */
#define SCI_ERR_MASK        0x40000000
#define ESCI_REMOTE_MASK    0x01000000    

#define SCI_ERR(u) ((unsigned32)(u)&0x7FFFFFFF )
#define _SCI_ERROR(x) ((x) | SCI_ERR_MASK) 
#define _SCI_REMOTE_ERROR(x) ( _SCI_ERROR(x) | ESCI_REMOTE_MASK )

/*
 * Error codes
 */
typedef enum {
    ESCI_OK                          = 0x000,
    ESCI_STILL_EXPORTED              = _SCI_ERROR(0x800),

    ESCI_BUS_ERR                     = _SCI_ERROR(0x900), 
    ESCI_PEND_SCIERR                 = _SCI_ERROR(0x901),
    ESCI_SCI_ERR                     = _SCI_ERROR(0x902),
    
    /*
     * Specific SCI error responses:
     */
    ESCI_SCI_ERR_DATA                = _SCI_ERROR(0x9021),
    ESCI_SCI_ERR_TYPE                = _SCI_ERROR(0x9022),
    ESCI_SCI_ERR_ADDR                = _SCI_ERROR(0x9023),

    ESCI_LINK_TIMEOUT                = _SCI_ERROR(0x903),
    ESCI_EXDEV_TIMEOUT               = _SCI_ERROR(0x904),
    ESCI_REMOTE_ERR                  = _SCI_ERROR(0x905),
    ESCI_MBX_BUSY                    = _SCI_ERROR(0x906),
    ESCI_DMAERR                      = _SCI_ERROR(0x907),
    ESCI_DMA_DISABLED                = _SCI_ERROR(0x908),
    ESCI_SW_MBX_SEND_FAILED          = _SCI_ERROR(0x909),
    ESCI_HW_MBX_SEND_FAILED          = _SCI_ERROR(0x90A),
    ESCI_HAS_NO_SESSION              = _SCI_ERROR(0xA00), 
    ESCI_CONNREFUSED_SESSION         = _SCI_ERROR(0xA01),
    ESCI_SESSION_NOT_ESTABLISHED     = _SCI_ERROR(0xA11),
    ESCI_REMOTE_NO_VALID_SESSION     = _SCI_ERROR(0xA02), 
    ESCI_SESSION_DISABLED            = _SCI_ERROR(0xA03),     
    ESCI_NODE_CLOSED                 = _SCI_ERROR(0xA04),
    ESCI_NODE_DISABLED               = _SCI_ERROR(0xA05),

    ESCI_LOCAL_MASTER_ERR            = _SCI_ERROR(0xA06),
    ESCI_REMOTE_MASTER_ERR           = _SCI_REMOTE_ERROR(0xA06),

    ESCI_ILLEGAL_CMD_RECEIVED        = _SCI_ERROR(0xA08),
    ESCI_ILLEGAL_CMD_SENT            = _SCI_ERROR(0xA09),

    /* used above:  ESCI_SESSION_NOT_ESTABLISHED     = _SCI_ERROR(0xA11), */

    /*
     * Remote error codes
     */            
    ESCI_CONNREFUSED                 = _SCI_ERROR(0xB00),
    ESCI_NODE_NOT_RESPONDING         = _SCI_ERROR(0xB01),
    ESCI_ISCONN                      = _SCI_ERROR(0xB02),
    ESCI_HOSTUNREACH                 = _SCI_ERROR(0xB03),
    ESCI_NO_SUCH_USER_ID             = _SCI_ERROR(0xB04),
    ESCI_REMOTE_NO_SUCH_USER_ID      = _SCI_REMOTE_ERROR(0xB04),  /* ESCI_NO_SUCH_USER_ID        */
    ESCI_NO_SUCH_KEY                 = _SCI_ERROR(0xB04),         /* ESCI_NO_SUCH_USER_ID        */
    ESCI_REMOTE_NO_SUCH_KEY          = _SCI_REMOTE_ERROR(0xB04),  /* ESCI_REMOTE_NO_SUCH_USER_ID */
    ESCI_NODE_ERR                    = _SCI_ERROR(0xB06),
    ESCI_REMOTE_NODE_ERR             = _SCI_REMOTE_ERROR(0xB06),  /* ESCI_NODE_ERR               */
    ESCI_NOSPC                       = _SCI_ERROR(0xB08),
    ESCI_REMOTE_NOSPC                = _SCI_REMOTE_ERROR(0xB08),  /* ESCI_NOSPC                  */
    ESCI_NODMASPC                    = _SCI_ERROR(0xB0A), 
    ESCI_REMOTE_NODMASPC             = _SCI_REMOTE_ERROR(0xB0A),  /* ESCI_NODMASPC               */
    ESCI_NOTMAP                      = _SCI_ERROR(0xC00), 
    ESCI_ISMAP                       = _SCI_ERROR(0xC01), 
    ESCI_NOT_INITIALIZED             = _SCI_ERROR(0xD00),
    ESCI_REMOTE_NOT_INITIALIZED      = _SCI_REMOTE_ERROR(ESCI_NOT_INITIALIZED),
    /*
     * ???
     */
    ESCI_PARAM_ERR                   = _SCI_ERROR(0xD01),
    ESCI_NO_FREE_VC                  = _SCI_ERROR(0xD02),   
    ESCI_REMOTE_NO_FREE_VC           = _SCI_REMOTE_ERROR(0xD02),  /* ESCI_NO_FREE_VC             */ 

    /*
     * Adapter state related error codes:
     */
    ESCI_SUSPENDED                   = _SCI_ERROR(0xD03),
    ESCI_NOT_SUSPENDED               = _SCI_ERROR(0xD04),
    ESCI_NOT_READY                   = _SCI_ERROR(0xD05),
    ESCI_NOT_CONFIGURED              = _SCI_ERROR(0xD06),
    ESCI_INVALID_ADAPTERID           = _SCI_ERROR(0xD07), /* if an adapter-id is out of range */
    ESCI_NONEXIST_ADAPTERID          = _SCI_ERROR(0xD08), /* if adapter-id is valid but no adapter matches */
    ESCI_ADAPTERID_INUSE             = _SCI_ERROR(0xD09),

    ESCI_INVALID_INSTANCE            = _SCI_ERROR(0xD0A),
    ESCI_NONEXIST_INSTANCE           = _SCI_ERROR(0xD0B), 

    ESCI_ADAPTER_INIT_FAILURE        = _SCI_ERROR(0xD0C),

    ESCI_PAUSED                      = _SCI_ERROR(0xD0D),
    ESCI_NOT_PAUSED                  = _SCI_ERROR(0xD0E),
    ESCI_ADAPTER_NEED_RESET          = _SCI_ERROR(0xD0F),

    ESCI_NONEXIST_SERIAL_NUMBER      = _SCI_ERROR(0xD10),
    ESCI_NOT_AVAILABLE               = _SCI_ERROR(0xD11),

    ESCI_EACCESS                     = _SCI_ERROR(0xD12),

    /* 
     * Local error codes
     */
    ESCI_NO_LOCAL_ACCESS             = _SCI_ERROR(0xE00),       
    ESCI_LRESOURCE_BUSY              = _SCI_ERROR(0xE01),
    ESCI_LRESOURCE_EXIST             = _SCI_ERROR(0xE02),  
    ESCI_NO_LRESOURCE                = _SCI_ERROR(0xE03),
    ESCI_NOTCONN                     = _SCI_ERROR(0xE04),  
    ESCI_LOCAL_ERR                   = _SCI_ERROR(0xE05),  
    ESCI_NOVAL_NODEID                = _SCI_ERROR(0xE06),
    ESCI_NOT_SUPPORTED               = _SCI_ERROR(0xE07),
    ESCI_TIMEOUT                     = _SCI_ERROR(0xE08),
    ESCI_NO_LOCAL_LC_ACCESS          = _SCI_ERROR(0xE0A), 
    ESCI_INVALID_ATT                 = _SCI_ERROR(0xE0B),
    ESCI_BAD_CHECKSUM                = _SCI_ERROR(0xE0C),
    ESCI_INTERRUPT_FLAG_DISABLED     = _SCI_ERROR(0xE0D),
    ESCI_COND_INT_RACE_PROBLEM       = _SCI_ERROR(0xE0E),
    ESCI_OVERFLOW                    = _SCI_ERROR(0xE0F),
    ESCI_BLINK_PARITY_ERROR          = _SCI_ERROR(0xE10),
    ESCI_FIRMWARE_VERSION_MISMATCH   = _SCI_ERROR(0xE11),

    /*
     * Link error codes
     */
    ESCI_NO_LINK_ACCESS              = _SCI_ERROR(0xF00),  
    ESCI_NO_REMOTE_LINK_ACCESS       = _SCI_REMOTE_ERROR(0xF00),  /* ESCI_NO_LINK_ACCESS         */

    ESCI_NO_SUCH_NODE                = _SCI_ERROR(0xF02),
    ESCI_USR_ACCESS_DISABLED         = _SCI_ERROR(0xF03),
    ESCI_HW_AVOID_DEADLOCK           = _SCI_ERROR(0xF04),
    ESCI_POTENTIAL_ERROR             = _SCI_ERROR(0xF05),

    ESCI_FENCED                      = _SCI_ERROR(0xF06),
    ESCI_SWITCH_HW_FAILURE           = _SCI_ERROR(0xF07),
    ESCI_SWITCH_WRONG_BLINK_ID       = _SCI_ERROR(0xF08),
    ESCI_SWITCH_WRONG_PORT_NUMB      = _SCI_ERROR(0xF09),
    ESCI_SWITCH_WRONG_INIT_TYPE      = _SCI_ERROR(0xF0A),  /* It is determined that the swith initialization
                                                            * do not match the local adapter initialization
                                                            */
    ESCI_SWITCH_WRONG_SWITCH_NUMB    = _SCI_ERROR(0xF0B),  /* It is determined that we are operationg on the
                                                            * wrong switch port 
                                                            */
    ESCI_SWITCH_NOT_CONNECTED        = _SCI_ERROR(0xF0C),
    ESCI_SWITCH_NOT_RECOGNIZED       = _SCI_ERROR(0xF0D),
    ESCI_SWITCH_INIT_IN_PROGRESS     = _SCI_ERROR(0xF0E),  /* Switch TINI initialization in progress */


    ESCI_NO_BACKBONE_LINK_ACCESS     = _SCI_ERROR(0xF20),  
    ESCI_BACKBONE_FENCED             = _SCI_ERROR(0xF21),  
    ESCI_NO_BACKBONE_ACCESS          = _SCI_ERROR(0xF22),  
    ESCI_BACKBONE_CABLE_PROBLEM      = _SCI_ERROR(0xF23),  
    ESCI_BACKBONE_BLINK_PROBLEM      = _SCI_ERROR(0xF24),  
    ESCI_BACKBONE_HWINIT_PROBLEM     = _SCI_ERROR(0xF25),  
    ESCI_BACKBONE_ID_PROBLEM         = _SCI_ERROR(0xF26),  
    ESCI_BACKBONE_STATE_PROBLEM      = _SCI_ERROR(0xF27),  
    ESCI_BACKBONE_REQ_LINK_PROBLEM   = _SCI_ERROR(0xF28),  
    ESCI_BACKBONE_UNFENCING          = _SCI_ERROR(0xF29),   /* Unfencing in progress */

    /*
     * added for pci port
     */
    ESCI_AGAIN                       = _SCI_ERROR(0xF15),
    ESCI_ORANGE                      = _SCI_ERROR(0xF16), /* Out of range */
    ESCI_NOSYS                       = _SCI_ERROR(0xF17), /* Used instead of ENOSYS. Means function not implemented */
    ESCI_REMOTE_NOSYS                = _SCI_REMOTE_ERROR(ESCI_NOSYS), 
    ESCI_INTR                        = _SCI_ERROR(0xF18), /* Used instead of EINTR from sys/errno.h */
    ESCI_IO                          = _SCI_ERROR(0xF19), /* Used instead of EIO from sys/errno.h */
    ESCI_FAULT                       = _SCI_ERROR(0xF1A), /* Used instead of EFAULT from sys/errno.h */
    ESCI_BUSY                        = _SCI_ERROR(0xF1B), /* Used instead of EBUST from sys/errno.h */
    ESCI_INVAL                       = _SCI_ERROR(0xF1C), /* Used instead of EINVAL from sys/errno.h */
    ESCI_NXIO                        = _SCI_ERROR(0xF1D), /* Used instead of ENXIO from sys/errno.h */
    ESCI_EXIST                       = _SCI_ERROR(0xF1E)  /* Used instead of EEXIST from sys/errno.h */

} scierror_t;

#endif /* _SCI_ERRNO_H_ */




