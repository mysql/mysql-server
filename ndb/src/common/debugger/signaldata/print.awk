BEGIN {
  m_curr="";
  m_count=0;
  m_level=0;
}
/^[ ]*class[ ]+.*{/ { 
  if(m_curr != ""){
    print;
    print "ERROR: " m_curr;
    exit;
  }
  m_curr = $2;
}
/{/ {
   m_level++;
} 
/bool print/{
  m_print=$3;
  i=index($3, "(");
  if(i > 0){
    m_print=substr($3, 0, i-1);
  }
}

/[ ]+Uint32[ ]+[^)]*;/ {
  if(m_level >= 0){
    m=$2;
    i=index($2, ";");
    if(i > 0){
      m=substr($2, 0, i-1);
    }
    m_members[m_count]=m;
    m_count++;
  }
}
/^[ ]*}[ ]*;/ {
  m_level--;
  if(m_level == 0){
    if(m_count > 0 && m_print != ""){
      print "bool";
      print m_print "(FILE * output, const Uint32 * theData, ";
      print "Uint32 len, Uint16 receiverBlockNo) {";
      print "const " m_curr " * const sig = (" m_curr " *)theData;";
      for(i = 0; i<m_count; i++){
	print "fprintf(output, \" " m_members[i] ": %x\\n\", sig->" m_members[i] ");"; 
      }
      print "return true;";
      print "}";
      print "";
    }
    m_curr="";
    m_print="";
    m_count=0;
  }
}
