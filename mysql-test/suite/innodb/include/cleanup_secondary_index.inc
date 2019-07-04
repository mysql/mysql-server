--connection view_keeper
  ROLLBACK;

--connection default
--disconnect view_keeper

  DROP TABLE t1;