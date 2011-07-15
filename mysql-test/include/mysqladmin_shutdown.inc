# Initiates a clean shutdown of the server and waits for its completion

--exec $MYSQLADMIN -S $MASTER_MYSOCK -P $MASTER_MYPORT  -u root --password= shutdown 2>&1;

# On Windows mysqladmin does not wait for shutdown to be finished,
# so we have to monitor this with our connection:
--source include/wait_until_disconnected.inc
