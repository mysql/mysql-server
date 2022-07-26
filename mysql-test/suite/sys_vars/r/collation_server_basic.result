SET @global_start_value = @@global.collation_server;
SELECT @global_start_value;
@global_start_value
utf8mb4_0900_ai_ci
SET @session_start_value = @@collation_server;
SELECT @session_start_value;
@session_start_value
utf8mb4_0900_ai_ci
SET @session_start_value = @@local.collation_server;
SELECT @session_start_value;
@session_start_value
utf8mb4_0900_ai_ci
SET @session_start_value = @@session.collation_server;
SELECT @session_start_value;
@session_start_value
utf8mb4_0900_ai_ci
'#--------------------FN_DYNVARS_014_01------------------#'
SELECT collation_server;
ERROR 42S22: Unknown column 'collation_server' in 'field list'
SET collation_server=utf8_unicode_ci;
Warnings:
Warning	3778	'utf8mb3_unicode_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
utf8mb3_unicode_ci
SET global.collation_server=utf8_unicode_ci;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'global.collation_server=utf8_unicode_ci' at line 1
SET session collation_server=utf8_unicode_ci;
Warnings:
Warning	3778	'utf8mb3_unicode_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT session collation_server;
ERROR 42S22: Unknown column 'session' in 'field list'
SET global collation_server=utf8_unicode_ci;
Warnings:
Warning	3778	'utf8mb3_unicode_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT global collation_server;
ERROR 42S22: Unknown column 'global' in 'field list'
'#--------------------FN_DYNVARS_014_02-------------------------#'
SET @@collation_server = latin1_bin;
SET @@collation_server = DEFAULT;
SELECT @@collation_server AS DEFAULT_VALUE;
DEFAULT_VALUE
utf8mb3_unicode_ci
SET @@global.collation_server = latin1_bin;
SET @@global.collation_server = DEFAULT;
SELECT @@global.collation_server;
@@global.collation_server
utf8mb4_0900_ai_ci
'#--------------------FN_DYNVARS_014_03-------------------------#'
SET @@session.collation_server = utf8_polish_ci;
Warnings:
Warning	3778	'utf8mb3_polish_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
utf8mb3_polish_ci
SET @@global.collation_server = latin7_general_ci;
SELECT @@global.collation_server;
@@global.collation_server
latin7_general_ci
SELECT @@session.collation_server AS res_is_utf8_polish_ci;
res_is_utf8_polish_ci
utf8mb3_polish_ci
SET @@session.collation_server = latin7_bin;
SELECT @@session.collation_server;
@@session.collation_server
latin7_bin
SELECT @@global.collation_server AS res_is_latin7_general_ci;
res_is_latin7_general_ci
latin7_general_ci
SELECT @@global.collation_server=@@session.collation_server AS res_is_false;
res_is_false
0
'#--------------------FN_DYNVARS_014_04-------------------------#'
SELECT @@collation_server = @@session.collation_server AS res;
res
1
SELECT @@collation_server = @@local.collation_server AS res;
res
1
'#--------------------FN_DYNVARS_014_05-------------------------#'
SET @@collation_server = latin7_general_ci + latin7_general_cs;
ERROR 42S22: Unknown column 'latin7_general_ci' in 'field list'
'#--------------------FN_DYNVARS_014_06-------------------------#'
SET @@session.collation_server = big5_chinese_ci;
SELECT @@session.collation_server;
@@session.collation_server
big5_chinese_ci
SET @@session.collation_server = big5_bin;
SELECT @@session.collation_server;
@@session.collation_server
big5_bin
SET @@session.collation_server = dec8_swedish_ci;
Warnings:
Warning	4079	'dec8_swedish_ci' is a collation of the deprecated character set dec8. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
dec8_swedish_ci
SET @@session.collation_server = dec8_bin;
Warnings:
Warning	4079	'dec8_bin' is a collation of the deprecated character set dec8. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
dec8_bin
SET @@session.collation_server = cp850_general_ci;
SELECT @@session.collation_server;
@@session.collation_server
cp850_general_ci
SET @@session.collation_server = cp850_bin;
SELECT @@session.collation_server;
@@session.collation_server
cp850_bin
SET @@session.collation_server = hp8_english_ci;
Warnings:
Warning	4079	'hp8_english_ci' is a collation of the deprecated character set hp8. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
hp8_english_ci
SET @@session.collation_server = hp8_bin;
Warnings:
Warning	4079	'hp8_bin' is a collation of the deprecated character set hp8. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
hp8_bin
SET @@session.collation_server = koi8r_general_ci;
SELECT @@session.collation_server;
@@session.collation_server
koi8r_general_ci
SET @@session.collation_server = koi8r_bin;
SELECT @@session.collation_server;
@@session.collation_server
koi8r_bin
SET @@session.collation_server = latin1_german1_ci;
SELECT @@session.collation_server;
@@session.collation_server
latin1_german1_ci
SET @@session.collation_server = latin1_swedish_ci;
SELECT @@session.collation_server;
@@session.collation_server
latin1_swedish_ci
SET @@session.collation_server = latin1_danish_ci;
SELECT @@session.collation_server;
@@session.collation_server
latin1_danish_ci
SET @@session.collation_server = latin1_german2_ci;
SELECT @@session.collation_server;
@@session.collation_server
latin1_german2_ci
SET @@session.collation_server = latin1_bin;
SELECT @@session.collation_server;
@@session.collation_server
latin1_bin
SET @@session.collation_server = latin1_general_ci;
SELECT @@session.collation_server;
@@session.collation_server
latin1_general_ci
SET @@session.collation_server = latin1_general_cs;
SELECT @@session.collation_server;
@@session.collation_server
latin1_general_cs
SET @@session.collation_server = latin1_spanish_ci;
SELECT @@session.collation_server;
@@session.collation_server
latin1_spanish_ci
SET @@session.collation_server = latin2_czech_cs;
SELECT @@session.collation_server;
@@session.collation_server
latin2_czech_cs
SET @@session.collation_server = latin2_general_ci;
SELECT @@session.collation_server;
@@session.collation_server
latin2_general_ci
SET @@session.collation_server = latin2_hungarian_ci;
SELECT @@session.collation_server;
@@session.collation_server
latin2_hungarian_ci
SET @@session.collation_server = latin2_croatian_ci;
SELECT @@session.collation_server;
@@session.collation_server
latin2_croatian_ci
SET @@session.collation_server = latin2_bin;
SELECT @@session.collation_server;
@@session.collation_server
latin2_bin
SET @@session.collation_server = swe7_swedish_ci;
SELECT @@session.collation_server;
@@session.collation_server
swe7_swedish_ci
SET @@session.collation_server = swe7_bin;
SELECT @@session.collation_server;
@@session.collation_server
swe7_bin
SET @@session.collation_server = ascii_general_ci;
SELECT @@session.collation_server;
@@session.collation_server
ascii_general_ci
SET @@session.collation_server = ascii_bin;
SELECT @@session.collation_server;
@@session.collation_server
ascii_bin
SET @@session.collation_server = ujis_japanese_ci;
SELECT @@session.collation_server;
@@session.collation_server
ujis_japanese_ci
SET @@session.collation_server = ujis_bin;
SELECT @@session.collation_server;
@@session.collation_server
ujis_bin
SET @@session.collation_server = sjis_japanese_ci;
SELECT @@session.collation_server;
@@session.collation_server
sjis_japanese_ci
SET @@session.collation_server = sjis_bin;
SELECT @@session.collation_server;
@@session.collation_server
sjis_bin
SET @@session.collation_server = hebrew_general_ci;
SELECT @@session.collation_server;
@@session.collation_server
hebrew_general_ci
SET @@session.collation_server = hebrew_bin;
SELECT @@session.collation_server;
@@session.collation_server
hebrew_bin
SET @@session.collation_server = tis620_thai_ci;
SELECT @@session.collation_server;
@@session.collation_server
tis620_thai_ci
SET @@session.collation_server = tis620_bin;
SELECT @@session.collation_server;
@@session.collation_server
tis620_bin
SET @@session.collation_server = euckr_korean_ci;
SELECT @@session.collation_server;
@@session.collation_server
euckr_korean_ci
SET @@session.collation_server = euckr_bin;
SELECT @@session.collation_server;
@@session.collation_server
euckr_bin
SET @@session.collation_server = koi8u_general_ci;
SELECT @@session.collation_server;
@@session.collation_server
koi8u_general_ci
SET @@session.collation_server = koi8u_bin;
SELECT @@session.collation_server;
@@session.collation_server
koi8u_bin
SET @@session.collation_server = gb2312_chinese_ci;
SELECT @@session.collation_server;
@@session.collation_server
gb2312_chinese_ci
SET @@session.collation_server = gb2312_bin;
SELECT @@session.collation_server;
@@session.collation_server
gb2312_bin
SET @@session.collation_server = greek_general_ci;
SELECT @@session.collation_server;
@@session.collation_server
greek_general_ci
SET @@session.collation_server = greek_bin;
SELECT @@session.collation_server;
@@session.collation_server
greek_bin
SET @@session.collation_server = cp1250_general_ci;
SELECT @@session.collation_server;
@@session.collation_server
cp1250_general_ci
SET @@session.collation_server = cp1250_czech_cs;
SELECT @@session.collation_server;
@@session.collation_server
cp1250_czech_cs
SET @@session.collation_server = cp1250_croatian_ci;
SELECT @@session.collation_server;
@@session.collation_server
cp1250_croatian_ci
SET @@session.collation_server = cp1250_bin;
SELECT @@session.collation_server;
@@session.collation_server
cp1250_bin
SET @@session.collation_server = cp1250_polish_ci;
SELECT @@session.collation_server;
@@session.collation_server
cp1250_polish_ci
SET @@session.collation_server = gbk_chinese_ci;
SELECT @@session.collation_server;
@@session.collation_server
gbk_chinese_ci
SET @@session.collation_server = gbk_bin;
SELECT @@session.collation_server;
@@session.collation_server
gbk_bin
SET @@session.collation_server = gb18030_chinese_ci;
SELECT @@session.collation_server;
@@session.collation_server
gb18030_chinese_ci
SET @@session.collation_server = gb18030_bin;
SELECT @@session.collation_server;
@@session.collation_server
gb18030_bin
SET @@session.collation_server = latin5_turkish_ci;
SELECT @@session.collation_server;
@@session.collation_server
latin5_turkish_ci
SET @@session.collation_server = latin5_bin;
SELECT @@session.collation_server;
@@session.collation_server
latin5_bin
SET @@session.collation_server = armscii8_general_ci;
SELECT @@session.collation_server;
@@session.collation_server
armscii8_general_ci
SET @@session.collation_server = armscii8_bin;
SELECT @@session.collation_server;
@@session.collation_server
armscii8_bin
SET @@session.collation_server = utf8_general_ci;
Warnings:
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
utf8mb3_general_ci
SET @@session.collation_server = utf8_bin;
Warnings:
Warning	3778	'utf8mb3_bin' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
utf8mb3_bin
SET @@session.collation_server = utf8_unicode_ci;
Warnings:
Warning	3778	'utf8mb3_unicode_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
utf8mb3_unicode_ci
SET @@session.collation_server = utf8_icelandic_ci;
Warnings:
Warning	3778	'utf8mb3_icelandic_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
utf8mb3_icelandic_ci
SET @@session.collation_server = utf8_latvian_ci;
Warnings:
Warning	3778	'utf8mb3_latvian_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
utf8mb3_latvian_ci
SET @@session.collation_server = utf8_romanian_ci;
Warnings:
Warning	3778	'utf8mb3_romanian_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
utf8mb3_romanian_ci
SET @@session.collation_server = utf8_slovenian_ci;
Warnings:
Warning	3778	'utf8mb3_slovenian_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
utf8mb3_slovenian_ci
SET @@session.collation_server = utf8_polish_ci;
Warnings:
Warning	3778	'utf8mb3_polish_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
utf8mb3_polish_ci
SET @@session.collation_server = utf8_estonian_ci;
Warnings:
Warning	3778	'utf8mb3_estonian_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
utf8mb3_estonian_ci
SET @@session.collation_server = utf8_spanish_ci;
Warnings:
Warning	3778	'utf8mb3_spanish_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
utf8mb3_spanish_ci
SET @@session.collation_server = utf8_swedish_ci;
Warnings:
Warning	3778	'utf8mb3_swedish_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
utf8mb3_swedish_ci
SET @@session.collation_server = utf8_turkish_ci;
Warnings:
Warning	3778	'utf8mb3_turkish_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
utf8mb3_turkish_ci
SET @@session.collation_server = utf8_czech_ci;
Warnings:
Warning	3778	'utf8mb3_czech_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
utf8mb3_czech_ci
SET @@session.collation_server = utf8_danish_ci;
Warnings:
Warning	3778	'utf8mb3_danish_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
utf8mb3_danish_ci
SET @@session.collation_server = utf8_lithuanian_ci;
Warnings:
Warning	3778	'utf8mb3_lithuanian_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
utf8mb3_lithuanian_ci
SET @@session.collation_server = utf8_slovak_ci;
Warnings:
Warning	3778	'utf8mb3_slovak_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
utf8mb3_slovak_ci
SET @@session.collation_server = utf8_spanish2_ci;
Warnings:
Warning	3778	'utf8mb3_spanish2_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
utf8mb3_spanish2_ci
SET @@session.collation_server = utf8_roman_ci;
Warnings:
Warning	3778	'utf8mb3_roman_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
utf8mb3_roman_ci
SET @@session.collation_server = utf8_persian_ci;
Warnings:
Warning	3778	'utf8mb3_persian_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
utf8mb3_persian_ci
SET @@session.collation_server = utf8_esperanto_ci;
Warnings:
Warning	3778	'utf8mb3_esperanto_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
utf8mb3_esperanto_ci
SET @@session.collation_server = utf8_hungarian_ci;
Warnings:
Warning	3778	'utf8mb3_hungarian_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
utf8mb3_hungarian_ci
SET @@session.collation_server = utf8_unicode_520_ci;
Warnings:
Warning	3778	'utf8mb3_unicode_520_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
utf8mb3_unicode_520_ci
SET @@session.collation_server = ucs2_general_ci;
Warnings:
Warning	4079	'ucs2_general_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
ucs2_general_ci
SET @@session.collation_server = ucs2_bin;
Warnings:
Warning	4079	'ucs2_bin' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
ucs2_bin
SET @@session.collation_server = ucs2_unicode_ci;
Warnings:
Warning	4079	'ucs2_unicode_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
ucs2_unicode_ci
SET @@session.collation_server = ucs2_icelandic_ci;
Warnings:
Warning	4079	'ucs2_icelandic_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
ucs2_icelandic_ci
SET @@session.collation_server = ucs2_latvian_ci;
Warnings:
Warning	4079	'ucs2_latvian_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
ucs2_latvian_ci
SET @@session.collation_server = ucs2_romanian_ci;
Warnings:
Warning	4079	'ucs2_romanian_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
ucs2_romanian_ci
SET @@session.collation_server = ucs2_slovenian_ci;
Warnings:
Warning	4079	'ucs2_slovenian_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
ucs2_slovenian_ci
SET @@session.collation_server = ucs2_polish_ci;
Warnings:
Warning	4079	'ucs2_polish_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
ucs2_polish_ci
SET @@session.collation_server = ucs2_estonian_ci;
Warnings:
Warning	4079	'ucs2_estonian_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
ucs2_estonian_ci
SET @@session.collation_server = ucs2_spanish_ci;
Warnings:
Warning	4079	'ucs2_spanish_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
ucs2_spanish_ci
SET @@session.collation_server = ucs2_swedish_ci;
Warnings:
Warning	4079	'ucs2_swedish_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
ucs2_swedish_ci
SET @@session.collation_server = ucs2_turkish_ci;
Warnings:
Warning	4079	'ucs2_turkish_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
ucs2_turkish_ci
SET @@session.collation_server = ucs2_czech_ci;
Warnings:
Warning	4079	'ucs2_czech_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
ucs2_czech_ci
SET @@session.collation_server = ucs2_danish_ci;
Warnings:
Warning	4079	'ucs2_danish_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
ucs2_danish_ci
SET @@session.collation_server = ucs2_lithuanian_ci;
Warnings:
Warning	4079	'ucs2_lithuanian_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
ucs2_lithuanian_ci
SET @@session.collation_server = ucs2_slovak_ci;
Warnings:
Warning	4079	'ucs2_slovak_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
ucs2_slovak_ci
SET @@session.collation_server = ucs2_spanish2_ci;
Warnings:
Warning	4079	'ucs2_spanish2_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
ucs2_spanish2_ci
SET @@session.collation_server = ucs2_roman_ci;
Warnings:
Warning	4079	'ucs2_roman_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
ucs2_roman_ci
SET @@session.collation_server = ucs2_persian_ci;
Warnings:
Warning	4079	'ucs2_persian_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
ucs2_persian_ci
SET @@session.collation_server = ucs2_esperanto_ci;
Warnings:
Warning	4079	'ucs2_esperanto_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
ucs2_esperanto_ci
SET @@session.collation_server = ucs2_hungarian_ci;
Warnings:
Warning	4079	'ucs2_hungarian_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
ucs2_hungarian_ci
SET @@session.collation_server = ucs2_unicode_520_ci;
Warnings:
Warning	4079	'ucs2_unicode_520_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
ucs2_unicode_520_ci
SET @@session.collation_server = cp866_general_ci;
SELECT @@session.collation_server;
@@session.collation_server
cp866_general_ci
SET @@session.collation_server = cp866_bin;
SELECT @@session.collation_server;
@@session.collation_server
cp866_bin
SET @@session.collation_server = keybcs2_general_ci;
SELECT @@session.collation_server;
@@session.collation_server
keybcs2_general_ci
SET @@session.collation_server = keybcs2_bin;
SELECT @@session.collation_server;
@@session.collation_server
keybcs2_bin
SET @@session.collation_server = macce_general_ci;
Warnings:
Warning	4079	'macce_general_ci' is a collation of the deprecated character set macce. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
macce_general_ci
SET @@session.collation_server = macce_bin;
Warnings:
Warning	4079	'macce_bin' is a collation of the deprecated character set macce. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
macce_bin
SET @@session.collation_server = macroman_general_ci;
Warnings:
Warning	4079	'macroman_general_ci' is a collation of the deprecated character set macroman. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
macroman_general_ci
SET @@session.collation_server = macroman_bin;
Warnings:
Warning	4079	'macroman_bin' is a collation of the deprecated character set macroman. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@session.collation_server;
@@session.collation_server
macroman_bin
SET @@session.collation_server = cp852_general_ci;
SELECT @@session.collation_server;
@@session.collation_server
cp852_general_ci
SET @@session.collation_server = cp852_bin;
SELECT @@session.collation_server;
@@session.collation_server
cp852_bin
SET @@session.collation_server = latin7_estonian_cs;
SELECT @@session.collation_server;
@@session.collation_server
latin7_estonian_cs
SET @@session.collation_server = latin7_general_ci;
SELECT @@session.collation_server;
@@session.collation_server
latin7_general_ci
SET @@session.collation_server = latin7_general_cs;
SELECT @@session.collation_server;
@@session.collation_server
latin7_general_cs
SET @@session.collation_server = latin7_bin;
SELECT @@session.collation_server;
@@session.collation_server
latin7_bin
SET @@session.collation_server = cp1251_bulgarian_ci;
SELECT @@session.collation_server;
@@session.collation_server
cp1251_bulgarian_ci
SET @@session.collation_server = cp1251_ukrainian_ci;
SELECT @@session.collation_server;
@@session.collation_server
cp1251_ukrainian_ci
SET @@session.collation_server = cp1251_bin;
SELECT @@session.collation_server;
@@session.collation_server
cp1251_bin
SET @@session.collation_server = cp1251_general_ci;
SELECT @@session.collation_server;
@@session.collation_server
cp1251_general_ci
SET @@session.collation_server = cp1251_general_cs;
SELECT @@session.collation_server;
@@session.collation_server
cp1251_general_cs
SET @@session.collation_server = cp1256_general_ci;
SELECT @@session.collation_server;
@@session.collation_server
cp1256_general_ci
SET @@session.collation_server = cp1256_bin;
SELECT @@session.collation_server;
@@session.collation_server
cp1256_bin
SET @@session.collation_server = cp1257_lithuanian_ci;
SELECT @@session.collation_server;
@@session.collation_server
cp1257_lithuanian_ci
SET @@session.collation_server = cp1257_bin;
SELECT @@session.collation_server;
@@session.collation_server
cp1257_bin
SET @@session.collation_server = cp1257_general_ci;
SELECT @@session.collation_server;
@@session.collation_server
cp1257_general_ci
SET @@session.collation_server = binary;
SELECT @@session.collation_server;
@@session.collation_server
binary
SET @@session.collation_server = geostd8_general_ci;
SELECT @@session.collation_server;
@@session.collation_server
geostd8_general_ci
SET @@session.collation_server = geostd8_bin;
SELECT @@session.collation_server;
@@session.collation_server
geostd8_bin
SET @@session.collation_server = cp932_japanese_ci;
SELECT @@session.collation_server;
@@session.collation_server
cp932_japanese_ci
SET @@session.collation_server = cp932_bin;
SELECT @@session.collation_server;
@@session.collation_server
cp932_bin
SET @@session.collation_server = eucjpms_japanese_ci;
SELECT @@session.collation_server;
@@session.collation_server
eucjpms_japanese_ci
SET @@session.collation_server = eucjpms_bin;
SELECT @@session.collation_server;
@@session.collation_server
eucjpms_bin
SET @@session.collation_server = utf8mb4_0900_ai_ci;
SELECT @@session.collation_server;
@@session.collation_server
utf8mb4_0900_ai_ci
'#--------------------FN_DYNVARS_014_07-------------------------#'
SET @@global.collation_server = big5_chinese_ci;
SELECT @@global.collation_server;
@@global.collation_server
big5_chinese_ci
SET @@global.collation_server = big5_bin;
SELECT @@global.collation_server;
@@global.collation_server
big5_bin
SET @@global.collation_server = dec8_swedish_ci;
Warnings:
Warning	4079	'dec8_swedish_ci' is a collation of the deprecated character set dec8. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
dec8_swedish_ci
SET @@global.collation_server = dec8_bin;
Warnings:
Warning	4079	'dec8_bin' is a collation of the deprecated character set dec8. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
dec8_bin
SET @@global.collation_server = cp850_general_ci;
SELECT @@global.collation_server;
@@global.collation_server
cp850_general_ci
SET @@global.collation_server = cp850_bin;
SELECT @@global.collation_server;
@@global.collation_server
cp850_bin
SET @@global.collation_server = hp8_english_ci;
Warnings:
Warning	4079	'hp8_english_ci' is a collation of the deprecated character set hp8. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
hp8_english_ci
SET @@global.collation_server = hp8_bin;
Warnings:
Warning	4079	'hp8_bin' is a collation of the deprecated character set hp8. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
hp8_bin
SET @@global.collation_server = koi8r_general_ci;
SELECT @@global.collation_server;
@@global.collation_server
koi8r_general_ci
SET @@global.collation_server = koi8r_bin;
SELECT @@global.collation_server;
@@global.collation_server
koi8r_bin
SET @@global.collation_server = latin1_german1_ci;
SELECT @@global.collation_server;
@@global.collation_server
latin1_german1_ci
SET @@global.collation_server = latin1_swedish_ci;
SELECT @@global.collation_server;
@@global.collation_server
latin1_swedish_ci
SET @@global.collation_server = latin1_danish_ci;
SELECT @@global.collation_server;
@@global.collation_server
latin1_danish_ci
SET @@global.collation_server = latin1_german2_ci;
SELECT @@global.collation_server;
@@global.collation_server
latin1_german2_ci
SET @@global.collation_server = latin1_bin;
SELECT @@global.collation_server;
@@global.collation_server
latin1_bin
SET @@global.collation_server = latin1_general_ci;
SELECT @@global.collation_server;
@@global.collation_server
latin1_general_ci
SET @@global.collation_server = latin1_general_cs;
SELECT @@global.collation_server;
@@global.collation_server
latin1_general_cs
SET @@global.collation_server = latin1_spanish_ci;
SELECT @@global.collation_server;
@@global.collation_server
latin1_spanish_ci
SET @@global.collation_server = latin2_czech_cs;
SELECT @@global.collation_server;
@@global.collation_server
latin2_czech_cs
SET @@global.collation_server = latin2_general_ci;
SELECT @@global.collation_server;
@@global.collation_server
latin2_general_ci
SET @@global.collation_server = latin2_hungarian_ci;
SELECT @@global.collation_server;
@@global.collation_server
latin2_hungarian_ci
SET @@global.collation_server = latin2_croatian_ci;
SELECT @@global.collation_server;
@@global.collation_server
latin2_croatian_ci
SET @@global.collation_server = latin2_bin;
SELECT @@global.collation_server;
@@global.collation_server
latin2_bin
SET @@global.collation_server = swe7_swedish_ci;
SELECT @@global.collation_server;
@@global.collation_server
swe7_swedish_ci
SET @@global.collation_server = swe7_bin;
SELECT @@global.collation_server;
@@global.collation_server
swe7_bin
SET @@global.collation_server = ascii_general_ci;
SELECT @@global.collation_server;
@@global.collation_server
ascii_general_ci
SET @@global.collation_server = ascii_bin;
SELECT @@global.collation_server;
@@global.collation_server
ascii_bin
SET @@global.collation_server = ujis_japanese_ci;
SELECT @@global.collation_server;
@@global.collation_server
ujis_japanese_ci
SET @@global.collation_server = ujis_bin;
SELECT @@global.collation_server;
@@global.collation_server
ujis_bin
SET @@global.collation_server = sjis_japanese_ci;
SELECT @@global.collation_server;
@@global.collation_server
sjis_japanese_ci
SET @@global.collation_server = sjis_bin;
SELECT @@global.collation_server;
@@global.collation_server
sjis_bin
SET @@global.collation_server = hebrew_general_ci;
SELECT @@global.collation_server;
@@global.collation_server
hebrew_general_ci
SET @@global.collation_server = hebrew_bin;
SELECT @@global.collation_server;
@@global.collation_server
hebrew_bin
SET @@global.collation_server = tis620_thai_ci;
SELECT @@global.collation_server;
@@global.collation_server
tis620_thai_ci
SET @@global.collation_server = tis620_bin;
SELECT @@global.collation_server;
@@global.collation_server
tis620_bin
SET @@global.collation_server = euckr_korean_ci;
SELECT @@global.collation_server;
@@global.collation_server
euckr_korean_ci
SET @@global.collation_server = euckr_bin;
SELECT @@global.collation_server;
@@global.collation_server
euckr_bin
SET @@global.collation_server = koi8u_general_ci;
SELECT @@global.collation_server;
@@global.collation_server
koi8u_general_ci
SET @@global.collation_server = koi8u_bin;
SELECT @@global.collation_server;
@@global.collation_server
koi8u_bin
SET @@global.collation_server = gb2312_chinese_ci;
SELECT @@global.collation_server;
@@global.collation_server
gb2312_chinese_ci
SET @@global.collation_server = gb2312_bin;
SELECT @@global.collation_server;
@@global.collation_server
gb2312_bin
SET @@global.collation_server = greek_general_ci;
SELECT @@global.collation_server;
@@global.collation_server
greek_general_ci
SET @@global.collation_server = greek_bin;
SELECT @@global.collation_server;
@@global.collation_server
greek_bin
SET @@global.collation_server = cp1250_general_ci;
SELECT @@global.collation_server;
@@global.collation_server
cp1250_general_ci
SET @@global.collation_server = cp1250_czech_cs;
SELECT @@global.collation_server;
@@global.collation_server
cp1250_czech_cs
SET @@global.collation_server = cp1250_croatian_ci;
SELECT @@global.collation_server;
@@global.collation_server
cp1250_croatian_ci
SET @@global.collation_server = cp1250_bin;
SELECT @@global.collation_server;
@@global.collation_server
cp1250_bin
SET @@global.collation_server = cp1250_polish_ci;
SELECT @@global.collation_server;
@@global.collation_server
cp1250_polish_ci
SET @@global.collation_server = gbk_chinese_ci;
SELECT @@global.collation_server;
@@global.collation_server
gbk_chinese_ci
SET @@global.collation_server = gbk_bin;
SELECT @@global.collation_server;
@@global.collation_server
gbk_bin
SET @@global.collation_server = gb18030_chinese_ci;
SELECT @@global.collation_server;
@@global.collation_server
gb18030_chinese_ci
SET @@global.collation_server = gb18030_bin;
SELECT @@global.collation_server;
@@global.collation_server
gb18030_bin
SET @@global.collation_server = latin5_turkish_ci;
SELECT @@global.collation_server;
@@global.collation_server
latin5_turkish_ci
SET @@global.collation_server = latin5_bin;
SELECT @@global.collation_server;
@@global.collation_server
latin5_bin
SET @@global.collation_server = armscii8_general_ci;
SELECT @@global.collation_server;
@@global.collation_server
armscii8_general_ci
SET @@global.collation_server = armscii8_bin;
SELECT @@global.collation_server;
@@global.collation_server
armscii8_bin
SET @@global.collation_server = utf8_general_ci;
Warnings:
Warning	3778	'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
utf8mb3_general_ci
SET @@global.collation_server = utf8_bin;
Warnings:
Warning	3778	'utf8mb3_bin' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
utf8mb3_bin
SET @@global.collation_server = utf8_unicode_ci;
Warnings:
Warning	3778	'utf8mb3_unicode_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
utf8mb3_unicode_ci
SET @@global.collation_server = utf8_icelandic_ci;
Warnings:
Warning	3778	'utf8mb3_icelandic_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
utf8mb3_icelandic_ci
SET @@global.collation_server = utf8_latvian_ci;
Warnings:
Warning	3778	'utf8mb3_latvian_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
utf8mb3_latvian_ci
SET @@global.collation_server = utf8_romanian_ci;
Warnings:
Warning	3778	'utf8mb3_romanian_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
utf8mb3_romanian_ci
SET @@global.collation_server = utf8_slovenian_ci;
Warnings:
Warning	3778	'utf8mb3_slovenian_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
utf8mb3_slovenian_ci
SET @@global.collation_server = utf8_polish_ci;
Warnings:
Warning	3778	'utf8mb3_polish_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
utf8mb3_polish_ci
SET @@global.collation_server = utf8_estonian_ci;
Warnings:
Warning	3778	'utf8mb3_estonian_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
utf8mb3_estonian_ci
SET @@global.collation_server = utf8_spanish_ci;
Warnings:
Warning	3778	'utf8mb3_spanish_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
utf8mb3_spanish_ci
SET @@global.collation_server = utf8_swedish_ci;
Warnings:
Warning	3778	'utf8mb3_swedish_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
utf8mb3_swedish_ci
SET @@global.collation_server = utf8_turkish_ci;
Warnings:
Warning	3778	'utf8mb3_turkish_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
utf8mb3_turkish_ci
SET @@global.collation_server = utf8_czech_ci;
Warnings:
Warning	3778	'utf8mb3_czech_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
utf8mb3_czech_ci
SET @@global.collation_server = utf8_danish_ci;
Warnings:
Warning	3778	'utf8mb3_danish_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
utf8mb3_danish_ci
SET @@global.collation_server = utf8_lithuanian_ci;
Warnings:
Warning	3778	'utf8mb3_lithuanian_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
utf8mb3_lithuanian_ci
SET @@global.collation_server = utf8_slovak_ci;
Warnings:
Warning	3778	'utf8mb3_slovak_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
utf8mb3_slovak_ci
SET @@global.collation_server = utf8_spanish2_ci;
Warnings:
Warning	3778	'utf8mb3_spanish2_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
utf8mb3_spanish2_ci
SET @@global.collation_server = utf8_roman_ci;
Warnings:
Warning	3778	'utf8mb3_roman_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
utf8mb3_roman_ci
SET @@global.collation_server = utf8_persian_ci;
Warnings:
Warning	3778	'utf8mb3_persian_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
utf8mb3_persian_ci
SET @@global.collation_server = utf8_esperanto_ci;
Warnings:
Warning	3778	'utf8mb3_esperanto_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
utf8mb3_esperanto_ci
SET @@global.collation_server = utf8_hungarian_ci;
Warnings:
Warning	3778	'utf8mb3_hungarian_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
utf8mb3_hungarian_ci
SET @@global.collation_server = utf8_unicode_520_ci;
Warnings:
Warning	3778	'utf8mb3_unicode_520_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
utf8mb3_unicode_520_ci
SET @@global.collation_server = ucs2_general_ci;
Warnings:
Warning	4079	'ucs2_general_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
ucs2_general_ci
SET @@global.collation_server = ucs2_bin;
Warnings:
Warning	4079	'ucs2_bin' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
ucs2_bin
SET @@global.collation_server = ucs2_unicode_ci;
Warnings:
Warning	4079	'ucs2_unicode_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
ucs2_unicode_ci
SET @@global.collation_server = ucs2_icelandic_ci;
Warnings:
Warning	4079	'ucs2_icelandic_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
ucs2_icelandic_ci
SET @@global.collation_server = ucs2_latvian_ci;
Warnings:
Warning	4079	'ucs2_latvian_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
ucs2_latvian_ci
SET @@global.collation_server = ucs2_romanian_ci;
Warnings:
Warning	4079	'ucs2_romanian_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
ucs2_romanian_ci
SET @@global.collation_server = ucs2_slovenian_ci;
Warnings:
Warning	4079	'ucs2_slovenian_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
ucs2_slovenian_ci
SET @@global.collation_server = ucs2_polish_ci;
Warnings:
Warning	4079	'ucs2_polish_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
ucs2_polish_ci
SET @@global.collation_server = ucs2_estonian_ci;
Warnings:
Warning	4079	'ucs2_estonian_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
ucs2_estonian_ci
SET @@global.collation_server = ucs2_spanish_ci;
Warnings:
Warning	4079	'ucs2_spanish_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
ucs2_spanish_ci
SET @@global.collation_server = ucs2_swedish_ci;
Warnings:
Warning	4079	'ucs2_swedish_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
ucs2_swedish_ci
SET @@global.collation_server = ucs2_turkish_ci;
Warnings:
Warning	4079	'ucs2_turkish_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
ucs2_turkish_ci
SET @@global.collation_server = ucs2_czech_ci;
Warnings:
Warning	4079	'ucs2_czech_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
ucs2_czech_ci
SET @@global.collation_server = ucs2_danish_ci;
Warnings:
Warning	4079	'ucs2_danish_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
ucs2_danish_ci
SET @@global.collation_server = ucs2_lithuanian_ci;
Warnings:
Warning	4079	'ucs2_lithuanian_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
ucs2_lithuanian_ci
SET @@global.collation_server = ucs2_slovak_ci;
Warnings:
Warning	4079	'ucs2_slovak_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
ucs2_slovak_ci
SET @@global.collation_server = ucs2_spanish2_ci;
Warnings:
Warning	4079	'ucs2_spanish2_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
ucs2_spanish2_ci
SET @@global.collation_server = ucs2_roman_ci;
Warnings:
Warning	4079	'ucs2_roman_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
ucs2_roman_ci
SET @@global.collation_server = ucs2_persian_ci;
Warnings:
Warning	4079	'ucs2_persian_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
ucs2_persian_ci
SET @@global.collation_server = ucs2_esperanto_ci;
Warnings:
Warning	4079	'ucs2_esperanto_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
ucs2_esperanto_ci
SET @@global.collation_server = ucs2_hungarian_ci;
Warnings:
Warning	4079	'ucs2_hungarian_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
ucs2_hungarian_ci
SET @@global.collation_server = ucs2_unicode_520_ci;
Warnings:
Warning	4079	'ucs2_unicode_520_ci' is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
ucs2_unicode_520_ci
SET @@global.collation_server = cp866_general_ci;
SELECT @@global.collation_server;
@@global.collation_server
cp866_general_ci
SET @@global.collation_server = cp866_bin;
SELECT @@global.collation_server;
@@global.collation_server
cp866_bin
SET @@global.collation_server = keybcs2_general_ci;
SELECT @@global.collation_server;
@@global.collation_server
keybcs2_general_ci
SET @@global.collation_server = keybcs2_bin;
SELECT @@global.collation_server;
@@global.collation_server
keybcs2_bin
SET @@global.collation_server = macce_general_ci;
Warnings:
Warning	4079	'macce_general_ci' is a collation of the deprecated character set macce. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
macce_general_ci
SET @@global.collation_server = macce_bin;
Warnings:
Warning	4079	'macce_bin' is a collation of the deprecated character set macce. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
macce_bin
SET @@global.collation_server = macroman_general_ci;
Warnings:
Warning	4079	'macroman_general_ci' is a collation of the deprecated character set macroman. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
macroman_general_ci
SET @@global.collation_server = macroman_bin;
Warnings:
Warning	4079	'macroman_bin' is a collation of the deprecated character set macroman. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
macroman_bin
SET @@global.collation_server = cp852_general_ci;
SELECT @@global.collation_server;
@@global.collation_server
cp852_general_ci
SET @@global.collation_server = cp852_bin;
SELECT @@global.collation_server;
@@global.collation_server
cp852_bin
SET @@global.collation_server = latin7_estonian_cs;
SELECT @@global.collation_server;
@@global.collation_server
latin7_estonian_cs
SET @@global.collation_server = latin7_general_ci;
SELECT @@global.collation_server;
@@global.collation_server
latin7_general_ci
SET @@global.collation_server = latin7_general_cs;
SELECT @@global.collation_server;
@@global.collation_server
latin7_general_cs
SET @@global.collation_server = latin7_bin;
SELECT @@global.collation_server;
@@global.collation_server
latin7_bin
SET @@global.collation_server = cp1251_bulgarian_ci;
SELECT @@global.collation_server;
@@global.collation_server
cp1251_bulgarian_ci
SET @@global.collation_server = cp1251_ukrainian_ci;
SELECT @@global.collation_server;
@@global.collation_server
cp1251_ukrainian_ci
SET @@global.collation_server = cp1251_bin;
SELECT @@global.collation_server;
@@global.collation_server
cp1251_bin
SET @@global.collation_server = cp1251_general_ci;
SELECT @@global.collation_server;
@@global.collation_server
cp1251_general_ci
SET @@global.collation_server = cp1251_general_cs;
SELECT @@global.collation_server;
@@global.collation_server
cp1251_general_cs
SET @@global.collation_server = cp1256_general_ci;
SELECT @@global.collation_server;
@@global.collation_server
cp1256_general_ci
SET @@global.collation_server = cp1256_bin;
SELECT @@global.collation_server;
@@global.collation_server
cp1256_bin
SET @@global.collation_server = cp1257_lithuanian_ci;
SELECT @@global.collation_server;
@@global.collation_server
cp1257_lithuanian_ci
SET @@global.collation_server = cp1257_bin;
SELECT @@global.collation_server;
@@global.collation_server
cp1257_bin
SET @@global.collation_server = cp1257_general_ci;
SELECT @@global.collation_server;
@@global.collation_server
cp1257_general_ci
SET @@global.collation_server = binary;
SELECT @@global.collation_server;
@@global.collation_server
binary
SET @@global.collation_server = geostd8_general_ci;
SELECT @@global.collation_server;
@@global.collation_server
geostd8_general_ci
SET @@global.collation_server = geostd8_bin;
SELECT @@global.collation_server;
@@global.collation_server
geostd8_bin
SET @@global.collation_server = cp932_japanese_ci;
SELECT @@global.collation_server;
@@global.collation_server
cp932_japanese_ci
SET @@global.collation_server = cp932_bin;
SELECT @@global.collation_server;
@@global.collation_server
cp932_bin
SET @@global.collation_server = eucjpms_japanese_ci;
SELECT @@global.collation_server;
@@global.collation_server
eucjpms_japanese_ci
SET @@global.collation_server = eucjpms_bin;
SELECT @@global.collation_server;
@@global.collation_server
eucjpms_bin
SET @@global.collation_server = utf8mb4_0900_ai_ci;
SELECT @@global.collation_server;
@@global.collation_server
utf8mb4_0900_ai_ci
'#--------------------FN_DYNVARS_014_08-------------------------#'
SET @@collation_server = LATIN7_GENERAL_CS;
SELECT @@collation_server;
@@collation_server
latin7_general_cs
SET @@collation_server = latin7_general_cs;
SELECT @@collation_server;
@@collation_server
latin7_general_cs
SET @@global.collation_server = Latin7_GeneRal_cS;
SELECT @@global.collation_server;
@@global.collation_server
latin7_general_cs
'#--------------------FN_DYNVARS_014_09-------------------------#'
SET @@collation_server = 1;
SELECT @@collation_server;
@@collation_server
big5_chinese_ci
SET @@collation_server = 2;
SELECT @@collation_server;
@@collation_server
latin2_czech_cs
SET @@collation_server = 3;
Warnings:
Warning	4079	'dec8_swedish_ci' is a collation of the deprecated character set dec8. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@collation_server;
@@collation_server
dec8_swedish_ci
SET @@collation_server = 99;
SELECT @@collation_server;
@@collation_server
cp1250_polish_ci
SET @@collation_server = 100;
ERROR HY000: Unknown collation: '100'
SET @@global.collation_server = 1;
SELECT @@global.collation_server;
@@global.collation_server
big5_chinese_ci
SET @@global.collation_server = 2;
SELECT @@global.collation_server;
@@global.collation_server
latin2_czech_cs
SET @@global.collation_server = 3;
Warnings:
Warning	4079	'dec8_swedish_ci' is a collation of the deprecated character set dec8. Please consider using utf8mb4 with an appropriate collation instead.
SELECT @@global.collation_server;
@@global.collation_server
dec8_swedish_ci
SET @@global.collation_server = 99;
SELECT @@global.collation_server;
@@global.collation_server
cp1250_polish_ci
SET @@global.collation_server = 100;
ERROR HY000: Unknown collation: '100'
SET @total_collations = (SELECT count(*) FROM INFORMATION_SCHEMA.COLLATIONS);
SELECT @total_collations > 120;
@total_collations > 120
1
'#--------------------FN_DYNVARS_014_10-------------------------#'
SET @@collation_server = latin7_binary;
ERROR HY000: Unknown collation: 'latin7_binary'
SET @@collation_server = 'eucjpms_japanese_cs';
ERROR HY000: Unknown collation: 'eucjpms_japanese_cs'
SET @@collation_server = 0;
ERROR HY000: Unknown collation: '0'
SET @@collation_server = 1.01;
ERROR 42000: Incorrect argument type to variable 'collation_server'
SET @@collation_server = -1;
ERROR HY000: Unknown collation: '-1'
SET @@collation_server = '';
ERROR HY000: Unknown collation: ''
SET @@collation_server = ' eucjpms_bin';
ERROR HY000: Unknown collation: ' eucjpms_bin'
SET @@collation_server = true;
SELECT @@collation_server AS res_with_true;
res_with_true
big5_chinese_ci
SET @@collation_server = ON;
ERROR HY000: Unknown collation: 'ON'
SET @@collation_server = repeat('e', 256);
ERROR HY000: Unknown collation: 'eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee'
SET @@collation_server = repeat('e', 1024);
ERROR HY000: Unknown collation: 'eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee'
'#--------------------FN_DYNVARS_014_11-------------------------#'
SET @@global.collation_server = latin7_binary;
ERROR HY000: Unknown collation: 'latin7_binary'
SET @@global.collation_server = 'eucjpms_japanese_cs';
ERROR HY000: Unknown collation: 'eucjpms_japanese_cs'
SET @@global.collation_server = 0;
ERROR HY000: Unknown collation: '0'
SET @@global.collation_server = 1.1;
ERROR 42000: Incorrect argument type to variable 'collation_server'
SET @@global.collation_server = -1;
ERROR HY000: Unknown collation: '-1'
SET @@global.collation_server = "";
ERROR HY000: Unknown collation: ''
SET @@global.collation_server = ' eucjpms_bin';
ERROR HY000: Unknown collation: ' eucjpms_bin'
SET @@global.collation_server = true;
SET @@global.collation_server = ON;
ERROR HY000: Unknown collation: 'ON'
SET @@global.collation_server = repeat('e', 256);
ERROR HY000: Unknown collation: 'eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee'
SET @@global.collation_server = repeat('e', 1024);
ERROR HY000: Unknown collation: 'eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee'
'#--------------------FN_DYNVARS_014_12-------------------------#'
SELECT @@global.collation_server =
(SELECT VARIABLE_VALUE FROM performance_schema.global_variables
WHERE VARIABLE_NAME='collation_server') AS res;
res
1
SET @@global.collation_server = 1;
SELECT @@global.collation_server;
@@global.collation_server
big5_chinese_ci
SELECT @@global.collation_server =
(SELECT VARIABLE_VALUE FROM performance_schema.global_variables
WHERE VARIABLE_NAME='collation_server') AS res;
res
1
'#--------------------FN_DYNVARS_014_13-------------------------#'
SELECT @@collation_server =
(SELECT VARIABLE_VALUE FROM performance_schema.session_variables
WHERE VARIABLE_NAME='collation_server') AS res;
res
1
SELECT @@local.collation_server =
(SELECT VARIABLE_VALUE FROM performance_schema.session_variables
WHERE VARIABLE_NAME='collation_server') AS res;
res
1
SELECT @@session.collation_server =
(SELECT VARIABLE_VALUE FROM performance_schema.session_variables
WHERE VARIABLE_NAME='collation_server') AS res;
res
1
SET @@global.collation_server = @global_start_value;
SELECT @@global.collation_server;
@@global.collation_server
utf8mb4_0900_ai_ci
SET @@session.collation_server = @session_start_value;
SELECT @@session.collation_server;
@@session.collation_server
utf8mb4_0900_ai_ci
