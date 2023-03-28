# Run it only if --big-test option is specified
-- source include/big_test.inc

--echo #
--echo # Bug#110494 "Deadlock between FLUSH STATUS, COM_CHANGE_USER
--echo # and SELECT FROM I_S.PROCESSLIST".
--echo #

--echo # The original problem reported was that concurrent execution of
--echo # COM_STATISTICS, COM_CHANGE_USER commands and SHOW FULL PROCESSLIST
--echo # statements sometimes led to deadlock. This test uses FLUSH STATUS
--echo # statement instead of the first command and SELECT ... FROM
--echo # I_S.PROCESSLIST instead of the latter. They acquire the same
--echo # locks and were affected by the same problem.
--echo # Doing 3000 concurrent runs of each statement was enough to reproduce
--echo # the deadlock with 80% probability on my machine.
--echo # Hence, the test doesn't reproduce the issue consistently. It is observed
--echo # that the test fails when run with mtr option --repeat=10 or more.
--echo # Also, it is hard to write a MTR test using DEBUG_SYNC, because
--echo # MTR doesn't allow us to run --change_user in background.

--delimiter |

CREATE PROCEDURE p_flush_status()
BEGIN
  DECLARE x INT DEFAULT 3000;
  WHILE x DO
    SET x = x-1;
    FLUSH STATUS;
  END WHILE;
END |

CREATE PROCEDURE p_processlist()
BEGIN
  DECLARE x INT DEFAULT 3000;
  WHILE x DO
    SET x = x-1;
    SELECT COUNT(*) INTO @a FROM information_schema.processlist;
  END WHILE;
END |

--delimiter ;

--enable_connect_log
--connect (con1, localhost, root,,)
--echo # Send:
--send CALL p_flush_status()

--echo # Send:
--connect (con2, localhost, root,,)
--send CALL p_processlist()

--connection default

--echo # Execute COM_CHANGE_USER command 3000 times.
let $i = 3000;
while ($i)
{
  dec $i;
--change_user
}

--connection con1
--echo # Reap p_flush_status().
--reap
--disconnect con1
--source include/wait_until_disconnected.inc

--connection con2
--echo # Reap p_processlist().
--reap
--disconnect con2
--source include/wait_until_disconnected.inc

--connection default
--disable_connect_log
DROP PROCEDURE p_flush_status;
DROP PROCEDURE p_processlist;
