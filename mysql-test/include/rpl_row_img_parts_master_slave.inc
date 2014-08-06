#
# This is a wrapper to help minimize writing of assertions
# that resort to rpl_row_img_parts_assertion.inc on a 
# master-slave scenario.
#
# This script takes a statement to be executed and master
# and slave expected images description as parameters. Before
# statement execution (on the master only), both servers are
# synchronized, and positions explicitly saved. These are
# then used as starting position in mysqlbinlog -v (see
# suite/rpl/include/rpl_row_img_parts_assertion.inc).
# 
# Parameters:
#  - $row_img_query
#    The query to be executed on the master, which will
#    make master and slave log one or more row events.
#
#  - $row_img_expected_master
#    String representation of the expected images to search
#    on master's mysqlbinlog -v output
#
#  - $row_img_expected_slave
#    String representation of the expected images to search
#    on slaves's mysqlbinlog -v output
#
# Sample usage:
#
#  -- let $row_img_query= INSERT INTO t VALUES (1, 2, 3)
#  -- let $row_img_expected_master= | 1:1 2:2 3:3
#  -- let $row_img_expected_slave = | 1:1 2:2 3:3
#  -- source include/rpl_row_img_parts_master_slave.inc
#
#  -- let $row_img_query= UPDATE t SET c2= 4 WHERE c1=1
#  -- let $row_img_expected_master= 1:1 | 2:4
#  -- let $row_img_expected_slave = 1:1 2:2 3:3 | 1:1 2:4 3:3
#  -- source include/rpl_row_img_parts_master_slave.inc
#
#  -- let $row_img_query= DELETE FROM t WHERE c2=4
#  -- let $row_img_expected_master= 1:1 |
#  -- let $row_img_expected_slave = 1:1 2:4 3:3 | 
#  -- source include/rpl_row_img_parts_master_slave.inc
#
# NOTES:
#
#  1. Both master and slave are synchronized before execution
#     of the query takes place
#  
#  2. original connection - the one in use before including
#     this script - is restored at the end

-- let $old_conn= $CURRENT_CONNECTION

SET sql_mode = 'NO_ENGINE_SUBSTITUTION';

-- connection master
-- sync_slave_with_master

-- let $row_img_pos_slave= query_get_value(SHOW MASTER STATUS, Position, 1)

-- connection master
-- let $row_img_pos_master= query_get_value(SHOW MASTER STATUS, Position, 1)

# execute the query
-- connection master
-- eval $row_img_query

# lets wait until the binary log position changes
-- let $_pos= query_get_value("SHOW MASTER STATUS", Position, 1)

# wait for 5 minutes
-- let $iterations=3000
while($_pos == $row_img_pos_master)
{
  -- sleep 0.1
  -- let $_pos= query_get_value("SHOW MASTER STATUS", Position, 1)
  -- dec $iterations
}

if (!$iterations)
{
  -- echo Time out while waiting for the event to be written to the binary log at the master!
  -- echo Query: $row_img_query
  -- source include/show_rpl_debug_info.inc
  -- exit
}

# now sync the slave
-- sync_slave_with_master

# master assertion
-- connection master
-- let $row_img_expected= $row_img_expected_master
-- let $row_img_pos= $row_img_pos_master
-- source include/rpl_row_img_parts_assertion.inc

# slave assertion 
-- connection slave
-- let $row_img_expected= $row_img_expected_slave
-- let $row_img_pos= $row_img_pos_slave
-- source include/rpl_row_img_parts_assertion.inc

-- connection $old_conn

SET sql_mode = default;
