INSTALL PLUGIN test_udf_extension_services SONAME 'TESTUDFEXTENSIONSERVICES';
CREATE TABLE test.country (utf varchar(40) CHARACTER SET utf8mb4,
cp12 varchar(40) CHARACTER SET cp1251,
lt1 VARCHAR(40) CHARACTER SET latin1);
INSERT INTO test.country(utf, cp12, lt1) VALUES
(UNHEX('E0A4ADE0A4BEE0A4B0E0A4A4'), UNHEX('C1FAEBE3E0F0E8FF'),
UNHEX('D6737465727265696368')),
(UNHEX('D0A0D0BED181D181D0B8D18F'), UNHEX('D1F0E1E8BCE0'),
UNHEX('42656C6769EB')),
(UNHEX('496E646961'), UNHEX('F3EAF0E0BFEDE0'), UNHEX('526F6DE26E6961'));
CREATE TABLE test.names(utf_name varchar(40), eng_name varchar(40))
CHARACTER SET utf8mb4;
INSERT INTO test.names(utf_name, eng_name) VALUES
(UNHEX('E0A4ADE0A4BEE0A4B0E0A4A4'), 'India'),
(UNHEX('D091D18AD0BBD0B3D0B0D180D0B8D18F'), 'Bulgaria'),
(UNHEX('C396737465727265696368'), 'Austria'),
(UNHEX('D0A0D0BED181D181D0B8D18F'), 'Russia'),
(UNHEX('D0A1D180D0B1D0B8D198D0B0'), 'Serbia'),
(UNHEX('42656C6769C3AB'), 'Belgium'),
(UNHEX('D183D0BAD180D0B0D197D0BDD0B0'), 'Ukraine'),
(UNHEX('526F6DC3A26E6961'), 'Romania');
#------------------------------------------------------------------------
# Tests that fetch the charset of second argument and apply that to the
# return value. This UDF returns first argument.
#------------------------------------------------------------------------
#
# 1.1 Without conversion the return values of UDF should not match with
#     ut8mb4 values.
SELECT HEX(utf_name) AS utf8_hex, HEX(lt1) as latin1_hex, eng_name
FROM test.names INNER JOIN test.country ON
HEX(test_result_charset(lt1, lt1)) = HEX(utf_name) ORDER BY eng_name;
utf8_hex	latin1_hex	eng_name

SELECT HEX(utf_name) AS utf8_hex, HEX(cp12) as cp1251_hex, eng_name
FROM test.names INNER JOIN test.country ON
HEX(test_result_charset(cp12, cp12)) = HEX(utf_name) ORDER BY eng_name;
utf8_hex	cp1251_hex	eng_name
#
# 1.2 After conversion the return values should match with ut8mb4 values
#
SELECT HEX(utf_name) AS utf8_hex, HEX(lt1) as latin1_hex, eng_name
FROM test.names INNER JOIN test.country ON
HEX(test_result_charset(lt1, utf)) = HEX(utf_name) ORDER BY eng_name;
utf8_hex	latin1_hex	eng_name
C396737465727265696368	D6737465727265696368	Austria
42656C6769C3AB	42656C6769EB	Belgium
526F6DC3A26E6961	526F6DE26E6961	Romania

SELECT HEX(utf_name) AS utf8_hex, HEX(cp12) as cp1251_hex, eng_name
FROM test.names INNER JOIN test.country ON
HEX(test_result_charset(cp12, utf)) = HEX(utf_name) ORDER BY eng_name;
utf8_hex	cp1251_hex	eng_name
D091D18AD0BBD0B3D0B0D180D0B8D18F	C1FAEBE3E0F0E8FF	Bulgaria
D0A1D180D0B1D0B8D198D0B0	D1F0E1E8BCE0	Serbia
D183D0BAD180D0B0D197D0BDD0B0	F3EAF0E0BFEDE0	Ukraine
#
# 1.3 Check the charset of the return value.
#
SELECT CHARSET(test_result_charset(cp12, cp12))FROM test.country;
CHARSET(test_result_charset(cp12, cp12))
cp1251
cp1251
cp1251
SELECT CHARSET(test_result_charset(lt1, lt1))FROM test.country;
CHARSET(test_result_charset(lt1, lt1))
latin1
latin1
latin1
# Should be changed to ut8mb4 charset
SELECT CHARSET(test_result_charset(cp12, utf))FROM test.country;
CHARSET(test_result_charset(cp12, utf))
utf8mb4
utf8mb4
utf8mb4
SELECT CHARSET(test_result_charset(lt1, utf))FROM test.country;
CHARSET(test_result_charset(lt1, utf))
utf8mb4
utf8mb4
utf8mb4
#------------------------------------------------------------------------
# Tests that fetch the collation of second argument and apply that to the
# return value. This UDF returns first argument.
#------------------------------------------------------------------------
#
# 2.1 Without conversion the return values of UDF should not match with
#     ut8mb4 values.
SELECT HEX(utf_name) AS utf8_hex, HEX(lt1) as lt1_hex, eng_name
FROM test.names INNER JOIN test.country ON
HEX(test_result_collation(lt1, lt1)) = HEX(utf_name) ORDER BY eng_name;
utf8_hex	lt1_hex	eng_name

SELECT HEX(utf_name) AS utf8_hex, HEX(cp12) as cp12_hex, eng_name
FROM test.names INNER JOIN test.country ON
HEX(test_result_collation(cp12, cp12)) = HEX(utf_name) ORDER BY eng_name;
utf8_hex	cp12_hex	eng_name
#
# 2.2 After conversion the return values should match with ut8mb4 values
#
SELECT HEX(utf_name) AS utf8_hex, HEX(lt1) as lt1_hex, eng_name
FROM test.names INNER JOIN test.country ON
HEX(test_result_collation(lt1, utf)) = HEX(utf_name) ORDER BY eng_name;
utf8_hex	lt1_hex	eng_name
C396737465727265696368	D6737465727265696368	Austria
42656C6769C3AB	42656C6769EB	Belgium
526F6DC3A26E6961	526F6DE26E6961	Romania

SELECT HEX(utf_name) AS utf8_hex, HEX(cp12) as cp12_hex, eng_name
FROM test.names INNER JOIN test.country ON
HEX(test_result_collation(cp12, utf)) = HEX(utf_name) ORDER BY eng_name;
utf8_hex	cp12_hex	eng_name
D091D18AD0BBD0B3D0B0D180D0B8D18F	C1FAEBE3E0F0E8FF	Bulgaria
D0A1D180D0B1D0B8D198D0B0	D1F0E1E8BCE0	Serbia
D183D0BAD180D0B0D197D0BDD0B0	F3EAF0E0BFEDE0	Ukraine

# 2.3  Check the charset of the value returned by the UDF
#
SELECT CHARSET(test_result_collation(cp12, cp12)) from test.country;
CHARSET(test_result_collation(cp12, cp12))
cp1251
cp1251
cp1251
SELECT CHARSET(test_result_collation(lt1, lt1)) from test.country;
CHARSET(test_result_collation(lt1, lt1))
latin1
latin1
latin1
# Should be changed to utf8mb4 charset
SELECT CHARSET(test_result_collation(cp12, utf)) from test.country;
CHARSET(test_result_collation(cp12, utf))
utf8mb4
utf8mb4
utf8mb4
SELECT CHARSET(test_result_collation(lt1, utf)) from test.country;
CHARSET(test_result_collation(lt1, utf))
utf8mb4
utf8mb4
utf8mb4
# 2.6  Check the collation of the value returned by the UDF
SELECT COLLATION(test_result_collation(cp12, cp12)) from test.country;
COLLATION(test_result_collation(cp12, cp12))
cp1251_general_ci
cp1251_general_ci
cp1251_general_ci
SELECT COLLATION(test_result_collation(lt1, lt1)) from test.country;
COLLATION(test_result_collation(lt1, lt1))
latin1_swedish_ci
latin1_swedish_ci
latin1_swedish_ci
# Should be changed to default utf8mb4 collation.
SELECT COLLATION(test_result_collation(cp12, utf)) from test.country;
COLLATION(test_result_collation(cp12, utf))
utf8mb4_0900_ai_ci
utf8mb4_0900_ai_ci
utf8mb4_0900_ai_ci
SELECT COLLATION(test_result_collation(lt1, utf)) from test.country;
COLLATION(test_result_collation(lt1, utf))
utf8mb4_0900_ai_ci
utf8mb4_0900_ai_ci
utf8mb4_0900_ai_ci

#------------------------------------------------------------------------
# Tests that fetch charset of second argument, apply that to first
# argument and return the same.
#------------------------------------------------------------------------
#
# 3.1 Without conversion the return values of UDF should not match with
#     ut8mb4 values.
SELECT HEX(utf_name) AS utf8_hex, HEX(lt1) as lt1_hex, eng_name
FROM test.names INNER JOIN test.country ON
HEX(test_args_charset(lt1, lt1)) = HEX(utf_name) ORDER BY eng_name;
utf8_hex	lt1_hex	eng_name

SELECT HEX(utf_name) AS utf8_hex, HEX(cp12) as cp12_hex, eng_name
FROM test.names INNER JOIN test.country ON
HEX(test_args_charset(cp12, cp12)) = HEX(utf_name) ORDER BY eng_name;
utf8_hex	cp12_hex	eng_name
#
# 3.2 After conversion the return values should match with ut8mb4 values
#
SELECT HEX(utf_name) AS utf8_hex, HEX(lt1) as lt1_hex, eng_name
FROM test.names INNER JOIN test.country ON
HEX(test_args_charset(lt1, utf)) = HEX(utf_name) ORDER BY eng_name;
utf8_hex	lt1_hex	eng_name
C396737465727265696368	D6737465727265696368	Austria
42656C6769C3AB	42656C6769EB	Belgium
526F6DC3A26E6961	526F6DE26E6961	Romania

SELECT HEX(utf_name) AS utf8_hex, HEX(cp12) as cp12_hex, eng_name
FROM test.names INNER JOIN test.country ON
HEX(test_args_charset(cp12, utf)) = HEX(utf_name) ORDER BY eng_name;
utf8_hex	cp12_hex	eng_name
D091D18AD0BBD0B3D0B0D180D0B8D18F	C1FAEBE3E0F0E8FF	Bulgaria
D0A1D180D0B1D0B8D198D0B0	D1F0E1E8BCE0	Serbia
D183D0BAD180D0B0D197D0BDD0B0	F3EAF0E0BFEDE0	Ukraine

# 3.3 Check that charset of returning value is same as
#     default(i.e. binary)
SELECT CHARSET(test_args_charset(cp12, utf)) from test.country;
CHARSET(test_args_charset(cp12, utf))
binary
binary
binary
SELECT CHARSET(test_args_charset(lt1, utf)) from test.country;
CHARSET(test_args_charset(lt1, utf))
binary
binary
binary

#------------------------------------------------------------------------
# Tests that fetch collation of second argument, apply that to first
# argument and return the same.
#------------------------------------------------------------------------
#
# 4.1 Without conversion the return values of UDF should not match with
#     ut8mb4 values.
SELECT HEX(utf_name) AS utf8_hex, HEX(lt1) as lt1_hex, eng_name
FROM test.names INNER JOIN test.country ON
HEX(test_args_collation(lt1, lt1)) = HEX(utf_name) ORDER BY eng_name;
utf8_hex	lt1_hex	eng_name

SELECT HEX(utf_name) AS utf8_hex, HEX(cp12) as cp12_hex, eng_name
FROM test.names INNER JOIN test.country ON
HEX(test_args_collation(cp12, cp12)) = HEX(utf_name) ORDER BY eng_name;
utf8_hex	cp12_hex	eng_name
#
# 4.2 After conversion the return values should match with ut8mb4 values
#
SELECT HEX(utf_name) AS utf8_hex, HEX(lt1) as lt1_hex, eng_name
FROM test.names INNER JOIN test.country ON
HEX(test_args_collation(lt1, utf)) = HEX(utf_name) ORDER BY eng_name;
utf8_hex	lt1_hex	eng_name
C396737465727265696368	D6737465727265696368	Austria
42656C6769C3AB	42656C6769EB	Belgium
526F6DC3A26E6961	526F6DE26E6961	Romania

SELECT HEX(utf_name) AS utf8_hex, HEX(cp12) as cp12_hex, eng_name
FROM test.names INNER JOIN test.country ON
HEX(test_args_collation(cp12, utf)) = HEX(utf_name) ORDER BY eng_name;
utf8_hex	cp12_hex	eng_name
D091D18AD0BBD0B3D0B0D180D0B8D18F	C1FAEBE3E0F0E8FF	Bulgaria
D0A1D180D0B1D0B8D198D0B0	D1F0E1E8BCE0	Serbia
D183D0BAD180D0B0D197D0BDD0B0	F3EAF0E0BFEDE0	Ukraine

# 4.3 Check that charset of returning value is same as
#     default(i.e. binary)
SELECT CHARSET(test_args_collation(cp12, utf)) from test.country;
CHARSET(test_args_collation(cp12, utf))
binary
binary
binary
SELECT CHARSET(test_args_collation(lt1, utf)) from test.country;
CHARSET(test_args_collation(lt1, utf))
binary
binary
binary

# 4.4 Check that collation of returning value is same as default
#
SELECT CHARSET(test_args_collation(cp12, utf)) from test.country;
CHARSET(test_args_collation(cp12, utf))
binary
binary
binary
SELECT CHARSET(test_args_collation(lt1, utf)) from test.country;
CHARSET(test_args_collation(lt1, utf))
binary
binary
binary
#------------------------------------------------------------------------
# Tests that check the Charset conversion happens through constant value.
# UDFs pick the first argument as return value.
#------------------------------------------------------------------------
#
# 5.1 Without conversion the return values of UDF should not match with
#     ut8mb4 values.
SELECT HEX(utf_name) AS utf8_hex, HEX(lt1) as lt1_hex, eng_name
FROM test.names INNER JOIN test.country ON
HEX(test_result_charset_with_value(lt1, 'latin1')) = HEX(utf_name)
ORDER BY eng_name;
utf8_hex	lt1_hex	eng_name

SELECT HEX(utf_name) AS utf8_hex, HEX(cp12) as cp12_hex, eng_name
FROM test.names INNER JOIN test.country ON
HEX(test_result_charset_with_value(cp12, 'cp1251')) = HEX(utf_name)
ORDER BY eng_name;
utf8_hex	cp12_hex	eng_name
#
# 5.2 After conversion the return values should match with ut8mb4 values
#
SELECT HEX(utf_name) AS utf8_hex, HEX(lt1) as lt1_hex, eng_name
FROM test.names INNER JOIN test.country ON
HEX(test_result_charset_with_value(lt1, 'utf8mb4')) = HEX(utf_name)
ORDER BY eng_name;
utf8_hex	lt1_hex	eng_name
C396737465727265696368	D6737465727265696368	Austria
42656C6769C3AB	42656C6769EB	Belgium
526F6DC3A26E6961	526F6DE26E6961	Romania

SELECT HEX(utf_name) AS utf8_hex, HEX(cp12) as cp12_hex, eng_name
FROM test.names INNER JOIN test.country ON
HEX(test_result_charset_with_value(cp12, 'utf8mb4')) = HEX(utf_name)
ORDER BY eng_name;
utf8_hex	cp12_hex	eng_name
D091D18AD0BBD0B3D0B0D180D0B8D18F	C1FAEBE3E0F0E8FF	Bulgaria
D0A1D180D0B1D0B8D198D0B0	D1F0E1E8BCE0	Serbia
D183D0BAD180D0B0D197D0BDD0B0	F3EAF0E0BFEDE0	Ukraine
#
# 5.3 Check the charset of the return value.
#
SELECT CHARSET(test_result_charset_with_value(cp12, 'cp1251'))FROM test.country;
CHARSET(test_result_charset_with_value(cp12, 'cp1251'))
cp1251
cp1251
cp1251
SELECT CHARSET(test_result_charset_with_value(lt1, 'latin1'))FROM test.country;
CHARSET(test_result_charset_with_value(lt1, 'latin1'))
latin1
latin1
latin1
# Should be changed to ut8mb4 charset
SELECT CHARSET(test_result_charset_with_value(cp12, 'utf8mb4'))FROM test.country;
CHARSET(test_result_charset_with_value(cp12, 'utf8mb4'))
utf8mb4
utf8mb4
utf8mb4
SELECT CHARSET(test_result_charset_with_value(lt1, 'utf8mb4'))FROM test.country;
CHARSET(test_result_charset_with_value(lt1, 'utf8mb4'))
utf8mb4
utf8mb4
utf8mb4
#------------------------------------------------------------------------
# Tests that check the Charset conversion happens through constant value.
# We specify the collation name of return value. UDF converts the return
# value into the valid charset for that collation.
# This UDF picks the first argument as return value.
#------------------------------------------------------------------------
#
# 5.4 After conversion the return values should match with ut8mb4 values
#
SELECT HEX(utf_name) AS utf8_hex, HEX(lt1) as lt1_hex, eng_name
FROM test.names INNER JOIN test.country ON
HEX(test_result_collation_with_value(lt1, 'utf8mb4_0900_ai_ci')) =
HEX(utf_name) ORDER BY eng_name;
utf8_hex	lt1_hex	eng_name
C396737465727265696368	D6737465727265696368	Austria
42656C6769C3AB	42656C6769EB	Belgium
526F6DC3A26E6961	526F6DE26E6961	Romania
#
# 5.5 First column has charset as 'cp1251' that is changed to 'utf8mb4'
#
SELECT CHARSET(test_result_collation_with_value(cp12, 'utf8mb4_0900_ai_ci')) FROM test.country;
CHARSET(test_result_collation_with_value(cp12, 'utf8mb4_0900_ai_ci'))
utf8mb4
utf8mb4
utf8mb4
SELECT CHARSET(test_result_collation_with_value(cp12, 'utf8mb4_0900_bin')) FROM test.country;
CHARSET(test_result_collation_with_value(cp12, 'utf8mb4_0900_bin'))
utf8mb4
utf8mb4
utf8mb4
SELECT CHARSET(test_result_collation_with_value(cp12, 'utf8mb4_general_ci')) FROM test.country;
CHARSET(test_result_collation_with_value(cp12, 'utf8mb4_general_ci'))
utf8mb4
utf8mb4
utf8mb4
#
# 5.6 Collation of the return string is also updated as specified
#
SELECT COLLATION(test_result_collation_with_value(cp12, 'utf8mb4_0900_ai_ci')) FROM test.country;
COLLATION(test_result_collation_with_value(cp12, 'utf8mb4_0900_ai_ci'))
utf8mb4_0900_ai_ci
utf8mb4_0900_ai_ci
utf8mb4_0900_ai_ci
SELECT COLLATION(test_result_collation_with_value(cp12, 'utf8mb4_0900_bin')) FROM test.country;
COLLATION(test_result_collation_with_value(cp12, 'utf8mb4_0900_bin'))
utf8mb4_0900_bin
utf8mb4_0900_bin
utf8mb4_0900_bin
SELECT COLLATION(test_result_collation_with_value(cp12, 'utf8mb4_general_ci')) FROM test.country;
COLLATION(test_result_collation_with_value(cp12, 'utf8mb4_general_ci'))
utf8mb4_general_ci
utf8mb4_general_ci
utf8mb4_general_ci
#------------------------------------------------------------------------
# Tests that check the Charset conversion happens through constant value.
# We specify the collation name of first argument. UDF returns converted
# first argument value into the valid charset for that collation.
#------------------------------------------------------------------------

#
# 6.1 After conversion the return values should match with ut8mb4 values
#
SELECT HEX(utf_name) AS utf8_hex, HEX(lt1) as lt1_hex, eng_name
FROM test.names INNER JOIN test.country ON
HEX(test_args_collation_with_value(lt1, 'utf8mb4_0900_ai_ci')) =
HEX(utf_name) ORDER BY eng_name;
utf8_hex	lt1_hex	eng_name
C396737465727265696368	D6737465727265696368	Austria
42656C6769C3AB	42656C6769EB	Belgium
526F6DC3A26E6961	526F6DE26E6961	Romania
# 6.2 Convert the charset of first argument according to second arg
SELECT CHARSET(test_args_collation_with_value(cp12, 'utf8mb4_0900_ai_ci')) FROM test.country;
CHARSET(test_args_collation_with_value(cp12, 'utf8mb4_0900_ai_ci'))
binary
binary
binary
SELECT CHARSET(test_args_collation_with_value(cp12, 'utf8mb4_0900_bin')) FROM test.country;
CHARSET(test_args_collation_with_value(cp12, 'utf8mb4_0900_bin'))
binary
binary
binary
SELECT CHARSET(test_args_collation_with_value(cp12, 'utf8mb4_general_ci')) FROM test.country;
CHARSET(test_args_collation_with_value(cp12, 'utf8mb4_general_ci'))
binary
binary
binary
# 6.3 Check the collation of converted charset
SELECT COLLATION(test_args_collation_with_value(cp12, 'utf8mb4_0900_ai_ci')) FROM test.country;
COLLATION(test_args_collation_with_value(cp12, 'utf8mb4_0900_ai_ci'))
binary
binary
binary
SELECT COLLATION(test_args_collation_with_value(cp12, 'utf8mb4_0900_bin')) FROM test.country;
COLLATION(test_args_collation_with_value(cp12, 'utf8mb4_0900_bin'))
binary
binary
binary
SELECT COLLATION(test_args_collation_with_value(cp12, 'utf8mb4_general_ci')) FROM test.country;
COLLATION(test_args_collation_with_value(cp12, 'utf8mb4_general_ci'))
binary
binary
binary
#------------------------------------------------------------------------
# Negative test scenarios
#------------------------------------------------------------------------
SELECT test_result_charset_with_value(cp12, 'utf8mb4_invalid') FROM test.country;
ERROR HY000: Invalid character set 'utf8mb4_invalid' was specified. It must be a character set name as supported by server.
SELECT test_args_charset_with_value(cp12, 'utf8mb4_invalid') FROM test.country;
ERROR HY000: Invalid character set 'utf8mb4_invalid' was specified. It must be a character set name as supported by server.
SELECT test_result_collation_with_value(cp12, 'utf8mb4_invalid') FROM test.country;
ERROR HY000: Invalid collation 'utf8mb4_invalid' was specified. It must be a collation name as supported by server.
SELECT test_result_charset_with_value(cp12, '') FROM test.country;
ERROR HY000: Invalid character set '' was specified. It must be a character set name as supported by server.
SELECT test_args_charset_with_value('', 'utf8mb4') FROM test.country;
test_args_charset_with_value('', 'utf8mb4')



SELECT test_args_charset_with_value(utf, '') FROM test.country;
ERROR HY000: Invalid character set '' was specified. It must be a character set name as supported by server.
SELECT test_result_collation_with_value('cp12', '') FROM test.country;
ERROR HY000: Invalid collation '' was specified. It must be a collation name as supported by server.
# cleanup
DROP TABLE test.country;
DROP TABLE test.names;
UNINSTALL PLUGIN test_udf_extension_services;
