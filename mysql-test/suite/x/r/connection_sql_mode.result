1. Check connection for SQL_MODE = DEFAULT
RUN select 1 as value

value
1
0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
2. Check connection for SQL_MODE = DEFAULT + PAD_CHAR_TO_FULL_LENGTH
SELECT CONCAT_WS(',', @@global.SQL_MODE, 'PAD_CHAR_TO_FULL_LENGTH') INTO @MODES;
SET @@GLOBAL.SQL_MODE=@MODES;
Warnings:
Warning	3090	Changing sql mode 'PAD_CHAR_TO_FULL_LENGTH' is deprecated. It will be removed in a future release.
RUN select 2 as value

value
2
0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
3. Check connection for SQL_MODE set modes taken from bug log
SET @@GLOBAL.SQL_MODE='ANSI_QUOTES,NO_AUTO_VALUE_ON_ZERO,NO_BACKSLASH_ESCAPES,NO_DIR_IN_CREATE,NO_ZERO_DATE,PAD_CHAR_TO_FULL_LENGTH';
Warnings:
Warning	3135	'NO_ZERO_DATE', 'NO_ZERO_IN_DATE' and 'ERROR_FOR_DIVISION_BY_ZERO' sql modes should be used with strict mode. They will be merged with strict mode in a future release.
Warning	3090	Changing sql mode 'PAD_CHAR_TO_FULL_LENGTH' is deprecated. It will be removed in a future release.
RUN select 3 as value

value
3
0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
4. Check connection for SQL_MODE back to DEFAULT
SET @@GLOBAL.SQL_MODE=DEFAULT;
RUN select 4 as value

value
4
0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
