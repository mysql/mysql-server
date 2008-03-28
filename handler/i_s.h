/******************************************************
InnoDB INFORMATION SCHEMA tables interface to MySQL.

(c) 2007 Innobase Oy

Created July 18, 2007 Vasil Dimov
*******************************************************/

#ifndef i_s_h
#define i_s_h

extern struct st_mysql_plugin	i_s_innodb_trx;
extern struct st_mysql_plugin	i_s_innodb_locks;
extern struct st_mysql_plugin	i_s_innodb_lock_waits;
extern struct st_mysql_plugin	i_s_innodb_compression;
extern struct st_mysql_plugin	i_s_innodb_compression_reset;

#endif /* i_s_h */
