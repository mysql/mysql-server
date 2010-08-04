--echo "BEGIN ctype_german.inc"
#
# Bug #27877 incorrect german order in utf8_general_ci
#
# Testing if "SHARP S" is equal to "S",
# like in latin1_german1_ci, utf8_general_ci, ucs2_general_ci
# Or if "SHART S" is equal to "SS",
# like in latin1_german2_ci, utf8_unicode_ci, ucs2_unicode_ci
# 
# Also testing A-uml, O-uml, U-uml
#

--disable_warnings
drop table if exists t1;
--enable_warnings

#
# Create a table with a varchar(x) column,
# using current values of
# @@character_set_connection and  @@collation_connection.
#

create table t1 as select repeat(' ', 64) as s1;
select collation(s1) from t1;
delete from t1;

#
# Populate data
#

INSERT INTO t1 VALUES ('ud'),('uf');
INSERT INTO t1 VALUES ('od'),('of');
INSERT INTO t1 VALUES ('e');
INSERT INTO t1 VALUES ('ad'),('af');

insert into t1 values ('a'),('ae'),(_latin1 0xE4);
insert into t1 values ('o'),('oe'),(_latin1 0xF6);
insert into t1 values ('s'),('ss'),(_latin1 0xDF);
insert into t1 values ('u'),('ue'),(_latin1 0xFC);

# LIGATURE AE
INSERT INTO t1 VALUES (_latin1 0xE6), (_latin1 0xC6);
# LIGATURE OE
INSERT INTO t1 VALUES (_latin1 0x9C), (_latin1 0x8C);

#
# Check order
#
select s1, hex(s1) from t1 order by s1, binary s1;
select group_concat(s1 order by binary s1) from t1 group by s1;

SELECT s1, hex(s1), hex(weight_string(s1)) FROM t1 ORDER BY s1, BINARY(s1);
SELECT s1, hex(s1) FROM t1 WHERE s1='ae' ORDER BY s1, BINARY(s1);

drop table t1;

--echo "END ctype_german.inc"
