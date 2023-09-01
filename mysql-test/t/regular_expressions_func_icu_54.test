SET NAMES utf8mb3;

--let $required_icu_version=54
--source include/require_icu_version.inc

--error ER_REGEXP_TIME_OUT
select regexp_like('bbbbXcXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','.X(.+)+X') /* Result: yi */;
--error ER_REGEXP_TIME_OUT
select regexp_like('bbbbXcXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','.X(.+)+XX') /* Result: yi */;
--error ER_REGEXP_TIME_OUT
select regexp_like('bbbbXXcXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','.XX(.+)+X') /* Result: yi */;
--error ER_REGEXP_TIME_OUT
select regexp_like('bbbbXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','.X(.+)+X') /* Result: ni */;
--error ER_REGEXP_TIME_OUT
select regexp_like('bbbbXXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','.X(.+)+XX') /* Result: ni */;
--error ER_REGEXP_TIME_OUT
select regexp_like('bbbbXXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','.XX(.+)+X') /* Result: ni */;
--error ER_REGEXP_TIME_OUT
select regexp_like('bbbbXcXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','.X(.+)+[X]') /* Result: yi */;
--error ER_REGEXP_TIME_OUT
select regexp_like('bbbbXcXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','.X(.+)+[X][X]') /* Result: yi */;
--error ER_REGEXP_TIME_OUT
select regexp_like('bbbbXXcXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','.XX(.+)+[X]') /* Result: yi */;
--error ER_REGEXP_TIME_OUT
select regexp_like('bbbbXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','.X(.+)+[X]') /* Result: ni */;
--error ER_REGEXP_TIME_OUT
select regexp_like('bbbbXXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','.X(.+)+[X][X]') /* Result: ni */;
--error ER_REGEXP_TIME_OUT
select regexp_like('bbbbXXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','.XX(.+)+[X]') /* Result: ni */;
--error ER_REGEXP_TIME_OUT
select regexp_like('bbbbXcXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','.[X](.+)+[X]') /* Result: yi */;
--error ER_REGEXP_TIME_OUT
select regexp_like('bbbbXcXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','.[X](.+)+[X][X]') /* Result: yi */;
--error ER_REGEXP_TIME_OUT
select regexp_like('bbbbXXcXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','.[X][X](.+)+[X]') /* Result: yi */;
--error ER_REGEXP_TIME_OUT
select regexp_like('bbbbXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','.[X](.+)+[X]') /* Result: ni */;
--error ER_REGEXP_TIME_OUT
select regexp_like('bbbbXXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','.[X](.+)+[X][X]') /* Result: ni */;
--error ER_REGEXP_TIME_OUT
select regexp_like('bbbbXXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','.[X][X](.+)+[X]') /* Result: ni */;
