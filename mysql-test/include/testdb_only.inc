#################### include/testdb_only.inc ######################
#                                                                 #
# We must prevent to work on databases created by customers,      #
# because we DROP/CREATE/MODIFY objects with sometimes common     #
# names like STAFF, EMPLOYEE etc.                                 #
#                                                                 #
# Therefore we check the environment variable USE_RUNNING_SERVER. #
# USE_RUNNING_SERVER is exported by "mysql-test-run" and could    #
# contain the following values:                                   #
# 0  -- mysql-test-run was started without the --extern option    #
#       That means the test will be performed within the test     #
#       area 'mysql-test/var/...' .                               #
# 1  -- mysql-test-run was started with the --extern option       #
#       That means the test will be performed by an already       #
#       running server and data modifications will most probably  #
#       outside of the common test area 'mysql-test/var/...' .    #
#                                                                 #
# If USE_RUNNING_SERVER is not 0 the test will be skipped.        #
#                                                                 #
###################################################################

--disable_query_log
eval set @USE_RUNNING_SERVER= '$USE_RUNNING_SERVER';
--require r/testdb_only.require
SELECT 'use extern server'
         AS "Variable_name    ",
         IF(@USE_RUNNING_SERVER= '1','YES',
            IF(@USE_RUNNING_SERVER= '0','NO','UNEXPECTED'))
         AS "Value" ;
--enable_query_log
