#
# WL#12370 : Helper file that calls the test UDFs
#

# Create the tables to test
CREATE TABLE test.country (utf varchar(40) CHARACTER SET utf8mb4,
                           cp12 varchar(40) CHARACTER SET cp1251,
                           lt1 VARCHAR(40) CHARACTER SET latin1);

# We are storing the HEX values of the non-ASCII characters as some
# editors may misinterpret the non-ASCII chars while reading/displaying them.
# Here are the actual string and their HEX code in their respective charset.
#
#   ????    E0A4ADE0A4BEE0A4B0E0A4A4  utf8mb4
# ????????  C1FAEBE3E0F0E8FF    cp1251
# Österreich D6737465727265696368  latin1
# ??????  D0A0D0BED181D181D0B8D18F   utf8mb4
# ??????  D1F0E1E8BCE0              cp1251
# België  42656C6769EB              latin1
# India   496E646961                utf8mb4
# ??????? F3EAF0E0BFEDE0  cp1251
# România 526F6DE26E6961  latin1
INSERT INTO test.country(utf, cp12, lt1) VALUES
   (UNHEX('E0A4ADE0A4BEE0A4B0E0A4A4'), UNHEX('C1FAEBE3E0F0E8FF'),
    UNHEX('D6737465727265696368')),
   (UNHEX('D0A0D0BED181D181D0B8D18F'), UNHEX('D1F0E1E8BCE0'),
    UNHEX('42656C6769EB')),
   (UNHEX('496E646961'), UNHEX('F3EAF0E0BFEDE0'), UNHEX('526F6DE26E6961'));

CREATE TABLE test.names(utf_name varchar(40), eng_name varchar(40))
                        CHARACTER SET utf8mb4;
# The column 'utf_name' are stored in the utf8mb4 charset
# Following strings are stored in the table.
#   ????    E0A4ADE0A4BEE0A4B0E0A4A4
# ???????? D091D18AD0BBD0B3D0B0D180D0B8D18F
# Österreich  C396737465727265696368
# ??????  D0A0D0BED181D181D0B8D18F
# ??????  D0A1D180D0B1D0B8D198D0B0
# België  42656C6769C3AB
# ???????  D183D0BAD180D0B0D197D0BDD0B0
# România 526F6DC3A26E6961
INSERT INTO test.names(utf_name, eng_name) VALUES
    (UNHEX('E0A4ADE0A4BEE0A4B0E0A4A4'), 'India'),
    (UNHEX('D091D18AD0BBD0B3D0B0D180D0B8D18F'), 'Bulgaria'),
    (UNHEX('C396737465727265696368'), 'Austria'),
    (UNHEX('D0A0D0BED181D181D0B8D18F'), 'Russia'),
    (UNHEX('D0A1D180D0B1D0B8D198D0B0'), 'Serbia'),
    (UNHEX('42656C6769C3AB'), 'Belgium'),
    (UNHEX('D183D0BAD180D0B0D197D0BDD0B0'), 'Ukraine'),
    (UNHEX('526F6DC3A26E6961'), 'Romania');

--echo #------------------------------------------------------------------------
--echo # Tests that fetch the charset of second argument and apply that to the
--echo # return value. This UDF returns first argument.
--echo #------------------------------------------------------------------------

--echo #
--echo # 1.1 Without conversion the return values of UDF should not match with
--echo #     ut8mb4 values.

SELECT HEX(utf_name) AS utf8_hex, HEX(lt1) as latin1_hex, eng_name
    FROM test.names INNER JOIN test.country ON
        HEX(test_result_charset(lt1, lt1)) = HEX(utf_name) ORDER BY eng_name;

--echo

SELECT HEX(utf_name) AS utf8_hex, HEX(cp12) as cp1251_hex, eng_name
    FROM test.names INNER JOIN test.country ON
        HEX(test_result_charset(cp12, cp12)) = HEX(utf_name) ORDER BY eng_name;

--echo #
--echo # 1.2 After conversion the return values should match with ut8mb4 values
--echo #

SELECT HEX(utf_name) AS utf8_hex, HEX(lt1) as latin1_hex, eng_name
    FROM test.names INNER JOIN test.country ON
        HEX(test_result_charset(lt1, utf)) = HEX(utf_name) ORDER BY eng_name;

--echo

SELECT HEX(utf_name) AS utf8_hex, HEX(cp12) as cp1251_hex, eng_name
    FROM test.names INNER JOIN test.country ON
        HEX(test_result_charset(cp12, utf)) = HEX(utf_name) ORDER BY eng_name;

--echo #
--echo # 1.3 Check the charset of the return value.
--echo #
SELECT CHARSET(test_result_charset(cp12, cp12))FROM test.country;
SELECT CHARSET(test_result_charset(lt1, lt1))FROM test.country;
--echo # Should be changed to ut8mb4 charset
SELECT CHARSET(test_result_charset(cp12, utf))FROM test.country;
SELECT CHARSET(test_result_charset(lt1, utf))FROM test.country;

--echo #------------------------------------------------------------------------
--echo # Tests that fetch the collation of second argument and apply that to the
--echo # return value. This UDF returns first argument.
--echo #------------------------------------------------------------------------

--echo #
--echo # 2.1 Without conversion the return values of UDF should not match with
--echo #     ut8mb4 values.

SELECT HEX(utf_name) AS utf8_hex, HEX(lt1) as lt1_hex, eng_name
    FROM test.names INNER JOIN test.country ON
        HEX(test_result_collation(lt1, lt1)) = HEX(utf_name) ORDER BY eng_name;

--echo

SELECT HEX(utf_name) AS utf8_hex, HEX(cp12) as cp12_hex, eng_name
    FROM test.names INNER JOIN test.country ON
        HEX(test_result_collation(cp12, cp12)) = HEX(utf_name) ORDER BY eng_name;

--echo #
--echo # 2.2 After conversion the return values should match with ut8mb4 values
--echo #

SELECT HEX(utf_name) AS utf8_hex, HEX(lt1) as lt1_hex, eng_name
    FROM test.names INNER JOIN test.country ON
        HEX(test_result_collation(lt1, utf)) = HEX(utf_name) ORDER BY eng_name;

--echo

SELECT HEX(utf_name) AS utf8_hex, HEX(cp12) as cp12_hex, eng_name
    FROM test.names INNER JOIN test.country ON
        HEX(test_result_collation(cp12, utf)) = HEX(utf_name) ORDER BY eng_name;

--echo

--echo # 2.3  Check the charset of the value returned by the UDF
--echo #

SELECT CHARSET(test_result_collation(cp12, cp12)) from test.country;
SELECT CHARSET(test_result_collation(lt1, lt1)) from test.country;
--echo # Should be changed to utf8mb4 charset
SELECT CHARSET(test_result_collation(cp12, utf)) from test.country;
SELECT CHARSET(test_result_collation(lt1, utf)) from test.country;

--echo # 2.6  Check the collation of the value returned by the UDF
SELECT COLLATION(test_result_collation(cp12, cp12)) from test.country;
SELECT COLLATION(test_result_collation(lt1, lt1)) from test.country;
--echo # Should be changed to default utf8mb4 collation.
SELECT COLLATION(test_result_collation(cp12, utf)) from test.country;
SELECT COLLATION(test_result_collation(lt1, utf)) from test.country;

--echo
--echo #------------------------------------------------------------------------
--echo # Tests that fetch charset of second argument, apply that to first
--echo # argument and return the same.
--echo #------------------------------------------------------------------------

--echo #
--echo # 3.1 Without conversion the return values of UDF should not match with
--echo #     ut8mb4 values.

SELECT HEX(utf_name) AS utf8_hex, HEX(lt1) as lt1_hex, eng_name
    FROM test.names INNER JOIN test.country ON
        HEX(test_args_charset(lt1, lt1)) = HEX(utf_name) ORDER BY eng_name;

--echo

SELECT HEX(utf_name) AS utf8_hex, HEX(cp12) as cp12_hex, eng_name
    FROM test.names INNER JOIN test.country ON
        HEX(test_args_charset(cp12, cp12)) = HEX(utf_name) ORDER BY eng_name;

--echo #
--echo # 3.2 After conversion the return values should match with ut8mb4 values
--echo #

SELECT HEX(utf_name) AS utf8_hex, HEX(lt1) as lt1_hex, eng_name
    FROM test.names INNER JOIN test.country ON
        HEX(test_args_charset(lt1, utf)) = HEX(utf_name) ORDER BY eng_name;

--echo

SELECT HEX(utf_name) AS utf8_hex, HEX(cp12) as cp12_hex, eng_name
    FROM test.names INNER JOIN test.country ON
        HEX(test_args_charset(cp12, utf)) = HEX(utf_name) ORDER BY eng_name;

--echo
--echo # 3.3 Check that charset of returning value is same as
--echo #     default(i.e. binary)
SELECT CHARSET(test_args_charset(cp12, utf)) from test.country;
SELECT CHARSET(test_args_charset(lt1, utf)) from test.country;

--echo
--echo #------------------------------------------------------------------------
--echo # Tests that fetch collation of second argument, apply that to first
--echo # argument and return the same.
--echo #------------------------------------------------------------------------

--echo #
--echo # 4.1 Without conversion the return values of UDF should not match with
--echo #     ut8mb4 values.

SELECT HEX(utf_name) AS utf8_hex, HEX(lt1) as lt1_hex, eng_name
    FROM test.names INNER JOIN test.country ON
        HEX(test_args_collation(lt1, lt1)) = HEX(utf_name) ORDER BY eng_name;

--echo

SELECT HEX(utf_name) AS utf8_hex, HEX(cp12) as cp12_hex, eng_name
    FROM test.names INNER JOIN test.country ON
        HEX(test_args_collation(cp12, cp12)) = HEX(utf_name) ORDER BY eng_name;

--echo #
--echo # 4.2 After conversion the return values should match with ut8mb4 values
--echo #

SELECT HEX(utf_name) AS utf8_hex, HEX(lt1) as lt1_hex, eng_name
    FROM test.names INNER JOIN test.country ON
        HEX(test_args_collation(lt1, utf)) = HEX(utf_name) ORDER BY eng_name;

--echo

SELECT HEX(utf_name) AS utf8_hex, HEX(cp12) as cp12_hex, eng_name
    FROM test.names INNER JOIN test.country ON
        HEX(test_args_collation(cp12, utf)) = HEX(utf_name) ORDER BY eng_name;

--echo
--echo # 4.3 Check that charset of returning value is same as
--echo #     default(i.e. binary)
SELECT CHARSET(test_args_collation(cp12, utf)) from test.country;
SELECT CHARSET(test_args_collation(lt1, utf)) from test.country;

--echo
--echo # 4.4 Check that collation of returning value is same as default
--echo #
SELECT CHARSET(test_args_collation(cp12, utf)) from test.country;
SELECT CHARSET(test_args_collation(lt1, utf)) from test.country;


--echo #------------------------------------------------------------------------
--echo # Tests that check the Charset conversion happens through constant value.
--echo # UDFs pick the first argument as return value.
--echo #------------------------------------------------------------------------

# Specify the charset name of return value. UDF converts the charset
# of returned value as specified by the user.

--echo #
--echo # 5.1 Without conversion the return values of UDF should not match with
--echo #     ut8mb4 values.

SELECT HEX(utf_name) AS utf8_hex, HEX(lt1) as lt1_hex, eng_name
    FROM test.names INNER JOIN test.country ON
        HEX(test_result_charset_with_value(lt1, 'latin1')) = HEX(utf_name)
            ORDER BY eng_name;

--echo

SELECT HEX(utf_name) AS utf8_hex, HEX(cp12) as cp12_hex, eng_name
    FROM test.names INNER JOIN test.country ON
        HEX(test_result_charset_with_value(cp12, 'cp1251')) = HEX(utf_name)
            ORDER BY eng_name;

--echo #
--echo # 5.2 After conversion the return values should match with ut8mb4 values
--echo #

SELECT HEX(utf_name) AS utf8_hex, HEX(lt1) as lt1_hex, eng_name
    FROM test.names INNER JOIN test.country ON
        HEX(test_result_charset_with_value(lt1, 'utf8mb4')) = HEX(utf_name)
            ORDER BY eng_name;

--echo

SELECT HEX(utf_name) AS utf8_hex, HEX(cp12) as cp12_hex, eng_name
    FROM test.names INNER JOIN test.country ON
        HEX(test_result_charset_with_value(cp12, 'utf8mb4')) = HEX(utf_name)
            ORDER BY eng_name;

--echo #
--echo # 5.3 Check the charset of the return value.
--echo #
SELECT CHARSET(test_result_charset_with_value(cp12, 'cp1251'))FROM test.country;
SELECT CHARSET(test_result_charset_with_value(lt1, 'latin1'))FROM test.country;
--echo # Should be changed to ut8mb4 charset
SELECT CHARSET(test_result_charset_with_value(cp12, 'utf8mb4'))FROM test.country;
SELECT CHARSET(test_result_charset_with_value(lt1, 'utf8mb4'))FROM test.country;

--echo #------------------------------------------------------------------------
--echo # Tests that check the Charset conversion happens through constant value.
--echo # We specify the collation name of return value. UDF converts the return
--echo # value into the valid charset for that collation.
--echo # This UDF picks the first argument as return value.
--echo #------------------------------------------------------------------------

--echo #
--echo # 5.4 After conversion the return values should match with ut8mb4 values
--echo #

SELECT HEX(utf_name) AS utf8_hex, HEX(lt1) as lt1_hex, eng_name
    FROM test.names INNER JOIN test.country ON
        HEX(test_result_collation_with_value(lt1, 'utf8mb4_0900_ai_ci')) =
            HEX(utf_name) ORDER BY eng_name;

--echo #
--echo # 5.5 First column has charset as 'cp1251' that is changed to 'utf8mb4'
--echo #
SELECT CHARSET(test_result_collation_with_value(cp12, 'utf8mb4_0900_ai_ci')) FROM test.country;
SELECT CHARSET(test_result_collation_with_value(cp12, 'utf8mb4_0900_bin')) FROM test.country;
SELECT CHARSET(test_result_collation_with_value(cp12, 'utf8mb4_general_ci')) FROM test.country;
--echo #
--echo # 5.6 Collation of the return string is also updated as specified
--echo #
SELECT COLLATION(test_result_collation_with_value(cp12, 'utf8mb4_0900_ai_ci')) FROM test.country;
SELECT COLLATION(test_result_collation_with_value(cp12, 'utf8mb4_0900_bin')) FROM test.country;
SELECT COLLATION(test_result_collation_with_value(cp12, 'utf8mb4_general_ci')) FROM test.country;

--echo #------------------------------------------------------------------------
--echo # Tests that check the Charset conversion happens through constant value.
--echo # We specify the collation name of first argument. UDF returns converted
--echo # first argument value into the valid charset for that collation.
--echo #------------------------------------------------------------------------

--echo
--echo #
--echo # 6.1 After conversion the return values should match with ut8mb4 values
--echo #


SELECT HEX(utf_name) AS utf8_hex, HEX(lt1) as lt1_hex, eng_name
    FROM test.names INNER JOIN test.country ON
        HEX(test_args_collation_with_value(lt1, 'utf8mb4_0900_ai_ci')) =
            HEX(utf_name) ORDER BY eng_name;

--echo # 6.2 Convert the charset of first argument according to second arg

SELECT CHARSET(test_args_collation_with_value(cp12, 'utf8mb4_0900_ai_ci')) FROM test.country;
SELECT CHARSET(test_args_collation_with_value(cp12, 'utf8mb4_0900_bin')) FROM test.country;
SELECT CHARSET(test_args_collation_with_value(cp12, 'utf8mb4_general_ci')) FROM test.country;

--echo # 6.3 Check the collation of converted charset

# By default collation of return value remains 'binary'
SELECT COLLATION(test_args_collation_with_value(cp12, 'utf8mb4_0900_ai_ci')) FROM test.country;
SELECT COLLATION(test_args_collation_with_value(cp12, 'utf8mb4_0900_bin')) FROM test.country;
SELECT COLLATION(test_args_collation_with_value(cp12, 'utf8mb4_general_ci')) FROM test.country;

--echo #------------------------------------------------------------------------
--echo # Negative test scenarios
--echo #------------------------------------------------------------------------

--error ER_DA_UDF_INVALID_CHARSET
SELECT test_result_charset_with_value(cp12, 'utf8mb4_invalid') FROM test.country;
--error ER_DA_UDF_INVALID_CHARSET
SELECT test_args_charset_with_value(cp12, 'utf8mb4_invalid') FROM test.country;
--error ER_DA_UDF_INVALID_COLLATION
SELECT test_result_collation_with_value(cp12, 'utf8mb4_invalid') FROM test.country;
--error ER_DA_UDF_INVALID_CHARSET
SELECT test_result_charset_with_value(cp12, '') FROM test.country;
SELECT test_args_charset_with_value('', 'utf8mb4') FROM test.country;
--error ER_DA_UDF_INVALID_CHARSET
SELECT test_args_charset_with_value(utf, '') FROM test.country;
--error ER_DA_UDF_INVALID_COLLATION
SELECT test_result_collation_with_value('cp12', '') FROM test.country;

--echo # cleanup
DROP TABLE test.country;
DROP TABLE test.names;
