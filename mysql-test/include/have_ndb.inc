-- require r/have_ndb.require
disable_query_log;
show variables like "have_ndbcluster";
enable_query_log;
connect (server1,127.0.0.1,root,,test,$MASTER_MYPORT,$MASTER_MYSOCK);
connect (server2,127.0.0.1,root,,test,$MASTER_MYPORT1,$MASTER_MYSOCK1);
connection server1;
