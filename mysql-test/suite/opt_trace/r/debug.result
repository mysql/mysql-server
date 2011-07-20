# We want to make sure that the common case of
# a connection which has not enabled tracing,
# is optimized, i.e. neither Opt_trace_context_impl nor
# Opt_trace_context_stmt is created.
set debug="d,no_new_opt_trace_stmt";
select 1;
1
1
select * from information_schema.OPTIMIZER_TRACE;
QUERY	TRACE	MISSING_BYTES_BEYOND_MAX_MEM_SIZE	INSUFFICIENT_PRIVILEGES
set @a=25;
set debug="default";
set optimizer_trace="enabled=on";
select 2;
2
2
select * from information_schema.OPTIMIZER_TRACE;
QUERY	TRACE	MISSING_BYTES_BEYOND_MAX_MEM_SIZE	INSUFFICIENT_PRIVILEGES
select 2	{
  "steps": [
    {
      "join_preparation": {
        "select#": 1,
        "steps": [
          {
            "expanded_query": "/* select#1 */ select 2 AS `2`"
          }
        ]
      }
    },
    {
      "join_optimization": {
        "select#": 1,
        "steps": [
        ]
      }
    },
    {
      "join_execution": {
        "select#": 1,
        "steps": [
        ]
      }
    }
  ]
}	0	0
set optimizer_trace="enabled=off";
set debug="d,no_new_opt_trace_stmt";
select 3;
3
3
select * from information_schema.OPTIMIZER_TRACE;
QUERY	TRACE	MISSING_BYTES_BEYOND_MAX_MEM_SIZE	INSUFFICIENT_PRIVILEGES
select 2	{
  "steps": [
    {
      "join_preparation": {
        "select#": 1,
        "steps": [
          {
            "expanded_query": "/* select#1 */ select 2 AS `2`"
          }
        ]
      }
    },
    {
      "join_optimization": {
        "select#": 1,
        "steps": [
        ]
      }
    },
    {
      "join_execution": {
        "select#": 1,
        "steps": [
        ]
      }
    }
  ]
}	0	0
