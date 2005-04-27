BEGIN{
  PRINT=0;
  SIGNAL_ARRAY[0]="";
  BLOCK_ID=0;
  SIGNAL_ID=-22;
}
{
  SIGNAL_ARRAY[SIGNAL_ID]=SIGNAL_ID;
}

/^---- Send ----- Signal ----------------/ {
  DIRECTION="S";
  SENDER="";
  SENDPROCESS="";
  RECEIVER="";
  RECPROCESS="";
  SIGNAL="";
  RECSIGID="?";
  SIGID="?";
  DELAY="N/A";
}

/^---- Send delay Signal/ {
  DIRECTION="SD";
  SENDER="";
  SENDPROCESS="";
  RECEIVER="";
  RECPROCESS="";
  SIGNAL="";
  RECSIGID="?";
  SIGID="?";
  DELAY=$5;
  
  LEN=length(DELAY);
  DELAY=substr(DELAY,2,LEN);
}

/^---- Received - Signal ----------------/ {
  DIRECTION="R";
  SENDER="";
  SENDPROCESS="";
  RECEIVER="";
  RECPROCESS="";
  SIGNAL="";
  RECSIGID="?";
  SIGID="?";
  DELAY="N/A";
}

/r.bn:/{

  RECEIVER=$3;
  RECPROCESS=$5;
  
  if(DIRECTION == "R"){
    SIGNAL=$10;
    RECSIGID=$7;
  }
  else
    SIGNAL=$8;
}

/s.bn:/{

  SENDER=$3;
  SIGID=$7;

  if(SIGID == SIGNAL_ARRAY[SIGID]){
    PRINT=1;
    if(DIRECTION == "R"){
      SIGNAL_ARRAY[RECSIGID]=RECSIGID;
    }; 
  }

  SENDPROCESS=$5;
  
  LEN=length(RECEIVER);
  RECEIVER=substr(RECEIVER,2,LEN-3);

  if(BLOCK_ID == "ALL" || RECEIVER==BLOCK_ID){PRINT=1; }
  
  LEN=length(SENDER);
  SENDER=substr(SENDER,2,LEN-3);
  if(BLOCK_ID == "ALL" || SENDER == BLOCK_ID){ PRINT=1;}  

  LEN=length(SIGNAL);
  SIGNAL=substr(SIGNAL,2,LEN-2);
  
  LEN=length(SENDPROCESS);
  SENDPROCESS=substr(SENDPROCESS,1,LEN-1);
  
  LEN=length(RECPROCESS);
  RECPROCESS=substr(RECPROCESS,1,LEN-1);
  
  if( PRINT == 1){
    print DIRECTION" "SENDPROCESS" "SENDER" "RECPROCESS" "RECEIVER" "SIGNAL" "SIGID" "RECSIGID" "DELAY;
  }
  
  PRINT=0;
}


