INSERT INTO db1.trans (a) VALUES (1);
--disable_result_log
eval $statement;
--enable_result_log
CALL db1.test_if_commit();
