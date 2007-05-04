#### suite/funcs_1/datadict/datadict_load.inc
#
# The sub testcases are nearly independend. That is the reason why we do not
# want to abort after the first error.
--disable_abort_on_error

################################################################################
#
#  prepare variables for --replace_result
#
################################################################################
--disable_query_log

# ------------------------------------------------------------------------------
# Get the size of ONE known colum and check the size against some values to 
# be able to use the correct --replace_result statement. Using this only the 
# one pair of 'wrong' values is replaced and not all occurrencies of all 
# possible pairs of values. See bug #12777 for details.
SELECT character_maximum_length INTO @CML 
  FROM information_schema.columns 
 WHERE table_schema = 'information_schema' 
   AND table_name   = 'columns' 
   AND column_name  = 'table_catalog';

let $bug_12777_0512= `SELECT @CML =  512`;
let $bug_12777_1023= `SELECT @CML = 1023`;
let $bug_12777_1024= `SELECT @CML = 1024`;
let $bug_12777_2048= `SELECT @CML = 2048`;
# 4096 is the value used in the .results
let $bug_12777_4095= `SELECT @CML = 4095`;

if (0)
{
   # enable this for debugging only, but NOT in a pushed version, as then the
   # result changes from OS to OS ...
   eval SELECT @CML AS 'CML',
               $bug_12777_0512 AS '512',
               $bug_12777_1023 AS '1023',
               $bug_12777_1024 AS '1024',
               $bug_12777_2048 AS '2048',
               $bug_12777_4095 AS '4095';
}

# ------------------------------------------------------------------------------
# prepare a variable to be able to suppress machine dependant diffs
# this can be used in: --replace_result $SERVER_NAME <SERVER_NAME>
# let $SERVER_NAME= `SELECT DISTINCT host FROM mysql.user WHERE host LIKE "%\%" AND host NOT In ("localhost", "127.0.0.1", "%")`;
let $SERVER_NAME= `SELECT DISTINCT host FROM mysql.user WHERE host NOT In ("localhost", "127.0.0.1", "%")`;


################################################################################
#
#  load tables
#  -----------
#
#  this was part of the 3 files $<engine>_datadict.test, but it has been moved 
#  here to have only one place where all preparation for the test is done.
#
################################################################################

eval SET @ENGINE_INNODB = IF( '$engine_type' = 'innodb', 1, 0);
eval SET @ENGINE_MEMORY = IF( '$engine_type' = 'memory', 1, 0);
eval SET @ENGINE_MYISAM = IF( '$engine_type' = 'myisam', 1, 0);
--enable_query_log

let $engine_myisam= `SELECT @ENGINE_MYISAM = 1`;
let $engine_innodb= `SELECT @ENGINE_INNODB = 1`;
let $engine_memory= `SELECT @ENGINE_MEMORY = 1`;

# Decide, if the objects are to be (re)created
#
# - once at the beginning of a set of testcases ('$NO_REFRESH' <> '' --> TRUE)
#   That means the current script must not (re)create any object.
#   It can expect, that the objects already exist.
#
# - per every testscript/case ('$NO_REFRESH' = '' --> FALSE)
#   That means all objects have to be (re)created within the current script.
#
eval SET @NO_REFRESH = IF( '$NO_REFRESH' = '', 0, 1);
let $run= `SELECT @NO_REFRESH = 0`;
if ($run)
{
   --disable_warnings
   DROP DATABASE IF EXISTS test1;
   --enable_warnings
   CREATE DATABASE test1;
   USE test;

   # until a statement 'eval --source suite/funcs_1/include/$var_tb1.inc
   # works we need to have similar statements for each $engine
   if ($engine_innodb)
   {
       --source suite/funcs_1/include/innodb_tb1.inc
       --source suite/funcs_1/include/innodb_tb2.inc
       --source suite/funcs_1/include/innodb_tb3.inc
       --source suite/funcs_1/include/innodb_tb4.inc
       USE test1;
       --source suite/funcs_1/include/innodb_tb2.inc
   }

   if ($engine_memory)
   {
      --source suite/funcs_1/include/memory_tb1.inc
      --source suite/funcs_1/include/memory_tb2.inc
      --source suite/funcs_1/include/memory_tb3.inc
      --source suite/funcs_1/include/memory_tb4.inc
      USE test1;
      --source suite/funcs_1/include/memory_tb2.inc
   }

   if ($engine_myisam)
   {
      --source suite/funcs_1/include/myisam_tb1.inc
      --source suite/funcs_1/include/myisam_tb2.inc
      --source suite/funcs_1/include/myisam_tb3.inc
      --source suite/funcs_1/include/myisam_tb4.inc
      USE test1;
      --source suite/funcs_1/include/myisam_tb2.inc
   }
   USE test;
   --source suite/funcs_1/include/sp_tb.inc
}
