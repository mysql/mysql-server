# ==== Purpose ====
#
# Check if uz/unzip utility is available
# The variable $uz contains name of utility
# (uz or unzip)
#
# ==== Usage ====
#
# --source include/have_util_unzip.inc
# ...
# --exec $uz some options
#

disable_result_log;
disable_query_log;

--let $uz=
--let $_outfile= $MYSQLTEST_VARDIR/tmp/have_uz.tmp

# find unzip utility 

--error 0,1,127
--exec unzip -h > $_outfile 2>&1
--eval SET @content = LOAD_FILE('$_outfile')
--remove_file $_outfile

if (`SELECT LOCATE('usage', @content) > 0`) {
  --let $uz=unzip 
  SET @content="";
}

# Skip the test case if utility not found
if (`SELECT '$uz' = ''`) {
  --skip the test uz/unzip utility not found
}

enable_query_log;
enable_result_log;

