
#
# Tests that depend on the timestamp and the TZ variable
#

--disable_warnings
drop table if exists t1;
--enable_warnings

# Set timezone to GMT-3, to make it possible to use "interval 3 hour"
set time_zone="+03:00";

create table t1 (Zeit time, Tag tinyint not null, Monat tinyint not null,
Jahr smallint not null, index(Tag), index(Monat), index(Jahr) );
insert into t1 values ("09:26:00",16,9,1998),("09:26:00",16,9,1998);
SELECT CONCAT(Jahr,'-',Monat,'-',Tag,' ',Zeit) AS Date,
   UNIX_TIMESTAMP(CONCAT(Jahr,'-',Monat,'-',Tag,' ',Zeit)) AS Unix
FROM t1;
drop table t1;

# End of 4.1 tests

# Restore timezone to default
set time_zone= @@global.time_zone;
