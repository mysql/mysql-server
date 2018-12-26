SET NAMES utf8;
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
select regexp_like('bbbbXcXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','.X(.+)+X') /* Result: yi */;
ERROR HY000: Timeout exceeded in regular expression match.
select regexp_like('bbbbXcXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','.X(.+)+XX') /* Result: yi */;
ERROR HY000: Timeout exceeded in regular expression match.
select regexp_like('bbbbXXcXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','.XX(.+)+X') /* Result: yi */;
ERROR HY000: Timeout exceeded in regular expression match.
select regexp_like('bbbbXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','.X(.+)+X') /* Result: ni */;
ERROR HY000: Timeout exceeded in regular expression match.
select regexp_like('bbbbXXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','.X(.+)+XX') /* Result: ni */;
ERROR HY000: Timeout exceeded in regular expression match.
select regexp_like('bbbbXXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','.XX(.+)+X') /* Result: ni */;
ERROR HY000: Timeout exceeded in regular expression match.
select regexp_like('bbbbXcXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','.X(.+)+[X]') /* Result: yi */;
ERROR HY000: Timeout exceeded in regular expression match.
select regexp_like('bbbbXcXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','.X(.+)+[X][X]') /* Result: yi */;
ERROR HY000: Timeout exceeded in regular expression match.
select regexp_like('bbbbXXcXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','.XX(.+)+[X]') /* Result: yi */;
ERROR HY000: Timeout exceeded in regular expression match.
select regexp_like('bbbbXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','.X(.+)+[X]') /* Result: ni */;
ERROR HY000: Timeout exceeded in regular expression match.
select regexp_like('bbbbXXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','.X(.+)+[X][X]') /* Result: ni */;
ERROR HY000: Timeout exceeded in regular expression match.
select regexp_like('bbbbXXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','.XX(.+)+[X]') /* Result: ni */;
ERROR HY000: Timeout exceeded in regular expression match.
select regexp_like('bbbbXcXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','.[X](.+)+[X]') /* Result: yi */;
ERROR HY000: Timeout exceeded in regular expression match.
select regexp_like('bbbbXcXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','.[X](.+)+[X][X]') /* Result: yi */;
ERROR HY000: Timeout exceeded in regular expression match.
select regexp_like('bbbbXXcXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','.[X][X](.+)+[X]') /* Result: yi */;
ERROR HY000: Timeout exceeded in regular expression match.
select regexp_like('bbbbXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','.[X](.+)+[X]') /* Result: ni */;
ERROR HY000: Timeout exceeded in regular expression match.
select regexp_like('bbbbXXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','.[X](.+)+[X][X]') /* Result: ni */;
ERROR HY000: Timeout exceeded in regular expression match.
select regexp_like('bbbbXXXaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','.[X][X](.+)+[X]') /* Result: ni */;
ERROR HY000: Timeout exceeded in regular expression match.
