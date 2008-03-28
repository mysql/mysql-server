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
extern struct st_mysql_plugin	i_s_innodb_cmp;
extern struct st_mysql_plugin	i_s_innodb_cmp_reset;
extern struct st_mysql_plugin	i_s_innodb_cmpmem;
extern struct st_mysql_plugin	i_s_innodb_cmpmem_reset;

#endif /* i_s_h */
