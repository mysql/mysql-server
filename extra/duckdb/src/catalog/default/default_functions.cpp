#include "duckdb/catalog/default/default_functions.hpp"
#include "duckdb/parser/parser.hpp"
#include "duckdb/parser/parsed_data/create_macro_info.hpp"
#include "duckdb/parser/expression/columnref_expression.hpp"
#include "duckdb/catalog/catalog_entry/scalar_macro_catalog_entry.hpp"

#include "duckdb/function/scalar_macro_function.hpp"

namespace duckdb {

static const DefaultMacro internal_macros[] = {
	{DEFAULT_SCHEMA, "current_role", {nullptr}, {{nullptr, nullptr}}, "'duckdb'"},                       // user name of current execution context
	{DEFAULT_SCHEMA, "current_user", {nullptr}, {{nullptr, nullptr}}, "'duckdb'"},                       // user name of current execution context
	{DEFAULT_SCHEMA, "current_catalog", {nullptr}, {{nullptr, nullptr}}, "main.current_database()"},          // name of current database (called "catalog" in the SQL standard)
	{DEFAULT_SCHEMA, "user", {nullptr}, {{nullptr, nullptr}}, "current_user"},                           // equivalent to current_user
	{DEFAULT_SCHEMA, "session_user", {nullptr}, {{nullptr, nullptr}}, "'duckdb'"},                       // session user name
	{"pg_catalog", "inet_client_addr", {nullptr}, {{nullptr, nullptr}}, "NULL"},                       // address of the remote connection
	{"pg_catalog", "inet_client_port", {nullptr}, {{nullptr, nullptr}}, "NULL"},                       // port of the remote connection
	{"pg_catalog", "inet_server_addr", {nullptr}, {{nullptr, nullptr}}, "NULL"},                       // address of the local connection
	{"pg_catalog", "inet_server_port", {nullptr}, {{nullptr, nullptr}}, "NULL"},                       // port of the local connection
	{"pg_catalog", "pg_my_temp_schema", {nullptr}, {{nullptr, nullptr}}, "0"},                         // OID of session's temporary schema, or 0 if none
	{"pg_catalog", "pg_is_other_temp_schema", {"schema_id", nullptr}, {{nullptr, nullptr}}, "false"},  // is schema another session's temporary schema?

	{"pg_catalog", "pg_conf_load_time", {nullptr}, {{nullptr, nullptr}}, "current_timestamp"},         // configuration load time
	{"pg_catalog", "pg_postmaster_start_time", {nullptr}, {{nullptr, nullptr}}, "current_timestamp"},  // server start time

	{"pg_catalog", "pg_typeof", {"expression", nullptr}, {{nullptr, nullptr}}, "lower(typeof(expression))"},  // get the data type of any value

	{"pg_catalog", "current_database", {nullptr}, {{nullptr, nullptr}}, "system.main.current_database()"},  	    // name of current database (called "catalog" in the SQL standard)
	{"pg_catalog", "current_query", {nullptr}, {{nullptr, nullptr}}, "system.main.current_query()"},  	        // the currently executing query (NULL if not inside a plpgsql function)
	{"pg_catalog", "current_schema", {nullptr}, {{nullptr, nullptr}}, "system.main.current_schema()"},  	        // name of current schema
	{"pg_catalog", "current_schemas", {"include_implicit"}, {{nullptr, nullptr}}, "system.main.current_schemas(include_implicit)"},  	// names of schemas in search path

	// privilege functions
	{"pg_catalog", "has_any_column_privilege", {"table", "privilege", nullptr}, {{nullptr, nullptr}}, "true"},  //boolean  //does current user have privilege for any column of table
	{"pg_catalog", "has_any_column_privilege", {"user", "table", "privilege", nullptr}, {{nullptr, nullptr}}, "true"},  //boolean  //does user have privilege for any column of table
	{"pg_catalog", "has_column_privilege", {"table", "column", "privilege", nullptr}, {{nullptr, nullptr}}, "true"},  //boolean  //does current user have privilege for column
	{"pg_catalog", "has_column_privilege", {"user", "table", "column", "privilege", nullptr}, {{nullptr, nullptr}}, "true"},  //boolean  //does user have privilege for column
	{"pg_catalog", "has_database_privilege", {"database", "privilege", nullptr}, {{nullptr, nullptr}}, "true"},  //boolean  //does current user have privilege for database
	{"pg_catalog", "has_database_privilege", {"user", "database", "privilege", nullptr}, {{nullptr, nullptr}}, "true"},  //boolean  //does user have privilege for database
	{"pg_catalog", "has_foreign_data_wrapper_privilege", {"fdw", "privilege", nullptr}, {{nullptr, nullptr}}, "true"},  //boolean  //does current user have privilege for foreign-data wrapper
	{"pg_catalog", "has_foreign_data_wrapper_privilege", {"user", "fdw", "privilege", nullptr}, {{nullptr, nullptr}}, "true"},  //boolean  //does user have privilege for foreign-data wrapper
	{"pg_catalog", "has_function_privilege", {"function", "privilege", nullptr}, {{nullptr, nullptr}}, "true"},  //boolean  //does current user have privilege for function
	{"pg_catalog", "has_function_privilege", {"user", "function", "privilege", nullptr}, {{nullptr, nullptr}}, "true"},  //boolean  //does user have privilege for function
	{"pg_catalog", "has_language_privilege", {"language", "privilege", nullptr}, {{nullptr, nullptr}}, "true"},  //boolean  //does current user have privilege for language
	{"pg_catalog", "has_language_privilege", {"user", "language", "privilege", nullptr}, {{nullptr, nullptr}}, "true"},  //boolean  //does user have privilege for language
	{"pg_catalog", "has_schema_privilege", {"schema", "privilege", nullptr}, {{nullptr, nullptr}}, "true"},  //boolean  //does current user have privilege for schema
	{"pg_catalog", "has_schema_privilege", {"user", "schema", "privilege", nullptr}, {{nullptr, nullptr}}, "true"},  //boolean  //does user have privilege for schema
	{"pg_catalog", "has_sequence_privilege", {"sequence", "privilege", nullptr}, {{nullptr, nullptr}}, "true"},  //boolean  //does current user have privilege for sequence
	{"pg_catalog", "has_sequence_privilege", {"user", "sequence", "privilege", nullptr}, {{nullptr, nullptr}}, "true"},  //boolean  //does user have privilege for sequence
	{"pg_catalog", "has_server_privilege", {"server", "privilege", nullptr}, {{nullptr, nullptr}}, "true"},  //boolean  //does current user have privilege for foreign server
	{"pg_catalog", "has_server_privilege", {"user", "server", "privilege", nullptr}, {{nullptr, nullptr}}, "true"},  //boolean  //does user have privilege for foreign server
	{"pg_catalog", "has_table_privilege", {"table", "privilege", nullptr}, {{nullptr, nullptr}}, "true"},  //boolean  //does current user have privilege for table
	{"pg_catalog", "has_table_privilege", {"user", "table", "privilege", nullptr}, {{nullptr, nullptr}}, "true"},  //boolean  //does user have privilege for table
	{"pg_catalog", "has_tablespace_privilege", {"tablespace", "privilege", nullptr}, {{nullptr, nullptr}}, "true"},  //boolean  //does current user have privilege for tablespace
	{"pg_catalog", "has_tablespace_privilege", {"user", "tablespace", "privilege", nullptr}, {{nullptr, nullptr}}, "true"},  //boolean  //does user have privilege for tablespace

	// various postgres system functions
	{"pg_catalog", "pg_get_viewdef", {"oid", nullptr}, {{nullptr, nullptr}}, "(select sql from duckdb_views() v where v.view_oid=oid)"},
	{"pg_catalog", "pg_get_constraintdef", {"constraint_oid", nullptr}, {{nullptr, nullptr}}, "(select constraint_text from duckdb_constraints() d_constraint where d_constraint.table_oid=constraint_oid//1000000 and d_constraint.constraint_index=constraint_oid%1000000)"},
	{"pg_catalog", "pg_get_constraintdef", {"constraint_oid", "pretty_bool", nullptr}, {{nullptr, nullptr}}, "pg_get_constraintdef(constraint_oid)"},
	{"pg_catalog", "pg_get_expr", {"pg_node_tree", "relation_oid", nullptr}, {{nullptr, nullptr}}, "pg_node_tree"},
	{"pg_catalog", "format_pg_type", {"logical_type", "type_name", nullptr}, {{nullptr, nullptr}}, "case upper(logical_type) when 'FLOAT' then 'float4' when 'DOUBLE' then 'float8' when 'DECIMAL' then 'numeric' when 'ENUM' then lower(type_name) when 'VARCHAR' then 'varchar' when 'BLOB' then 'bytea' when 'TIMESTAMP' then 'timestamp' when 'TIME' then 'time' when 'TIMESTAMP WITH TIME ZONE' then 'timestamptz' when 'TIME WITH TIME ZONE' then 'timetz' when 'SMALLINT' then 'int2' when 'INTEGER' then 'int4' when 'BIGINT' then 'int8' when 'BOOLEAN' then 'bool' else lower(logical_type) end"},
	{"pg_catalog", "format_type", {"type_oid", "typemod", nullptr}, {{nullptr, nullptr}}, "(select format_pg_type(logical_type, type_name) from duckdb_types() t where t.type_oid=type_oid) || case when typemod>0 then concat('(', typemod//1000, ',', typemod%1000, ')') else '' end"},
	{"pg_catalog", "map_to_pg_oid", {"type_name", nullptr}, {{nullptr, nullptr}}, "case type_name when 'bool' then 16 when 'int16' then 21 when 'int' then 23 when 'bigint' then 20 when 'date' then 1082 when 'time' then 1083 when 'datetime' then 1114 when 'dec' then 1700 when 'float' then 700 when 'double' then 701 when 'bpchar' then 1043 when 'binary' then 17 when 'interval' then 1186 when 'timestamptz' then 1184 when 'timestamp with time zone' then 1184 when 'timetz' then 1266 when 'time with time zone' then 1266 when 'bit' then 1560 when 'guid' then 2950 else null end"}, // map duckdb_oid to pg_oid. If no corresponding type, return null

	{"pg_catalog", "pg_has_role", {"user", "role", "privilege", nullptr}, {{nullptr, nullptr}}, "true"},  //boolean  //does user have privilege for role
	{"pg_catalog", "pg_has_role", {"role", "privilege", nullptr}, {{nullptr, nullptr}}, "true"},  //boolean  //does current user have privilege for role

	{"pg_catalog", "col_description", {"table_oid", "column_number", nullptr}, {{nullptr, nullptr}}, "NULL"},   // get comment for a table column
	{"pg_catalog", "obj_description", {"object_oid", "catalog_name", nullptr}, {{nullptr, nullptr}}, "NULL"},   // get comment for a database object
	{"pg_catalog", "shobj_description", {"object_oid", "catalog_name", nullptr}, {{nullptr, nullptr}}, "NULL"}, // get comment for a shared database object

	// visibility functions
	{"pg_catalog", "pg_collation_is_visible", {"collation_oid", nullptr}, {{nullptr, nullptr}}, "true"},
	{"pg_catalog", "pg_conversion_is_visible", {"conversion_oid", nullptr}, {{nullptr, nullptr}}, "true"},
	{"pg_catalog", "pg_function_is_visible", {"function_oid", nullptr}, {{nullptr, nullptr}}, "true"},
	{"pg_catalog", "pg_opclass_is_visible", {"opclass_oid", nullptr}, {{nullptr, nullptr}}, "true"},
	{"pg_catalog", "pg_operator_is_visible", {"operator_oid", nullptr}, {{nullptr, nullptr}}, "true"},
	{"pg_catalog", "pg_opfamily_is_visible", {"opclass_oid", nullptr}, {{nullptr, nullptr}}, "true"},
	{"pg_catalog", "pg_table_is_visible", {"table_oid", nullptr}, {{nullptr, nullptr}}, "true"},
	{"pg_catalog", "pg_ts_config_is_visible", {"config_oid", nullptr}, {{nullptr, nullptr}}, "true"},
	{"pg_catalog", "pg_ts_dict_is_visible", {"dict_oid", nullptr}, {{nullptr, nullptr}}, "true"},
	{"pg_catalog", "pg_ts_parser_is_visible", {"parser_oid", nullptr}, {{nullptr, nullptr}}, "true"},
	{"pg_catalog", "pg_ts_template_is_visible", {"template_oid", nullptr}, {{nullptr, nullptr}}, "true"},
	{"pg_catalog", "pg_type_is_visible", {"type_oid", nullptr}, {{nullptr, nullptr}}, "true"},

	{"pg_catalog", "pg_size_pretty", {"bytes", nullptr}, {{nullptr, nullptr}}, "format_bytes(bytes)"},
	{"pg_catalog", "pg_sleep", {"seconds", nullptr}, {{nullptr, nullptr}}, "sleep_ms(CAST(seconds * 1000 AS BIGINT))"},

	{DEFAULT_SCHEMA, "round_even", {"x", "n", nullptr}, {{nullptr, nullptr}}, "CASE ((abs(x) * power(10, n+1)) % 10) WHEN 5 THEN round(x/2, n) * 2 ELSE round(x, n) END"},
	{DEFAULT_SCHEMA, "roundbankers", {"x", "n", nullptr}, {{nullptr, nullptr}}, "round_even(x, n)"},
	{DEFAULT_SCHEMA, "nullif", {"a", "b", nullptr}, {{nullptr, nullptr}}, "CASE WHEN a=b THEN NULL ELSE a END"},
	{DEFAULT_SCHEMA, "list_append", {"l", "e", nullptr}, {{nullptr, nullptr}}, "list_concat(l, list_value(e))"},
	{DEFAULT_SCHEMA, "array_append", {"arr", "el", nullptr}, {{nullptr, nullptr}}, "list_append(arr, el)"},
	{DEFAULT_SCHEMA, "list_prepend", {"e", "l", nullptr}, {{nullptr, nullptr}}, "list_concat(list_value(e), l)"},
	{DEFAULT_SCHEMA, "array_prepend", {"el", "arr", nullptr}, {{nullptr, nullptr}}, "list_prepend(el, arr)"},
	{DEFAULT_SCHEMA, "array_pop_back", {"arr", nullptr}, {{nullptr, nullptr}}, "arr[:LEN(arr)-1]"},
	{DEFAULT_SCHEMA, "array_pop_front", {"arr", nullptr}, {{nullptr, nullptr}}, "arr[2:]"},
	{DEFAULT_SCHEMA, "array_push_back", {"arr", "e", nullptr}, {{nullptr, nullptr}}, "list_concat(arr, list_value(e))"},
	{DEFAULT_SCHEMA, "array_push_front", {"arr", "e", nullptr}, {{nullptr, nullptr}}, "list_concat(list_value(e), arr)"},
	{DEFAULT_SCHEMA, "array_to_string", {"arr", "sep", nullptr}, {{nullptr, nullptr}}, "case len(arr::varchar[]) when 0 then '' else list_aggr(arr::varchar[], 'string_agg', sep) end"},
	// Test default parameters
	{DEFAULT_SCHEMA, "array_to_string_comma_default", {"arr", nullptr}, {{"sep", "','"}, {nullptr, nullptr}}, "case len(arr::varchar[]) when 0 then '' else list_aggr(arr::varchar[], 'string_agg', sep) end"},

	{DEFAULT_SCHEMA, "generate_subscripts", {"arr", "dim", nullptr}, {{nullptr, nullptr}}, "unnest(generate_series(1, array_length(arr, dim)))"},
	{DEFAULT_SCHEMA, "fdiv", {"x", "y", nullptr}, {{nullptr, nullptr}}, "floor(x/y)"},
	{DEFAULT_SCHEMA, "fmod", {"x", "y", nullptr}, {{nullptr, nullptr}}, "(x-y*floor(x/y))"},
	{DEFAULT_SCHEMA, "split_part", {"string", "delimiter", "position", nullptr}, {{nullptr, nullptr}}, "if(string IS NOT NULL AND delimiter IS NOT NULL AND position IS NOT NULL, coalesce(string_split(string, delimiter)[position],''), NULL)"},
	{DEFAULT_SCHEMA, "geomean", {"x", nullptr}, {{nullptr, nullptr}}, "exp(avg(ln(x)))"},
	{DEFAULT_SCHEMA, "geometric_mean", {"x", nullptr}, {{nullptr, nullptr}}, "geomean(x)"},

	{DEFAULT_SCHEMA, "weighted_avg", {"value", "weight", nullptr}, {{nullptr, nullptr}}, "SUM(value * weight) / SUM(CASE WHEN value IS NOT NULL THEN weight ELSE 0 END)"},
	{DEFAULT_SCHEMA, "wavg", {"value", "weight", nullptr}, {{nullptr, nullptr}}, "weighted_avg(value, weight)"},

    {DEFAULT_SCHEMA, "list_reverse", {"l", nullptr}, {{nullptr, nullptr}}, "l[:-:-1]"},
    {DEFAULT_SCHEMA, "array_reverse", {"l", nullptr}, {{nullptr, nullptr}}, "list_reverse(l)"},

	// algebraic list aggregates
	{DEFAULT_SCHEMA, "list_avg", {"l", nullptr}, {{nullptr, nullptr}}, "list_aggr(l, 'avg')"},
	{DEFAULT_SCHEMA, "list_var_samp", {"l", nullptr}, {{nullptr, nullptr}}, "list_aggr(l, 'var_samp')"},
	{DEFAULT_SCHEMA, "list_var_pop", {"l", nullptr}, {{nullptr, nullptr}}, "list_aggr(l, 'var_pop')"},
	{DEFAULT_SCHEMA, "list_stddev_pop", {"l", nullptr}, {{nullptr, nullptr}}, "list_aggr(l, 'stddev_pop')"},
	{DEFAULT_SCHEMA, "list_stddev_samp", {"l", nullptr}, {{nullptr, nullptr}}, "list_aggr(l, 'stddev_samp')"},
	{DEFAULT_SCHEMA, "list_sem", {"l", nullptr}, {{nullptr, nullptr}}, "list_aggr(l, 'sem')"},

	// distributive list aggregates
	{DEFAULT_SCHEMA, "list_approx_count_distinct", {"l", nullptr}, {{nullptr, nullptr}}, "list_aggr(l, 'approx_count_distinct')"},
	{DEFAULT_SCHEMA, "list_bit_xor", {"l", nullptr}, {{nullptr, nullptr}}, "list_aggr(l, 'bit_xor')"},
	{DEFAULT_SCHEMA, "list_bit_or", {"l", nullptr}, {{nullptr, nullptr}}, "list_aggr(l, 'bit_or')"},
	{DEFAULT_SCHEMA, "list_bit_and", {"l", nullptr}, {{nullptr, nullptr}}, "list_aggr(l, 'bit_and')"},
	{DEFAULT_SCHEMA, "list_bool_and", {"l", nullptr}, {{nullptr, nullptr}}, "list_aggr(l, 'bool_and')"},
	{DEFAULT_SCHEMA, "list_bool_or", {"l", nullptr}, {{nullptr, nullptr}}, "list_aggr(l, 'bool_or')"},
	{DEFAULT_SCHEMA, "list_count", {"l", nullptr}, {{nullptr, nullptr}}, "list_aggr(l, 'count')"},
	{DEFAULT_SCHEMA, "list_entropy", {"l", nullptr}, {{nullptr, nullptr}}, "list_aggr(l, 'entropy')"},
	{DEFAULT_SCHEMA, "list_last", {"l", nullptr}, {{nullptr, nullptr}}, "list_aggr(l, 'last')"},
	{DEFAULT_SCHEMA, "list_first", {"l", nullptr}, {{nullptr, nullptr}}, "list_aggr(l, 'first')"},
	{DEFAULT_SCHEMA, "list_any_value", {"l", nullptr}, {{nullptr, nullptr}}, "list_aggr(l, 'any_value')"},
	{DEFAULT_SCHEMA, "list_kurtosis", {"l", nullptr}, {{nullptr, nullptr}}, "list_aggr(l, 'kurtosis')"},
	{DEFAULT_SCHEMA, "list_kurtosis_pop", {"l", nullptr}, {{nullptr, nullptr}}, "list_aggr(l, 'kurtosis_pop')"},
	{DEFAULT_SCHEMA, "list_min", {"l", nullptr}, {{nullptr, nullptr}}, "list_aggr(l, 'min')"},
	{DEFAULT_SCHEMA, "list_max", {"l", nullptr}, {{nullptr, nullptr}}, "list_aggr(l, 'max')"},
	{DEFAULT_SCHEMA, "list_product", {"l", nullptr}, {{nullptr, nullptr}}, "list_aggr(l, 'product')"},
	{DEFAULT_SCHEMA, "list_skewness", {"l", nullptr}, {{nullptr, nullptr}}, "list_aggr(l, 'skewness')"},
	{DEFAULT_SCHEMA, "list_sum", {"l", nullptr}, {{nullptr, nullptr}}, "list_aggr(l, 'sum')"},
	{DEFAULT_SCHEMA, "list_string_agg", {"l", nullptr}, {{nullptr, nullptr}}, "list_aggr(l, 'string_agg')"},

	// holistic list aggregates
	{DEFAULT_SCHEMA, "list_mode", {"l", nullptr}, {{nullptr, nullptr}}, "list_aggr(l, 'mode')"},
	{DEFAULT_SCHEMA, "list_median", {"l", nullptr}, {{nullptr, nullptr}}, "list_aggr(l, 'median')"},
	{DEFAULT_SCHEMA, "list_mad", {"l", nullptr}, {{nullptr, nullptr}}, "list_aggr(l, 'mad')"},

	// nested list aggregates
	{DEFAULT_SCHEMA, "list_histogram", {"l", nullptr}, {{nullptr, nullptr}}, "list_aggr(l, 'histogram')"},

	// map functions
	{DEFAULT_SCHEMA, "map_contains_entry", {"map", "key", "value"}, {{nullptr, nullptr}}, "contains(map_entries(map), {'key': key, 'value': value})"},
	{DEFAULT_SCHEMA, "map_contains_value", {"map", "value", nullptr}, {{nullptr, nullptr}}, "contains(map_values(map), value)"},

	// date functions
	{DEFAULT_SCHEMA, "date_add", {"date", "interval", nullptr}, {{nullptr, nullptr}}, "date + interval"},

	// date functions - convenience macro for getting days in month
	{DEFAULT_SCHEMA, "days_in_month", {"date", nullptr}, {{nullptr, nullptr}}, "day(last_day(date))"},

	// timestamptz functions
	{DEFAULT_SCHEMA, "ago", {"i", nullptr}, {{nullptr, nullptr}}, "current_timestamp - i::interval"},

	// regexp functions
	{DEFAULT_SCHEMA, "regexp_split_to_table", {"text", "pattern", nullptr}, {{nullptr, nullptr}}, "unnest(string_split_regex(text, pattern))"},

	// storage helper functions
	{DEFAULT_SCHEMA, "get_block_size", {"db_name"}, {{nullptr, nullptr}}, "(SELECT block_size FROM pragma_database_size() WHERE database_name = db_name)"},

	// string functions
	{DEFAULT_SCHEMA, "md5_number_upper", {"param"}, {{nullptr, nullptr}}, "((md5_number(param)::bit::varchar)[65:])::bit::uint64"},
	{DEFAULT_SCHEMA, "md5_number_lower", {"param"}, {{nullptr, nullptr}}, "((md5_number(param)::bit::varchar)[:64])::bit::uint64"},

	// mysql date function
	{DEFAULT_SCHEMA, "convert_tz", {"timestamp", "tz1", "tz2", nullptr}, {{nullptr, nullptr}}, "timezone(tz2, timezone(tz1, timestamp))"},
	{DEFAULT_SCHEMA, "datediff", {"date1", "date2", nullptr}, {{nullptr, nullptr}}, "date_diff('day', date2, date1)"},
	{DEFAULT_SCHEMA, "date_sub", {"date", "interval", nullptr}, {{nullptr, nullptr}}, "date - interval"},
	{DEFAULT_SCHEMA, "date", {"expr", nullptr}, {{nullptr, nullptr}}, "cast(expr as DATE)"},
	{DEFAULT_SCHEMA, "addtime", {"expr1", "expr2", nullptr}, {{nullptr, nullptr}}, "expr1 + to_days_duckdb(if(split_part(expr2, ' ', -2)=='', 0, cast(split_part(expr2, ' ', -2) as int))) + to_seconds_duckdb(epoch(cast(split_part(expr2, ' ', -1) as TIME)))"},
	{DEFAULT_SCHEMA, "to_days", {"expr1", nullptr}, {{nullptr, nullptr}}, "cast(cast(expr1 as timestamp) as date) - DATE '0000-01-01'"},
	{DEFAULT_SCHEMA, "to_seconds", {"expr1", nullptr}, {{nullptr, nullptr}}, "epoch(cast(expr1 as timestamp) - TIMESTAMP '0000-01-01')"},
	// The function timediff will return incorrect result due to the different domain, so remove it.
	// {DEFAULT_SCHEMA, "timediff", {"expr1", "expr2", nullptr}, {{nullptr, nullptr}}, "expr1 - expr2 + TIMESTAMP '1970-01-01 00:00:00'"},
	{DEFAULT_SCHEMA, "time_to_sec", {"expr1", nullptr}, {{nullptr, nullptr}}, "epoch(cast(expr1 as time))"},
	{DEFAULT_SCHEMA, "subtime", {"expr1", "expr2", nullptr}, {{nullptr, nullptr}}, "expr1 - to_days_duckdb(if(split_part(expr2, ' ', -2)=='', 0, cast(split_part(expr2, ' ', -2) as int))) - to_seconds_duckdb(epoch(cast(split_part(expr2, ' ', -1) as TIME)))"},
	{DEFAULT_SCHEMA, "sec_to_time", {"expr1", nullptr}, {{nullptr, nullptr}}, "cast((TIME '00:00:00' + to_seconds_duckdb(expr1)) AS TIME)"},
	{DEFAULT_SCHEMA, "from_days", {"expr1", nullptr}, {{nullptr, nullptr}}, "DATE '0000-01-01' + to_days_duckdb(cast(expr1 as int))"},
	{DEFAULT_SCHEMA, "makedate", {"expr1", "expr2", nullptr}, {{nullptr, nullptr}}, "if(expr2 = 0, NULL, DATE '0000-01-01' + to_years(cast(expr1 as integer)) + to_days_duckdb(cast(expr2 as integer) - 1))"},
	{DEFAULT_SCHEMA, "period_add", {"p", "m", nullptr}, {{nullptr, nullptr}}, "strftime(CASE  WHEN length(CAST(p AS char)) <= 4 THEN  CASE  WHEN substring(LPAD(CAST(p AS char), 4, '0'), 1, 2) >= '70' THEN strptime(concat('19', LPAD(CAST(p AS char), 4, '0'), '01'), '%Y%m%d') ELSE strptime(concat('20', LPAD(CAST(p AS char), 4, '0'), '01'), '%Y%m%d') END ELSE strptime(CONCAT(LPAD(CAST(p AS char), 6, '0'), '01'), '%Y%m%d') END + to_months(m), '%Y%m')"},
	{DEFAULT_SCHEMA, "period_diff", {"p1", "p2", nullptr}, {{nullptr, nullptr}}, "date_diff('month', CASE  WHEN length(CAST(p2 AS VARCHAR)) <= 4 THEN strptime(CONCAT(CASE  WHEN substring(LPAD(CAST(p2 AS char), 4, '0'), 1, 2) >= '70' THEN '19' ELSE '20' END, LPAD(CAST(p2 AS char), 4, '0'), '01'), '%Y%m%d') ELSE strptime(CONCAT(LPAD(CAST(p2 AS char), 6, '0'), '01'), '%Y%m%d') END, CASE  WHEN length(CAST(p1 AS VARCHAR)) = 4 THEN strptime(concat(CASE  WHEN substring(LPAD(CAST(p1 AS char), 4, '0'), 1, 2) >= '70' THEN '19' ELSE '20' END, LPAD(CAST(p1 AS char), 4, '0'), '01'), '%Y%m%d') ELSE strptime(concat(LPAD(CAST(p1 AS char), 6, '0'), '01'), '%Y%m%d') END)"},
	{DEFAULT_SCHEMA, "maketime", {"h", "m", "s", nullptr}, {{nullptr, nullptr}}, "make_time(h, m, s)"},
	{DEFAULT_SCHEMA, "current_time", {nullptr}, {{nullptr, nullptr}}, "cast(get_current_time() AS time)"},
	{DEFAULT_SCHEMA, "current_time", {"fsp", nullptr}, {{nullptr, nullptr}}, "cast(get_current_time() AS time)"},
	{DEFAULT_SCHEMA, "curtime", {nullptr}, {{nullptr, nullptr}}, "cast(get_current_time() AS time)"},
	{DEFAULT_SCHEMA, "curtime", {"fsp", nullptr}, {{nullptr, nullptr}}, "cast(get_current_time() AS time)"},
	{DEFAULT_SCHEMA, "from_unixtime", {"sec", nullptr}, {{nullptr, nullptr}}, "make_timestamptz(cast(cast(sec as double) * 1000000 as bigint))"},
	{DEFAULT_SCHEMA, "from_unixtime", {"sec", "format", nullptr}, {{nullptr, nullptr}}, "strftime(make_timestamptz(cast(cast(sec as double) * 1000000 as bigint)), format)"},

	// mysql string function
	{DEFAULT_SCHEMA, "locate", {"substr", "str", nullptr}, {{nullptr, nullptr}}, "position(substr IN str)"},
	{DEFAULT_SCHEMA, "locate", {"substr", "str", "pos", nullptr}, {{nullptr, nullptr}}, "position(substr IN str[pos:]) + if(position(substr IN str[pos:]), pos - 1, 0)"},
	{DEFAULT_SCHEMA, "strcmp", {"expr1", "expr2", nullptr}, {{nullptr, nullptr}}, "if(expr1 is null or expr2 is null, null, if(cast(expr1 as char) >= cast(expr2 as char), if(cast(expr1 as char) = cast(expr2 as char), 0, 1), -1))"},
	{DEFAULT_SCHEMA, "substring_index", {"expr1", "sep", "index", nullptr}, {{nullptr, nullptr}}, "if(expr1 is null or sep is null or index is null, null, if(index != 0, if((index > 0), list_reduce((split(expr1, sep)[:index]), (ret, x)->concat_ws(sep, ret, x)), list_reduce((split(expr1, sep)[index:]), (ret, x)->concat_ws(sep, ret, x))), ''))"},
	{DEFAULT_SCHEMA, "space", {"num", nullptr}, {{nullptr, nullptr}}, "repeat(' ', cast(num as double))"},
	{DEFAULT_SCHEMA, "insert", {"str", "pos", "len", "newstr", nullptr}, {{nullptr, nullptr}}, "if(str is null or pos is null or len is null or newstr is null, null, if(pos between 1 and length(str), concat(cast(str as varchar)[:pos - 1], cast(newstr as varchar), cast(str as varchar)[pos+len:]), str))"},
	{DEFAULT_SCHEMA, "not_regexp_like", {"str", "pattern", nullptr}, {{nullptr, nullptr}}, "not regexp_like(str, pattern)"},

	// mysql numeric function
	{DEFAULT_SCHEMA, "rand", {nullptr}, {{nullptr, nullptr}}, "random()"},
	{DEFAULT_SCHEMA, "mod", {"n", "m", nullptr}, {{nullptr, nullptr}}, "n % m"},
	{nullptr, nullptr, {nullptr}, {{nullptr, nullptr}}, nullptr}
	};

unique_ptr<CreateMacroInfo> DefaultFunctionGenerator::CreateInternalMacroInfo(const DefaultMacro &default_macro) {
	return CreateInternalMacroInfo(array_ptr<const DefaultMacro>(default_macro));
}


unique_ptr<CreateMacroInfo> DefaultFunctionGenerator::CreateInternalMacroInfo(array_ptr<const DefaultMacro> macros) {
	auto type = CatalogType::MACRO_ENTRY;
	auto bind_info = make_uniq<CreateMacroInfo>(type);
	for(auto &default_macro : macros) {
		// parse the expression
		auto expressions = Parser::ParseExpressionList(default_macro.macro);
		D_ASSERT(expressions.size() == 1);

		auto function = make_uniq<ScalarMacroFunction>(std::move(expressions[0]));
		for (idx_t param_idx = 0; default_macro.parameters[param_idx] != nullptr; param_idx++) {
			function->parameters.push_back(
			    make_uniq<ColumnRefExpression>(default_macro.parameters[param_idx]));
		}
		for (idx_t named_idx = 0; default_macro.named_parameters[named_idx].name != nullptr; named_idx++) {
			const auto &named_param = default_macro.named_parameters[named_idx];
			auto expr_list = Parser::ParseExpressionList(named_param.default_value);
			if (expr_list.size() != 1) {
				throw InternalException("Expected a single expression");
			}
			function->parameters.push_back(make_uniq<ColumnRefExpression>(named_param.name));
			function->default_parameters.insert(make_pair(named_param.name, std::move(expr_list[0])));
		}
		D_ASSERT(function->type == MacroType::SCALAR_MACRO);
		bind_info->macros.push_back(std::move(function));
	}
	bind_info->schema = macros[0].schema;
	bind_info->name = macros[0].name;
	bind_info->temporary = true;
	bind_info->internal = true;
	return bind_info;
}

static bool DefaultFunctionMatches(const DefaultMacro &macro, const string &schema, const string &name) {
	return macro.schema == schema && macro.name == name;
}

static unique_ptr<CreateFunctionInfo> GetDefaultFunction(const string &input_schema, const string &input_name) {
	auto schema = StringUtil::Lower(input_schema);
	auto name = StringUtil::Lower(input_name);
	for (idx_t index = 0; internal_macros[index].name != nullptr; index++) {
		if (DefaultFunctionMatches(internal_macros[index], schema, name)) {
			// found the function! keep on iterating to find all overloads
			idx_t overload_count;
			for(overload_count = 1; internal_macros[index + overload_count].name; overload_count++) {
				if (!DefaultFunctionMatches(internal_macros[index + overload_count], schema, name)) {
					break;
				}
			}
			return DefaultFunctionGenerator::CreateInternalMacroInfo(array_ptr<const DefaultMacro>(internal_macros + index, overload_count));
		}
	}
	return nullptr;
}

DefaultFunctionGenerator::DefaultFunctionGenerator(Catalog &catalog, SchemaCatalogEntry &schema)
    : DefaultGenerator(catalog), schema(schema) {
}

unique_ptr<CatalogEntry> DefaultFunctionGenerator::CreateDefaultEntry(ClientContext &context,
                                                                      const string &entry_name) {
	auto info = GetDefaultFunction(schema.name, entry_name);
	if (info) {
		return make_uniq_base<CatalogEntry, ScalarMacroCatalogEntry>(catalog, schema, info->Cast<CreateMacroInfo>());
	}
	return nullptr;
}

vector<string> DefaultFunctionGenerator::GetDefaultEntries() {
	vector<string> result;
	for (idx_t index = 0; internal_macros[index].name != nullptr; index++) {
		if (StringUtil::Lower(internal_macros[index].name) != internal_macros[index].name) {
			throw InternalException("Default macro name %s should be lowercase", internal_macros[index].name);
		}
		if (internal_macros[index].schema == schema.name) {
			result.emplace_back(internal_macros[index].name);
		}
	}
	return result;
}

} // namespace duckdb
