###############################################################################
#                                                                             #
# Variable Name: Host_Cache_Size                                              #
# Scope: Global                                                               #
# Access Type: Dynamic                                                        #
# Data Type: numeric                                                          #
#                                                                             #
#                                                                             #
# Creation Date: 2012-08-31                                                   #
# Author : Tanjot Singh Uppal                                                 #
#                                                                             #
#                                                                             #
# Description:Test Cases of Dynamic System Variable Host_Cache_Size           #
#             that checks the behavior of this variable in the following ways #
#              * Value Check                                                  #
#              * Scope Check                                                  #
#              * Functionality Check                                          #
#              * Accessability Check                                          #
#                                                                             #               
# This test does not perform the crash recovery on this variable              # 
# For crash recovery test on default change please run the ibtest             #
###############################################################################

echo '#________________________VAR_06_Host_Cache_Size__________________#'
echo '##'
--echo '#---------------------WL6372_VAR_6_01----------------------#'
####################################################################
#   Checking default value                                         #
####################################################################
SELECT COUNT(@@GLOBAL.Host_Cache_Size);
--echo 1 Expected

set @Default_host_cache_size=(select if(if(@@global.max_connections<500,128+@@global.max_connections,128+@@global.max_connections+floor((@@global.max_connections-500)/20))>2000,2000,if(@@global.max_connections<500,128+@@global.max_connections,128+@@global.max_connections+floor((@@global.max_connections-500)/20))));

select @@global.Host_Cache_Size=@Default_host_cache_size;
--echo 1 Expected


--echo '#---------------------WL6372_VAR_6_02----------------------#'
#################################################################################
# Checking the Default value post starting the server with other value          #
#################################################################################
--echo # Restart server with Host_Cache_Size 1

let $restart_file= $MYSQLTEST_VARDIR/tmp/mysqld.1.expect;
--exec echo "wait" > $restart_file
--shutdown_server 
--source include/wait_until_disconnected.inc
-- exec echo "restart:--host_cache_size=1  " > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
-- source include/wait_until_connected_again.inc

SELECT @@GLOBAL.Host_Cache_Size;
--echo 1 Expected

set @Default_host_cache_size=(select if(if(@@global.max_connections<500,128+@@global.max_connections,128+@@global.max_connections+floor((@@global.max_connections-500)/20))>2000,2000,if(@@global.max_connections<500,128+@@global.max_connections,128+@@global.max_connections+floor((@@global.max_connections-500)/20))));
SET @@GLOBAL.Host_Cache_Size=DEFAULT;
select @@global.Host_Cache_Size=@Default_host_cache_size;
--echo 1 Expected


--echo '#---------------------WL6372_VAR_6_03----------------------#'
####################################################################
#   Checking Value can be set - Dynamic                            #
####################################################################
--error ER_GLOBAL_VARIABLE 
SET @@local.Host_Cache_Size=1;
--echo Expected error 'Global variable'

--error ER_GLOBAL_VARIABLE 
SET @@session.Host_Cache_Size=1;
--echo Expected error 'Global variable'

SET @@GLOBAL.Host_Cache_Size=1;
SET @@GLOBAL.Host_Cache_Size=DEFAULT;

SELECT COUNT(@@GLOBAL.Host_Cache_Size);
--echo 1 Expected

select @@global.Host_Cache_Size=@Default_host_cache_size;
--echo 1 Expected

--echo '#---------------------WL6372_VAR_6_04----------------------#'
#################################################################
# Check if the value in GLOBAL Table matches value in variable  #
#################################################################
SELECT @@GLOBAL.Host_Cache_Size = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='Host_Cache_Size';
--echo 1 Expected

SELECT COUNT(@@GLOBAL.Host_Cache_Size);
--echo 1 Expected

SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='Host_Cache_Size';
--echo 1 Expected

--echo '#---------------------WL6372_VAR_6_05----------------------#'
################################################################################
#  Checking Variable Scope                                                     #
################################################################################
SELECT @@Host_Cache_Size = @@GLOBAL.Host_Cache_Size;
--echo 1 Expected

--Error ER_INCORRECT_GLOBAL_LOCAL_VAR
SELECT COUNT(@@local.Host_Cache_Size);
--echo Expected error 'Variable is a GLOBAL variable'

--Error ER_INCORRECT_GLOBAL_LOCAL_VAR
SELECT COUNT(@@SESSION.Host_Cache_Size);
--echo Expected error 'Variable is a GLOBAL variable'

SELECT COUNT(@@GLOBAL.Host_Cache_Size);
--echo 1 Expected

--Error ER_BAD_FIELD_ERROR
SELECT Host_Cache_Size;
--echo Expected error 'Unknown column Host_Cache_Size in field list'

#The below check has been commented out as the IP fetch is different in a P2P connection than BroadBand connection
#--echo '#---------------------WL6372_VAR_6_06----------------------#'
###############################################################################
# Checking the Host   cahce functionality                                     #
###############################################################################

#SET @@GLOBAL.Host_Cache_Size=2;
#--disable_warnings

#--perl
#my $ip=`ifconfig | egrep "inet addr|inet" | sed -e 's/^.*inet addr://' -e 's/^.*inet//'| sed 's/ .*\$//'|egrep -i "broadcast|bcast"|head -1|awk '{print $1}'`;
#open (LOGFH, ">" . $ENV{'MYSQL_TMP_DIR'} . "/bind_ip");
#print LOGFH "let \$bind_ip = $ip;\n";
#close LOGFH;
#EOF

#--source $MYSQL_TMP_DIR/bind_ip
#--remove_file $MYSQL_TMP_DIR/bind_ip

#let $restart_file= $MYSQLTEST_VARDIR/tmp/mysqld.1.expect;
#--exec echo "wait" > $restart_file
#--shutdown_server 
#--source include/wait_until_disconnected.inc
#-- exec echo "restart:--bind-address=$bind_ip  " > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
#-- source include/wait_until_connected_again.inc

#connection default;
#--disable_warnings

#create user binduser;
#grant all on *.* to binduser;

#select count(IP) from performance_schema.host_cache;
#--echo 0 Expected

#connect (con1,$bind_ip,binduser,,);
#select count(IP) from performance_schema.host_cache;
#--echo 1 Expected

#disconnect con1;
#connection default;

#--disable_warnings

#connect (con2,$bind_ip,binduser,,);
#select count(IP) from performance_schema.host_cache;
#--echo 1 Expected

#disconnect con2;

SET @@GLOBAL.Host_Cache_Size=DEFAULT;
