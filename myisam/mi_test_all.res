mi_test2 -s -L -K -R1 -m2000 ;  Should give error 135
Error: 135 in write at record: 1105
got error: 135 when using MyISAM-database
myisamchk: MyISAM file test2
myisamchk: warning: Datafile is almost full,      65532 of      65534 used
MyISAM-table 'test2' is usable but should be fixed
Commands   Used count    Errors   Recover errors
open                7         0                0
write             350         0                0
update             35         0                0
delete            350         0                0
close               7         0                0
extra              42         0                0
Total             791         0                0
Commands   Used count    Errors   Recover errors
open                8         0                0
write             400         0                0
update             40         0                0
delete            400         0                0
close               8         0                0
extra              48         0                0
Total             904         0                0

real	0m0.221s
user	0m0.120s
sys	0m0.100s

real	0m0.222s
user	0m0.140s
sys	0m0.084s

real	0m0.232s
user	0m0.112s
sys	0m0.120s

real	0m0.163s
user	0m0.116s
sys	0m0.036s

real	0m0.159s
user	0m0.136s
sys	0m0.020s

real	0m0.147s
user	0m0.132s
sys	0m0.016s

real	0m0.211s
user	0m0.124s
sys	0m0.088s
