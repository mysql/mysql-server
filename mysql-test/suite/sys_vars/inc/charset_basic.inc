# 
# This auxiliary script is used by character set test cases.
#



--Error 0,ER_UNKNOWN_CHARACTER_SET
eval SET $charset_variable = big5;
eval SELECT $charset_variable;
eval SET $charset_variable = dec8;
eval SELECT $charset_variable;
eval SET $charset_variable = cp850;
eval SELECT $charset_variable;
eval SET $charset_variable = hp8;
eval SELECT $charset_variable;
eval SET $charset_variable = koi8r;
eval SELECT $charset_variable;
eval SET $charset_variable = latin1;
eval SELECT $charset_variable;
eval SET $charset_variable = latin2;
eval SELECT $charset_variable;
eval SET $charset_variable = swe7;
eval SELECT $charset_variable;
eval SET $charset_variable = ascii;
eval SELECT $charset_variable;
eval SET $charset_variable = ujis;
eval SELECT $charset_variable;
eval SET $charset_variable = sjis;
eval SELECT $charset_variable;
eval SET $charset_variable = hebrew;
eval SELECT $charset_variable;
eval SET $charset_variable = tis620;
eval SELECT $charset_variable;
eval SET $charset_variable = euckr;
eval SELECT $charset_variable;
eval SET $charset_variable = koi8u;
eval SELECT $charset_variable;
eval SET $charset_variable = gb2312;
eval SELECT $charset_variable;
eval SET $charset_variable = greek;
eval SELECT $charset_variable;
eval SET $charset_variable = cp1250;
eval SELECT $charset_variable;
eval SET $charset_variable = gbk;
eval SELECT $charset_variable;
eval SET $charset_variable = gb18030;
eval SELECT $charset_variable;
eval SET $charset_variable = latin5;
eval SELECT $charset_variable;
eval SET $charset_variable = armscii8;
eval SELECT $charset_variable;
eval SET $charset_variable = utf8mb3;
eval SELECT $charset_variable;
eval SET $charset_variable = utf8mb4;
eval SELECT $charset_variable;
# Error with Linux
--error 0,ER_WRONG_VALUE_FOR_VAR
eval SET $charset_variable = ucs2;
eval SELECT $charset_variable;
eval SET $charset_variable = cp866;
eval SELECT $charset_variable;
eval SET $charset_variable = keybcs2;
eval SELECT $charset_variable;
eval SET $charset_variable = macce;
eval SELECT $charset_variable;
eval SET $charset_variable = macroman;
eval SELECT $charset_variable;
eval SET $charset_variable = cp852;
eval SELECT $charset_variable;
eval SET $charset_variable = latin7;
eval SELECT $charset_variable;
eval SET $charset_variable = cp1251;
eval SELECT $charset_variable;
eval SET $charset_variable = cp1256;
eval SELECT $charset_variable;
eval SET $charset_variable = cp1257;
eval SELECT $charset_variable;
eval SET $charset_variable = binary;
eval SELECT $charset_variable;
eval SET $charset_variable = geostd8;
eval SELECT $charset_variable;
eval SET $charset_variable = cp932;
eval SELECT $charset_variable;
eval SET $charset_variable = eucjpms;
eval SELECT $charset_variable;
