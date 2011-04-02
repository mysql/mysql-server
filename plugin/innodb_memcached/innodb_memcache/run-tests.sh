#!/bin/bash

let npass=0
let nfail=0
let nskip=0

do_test() {
  if 
    memcapable -T "$1" >/dev/null 2&>1 
  then 
    echo "[pass]   $1"
    let npass+=1
  else
    echo "*** [FAIL]   $1 ***"
    let nfail+=1
  fi
}

skip_test() { 
  let nskip+=1
}

# Passing Tests -- ASCII protocol
do_test          "ascii quit"
do_test          "ascii version"
do_test          "ascii verbosity"
do_test          "ascii set"
do_test          "ascii set noreply"
do_test          "ascii get"
do_test          "ascii gets"
do_test          "ascii add"
do_test          "ascii add noreply"
do_test          "ascii replace"
do_test          "ascii replace noreply"
do_test          "ascii delete"
do_test          "ascii delete noreply"
do_test          "ascii stat"
do_test          "ascii mget"
do_test          "ascii cas"   


# Passing Tests -- binary protocol
do_test          "binary noop"
do_test          "binary quit"
do_test          "binary quitq"
do_test          "binary stat"
do_test          "binary version"
do_test          "binary get"                 
do_test          "binary getq"                 
do_test          "binary getk"                 
do_test          "binary getkq"    
do_test          "binary delete"                   
do_test          "binary deleteq"                   
do_test          "binary set"                              
do_test          "binary setq"      
do_test          "binary add"                       
do_test          "binary addq"                        
do_test          "binary replace"                     
do_test          "binary replaceq"                          

skip_test "ascii flush"                 
skip_test "ascii flush noreply"            
skip_test "ascii incr"                        
skip_test "ascii incr noreply"                 
skip_test "ascii decr"                              
skip_test "ascii decr noreply"                      
skip_test "ascii append"                            
skip_test "ascii append noreply"                    
skip_test "ascii prepend"                   
skip_test "ascii prepend noreply"                   

               
skip_test "binary flush"                           
skip_test "binary flushq"      
skip_test "binary append"               
skip_test "binary appendq"              
skip_test "binary prepend"             
skip_test "binary prependq"     


echo 
echo ====== $npass passed ====== $nfail failed ====== $nskip skipped
echo

exit $nfail
