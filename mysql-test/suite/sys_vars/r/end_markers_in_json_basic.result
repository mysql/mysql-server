SET @start_global_value = @@global.end_markers_in_json;
SELECT @start_global_value;
@start_global_value
0
select @@global.end_markers_in_json;
@@global.end_markers_in_json
0
select @@session.end_markers_in_json;
@@session.end_markers_in_json
0
show global variables like 'end_markers_in_json';
Variable_name	Value
end_markers_in_json	OFF
show session variables like 'end_markers_in_json';
Variable_name	Value
end_markers_in_json	OFF
select * from performance_schema.global_variables where variable_name='end_markers_in_json';
VARIABLE_NAME	VARIABLE_VALUE
end_markers_in_json	OFF
select * from performance_schema.session_variables where variable_name='end_markers_in_json';
VARIABLE_NAME	VARIABLE_VALUE
end_markers_in_json	OFF
set global end_markers_in_json=1;
select @@global.end_markers_in_json;
@@global.end_markers_in_json
1
set session end_markers_in_json=1;
select @@session.end_markers_in_json;
@@session.end_markers_in_json
1
set global end_markers_in_json=0;
select @@global.end_markers_in_json;
@@global.end_markers_in_json
0
set session end_markers_in_json=0;
select @@session.end_markers_in_json;
@@session.end_markers_in_json
0
set session end_markers_in_json=on;
select @@session.end_markers_in_json;
@@session.end_markers_in_json
1
set session end_markers_in_json=off;
select @@session.end_markers_in_json;
@@session.end_markers_in_json
0
set session end_markers_in_json=default;
select @@session.end_markers_in_json;
@@session.end_markers_in_json
0
set global end_markers_in_json=1.1;
ERROR 42000: Incorrect argument type to variable 'end_markers_in_json'
set global end_markers_in_json=1e1;
ERROR 42000: Incorrect argument type to variable 'end_markers_in_json'
set session end_markers_in_json="foobar";
ERROR 42000: Variable 'end_markers_in_json' can't be set to the value of 'foobar'
SET @@global.end_markers_in_json = @start_global_value;
SELECT @@global.end_markers_in_json;
@@global.end_markers_in_json
0
