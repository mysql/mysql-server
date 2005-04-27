myisamchk: MyISAM file test1
myisamchk: warning: Size of indexfile is: 1024          Should be: 2048
MyISAM-table 'test1' is usable but should be fixed
mi_test2 -s -L -K -R1 -m2000 ;  Should give error 135
Error: 135 in write at record: 1105
got error: 135 when using MyISAM-database
myisamchk: MyISAM file test2
myisamchk: warning: Datafile is almost full,      65532 of      65534 used
MyISAM-table 'test2' is usable but should be fixed
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

real	0m0.791s
user	0m0.137s
sys	0m0.117s

real	0m0.659s
user	0m0.252s
sys	0m0.102s

real	0m0.571s
user	0m0.188s
sys	0m0.098s

real	0m1.111s
user	0m0.236s
sys	0m0.037s

real	0m0.621s
user	0m0.242s
sys	0m0.022s

real	0m0.698s
user	0m0.248s
sys	0m0.021s

real	0m0.683s
user	0m0.265s
sys	0m0.079s
