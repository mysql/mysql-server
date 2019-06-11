#
# Bug #23755664 DEADLOCK WITH 3 CONCURRENT DELETES BY UNIQUE KEY
#
CREATE TABLE `t`(
`id` INT,
`a` INT DEFAULT NULL,
PRIMARY KEY(`id`),
UNIQUE KEY `u`(`a`)
) ENGINE=InnoDB;
INSERT INTO t (`id`,`a`) VALUES
(1,1),
(2,9999),
(3,10000);
SET DEBUG_SYNC =
'lock_sec_rec_read_check_and_lock_has_locked
      SIGNAL deleter_has_locked
      WAIT_FOR waiter_has_locked';
DELETE FROM t WHERE a = 9999;
SET DEBUG_SYNC=
'now WAIT_FOR deleter_has_locked';
SET DEBUG_SYNC=
'lock_sec_rec_read_check_and_lock_has_locked SIGNAL holder_has_locked';
DELETE FROM t WHERE a = 9999;
SET DEBUG_SYNC=
'now WAIT_FOR holder_has_locked';
SET DEBUG_SYNC=
'lock_sec_rec_read_check_and_lock_has_locked SIGNAL waiter_has_locked';
DELETE FROM t WHERE a = 9999;
DROP TABLE `t`;
CREATE TABLE `t`(
`id` INT NOT NULL PRIMARY KEY
) ENGINE=InnoDB;
INSERT INTO t (`id`) VALUES (1), (2);
BEGIN;
SELECT id FROM t WHERE id=1 FOR UPDATE;
id
1
SELECT id FROM t WHERE id=2 FOR UPDATE;
id
2
SET DEBUG_SYNC=
'lock_wait_will_wait SIGNAL waiter_will_wait';
SELECT id FROM t WHERE id = 1 FOR UPDATE;
SET DEBUG_SYNC=
'now WAIT_FOR waiter_will_wait';
SELECT * FROM t FOR UPDATE;
id
1
2
COMMIT;
id
1
DROP TABLE `t`;
