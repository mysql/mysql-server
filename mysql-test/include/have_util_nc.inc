# ==== Purpose ====
#
# Check if nc/netcat utility is available
# The variable $nc contains name of utility
# (nc or netcat)
#
# ==== Usage ====
#
# --source include/have_util_nc.inc
# ...
# --exec $nc some options
#

disable_result_log;
disable_query_log;

--let $nc=
--let $_outfile= $MYSQLTEST_VARDIR/tmp/have_nc.tmp

# find nc

--error 0,1,127
--exec which nc > $_outfile 2>&1
--eval SET @content = LOAD_FILE('$_outfile')
--remove_file $_outfile

if (`SELECT LOCATE('/bin/nc', @content) > 0`) {
  --let $nc= nc
  SET @content="";
}

# find netcat

if (`SELECT '$nc' = ''`) {
  --error 0,1,127
  --exec which netcat > $_outfile 2>&1
  --eval SET @content = LOAD_FILE('$_outfile')
  --remove_file $_outfile
}

if (`SELECT LOCATE('/bin/netcat', @content) > 0`) {
  --let $nc= netcat
 SET @content="";
}

enable_query_log;
enable_result_log;

# Skip the test case if utility not found
if (`SELECT '$nc' = ''`) {
  --skip nc/netcat not found
}
