connect (master,127.0.0.1,root,,test,$MASTER_MYPORT,);
connect (master1,127.0.0.1,root,,test,$MASTER_MYPORT,);
connect (slave,127.0.0.1,root,,test,$SLAVE_MYPORT,);
connect (slave1,127.0.0.1,root,,test,$SLAVE_MYPORT,);
connection slave;
--error 0,1199
!slave stop;
@r/slave-stopped.result show status like 'Slave_running';
connection master;
drop table if exists t1,t2,t3,t4,t5,t6,t7,t8,t9;
reset master;
connection slave;
reset slave;
# Clean up old test tables
drop table if exists t1,t2,t3,t4,t5,t6,t7,t8,t9;
slave start;
@r/slave-running.result show status like 'Slave_running';

# Set the default connection to 'master'
connection master;
