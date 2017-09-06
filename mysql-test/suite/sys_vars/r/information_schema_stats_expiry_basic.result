SET @@global.information_schema_stats_expiry = DEFAULT;
SELECT @@global.information_schema_stats_expiry;
@@global.information_schema_stats_expiry
86400
SET @@session.information_schema_stats_expiry = DEFAULT;
SELECT @@session.information_schema_stats_expiry;
@@session.information_schema_stats_expiry
86400
SET @@global.information_schema_stats_expiry= 0;
SELECT @@global.information_schema_stats_expiry;
@@global.information_schema_stats_expiry
0
SET @@global.information_schema_stats_expiry = 31536000;
SELECT @@global.information_schema_stats_expiry;
@@global.information_schema_stats_expiry
31536000
SET @@session.information_schema_stats_expiry =6000;
SELECT @@session.information_schema_stats_expiry;
@@session.information_schema_stats_expiry
6000
SET @@global.information_schema_stats_expiry = 0;
SELECT @@global.information_schema_stats_expiry;
@@global.information_schema_stats_expiry
0
SET @@global.information_schema_stats_expiry = -1024;
Warnings:
Warning	1292	Truncated incorrect information_schema_stats_expiry value: '-1024'
SELECT @@global.information_schema_stats_expiry;
@@global.information_schema_stats_expiry
0
SET @@global.information_schema_stats_expiry = 123456789031;
Warnings:
Warning	1292	Truncated incorrect information_schema_stats_expiry value: '123456789031'
SELECT @@global.information_schema_stats_expiry;
@@global.information_schema_stats_expiry
31536000
SET @@global.information_schema_stats_expiry = 31536003;
Warnings:
Warning	1292	Truncated incorrect information_schema_stats_expiry value: '31536003'
SELECT @@global.information_schema_stats_expiry;
@@global.information_schema_stats_expiry
31536000
SET @@global.information_schema_stats_expiry = ' ';
ERROR 42000: Incorrect argument type to variable 'information_schema_stats_expiry'
SELECT @@global.information_schema_stats_expiry;
@@global.information_schema_stats_expiry
31536000
SET @@global.information_schema_stats_expiry = ON;
ERROR 42000: Incorrect argument type to variable 'information_schema_stats_expiry'
SET @@global.information_schema_stats_expiry = OFF;
ERROR 42000: Incorrect argument type to variable 'information_schema_stats_expiry'
SET @@global.information_schema_stats_expiry = True;
SELECT @@global.information_schema_stats_expiry;
@@global.information_schema_stats_expiry
1
SET @@global.information_schema_stats_expiry = False;
SELECT @@global.information_schema_stats_expiry;
@@global.information_schema_stats_expiry
0
SET @@global.information_schema_stats_expiry = 65530.34;
ERROR 42000: Incorrect argument type to variable 'information_schema_stats_expiry'
SELECT @@global.information_schema_stats_expiry;
@@global.information_schema_stats_expiry
0
SET @@global.information_schema_stats_expiry ="Test";
ERROR 42000: Incorrect argument type to variable 'information_schema_stats_expiry'
SET @@session.information_schema_stats_expiry = ON;
ERROR 42000: Incorrect argument type to variable 'information_schema_stats_expiry'
SET @@session.information_schema_stats_expiry = OFF;
ERROR 42000: Incorrect argument type to variable 'information_schema_stats_expiry'
SET @@session.information_schema_stats_expiry = True;
SELECT @@session.information_schema_stats_expiry;
@@session.information_schema_stats_expiry
1
SET @@session.information_schema_stats_expiry = False;
SELECT @@session.information_schema_stats_expiry;
@@session.information_schema_stats_expiry
0
SET @@session.information_schema_stats_expiry = "Test";
ERROR 42000: Incorrect argument type to variable 'information_schema_stats_expiry'
SET @@session.information_schema_stats_expiry = 65530.34;
ERROR 42000: Incorrect argument type to variable 'information_schema_stats_expiry'
SELECT @@session.information_schema_stats_expiry;
@@session.information_schema_stats_expiry
0
SET @@session.information_schema_stats_expiry = 'test';
ERROR 42000: Incorrect argument type to variable 'information_schema_stats_expiry'
SET @@session.information_schema_stats_expiry = 0;
SELECT @@session.information_schema_stats_expiry;
@@session.information_schema_stats_expiry
0
SET @@session.information_schema_stats_expiry = ' ';
ERROR 42000: Incorrect argument type to variable 'information_schema_stats_expiry'
SELECT @@session.information_schema_stats_expiry;
@@session.information_schema_stats_expiry
0
SET @@global.information_schema_stats_expiry = 30000;
SET @@global.information_schema_stats_expiry = 40000;
SELECT @@information_schema_stats_expiry = @@global.information_schema_stats_expiry;
@@information_schema_stats_expiry = @@global.information_schema_stats_expiry
0
SET @@information_schema_stats_expiry = 100;
SELECT @@information_schema_stats_expiry = @@local.information_schema_stats_expiry;
@@information_schema_stats_expiry = @@local.information_schema_stats_expiry
1
SELECT @@local.information_schema_stats_expiry = @@session.information_schema_stats_expiry;
@@local.information_schema_stats_expiry = @@session.information_schema_stats_expiry
1
SET @@global.information_schema_stats_expiry = default;
SET @@session.information_schema_stats_expiry = default;
