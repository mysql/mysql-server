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


#if defined NDB_OSE || defined NDB_SOFTOSE

#include <NdbOut.hpp>
#include <ndb_types.h>

#include "ose.h"
#include "ose_err.h"
#include "osetypes.h"


#define BUFSIZE 100

typedef struct {
  char header1[BUFSIZE];
  char header2[BUFSIZE];
  char error_code_line[BUFSIZE];
  char subcode_line[BUFSIZE];
  char product_line[BUFSIZE];
  char header_file_line[BUFSIZE];
  char extra_line[BUFSIZE];
  char user_called_line[BUFSIZE];
  char current_process_id_line[BUFSIZE];
  char current_process_name_line[BUFSIZE];
  char file_line[BUFSIZE];
  char line_line[BUFSIZE];
  char err_hnd_file[BUFSIZE];
} Error_message;

char assert_line[BUFSIZE];
char unknown_signal_line[BUFSIZE];
char signal_number_line[BUFSIZE];
char sender_line[BUFSIZE];
char receiver_line[BUFSIZE];

extern "C" OSBOOLEAN ndb_err_hnd(bool user_called, 
				 Uint32 error_code,
				 Uint32 extra)
{
  static  Error_message error_message;
  bool error_handled;
  Uint32  subcode;
  
  char* subcode_mnemonic;
  char* product_name;
  char* file_name; 
  
  /*The subcode (bit 16 - 30) is extracted from error_code */
  subcode = (error_code & 0x7fff0000) >> 16;
  
  if (user_called) {
    switch (subcode) {
    case 0x0050 :
      subcode_mnemonic= "OSE_PRH_PLS";
      product_name=     "Program Loader";
      file_name =       "prherr.h";
      break;
    case 0x0051 :
      subcode_mnemonic = "OSE_PRH_START_PRH";
      product_name=      "start_prh";
      file_name= "       start_prh.c";
      break;
    case 0x0052 :
      subcode_mnemonic= "OSE_PRH_ASF";
      product_name=     "Archive Server";
      file_name =       "prherr.h";
      break;
    case 0x0058 :
    case 0x4058 :
    case 0x3fff :
    case 0x8058 :
      subcode_mnemonic= "OSE_MMS_EBASE";
      product_name=     "MMS";
      file_name=        "mms_err.h";
      break;
      /*Link Handler G3***************************************/
    case 0x0060 :
    case 0x8060 :
      subcode_mnemonic= "OSE_GLH_EBASE";
      product_name=     "General Link Handler";
      file_name=        "glherr.h";
      break;
    case 0x0064 :
    case 0x8064 : 
      subcode_mnemonic= "OSE_GPL_EBASE";
      product_name=     "General Protocol Link Handler";
      file_name=        "gplerr.h";
      break;
    case 0x0066 :
    case 0x8066 :
      subcode_mnemonic= "OSE_UDPPDR_EBASE";
      product_name=     "UDP driver for GPL";
      file_name=        "udppdrerr.h";
      break;
    case 0x0067 :
    case 0x8067 :
      subcode_mnemonic= "OSE_SERPDR_EBASE";
      product_name=     "Serial driver for GPL";
      file_name=        "serpdrerr.h";
      break;
    case 0x0068 :
    case 0x8068 :
      subcode_mnemonic= "OSE_ETHPDR_EBASE";
      product_name=     "Ethernet driver for GPL";
      file_name=        "ethpdrerr.h";
      break;
      /*Link handler G4***************************************/
    case 0x0061 :
      subcode_mnemonic= "OSE_OTL_EBASE";
      product_name=     "OSE Transport Layer";
      file_name=        "otlerr.h";
      break;
    case 0x0062 :
      subcode_mnemonic= "OSE_LALUDP_EBASE";
      product_name=     "Link Adaption Layer for UDP";
      file_name=        "header file unknown";
      break;
      /*Internet Utilities************************************/
    case 0x0069 :
      subcode_mnemonic= "OSE_TFTPD";
      product_name=     "TFTP server";
      file_name=        "inetutilerr.h";
      break;
    case 0x006a :
      subcode_mnemonic= "OSE_TELUDPD";
      product_name=     "TELNET/UDP server";
      file_name=        "inetutilerr.h";
      break;
    case 0x006b :
      subcode_mnemonic= "OSE_FTPD";
      product_name=     "FTP server";
      file_name=        "inetutilerr.h";
      break;
    case 0x006c :
      subcode_mnemonic= "OSE_TELNETD";
      product_name=     "TELNET server";
      file_name=        "inetutilerr.h";
      break;
    case 0x006d :
      subcode_mnemonic= "OSE_SURFER";
      product_name=     "OSE System Surfer";
      file_name=        "inetutilerr.h";
      break;
    case 0x006e :
      subcode_mnemonic= "OSE_BOOTP";
      product_name=     "BOOTP client";
      file_name=        "inetutilerr.h";
      break;
    case 0x006f :
      switch((error_code & 0x0000f000)){
      case 0x00000000 :
        subcode_mnemonic= "OSE_RES";
        product_name=     "DNS resolver";
        file_name=        "inetutilerr.h";   
        break;
      case 0x00001000 :
        subcode_mnemonic= "OSE_DHCPC";
        product_name=     "DHCP client";
        file_name=        "inetutilerr.h";
        break;
      case 0x00002000 :
        subcode_mnemonic= "OSE_FTP";
        product_name=     "FTP client";
        file_name=        "inetutilerr.h";
        break;
      default :
        subcode_mnemonic= "Unknown error";
        product_name=     "unknown product";
        file_name =       "header file unknown";
        break;
      }
      break;
    case 0x00c2 :
      subcode_mnemonic= "OSE_DNS";
      product_name=     "DNS server";
      file_name=        "dns_err.h";
      break;
      /*INET**************************/
    case 0x0070 :
      subcode_mnemonic= "INET_ERRBASE";
      product_name=     "Internet Protocols (INET)";
      file_name=        "ineterr.h";
      break;
    case 0x0071 :
      subcode_mnemonic= "WEBS_ERRBASE";
      product_name=     "Web Server (WEBS)";
      file_name=        "webserr.h";
      break;
    case 0x0072 :
      subcode_mnemonic= "SNMP";
      product_name=     "SNMP";
      file_name=        "header file unknown";
      break;
    case 0x0073 :
      subcode_mnemonic= "STP_BRIDGE";
      product_name=     "STP bridge";
      file_name=        "header file unknown";
      break;
    case 0x0200 :
    case 0x0201 :
    case 0x0202 :
    case 0x0203 :
    case 0x0204 :
    case 0x0205 :
    case 0x0206 :
    case 0x0207 :
    case 0x0208 :
    case 0x0209 :
    case 0x020a :
    case 0x020b :
    case 0x020c :
    case 0x020d :
    case 0x020e :
    case 0x020f :
      subcode_mnemonic = "INETINIT_ERR_BASE";
      product_name =     "INET";
      file_name =        "startinet.c";
      break;
      /*Miscellanous******************************************/
    case 0x0082 :
      subcode_mnemonic= "OSE_HEAP_EBASE";
      product_name=     "Heap Manager";
      file_name=        "heap_err.h";
      break;
    case 0x0088 :
      subcode_mnemonic= "OSE_BSP";
      product_name=     "Board Support Package";
      file_name=        "bsperr.h";
      break;
    case 0x008a :
      subcode_mnemonic= "OSE_TOSV_EBASE";
      product_name=     "Time Out Server";
      file_name=        "tosverr.h";
      break;
    case 0x008b :
      subcode_mnemonic= "OSE_RTC_EBASE";
      product_name=     "Real Time Clock";
      file_name=        "rtcerr.h";
      break;
    case 0x008d :
    case 0x808d :
      subcode_mnemonic= "OSENS_ERR_BASE";
      product_name=     "Name Server";
      file_name=        "osens_err.h";
      break;
    case 0x008e :
      subcode_mnemonic= "PMD_ERR_BASE";
      product_name=     "Post Mortem Dump";
      file_name=        "pmderr.h";
      break;
      /*Embedded File System***********************************/
    case 0x0090 :
      subcode_mnemonic= "OSE_EFS_COMMON";
      product_name=     "EFS common";
      file_name=        "efs_err.h";
      break;
    case 0x0091 :
      subcode_mnemonic= "OSE_EFS_FLIB";
      product_name=     "EFS function library";
      file_name=        "efs_err.h";
      break;
    case 0x0092 :
      subcode_mnemonic= "OSE_EFS_SERDD";
      product_name=     "EFS serdd";
      file_name=        "efs_err.h";
      break;
    case 0x0093 :
      subcode_mnemonic= "OSE_EFS_SHELL";
      product_name=     "OSE shell";
      file_name=        "efs_err.h";
      break;
    case 0x0094 :
      subcode_mnemonic= "OSE_EFS_STARTEFS";
      product_name=     "EFS startefs.c";
      file_name=        "efs_err.h";
      break;
      /*Debugger related***************************************/
    case 0x00a0 :
      subcode_mnemonic= "DBGSERVER_ERR_BASE";
      product_name=     "Debug server for Illuminator";
      file_name=        "degservererr.h";
      break;
    case 0x00b2 :
      subcode_mnemonic= "OSE_MDM";
      product_name=     "Multi INDRT monitor";
      file_name=        "header file unknown";
      break;
      /*Miscellanous*******************************************/
    case 0x00c0 :
      subcode_mnemonic= "OSE_POTS_EBASE";
      product_name=     "POTS tutorial example";
      file_name=        "pots_err.h";
      break;
    case 0x00c1 :
      subcode_mnemonic= "OSE_PTH_ECODE_BASE";
      product_name=     "Pthreads";
      file_name=        "pthread_err.h";
      break;
    case 0x00c3 :
      subcode_mnemonic= "OSE_NTP_EBASE";
      product_name=     "OSE NTP/SNTP";
      file_name=        "ntp_err.h";
      break;
    case 0x00c4 :
      subcode_mnemonic= "TRILLIUM_BASE";
      product_name=     "Trillium OSE port";
      file_name=        "sk_ss.c";
      break;
    case 0x00c5 :
      subcode_mnemonic= "OSE_OSECPP_EBASE";
      product_name=     "C++ Support with libosecpp.a";
      file_name=        "cpp_err.h";
      break;
    case 0x00c6 :
      subcode_mnemonic= "OSE_RIP_ERR_BASE";
      product_name=     "OSE RIP";
      file_name=        "oserip.h";
      break;
      /*Unknown error_code*************************************/
    default :
      subcode_mnemonic= "Unknown error";
      product_name=     "unknown product";
      file_name =       "header file unknown";
      break;
    }
  } else {
    /* user_called = 0, i.e. reported by the kernel */
    subcode_mnemonic= "OSE_KRN";
    product_name=     "Kernel";
    file_name =       "ose_err.h";
  }

  snprintf (error_message.header1,
            BUFSIZE,
            "This is the OSE Example System Error handler\r\n");
  
  snprintf (error_message.err_hnd_file,
            BUFSIZE,
            "located in: " __FILE__ "\r\n");
  
  snprintf (error_message.header2,
            BUFSIZE,
            "An Error has been reported:\r\n");
  
  if (user_called == (OSBOOLEAN) 0 ) {
    BaseString::snprintf(error_message.user_called_line,
             BUFSIZE,
             "user_called:      0x%x (Error detected by the kernel)\r\n",
             user_called);
  }
  else {
    BaseString::snprintf(error_message.user_called_line,
             BUFSIZE,
             "user_called:      0x%x (Error detected by an application)\r\n",
             user_called);
  }
  
  snprintf (error_message.error_code_line,
            BUFSIZE,
            "error code:       0x%08x\r\n",
            error_code);
  
  snprintf (error_message.subcode_line,
            BUFSIZE,
            "   subcode:       %s (0x%08x)\r\n",
            subcode_mnemonic,
            ( subcode << 16));
  
  snprintf (error_message.product_line,
            BUFSIZE,
            "   product:       %s\r\n",
            product_name);
  
  snprintf (error_message.header_file_line,
            BUFSIZE,
            "   header file:   %s\r\n",
            file_name);
  
  snprintf (error_message.extra_line,
            BUFSIZE,
            "extra:            0x%08x\r\n",
            extra);
    
  if (error_code != OSE_ENO_KERN_SPACE || user_called){
    struct OS_pcb *pcb = get_pcb(current_process());
    const char *process_name = &pcb->strings[pcb->name];
      
    BaseString::snprintf(error_message.current_process_id_line,
             BUFSIZE,
             "Current Process:  0x%08x\r\n",
             current_process());
      
    BaseString::snprintf(error_message.current_process_name_line,
             BUFSIZE,
             "Process Name:     %s\r\n",
             process_name);
      
    BaseString::snprintf(error_message.file_line,
             BUFSIZE,
             "File:             %s\r\n",
             &pcb->strings[pcb->file]);
      
    BaseString::snprintf(error_message.line_line,
             BUFSIZE,
             "Line:             %d\r\n",
             pcb->line);
      
    free_buf((union SIGNAL **)&pcb);
  }

  if ( !(((error_code & OSE_EFATAL_MASK) != 0) && (user_called == 0))){
    /* If the error is reported by the kernel and the fatal flag is set,
     * dbgprintf can't be trusted */
    ndbout << error_message.header1;
    ndbout << error_message.err_hnd_file;
    ndbout << error_message.header2;
    ndbout << error_message.user_called_line;
    ndbout << error_message.error_code_line;
    ndbout << error_message.subcode_line;
    ndbout << error_message.product_line;
    ndbout << error_message.header_file_line;
    ndbout << error_message.extra_line;
    ndbout << error_message.current_process_id_line;
    ndbout << error_message.current_process_name_line;
    ndbout << error_message.file_line;
    ndbout << error_message.line_line;
    ndbout << endl;
  }
  
  if(user_called){
    switch (error_code) {
      /* Check for assertion failure (see oseassert.h and assert.c). */
    case (OSERRCODE) 0xffffffff:
      {
        if(extra != 0){
          char *expr = ((char **)extra)[0];
          char *file = ((char **)extra)[1];
          unsigned line = ((unsigned *)extra)[2];
          BaseString::snprintf(assert_line, BUFSIZE, "Assertion Failed: %s:%u: %s\r\n", file, line, expr);
          ndbout << assert_line;
        }
      }
      /* Check for unknown signal */
    case (OSERRCODE) 0xfffffffe:
      {
        union SIGNAL *sig = (union SIGNAL *)extra;
        SIGSELECT signo = *(SIGSELECT*)sig;
        PROCESS rcv_ = current_process();
        PROCESS snd_ = sender(&sig);
        struct OS_pcb *rcv = get_pcb(rcv_);
        const char *rcv_name = &rcv->strings[rcv->name];
        struct OS_pcb *snd = get_pcb(snd_);
        const char *snd_name = &snd->strings[snd->name];
        BaseString::snprintf(unknown_signal_line, BUFSIZE, 
                 "Unknown Signal Received\r\n");
        BaseString::snprintf(unknown_signal_line, BUFSIZE, 
                 "Signal Number: 0x%08lx\r\n", signo);
        BaseString::snprintf(unknown_signal_line, BUFSIZE, 
                 "Sending Process: 0x%08lx (%s))\r\n", snd_, snd_name);
        BaseString::snprintf(unknown_signal_line, BUFSIZE, 
                 "Receiving Process: 0x%08lx (%s))\r\n", rcv_, rcv_name);
        free_buf((union SIGNAL **)&rcv);
        free_buf((union SIGNAL **)&snd);          }
      ndbout << unknown_signal_line;
      ndbout << signal_number_line;
      ndbout << sender_line;
      ndbout << receiver_line;
    } /* switch */
  } /* if */

  /* Zero means the error has not been fixed by the error handler. */
  error_handled = 0; 
  return error_handled;  
}

#endif
