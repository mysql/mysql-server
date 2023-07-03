# WL15040: Add system variable to control EXPLAIN format

# Save the original explain_format value.
set @orig_explain_format=@@global.explain_format;

# Test the initial value. It should be TRADITIONAL_STRICT.
select @@explain_format, @@global.explain_format;

# hypergraph with TRADITIONAL_STRICT should error out, rather than convert to TREE.
--error 0, ER_HYPERGRAPH_NOT_SUPPORTED_YET
explain select 1;

set explain_format=traditional;

--echo # With hypergraph and TRADITIONAL, EXPLAIN should use TREE format.
explain select 1;

--echo # Test that explain_format can be set to any of the scopes; i.e. session,
--echo # global and persist.

set persist explain_format=tree;
select @@explain_format, @@global.explain_format;
set global explain_format=json;
select @@explain_format, @@global.explain_format;
set explain_format=tree;
select @@explain_format, @@global.explain_format;
set explain_format=default;
select @@explain_format, @@global.explain_format;
set global explain_format=default;
select @@explain_format, @@global.explain_format;
set persist explain_format=default;
select @@explain_format, @@global.explain_format;
set explain_format=default;
select @@explain_format, @@global.explain_format;
reset persist explain_format; # Required to clear the conf file.
select @@explain_format, @@global.explain_format;
# Also delete the conf file to remote test foot-prints
let $MYSQLD_DATADIR= `select @@datadir`;
--remove_file $MYSQLD_DATADIR/mysqld-auto.cnf

explain select 1;
explain format=tree select 1;
explain format=json select 1;

set explain_format=tree;
explain select 1;
explain format=json select 1;

set explain_format=json;
explain format=tree select 1;
explain select 1;

set explain_format=default;
explain select 1;
explain format=tree select 1;

set global explain_format=json; # Test that global setting does not interfere
set explain_format=traditional;
explain select 1;

# Analyze should work with both traditional and traditional_strict, regardless
# of hypergraph.
set explain_format=traditional;
--replace_regex /time=\d+\.\d+\.\.\d+\.\d+/time=N.NNN..N.NNN/
explain analyze select 1;
set explain_format=traditional_strict;
--replace_regex /time=\d+\.\d+\.\.\d+\.\d+/time=N.NNN..N.NNN/
explain analyze select 1;

# But explicit use of unsupported format should error out.
--error ER_NOT_SUPPORTED_YET
explain analyze format=traditional select 1;

# TRADITIONAL_STRICT value is only allowed in the sys variable; not in EXPLAIN.
--error ER_UNKNOWN_EXPLAIN_FORMAT
explain format=traditional_strict select 1;

# Reset the global var to the original value
set global explain_format=@orig_explain_format;

select @@explain_format, @@global.explain_format;
