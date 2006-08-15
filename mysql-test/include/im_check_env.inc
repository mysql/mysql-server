# This file is intended to be used in each IM-test. It contains stamements,
# that ensure that starting conditions (environment) for the IM-test are as
# expected.

# Wait for mysqld1 (guarded instance) to start.

--exec $MYSQL_TEST_DIR/t/wait_for_process.sh $IM_MYSQLD1_PATH_PID 30 started

# Check the running instances.

--connect (mysql1_con,localhost,root,,mysql,$IM_MYSQLD1_PORT,$IM_MYSQLD1_SOCK)

--connection mysql1_con

SHOW VARIABLES LIKE 'server_id';

--connection default

# Let IM detect that mysqld1 is online. This delay should be longer than
# monitoring interval.

--sleep 2

# Check that IM understands that mysqld1 is online, while mysqld2 is
# offline.

SHOW INSTANCES;
