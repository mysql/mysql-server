mi_test2 -s -L -K -R1 -m2000 ;  Should give error 135
Error: 135 in write at record: 1105
got error: 135 when using MyISAM-database
myisamchk: MyISAM file test2
myisamchk: warning: Datafile is almost full,      65532 of      65534 used
MyISAM-table 'test2' is usable but should be fixed
Commands   Used count    Errors   Recover errors
open               17         0                0
write             850         0                0
update             85         0                0
delete            850         0                0
close              17         0                0
extra             102         0                0
Total            1921         0                0
Commands   Used count    Errors   Recover errors
open               18         0                0
write             900         0                0
update             90         0                0
delete            900         0                0
close              18         0                0
extra             108         0                0
Total            2034         0                0

real	0m1.054s
user	0m0.410s
sys	0m0.640s

real	0m1.077s
user	0m0.550s
sys	0m0.530s

real	0m1.100s
user	0m0.420s
sys	0m0.680s

real	0m0.783s
user	0m0.590s
sys	0m0.200s

real	0m0.764s
user	0m0.560s
sys	0m0.210s

real	0m0.699s
user	0m0.570s
sys	0m0.130s

real	0m0.991s
user	0m0.630s
sys	0m0.350s
