include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
[connection slave]
# Success criteria: should produce an error
CHANGE REPLICATION SOURCE TO
SOURCE_COMPRESSION_ALGORITHMS='aaaaaaaaaabbbbbbbbbbccccccccccddddddddddeeeeeee
eeeffffffffffgggggggggghhhhhhhhhhhiiiiiiiiiijjjjjjjjjjkkkkkkkkkk';
ERROR HY000: Value too long setting SOURCE_COMPRESSION_ALGORITHMS option to a 112 chars long string for channel ''.
include/start_slave.inc
include/rpl_end.inc
# End of 8.0 tests
