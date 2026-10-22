# Copyright 2026 Google LLC

# Wait for the vector index to be in the INDEX_READY_TO_USE state
let $wait_condition = select count(*)=0 from information_schema.innodb_vector_indexes where state!='INDEX_READY_TO_USE';
--source include/wait_condition.inc
