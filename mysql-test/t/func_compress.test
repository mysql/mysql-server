#
# Test for compress and uncompress functions:
#
# Note that this test gives error in the gzip library when running under
# valgrind, but these warnings can be ignored

select @test_compress_string:='string for test compress function aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa ';
select length(@test_compress_string);

select uncompress(compress(@test_compress_string));
explain select uncompress(compress(@test_compress_string));
select uncompressed_length(compress(@test_compress_string))=length(@test_compress_string);
explain select uncompressed_length(compress(@test_compress_string))=length(@test_compress_string);
select uncompressed_length(compress(@test_compress_string));
select length(compress(@test_compress_string))<length(@test_compress_string);

create table t1 (a blob, b binary(255), c char(4)) engine=innodb;
insert into t1 (a,b,c) values (compress(@test_compress_string),compress(@test_compress_string),'d ');
select uncompress(a) from t1;
select uncompress(b) from t1;
select concat('|',c,'|') from t1;
drop table t1;

select compress("");
select uncompress("");
select uncompress(compress(""));
select uncompressed_length("");

#
# errors
#

create table t1 (a blob);
insert t1 values (compress(null)), ('A\0\0\0BBBBBBBB'), (compress(space(50000))), (space(50000));
select length(a) from t1;
select length(uncompress(a)) from t1;
drop table t1;


#
# Bug #5497: a problem with large strings
# note that when LOW_MEMORY is set the "test" below is meaningless
#

set @@global.max_allowed_packet=1048576*100;
--replace_result "''" XXX "'1'" XXX

# reconnect to make the new max packet size take effect
--connect (newconn, localhost, root,,)
eval select compress(repeat('aaaaaaaaaa', IF('$LOW_MEMORY', 10, 10000000))) is null;
disconnect newconn;
--source include/wait_until_disconnected.inc
connection default;
set @@global.max_allowed_packet=default;

#
# Bug #18643: problem with null values
#

create table t1(a blob);
insert into t1 values(NULL), (compress('a'));
select uncompress(a), uncompressed_length(a) from t1;
drop table t1;

#
# Bug #23254: problem with compress(NULL)
#

create table t1(a blob);
insert into t1 values ('0'), (NULL), ('0');
--disable_result_log
select compress(a), compress(a) from t1;
--enable_result_log
select compress(a) is null from t1;
drop table t1;

--echo End of 4.1 tests

#
# Bug #18539: uncompress(d) is null: impossible?
#
create table t1 (a varchar(32) not null);
insert into t1 values ('foo');
analyze table t1;
explain select * from t1 where uncompress(a) is null;
select * from t1 where uncompress(a) is null;
explain select *, uncompress(a) from t1;
select *, uncompress(a) from t1;
select *, uncompress(a), uncompress(a) is null from t1;
drop table t1;

#
# Bug #44796: valgrind: too many my_longlong10_to_str_8bit warnings after 
#             uncompressed_length
#

CREATE TABLE t1 (c1 INT);
INSERT INTO t1 VALUES (1), (1111), (11111);

# Disable warnings to avoid dependency on max_allowed_packet value
--disable_warnings
SELECT UNCOMPRESS(c1), UNCOMPRESSED_LENGTH(c1) FROM t1;
--enable_warnings

# We do not need the results, just make sure there are no valgrind errors
--disable_result_log
EXPLAIN SELECT * FROM (SELECT UNCOMPRESSED_LENGTH(c1) FROM t1) AS s;
--enable_result_log

DROP TABLE t1;

--echo End of 5.0 tests

--echo #
--echo # Bug#18693654 VALGRIND WARNINGS IN INFLATE ON UNCOMPRESS
--echo #

SELECT UNCOMPRESS( CAST( 0 AS BINARY(5) ) );
