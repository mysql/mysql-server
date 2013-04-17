#!/bin/bash
#Export BINSUF before running
make db-benchmark-test-tokudb$BINSUF
make scanscan-tokudb$BINSUF
alias db='./db-benchmark-test-tokudb$BINSUF' # standard db-benchmark test
alias db='rm -rf bench.db && ./db-benchmark-test-tokudb$BINSUF' # standard db-benchmark test
alias dbtxn='db -x --singlex --prelock' # db-benchmark test with single transaction
alias dbabort='dbtxn --abort' # db-benchmark test with single transaction (AND ABORT) at end
alias scan='./scanscan-tokudb$BINSUF --lwc --prelock --prelockflag --cachesize 268435456'     # scanscan default, cache of windows (256MB)
alias flatteneddb='db && scan'
alias flattenedtxndb='dbtxn && scan'
alias flatteningscan='(db 2>&1 >/dev/null) && scan'
alias flatteningtxnscan='(dbtxn 2>&1 >/dev/null) && scan'

(flatteningscan)2>&1 >/dev/null # Cache binaries/etc.


echo db-benchmark-test no transactions
db
echo Time for 5 runs:
time for (( i = 0; i < 5; i++ )); do (db) 2>&1 >/dev/null; done
echo
echo ============================
echo db-benchmark-test single transactions
dbtxn
echo Time for 5 runs:
time for (( i = 0; i < 5; i++ )); do (dbtxn) 2>&1 >/dev/null; done
echo
echo ============================
echo db-benchmark-test single transactions ABORT at end
dbabort
echo Time for 5 runs:
time for (( i = 0; i < 5; i++ )); do (dbabort) 2>&1 >/dev/null; done
echo
echo ============================
echo flattening scanscan
flatteningscan
echo Time for 5 runs:
time for (( i = 0; i < 5; i++ )); do (flatteningscan) 2>&1 >/dev/null; done
echo
echo ============================
echo flattening scanscan on txn db
flatteningtxnscan
echo Time for 5 runs:
time for (( i = 0; i < 5; i++ )); do (flatteningtxnscan) 2>&1 >/dev/null; done
echo
echo ============================
echo pre-flattened scanscan
(flatteneddb) 2>&1 >/dev/null
scan
echo Time for 5 runs:
time for (( i = 0; i < 5; i++ )); do (scan) 2>&1 >/dev/null; done
echo
echo ============================
echo pre-flattened scanscan on txn db
(flattenedtxndb) 2>&1 >/dev/null
scan
echo Time for 5 runs:
time for (( i = 0; i < 5; i++ )); do (scan) 2>&1 >/dev/null; done
echo
echo ============================

