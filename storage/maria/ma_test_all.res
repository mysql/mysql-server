maria_chk: MARIA file test1
maria_chk: warning: Size of indexfile is: 8192          Should be: 16384
MARIA-table 'test1' is usable but should be fixed
ma_test2 -s -L -K -R1 -m2000 ;  Should give error 135
Error: 135 in write at record: 1105
got error: 135 when using MARIA-database
maria_chk: MARIA file test2
maria_chk: warning: Datafile is almost full,      65532 of      65534 used
MARIA-table 'test2' is usable but should be fixed
Commands   Used count    Errors   Recover errors
open                1         0                0
write              50         0                0
update              5         0                0
delete             50         0                0
close               1         0                0
extra               6         0                0
Total             113         0                0
Commands   Used count    Errors   Recover errors
open                2         0                0
write             100         0                0
update             10         0                0
delete            100         0                0
close               2         0                0
extra              12         0                0
Total             226         0                0

real	0m0.994s
user	0m0.432s
sys	0m0.184s

real	0m2.153s
user	0m1.196s
sys	0m0.228s

real	0m1.483s
user	0m0.772s
sys	0m0.180s

real	0m1.992s
user	0m1.180s
sys	0m0.188s

real	0m2.028s
user	0m1.184s
sys	0m0.152s

real	0m1.878s
user	0m1.028s
sys	0m0.136s

real	0m1.980s
user	0m1.116s
sys	0m0.192s
