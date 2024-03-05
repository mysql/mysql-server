--echo #
--echo # Error handling/edge cases.
--echo #

--error ER_UNDISCLOSED_PARSE_ERROR_IN_DIGEST_FN
eval SELECT $digest_fn( (SELECT * FROM( SELECT user() ) t) );

--error ER_PARSE_ERROR_IN_DIGEST_FN
eval SELECT $digest_fn( 'This can''t be parsed but yields a digest.' ) IS NULL;
--error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
eval SELECT $digest_fn();
--error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
eval SELECT $digest_fn( 'too many', 'arguments' );
eval SELECT $digest_fn( NULL );
--error ER_PARSE_ERROR_IN_DIGEST_FN
eval SELECT $digest_fn( '/*' ) IS NULL;
--error ER_PARSE_ERROR_IN_DIGEST_FN
eval SELECT $digest_fn( '@@' ) IS NULL;
--error ER_PARSE_ERROR_IN_DIGEST_FN
eval SELECT $digest_fn( "SELECT 1; SELECT 2" );
eval SELECT $digest_fn( "SELECT 1;" ) IS NULL;

--error ER_UNDISCLOSED_PARSE_ERROR_IN_DIGEST_FN
eval SELECT $digest_fn( repeat( 'a', character_maximum_length ) ) IS NULL
FROM information_schema.columns
WHERE table_name = 'events_statements_history' AND column_name = 'digest';

--error ER_FUNCTION_DOES_NOT_SUPPORT_CHARACTER_SET
eval SELECT $digest_fn( point(1, 2) ) IS NULL;
--echo # Make sure that UTF-8 content is not printed in the error message
--echo # without being converted.
SET NAMES latin1;
--error ER_PARSE_ERROR_IN_DIGEST_FN
eval SELECT $digest_fn( _utf8mb4 x'e298ba' ) IS NULL; # White Smiling Face U+263A
SET NAMES utf8mb4;
--error ER_PARSE_ERROR_IN_DIGEST_FN
eval SELECT $digest_fn( _utf8mb4 x'e298ba' ) IS NULL; # White Smiling Face U+263A
SET NAMES DEFAULT;
--error ER_PARSE_ERROR_IN_DIGEST_FN
eval SELECT $digest_fn( 'a\0bc' ) IS NULL;
--error ER_PARSE_ERROR_IN_DIGEST_FN
eval SELECT $digest_fn( '\0abc' ) IS NULL;
--error ER_PARSE_ERROR_IN_DIGEST_FN
eval SELECT $digest_fn( '' );
eval SELECT $digest_fn( $digest_fn( NULL ) );
--error ER_PARSE_ERROR_IN_DIGEST_FN
eval SELECT $digest_fn( $digest_fn( '' ) );
--error ER_PARSE_ERROR_IN_DIGEST_FN
eval SELECT $digest_fn( $digest_fn( '\0' ) );
--echo # Prepared statement syntax is not supported...
--error ER_PARSE_ERROR_IN_DIGEST_FN
eval SELECT $digest_fn( 'SELECT ?' );
--echo # ... which is why we get the text in this case; The error stems from
--echo # the inner call.
--error ER_PARSE_ERROR_IN_DIGEST_FN
eval SELECT $digest_fn( $digest_fn( 'SELECT ?' ) );

SET sql_mode = ''; # Un-strict
CREATE TABLE t1 ( g GEOMETRY NOT NULL );
INSERT INTO t1 VALUES ( POINT(1,2) );
--error ER_FUNCTION_DOES_NOT_SUPPORT_CHARACTER_SET
eval SELECT concat( $digest_fn(g), 'x' ) FROM t1;
SET sql_mode = DEFAULT;
DROP TABLE t1;
--echo # Let's check that warnings in the parser are silenced.
eval SELECT $digest_fn( "INSERT DELAYED INTO t1 VALUES(1)" ) IS NULL;
