connect (master,localhost,root,,test,0,master.sock);
connect (master1,localhost,root,,test,0,master.sock);
connect (slave,localhost,root,,test,0,slave.sock);
connect (slave1,localhost,root,,test,0,slave.sock);
connection slave;
!slave stop;
@r/slave-stopped.result show status like 'Slave_running';
connection master;
reset master;
connection slave;
reset slave;
# Clean up old test tables
drop table if exists t1,t2,t3,t4,t5,t6,t7,t8,t9;
slave start;
@r/slave-running.result show status like 'Slave_running';
