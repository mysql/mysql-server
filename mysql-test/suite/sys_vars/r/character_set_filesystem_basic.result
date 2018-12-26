SET @global_start_value = @@global.character_set_filesystem;
SELECT @global_start_value;
@global_start_value
binary
SET @session_start_value = @@character_set_filesystem;
SELECT @session_start_value;
@session_start_value
binary
SET @session_start_value = @@local.character_set_filesystem;
SELECT @session_start_value;
@session_start_value
binary
SET @session_start_value = @@session.character_set_filesystem;
SELECT @session_start_value;
@session_start_value
binary
'#--------------------FN_DYNVARS_008_01------------------#'
SELECT character_set_filesystem;
ERROR 42S22: Unknown column 'character_set_filesystem' in 'field list'
SET character_set_filesystem=utf8;
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
SELECT @@session.character_set_filesystem;
@@session.character_set_filesystem
utf8
SET global.character_set_filesystem=utf8;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'global.character_set_filesystem=utf8' at line 1
SET session character_set_filesystem=utf8;
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
SELECT session character_set_filesystem;
ERROR 42S22: Unknown column 'session' in 'field list'
SET global character_set_filesystem=utf8;
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
SELECT global character_set_filesystem;
ERROR 42S22: Unknown column 'global' in 'field list'
'#--------------------FN_DYNVARS_008_02-------------------------#'
SET @@character_set_filesystem = latin5;
SET @@character_set_filesystem = DEFAULT;
SELECT @@character_set_filesystem AS DEFAULT_VALUE;
DEFAULT_VALUE
utf8
SET @@global.character_set_filesystem = latin5;
SET @@global.character_set_filesystem = DEFAULT;
SELECT @@global.character_set_filesystem AS DEFAULT_VALUE;
DEFAULT_VALUE
binary
'#--------------------FN_DYNVARS_008_03-------------------------#'
SET @@session.character_set_filesystem = utf8;
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
SELECT @@session.character_set_filesystem;
@@session.character_set_filesystem
utf8
SET @@global.character_set_filesystem = latin2;
SELECT @@global.character_set_filesystem;
@@global.character_set_filesystem
latin2
SELECT @@session.character_set_filesystem AS res_is_utf8;
res_is_utf8
utf8
SET @@session.character_set_filesystem = latin5;
SELECT @@session.character_set_filesystem;
@@session.character_set_filesystem
latin5
SELECT @@global.character_set_filesystem AS res_is_latin2;
res_is_latin2
latin2
SELECT @@global.character_set_filesystem=
@@session.character_set_filesystem AS res_is_false;
res_is_false
0
'#--------------------FN_DYNVARS_008_04-------------------------#'
SELECT @@character_set_filesystem = @@session.character_set_filesystem AS res;
res
1
SELECT @@character_set_filesystem = @@local.character_set_filesystem AS res;
res
1
'#--------------------FN_DYNVARS_008_05-------------------------#'
SET @@character_set_filesystem = utf8 + latin2;
ERROR 42S22: Unknown column 'utf8' in 'field list'
'#--------------------FN_DYNVARS_008_06-------------------------#'
SET @@session.character_set_filesystem = big5;
SELECT @@session.character_set_filesystem;
@@session.character_set_filesystem
big5
SET @@session.character_set_filesystem = dec8;
SELECT @@session.character_set_filesystem;
@@session.character_set_filesystem
dec8
SET @@session.character_set_filesystem = cp850;
SELECT @@session.character_set_filesystem;
@@session.character_set_filesystem
cp850
SET @@session.character_set_filesystem = hp8;
SELECT @@session.character_set_filesystem;
@@session.character_set_filesystem
hp8
SET @@session.character_set_filesystem = koi8r;
SELECT @@session.character_set_filesystem;
@@session.character_set_filesystem
koi8r
SET @@session.character_set_filesystem = latin1;
SELECT @@session.character_set_filesystem;
@@session.character_set_filesystem
latin1
SET @@session.character_set_filesystem = latin2;
SELECT @@session.character_set_filesystem;
@@session.character_set_filesystem
latin2
SET @@session.character_set_filesystem = swe7;
SELECT @@session.character_set_filesystem;
@@session.character_set_filesystem
swe7
SET @@session.character_set_filesystem = ascii;
SELECT @@session.character_set_filesystem;
@@session.character_set_filesystem
ascii
SET @@session.character_set_filesystem = ujis;
SELECT @@session.character_set_filesystem;
@@session.character_set_filesystem
ujis
SET @@session.character_set_filesystem = sjis;
SELECT @@session.character_set_filesystem;
@@session.character_set_filesystem
sjis
SET @@session.character_set_filesystem = hebrew;
SELECT @@session.character_set_filesystem;
@@session.character_set_filesystem
hebrew
SET @@session.character_set_filesystem = tis620;
SELECT @@session.character_set_filesystem;
@@session.character_set_filesystem
tis620
SET @@session.character_set_filesystem = euckr;
SELECT @@session.character_set_filesystem;
@@session.character_set_filesystem
euckr
SET @@session.character_set_filesystem = koi8u;
SELECT @@session.character_set_filesystem;
@@session.character_set_filesystem
koi8u
SET @@session.character_set_filesystem = gb2312;
SELECT @@session.character_set_filesystem;
@@session.character_set_filesystem
gb2312
SET @@session.character_set_filesystem = greek;
SELECT @@session.character_set_filesystem;
@@session.character_set_filesystem
greek
SET @@session.character_set_filesystem = cp1250;
SELECT @@session.character_set_filesystem;
@@session.character_set_filesystem
cp1250
SET @@session.character_set_filesystem = gbk;
SELECT @@session.character_set_filesystem;
@@session.character_set_filesystem
gbk
SET @@session.character_set_filesystem = gb18030;
SELECT @@session.character_set_filesystem;
@@session.character_set_filesystem
gb18030
SET @@session.character_set_filesystem = latin5;
SELECT @@session.character_set_filesystem;
@@session.character_set_filesystem
latin5
SET @@session.character_set_filesystem = armscii8;
SELECT @@session.character_set_filesystem;
@@session.character_set_filesystem
armscii8
SET @@session.character_set_filesystem = utf8;
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
SELECT @@session.character_set_filesystem;
@@session.character_set_filesystem
utf8
SET @@session.character_set_filesystem = utf8mb4;
SELECT @@session.character_set_filesystem;
@@session.character_set_filesystem
utf8mb4
SET @@session.character_set_filesystem = ucs2;
SELECT @@session.character_set_filesystem;
@@session.character_set_filesystem
ucs2
SET @@session.character_set_filesystem = cp866;
SELECT @@session.character_set_filesystem;
@@session.character_set_filesystem
cp866
SET @@session.character_set_filesystem = keybcs2;
SELECT @@session.character_set_filesystem;
@@session.character_set_filesystem
keybcs2
SET @@session.character_set_filesystem = macce;
SELECT @@session.character_set_filesystem;
@@session.character_set_filesystem
macce
SET @@session.character_set_filesystem = macroman;
SELECT @@session.character_set_filesystem;
@@session.character_set_filesystem
macroman
SET @@session.character_set_filesystem = cp852;
SELECT @@session.character_set_filesystem;
@@session.character_set_filesystem
cp852
SET @@session.character_set_filesystem = latin7;
SELECT @@session.character_set_filesystem;
@@session.character_set_filesystem
latin7
SET @@session.character_set_filesystem = cp1251;
SELECT @@session.character_set_filesystem;
@@session.character_set_filesystem
cp1251
SET @@session.character_set_filesystem = cp1256;
SELECT @@session.character_set_filesystem;
@@session.character_set_filesystem
cp1256
SET @@session.character_set_filesystem = cp1257;
SELECT @@session.character_set_filesystem;
@@session.character_set_filesystem
cp1257
SET @@session.character_set_filesystem = binary;
SELECT @@session.character_set_filesystem;
@@session.character_set_filesystem
binary
SET @@session.character_set_filesystem = geostd8;
SELECT @@session.character_set_filesystem;
@@session.character_set_filesystem
geostd8
SET @@session.character_set_filesystem = cp932;
SELECT @@session.character_set_filesystem;
@@session.character_set_filesystem
cp932
SET @@session.character_set_filesystem = eucjpms;
SELECT @@session.character_set_filesystem;
@@session.character_set_filesystem
eucjpms
'#--------------------FN_DYNVARS_008_07-------------------------#'
SET @@global.character_set_filesystem = big5;
SELECT @@global.character_set_filesystem;
@@global.character_set_filesystem
big5
SET @@global.character_set_filesystem = dec8;
SELECT @@global.character_set_filesystem;
@@global.character_set_filesystem
dec8
SET @@global.character_set_filesystem = cp850;
SELECT @@global.character_set_filesystem;
@@global.character_set_filesystem
cp850
SET @@global.character_set_filesystem = hp8;
SELECT @@global.character_set_filesystem;
@@global.character_set_filesystem
hp8
SET @@global.character_set_filesystem = koi8r;
SELECT @@global.character_set_filesystem;
@@global.character_set_filesystem
koi8r
SET @@global.character_set_filesystem = latin1;
SELECT @@global.character_set_filesystem;
@@global.character_set_filesystem
latin1
SET @@global.character_set_filesystem = latin2;
SELECT @@global.character_set_filesystem;
@@global.character_set_filesystem
latin2
SET @@global.character_set_filesystem = swe7;
SELECT @@global.character_set_filesystem;
@@global.character_set_filesystem
swe7
SET @@global.character_set_filesystem = ascii;
SELECT @@global.character_set_filesystem;
@@global.character_set_filesystem
ascii
SET @@global.character_set_filesystem = ujis;
SELECT @@global.character_set_filesystem;
@@global.character_set_filesystem
ujis
SET @@global.character_set_filesystem = sjis;
SELECT @@global.character_set_filesystem;
@@global.character_set_filesystem
sjis
SET @@global.character_set_filesystem = hebrew;
SELECT @@global.character_set_filesystem;
@@global.character_set_filesystem
hebrew
SET @@global.character_set_filesystem = tis620;
SELECT @@global.character_set_filesystem;
@@global.character_set_filesystem
tis620
SET @@global.character_set_filesystem = euckr;
SELECT @@global.character_set_filesystem;
@@global.character_set_filesystem
euckr
SET @@global.character_set_filesystem = koi8u;
SELECT @@global.character_set_filesystem;
@@global.character_set_filesystem
koi8u
SET @@global.character_set_filesystem = gb2312;
SELECT @@global.character_set_filesystem;
@@global.character_set_filesystem
gb2312
SET @@global.character_set_filesystem = greek;
SELECT @@global.character_set_filesystem;
@@global.character_set_filesystem
greek
SET @@global.character_set_filesystem = cp1250;
SELECT @@global.character_set_filesystem;
@@global.character_set_filesystem
cp1250
SET @@global.character_set_filesystem = gbk;
SELECT @@global.character_set_filesystem;
@@global.character_set_filesystem
gbk
SET @@global.character_set_filesystem = gb18030;
SELECT @@global.character_set_filesystem;
@@global.character_set_filesystem
gb18030
SET @@global.character_set_filesystem = latin5;
SELECT @@global.character_set_filesystem;
@@global.character_set_filesystem
latin5
SET @@global.character_set_filesystem = armscii8;
SELECT @@global.character_set_filesystem;
@@global.character_set_filesystem
armscii8
SET @@global.character_set_filesystem = utf8;
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
SELECT @@global.character_set_filesystem;
@@global.character_set_filesystem
utf8
SET @@global.character_set_filesystem = utf8mb4;
SELECT @@global.character_set_filesystem;
@@global.character_set_filesystem
utf8mb4
SET @@global.character_set_filesystem = ucs2;
SELECT @@global.character_set_filesystem;
@@global.character_set_filesystem
ucs2
SET @@global.character_set_filesystem = cp866;
SELECT @@global.character_set_filesystem;
@@global.character_set_filesystem
cp866
SET @@global.character_set_filesystem = keybcs2;
SELECT @@global.character_set_filesystem;
@@global.character_set_filesystem
keybcs2
SET @@global.character_set_filesystem = macce;
SELECT @@global.character_set_filesystem;
@@global.character_set_filesystem
macce
SET @@global.character_set_filesystem = macroman;
SELECT @@global.character_set_filesystem;
@@global.character_set_filesystem
macroman
SET @@global.character_set_filesystem = cp852;
SELECT @@global.character_set_filesystem;
@@global.character_set_filesystem
cp852
SET @@global.character_set_filesystem = latin7;
SELECT @@global.character_set_filesystem;
@@global.character_set_filesystem
latin7
SET @@global.character_set_filesystem = cp1251;
SELECT @@global.character_set_filesystem;
@@global.character_set_filesystem
cp1251
SET @@global.character_set_filesystem = cp1256;
SELECT @@global.character_set_filesystem;
@@global.character_set_filesystem
cp1256
SET @@global.character_set_filesystem = cp1257;
SELECT @@global.character_set_filesystem;
@@global.character_set_filesystem
cp1257
SET @@global.character_set_filesystem = binary;
SELECT @@global.character_set_filesystem;
@@global.character_set_filesystem
binary
SET @@global.character_set_filesystem = geostd8;
SELECT @@global.character_set_filesystem;
@@global.character_set_filesystem
geostd8
SET @@global.character_set_filesystem = cp932;
SELECT @@global.character_set_filesystem;
@@global.character_set_filesystem
cp932
SET @@global.character_set_filesystem = eucjpms;
SELECT @@global.character_set_filesystem;
@@global.character_set_filesystem
eucjpms
'#--------------------FN_DYNVARS_008_08-------------------------#'
SET @@character_set_filesystem = UTF8;
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
SELECT @@character_set_filesystem;
@@character_set_filesystem
utf8
SET @@character_set_filesystem = utf8;
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
SELECT @@character_set_filesystem;
@@character_set_filesystem
utf8
SET @@character_set_filesystem = uTf8;
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
SELECT @@character_set_filesystem;
@@character_set_filesystem
utf8
'#--------------------FN_DYNVARS_008_09-------------------------#'
SET @@character_set_filesystem = 1;
SELECT @@character_set_filesystem;
@@character_set_filesystem
big5
SET @@character_set_filesystem = 2;
SELECT @@character_set_filesystem;
@@character_set_filesystem
latin2
SET @@character_set_filesystem = 3;
SELECT @@character_set_filesystem;
@@character_set_filesystem
dec8
SET @@character_set_filesystem = 36;
SELECT @@character_set_filesystem;
@@character_set_filesystem
cp866
SET @@character_set_filesystem = 99;
SELECT @@character_set_filesystem;
@@character_set_filesystem
cp1250
SET @@character_set_filesystem = 100;
ERROR 42000: Unknown character set: '100'
SET @total_charset = (SELECT count(*) FROM INFORMATION_SCHEMA.CHARACTER_SETS);
SELECT @total_charset;
@total_charset
41
'#--------------------FN_DYNVARS_008_10-------------------------#'
SET @@character_set_filesystem = abc;
ERROR 42000: Unknown character set: 'abc'
SET @@character_set_filesystem = 1utf8;
ERROR 42000: Unknown character set: '1utf8'
SET @@character_set_filesystem = 0;
ERROR 42000: Unknown character set: '0'
SET @@character_set_filesystem = 1.1;
ERROR 42000: Incorrect argument type to variable 'character_set_filesystem'
SET @@character_set_filesystem = -1;
ERROR 42000: Unknown character set: '-1'
SET @@character_set_filesystem = '';
ERROR 42000: Unknown character set: ''
SET @@character_set_filesystem = 'utf';
ERROR 42000: Unknown character set: 'utf'
SET @@character_set_filesystem = true;
SELECT @@character_set_filesystem AS res_with_true;
res_with_true
big5
SET @@character_set_filesystem = ON;
ERROR 42000: Unknown character set: 'ON'
'#--------------------FN_DYNVARS_008_11-------------------------#'
SET @@global.character_set_filesystem = abc;
ERROR 42000: Unknown character set: 'abc'
SET @@global.character_set_filesystem = 1utf8;
ERROR 42000: Unknown character set: '1utf8'
SET @@global.character_set_filesystem = 0;
ERROR 42000: Unknown character set: '0'
SET @@global.character_set_filesystem = 1.1;
ERROR 42000: Incorrect argument type to variable 'character_set_filesystem'
SET @@global.character_set_filesystem = -1;
ERROR 42000: Unknown character set: '-1'
SET @@global.character_set_filesystem = '';
ERROR 42000: Unknown character set: ''
SET @@global.character_set_filesystem = 'utf';
ERROR 42000: Unknown character set: 'utf'
SET @@global.character_set_filesystem = true;
SET @@global.character_set_filesystem = ON;
ERROR 42000: Unknown character set: 'ON'
'#--------------------FN_DYNVARS_008_12-------------------------#'
SELECT @@global.character_set_filesystem =
(SELECT VARIABLE_VALUE FROM performance_schema.global_variables
WHERE VARIABLE_NAME='character_set_filesystem') AS res;
res
1
SET @@global.character_set_filesystem = 1;
SELECT @@global.character_set_filesystem;
@@global.character_set_filesystem
big5
SELECT @@global.character_set_filesystem =
(SELECT VARIABLE_VALUE FROM performance_schema.global_variables
WHERE VARIABLE_NAME='character_set_filesystem') AS res;
res
1
'#--------------------FN_DYNVARS_008_13-------------------------#'
SELECT @@character_set_filesystem =
(SELECT VARIABLE_VALUE FROM performance_schema.session_variables
WHERE VARIABLE_NAME='character_set_filesystem') AS res;
res
1
SELECT @@local.character_set_filesystem =
(SELECT VARIABLE_VALUE FROM performance_schema.session_variables
WHERE VARIABLE_NAME='character_set_filesystem') AS res;
res
1
SELECT @@session.character_set_filesystem =
(SELECT VARIABLE_VALUE FROM performance_schema.session_variables
WHERE VARIABLE_NAME='character_set_filesystem') AS res;
res
1
SET @@global.character_set_filesystem = @global_start_value;
SELECT @@global.character_set_filesystem;
@@global.character_set_filesystem
binary
SET @@session.character_set_filesystem = @session_start_value;
SELECT @@session.character_set_filesystem;
@@session.character_set_filesystem
binary
