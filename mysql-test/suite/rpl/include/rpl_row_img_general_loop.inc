#
# This is a helper script for rpl_row_img.test. It creates
# all combinations MyISAM / InnoDB in a three server replication 
# chain. Each engine combination is tested against the three 
# different values for binlog_row_image (same on every server).
# 
# The test script that is executed on every combination is the
# only argument to this wrapper script. See below.
# 
# This script takes one parameter:
#  - $row_img_test_script
#    the name of the test script to include in every combination
#
# Sample usage:
#   -- let $row_img_test_script= extra/rpl_tests/rpl_row_img.test
#   -- source suite/rpl/include/rpl_row_img_general_loop.test

-- let $row_img_set=mysqld_a:FULL:N,mysqld_b:FULL:Y,mysqld_c:FULL:Y
-- source suite/rpl/include/rpl_row_img_set.inc

-- let $image_param_ctr= 3
while($image_param_ctr)
{

  if (`SELECT $image_param_ctr = 2`)
  {
    -- let $row_img_set=mysqld_a:NOBLOB:N,mysqld_b:NOBLOB:Y,mysqld_c:NOBLOB:Y
    -- source suite/rpl/include/rpl_row_img_set.inc
  }

  if (`SELECT $image_param_ctr = 1`)
  {
    -- let $row_img_set=mysqld_a:MINIMAL:N,mysqld_b:MINIMAL:Y,mysqld_c:MINIMAL:Y
    -- source suite/rpl/include/rpl_row_img_set.inc
  }

  -- let $cur_img= `SELECT @@binlog_row_image`
  -- echo #### row image type: $cur_img

  -- let $engine_type_a= 2
  -- let $mysqld_a_engine=  MyISAM
  while($engine_type_a)
  {
    -- let $engine_type_b= 2
    -- let $mysqld_b_engine=  MyISAM
    while($engine_type_b)
    {
      -- let $engine_type_c= 2
      -- let $mysqld_c_engine=  MyISAM
      while($engine_type_c)
      {
        -- echo ### engines: $mysqld_a_engine, $mysqld_b_engine, $mysqld_c_engine
        -- source $row_img_test_script

        -- let $mysqld_c_engine= InnoDB
        -- dec $engine_type_c
      }
      -- let $mysqld_b_engine= InnoDB
      -- dec $engine_type_b
    }
    -- let $mysqld_a_engine= InnoDB
    -- dec $engine_type_a
  }

  -- dec $image_param_ctr
}

-- let $row_img_set=mysqld_a:FULL:N,mysqld_b:FULL:Y,mysqld_c:FULL:Y
-- source suite/rpl/include/rpl_row_img_set.inc
