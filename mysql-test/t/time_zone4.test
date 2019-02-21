
#
# Tests for time functions. The difference from func_time test is the
# timezone. In func_time it's GMT-3. In our case it's GMT+10
#

#
# Test for bug bug #9191 "TIMESTAMP/from_unixtime() no longer accepts 2^31-1"
#

select from_unixtime(0);
# check 0 boundary
select unix_timestamp('1969-12-31 14:00:01');

