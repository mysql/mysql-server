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
- Test nisam_records_in_range
- nisam_info
- nisam_extra(CACHE) + nisam_rrnd.... + nisam_extra(NO_CACHE)
- Removing keys

Following test have been made:
Write records: 907
Update records: 87
Same-key-read: 6
Delete records: 907
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
- Test nisam_records_in_range
- nisam_info
- nisam_extra(CACHE) + nisam_rrnd.... + nisam_extra(NO_CACHE)
- Removing keys

Following test have been made:
Write records: 907
Update records: 87
Same-key-read: 6
Delete records: 907
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
- Test nisam_records_in_range
- nisam_info
- nisam_extra(CACHE) + nisam_rrnd.... + nisam_extra(NO_CACHE)
- Removing keys

Following test have been made:
Write records: 475
Update records: 44
Same-key-read: 4
Delete records: 475
Record pointer size: 1
Key cacheing used
Write cacheing used
Locking used
test2 -L -K -R1 -m2000 ; Should give error 135
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
- Read first - delete - next -> last
- Read last - delete - prev -> first
- Test if: Read rrnd - same
- Test nisam_records_in_range
- nisam_info
- nisam_extra(CACHE) + nisam_rrnd.... + nisam_extra(NO_CACHE)
- Removing keys

Following test have been made:
Write records: 1647
Update records: 125
Same-key-read: 8
Delete records: 1647
Record pointer size: 1
Key cacheing used
Locking used
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
- Test nisam_records_in_range
- nisam_info
- nisam_extra(CACHE) + nisam_rrnd.... + nisam_extra(NO_CACHE)
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
- Test nisam_records_in_range
- nisam_info
- nisam_extra(CACHE) + nisam_rrnd.... + nisam_extra(NO_CACHE)
- Removing keys

Following test have been made:
Write records: 903
Update records: 86
Same-key-read: 5
Delete records: 903
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
- Test nisam_records_in_range
- nisam_info
- nisam_extra(CACHE) + nisam_rrnd.... + nisam_extra(NO_CACHE)
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
open               14         0                0
write             700         0                0
update             70         0                0
delete            700         0                0
close              14         0                0
extra              84         0                0
Total            1582         0                0
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
- Test nisam_records_in_range
- nisam_info
- nisam_extra(CACHE) + nisam_rrnd.... + nisam_extra(NO_CACHE)
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
open               15         0                0
write             750         0                0
update             75         0                0
delete            750         0                0
close              15         0                0
extra              90         0                0
Total            1695         0                0
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
- Test nisam_records_in_range
- nisam_info
- nisam_extra(CACHE) + nisam_rrnd.... + nisam_extra(NO_CACHE)
- Removing keys

Following test have been made:
Write records: 907
Update records: 87
Same-key-read: 6
Delete records: 907
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
- Test nisam_records_in_range
- nisam_info
- nisam_extra(CACHE) + nisam_rrnd.... + nisam_extra(NO_CACHE)
- Removing keys

Following test have been made:
Write records: 907
Update records: 87
Same-key-read: 6
Delete records: 907
Key cacheing used
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
- Test nisam_records_in_range
- nisam_info
- nisam_extra(CACHE) + nisam_rrnd.... + nisam_extra(NO_CACHE)
- Removing keys

Following test have been made:
Write records: 907
Update records: 87
Same-key-read: 6
Delete records: 907
Locking used
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
- Test nisam_records_in_range
- nisam_info
- nisam_extra(CACHE) + nisam_rrnd.... + nisam_extra(NO_CACHE)
- Removing keys

Following test have been made:
Write records: 907
Update records: 87
Same-key-read: 6
Delete records: 907
Key cacheing used
Locking used
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
- Test nisam_records_in_range
- nisam_info
- nisam_extra(CACHE) + nisam_rrnd.... + nisam_extra(NO_CACHE)
- Removing keys

Following test have been made:
Write records: 907
Update records: 87
Same-key-read: 6
Delete records: 907
Key cacheing used
Write cacheing used
Locking used
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
- Test nisam_records_in_range
- nisam_info
- nisam_extra(CACHE) + nisam_rrnd.... + nisam_extra(NO_CACHE)
- Removing keys

Following test have been made:
Write records: 907
Update records: 87
Same-key-read: 6
Delete records: 907
Key cacheing used
Write cacheing used
Locking used
