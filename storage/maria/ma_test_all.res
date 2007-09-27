Running tests with dynamic row format
Running tests with static row format
Running tests with block row format
ma_test2 -s -L -K -R1 -m2000 ;  Should give error 135
Error: 135 in write at record: 1099
got error: 135 when using MARIA-database
./maria_chk -sm test2 will warn that 'Datafile is almost full'
maria_chk: MARIA file test2
maria_chk: warning: Datafile is almost full,      65516 of      65534 used
MARIA-table 'test2' is usable but should be fixed

real	0m0.808s
user	0m0.584s
sys	0m0.212s

real	0m0.780s
user	0m0.584s
sys	0m0.176s

real	0m0.809s
user	0m0.616s
sys	0m0.180s

real	0m1.356s
user	0m1.140s
sys	0m0.188s

real	0m0.783s
user	0m0.600s
sys	0m0.176s

real	0m1.390s
user	0m1.184s
sys	0m0.152s

real	0m1.875s
user	0m1.632s
sys	0m0.244s

real	0m1.313s
user	0m1.148s
sys	0m0.160s

real	0m1.846s
user	0m1.644s
sys	0m0.188s

real	0m1.875s
user	0m1.632s
sys	0m0.212s

real	0m1.819s
user	0m1.672s
sys	0m0.124s

real	0m2.117s
user	0m1.816s
sys	0m0.292s

real	0m1.871s
user	0m1.636s
sys	0m0.196s
