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
--echo #========================================================================
--echo #  Check partitioning methods on just created tables
--echo #  The tables should be defined without/with PRIMARY KEY and
--echo #  UNIQUE INDEXes.
--echo #  Every test round has to check
--echo #        PARTITION BY HASH/KEY/LIST/RANGE
--echo #        PARTITION BY RANGE/LIST ... SUBPARTITION BY HASH/KEY ...
--echo #========================================================================
--echo #------------------------------------------------------------------------
--echo #  1   Tables without PRIMARY KEY or UNIQUE INDEXes
--echo #------------------------------------------------------------------------
--echo #  1.1 The partitioning function contains one column.
let $unique= ;
--source suite/parts/inc/partition_methods1.inc
#
   --echo # 1.1.1 with DATA DIECTORY/INDEX DIRECTORY
   #
   --disable_query_log
   # DATA DIRECTORY
   # Make directory for partition data
   --exec mkdir $MYSQLTEST_VARDIR/master-data/test/data || true
   eval SET @data_dir = 'DATA DIRECTORY =
   ''''$MYSQLTEST_VARDIR/master-data/test/data''''';
   let $data_directory = `select @data_dir`;

   #INDEX DIRECTORY
   # Make directory for partition index
   --exec mkdir $MYSQLTEST_VARDIR/master-data/test/index || true
   eval SET @indx_dir = 'INDEX DIRECTORY =
   ''''$MYSQLTEST_VARDIR/master-data/test/index''''';
   let $index_directory = `select @indx_dir`;

   let $with_directories= 1;
   --source suite/parts/inc/partition_methods1.inc
   --source suite/parts/inc/partition_directory.inc
   let $with_directories= 0;
   --enable_query_log
   #
--echo #  1.2 The partitioning function contains two columns.
let $unique= ;
--source suite/parts/inc/partition_methods2.inc
#
--echo #------------------------------------------------------------------------
--echo #  2   Tables with PRIMARY KEY and/or UNIQUE INDEXes
--echo #        The partitioning function contains one column.
--echo #------------------------------------------------------------------------
if ($more_pk_ui_tests)
{
   if ($do_pk_tests)
   {
      --echo #  2.1 PRIMARY KEY consisting of one column
      let $unique= , PRIMARY KEY(f_int1);
      --source suite/parts/inc/partition_methods1.inc
   }
   --echo #  2.2 UNIQUE INDEX consisting of one column
   let $unique= , UNIQUE INDEX uidx1 (f_int1);
   --source suite/parts/inc/partition_methods1.inc

   --echo # 2.2.1 with DATA DIECTORY/INDEX DIRECTORY
   #
   --disable_query_log
   # DATA DIRECTORY
   # Make directory for partition data
   --exec mkdir $MYSQLTEST_VARDIR/master-data/test/data || true
   eval SET @data_dir = 'DATA DIRECTORY =
   ''''$MYSQLTEST_VARDIR/master-data/test/data''''';
   let $data_directory = `select @data_dir`;

   #INDEX DIRECTORY
   # Make directory for partition index
   --exec mkdir $MYSQLTEST_VARDIR/master-data/test/index || true
   eval SET @indx_dir = 'INDEX DIRECTORY =
   ''''$MYSQLTEST_VARDIR/master-data/test/index''''';
   let $index_directory = `select @indx_dir`;

   let $with_directories= TRUE;
   --source suite/parts/inc/partition_methods1.inc
   let $with_directories= FALSE;
   --enable_query_log
   #
   if ($do_pk_tests)
   {
      --echo #  2.3 PRIMARY KEY consisting of two columns
      let $unique= , PRIMARY KEY(f_int1,f_int2);
      --source suite/parts/inc/partition_methods1.inc
      let $unique= , PRIMARY KEY(f_int2,f_int1);
      --source suite/parts/inc/partition_methods1.inc
   }
   #
   --echo #  2.4 UNIQUE INDEX consisting of two columns
   let $unique= , UNIQUE INDEX uidx1 (f_int1,f_int2);
   --source suite/parts/inc/partition_methods1.inc
   let $unique= , UNIQUE INDEX uidx1 (f_int2,f_int1);
   --source suite/parts/inc/partition_methods1.inc
   #
}
--echo #  2.5 PRIMARY KEY + UNIQUE INDEX consisting of two columns
if ($do_pk_tests)
{
   let $unique= , UNIQUE INDEX uidx1 (f_int1,f_int2), PRIMARY KEY(f_int2,f_int1);
   --source suite/parts/inc/partition_methods1.inc
   let $unique= , UNIQUE INDEX uidx1 (f_int2,f_int1), PRIMARY KEY(f_int1,f_int2);
   --source suite/parts/inc/partition_methods1.inc
}
let $unique= , UNIQUE INDEX uidx1 (f_int1,f_int2), UNIQUE INDEX uidx2 (f_int2,f_int1);
--source suite/parts/inc/partition_methods1.inc

--echo #------------------------------------------------------------------------
--echo #  3   Tables with PRIMARY KEY and/or UNIQUE INDEXes
--echo #        The partitioning function contains two columns.
--echo #------------------------------------------------------------------------
#
if ($more_pk_ui_tests)
{
   if ($do_pk_tests)
   {
      --echo #  3.1 PRIMARY KEY consisting of two columns
      let $unique= , PRIMARY KEY(f_int1,f_int2);
      --source suite/parts/inc/partition_methods2.inc

      let $unique= , PRIMARY KEY(f_int2,f_int1);
      --source suite/parts/inc/partition_methods2.inc
   }
   #
   --echo #  3.2 UNIQUE INDEX consisting of two columns
   let $unique= , UNIQUE INDEX uidx1 (f_int1,f_int2);
   --source suite/parts/inc/partition_methods2.inc
   let $unique= , UNIQUE INDEX uidx1 (f_int2,f_int1);
   --source suite/parts/inc/partition_methods2.inc
}
#
--echo #  3.3 PRIMARY KEY and UNIQUE INDEX consisting of two columns
if ($do_pk_tests)
{
   let $unique= , UNIQUE INDEX uidx1 (f_int1,f_int2), PRIMARY KEY(f_int2,f_int1);
   --source suite/parts/inc/partition_methods2.inc
   let $unique= , UNIQUE INDEX uidx1 (f_int2,f_int1), PRIMARY KEY(f_int1,f_int2);
   --source suite/parts/inc/partition_methods2.inc
}
let $unique= , UNIQUE INDEX uidx1 (f_int1,f_int2), UNIQUE INDEX uidx2 (f_int2,f_int1);
--source suite/parts/inc/partition_methods2.inc
