/*
   Copyright (C) 2003-2006 MySQL AB
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

#include <ndb_global.h>

#include "sisci_types.h"
#include "sisci_api.h"
#include "sisci_error.h"
//#include "sisci_demolib.h"
#include <NdbTick.h>
#include <NdbSleep.h>
#define NO_CALLBACK         NULL
#define NO_FLAGS            0
#define DATA_TRANSFER_READY 8

sci_error_t             error;
sci_desc_t              sdOne;
sci_desc_t              sdTwo;
sci_local_segment_t     localSegmentOne;
sci_local_segment_t     localSegmentTwo;
sci_remote_segment_t    remoteSegmentOne;
sci_remote_segment_t    remoteSegmentTwo;
sci_map_t               localMapOne;
sci_map_t               localMapTwo;
sci_map_t               remoteMapOne;
sci_map_t               remoteMapTwo;
unsigned int            localAdapterNo = 0;
unsigned int            standbyAdapterNo = 1;
unsigned int            localNodeId1;
unsigned int            localNodeId2;
unsigned int            remoteNodeId1 = 0;
unsigned int            remoteNodeId2 = 0;
unsigned int            localSegmentId;
unsigned int            remoteSegmentId1;
unsigned int            remoteSegmentId2;
unsigned int            segmentSize = 8192;
unsigned int            offset = 0;
unsigned int            client = 0;
unsigned int            server = 0;
unsigned int            *localbufferPtr;
static int data;
static int interruptConnected=0;

/*********************************************************************************/
/*                                U S A G E                                      */
/*                                                                               */
/*********************************************************************************/

void Usage()
{
    printf("Usage of shmem\n");
    printf("shmem -rn <remote node-id> -client/server [ -adapterno <adapter no> -size <segment size> ] \n\n");
    printf(" -rn               : Remote node-id\n");
    printf(" -client           : The local node is client\n");
    printf(" -server           : The local node is server\n");
    printf(" -adapterno        : Local adapter number (default %d)\n", localAdapterNo);
    printf(" -size             : Segment block size   (default %d)\n", segmentSize);
    printf(" -help             : This helpscreen\n");

    printf("\n");
}


/*********************************************************************************/
/*                   P R I N T   P A R A M E T E R S                             */
/*                                                                               */
/*********************************************************************************/
void PrintParameters(void)
{

    printf("Test parameters for %s \n",(client) ?  "client" : "server" );
    printf("----------------------------\n\n");
    printf("Local node-id1      : %d\n",localNodeId1);
    printf("Local node-id2      : %d\n",localNodeId2);
    //    printf("Remote node-id     : %d\n",remoteNodeId);
    printf("Local adapter no.  : %d\n",localAdapterNo);
    printf("Segment size       : %d\n",segmentSize);
    printf("----------------------------\n\n");

}


/*********************************************************************************/
/*                F I L L   S E G M E N T   W I T H   D A T A                    */
/*                                                                               */
/*********************************************************************************/

sci_error_t GetLocalNodeId(Uint32 localAdapterNo, Uint32* localNodeId)
{
  sci_query_adapter_t queryAdapter;
  sci_error_t  error;
  unsigned int _localNodeId;
  
  queryAdapter.subcommand = SCI_Q_ADAPTER_NODEID;
  queryAdapter.localAdapterNo = localAdapterNo;
  queryAdapter.data = &_localNodeId;

  SCIQuery(SCI_Q_ADAPTER,&queryAdapter,NO_FLAGS,&error);
  
  *localNodeId=_localNodeId;
  
  return error;
}






sci_error_t SendInterrupt(sci_desc_t   sd,
	      Uint32 localAdapterNo, 
	      Uint32 localSciNodeId, 
	      Uint32 remoteSciNodeId,
	      Uint32 interruptNo){
  
  sci_error_t             error;
  sci_remote_interrupt_t  remoteInterrupt;
  Uint32 timeOut = SCI_INFINITE_TIMEOUT;

    // Now connect to the other sides interrupt flag 
    do {
      SCIConnectInterrupt(sd, &remoteInterrupt, remoteSciNodeId, localAdapterNo,
			  interruptNo, timeOut, NO_FLAGS, &error);
    } while (error != SCI_ERR_OK);
    
    if (error != SCI_ERR_OK) {
      fprintf(stderr, "SCIConnectInterrupt failed - Error code 0x%x\n", error);
      return error;
    } 

  // Trigger interrupt
  printf("\nNode %u sent interrupt (0x%x) to node %d\n",localSciNodeId, interruptNo, remoteSciNodeId);
  SCITriggerInterrupt(remoteInterrupt, NO_FLAGS, &error);
  if (error != SCI_ERR_OK) {
    fprintf(stderr, "SCITriggerInterrupt failed - Error code 0x%x\n", error);
    return error;
  } 
   

    // Disconnect and remove interrupts 
    SCIDisconnectInterrupt(remoteInterrupt, NO_FLAGS, &error);
    if (error != SCI_ERR_OK) {
      fprintf(stderr, "SCIDisconnectInterrupt failed - Error code 0x%x\n", error);
      return error; 
    }    

  return error;
}


sci_error_t ReceiveInterrupt(sci_desc_t sd,
		 Uint32 localAdapterNo,
		 Uint32 localSciNodeId, 
		 Uint32 interruptNo,
		 Uint32 timeout) {
  
  sci_error_t             error;
  sci_local_interrupt_t   localInterrupt;
  Uint32 timeOut = SCI_INFINITE_TIMEOUT;

  // Create an interrupt 
  SCICreateInterrupt(sd, &localInterrupt, localAdapterNo,
		       &interruptNo, 0, NULL, SCI_FLAG_FIXED_INTNO, &error);
  if (error != SCI_ERR_OK) {
    fprintf(stderr, "SCICreateInterrupt failed - Error code 0x%x\n", error);
    return error; 
  }  
  
  
  // Wait for an interrupt 
  SCIWaitForInterrupt(localInterrupt, timeOut, NO_FLAGS, &error);

  printf("\nNode %u received interrupt (0x%x)\n", localSciNodeId, interruptNo); 
 
  // Remove interrupt 
  
  SCIRemoveInterrupt(localInterrupt, NO_FLAGS, &error);
  if (error != SCI_ERR_OK) {
    fprintf(stderr, "SCIRemoveInterrupt failed - Error code 0x%x\n", error);
    return error; 
  }
  return error;
}


sci_error_t FillSegmentWithData(unsigned int segmentSize, int reverse)
{
    unsigned int i;
    unsigned int nostores;

    
    nostores = (segmentSize) / sizeof(unsigned int);
    
    /* Allocate buffer */
    
    localbufferPtr = (unsigned int*)malloc( segmentSize );
    if ( localbufferPtr == NULL ) {
        /*
         * Unable to create local buffer - Insufficient memory available
         */
        return SCI_ERR_NOSPC;
    }
    if(reverse) {
      /* Fill in the data into a local buffer */
      printf("Filling forward order \n");
      for (i=0;i<nostores;i++) {
	localbufferPtr[i] = i;
      }
    }
    else {
      int temp=nostores;
      printf("Filling reverse order \n");
      for (i=0;i<nostores;i++) {
	localbufferPtr[i] = temp-- ;

      }
     
    }
    
    return SCI_ERR_OK;
}




/*********************************************************************************/
/*                    P R I N T   C L I E N T   D A T A                          */
/*                                                                               */
/*********************************************************************************/

void PrintClientData(void)
{
    unsigned int i;

    printf("\nClient data: ");
    /* Print the first 20 entries in the segment */
    for (i=0;i<20;i++) {
        printf("%d ",localbufferPtr[i]);
    }

    printf("\n");
}


/*********************************************************************************/
/*                    P R I N T   S E R V E R   D A T A                          */
/*                                                                               */
/*********************************************************************************/

void PrintServerData(volatile unsigned int *localMapAddr)
{
 
    unsigned int *buffer; 
    int i;

    //    printf("\nServer data: ");
    buffer = (unsigned int *)localMapAddr;

    /* Print the first 20 entries in the segment */
    for (i=0; i< 20; i++) {
        
        printf("%d ",buffer[i]);
    }
    printf("\n");

}



/*********************************************************************************/
/*                       T R A N S F E R   D A T A                               */
/*                                                                               */
/*********************************************************************************/

unsigned int TransferData(sci_map_t             remoteMap,
                          volatile unsigned int *remoteSegmentAddr1, 
			  volatile unsigned int *remoteSegmentAddr2, 
                          unsigned int          segmentSize)

{
  
  volatile unsigned int   *remoteBuffer1;
  volatile unsigned int   *remoteBuffer;
  volatile unsigned int   *remoteBuffer2;
  static int times = 0;
    sci_sequence_t          sequence;
    sci_error_t             error;
    unsigned int            nostores;
    unsigned int            j;
    sci_sequence_status_t   sequenceStatus;


    remoteBuffer1 = (volatile unsigned int *)remoteSegmentAddr1;
    remoteBuffer2 = (volatile unsigned int *)remoteSegmentAddr2;
    remoteBuffer=remoteBuffer1;

	/* 4-byte test only */ 
	nostores = (segmentSize) / sizeof(unsigned int);

    /* Create a sequence for data error checking */    
    SCICreateMapSequence(remoteMapOne,&sequence,NO_FLAGS,&error);
    if (error != SCI_ERR_OK) {
        fprintf(stderr,"SCICreateMapSequence failed - Error code 0x%x\n",error);
        return error;
    }



    /* Fill in the data into a local buffer */
    error = SendInterrupt(sdOne,localAdapterNo,localNodeId1,remoteNodeId1, DATA_TRANSFER_READY);

    error = FillSegmentWithData(segmentSize, 0);

 tryagain:
    PrintServerData(localbufferPtr);
    fprintf(stderr,"After recover \n");
    while(1){


      //data=0;
    
      if (error != SCI_ERR_OK) {
        /*
         * Unable to create local buffer - Insufficient memory available
         */
        printf( "Unable to create local buffer - Insufficient memory available\n" );
	
        return error;
      }

      do {
        /* Start data error checking */
        sequenceStatus = SCIStartSequence(sequence,NO_FLAGS,&error);
      } while (sequenceStatus != SCI_SEQ_OK) ;
      
            
    /* Transfer data to remote node */
      for (j=0;j<nostores;j++) {
	remoteBuffer[j] = localbufferPtr[j];
      }
      
      /* Check for error after data transfer */
      sequenceStatus = SCICheckSequence(sequence,NO_FLAGS,&error);
      if (sequenceStatus != SCI_SEQ_OK) {
        fprintf(stderr,"Data transfer failed\n");
	if(times==0) {
	  error = FillSegmentWithData(segmentSize, 1);
	  
	  SCICreateMapSequence(remoteMapTwo,&sequence,NO_FLAGS,&error);
	  if (error != SCI_ERR_OK) {
	    fprintf(stderr,"SCICreateMapSequence failed - Error code 0x%x\n",error);
	    return error;
	    return SCI_ERR_TRANSFER_FAILED;
	  }
	}
	else 
	  {
	    error = FillSegmentWithData(segmentSize, 0);
	    /* Create a sequence for data error checking */    
	    SCICreateMapSequence(remoteMapOne,&sequence,NO_FLAGS,&error);
	    if (error != SCI_ERR_OK) {
	      fprintf(stderr,"SCICreateMapSequence failed - Error code 0x%x\n",error);
	      return error;
	      return SCI_ERR_TRANSFER_FAILED;
	}
	    
	  }
	fprintf(stderr,"Recovery \n");
	if(times==0)
	  remoteBuffer=remoteBuffer2;
	else
	  remoteBuffer=remoteBuffer1;
	times++;
	printf("remotebuffer %p   times %d\n", remoteBuffer, times);
	goto tryagain;

      }    
      int timeout=0;
      //      error = SendInterrupt(sdOne,localAdapterNo,localNodeId1,remoteNodeId1, DATA_TRANSFER_READY);
      //      NdbSleep_MilliSleep(100);
      //error = ReceiveInterrupt(sdOne,localAdapterNo,localNodeId1,DATA_TRANSFER_READY, timeout);

    }
    /* Remove the Sequence */
    SCIRemoveSequence(sequence,NO_FLAGS, &error);
    if (error != SCI_ERR_OK) {
      fprintf(stderr,"SCIRemoveSequence failed - Error code 0x%x\n",error);
      return error;
    }
    
    return SCI_ERR_OK;
}


/*********************************************************************************/
/*                    S H M E M   C L I E N T   N O D E                          */
/*                                                                               */
/*********************************************************************************/

unsigned int ShmemClientNode(void)
{

    volatile unsigned int *remoteMapAddr1;
    volatile unsigned int *remoteMapAddr2;
    printf("here?\n");

    
    /* Create a segmentId */
    remoteSegmentId1 = 1;//(remoteNodeId1 << 16) | localNodeId1;

    /* Connect to remote segment */

    printf("Connect to remote segment ....  \n");
    printf("segid = %d  node %d \n",remoteSegmentId1, remoteNodeId1 );

     do { 
      SCIConnectSegment(sdOne,
			&remoteSegmentOne,
			remoteNodeId1,
			remoteSegmentId1,
			localAdapterNo,
			NO_CALLBACK,
			NULL,
			SCI_INFINITE_TIMEOUT,
			NO_FLAGS,
			&error);
      
        } while (error != SCI_ERR_OK);


    printf("connected\n");
    
    // remoteSegmentId2 = (remoteNodeId2 << 16) | localNodeId2;
    //  printf("segid = %d\n",remoteSegmentId2 );
    printf("segid = %d  node %d \n",remoteSegmentId1, remoteNodeId1 );
    do { 
      SCIConnectSegment(sdTwo,
			&remoteSegmentTwo,
			remoteNodeId2,
			remoteSegmentId1,
			standbyAdapterNo,
			NO_CALLBACK,
			NULL,
			SCI_INFINITE_TIMEOUT,
			NO_FLAGS,
			&error);
      
    } while (error != SCI_ERR_OK);
    
   

    printf("connected 3\n");
    printf("Remote segment (id=0x%x) is connected.\n", remoteSegmentId2);


	/* Map remote segment to user space */
    remoteMapAddr1 = (unsigned int*)SCIMapRemoteSegment(remoteSegmentOne,&remoteMapOne,offset,segmentSize,NULL,NO_FLAGS,&error);
    if (error == SCI_ERR_OK) {
        printf("Remote segment (id=0x%x) is mapped to user space @ 0x%x. \n", remoteSegmentId1, remoteMapAddr1);         
    } else {
        fprintf(stderr,"SCIMapRemoteSegment failed - Error code 0x%x\n",error);
        return 0;
    } 

    remoteMapAddr2 = (unsigned int *)SCIMapRemoteSegment(remoteSegmentTwo,&remoteMapTwo,offset,segmentSize,NULL,NO_FLAGS,&error);
    if (error == SCI_ERR_OK) {
      printf("Remote segment (id=0x%x) is mapped to user space @ 0x%x. \n", remoteSegmentId2, remoteMapAddr2);         
    } else {
        fprintf(stderr,"SCIMapRemoteSegment failed - Error code 0x%x\n",error);
        return 0;
    } 

        
    /* Start data transfer and error checking */
    error = (sci_error_t)TransferData(remoteMapOne,remoteMapAddr1, remoteMapAddr2,segmentSize);
    if (error == SCI_ERR_OK) {
        printf("Data transfer done!\n\n");
    } else {
        fprintf(stderr,"Data transfer failed - Error code 0x%x\n\n",error);
        return 0;
    }

    /* Send an interrupt to remote node telling that the data transfer is ready */
    error = SendInterrupt(sdOne,localAdapterNo,localNodeId1,remoteNodeId1, DATA_TRANSFER_READY);
    if (error == SCI_ERR_OK) {
        printf("\nInterrupt message sent to remote node\n");
    } else {
        printf("\nInterrupt synchronization failed\n");
        return 0;
    }
    
    PrintClientData();

    /* Unmap remote segment */
    SCIUnmapSegment(remoteMapOne,NO_FLAGS,&error);
    if (error == SCI_ERR_OK) {
       	printf("The remote segment is unmapped\n"); 
    } else {
        fprintf(stderr,"SCIUnmapSegment failed - Error code 0x%x\n",error);
        return 0;
    }
    
    SCIUnmapSegment(remoteMapTwo,NO_FLAGS,&error);
    if (error == SCI_ERR_OK) {
      printf("The remote segment is unmapped\n"); 
    } else {
      fprintf(stderr,"SCIUnmapSegment failed - Error code 0x%x\n",error);
      return 0;
    }
    /* Disconnect segment */
    SCIDisconnectSegment(remoteSegmentOne,NO_FLAGS,&error);
    if (error == SCI_ERR_OK) {
       	printf("The segment is disconnected\n"); 
    } else {
        fprintf(stderr,"SCIDisconnectSegment failed - Error code 0x%x\n",error);
        return 0;
    } 
    
    SCIDisconnectSegment(remoteSegmentTwo,NO_FLAGS,&error);
    if (error == SCI_ERR_OK) {
       	printf("The segment is disconnected\n"); 
    } else {
        fprintf(stderr,"SCIDisconnectSegment failed - Error code 0x%x\n",error);
        return 0;
    } 


    return 1;
}


/*********************************************************************************/
/*                    S H M E M   S E R V E R   N O D E                          */
/*                                                                               */
/*********************************************************************************/

unsigned int ShmemServerNode(void)
{

  unsigned int *localMapAddr;
    
    /* Create a segmentId */
  localSegmentId  =1;// (localNodeId1 << 16)  | remoteNodeId1;

    /* Create local segment */    
        SCICreateSegment(sdOne,&localSegmentOne,localSegmentId, segmentSize, NO_CALLBACK, NULL, NO_FLAGS,&error);
    if (error == SCI_ERR_OK) {
        printf("Local segment (id=%d, size=%d) is created. \n", localSegmentId, segmentSize);  
    } else {
        fprintf(stderr,"SCICreateSegment failed - Error code 0x%x\n",error);
        return 0;
    }

    //localSegmentId  = (localNodeId2 << 16)  | remoteNodeId2;
    /*
    SCICreateSegment(sdTwo,&localSegmentTwo,localSegmentId+1, segmentSize, NO_CALLBACK, NULL, NO_FLAGS,&error);
    if (error == SCI_ERR_OK) {
      printf("Local segment (id=%d, size=%d) is created (2). \n", localSegmentId, segmentSize);  
    } else {
      fprintf(stderr,"SCICreateSegment failed - Error code 0x%x\n",error);
      return 0;
    }
    
    printf("segment one %p segment 2 %p\n", localSegmentOne, localSegmentTwo);
    */
    /* Prepare the segment */
    SCIPrepareSegment(localSegmentOne,localAdapterNo,NO_FLAGS,&error);
    if (error == SCI_ERR_OK) {
        printf("Local segment (id=%d, size=%d) is prepared. \n", localSegmentId, segmentSize);  
    } else {
        fprintf(stderr,"SCIPrepareSegment failed - Error code 0x%x\n",error);
        return 0;
    }
    
    
    /* Prepare the segment */
   
    SCIPrepareSegment(localSegmentOne,standbyAdapterNo,NO_FLAGS,&error);
    if (error == SCI_ERR_OK) {
        printf("Local segment (id=%d, size=%d) is created. \n", localSegmentId, segmentSize);  
    } else {
        fprintf(stderr,"SCIPrepareSegment failed - Error code 0x%x\n",error);
        return 0;
    }
   

    /* Map local segment to user space */
    localMapAddr = (unsigned int *)SCIMapLocalSegment(localSegmentOne,&localMapOne, offset,segmentSize, NULL,NO_FLAGS,&error);
    if (error == SCI_ERR_OK) {
       	printf("Local segment (id=0x%x) is mapped to user space @ 0x%x.\n", localSegmentId, localMapAddr); 
    } else {
        fprintf(stderr,"SCIMapLocalSegment failed - Error code 0x%x\n",error);
        return 0;
    } 
    

    /* Map local segment to user space */
    /*
    localMapAddr = (unsigned int *)SCIMapLocalSegment(localSegmentTwo,&localMapTwo, offset,segmentSize, NULL,NO_FLAGS,&error);
    if (error == SCI_ERR_OK) {
      printf("Local segment (id=0x%x) is mapped to user space @ 0x%x.\n", localSegmentId, localMapAddr); 
      printf("Local segment (id=%d) is mapped to user space.\n", localSegmentId); 
    } else {
      fprintf(stderr,"SCIMapLocalSegment failed - Error code 0x%x\n",error);
      return 0;
    } 
    */

    /* Set the segment available */
    SCISetSegmentAvailable(localSegmentOne, localAdapterNo, NO_FLAGS, &error);
    if (error == SCI_ERR_OK) {
       	printf("Local segment (id=0x%x) is available for remote connections. \n", localSegmentId); 
    } else {
        fprintf(stderr,"SCISetSegmentAvailable failed - Error code 0x%x\n",error);
        return 0;
    } 

    
    SCISetSegmentAvailable(localSegmentOne, standbyAdapterNo, NO_FLAGS, &error);
    if (error == SCI_ERR_OK) {
       	printf("Local segment (id=0x%x) is available for remote connections. \n", localSegmentId); 
    } else {
        fprintf(stderr,"SCISetSegmentAvailable failed - Error code 0x%x\n",error);
        return 0;
    } 
    int timeout=0;
    error = ReceiveInterrupt(sdOne,localAdapterNo,localNodeId1,DATA_TRANSFER_READY, timeout);    

    if (error == SCI_ERR_OK) {
      printf("\nThe data transfer is ready\n");
    } else {
      printf("\nInterrupt synchronization failed\n");
      return 0;
    }
    

 again:

    //    printf("Wait for the shared memory data transfer .....");
    /* Wait for interrupt signal telling that block transfer is ready */

    //printf("\nData transfer done!\n");
    //PrintClientData()
    PrintServerData(localMapAddr);
    /*Uint32 micros;
    Uint32 micros2;
    NDB_TICKS secs;
    NdbTick_CurrentMicrosecond(&secs, &micros);
    error = SendInterrupt(sdOne,localAdapterNo,localNodeId1,remoteNodeId1, DATA_TRANSFER_READY);
    NdbTick_CurrentMicrosecond(&secs, &micros2);
    printf("TIME ELAPSED %d \n", micros2-micros);
//    NdbSleep_MilliSleep(100);
    */
    goto again;

    /* Unmap local segment */
    SCIUnmapSegment(localMapTwo,NO_FLAGS,&error);
    if (error == SCI_ERR_OK) {
      printf("The local segment is unmapped\n"); 
    } else {
      fprintf(stderr,"SCIUnmapSegment failed - Error code 0x%x\n",error);
      return 0;
    }
    
    /* Unmap local segment */
    SCIUnmapSegment(localMapOne,NO_FLAGS,&error);
    if (error == SCI_ERR_OK) {
      printf("The local segment is unmapped\n"); 
    } else {
      fprintf(stderr,"SCIUnmapSegment failed - Error code 0x%x\n",error);
        return 0;
    }
    /* Remove local segment */
    SCIRemoveSegment(localSegmentOne,NO_FLAGS,&error);
    if (error == SCI_ERR_OK) {
       	printf("The local segment is removed\n"); 
    } else {
        fprintf(stderr,"SCIRemoveSegment failed - Error code 0x%x\n",error);
        return 0;
    }  

    /* Remove local segment */
    SCIRemoveSegment(localSegmentTwo,NO_FLAGS,&error);
    if (error == SCI_ERR_OK) {
       	printf("The local segment is removed\n"); 
    } else {
        fprintf(stderr,"SCIRemoveSegment failed - Error code 0x%x\n",error);
        return 0;
    } 




    return 1;
}



/*********************************************************************************/
/*                                M A I N                                        */
/*                                                                               */
/*********************************************************************************/

int main(int argc,char *argv[])
{

    int counter; 

    printf("\n %s compiled %s : %s\n\n",argv[0],__DATE__,__TIME__);
    
    if (argc<3) {
        Usage();
        exit(-1);
    }


    /* Get the parameters */
    for (counter=1; counter<argc; counter++) {

        if (!strcmp("-rn",argv[counter])) {
	  //            remoteNodeId = strtol(argv[counter+1],(char **) NULL,10);
            continue;
        } 

        if (!strcmp("-size",argv[counter])) {
            segmentSize = strtol(argv[counter+1],(char **) NULL,10);
            continue;
        } 

        if (!strcmp("-adapterno",argv[counter])) {
            localAdapterNo = strtol(argv[counter+1],(char **) NULL,10);
            continue;
        }

        if (!strcmp("-client",argv[counter])) {
            client = 1;
            continue;
        }

        if (!strcmp("-server",argv[counter])) {
            server = 1;
            continue;
        }

        if (!strcmp("-help",argv[counter])) {
            Usage();
            exit(0);
        }
    }

    //    if (remoteNodeId == 0) {
    //   fprintf(stderr,"Remote node-id is not specified. Use -rn <remote node-id>\n");
    //  exit(-1);
    //}

    if (server == 0 && client == 0) {
        fprintf(stderr,"You must specify a client node or a server node\n");
        exit(-1);
    }

    if (server == 1 && client == 1) {
        fprintf(stderr,"Both server node and client node is selected.\n"); 
        fprintf(stderr,"You must specify either a client or a server node\n");
        exit(-1);
    }


    /* Initialize the SISCI library */
    SCIInitialize(NO_FLAGS, &error);
    if (error != SCI_ERR_OK) {
        fprintf(stderr,"SCIInitialize failed - Error code: 0x%x\n",error);
        exit(error);
    }


    /* Open a file descriptor */
    SCIOpen(&sdOne,NO_FLAGS,&error);
    if (error != SCI_ERR_OK) {
        if (error == SCI_ERR_INCONSISTENT_VERSIONS) {
            fprintf(stderr,"Version mismatch between SISCI user library and SISCI driver\n");
        }
        fprintf(stderr,"SCIOpen failed - Error code 0x%x\n",error);
        exit(error); 
    }

    /* Open a file descriptor */
    SCIOpen(&sdTwo,NO_FLAGS,&error);
    if (error != SCI_ERR_OK) {
        if (error == SCI_ERR_INCONSISTENT_VERSIONS) {
            fprintf(stderr,"Version mismatch between SISCI user library and SISCI driver\n");
        }
        fprintf(stderr,"SCIOpen failed - Error code 0x%x\n",error);
        exit(error); 
    }


    /* Get local node-id */
    error = GetLocalNodeId(localAdapterNo, &localNodeId1);
    error = GetLocalNodeId(standbyAdapterNo, &localNodeId2);
    if (error != SCI_ERR_OK) {
      fprintf(stderr,"Could not find the local adapter %d\n", localAdapterNo);
      SCIClose(sdOne,NO_FLAGS,&error);
      SCIClose(sdTwo,NO_FLAGS,&error);
      exit(-1);
    }
    

    /* Print parameters */
    PrintParameters();

    if (client) {
      remoteNodeId1=324;
      remoteNodeId2=328;
        ShmemClientNode();
    } else {
         remoteNodeId1=452;
	 remoteNodeId2=456;
        ShmemServerNode();
    }

    /* Close the file descriptor */
    SCIClose(sdOne,NO_FLAGS,&error);
    SCIClose(sdTwo,NO_FLAGS,&error);
    if (error != SCI_ERR_OK) {
        fprintf(stderr,"SCIClose failed - Error code: 0x%x\n",error);
    }


    /* Free allocated resources */
    SCITerminate();

    return SCI_ERR_OK;
}

















