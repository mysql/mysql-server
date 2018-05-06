#
# Bug#24286064: JSON MEMORY USAGE MISSING FROM THE MEMORY SUMMARY TABLES
#
SELECT CAST(123 AS JSON);
CAST(123 AS JSON)
123
SELECT event_name, count_alloc > 0, count_alloc = count_free,
sum_number_of_bytes_alloc > 0,
sum_number_of_bytes_alloc = sum_number_of_bytes_free
FROM performance_schema.memory_summary_global_by_event_name
WHERE event_name = 'memory/sql/JSON';
event_name	count_alloc > 0	count_alloc = count_free	sum_number_of_bytes_alloc > 0	sum_number_of_bytes_alloc = sum_number_of_bytes_free
memory/sql/JSON	1	1	1	1
