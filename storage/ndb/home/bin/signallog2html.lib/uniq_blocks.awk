BEGIN{
  NAMES[""]="";
  ORDER[0]="";
  NUM=0;
}

{
  if(NAMES[$2$3]!=$2$3){
    NAMES[$2$3]=$2$3;
    ORDER[NUM]=$2$3;
    NUM++;
  }
 
  if(NAMES[$4$5]!=$4$5){
    NAMES[$4$5]=$4$5;
    ORDER[NUM]=$4$5;
    NUM++;
  }


}
END{
  for(i=0; i<NUM; i++){
    LIST=ORDER[i]" "LIST;

  }
  print LIST;
}

