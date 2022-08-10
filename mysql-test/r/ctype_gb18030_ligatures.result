SET NAMES utf8mb4;
SET collation_connection='gb18030_unicode_520_ci';
CREATE TABLE t1(C CHAR(10) CHARACTER SET gb18030 COLLATE gb18030_unicode_520_ci, UNIQUE KEY(c));
INSERT INTO t1 VALUES(CONVERT(_ucs2 0x0149 USING gb18030));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONVERT(_ucs2 0x01F0 USING gb18030));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONVERT(_ucs2 0x0390 USING gb18030));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONVERT(_ucs2 0x03B0 USING gb18030));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONVERT(_ucs2 0x0587 USING gb18030));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONVERT(_ucs2 0x1E96 USING gb18030));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONVERT(_ucs2 0x1E97 USING gb18030));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONVERT(_ucs2 0x1E98 USING gb18030));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONVERT(_ucs2 0x1E99 USING gb18030));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONVERT(_ucs2 0x1E9A USING gb18030));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONVERT(_ucs2 0x1FB2 USING gb18030));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONVERT(_ucs2 0x1FC2 USING gb18030));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONVERT(_ucs2 0x1FE4 USING gb18030));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONVERT(_ucs2 0x1FF2 USING gb18030));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONVERT(_ucs2 0xFB00 USING gb18030));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONVERT(_ucs2 0xFB01 USING gb18030));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONVERT(_ucs2 0xFB02 USING gb18030));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONVERT(_ucs2 0xFB03 USING gb18030));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONVERT(_ucs2 0xFB04 USING gb18030));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONVERT(_ucs2 0xFB05 USING gb18030));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONVERT(_ucs2 0xFB13 USING gb18030));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONVERT(_ucs2 0xFB14 USING gb18030));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONVERT(_ucs2 0xFB15 USING gb18030));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONVERT(_ucs2 0xFB16 USING gb18030));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONVERT(_ucs2 0xFB17 USING gb18030));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONVERT(_ucs2 0x00DF USING gb18030));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
SELECT * FROM t1;
C
ß
ŉ
ǰ
ΐ
ΰ
և
ẖ
ẗ
ẘ
ẙ
ẚ
ᾲ
ῂ
ῤ
ῲ
ﬀ
ﬁ
ﬂ
ﬃ
ﬄ
ﬅ
ﬓ
ﬔ
ﬕ
ﬖ
ﬗ
SELECT COUNT(*) FROM t1;
COUNT(*)
26
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x02BC USING gb18030), CONVERT(_ucs2 0x004E USING gb18030)));
ERROR 23000: Duplicate entry 'ʼN' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x02BC USING gb18030), CONVERT(_ucs2 0x006E USING gb18030)));
ERROR 23000: Duplicate entry 'ʼn' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x004A USING gb18030), CONVERT(_ucs2 0x030C USING gb18030)));
ERROR 23000: Duplicate entry 'J̌' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x006A USING gb18030), CONVERT(_ucs2 0x030C USING gb18030)));
ERROR 23000: Duplicate entry 'ǰ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0399 USING gb18030), CONVERT(_ucs2 0x0308 USING gb18030), CONVERT(_ucs2 0x0301 USING gb18030)));
ERROR 23000: Duplicate entry 'Ϊ́' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x03B9 USING gb18030), CONVERT(_ucs2 0x0308 USING gb18030), CONVERT(_ucs2 0x0301 USING gb18030)));
ERROR 23000: Duplicate entry 'ΐ' for key 't1.C'
INSERT INTO t1 VALUES(CONVERT(_ucs2 0x1FD3 USING gb18030));
ERROR 23000: Duplicate entry 'ΐ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x03A5 USING gb18030), CONVERT(_ucs2 0x0308 USING gb18030), CONVERT(_ucs2 0x0301 USING gb18030)));
ERROR 23000: Duplicate entry 'Ϋ́' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x03C5 USING gb18030), CONVERT(_ucs2 0x0308 USING gb18030), CONVERT(_ucs2 0x0301 USING gb18030)));
ERROR 23000: Duplicate entry 'ΰ' for key 't1.C'
INSERT INTO t1 VALUES(CONVERT(_ucs2 0x1FE3 USING gb18030));
ERROR 23000: Duplicate entry 'ΰ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0535 USING gb18030), CONVERT(_ucs2 0x0552 USING gb18030)));
ERROR 23000: Duplicate entry 'ԵՒ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0565 USING gb18030), CONVERT(_ucs2 0x0552 USING gb18030)));
ERROR 23000: Duplicate entry 'եՒ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0535 USING gb18030), CONVERT(_ucs2 0x0582 USING gb18030)));
ERROR 23000: Duplicate entry 'Եւ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0565 USING gb18030), CONVERT(_ucs2 0x0582 USING gb18030)));
ERROR 23000: Duplicate entry 'եւ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0048 USING gb18030), CONVERT(_ucs2 0x0331 USING gb18030)));
ERROR 23000: Duplicate entry 'H̱' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0068 USING gb18030), CONVERT(_ucs2 0x0331 USING gb18030)));
ERROR 23000: Duplicate entry 'ẖ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0054 USING gb18030), CONVERT(_ucs2 0x0308 USING gb18030)));
ERROR 23000: Duplicate entry 'T̈' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0074 USING gb18030), CONVERT(_ucs2 0x0308 USING gb18030)));
ERROR 23000: Duplicate entry 'ẗ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0057 USING gb18030), CONVERT(_ucs2 0x030A USING gb18030)));
ERROR 23000: Duplicate entry 'W̊' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0077 USING gb18030), CONVERT(_ucs2 0x030A USING gb18030)));
ERROR 23000: Duplicate entry 'ẘ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0059 USING gb18030), CONVERT(_ucs2 0x030A USING gb18030)));
ERROR 23000: Duplicate entry 'Y̊' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0079 USING gb18030), CONVERT(_ucs2 0x030A USING gb18030)));
ERROR 23000: Duplicate entry 'ẙ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0041 USING gb18030), CONVERT(_ucs2 0x02BE USING gb18030)));
ERROR 23000: Duplicate entry 'Aʾ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0061 USING gb18030), CONVERT(_ucs2 0x02BE USING gb18030)));
ERROR 23000: Duplicate entry 'aʾ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x03A5 USING gb18030), CONVERT(_ucs2 0x0313 USING gb18030)));
ERROR 23000: Duplicate entry 'Υ̓' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x03C5 USING gb18030), CONVERT(_ucs2 0x0313 USING gb18030)));
ERROR 23000: Duplicate entry 'ὐ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x03A5 USING gb18030), CONVERT(_ucs2 0x0313 USING gb18030), CONVERT(_ucs2 0x0300 USING gb18030)));
ERROR 23000: Duplicate entry 'Υ̓̀' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x03C5 USING gb18030), CONVERT(_ucs2 0x0313 USING gb18030), CONVERT(_ucs2 0x0300 USING gb18030)));
ERROR 23000: Duplicate entry 'ὒ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x1F50 USING gb18030), CONVERT(_ucs2 0x0300 USING gb18030)));
ERROR 23000: Duplicate entry 'ὒ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x03A5 USING gb18030), CONVERT(_ucs2 0x0313 USING gb18030), CONVERT(_ucs2 0x0301 USING gb18030)));
ERROR 23000: Duplicate entry 'Υ̓́' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x03C5 USING gb18030), CONVERT(_ucs2 0x0313 USING gb18030), CONVERT(_ucs2 0x0301 USING gb18030)));
ERROR 23000: Duplicate entry 'ὔ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x1F50 USING gb18030), CONVERT(_ucs2 0x0301 USING gb18030)));
ERROR 23000: Duplicate entry 'ὔ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x03A5 USING gb18030), CONVERT(_ucs2 0x0313 USING gb18030), CONVERT(_ucs2 0x0342 USING gb18030)));
ERROR 23000: Duplicate entry 'Υ̓͂' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x03C5 USING gb18030), CONVERT(_ucs2 0x0313 USING gb18030), CONVERT(_ucs2 0x0342 USING gb18030)));
ERROR 23000: Duplicate entry 'ὖ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x1F50 USING gb18030), CONVERT(_ucs2 0x0342 USING gb18030)));
ERROR 23000: Duplicate entry 'ὖ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0399 USING gb18030), CONVERT(_ucs2 0x0308 USING gb18030), CONVERT(_ucs2 0x0300 USING gb18030)));
ERROR 23000: Duplicate entry 'Ϊ̀' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x03B9 USING gb18030), CONVERT(_ucs2 0x0308 USING gb18030), CONVERT(_ucs2 0x0300 USING gb18030)));
ERROR 23000: Duplicate entry 'ῒ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0399 USING gb18030), CONVERT(_ucs2 0x0308 USING gb18030), CONVERT(_ucs2 0x0301 USING gb18030)));
ERROR 23000: Duplicate entry 'Ϊ́' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x03B9 USING gb18030), CONVERT(_ucs2 0x0308 USING gb18030), CONVERT(_ucs2 0x0301 USING gb18030)));
ERROR 23000: Duplicate entry 'ΐ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0399 USING gb18030), CONVERT(_ucs2 0x0342 USING gb18030)));
ERROR 23000: Duplicate entry 'Ι͂' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x03B9 USING gb18030), CONVERT(_ucs2 0x0342 USING gb18030)));
ERROR 23000: Duplicate entry 'ῖ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0399 USING gb18030), CONVERT(_ucs2 0x0308 USING gb18030), CONVERT(_ucs2 0x0342 USING gb18030)));
ERROR 23000: Duplicate entry 'Ϊ͂' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x03B9 USING gb18030), CONVERT(_ucs2 0x0308 USING gb18030), CONVERT(_ucs2 0x0342 USING gb18030)));
ERROR 23000: Duplicate entry 'ῗ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x03A5 USING gb18030), CONVERT(_ucs2 0x0308 USING gb18030), CONVERT(_ucs2 0x0300 USING gb18030)));
ERROR 23000: Duplicate entry 'Ϋ̀' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x03C5 USING gb18030), CONVERT(_ucs2 0x0308 USING gb18030), CONVERT(_ucs2 0x0300 USING gb18030)));
ERROR 23000: Duplicate entry 'ῢ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x03A5 USING gb18030), CONVERT(_ucs2 0x0308 USING gb18030), CONVERT(_ucs2 0x0301 USING gb18030)));
ERROR 23000: Duplicate entry 'Ϋ́' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x03C5 USING gb18030), CONVERT(_ucs2 0x0308 USING gb18030), CONVERT(_ucs2 0x0301 USING gb18030)));
ERROR 23000: Duplicate entry 'ΰ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x03A1 USING gb18030), CONVERT(_ucs2 0x0313 USING gb18030)));
ERROR 23000: Duplicate entry 'Ρ̓' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x03C1 USING gb18030), CONVERT(_ucs2 0x0313 USING gb18030)));
ERROR 23000: Duplicate entry 'ῤ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x03A5 USING gb18030), CONVERT(_ucs2 0x0342 USING gb18030)));
ERROR 23000: Duplicate entry 'Υ͂' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x03C5 USING gb18030), CONVERT(_ucs2 0x0342 USING gb18030)));
ERROR 23000: Duplicate entry 'ῦ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x03A5 USING gb18030), CONVERT(_ucs2 0x0308 USING gb18030), CONVERT(_ucs2 0x0342 USING gb18030)));
ERROR 23000: Duplicate entry 'Ϋ͂' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x03C5 USING gb18030), CONVERT(_ucs2 0x0308 USING gb18030), CONVERT(_ucs2 0x0342 USING gb18030)));
ERROR 23000: Duplicate entry 'ῧ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0046 USING gb18030), CONVERT(_ucs2 0x0046 USING gb18030)));
ERROR 23000: Duplicate entry 'FF' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0046 USING gb18030), CONVERT(_ucs2 0x0066 USING gb18030)));
ERROR 23000: Duplicate entry 'Ff' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0066 USING gb18030), CONVERT(_ucs2 0x0046 USING gb18030)));
ERROR 23000: Duplicate entry 'fF' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0066 USING gb18030), CONVERT(_ucs2 0x0066 USING gb18030)));
ERROR 23000: Duplicate entry 'ff' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0046 USING gb18030), CONVERT(_ucs2 0x0049 USING gb18030)));
ERROR 23000: Duplicate entry 'FI' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0046 USING gb18030), CONVERT(_ucs2 0x0069 USING gb18030)));
ERROR 23000: Duplicate entry 'Fi' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0066 USING gb18030), CONVERT(_ucs2 0x0049 USING gb18030)));
ERROR 23000: Duplicate entry 'fI' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0066 USING gb18030), CONVERT(_ucs2 0x0069 USING gb18030)));
ERROR 23000: Duplicate entry 'fi' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0046 USING gb18030), CONVERT(_ucs2 0x004C USING gb18030)));
ERROR 23000: Duplicate entry 'FL' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0046 USING gb18030), CONVERT(_ucs2 0x006C USING gb18030)));
ERROR 23000: Duplicate entry 'Fl' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0066 USING gb18030), CONVERT(_ucs2 0x004C USING gb18030)));
ERROR 23000: Duplicate entry 'fL' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0066 USING gb18030), CONVERT(_ucs2 0x006C USING gb18030)));
ERROR 23000: Duplicate entry 'fl' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0046 USING gb18030), CONVERT(_ucs2 0x0046 USING gb18030), CONVERT(_ucs2 0x0049 USING gb18030)));
ERROR 23000: Duplicate entry 'FFI' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0046 USING gb18030), CONVERT(_ucs2 0x0066 USING gb18030), CONVERT(_ucs2 0x0049 USING gb18030)));
ERROR 23000: Duplicate entry 'FfI' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0066 USING gb18030), CONVERT(_ucs2 0x0046 USING gb18030), CONVERT(_ucs2 0x0049 USING gb18030)));
ERROR 23000: Duplicate entry 'fFI' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0066 USING gb18030), CONVERT(_ucs2 0x0066 USING gb18030), CONVERT(_ucs2 0x0049 USING gb18030)));
ERROR 23000: Duplicate entry 'ffI' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0xFB00 USING gb18030), CONVERT(_ucs2 0x0049 USING gb18030)));
ERROR 23000: Duplicate entry 'ﬀI' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0046 USING gb18030), CONVERT(_ucs2 0x0046 USING gb18030), CONVERT(_ucs2 0x0069 USING gb18030)));
ERROR 23000: Duplicate entry 'FFi' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0046 USING gb18030), CONVERT(_ucs2 0x0066 USING gb18030), CONVERT(_ucs2 0x0069 USING gb18030)));
ERROR 23000: Duplicate entry 'Ffi' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0066 USING gb18030), CONVERT(_ucs2 0x0046 USING gb18030), CONVERT(_ucs2 0x0069 USING gb18030)));
ERROR 23000: Duplicate entry 'fFi' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0066 USING gb18030), CONVERT(_ucs2 0x0066 USING gb18030), CONVERT(_ucs2 0x0069 USING gb18030)));
ERROR 23000: Duplicate entry 'ffi' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0xFB00 USING gb18030), CONVERT(_ucs2 0x0069 USING gb18030)));
ERROR 23000: Duplicate entry 'ﬀi' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0046 USING gb18030), CONVERT(_ucs2 0xFB01 USING gb18030)));
ERROR 23000: Duplicate entry 'Fﬁ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0066 USING gb18030), CONVERT(_ucs2 0xFB01 USING gb18030)));
ERROR 23000: Duplicate entry 'fﬁ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0046 USING gb18030), CONVERT(_ucs2 0x0046 USING gb18030), CONVERT(_ucs2 0x004C USING gb18030)));
ERROR 23000: Duplicate entry 'FFL' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0046 USING gb18030), CONVERT(_ucs2 0x0066 USING gb18030), CONVERT(_ucs2 0x004C USING gb18030)));
ERROR 23000: Duplicate entry 'FfL' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0066 USING gb18030), CONVERT(_ucs2 0x0046 USING gb18030), CONVERT(_ucs2 0x004C USING gb18030)));
ERROR 23000: Duplicate entry 'fFL' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0066 USING gb18030), CONVERT(_ucs2 0x0066 USING gb18030), CONVERT(_ucs2 0x004C USING gb18030)));
ERROR 23000: Duplicate entry 'ffL' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0xFB00 USING gb18030), CONVERT(_ucs2 0x004C USING gb18030)));
ERROR 23000: Duplicate entry 'ﬀL' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0046 USING gb18030), CONVERT(_ucs2 0x0046 USING gb18030), CONVERT(_ucs2 0x006C USING gb18030)));
ERROR 23000: Duplicate entry 'FFl' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0046 USING gb18030), CONVERT(_ucs2 0x0066 USING gb18030), CONVERT(_ucs2 0x006C USING gb18030)));
ERROR 23000: Duplicate entry 'Ffl' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0066 USING gb18030), CONVERT(_ucs2 0x0046 USING gb18030), CONVERT(_ucs2 0x006C USING gb18030)));
ERROR 23000: Duplicate entry 'fFl' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0066 USING gb18030), CONVERT(_ucs2 0x0066 USING gb18030), CONVERT(_ucs2 0x006C USING gb18030)));
ERROR 23000: Duplicate entry 'ffl' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0xFB00 USING gb18030), CONVERT(_ucs2 0x006C USING gb18030)));
ERROR 23000: Duplicate entry 'ﬀl' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0046 USING gb18030), CONVERT(_ucs2 0xFB02 USING gb18030)));
ERROR 23000: Duplicate entry 'Fﬂ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0066 USING gb18030), CONVERT(_ucs2 0xFB02 USING gb18030)));
ERROR 23000: Duplicate entry 'fﬂ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0053 USING gb18030), CONVERT(_ucs2 0x0054 USING gb18030)));
ERROR 23000: Duplicate entry 'ST' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0053 USING gb18030), CONVERT(_ucs2 0x0074 USING gb18030)));
ERROR 23000: Duplicate entry 'St' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0073 USING gb18030), CONVERT(_ucs2 0x0054 USING gb18030)));
ERROR 23000: Duplicate entry 'sT' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0073 USING gb18030), CONVERT(_ucs2 0x0074 USING gb18030)));
ERROR 23000: Duplicate entry 'st' for key 't1.C'
INSERT INTO t1 VALUES(CONVERT(_ucs2 0xFB06 USING gb18030));
ERROR 23000: Duplicate entry 'ﬆ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0544 USING gb18030), CONVERT(_ucs2 0x0546 USING gb18030)));
ERROR 23000: Duplicate entry 'ՄՆ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0544 USING gb18030), CONVERT(_ucs2 0x0576 USING gb18030)));
ERROR 23000: Duplicate entry 'Մն' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0574 USING gb18030), CONVERT(_ucs2 0x0546 USING gb18030)));
ERROR 23000: Duplicate entry 'մՆ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0574 USING gb18030), CONVERT(_ucs2 0x0576 USING gb18030)));
ERROR 23000: Duplicate entry 'մն' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0544 USING gb18030), CONVERT(_ucs2 0x0535 USING gb18030)));
ERROR 23000: Duplicate entry 'ՄԵ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0544 USING gb18030), CONVERT(_ucs2 0x0565 USING gb18030)));
ERROR 23000: Duplicate entry 'Մե' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0574 USING gb18030), CONVERT(_ucs2 0x0535 USING gb18030)));
ERROR 23000: Duplicate entry 'մԵ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0574 USING gb18030), CONVERT(_ucs2 0x0565 USING gb18030)));
ERROR 23000: Duplicate entry 'մե' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0544 USING gb18030), CONVERT(_ucs2 0x053B USING gb18030)));
ERROR 23000: Duplicate entry 'ՄԻ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0544 USING gb18030), CONVERT(_ucs2 0x056B USING gb18030)));
ERROR 23000: Duplicate entry 'Մի' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0574 USING gb18030), CONVERT(_ucs2 0x053B USING gb18030)));
ERROR 23000: Duplicate entry 'մԻ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0574 USING gb18030), CONVERT(_ucs2 0x056B USING gb18030)));
ERROR 23000: Duplicate entry 'մի' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x054E USING gb18030), CONVERT(_ucs2 0x0546 USING gb18030)));
ERROR 23000: Duplicate entry 'ՎՆ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x054E USING gb18030), CONVERT(_ucs2 0x0576 USING gb18030)));
ERROR 23000: Duplicate entry 'Վն' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x057E USING gb18030), CONVERT(_ucs2 0x0546 USING gb18030)));
ERROR 23000: Duplicate entry 'վՆ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x057E USING gb18030), CONVERT(_ucs2 0x0576 USING gb18030)));
ERROR 23000: Duplicate entry 'վն' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0544 USING gb18030), CONVERT(_ucs2 0x053D USING gb18030)));
ERROR 23000: Duplicate entry 'ՄԽ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0544 USING gb18030), CONVERT(_ucs2 0x056D USING gb18030)));
ERROR 23000: Duplicate entry 'Մխ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0574 USING gb18030), CONVERT(_ucs2 0x053D USING gb18030)));
ERROR 23000: Duplicate entry 'մԽ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0574 USING gb18030), CONVERT(_ucs2 0x056D USING gb18030)));
ERROR 23000: Duplicate entry 'մխ' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0053 USING gb18030), CONVERT(_ucs2 0x0053 USING gb18030)));
ERROR 23000: Duplicate entry 'SS' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0053 USING gb18030), CONVERT(_ucs2 0x0073 USING gb18030)));
ERROR 23000: Duplicate entry 'Ss' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0073 USING gb18030), CONVERT(_ucs2 0x0053 USING gb18030)));
ERROR 23000: Duplicate entry 'sS' for key 't1.C'
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0073 USING gb18030), CONVERT(_ucs2 0x0073 USING gb18030)));
ERROR 23000: Duplicate entry 'ss' for key 't1.C'
SELECT COUNT(*) FROM t1;
COUNT(*)
26
DROP TABLE t1;
CREATE TABLE t1 (c1 CHAR(20), c2 CHAR(20)) CHARACTER SET gb18030 COLLATE gb18030_unicode_520_ci;
INSERT INTO t1 VALUES(CONVERT(_ucs2 0x005300DF00DF00DF00DF00DF00DF USING gb18030), CONVERT(_ucs2 0x00DF00DF00DF00DF00DF00DF0053 USING gb18030));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
SELECT * FROM t1 WHERE c1 = c2;
c1	c2
Sßßßßßß	ßßßßßßS
DROP TABLE t1;
CREATE TABLE t1 (c CHAR(10), KEY(c)) CHARACTER SET gb18030 COLLATE gb18030_unicode_520_ci;
INSERT INTO t1 VALUES(CONVERT(_ucs2 0xFB00 USING gb18030));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONVERT(_ucs2 0xFB01 USING gb18030));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONVERT(_ucs2 0xFB02 USING gb18030));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONVERT(_ucs2 0xFB03 USING gb18030));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONVERT(_ucs2 0xFB04 USING gb18030));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0066 USING gb18030), CONVERT(_ucs2 0x0046 USING gb18030)));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0066 USING gb18030), CONVERT(_ucs2 0x0066 USING gb18030)));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0046 USING gb18030), CONVERT(_ucs2 0x0069 USING gb18030)));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0066 USING gb18030), CONVERT(_ucs2 0x0049 USING gb18030)));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0066 USING gb18030), CONVERT(_ucs2 0x0066 USING gb18030), CONVERT(_ucs2 0x0069 USING gb18030)));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0xFB00 USING gb18030), CONVERT(_ucs2 0x0069 USING gb18030)));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0046 USING gb18030), CONVERT(_ucs2 0xFB01 USING gb18030)));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0066 USING gb18030), CONVERT(_ucs2 0xFB01 USING gb18030)));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0046 USING gb18030), CONVERT(_ucs2 0x0046 USING gb18030), CONVERT(_ucs2 0x0049 USING gb18030)));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0046 USING gb18030), CONVERT(_ucs2 0x0066 USING gb18030), CONVERT(_ucs2 0x0049 USING gb18030)));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0066 USING gb18030), CONVERT(_ucs2 0x0066 USING gb18030), CONVERT(_ucs2 0x006C USING gb18030)));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0xFB00 USING gb18030), CONVERT(_ucs2 0x006C USING gb18030)));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0046 USING gb18030), CONVERT(_ucs2 0xFB02 USING gb18030)));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0046 USING gb18030), CONVERT(_ucs2 0x0046 USING gb18030), CONVERT(_ucs2 0x004C USING gb18030)));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
INSERT INTO t1 VALUES(CONCAT(CONVERT(_ucs2 0x0046 USING gb18030), CONVERT(_ucs2 0x0066 USING gb18030), CONVERT(_ucs2 0x004C USING gb18030)));
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
SELECT c, hex(c) FROM t1 ORDER BY c, hex(c) ASC;
c	hex(c)
fF	6646
ff	6666
ﬀ	8430B232
FFI	464649
FfI	466649
Fﬁ	468430B233
ffi	666669
fﬁ	668430B233
ﬀi	8430B23269
ﬃ	8430B235
FFL	46464C
FfL	46664C
Fﬂ	468430B234
ffl	66666C
ﬀl	8430B2326C
ﬄ	8430B236
Fi	4669
fI	6649
ﬁ	8430B233
ﬂ	8430B234
SELECT c, hex(c) FROM t1 ORDER BY c, hex(c) DESC;
c	hex(c)
ﬀ	8430B232
ff	6666
fF	6646
ﬃ	8430B235
ﬀi	8430B23269
fﬁ	668430B233
ffi	666669
Fﬁ	468430B233
FfI	466649
FFI	464649
ﬄ	8430B236
ﬀl	8430B2326C
ffl	66666C
Fﬂ	468430B234
FfL	46664C
FFL	46464C
ﬁ	8430B233
fI	6649
Fi	4669
ﬂ	8430B234
SELECT c, hex(c) FROM t1 WHERE c = 'FFI';
c	hex(c)
FFI	464649
FfI	466649
Fﬁ	468430B233
ffi	666669
fﬁ	668430B233
ﬀi	8430B23269
ﬃ	8430B235
SELECT c, hex(c) FROM t1 WHERE c > 'FFI';
c	hex(c)
FFL	46464C
FfL	46664C
Fi	4669
Fﬂ	468430B234
fI	6649
ffl	66666C
ﬀl	8430B2326C
ﬁ	8430B233
ﬂ	8430B234
ﬄ	8430B236
SELECT c, hex(c) FROM t1 WHERE c > 'FF';
c	hex(c)
FFI	464649
FFL	46464C
FfI	466649
FfL	46664C
Fi	4669
Fﬁ	468430B233
Fﬂ	468430B234
fI	6649
ffi	666669
ffl	66666C
fﬁ	668430B233
ﬀi	8430B23269
ﬀl	8430B2326C
ﬁ	8430B233
ﬂ	8430B234
ﬃ	8430B235
ﬄ	8430B236
SELECT c, hex(c) FROM t1 WHERE c <= 'FFL' AND c > 'FF';
c	hex(c)
FFI	464649
FFL	46464C
FfI	466649
FfL	46664C
Fﬁ	468430B233
Fﬂ	468430B234
ffi	666669
ffl	66666C
fﬁ	668430B233
ﬀi	8430B23269
ﬀl	8430B2326C
ﬃ	8430B235
ﬄ	8430B236
SELECT c, hex(c) FROM t1 WHERE c LIKE 'FF%';
c	hex(c)
FFI	464649
FFL	46464C
FfI	466649
FfL	46664C
fF	6646
ff	6666
ffi	666669
ffl	66666C
SELECT c, hex(c) FROM t1 WHERE c LIKE '%L';
c	hex(c)
FFL	46464C
FfL	46664C
ffl	66666C
ﬀl	8430B2326C
DROP TABLE t1;
