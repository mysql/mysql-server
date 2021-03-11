'#________________________VAR_06_Host_Cache_Size__________________#'
echo '##'
--echo '#---------------------WL6372_VAR_6_01----------------------#'
####################################################################
#   Checking default value                                         #
####################################################################
SELECT COUNT(@@GLOBAL.Host_Cache_Size)
1 Expected
set @Default_host_cache_size=(select if(if(@@global.max_connections<500,128+@@global.max_connections,128+@@global.max_connections+floor((@@global.max_connections-500)/20))>2000,2000,if(@@global.max_connections<500,128+@@global.max_connections,128+@@global.max_connections+floor((@@global.max_connections-500)/20))));
select @@global.Host_Cache_Size=@Default_host_cache_size;
@@global.Host_Cache_Size=@Default_host_cache_size
1
1 Expected
'#---------------------WL6372_VAR_6_02----------------------#'
# Restart server with Host_Cache_Size 1
SELECT @@GLOBAL.Host_Cache_Size;
@@GLOBAL.Host_Cache_Size
1
1 Expected
set @Default_host_cache_size=(select if(if(@@global.max_connections<500,128+@@global.max_connections,128+@@global.max_connections+floor((@@global.max_connections-500)/20))>2000,2000,if(@@global.max_connections<500,128+@@global.max_connections,128+@@global.max_connections+floor((@@global.max_connections-500)/20))));
SET @@GLOBAL.Host_Cache_Size=DEFAULT;
select @@global.Host_Cache_Size=@Default_host_cache_size;
@@global.Host_Cache_Size=@Default_host_cache_size
1
1 Expected
'#---------------------WL6372_VAR_6_03----------------------#'
SET @@local.Host_Cache_Size=1;
ERROR HY000: Variable 'host_cache_size' is a GLOBAL variable and should be set with SET GLOBAL
Expected error 'Global variable'
SET @@session.Host_Cache_Size=1;
ERROR HY000: Variable 'host_cache_size' is a GLOBAL variable and should be set with SET GLOBAL
Expected error 'Global variable'
SET @@GLOBAL.Host_Cache_Size=1;
SET @@GLOBAL.Host_Cache_Size=DEFAULT;
SELECT COUNT(@@GLOBAL.Host_Cache_Size);
COUNT(@@GLOBAL.Host_Cache_Size)
1
1 Expected
select @@global.Host_Cache_Size=@Default_host_cache_size;
@@global.Host_Cache_Size=@Default_host_cache_size
1
1 Expected
'#---------------------WL6372_VAR_6_04----------------------#'
SELECT @@GLOBAL.Host_Cache_Size = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='Host_Cache_Size';
@@GLOBAL.Host_Cache_Size = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.Host_Cache_Size);
COUNT(@@GLOBAL.Host_Cache_Size)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='Host_Cache_Size';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------WL6372_VAR_6_05----------------------#'
SELECT @@Host_Cache_Size = @@GLOBAL.Host_Cache_Size;
@@Host_Cache_Size = @@GLOBAL.Host_Cache_Size
1
1 Expected
SELECT COUNT(@@local.Host_Cache_Size);
ERROR HY000: Variable 'host_cache_size' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.Host_Cache_Size);
ERROR HY000: Variable 'host_cache_size' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.Host_Cache_Size);
COUNT(@@GLOBAL.Host_Cache_Size)
1
1 Expected
SELECT Host_Cache_Size;
ERROR 42S22: Unknown column 'Host_Cache_Size' in 'field list'
Expected error 'Unknown column Host_Cache_Size in field list'
SET @@GLOBAL.Host_Cache_Size=DEFAULT;
