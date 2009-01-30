--echo **** Resetting master and slave ****
connection slave;
source include/stop_slave.inc;
RESET SLAVE;
connection master;
RESET MASTER;
connection slave;
source include/start_slave.inc;
