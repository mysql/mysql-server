#
# This is a helper script for rpl_row_img.test. It creates
# all combinations MyISAM / InnoDB in a three server replication 
# chain. Each engine combination is tested against the current
# seetings for binlog_row_image (on each server). 
#
# The test script that is executed on every combination is the
# only argument to this wrapper script. See below.
# 
# This script takes one parameter:
#  - $row_img_test_script
#    the name of the test script to include in every combination
#
# Sample usage:
#   -- let $row_img_test_script= common/rpl/rpl_row_img.test
#   -- source include/rpl/row_img_general_loop.test


-- let $engine_type_a= 2
-- let $server_1_engine=  MyISAM
while($engine_type_a)
{
  -- let $engine_type_b= 2
  -- let $server_2_engine=  MyISAM
  while($engine_type_b)
  {
    -- let $engine_type_c= 2
    -- let $server_3_engine=  MyISAM
    while($engine_type_c)
    {
      -- echo ### engines: $server_1_engine, $server_2_engine, $server_3_engine
      -- source $row_img_test_script

      -- let $server_3_engine= InnoDB
      -- dec $engine_type_c
    }
    -- let $server_2_engine= InnoDB
    -- dec $engine_type_b
  }
  -- let $server_1_engine= InnoDB
  -- dec $engine_type_a
}
