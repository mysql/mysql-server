connect (master,127.0.0.1,root,,test,$MASTER_MYPORT,);
connect (master1,127.0.0.1,root,,test,$MASTER_MYPORT,);
connect (slave,127.0.0.1,root,,test,$SLAVE_MYPORT,);
connect (slave1,127.0.0.1,root,,test,$SLAVE_MYPORT,);
connection slave;
#we expect STOP SLAVE to produce a warning as the slave is stopped
#(the server was started with skip-slave-start)
--disable_warnings
stop slave;
--enable_warnings
--require r/slave-stopped.result
show status like 'Slave_running';
connection master;
--disable_warnings
drop table if exists t1,t2,t3,t4,t5,t6,t7,t8,t9;
--enable_warnings
reset master;
connection slave;
reset slave;
# Clean up old test tables
--disable_warnings
drop table if exists t1,t2,t3,t4,t5,t6,t7,t8,t9;
--enable_warnings
start slave;
--require r/slave-running.result
show status like 'Slave_running';

# Set the default connection to 'master'
connection master;
