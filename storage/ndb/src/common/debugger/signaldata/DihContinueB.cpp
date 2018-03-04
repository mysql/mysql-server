/*
   Copyright (c) 2003, 2017, Oracle and/or its affiliates. All rights reserved.

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


#include <signaldata/DihContinueB.hpp>

bool
printCONTINUEB_DBDIH(FILE * output, const Uint32 * theData,
		     Uint32 len, Uint16 not_used){

  (void)not_used;

  switch (theData[0]) {
  case DihContinueB::ZPACK_TABLE_INTO_PAGES:
    fprintf(output, " Pack Table Into Pages: %d\n", theData[1]);
    return true;
    break;
  case DihContinueB::ZPACK_FRAG_INTO_PAGES:
    fprintf(output, " Pack Frag Into Pages: Table: %d Fragment: %d PageIndex: %d WordIndex: %d\n", 
	    theData[1], theData[2], theData[3], theData[4]);
    return true;
    break;
  case DihContinueB::ZREAD_PAGES_INTO_TABLE:
    fprintf(output, " Read Pages Into Table: %d\n", theData[1]);
    return true;
    break;
  case DihContinueB::ZREAD_PAGES_INTO_FRAG:
    fprintf(output, " Read Pages Into Frag: Table: %d Fragment: %d PageIndex: %d WordIndex: %d\n", 
	    theData[1], theData[2], theData[3], theData[4]);
    return true;
    break;
  case DihContinueB::ZCOPY_TABLE:
    fprintf(output, " Copy Table: %d\n", theData[1]);
    return true;
    break;
  case DihContinueB::ZCOPY_TABLE_NODE:
    fprintf(output, " Copy table node: TableId: %d NodeId: %d\n", 
            theData[1], theData[2]);
    fprintf(output, "PageIndex: %d WordIndex: %d NoOfWords: %d\n",
           theData[3], theData[4], theData[5]);
    return true;
    break;
  case DihContinueB::ZSTART_FRAGMENT:
    fprintf(output, " Start fragment: Table: %d Fragment: %d\n", 
	    theData[1], theData[2]);
    return true;
    break;
  case DihContinueB::ZCOMPLETE_RESTART:
    fprintf(output, "Complete Restart\n");
    return true;
    break;
  case DihContinueB::ZREAD_TABLE_FROM_PAGES:
    fprintf(output, " Read Table From Pages: Table: %d\n", theData[1]);    
    return true;
    break;
  case DihContinueB::ZSR_PHASE2_READ_TABLE:
    fprintf(output, " Phase 2 Read Table: Table: %d\n", theData[1]);    
    return true;
    break;
  case DihContinueB::ZCHECK_TC_COUNTER:
    fprintf(output, " Check Tc Counter from place %d\n", theData[1]);    
    return true;
    break;
  case DihContinueB::ZCALCULATE_KEEP_GCI:
    fprintf(output, " Calc Keep GCI: Table: %d Fragment: %d\n", 
	    theData[1], theData[2]);
    return true;
    break;
  case DihContinueB::ZSTORE_NEW_LCP_ID:
    fprintf(output, " Store New LCP Id\n");    
    return true;
    break;
  case DihContinueB::ZTABLE_UPDATE:
    fprintf(output, " Table Update: Table: %d\n", theData[1]);    
    return true;
    break;
  case DihContinueB::ZCHECK_LCP_COMPLETED:
    fprintf(output, " Check LCP Completed: TableId %d\n", theData[1]);    
    return true;
    break;
  case DihContinueB::ZINIT_LCP:
    fprintf(output, " Init LCP: Table: %d\n", theData[1]);
    return true;
    break;
  case DihContinueB::ZADD_TABLE_MASTER_PAGES:
    fprintf(output, " Add Table Master Pages: Table: %d\n", theData[1]);
    return true;
    break;
  case DihContinueB::ZDIH_ADD_TABLE_MASTER:
    fprintf(output, " Dih Add Table Master: Table: %d\n", theData[1]);
    return true;
    break;
  case DihContinueB::ZADD_TABLE_SLAVE_PAGES:
    fprintf(output, " Add Table Slave Pages: Table: %d\n", theData[1]);
    return true;
    break;
  case DihContinueB::ZDIH_ADD_TABLE_SLAVE:
    fprintf(output, " Add Table Slave: Table: %d\n", theData[1]);
    return true;
    break;
  case DihContinueB::ZSTART_GCP:
    fprintf(output, " Start GCP\n");
    return true;
    break;
  case DihContinueB::ZCOPY_GCI:
    fprintf(output, " Copy GCI\n");
    return true;
    break;
  case DihContinueB::ZEMPTY_VERIFY_QUEUE:
    fprintf(output, " Empty Verify Queue\n");
    return true;
    break;
  case DihContinueB::ZCHECK_GCP_STOP:
    fprintf(output, " Check GCP Stop\n");
    if (len == 6){
      fprintf(output, "coldGcpStatus   = %d\n", theData[1]);
      fprintf(output, "cgcpStatus      = %d\n", theData[2]);
      fprintf(output, "coldGcpId       = %d\n", theData[3]);
      fprintf(output, "cnewgcp         = %d\n", theData[4]);
      fprintf(output, "cgcpSameCounter = %d\n", theData[5]);
    }
    return true;
    break;
  case DihContinueB::ZREMOVE_NODE_FROM_TABLE:
    fprintf(output, " Remove Node From Table: Node: %d Table: %d\n", 
	    theData[1], theData[2]);
    return true;
    break;
  case DihContinueB::ZCOPY_NODE:
    fprintf(output, " Copy Node: Table: %d\n", theData[1]);
    return true;
    break;
  case DihContinueB::ZTO_START_COPY_FRAG:
    fprintf(output, " To Start Copy Frag: TakeOverPtr: %d\n", theData[1]);
    return true;
    break;
  case DihContinueB::ZTO_START_LOGGING:
    fprintf(output, " To Start REDO logging: TakeOverPtr: %d\n", theData[1]);
    return true;
    break;
  case DihContinueB::ZTO_START_FRAGMENTS:
    fprintf(output, " To Start Fragments: TakeOverPtr: %d\n", theData[1]);
    return true;
    break;
  case DihContinueB::ZINVALIDATE_NODE_LCP:
    fprintf(output, " Invalide LCP: NodeId: %d TableId %d\n",
            theData[1], theData[2]);
    return true;
    break;
  case DihContinueB::ZINITIALISE_RECORDS:
    fprintf(output, " Initialise Records: tdata0: %d\n", theData[1]);
    return true;
    break;
  case DihContinueB::ZSTART_PERMREQ_AGAIN:
    fprintf(output, " START_PERMREQ again for node: %d\n", theData[1]);
    return true;
    break;
  case DihContinueB::SwitchReplica:
    fprintf(output, " NodeId = %d TableId = %d FragNo = %d\n",
	    theData[1], theData[2], theData[3]);
    return true;
    break;
  case DihContinueB::ZSEND_START_TO:
     fprintf(output, " Send Start Take Over: TakeOverPtr: %d, startNode: %d, toNode: %d\n",
            theData[1], theData[2], theData[3]);
     return true;
     break;
  case DihContinueB::ZSEND_UPDATE_TO:
     fprintf(output, " Send Update Take Over: TakeOverPtr: %d, startNode: %d, toNode: %d\n",
            theData[1], theData[2], theData[3]);
     return true;
     break;
  case DihContinueB::ZSEND_ADD_FRAG:
     fprintf(output, " Send Add Fragment: TakeOverPtr: %d, startNode: %d, toNode: %d\n",
            theData[1], theData[2], theData[3]);
     return true;
     break;
  case DihContinueB::WAIT_DROP_TAB_WRITING_TO_FILE:
    fprintf(output, " Wait drop tab writing to file TableId: %d\n", theData[1]);
    return true;
  case DihContinueB::ZLCP_TRY_LOCK:
    fprintf(output, " Lcp trylock: attempt %u\n",
            theData[1]);
    break;
  case DihContinueB::ZWAIT_OLD_SCAN:
    fprintf(output, " Wait old scan\n");
    break;
  case DihContinueB::ZDEQUEUE_LCP_REP:
    fprintf(output, " Get Dequeue LCP report\n");
    break;
  case DihContinueB::ZGET_TABINFO:
    fprintf(output, " Get Tabinfo\n");
    break;
  case DihContinueB::ZGET_TABINFO_SEND:
    fprintf(output, " Get Tabinfo Send\n");
    break;
  default:
    fprintf(output, " Default system error lab...\n");
    break;
  }//switch
  return false;
}
