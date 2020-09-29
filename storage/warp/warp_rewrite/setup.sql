#a vim: set ts=2: set expandtab:
DELIMITER ;;
/*  Pro Parallel (async) (part of the Swanhart Toolkit)
    Copyright 2015 Justin Swanhart

    async is free software: you can redistribute it and/or modify
    it under the terms of the Lesser GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    async is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with FlexViews in the file COPYING, and the Lesser extension to
    the GPL (the LGPL) in COPYING.LESSER.
    If not, see <http://www.gnu.org/licenses/>.
*/
SET NAMES UTF8;

select 'Dropping and creating warpsql database' as message;

DROP DATABASE IF EXISTS warpsql;

CREATE DATABASE IF NOT EXISTS warpsql;

USE warpsql;

CREATE DEFINER=root@localhost PROCEDURE warpsql.check_reqs()
READS SQL DATA
SQL SECURITY DEFINER
BEGIN
  IF(VERSION() NOT LIKE '8.%') THEN
		SIGNAL SQLSTATE '99999'
		SET MESSAGE_TEXT = 'MySQL version 8 is needed for WarpSQL functionality';
  END IF;
END;;

CALL check_reqs();

select 'Creating setup procedure' as message;

DROP PROCEDURE IF EXISTS setup;;

CREATE DEFINER=root@localhost PROCEDURE warpsql.setup()
MODIFIES SQL DATA
SQL SECURITY DEFINER
BEGIN
	CALL check_reqs();

  -- LIFO queue of queries
  CREATE TABLE warpsql.q (
    q_id bigint auto_increment primary key,
    sql_text longtext not null,
    created_on timestamp,
    started_on datetime default null,
    completed_on datetime default null,
    parent bigint default null, 
    completed boolean default FALSE,
    state enum ('CHECKING','COMPLETED','WAITING','RUNNING','ERROR') NOT NULL DEFAULT 'WAITING',
    errno VARCHAR(10) DEFAULT NULL,
    errmsg TEXT DEFAULT NULL,
    sink text default null comment 'Table in which to store results',
    partition_filter text default null comment 'Partition to filter in alias.part_name format'
  ) CHARSET=UTF8MB4
  ENGINE=INNODB;

  CREATE TABLE settings(
    variable varchar(64) primary key, 
    value varchar(64) DEFAULT 'thread_count'
  ) CHARSET=UTF8MB4 
  ENGINE=InnoDB;
  
  INSERT INTO warpsql.settings VALUES ('thread_count', '8');
      
END;;

DROP PROCEDURE IF EXISTS warpsql.run_worker;;

CREATE DEFINER=root@localhost PROCEDURE warpsql.worker()
MODIFIES SQL DATA
SQL SECURITY DEFINER
worker:BEGIN
  DECLARE v_thread_count BIGINT DEFAULT 8;
  DECLARE v_running_threads BIGINT DEFAULT 0;
  DECLARE v_next_thread BIGINT DEFAULT 0;
  DECLARE v_got_lock BOOLEAN DEFAULT FALSE;
  DECLARE v_q_id BIGINT DEFAULT 0;
  DECLARE v_sql_text LONGTEXT DEFAULT NULL;

  -- FIXME: 
  -- These control the backoff in the loop waiting
  -- for queries to be placed in the q table.  You
  -- can change the values here but they should be
  -- sufficient for most setups. I'll convert them
  -- to configuration variables later.
  DECLARE v_wait FLOAT DEFAULT 0;
  DECLARE v_min_wait FLOAT DEFAULT 0;
  DECLARE v_inc_wait FLOAT DEFAULT 0.01;
  DECLARE v_max_wait FLOAT DEFAULT 0.25;

  SELECT `value`
    INTO v_thread_count
    FROM warpsql.settings
   WHERE variable = 'thread_count';

  IF(v_thread_count IS NULL) THEN
    SIGNAL SQLSTATE '99999'
       SET MESSAGE_TEXT = 'assertion: missing thread_count variable';
  END IF;

	SET v_next_thread := 0;
	
  start_thread:LOOP
    IF(v_next_thread > v_thread_count) THEN
			DO sleep(v_max_wait);  
      LEAVE worker;
    END IF;
    SET v_next_thread := v_next_thread + 1;
  
    SELECT GET_LOCK(CONCAT('thread#', v_next_thread),0) INTO v_got_lock;  
    IF(v_got_lock IS NULL OR v_got_lock = 0) THEN
			ITERATE start_thread;
    END IF;

    LEAVE start_thread;

  END LOOP;

  -- already enough threads running so exit the stored routine
  IF(v_next_thread > v_thread_count) THEN
    LEAVE worker;
  END IF;

  -- The execution loop to grab queries from the queue and run them
  run_block:BEGIN
    DECLARE CONTINUE HANDLER FOR SQLEXCEPTION
    BEGIN
      GET DIAGNOSTICS CONDITION 1
      @errno = RETURNED_SQLSTATE, @errmsg = MESSAGE_TEXT;
      SET v_wait := v_min_wait;
    END;
 
    run_loop:LOOP
      set @errno = NULL;
      set @errmsg = NULL;
      START TRANSACTION;

      -- get the next SQL to run.  

      SELECT q_id,
             sql_text
        INTO v_q_id,
             v_sql_text
        FROM q
       WHERE completed = FALSE
         AND state = 'WAITING'
       ORDER BY q_id
      LIMIT 1 
 			FOR UPDATE;

      -- Increase the wait if there was no SQL found to
      -- execute.  
      SET @errno := NULL;

      IF(v_q_id = 0 OR v_q_id IS NULL) THEN

        SET v_wait := v_wait + v_inc_wait;
        IF(v_wait > v_max_wait) THEN
          SET v_wait := v_max_wait;
        END IF;

        -- let go of the lock on the q!
        ROLLBACK; 

      ELSE 

        -- mark it as running
        UPDATE q 
           SET started_on = NOW(), 
                 state='RUNNING'
         WHERE q_id = v_q_id;

        -- unlock the record (don't block the q)
        COMMIT;

        START TRANSACTION;

        -- the output of the SELECT statement must go into a table
        -- other statements like INSERT or CALL can not return a 
        -- resultset
        IF(SUBSTR(TRIM(LOWER(v_sql_text)),1,6) = 'select') THEN
          SET @v_sql := CONCAT('CREATE TABLE warpsql.rs_', v_q_id, ' ENGINE=MYISAM AS ', v_sql_text);
				ELSE
					SET @v_sql := v_sql_text;
        END IF;

        PREPARE stmt FROM @v_sql;
        IF(@errno IS NULL) THEN
					-- lock the query for exec
					SELECT * from q where q_id = v_q_id FOR UPDATE;
        	EXECUTE stmt;
					DEALLOCATE PREPARE stmt;
				END IF;

        UPDATE q
           SET state = IF(@errno IS NULL, 'COMPLETED', 'ERROR'),
               errno = @errno,
               errmsg = @errmsg,
               completed = TRUE,
               completed_on = SYSDATE()
         WHERE q_id = v_q_id;

        COMMIT;

        -- reset the wait time to min because something
        -- was executed
        SET v_wait := v_min_wait;
      END IF;

			SET v_q_id := 0;

      -- wait a bit for the next SQL so that we aren't
      -- spamming MySQL with queries to execute an
      -- empty queue
      DO SLEEP(v_wait);

    END LOOP;  

  END;
  
END;;

CREATE DEFINER=root@localhost PROCEDURE warpsql.check(IN v_q_id BIGINT)
MODIFIES SQL DATA
SQL SECURITY DEFINER
BEGIN
	SELECT * from q where q_id = v_q_id;	
END;;

CREATE DEFINER=root@localhost PROCEDURE warpsql.wait(IN v_q_id BIGINT)
MODIFIES SQL DATA
SQL SECURITY DEFINER
BEGIN
	DECLARE v_errmsg TEXT;
  DECLARE v_errno TEXT;
  DECLARE v_status TEXT;
	-- this will block when the query is running
	SELECT state, errmsg, errno INTO v_status,v_errmsg, v_errno from q where q_id = v_q_id ;
	IF (v_status IS NULL) THEN
    SIGNAL SQLSTATE '99998'
       SET MESSAGE_TEXT='Invalid QUERY_NUMBER';
  END IF;
	IF(v_status = 'WAITING') THEN
		wait_loop:LOOP
			DO SLEEP(.025);
			SELECT state, errmsg, errno INTO v_status, v_errmsg, v_errno from q where q_id = v_q_id ;
			IF (v_status !='WAITING') THEN
				LEAVE wait_loop;
			END IF;
		END LOOP;
	END IF;

	IF(v_errno IS NOT NULL) THEN
    SIGNAL SQLSTATE '99990'
       SET MESSAGE_TEXT = 'CALL asynch.check(QUERY_NUMBER) to get the detailed error information';
  END IF;

  -- wait for record lock to be released for running query
	set innodb_lock_wait_timeout=86400*7;
	SELECT q_id INTO v_q_id from q where q_id = v_q_id FOR UPDATE;
	ROLLBACK;

  SET @v_sql := CONCAT('SELECT * from rs_', v_q_id);
  PREPARE stmt from @v_sql;
  EXECUTE stmt;
	DEALLOCATE PREPARE stmt;

	SET @v_sql := CONCAT('DROP TABLE rs_', v_q_id);
  PREPARE stmt from @v_sql;
  EXECUTE stmt;
	DEALLOCATE PREPARE stmt;

END;;
CREATE DEFINER=root@localhost PROCEDURE warpsql.queue(IN v_sql_text LONGTEXT)
MODIFIES SQL DATA
SQL SECURITY DEFINER
BEGIN
  INSERT INTO q(sql_text) values(v_sql_text);
	SET @query_number := LAST_INSERT_ID();
  IF(@query_list != '' AND @query_list IS NOT NULL) THEN
    SET @query_list := CONCAT(@query_list,',');
  ELSE
    SET @query_list :=  '';
  END IF;
  SET @query_list := CONCAT(@query_list, @query_number);
	SELECT @query_number as QUERY_NUMBER;
END;;

CREATE DEFINER=root@localhost PROCEDURE warpsql.wait_list(INOUT v_list TEXT)
MODIFIES SQL DATA
wait_list:BEGIN
  IF(v_list = '' OR v_list IS NULL) THEN
    LEAVE wait_list;
  END IF;

  SET @i := 1;
  SET @qnum := '';
  csv_loop:LOOP  -- why is there no FOR loop in MySQL stored procs?
    IF(@i > LENGTH(v_list)) THEN
      LEAVE csv_loop;
    END IF;
    IF(SUBSTR(v_list,@i,1) = ',') THEN
      CALL wait(@qnum);
      SET @qnum := '';
    ELSE
      SET @qnum := CONCAT(@qnum, SUBSTR(v_list, @i, 1));
    END IF;
    SET @i := @i + 1;
  END LOOP;
  IF(@qnum != '') THEN
    CALL wait(@qnum);
  END IF;
  SET v_list := '';
END;;

CREATE DEFINER=root@localhost PROCEDURE warpsql.wait_all()
MODIFIES SQL DATA
CALL wait_list(@query_list);;



SELECT 'Creating parallel query worker using events' as message;
CREATE EVENT IF NOT EXISTS start_async_worker
ON SCHEDULE EVERY 1 SECOND
DO CALL worker;


SELECT 'Creating parallel query tables' as message;;
call warpsql.setup();;

SELECT 'Creating triggers' as message; 
-- The triggers can't be created in stored routines, so they 
-- have to be created here.  They aren't vital to behavior so
-- it is not so bad if they go missing
CREATE TRIGGER trg_before_delete before DELETE ON warpsql.settings
FOR EACH ROW 
BEGIN  
    SIGNAL SQLSTATE '99999'   
    SET MESSAGE_TEXT = 'You may not delete rows in this table';
END;;

CREATE TRIGGER trg_before_insert before INSERT ON warpsql.settings
FOR EACH ROW 
BEGIN  
    SIGNAL SQLSTATE '99999'   
    SET MESSAGE_TEXT = 'You may not insert rows in this table';
END;;

CREATE TRIGGER trg_before_update before UPDATE ON warpsql.settings
FOR EACH ROW 
BEGIN  
    IF new.variable != 'thread_count' THEN 
        SIGNAL SQLSTATE '99999'   
        SET MESSAGE_TEXT = 'Only the thread_count variable is supported at this time';
    END IF;
    IF new.variable = 'thread_count' AND CAST(new.value AS SIGNED) < 1 THEN 
        SIGNAL SQLSTATE '99999'   
        SET MESSAGE_TEXT = 'Thread count must be greater than or equal to 1';
    END IF;
END;;

create procedure warpsql.parallel_query(
  IN v_ll_query TEXT,
  IN v_coord_query TEXT,
  IN v_ll_gb TEXT,
  IN v_coord_gb TEXT,
  IN v_ll_from TEXT,
  IN v_ll_where TEXT,
  IN v_coord_having TEXT,
  IN v_ll_partinfo TEXT
)
AS
BEGIN
  use warpsql;

END;;


SELECT 'Installation complete' as message;;

SELECT IF(@@event_scheduler=1,'The event scheduler is enabled.  The default of 8 running background threads of execution is currently being used.  Execute CALL warpsql.set_concurrency(X) to set the number of threads manually.',
                              'You must enable the event scheduler ( SET GLOBAL event_scheduler=1 ) to enable background parallel execution threads.') 
as message;;


DELIMITER ;

