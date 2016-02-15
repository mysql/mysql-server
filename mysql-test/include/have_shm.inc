# Only run this test if shared memory is available
let $shm= query_get_value("SHOW VARIABLES LIKE 'shared_memory'", Value, 1);
if ($shm != ON){
  skip No shm support;
}
