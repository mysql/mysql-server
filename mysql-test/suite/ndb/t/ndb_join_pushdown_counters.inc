#
# Show the counter diff compared to test start
# Note: These tables are 'temporary' within 'connection spj'
#
connection spj;

# Show the change in mysqld counters.
--disable_query_log ONCE
--disable_warnings ONCE
select new.variable_name variable_name,
       new.variable_value - old.variable_value as variable_value
       from server_counts_at_startup as old,
         information_schema.global_status as new
       where new.variable_name = old.variable_name
       order by new.variable_name;
