Running tests with dynamic row format
Running tests with static row format
Running tests with block row format
Running tests with block row format and transactions
ma_test2 -s -L -K -R1 -m2000 ;  Should give error 135
Error: 135 in write at record: 1099
got error: 135 when using MARIA-database
./maria_chk -sm test2 will warn that 'Datafile is almost full'
maria_chk: MARIA file test2
maria_chk: warning: Datafile is almost full,      65516 of      65534 used
MARIA-table 'test2' is usable but should be fixed
MARIA RECOVERY TESTS
ALL RECOVERY TESTS OK
!!!!!!!! BUT REMEMBER to FIX this BLOB issue !!!!!!!
