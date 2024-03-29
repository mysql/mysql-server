SET @global_start_value = @@global.collation_connection;
SELECT @global_start_value;
@global_start_value
utf8mb4_0900_ai_ci
SET @session_start_value = @@collation_connection;
SELECT @session_start_value;
@session_start_value
utf8mb4_0900_ai_ci
SET @session_start_value = @@local.collation_connection;
SELECT @session_start_value;
@session_start_value
utf8mb4_0900_ai_ci
SET @session_start_value = @@session.collation_connection;
SELECT @session_start_value;
@session_start_value
utf8mb4_0900_ai_ci
'#--------------------FN_DYNVARS_015_01------------------#'
SELECT collation_connection;
ERROR 42S22: Unknown column 'collation_connection' in 'field list'
SET collation_connection=utf8_unicode_ci;
Warnings:
Warning	3778	'utf8mb3_unicode_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
utf8mb3_unicode_ci
SET global.collation_connection=utf8_unicode_ci;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'global.collation_connection=utf8_unicode_ci' at line 1
SET session collation_connection=utf8_unicode_ci;
Warnings:
Warning	3778	'utf8mb3_unicode_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT session collation_connection;
ERROR 42S22: Unknown column 'session' in 'field list'
SET global collation_connection=utf8_unicode_ci;
Warnings:
Warning	3778	'utf8mb3_unicode_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT global collation_connection;
ERROR 42S22: Unknown column 'global' in 'field list'
'#--------------------FN_DYNVARS_015_02-------------------------#'
SET @@collation_connection = latin1_bin;
SET @@collation_connection = DEFAULT;
SELECT @@collation_connection AS DEFAULT_VALUE;
DEFAULT_VALUE
utf8mb3_unicode_ci
SET @@global.collation_connection = latin1_bin;
SET @@global.collation_connection = DEFAULT;
SELECT @@global.collation_connection;
@@global.collation_connection
utf8mb4_0900_ai_ci
'#--------------------FN_DYNVARS_015_03-------------------------#'
SET @@session.collation_connection = utf8_polish_ci;
Warnings:
Warning	3778	'utf8mb3_polish_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
utf8mb3_polish_ci
SET @@global.collation_connection = latin7_general_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
latin7_general_ci
SELECT @@session.collation_connection AS res_is_utf8_polish_ci;
res_is_utf8_polish_ci
utf8mb3_polish_ci
SET @@session.collation_connection = latin7_bin;
SELECT @@session.collation_connection;
@@session.collation_connection
latin7_bin
SELECT @@global.collation_connection AS res_is_latin7_general_ci;
res_is_latin7_general_ci
latin7_general_ci
SELECT @@global.collation_connection=@@session.collation_connection 
AS res_is_false;
res_is_false
0
'#--------------------FN_DYNVARS_015_04-------------------------#'
SELECT @@collation_connection = @@session.collation_connection AS res;
res
1
SELECT @@collation_connection = @@local.collation_connection AS res;
res
1
'#--------------------FN_DYNVARS_015_05-------------------------#'
SET @@collation_connection = latin7_general_ci + latin7_general_cs;
ERROR 42S22: Unknown column 'latin7_general_ci' in 'field list'
'#--------------------FN_DYNVARS_015_06-------------------------#'
SET @@session.collation_connection = big5_chinese_ci;
SELECT @@session.collation_connection;
@@session.collation_connection
big5_chinese_ci
SET @@session.collation_connection = big5_bin;
SELECT @@session.collation_connection;
@@session.collation_connection
big5_bin
SET @@session.collation_connection = dec8_swedish_ci;
Warnings:
Warning	4079	'dec8_swedish_ci' is a collation of the deprecated character set dec8. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
dec8_swedish_ci
SET @@session.collation_connection = dec8_bin;
Warnings:
Warning	4079	'dec8_bin' is a collation of the deprecated character set dec8. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
dec8_bin
SET @@session.collation_connection = cp850_general_ci;
SELECT @@session.collation_connection;
@@session.collation_connection
cp850_general_ci
SET @@session.collation_connection = cp850_bin;
SELECT @@session.collation_connection;
@@session.collation_connection
cp850_bin
SET @@session.collation_connection = hp8_english_ci;
Warnings:
Warning	4079	'hp8_english_ci' is a collation of the deprecated character set hp8. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
hp8_english_ci
SET @@session.collation_connection = hp8_bin;
Warnings:
Warning	4079	'hp8_bin' is a collation of the deprecated character set hp8. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
hp8_bin
SET @@session.collation_connection = koi8r_general_ci;
SELECT @@session.collation_connection;
@@session.collation_connection
koi8r_general_ci
SET @@session.collation_connection = koi8r_bin;
SELECT @@session.collation_connection;
@@session.collation_connection
koi8r_bin
SET @@session.collation_connection = latin1_german1_ci;
SELECT @@session.collation_connection;
@@session.collation_connection
latin1_german1_ci
SET @@session.collation_connection = latin1_swedish_ci;
SELECT @@session.collation_connection;
@@session.collation_connection
latin1_swedish_ci
SET @@session.collation_connection = latin1_danish_ci;
SELECT @@session.collation_connection;
@@session.collation_connection
latin1_danish_ci
SET @@session.collation_connection = latin1_german2_ci;
SELECT @@session.collation_connection;
@@session.collation_connection
latin1_german2_ci
SET @@session.collation_connection = latin1_bin;
SELECT @@session.collation_connection;
@@session.collation_connection
latin1_bin
SET @@session.collation_connection = latin1_general_ci;
SELECT @@session.collation_connection;
@@session.collation_connection
latin1_general_ci
SET @@session.collation_connection = latin1_general_cs;
SELECT @@session.collation_connection;
@@session.collation_connection
latin1_general_cs
SET @@session.collation_connection = latin1_spanish_ci;
SELECT @@session.collation_connection;
@@session.collation_connection
latin1_spanish_ci
SET @@session.collation_connection = latin2_czech_cs;
SELECT @@session.collation_connection;
@@session.collation_connection
latin2_czech_cs
SET @@session.collation_connection = latin2_general_ci;
SELECT @@session.collation_connection;
@@session.collation_connection
latin2_general_ci
SET @@session.collation_connection = latin2_hungarian_ci;
SELECT @@session.collation_connection;
@@session.collation_connection
latin2_hungarian_ci
SET @@session.collation_connection = latin2_croatian_ci;
SELECT @@session.collation_connection;
@@session.collation_connection
latin2_croatian_ci
SET @@session.collation_connection = latin2_bin;
SELECT @@session.collation_connection;
@@session.collation_connection
latin2_bin
SET @@session.collation_connection = swe7_swedish_ci;
SELECT @@session.collation_connection;
@@session.collation_connection
swe7_swedish_ci
SET @@session.collation_connection = swe7_bin;
SELECT @@session.collation_connection;
@@session.collation_connection
swe7_bin
SET @@session.collation_connection = ascii_general_ci;
SELECT @@session.collation_connection;
@@session.collation_connection
ascii_general_ci
SET @@session.collation_connection = ascii_bin;
SELECT @@session.collation_connection;
@@session.collation_connection
ascii_bin
SET @@session.collation_connection = ujis_japanese_ci;
SELECT @@session.collation_connection;
@@session.collation_connection
ujis_japanese_ci
SET @@session.collation_connection = ujis_bin;
SELECT @@session.collation_connection;
@@session.collation_connection
ujis_bin
SET @@session.collation_connection = sjis_japanese_ci;
SELECT @@session.collation_connection;
@@session.collation_connection
sjis_japanese_ci
SET @@session.collation_connection = sjis_bin;
SELECT @@session.collation_connection;
@@session.collation_connection
sjis_bin
SET @@session.collation_connection = hebrew_general_ci;
SELECT @@session.collation_connection;
@@session.collation_connection
hebrew_general_ci
SET @@session.collation_connection = hebrew_bin;
SELECT @@session.collation_connection;
@@session.collation_connection
hebrew_bin
SET @@session.collation_connection = tis620_thai_ci;
SELECT @@session.collation_connection;
@@session.collation_connection
tis620_thai_ci
SET @@session.collation_connection = tis620_bin;
SELECT @@session.collation_connection;
@@session.collation_connection
tis620_bin
SET @@session.collation_connection = euckr_korean_ci;
SELECT @@session.collation_connection;
@@session.collation_connection
euckr_korean_ci
SET @@session.collation_connection = euckr_bin;
SELECT @@session.collation_connection;
@@session.collation_connection
euckr_bin
SET @@session.collation_connection = koi8u_general_ci;
SELECT @@session.collation_connection;
@@session.collation_connection
koi8u_general_ci
SET @@session.collation_connection = koi8u_bin;
SELECT @@session.collation_connection;
@@session.collation_connection
koi8u_bin
SET @@session.collation_connection = gb2312_chinese_ci;
SELECT @@session.collation_connection;
@@session.collation_connection
gb2312_chinese_ci
SET @@session.collation_connection = gb2312_bin;
SELECT @@session.collation_connection;
@@session.collation_connection
gb2312_bin
SET @@session.collation_connection = greek_general_ci;
SELECT @@session.collation_connection;
@@session.collation_connection
greek_general_ci
SET @@session.collation_connection = greek_bin;
SELECT @@session.collation_connection;
@@session.collation_connection
greek_bin
SET @@session.collation_connection = cp1250_general_ci;
SELECT @@session.collation_connection;
@@session.collation_connection
cp1250_general_ci
SET @@session.collation_connection = cp1250_czech_cs;
SELECT @@session.collation_connection;
@@session.collation_connection
cp1250_czech_cs
SET @@session.collation_connection = cp1250_croatian_ci;
SELECT @@session.collation_connection;
@@session.collation_connection
cp1250_croatian_ci
SET @@session.collation_connection = cp1250_bin;
SELECT @@session.collation_connection;
@@session.collation_connection
cp1250_bin
SET @@session.collation_connection = cp1250_polish_ci;
SELECT @@session.collation_connection;
@@session.collation_connection
cp1250_polish_ci
SET @@session.collation_connection = gbk_chinese_ci;
SELECT @@session.collation_connection;
@@session.collation_connection
gbk_chinese_ci
SET @@session.collation_connection = gbk_bin;
SELECT @@session.collation_connection;
@@session.collation_connection
gbk_bin
SET @@session.collation_connection = gb18030_chinese_ci;
SELECT @@session.collation_connection;
@@session.collation_connection
gb18030_chinese_ci
SET @@session.collation_connection = gb18030_bin;
SELECT @@session.collation_connection;
@@session.collation_connection
gb18030_bin
SET @@session.collation_connection = latin5_turkish_ci;
SELECT @@session.collation_connection;
@@session.collation_connection
latin5_turkish_ci
SET @@session.collation_connection = latin5_bin;
SELECT @@session.collation_connection;
@@session.collation_connection
latin5_bin
SET @@session.collation_connection = armscii8_general_ci;
SELECT @@session.collation_connection;
@@session.collation_connection
armscii8_general_ci
SET @@session.collation_connection = armscii8_bin;
SELECT @@session.collation_connection;
@@session.collation_connection
armscii8_bin
SET @@session.collation_connection = utf8_general_ci;
Warnings:
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
utf8mb3_general_ci
SET @@session.collation_connection = utf8_bin;
Warnings:
Warning	3778	'utf8mb3_bin' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
utf8mb3_bin
SET @@session.collation_connection = utf8_unicode_ci;
Warnings:
Warning	3778	'utf8mb3_unicode_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
utf8mb3_unicode_ci
SET @@session.collation_connection = utf8_icelandic_ci;
Warnings:
Warning	3778	'utf8mb3_icelandic_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
utf8mb3_icelandic_ci
SET @@session.collation_connection = utf8_latvian_ci;
Warnings:
Warning	3778	'utf8mb3_latvian_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
utf8mb3_latvian_ci
SET @@session.collation_connection = utf8_romanian_ci;
Warnings:
Warning	3778	'utf8mb3_romanian_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
utf8mb3_romanian_ci
SET @@session.collation_connection = utf8_slovenian_ci;
Warnings:
Warning	3778	'utf8mb3_slovenian_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
utf8mb3_slovenian_ci
SET @@session.collation_connection = utf8_polish_ci;
Warnings:
Warning	3778	'utf8mb3_polish_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
utf8mb3_polish_ci
SET @@session.collation_connection = utf8_estonian_ci;
Warnings:
Warning	3778	'utf8mb3_estonian_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
utf8mb3_estonian_ci
SET @@session.collation_connection = utf8_spanish_ci;
Warnings:
Warning	3778	'utf8mb3_spanish_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
utf8mb3_spanish_ci
SET @@session.collation_connection = utf8_swedish_ci;
Warnings:
Warning	3778	'utf8mb3_swedish_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
utf8mb3_swedish_ci
SET @@session.collation_connection = utf8_turkish_ci;
Warnings:
Warning	3778	'utf8mb3_turkish_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
utf8mb3_turkish_ci
SET @@session.collation_connection = utf8_czech_ci;
Warnings:
Warning	3778	'utf8mb3_czech_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
utf8mb3_czech_ci
SET @@session.collation_connection = utf8_danish_ci;
Warnings:
Warning	3778	'utf8mb3_danish_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
utf8mb3_danish_ci
SET @@session.collation_connection = utf8_lithuanian_ci;
Warnings:
Warning	3778	'utf8mb3_lithuanian_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
utf8mb3_lithuanian_ci
SET @@session.collation_connection = utf8_slovak_ci;
Warnings:
Warning	3778	'utf8mb3_slovak_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
utf8mb3_slovak_ci
SET @@session.collation_connection = utf8_spanish2_ci;
Warnings:
Warning	3778	'utf8mb3_spanish2_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
utf8mb3_spanish2_ci
SET @@session.collation_connection = utf8_roman_ci;
Warnings:
Warning	3778	'utf8mb3_roman_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
utf8mb3_roman_ci
SET @@session.collation_connection = utf8_persian_ci;
Warnings:
Warning	3778	'utf8mb3_persian_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
utf8mb3_persian_ci
SET @@session.collation_connection = utf8_esperanto_ci;
Warnings:
Warning	3778	'utf8mb3_esperanto_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
utf8mb3_esperanto_ci
SET @@session.collation_connection = utf8_hungarian_ci;
Warnings:
Warning	3778	'utf8mb3_hungarian_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
utf8mb3_hungarian_ci
SET @@session.collation_connection = utf8_unicode_520_ci;
Warnings:
Warning	3778	'utf8mb3_unicode_520_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
utf8mb3_unicode_520_ci
SET @@session.collation_connection = ucs2_general_ci;
Warnings:
Warning	4079	'ucs2_general_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
ucs2_general_ci
SET @@session.collation_connection = ucs2_bin;
Warnings:
Warning	4079	'ucs2_bin' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
ucs2_bin
SET @@session.collation_connection = ucs2_unicode_ci;
Warnings:
Warning	4079	'ucs2_unicode_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
ucs2_unicode_ci
SET @@session.collation_connection = ucs2_icelandic_ci;
Warnings:
Warning	4079	'ucs2_icelandic_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
ucs2_icelandic_ci
SET @@session.collation_connection = ucs2_latvian_ci;
Warnings:
Warning	4079	'ucs2_latvian_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
ucs2_latvian_ci
SET @@session.collation_connection = ucs2_romanian_ci;
Warnings:
Warning	4079	'ucs2_romanian_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
ucs2_romanian_ci
SET @@session.collation_connection = ucs2_slovenian_ci;
Warnings:
Warning	4079	'ucs2_slovenian_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
ucs2_slovenian_ci
SET @@session.collation_connection = ucs2_polish_ci;
Warnings:
Warning	4079	'ucs2_polish_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
ucs2_polish_ci
SET @@session.collation_connection = ucs2_estonian_ci;
Warnings:
Warning	4079	'ucs2_estonian_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
ucs2_estonian_ci
SET @@session.collation_connection = ucs2_spanish_ci;
Warnings:
Warning	4079	'ucs2_spanish_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
ucs2_spanish_ci
SET @@session.collation_connection = ucs2_swedish_ci;
Warnings:
Warning	4079	'ucs2_swedish_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
ucs2_swedish_ci
SET @@session.collation_connection = ucs2_turkish_ci;
Warnings:
Warning	4079	'ucs2_turkish_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
ucs2_turkish_ci
SET @@session.collation_connection = ucs2_czech_ci;
Warnings:
Warning	4079	'ucs2_czech_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
ucs2_czech_ci
SET @@session.collation_connection = ucs2_danish_ci;
Warnings:
Warning	4079	'ucs2_danish_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
ucs2_danish_ci
SET @@session.collation_connection = ucs2_lithuanian_ci;
Warnings:
Warning	4079	'ucs2_lithuanian_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
ucs2_lithuanian_ci
SET @@session.collation_connection = ucs2_slovak_ci;
Warnings:
Warning	4079	'ucs2_slovak_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
ucs2_slovak_ci
SET @@session.collation_connection = ucs2_spanish2_ci;
Warnings:
Warning	4079	'ucs2_spanish2_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
ucs2_spanish2_ci
SET @@session.collation_connection = ucs2_roman_ci;
Warnings:
Warning	4079	'ucs2_roman_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
ucs2_roman_ci
SET @@session.collation_connection = ucs2_persian_ci;
Warnings:
Warning	4079	'ucs2_persian_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
ucs2_persian_ci
SET @@session.collation_connection = ucs2_esperanto_ci;
Warnings:
Warning	4079	'ucs2_esperanto_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
ucs2_esperanto_ci
SET @@session.collation_connection = ucs2_hungarian_ci;
Warnings:
Warning	4079	'ucs2_hungarian_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
ucs2_hungarian_ci
SET @@session.collation_connection = ucs2_unicode_520_ci;
Warnings:
Warning	4079	'ucs2_unicode_520_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
ucs2_unicode_520_ci
SET @@session.collation_connection = cp866_general_ci;
SELECT @@session.collation_connection;
@@session.collation_connection
cp866_general_ci
SET @@session.collation_connection = cp866_bin;
SELECT @@session.collation_connection;
@@session.collation_connection
cp866_bin
SET @@session.collation_connection = keybcs2_general_ci;
SELECT @@session.collation_connection;
@@session.collation_connection
keybcs2_general_ci
SET @@session.collation_connection = keybcs2_bin;
SELECT @@session.collation_connection;
@@session.collation_connection
keybcs2_bin
SET @@session.collation_connection = macce_general_ci;
Warnings:
Warning	4079	'macce_general_ci' is a collation of the deprecated character set macce. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
macce_general_ci
SET @@session.collation_connection = macce_bin;
Warnings:
Warning	4079	'macce_bin' is a collation of the deprecated character set macce. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
macce_bin
SET @@session.collation_connection = macroman_general_ci;
Warnings:
Warning	4079	'macroman_general_ci' is a collation of the deprecated character set macroman. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
macroman_general_ci
SET @@session.collation_connection = macroman_bin;
Warnings:
Warning	4079	'macroman_bin' is a collation of the deprecated character set macroman. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_connection;
@@session.collation_connection
macroman_bin
SET @@session.collation_connection = cp852_general_ci;
SELECT @@session.collation_connection;
@@session.collation_connection
cp852_general_ci
SET @@session.collation_connection = cp852_bin;
SELECT @@session.collation_connection;
@@session.collation_connection
cp852_bin
SET @@session.collation_connection = latin7_estonian_cs;
SELECT @@session.collation_connection;
@@session.collation_connection
latin7_estonian_cs
SET @@session.collation_connection = latin7_general_ci;
SELECT @@session.collation_connection;
@@session.collation_connection
latin7_general_ci
SET @@session.collation_connection = latin7_general_cs;
SELECT @@session.collation_connection;
@@session.collation_connection
latin7_general_cs
SET @@session.collation_connection = latin7_bin;
SELECT @@session.collation_connection;
@@session.collation_connection
latin7_bin
SET @@session.collation_connection = cp1251_bulgarian_ci;
SELECT @@session.collation_connection;
@@session.collation_connection
cp1251_bulgarian_ci
SET @@session.collation_connection = cp1251_ukrainian_ci;
SELECT @@session.collation_connection;
@@session.collation_connection
cp1251_ukrainian_ci
SET @@session.collation_connection = cp1251_bin;
SELECT @@session.collation_connection;
@@session.collation_connection
cp1251_bin
SET @@session.collation_connection = cp1251_general_ci;
SELECT @@session.collation_connection;
@@session.collation_connection
cp1251_general_ci
SET @@session.collation_connection = cp1251_general_cs;
SELECT @@session.collation_connection;
@@session.collation_connection
cp1251_general_cs
SET @@session.collation_connection = cp1256_general_ci;
SELECT @@session.collation_connection;
@@session.collation_connection
cp1256_general_ci
SET @@session.collation_connection = cp1256_bin;
SELECT @@session.collation_connection;
@@session.collation_connection
cp1256_bin
SET @@session.collation_connection = cp1257_lithuanian_ci;
SELECT @@session.collation_connection;
@@session.collation_connection
cp1257_lithuanian_ci
SET @@session.collation_connection = cp1257_bin;
SELECT @@session.collation_connection;
@@session.collation_connection
cp1257_bin
SET @@session.collation_connection = cp1257_general_ci;
SELECT @@session.collation_connection;
@@session.collation_connection
cp1257_general_ci
SET @@session.collation_connection = binary;
SELECT @@session.collation_connection;
@@session.collation_connection
binary
SET @@session.collation_connection = geostd8_general_ci;
SELECT @@session.collation_connection;
@@session.collation_connection
geostd8_general_ci
SET @@session.collation_connection = geostd8_bin;
SELECT @@session.collation_connection;
@@session.collation_connection
geostd8_bin
SET @@session.collation_connection = cp932_japanese_ci;
SELECT @@session.collation_connection;
@@session.collation_connection
cp932_japanese_ci
SET @@session.collation_connection = cp932_bin;
SELECT @@session.collation_connection;
@@session.collation_connection
cp932_bin
SET @@session.collation_connection = eucjpms_japanese_ci;
SELECT @@session.collation_connection;
@@session.collation_connection
eucjpms_japanese_ci
SET @@session.collation_connection = eucjpms_bin;
SELECT @@session.collation_connection;
@@session.collation_connection
eucjpms_bin
SET @@session.collation_connection = utf8mb4_0900_ai_ci;
SELECT @@session.collation_connection;
@@session.collation_connection
utf8mb4_0900_ai_ci
'#--------------------FN_DYNVARS_015_07-------------------------#'
SET @@global.collation_connection = big5_chinese_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
big5_chinese_ci
SET @@global.collation_connection = big5_bin;
SELECT @@global.collation_connection;
@@global.collation_connection
big5_bin
SET @@global.collation_connection = dec8_swedish_ci;
Warnings:
Warning	4079	'dec8_swedish_ci' is a collation of the deprecated character set dec8. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
dec8_swedish_ci
SET @@global.collation_connection = dec8_bin;
Warnings:
Warning	4079	'dec8_bin' is a collation of the deprecated character set dec8. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
dec8_bin
SET @@global.collation_connection = cp850_general_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
cp850_general_ci
SET @@global.collation_connection = cp850_bin;
SELECT @@global.collation_connection;
@@global.collation_connection
cp850_bin
SET @@global.collation_connection = hp8_english_ci;
Warnings:
Warning	4079	'hp8_english_ci' is a collation of the deprecated character set hp8. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
hp8_english_ci
SET @@global.collation_connection = hp8_bin;
Warnings:
Warning	4079	'hp8_bin' is a collation of the deprecated character set hp8. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
hp8_bin
SET @@global.collation_connection = koi8r_general_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
koi8r_general_ci
SET @@global.collation_connection = koi8r_bin;
SELECT @@global.collation_connection;
@@global.collation_connection
koi8r_bin
SET @@global.collation_connection = latin1_german1_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
latin1_german1_ci
SET @@global.collation_connection = latin1_swedish_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
latin1_swedish_ci
SET @@global.collation_connection = latin1_danish_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
latin1_danish_ci
SET @@global.collation_connection = latin1_german2_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
latin1_german2_ci
SET @@global.collation_connection = latin1_bin;
SELECT @@global.collation_connection;
@@global.collation_connection
latin1_bin
SET @@global.collation_connection = latin1_general_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
latin1_general_ci
SET @@global.collation_connection = latin1_general_cs;
SELECT @@global.collation_connection;
@@global.collation_connection
latin1_general_cs
SET @@global.collation_connection = latin1_spanish_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
latin1_spanish_ci
SET @@global.collation_connection = latin2_czech_cs;
SELECT @@global.collation_connection;
@@global.collation_connection
latin2_czech_cs
SET @@global.collation_connection = latin2_general_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
latin2_general_ci
SET @@global.collation_connection = latin2_hungarian_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
latin2_hungarian_ci
SET @@global.collation_connection = latin2_croatian_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
latin2_croatian_ci
SET @@global.collation_connection = latin2_bin;
SELECT @@global.collation_connection;
@@global.collation_connection
latin2_bin
SET @@global.collation_connection = swe7_swedish_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
swe7_swedish_ci
SET @@global.collation_connection = swe7_bin;
SELECT @@global.collation_connection;
@@global.collation_connection
swe7_bin
SET @@global.collation_connection = ascii_general_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
ascii_general_ci
SET @@global.collation_connection = ascii_bin;
SELECT @@global.collation_connection;
@@global.collation_connection
ascii_bin
SET @@global.collation_connection = ujis_japanese_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
ujis_japanese_ci
SET @@global.collation_connection = ujis_bin;
SELECT @@global.collation_connection;
@@global.collation_connection
ujis_bin
SET @@global.collation_connection = sjis_japanese_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
sjis_japanese_ci
SET @@global.collation_connection = sjis_bin;
SELECT @@global.collation_connection;
@@global.collation_connection
sjis_bin
SET @@global.collation_connection = hebrew_general_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
hebrew_general_ci
SET @@global.collation_connection = hebrew_bin;
SELECT @@global.collation_connection;
@@global.collation_connection
hebrew_bin
SET @@global.collation_connection = tis620_thai_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
tis620_thai_ci
SET @@global.collation_connection = tis620_bin;
SELECT @@global.collation_connection;
@@global.collation_connection
tis620_bin
SET @@global.collation_connection = euckr_korean_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
euckr_korean_ci
SET @@global.collation_connection = euckr_bin;
SELECT @@global.collation_connection;
@@global.collation_connection
euckr_bin
SET @@global.collation_connection = koi8u_general_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
koi8u_general_ci
SET @@global.collation_connection = koi8u_bin;
SELECT @@global.collation_connection;
@@global.collation_connection
koi8u_bin
SET @@global.collation_connection = gb2312_chinese_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
gb2312_chinese_ci
SET @@global.collation_connection = gb2312_bin;
SELECT @@global.collation_connection;
@@global.collation_connection
gb2312_bin
SET @@global.collation_connection = greek_general_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
greek_general_ci
SET @@global.collation_connection = greek_bin;
SELECT @@global.collation_connection;
@@global.collation_connection
greek_bin
SET @@global.collation_connection = cp1250_general_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
cp1250_general_ci
SET @@global.collation_connection = cp1250_czech_cs;
SELECT @@global.collation_connection;
@@global.collation_connection
cp1250_czech_cs
SET @@global.collation_connection = cp1250_croatian_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
cp1250_croatian_ci
SET @@global.collation_connection = cp1250_bin;
SELECT @@global.collation_connection;
@@global.collation_connection
cp1250_bin
SET @@global.collation_connection = cp1250_polish_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
cp1250_polish_ci
SET @@global.collation_connection = gbk_chinese_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
gbk_chinese_ci
SET @@global.collation_connection = gbk_bin;
SELECT @@global.collation_connection;
@@global.collation_connection
gbk_bin
SET @@global.collation_connection = gb18030_chinese_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
gb18030_chinese_ci
SET @@global.collation_connection = gb18030_bin;
SELECT @@global.collation_connection;
@@global.collation_connection
gb18030_bin
SET @@global.collation_connection = latin5_turkish_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
latin5_turkish_ci
SET @@global.collation_connection = latin5_bin;
SELECT @@global.collation_connection;
@@global.collation_connection
latin5_bin
SET @@global.collation_connection = armscii8_general_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
armscii8_general_ci
SET @@global.collation_connection = armscii8_bin;
SELECT @@global.collation_connection;
@@global.collation_connection
armscii8_bin
SET @@global.collation_connection = utf8_general_ci;
Warnings:
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
utf8mb3_general_ci
SET @@global.collation_connection = utf8_bin;
Warnings:
Warning	3778	'utf8mb3_bin' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
utf8mb3_bin
SET @@global.collation_connection = utf8_unicode_ci;
Warnings:
Warning	3778	'utf8mb3_unicode_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
utf8mb3_unicode_ci
SET @@global.collation_connection = utf8_icelandic_ci;
Warnings:
Warning	3778	'utf8mb3_icelandic_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
utf8mb3_icelandic_ci
SET @@global.collation_connection = utf8_latvian_ci;
Warnings:
Warning	3778	'utf8mb3_latvian_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
utf8mb3_latvian_ci
SET @@global.collation_connection = utf8_romanian_ci;
Warnings:
Warning	3778	'utf8mb3_romanian_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
utf8mb3_romanian_ci
SET @@global.collation_connection = utf8_slovenian_ci;
Warnings:
Warning	3778	'utf8mb3_slovenian_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
utf8mb3_slovenian_ci
SET @@global.collation_connection = utf8_polish_ci;
Warnings:
Warning	3778	'utf8mb3_polish_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
utf8mb3_polish_ci
SET @@global.collation_connection = utf8_estonian_ci;
Warnings:
Warning	3778	'utf8mb3_estonian_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
utf8mb3_estonian_ci
SET @@global.collation_connection = utf8_spanish_ci;
Warnings:
Warning	3778	'utf8mb3_spanish_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
utf8mb3_spanish_ci
SET @@global.collation_connection = utf8_swedish_ci;
Warnings:
Warning	3778	'utf8mb3_swedish_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
utf8mb3_swedish_ci
SET @@global.collation_connection = utf8_turkish_ci;
Warnings:
Warning	3778	'utf8mb3_turkish_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
utf8mb3_turkish_ci
SET @@global.collation_connection = utf8_czech_ci;
Warnings:
Warning	3778	'utf8mb3_czech_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
utf8mb3_czech_ci
SET @@global.collation_connection = utf8_danish_ci;
Warnings:
Warning	3778	'utf8mb3_danish_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
utf8mb3_danish_ci
SET @@global.collation_connection = utf8_lithuanian_ci;
Warnings:
Warning	3778	'utf8mb3_lithuanian_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
utf8mb3_lithuanian_ci
SET @@global.collation_connection = utf8_slovak_ci;
Warnings:
Warning	3778	'utf8mb3_slovak_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
utf8mb3_slovak_ci
SET @@global.collation_connection = utf8_spanish2_ci;
Warnings:
Warning	3778	'utf8mb3_spanish2_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
utf8mb3_spanish2_ci
SET @@global.collation_connection = utf8_roman_ci;
Warnings:
Warning	3778	'utf8mb3_roman_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
utf8mb3_roman_ci
SET @@global.collation_connection = utf8_persian_ci;
Warnings:
Warning	3778	'utf8mb3_persian_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
utf8mb3_persian_ci
SET @@global.collation_connection = utf8_esperanto_ci;
Warnings:
Warning	3778	'utf8mb3_esperanto_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
utf8mb3_esperanto_ci
SET @@global.collation_connection = utf8_hungarian_ci;
Warnings:
Warning	3778	'utf8mb3_hungarian_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
utf8mb3_hungarian_ci
SET @@global.collation_connection = utf8_unicode_520_ci;
Warnings:
Warning	3778	'utf8mb3_unicode_520_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
utf8mb3_unicode_520_ci
SET @@global.collation_connection = ucs2_general_ci;
Warnings:
Warning	4079	'ucs2_general_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
ucs2_general_ci
SET @@global.collation_connection = ucs2_bin;
Warnings:
Warning	4079	'ucs2_bin' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
ucs2_bin
SET @@global.collation_connection = ucs2_unicode_ci;
Warnings:
Warning	4079	'ucs2_unicode_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
ucs2_unicode_ci
SET @@global.collation_connection = ucs2_icelandic_ci;
Warnings:
Warning	4079	'ucs2_icelandic_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
ucs2_icelandic_ci
SET @@global.collation_connection = ucs2_latvian_ci;
Warnings:
Warning	4079	'ucs2_latvian_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
ucs2_latvian_ci
SET @@global.collation_connection = ucs2_romanian_ci;
Warnings:
Warning	4079	'ucs2_romanian_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
ucs2_romanian_ci
SET @@global.collation_connection = ucs2_slovenian_ci;
Warnings:
Warning	4079	'ucs2_slovenian_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
ucs2_slovenian_ci
SET @@global.collation_connection = ucs2_polish_ci;
Warnings:
Warning	4079	'ucs2_polish_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
ucs2_polish_ci
SET @@global.collation_connection = ucs2_estonian_ci;
Warnings:
Warning	4079	'ucs2_estonian_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
ucs2_estonian_ci
SET @@global.collation_connection = ucs2_spanish_ci;
Warnings:
Warning	4079	'ucs2_spanish_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
ucs2_spanish_ci
SET @@global.collation_connection = ucs2_swedish_ci;
Warnings:
Warning	4079	'ucs2_swedish_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
ucs2_swedish_ci
SET @@global.collation_connection = ucs2_turkish_ci;
Warnings:
Warning	4079	'ucs2_turkish_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
ucs2_turkish_ci
SET @@global.collation_connection = ucs2_czech_ci;
Warnings:
Warning	4079	'ucs2_czech_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
ucs2_czech_ci
SET @@global.collation_connection = ucs2_danish_ci;
Warnings:
Warning	4079	'ucs2_danish_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
ucs2_danish_ci
SET @@global.collation_connection = ucs2_lithuanian_ci;
Warnings:
Warning	4079	'ucs2_lithuanian_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
ucs2_lithuanian_ci
SET @@global.collation_connection = ucs2_slovak_ci;
Warnings:
Warning	4079	'ucs2_slovak_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
ucs2_slovak_ci
SET @@global.collation_connection = ucs2_spanish2_ci;
Warnings:
Warning	4079	'ucs2_spanish2_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
ucs2_spanish2_ci
SET @@global.collation_connection = ucs2_roman_ci;
Warnings:
Warning	4079	'ucs2_roman_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
ucs2_roman_ci
SET @@global.collation_connection = ucs2_persian_ci;
Warnings:
Warning	4079	'ucs2_persian_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
ucs2_persian_ci
SET @@global.collation_connection = ucs2_esperanto_ci;
Warnings:
Warning	4079	'ucs2_esperanto_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
ucs2_esperanto_ci
SET @@global.collation_connection = ucs2_hungarian_ci;
Warnings:
Warning	4079	'ucs2_hungarian_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
ucs2_hungarian_ci
SET @@global.collation_connection = ucs2_unicode_520_ci;
Warnings:
Warning	4079	'ucs2_unicode_520_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
ucs2_unicode_520_ci
SET @@global.collation_connection = cp866_general_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
cp866_general_ci
SET @@global.collation_connection = cp866_bin;
SELECT @@global.collation_connection;
@@global.collation_connection
cp866_bin
SET @@global.collation_connection = keybcs2_general_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
keybcs2_general_ci
SET @@global.collation_connection = keybcs2_bin;
SELECT @@global.collation_connection;
@@global.collation_connection
keybcs2_bin
SET @@global.collation_connection = macce_general_ci;
Warnings:
Warning	4079	'macce_general_ci' is a collation of the deprecated character set macce. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
macce_general_ci
SET @@global.collation_connection = macce_bin;
Warnings:
Warning	4079	'macce_bin' is a collation of the deprecated character set macce. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
macce_bin
SET @@global.collation_connection = macroman_general_ci;
Warnings:
Warning	4079	'macroman_general_ci' is a collation of the deprecated character set macroman. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
macroman_general_ci
SET @@global.collation_connection = macroman_bin;
Warnings:
Warning	4079	'macroman_bin' is a collation of the deprecated character set macroman. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
macroman_bin
SET @@global.collation_connection = cp852_general_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
cp852_general_ci
SET @@global.collation_connection = cp852_bin;
SELECT @@global.collation_connection;
@@global.collation_connection
cp852_bin
SET @@global.collation_connection = latin7_estonian_cs;
SELECT @@global.collation_connection;
@@global.collation_connection
latin7_estonian_cs
SET @@global.collation_connection = latin7_general_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
latin7_general_ci
SET @@global.collation_connection = latin7_general_cs;
SELECT @@global.collation_connection;
@@global.collation_connection
latin7_general_cs
SET @@global.collation_connection = latin7_bin;
SELECT @@global.collation_connection;
@@global.collation_connection
latin7_bin
SET @@global.collation_connection = cp1251_bulgarian_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
cp1251_bulgarian_ci
SET @@global.collation_connection = cp1251_ukrainian_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
cp1251_ukrainian_ci
SET @@global.collation_connection = cp1251_bin;
SELECT @@global.collation_connection;
@@global.collation_connection
cp1251_bin
SET @@global.collation_connection = cp1251_general_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
cp1251_general_ci
SET @@global.collation_connection = cp1251_general_cs;
SELECT @@global.collation_connection;
@@global.collation_connection
cp1251_general_cs
SET @@global.collation_connection = cp1256_general_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
cp1256_general_ci
SET @@global.collation_connection = cp1256_bin;
SELECT @@global.collation_connection;
@@global.collation_connection
cp1256_bin
SET @@global.collation_connection = cp1257_lithuanian_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
cp1257_lithuanian_ci
SET @@global.collation_connection = cp1257_bin;
SELECT @@global.collation_connection;
@@global.collation_connection
cp1257_bin
SET @@global.collation_connection = cp1257_general_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
cp1257_general_ci
SET @@global.collation_connection = binary;
SELECT @@global.collation_connection;
@@global.collation_connection
binary
SET @@global.collation_connection = geostd8_general_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
geostd8_general_ci
SET @@global.collation_connection = geostd8_bin;
SELECT @@global.collation_connection;
@@global.collation_connection
geostd8_bin
SET @@global.collation_connection = cp932_japanese_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
cp932_japanese_ci
SET @@global.collation_connection = cp932_bin;
SELECT @@global.collation_connection;
@@global.collation_connection
cp932_bin
SET @@global.collation_connection = eucjpms_japanese_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
eucjpms_japanese_ci
SET @@global.collation_connection = eucjpms_bin;
SELECT @@global.collation_connection;
@@global.collation_connection
eucjpms_bin
SET @@global.collation_connection = utf8mb4_0900_ai_ci;
SELECT @@global.collation_connection;
@@global.collation_connection
utf8mb4_0900_ai_ci
'#--------------------FN_DYNVARS_015_08-------------------------#'
SET @@collation_connection = LATIN7_GENERAL_CS;
SELECT @@collation_connection;
@@collation_connection
latin7_general_cs
SET @@collation_connection = latin7_general_cs;
SELECT @@collation_connection;
@@collation_connection
latin7_general_cs
SET @@global.collation_connection = Latin7_GeneRal_cS;
SELECT @@global.collation_connection;
@@global.collation_connection
latin7_general_cs
'#--------------------FN_DYNVARS_015_09-------------------------#'
SET @@collation_connection = 1;
SELECT @@collation_connection;
@@collation_connection
big5_chinese_ci
SET @@collation_connection = 2;
SELECT @@collation_connection;
@@collation_connection
latin2_czech_cs
SET @@collation_connection = 3;
Warnings:
Warning	4079	'dec8_swedish_ci' is a collation of the deprecated character set dec8. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@collation_connection;
@@collation_connection
dec8_swedish_ci
SET @@collation_connection = 99;
SELECT @@collation_connection;
@@collation_connection
cp1250_polish_ci
SET @@collation_connection = 100;
ERROR HY000: Unknown collation: '100'
SET @@global.collation_connection = 1;
SELECT @@global.collation_connection;
@@global.collation_connection
big5_chinese_ci
SET @@global.collation_connection = 2;
SELECT @@global.collation_connection;
@@global.collation_connection
latin2_czech_cs
SET @@global.collation_connection = 3;
Warnings:
Warning	4079	'dec8_swedish_ci' is a collation of the deprecated character set dec8. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_connection;
@@global.collation_connection
dec8_swedish_ci
SET @@global.collation_connection = 99;
SELECT @@global.collation_connection;
@@global.collation_connection
cp1250_polish_ci
SET @@global.collation_connection = 100;
ERROR HY000: Unknown collation: '100'
SET @total_collations = (SELECT count(*) FROM INFORMATION_SCHEMA.COLLATIONS);
SELECT @total_collations > 120;
@total_collations > 120
1
'#--------------------FN_DYNVARS_015_10-------------------------#'
SET @@collation_connection = latin7_binary;
ERROR HY000: Unknown collation: 'latin7_binary'
SET @@collation_connection = 'eucjpms_japanese_cs';
ERROR HY000: Unknown collation: 'eucjpms_japanese_cs'
SET @@collation_connection = 0;
ERROR HY000: Unknown collation: '0'
SET @@collation_connection = 1.01;
ERROR 42000: Incorrect argument type to variable 'collation_connection'
SET @@collation_connection = -1;
ERROR HY000: Unknown collation: '-1'
SET @@collation_connection = '';
ERROR HY000: Unknown collation: ''
SET @@collation_connection = ' eucjpms_bin';
ERROR HY000: Unknown collation: ' eucjpms_bin'
SET @@collation_connection = true;
SELECT @@collation_connection AS res_with_true;
res_with_true
big5_chinese_ci
SET @@collation_connection = ON;
ERROR HY000: Unknown collation: 'ON'
SET @@collation_connection = repeat('e', 256);
ERROR HY000: Unknown collation: 'eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee'
SET @@collation_connection = repeat('e', 1024);
ERROR HY000: Unknown collation: 'eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee'
'#--------------------FN_DYNVARS_015_11-------------------------#'
SET @@global.collation_connection = latin7_binary;
ERROR HY000: Unknown collation: 'latin7_binary'
SET @@global.collation_connection = 'eucjpms_japanese_cs';
ERROR HY000: Unknown collation: 'eucjpms_japanese_cs'
SET @@global.collation_connection = 0;
ERROR HY000: Unknown collation: '0'
SET @@global.collation_connection = 1.1;
ERROR 42000: Incorrect argument type to variable 'collation_connection'
SET @@global.collation_connection = -1;
ERROR HY000: Unknown collation: '-1'
SET @@global.collation_connection = "";
ERROR HY000: Unknown collation: ''
SET @@global.collation_connection = ' eucjpms_bin';
ERROR HY000: Unknown collation: ' eucjpms_bin'
SET @@global.collation_connection = true;
SET @@global.collation_connection = ON;
ERROR HY000: Unknown collation: 'ON'
SET @@global.collation_connection = repeat('e', 256);
ERROR HY000: Unknown collation: 'eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee'
SET @@global.collation_connection = repeat('e', 1024);
ERROR HY000: Unknown collation: 'eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee'
'#--------------------FN_DYNVARS_015_12-------------------------#'
SELECT @@global.collation_connection =
(SELECT VARIABLE_VALUE FROM performance_schema.global_variables
WHERE VARIABLE_NAME='collation_connection') AS res;
res
1
SET @@global.collation_connection = 1;
SELECT @@global.collation_connection;
@@global.collation_connection
big5_chinese_ci
SELECT @@global.collation_connection =
(SELECT VARIABLE_VALUE FROM performance_schema.global_variables
WHERE VARIABLE_NAME='collation_connection') AS res;
res
1
'#--------------------FN_DYNVARS_015_13-------------------------#'
SELECT @@collation_connection =
(SELECT VARIABLE_VALUE FROM performance_schema.session_variables
WHERE VARIABLE_NAME='collation_connection') AS res;
res
1
SELECT @@local.collation_connection =
(SELECT VARIABLE_VALUE FROM performance_schema.session_variables
WHERE VARIABLE_NAME='collation_connection') AS res;
res
1
SELECT @@session.collation_connection =
(SELECT VARIABLE_VALUE FROM performance_schema.session_variables
WHERE VARIABLE_NAME='collation_connection') AS res;
res
1
SET @@global.collation_connection = @global_start_value;
SELECT @@global.collation_connection;
@@global.collation_connection
utf8mb4_0900_ai_ci
SET @@session.collation_connection = @session_start_value;
SELECT @@session.collation_connection;
@@session.collation_connection
utf8mb4_0900_ai_ci
