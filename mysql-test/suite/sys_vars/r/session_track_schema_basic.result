#
# Variable name : session_track_schema
# Scope         : Global & Session
#
# Global - default
SELECT @@global.session_track_schema;
@@global.session_track_schema
1
# Session - default
SELECT @@session.session_track_schema;
@@session.session_track_schema
1

# via performance_schema.global_variables
SELECT * FROM performance_schema.global_variables WHERE VARIABLE_NAME LIKE 'track_current%';
VARIABLE_NAME	VARIABLE_VALUE
# via performance_schema.session_variables
SELECT * FROM performance_schema.session_variables WHERE VARIABLE_NAME LIKE 'track_current%';
VARIABLE_NAME	VARIABLE_VALUE
SET @global_saved_tmp =  @@global.session_track_schema;

# Altering global variable's value
SET @@global.session_track_schema = 0;
SELECT @@global.session_track_schema;
@@global.session_track_schema
0
SELECT @@session.session_track_schema;
@@session.session_track_schema
1
SET @@global.session_track_schema = TrUe;
SELECT @@global.session_track_schema;
@@global.session_track_schema
1
SELECT @@session.session_track_schema;
@@session.session_track_schema
1
SET @@global.session_track_schema = FaLsE;
SELECT @@global.session_track_schema;
@@global.session_track_schema
0
SELECT @@session.session_track_schema;
@@session.session_track_schema
1

# Altering session variable's value
SET @@session.session_track_schema = 0;
SELECT @@global.session_track_schema;
@@global.session_track_schema
0
SELECT @@session.session_track_schema;
@@session.session_track_schema
0

# Variables' values in a new session.
# Global - expect 0
SELECT @@global.session_track_schema;
@@global.session_track_schema
0

# Session - expect 0
SELECT @@session.session_track_schema;
@@session.session_track_schema
0

# Switching to the default connection.
SELECT @@global.session_track_schema;
@@global.session_track_schema
0
SELECT @@session.session_track_schema;
@@session.session_track_schema
0

# Test if DEFAULT is working as expected.
SET @@global.session_track_schema = DEFAULT;
SET @@session.session_track_schema = DEFAULT;

# Global - expect 1
SELECT @@global.session_track_schema;
@@global.session_track_schema
1
# Session - expect 1
SELECT @@session.session_track_schema;
@@session.session_track_schema
1

# Variables' values in a new session (con2).
SELECT @@global.session_track_schema;
@@global.session_track_schema
1
SELECT @@session.session_track_schema;
@@session.session_track_schema
1

# Altering session should not affect global.
SET @@session.session_track_schema = FALSE;
SELECT @@global.session_track_schema;
@@global.session_track_schema
1
SELECT @@session.session_track_schema;
@@session.session_track_schema
0

# Variables' values in a new session (con3).
# Altering global should not affect session.
SET @@global.session_track_schema = OFF;
SELECT @@global.session_track_schema;
@@global.session_track_schema
0
SELECT @@session.session_track_schema;
@@session.session_track_schema
1

# Switching to the default connection.
# Restoring the original values.
SET @@global.session_track_schema = @global_saved_tmp;
# End of tests.
