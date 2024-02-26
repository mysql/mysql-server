#########################################
# Author: Serge Kozlov skozlov@mysql.com
# Date: 07/10/2006
# Purpose: Showing binlog_format variables on current server (master or slave)
# Requirements: 
#########################################

show global variables like 'binlog_format%';
show session variables like 'binlog_format%';
select @@global.binlog_format, @@session.binlog_format;
