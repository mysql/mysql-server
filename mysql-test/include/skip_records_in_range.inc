SET optimizer_trace="enabled=on";
eval $query;
SELECT TRACE INTO @trace FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;
SELECT @trace RLIKE "skipped_due_to_force_index";
SET optimizer_trace="enabled=off";

let $format=traditional;
--source include/explain_other.inc

let $format=json;
--source include/explain_other.inc
