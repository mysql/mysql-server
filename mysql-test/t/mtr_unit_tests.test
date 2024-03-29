# This test runs all the perl unit test testing MTR libraries.
# It runs the separate perl processes to not get any variables from the actual
# MTR run. Only the environment is shared and used to extract needed paths.

--exec perl $MYSQL_TEST_DIR/lib/t/Base.t 2>&1
--exec perl $MYSQL_TEST_DIR/lib/t/copytree.t 2>&1
--exec perl $MYSQL_TEST_DIR/lib/t/Find.t 2>&1
--exec perl $MYSQL_TEST_DIR/lib/t/Options.t 2>&1
--exec perl $MYSQL_TEST_DIR/lib/t/rmtree.t 2>&1
--exec perl $MYSQL_TEST_DIR/lib/t/SafeProcess.t 2>&1
--exec perl $MYSQL_TEST_DIR/lib/t/testMyConfig.t 2>&1
--exec perl $MYSQL_TEST_DIR/lib/t/testMyConfigFactory.t 2>&1
