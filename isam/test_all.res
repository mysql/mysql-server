test2 -L -K -W -P
- Creating isam-file
- Writing key:s
- Delete
- Update
- Same key: first - next -> last - prev -> first
- All keys: first - next -> last - prev -> first
- Test if: Read first - next - prev - prev - next == first
- Test if: Read last - prev - next - next - prev == last
- Test read key-part
- Read key (first) - next - delete - next -> last
- Read last of key - prev - delete - prev -> first
- Test if: Read rrnd - same
- Test ni_records_in_range
- ni_info
- ni_extra(CACHE) + ni_rrnd.... + ni_extra(NO_CACHE)
- Removing keys

Following test have been made:
Write records: 915
Update records: 82
Same-key-read: 6
Delete records: 915
Key cacheing used
Write cacheing used
Locking used
test2 -L -K -W -P -A
- Creating isam-file
- Writing key:s
- Delete
- Update
- Same key: first - next -> last - prev -> first
- All keys: first - next -> last - prev -> first
- Test if: Read first - next - prev - prev - next == first
- Test if: Read last - prev - next - next - prev == last
- Test read key-part
- Read key (first) - next - delete - next -> last
- Read last of key - prev - delete - prev -> first
- Test if: Read rrnd - same
- Test ni_records_in_range
- ni_info
- ni_extra(CACHE) + ni_rrnd.... + ni_extra(NO_CACHE)
- Removing keys

Following test have been made:
Write records: 915
Update records: 82
Same-key-read: 6
Delete records: 915
Key cacheing used
Write cacheing used
Asyncron io with locking used
test2 -L -K -W -P -S -R1 -m500
- Creating isam-file
- Writing key:s
- Delete
- Update
- Same key: first - next -> last - prev -> first
- All keys: first - next -> last - prev -> first
- Test if: Read first - next - prev - prev - next == first
- Test if: Read last - prev - next - next - prev == last
- Test read key-part
- Read key (first) - next - delete - next -> last
- Test if: Read rrnd - same
- Test ni_records_in_range
- ni_info
- ni_extra(CACHE) + ni_rrnd.... + ni_extra(NO_CACHE)
- Removing keys

Following test have been made:
Write records: 484
Update records: 48
Same-key-read: 3
Delete records: 484
Record pointer size: 1
Key cacheing used
Write cacheing used
Locking used
test2 -L -K -R1 -m2000 ; Should give error 135
- Creating isam-file
- Writing key:s
Error: 135 in write at record: 1122
got error: 135 when using NISAM-database
test2 -L -K -P -S -R3 -m50 -b1000000
- Creating isam-file
- Writing key:s
- Delete
- Update
- Same key: first - next -> last - prev -> first
- All keys: first - next -> last - prev -> first
- Test if: Read first - next - prev - prev - next == first
- Test if: Read last - prev - next - next - prev == last
- Test read key-part
- Test if: Read rrnd - same
- Test ni_records_in_range
- ni_info
- ni_extra(CACHE) + ni_rrnd.... + ni_extra(NO_CACHE)
- Removing keys

Following test have been made:
Write records: 50
Update records: 5
Same-key-read: 2
Delete records: 50
Record pointer size: 3
Key cacheing used
Locking used
test2 -L -B
- Creating isam-file
- Writing key:s
- Delete
- Update
- Same key: first - next -> last - prev -> first
- All keys: first - next -> last - prev -> first
- Test if: Read first - next - prev - prev - next == first
- Test if: Read last - prev - next - next - prev == last
- Test read key-part
- Read key (first) - next - delete - next -> last
- Read last of key - prev - delete - prev -> first
- Test if: Read rrnd - same
- Test ni_records_in_range
- ni_info
- ni_extra(CACHE) + ni_rrnd.... + ni_extra(NO_CACHE)
- Removing keys

Following test have been made:
Write records: 912
Update records: 81
Same-key-read: 5
Delete records: 912
Locking used
blobs used
test2 -L -K -W -P -m50 -l
- Creating isam-file
- Writing key:s
- Delete
- Update
- Same key: first - next -> last - prev -> first
- All keys: first - next -> last - prev -> first
- Test if: Read first - next - prev - prev - next == first
- Test if: Read last - prev - next - next - prev == last
- Test read key-part
- Test if: Read rrnd - same
- Test ni_records_in_range
- ni_info
- ni_extra(CACHE) + ni_rrnd.... + ni_extra(NO_CACHE)
- Removing keys

Following test have been made:
Write records: 50
Update records: 5
Same-key-read: 2
Delete records: 50
Key cacheing used
Write cacheing used
Locking used
Commands   Used count    Errors   Recover errors
open                3         0                0
write             150         0                0
update             15         0                0
delete            150         0                0
close               3         0                0
extra              18         0                0
Total             339         0                0
test2 -L -K -W -P -m50 -l -b100
- Creating isam-file
- Writing key:s
- Delete
- Update
- Same key: first - next -> last - prev -> first
- All keys: first - next -> last - prev -> first
- Test if: Read first - next - prev - prev - next == first
- Test if: Read last - prev - next - next - prev == last
- Test read key-part
- Test if: Read rrnd - same
- Test ni_records_in_range
- ni_info
- ni_extra(CACHE) + ni_rrnd.... + ni_extra(NO_CACHE)
- Removing keys

Following test have been made:
Write records: 50
Update records: 5
Same-key-read: 2
Delete records: 50
Key cacheing used
Write cacheing used
Locking used
Commands   Used count    Errors   Recover errors
open                4         0                0
write             200         0                0
update             20         0                0
delete            200         0                0
close               4         0                0
extra              24         0                0
Total             452         0                0
time test2
- Creating isam-file
- Writing key:s
- Delete
- Update
- Same key: first - next -> last - prev -> first
- All keys: first - next -> last - prev -> first
- Test if: Read first - next - prev - prev - next == first
- Test if: Read last - prev - next - next - prev == last
- Test read key-part
- Read key (first) - next - delete - next -> last
- Read last of key - prev - delete - prev -> first
- Test if: Read rrnd - same
- Test ni_records_in_range
- ni_info
- ni_extra(CACHE) + ni_rrnd.... + ni_extra(NO_CACHE)
- Removing keys

Following test have been made:
Write records: 915
Update records: 82
Same-key-read: 6
Delete records: 915
4.77user 6.81system 0:15.07elapsed 76%CPU (0avgtext+0avgdata 0maxresident)k
0inputs+0outputs (0major+0minor)pagefaults 0swaps
time test2 -K
- Creating isam-file
- Writing key:s
- Delete
- Update
- Same key: first - next -> last - prev -> first
- All keys: first - next -> last - prev -> first
- Test if: Read first - next - prev - prev - next == first
- Test if: Read last - prev - next - next - prev == last
- Test read key-part
- Read key (first) - next - delete - next -> last
- Read last of key - prev - delete - prev -> first
- Test if: Read rrnd - same
- Test ni_records_in_range
- ni_info
- ni_extra(CACHE) + ni_rrnd.... + ni_extra(NO_CACHE)
- Removing keys

Following test have been made:
Write records: 915
Update records: 82
Same-key-read: 6
Delete records: 915
Key cacheing used
6.09user 4.33system 0:11.66elapsed 89%CPU (0avgtext+0avgdata 0maxresident)k
0inputs+0outputs (0major+0minor)pagefaults 0swaps
time test2 -L
- Creating isam-file
- Writing key:s
- Delete
- Update
- Same key: first - next -> last - prev -> first
- All keys: first - next -> last - prev -> first
- Test if: Read first - next - prev - prev - next == first
- Test if: Read last - prev - next - next - prev == last
- Test read key-part
- Read key (first) - next - delete - next -> last
- Read last of key - prev - delete - prev -> first
- Test if: Read rrnd - same
- Test ni_records_in_range
- ni_info
- ni_extra(CACHE) + ni_rrnd.... + ni_extra(NO_CACHE)
- Removing keys

Following test have been made:
Write records: 915
Update records: 82
Same-key-read: 6
Delete records: 915
Locking used
5.01user 5.20system 0:10.86elapsed 94%CPU (0avgtext+0avgdata 0maxresident)k
0inputs+0outputs (0major+0minor)pagefaults 0swaps
time test2 -L -K
- Creating isam-file
- Writing key:s
- Delete
- Update
- Same key: first - next -> last - prev -> first
- All keys: first - next -> last - prev -> first
- Test if: Read first - next - prev - prev - next == first
- Test if: Read last - prev - next - next - prev == last
- Test read key-part
- Read key (first) - next - delete - next -> last
- Read last of key - prev - delete - prev -> first
- Test if: Read rrnd - same
- Test ni_records_in_range
- ni_info
- ni_extra(CACHE) + ni_rrnd.... + ni_extra(NO_CACHE)
- Removing keys

Following test have been made:
Write records: 915
Update records: 82
Same-key-read: 6
Delete records: 915
Key cacheing used
Locking used
5.63user 0.97system 0:07.85elapsed 84%CPU (0avgtext+0avgdata 0maxresident)k
0inputs+0outputs (0major+0minor)pagefaults 0swaps
time test2 -L -K -W
- Creating isam-file
- Writing key:s
- Delete
- Update
- Same key: first - next -> last - prev -> first
- All keys: first - next -> last - prev -> first
- Test if: Read first - next - prev - prev - next == first
- Test if: Read last - prev - next - next - prev == last
- Test read key-part
- Read key (first) - next - delete - next -> last
- Read last of key - prev - delete - prev -> first
- Test if: Read rrnd - same
- Test ni_records_in_range
- ni_info
- ni_extra(CACHE) + ni_rrnd.... + ni_extra(NO_CACHE)
- Removing keys

Following test have been made:
Write records: 915
Update records: 82
Same-key-read: 6
Delete records: 915
Key cacheing used
Write cacheing used
Locking used
5.28user 1.32system 0:08.86elapsed 74%CPU (0avgtext+0avgdata 0maxresident)k
0inputs+0outputs (0major+0minor)pagefaults 0swaps
time test2 -L -K -W -S
- Creating isam-file
- Writing key:s
- Delete
- Update
- Same key: first - next -> last - prev -> first
- All keys: first - next -> last - prev -> first
- Test if: Read first - next - prev - prev - next == first
- Test if: Read last - prev - next - next - prev == last
- Test read key-part
- Read key (first) - next - delete - next -> last
- Read last of key - prev - delete - prev -> first
- Test if: Read rrnd - same
- Test ni_records_in_range
- ni_info
- ni_extra(CACHE) + ni_rrnd.... + ni_extra(NO_CACHE)
- Removing keys

Following test have been made:
Write records: 915
Update records: 82
Same-key-read: 6
Delete records: 915
Key cacheing used
Write cacheing used
Locking used
5.32user 0.62system 0:06.13elapsed 96%CPU (0avgtext+0avgdata 0maxresident)k
0inputs+0outputs (0major+0minor)pagefaults 0swaps
