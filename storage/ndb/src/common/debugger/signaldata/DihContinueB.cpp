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
#if 0
  case DihContinueB::ZREAD_TAB_DESCRIPTION:
    fprintf(output, " Read Table description: %d\n", theData[1]);
    return true;
    break;
#endif
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
  case DihContinueB::ZSTART_TAKE_OVER:
    fprintf(output, " Start Take Over: TakeOverPtr: %d, startNode: %d, toNode: %d\n",
            theData[1], theData[2], theData[3]);
    return true;
    break;
  case DihContinueB::ZCHECK_START_TAKE_OVER:
    fprintf(output, " Check Start Take Over\n");
    return true;
    break;
  case DihContinueB::ZTO_START_COPY_FRAG:
    fprintf(output, " To Start Copy Frag: TakeOverPtr: %d\n", theData[1]);
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
  case DihContinueB::ZSEND_END_TO:
     fprintf(output, " Send End Take Over: TakeOverPtr: %d, startNode: %d, toNode: %d\n",
            theData[1], theData[2], theData[3]);
     return true;
     break;
  case DihContinueB::ZSEND_ADD_FRAG:
     fprintf(output, " Send Add Fragment: TakeOverPtr: %d, startNode: %d, toNode: %d\n",
            theData[1], theData[2], theData[3]);
     return true;
     break;
  case DihContinueB::ZSEND_CREATE_FRAG:
     fprintf(output, " Send Create Fragment: TakeOverPtr: %d, storedType: %d, start Gci: %d, startNode: %d, toNode: %d\n",
            theData[1], theData[2], theData[3], theData[4], theData[5]);
     return true;
     break;
  case DihContinueB::WAIT_DROP_TAB_WRITING_TO_FILE:
    fprintf(output, " Wait drop tab writing to file TableId: %d\n", theData[1]);
    return true;
  case DihContinueB::CHECK_WAIT_DROP_TAB_FAILED_LQH:
    fprintf(output, " Wait drop tab FailedNodeId: %d TableId: %d\n", 
	    theData[1], theData[2]);
    return true;
  default:
    fprintf(output, " Default system error lab...\n");
    break;
  }//switch
  return false;
}
