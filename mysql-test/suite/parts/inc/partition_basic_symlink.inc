################################################################################
# inc/partition_basic_symlink.inc                                              #
#                                                                              #
# Purpose:                                                                     #
#   Basic tests around create partitioned table with/without PRIMARY KEY and   #
#   /or UNIQUE INDEX                                                           #
#   Also includes test for DATA/INDEX DIR which requires symlinked files       #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: mleich                                                      #
# Original Date: 2006-03-05                                                    #
# Change Author: mattiasj                                                      #
# Change Date: 2008-02-06                                                      #
# Change: copied it from partition_basic.inc and kept DATA/INDEX DIR           #
################################################################################
--enable_abort_on_error

--echo
let $unique= ;

# DATA DIRECTORY
# Make directory for partition data
let $data_dir_path= $MYSQLTEST_VARDIR/mysql-test-data-dir;
--mkdir $data_dir_path
let $data_directory= DATA DIRECTORY = '$data_dir_path';

#INDEX DIRECTORY
# Make directory for partition index
let $idx_dir_path= $MYSQLTEST_VARDIR/mysql-test-idx-dir;
--mkdir $idx_dir_path
let $index_directory= INDEX DIRECTORY = '$idx_dir_path';

let $with_directories= 1;
--echo #========================================================================
--echo # 0.5 use partition_basic with DATA/INDEX DIRECTORY
--echo #========================================================================
--source suite/parts/inc/partition_basic.inc
--echo #========================================================================
--echo # 5 use partition_directory with DATA/INDEX DIRECTORY
--echo #========================================================================
--source suite/parts/inc/partition_directory.inc
--rmdir $data_dir_path
--rmdir $idx_dir_path
let $with_directories= 0;
