select (@err_log:=variable_value)*0 from performance_schema.global_variables where variable_name="log_error";
(@err_log:=variable_value)*0
0
select instr(@err_log, ".err")>0;
instr(@err_log, ".err")>0
1
