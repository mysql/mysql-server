# ==== Purpose ====
# 
# Ensure that the server is running with binlogging on
#
# ==== Usage ====
#
# source include/have_log_bin.inc;

-- require r/have_log_bin.require
disable_query_log;
show variables like 'log_bin';
enable_query_log;

# For convenience, declare these human-readable, deterministic uuids here
--let $uuida= aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa
--let $uuidb= bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb
--let $uuidc= cccccccc-cccc-cccc-cccc-cccccccccccc
--let $uuidd= dddddddd-dddd-dddd-dddd-dddddddddddd
--let $uuide= eeeeeeee-eeee-eeee-eeee-eeeeeeeeeeee
--let $uuidf= ffffffff-ffff-ffff-ffff-ffffffffffff
