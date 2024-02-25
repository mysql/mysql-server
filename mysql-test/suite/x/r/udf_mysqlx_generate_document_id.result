CREATE SCHEMA xtest;
USE xtest;
CREATE TABLE util(f INT);
INSERT INTO util(f) VALUES(1),(2),(3),(4),(5);
SELECT mysqlx_generate_document_id() FROM util;
mysqlx_generate_document_id()
0000XXXXXXXX0000000000000001
0000XXXXXXXX0000000000000002
0000XXXXXXXX0000000000000003
0000XXXXXXXX0000000000000004
0000XXXXXXXX0000000000000005
SELECT mysqlx_generate_document_id(16) FROM util;
mysqlx_generate_document_id(16)
0000XXXXXXXX0000000000000006
0000XXXXXXXX0000000000000007
0000XXXXXXXX0000000000000008
0000XXXXXXXX0000000000000009
0000XXXXXXXX000000000000000a
SELECT mysqlx_generate_document_id(16, 8) FROM util;
mysqlx_generate_document_id(16, 8)
0000XXXXXXXX0000000000000010
0000XXXXXXXX0000000000000018
0000XXXXXXXX0000000000000020
0000XXXXXXXX0000000000000028
0000XXXXXXXX0000000000000030
SELECT mysqlx_generate_document_id(16, 8, 0) FROM util;
mysqlx_generate_document_id(16, 8, 0)
0000XXXXXXXX0000000000000038
0000XXXXXXXX0000000000000040
0000XXXXXXXX0000000000000048
0000XXXXXXXX0000000000000050
0000XXXXXXXX0000000000000058
SELECT mysqlx_generate_document_id(16, 8, 1) FROM util;
mysqlx_generate_document_id(16, 8, 1)
NULL
NULL
NULL
NULL
NULL
set auto_increment_offset = 16;
set auto_increment_increment = 8;
SELECT mysqlx_generate_document_id(@@auto_increment_offset, @@auto_increment_increment) FROM util;
mysqlx_generate_document_id(@@auto_increment_offset, @@auto_increment_increment)
0000XXXXXXXX0000000000000060
0000XXXXXXXX0000000000000068
0000XXXXXXXX0000000000000070
0000XXXXXXXX0000000000000078
0000XXXXXXXX0000000000000080
DO mysqlx_generate_document_id();
CALL mysqlx_generate_document_id();
ERROR 42000: PROCEDURE xtest.mysqlx_generate_document_id does not exist
DO mysqlx_generate_document_id('abc');
ERROR HY000: Can't initialize function 'mysqlx_generate_document_id'; Function expect integer argument
DO mysqlx_generate_document_id(NULL);
ERROR HY000: Can't initialize function 'mysqlx_generate_document_id'; Function expect integer argument
DO mysqlx_generate_document_id(123.45);
ERROR HY000: Can't initialize function 'mysqlx_generate_document_id'; Function expect integer argument
DO mysqlx_generate_document_id(1, 'abc');
ERROR HY000: Can't initialize function 'mysqlx_generate_document_id'; Function expect two integer arguments
DO mysqlx_generate_document_id(1, NULL);
ERROR HY000: Can't initialize function 'mysqlx_generate_document_id'; Function expect two integer arguments
DO mysqlx_generate_document_id(1, 123.45);
ERROR HY000: Can't initialize function 'mysqlx_generate_document_id'; Function expect two integer arguments
DO mysqlx_generate_document_id(1, 1, 'abc');
ERROR HY000: Can't initialize function 'mysqlx_generate_document_id'; Function expect three integer arguments
DO mysqlx_generate_document_id(1, 1, NULL);
ERROR HY000: Can't initialize function 'mysqlx_generate_document_id'; Function expect three integer arguments
DO mysqlx_generate_document_id(1, 1, 123.45);
ERROR HY000: Can't initialize function 'mysqlx_generate_document_id'; Function expect three integer arguments
DO mysqlx_generate_document_id(1, 1, 1, 1);
ERROR HY000: Can't initialize function 'mysqlx_generate_document_id'; Function expect up to three integer arguments
DO mysqlx_generate_document_id(1, 1, 1, 1, 1);
ERROR HY000: Can't initialize function 'mysqlx_generate_document_id'; Function expect up to three integer arguments
CREATE PROCEDURE my_procedure(IN offset INT, IN increment INT)
BEGIN
SELECT mysqlx_generate_document_id(offset, increment) AS document_id;
END ||
CALL my_procedure(16, 8);
document_id
0000XXXXXXXX0000000000000088
RUN SELECT mysqlx_generate_document_id() FROM xtest.util;
mysqlx_generate_document_id()
0000XXXXXXXX0000000000000089
0000XXXXXXXX000000000000008a
0000XXXXXXXX000000000000008b
0000XXXXXXXX000000000000008c
0000XXXXXXXX000000000000008d
0 rows affected
RUN SELECT mysqlx_generate_document_id(16) FROM xtest.util;
mysqlx_generate_document_id(16)
0000XXXXXXXX000000000000008e
0000XXXXXXXX000000000000008f
0000XXXXXXXX0000000000000090
0000XXXXXXXX0000000000000091
0000XXXXXXXX0000000000000092
0 rows affected
RUN SELECT mysqlx_generate_document_id(16, 8) FROM xtest.util;
mysqlx_generate_document_id(16, 8)
0000XXXXXXXX0000000000000098
0000XXXXXXXX00000000000000a0
0000XXXXXXXX00000000000000a8
0000XXXXXXXX00000000000000b0
0000XXXXXXXX00000000000000b8
0 rows affected
RUN SELECT mysqlx_generate_document_id(16, 8, 0) FROM xtest.util;
mysqlx_generate_document_id(16, 8, 0)
0000XXXXXXXX00000000000000c0
0000XXXXXXXX00000000000000c8
0000XXXXXXXX00000000000000d0
0000XXXXXXXX00000000000000d8
0000XXXXXXXX00000000000000e0
0 rows affected
RUN SELECT mysqlx_generate_document_id(16, 8, 1) FROM xtest.util;
mysqlx_generate_document_id(16, 8, 1)
null
null
null
null
null
0 rows affected
RUN set auto_increment_offset = 16

0 rows affected
RUN set auto_increment_increment = 8

0 rows affected
RUN SELECT mysqlx_generate_document_id(@@auto_increment_offset, @@auto_increment_increment) FROM xtest.util;
mysqlx_generate_document_id(@@auto_increment_offset, @@auto_increment_increment)
0000XXXXXXXX00000000000000e8
0000XXXXXXXX00000000000000f0
0000XXXXXXXX00000000000000f8
0000XXXXXXXX0000000000000100
0000XXXXXXXX0000000000000108
0 rows affected
RUN DO mysqlx_generate_document_id();

0 rows affected
RUN CALL mysqlx_generate_document_id();
Got expected error: No database selected (code 1046)
RUN USE xtest;

0 rows affected
RUN CALL mysqlx_generate_document_id();
Got expected error: PROCEDURE xtest.mysqlx_generate_document_id does not exist (code 1305)
RUN DO mysqlx_generate_document_id('abc');
Got expected error: Can't initialize function 'mysqlx_generate_document_id'; Function expect integer argument (code 1123)
RUN DO mysqlx_generate_document_id(NULL);
Got expected error: Can't initialize function 'mysqlx_generate_document_id'; Function expect integer argument (code 1123)
RUN DO mysqlx_generate_document_id(123.45);
Got expected error: Can't initialize function 'mysqlx_generate_document_id'; Function expect integer argument (code 1123)
RUN DO mysqlx_generate_document_id(1, 'abc');
Got expected error: Can't initialize function 'mysqlx_generate_document_id'; Function expect two integer arguments (code 1123)
RUN DO mysqlx_generate_document_id(1, NULL);
Got expected error: Can't initialize function 'mysqlx_generate_document_id'; Function expect two integer arguments (code 1123)
RUN DO mysqlx_generate_document_id(1, 123.45);
Got expected error: Can't initialize function 'mysqlx_generate_document_id'; Function expect two integer arguments (code 1123)
RUN DO mysqlx_generate_document_id(1, 1, 'abc');
Got expected error: Can't initialize function 'mysqlx_generate_document_id'; Function expect three integer arguments (code 1123)
RUN DO mysqlx_generate_document_id(1, 1, NULL);
Got expected error: Can't initialize function 'mysqlx_generate_document_id'; Function expect three integer arguments (code 1123)
RUN DO mysqlx_generate_document_id(1, 1, 123.45);
Got expected error: Can't initialize function 'mysqlx_generate_document_id'; Function expect three integer arguments (code 1123)
RUN DO mysqlx_generate_document_id(1, 1, 1, 1);
Got expected error: Can't initialize function 'mysqlx_generate_document_id'; Function expect up to three integer arguments (code 1123)
RUN DO mysqlx_generate_document_id(1, 1, 1, 1, 1);
Got expected error: Can't initialize function 'mysqlx_generate_document_id'; Function expect up to three integer arguments (code 1123)
RUN CALL my_procedure(16, 8);

document_id
0000XXXXXXXX0000000000000110

0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
SET @doc = '{"a":1}';
SELECT JSON_INSERT(@doc, '$.a', CONVERT(mysqlx_generate_document_id(1,1,JSON_CONTAINS_PATH(@doc, 'one', '$.a')) USING utf8mb4)) as doc;
doc
{"a": 1}
SELECT JSON_INSERT(@doc, '$.b', CONVERT(mysqlx_generate_document_id(1,1,JSON_CONTAINS_PATH(@doc, 'one', '$.b')) USING utf8mb4)) as doc;
doc
{"a": 1, "b": "0000XXXXXXXX0000000000000111"}
set auto_increment_increment = DEFAULT;
set auto_increment_offset = DEFAULT;
