CREATE DATABASE test_zone;
USE test_zone;
CREATE TABLE time_zone as SELECT * FROM mysql.time_zone WHERE 1 = 0;
CREATE TABLE time_zone_leap_second as SELECT * FROM mysql.time_zone_leap_second WHERE 1 = 0;
CREATE TABLE time_zone_name as SELECT * FROM mysql.time_zone_name WHERE 1 = 0;
CREATE TABLE time_zone_transition as SELECT * FROM mysql.time_zone_transition WHERE 1 = 0;
CREATE TABLE time_zone_transition_type as SELECT * FROM mysql.time_zone_transition_type WHERE 1 = 0;
# Load system zone table for Japanese zones. (mysql_tzinfo_to_sql <syszonepath>/Japan test_japan).
SELECT (count(*) > 0) FROM time_zone;
(count(*) > 0)
1
SELECT (count(*) > 0) FROM time_zone_name;
(count(*) > 0)
1
SELECT (count(*) > 0) FROM time_zone_transition;
(count(*) > 0)
1
SELECT (count(*) > 0) FROM time_zone_transition_type;
(count(*) > 0)
1
# Load system Moscow zone table with --leap option. (mysql_tzinfo_to_sql --leap <syszonepath>/right/Europe/Moscow)
OK
1
# Load system zone table files in Europe folder. (mysql_tzinfo_to_sql <syszonepath>/Europe)
SELECT (count(*) > 0) FROM time_zone;
(count(*) > 0)
1
SELECT (count(*) > 0) FROM time_zone_name;
(count(*) > 0)
1
SELECT (count(*) > 0) FROM time_zone_transition;
(count(*) > 0)
1
SELECT (count(*) > 0) FROM time_zone_transition_type;
(count(*) > 0)
1
DROP DATABASE test_zone;
