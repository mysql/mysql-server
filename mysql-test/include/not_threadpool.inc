if (`SELECT count(*) FROM information_schema.GLOBAL_VARIABLES WHERE
      VARIABLE_NAME = 'THREAD_HANDLING' AND
      VARIABLE_VALUE = 'loaded-dynamically'`){
  skip Test requires: 'not_threadpool';
}
