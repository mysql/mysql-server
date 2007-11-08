--echo **** Resetting master and slave ****
connection slave;
STOP SLAVE;
source include/wait_for_slave_to_stop.inc;
RESET SLAVE;
connection master;
RESET MASTER;
connection slave;
START SLAVE;
source include/wait_for_slave_to_start.inc;
