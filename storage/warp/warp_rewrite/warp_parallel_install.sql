#a vim: set ts=2: set expandtab:
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

DELIMITER ;;
CREATE DEFINER=root@localhost PROCEDURE warpsql.check_reqs()
READS SQL DATA
SQL SECURITY DEFINER
BEGIN
  IF(VERSION() NOT LIKE '8.%') THEN
		SIGNAL SQLSTATE '99999'
		SET MESSAGE_TEXT = 'MySQL version 8 is needed for WarpSQL functionality';
  END IF;
END;;

CALL check_reqs();;

select 'Creating setup procedure' as message;;

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
    created_on datetime(6) default now(6),
    started_on datetime(6) default now(6),
    completed_on datetime(6) default now(6),
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
  
  INSERT INTO warpsql.settings VALUES ('thread_count', '12');
      
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
  DECLARE v_partname LONGTEXT DEFAULT NULL;

  -- FIXME: 
  -- These control the backoff in the loop waiting
  -- for queries to be placed in the q table.  You
  -- can change the values here but they should be
  -- sufficient for most setups. I'll convert them
  -- to configuration variables later.
  DECLARE v_wait FLOAT DEFAULT 0;
  DECLARE v_min_wait FLOAT DEFAULT 0;
  DECLARE v_inc_wait FLOAT DEFAULT 0.01;
  DECLARE v_max_wait FLOAT DEFAULT 0.1;

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
			DO sleep(.01);  
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
             sql_text, 
             partition_filter
        INTO v_q_id,
             v_sql_text,
             v_partname
        FROM q
       WHERE completed = FALSE
         AND state = 'WAITING'
       ORDER BY q_id
      LIMIT 1 
 			FOR UPDATE SKIP LOCKED;

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
           SET started_on = default, 
                 state='RUNNING'
         WHERE q_id = v_q_id;

        -- unlock the record (don't block the q)
        COMMIT;

        START TRANSACTION;

        IF v_partname IS NOT NULL THEN
          -- insert into test.debug values (concat('setting partname to ', v_partname));
          SET warp_partition_filter = v_partname;
        ELSE 
          
          -- insert into test.debug values ('setting partname to empty string');
          SET warp_partition_filter = '';
        END IF;

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
					SELECT * from q where q_id = v_q_id FOR UPDATE SKIP LOCKED;
        	EXECUTE stmt;
					DEALLOCATE PREPARE stmt;
				END IF;

        UPDATE q
           SET state = IF(@errno IS NULL, 'COMPLETED', 'ERROR'),
               errno = @errno,
               errmsg = @errmsg,
               completed = TRUE,
               completed_on = default
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
    SET @v_message := CONCAT('Invalid QUERY_NUMBER: ', v_q_id);
    SIGNAL SQLSTATE '99998'
       SET MESSAGE_TEXT=@v_message;
  END IF;
	IF(v_status = 'WAITING') THEN
		wait_loop:LOOP
			DO SLEEP(.001);
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
  SET v_status := NULL;
  -- wait for record lock to be released for running query
	-- waitLoop: LOOP
	--  SELECT state INTO v_status from q where q_id = v_q_id AND STATE NOT IN ('ERROR','COMPLETED');
  --  IF v_status IS NOT NULL THEN
  --    LEAVE waitLoop;
  --  END IF;
  --  DO SLEEP(.001);
	-- END LOOP waitLoop;

  if @warp_async_query IS NULL THEN
    SET @v_sql := CONCAT('SELECT * from rs_', v_q_id);
    PREPARE stmt from @v_sql;
    EXECUTE stmt;
    DEALLOCATE PREPARE stmt;

    SET @v_sql := CONCAT('DROP TABLE rs_', v_q_id);
    PREPARE stmt from @v_sql;
    EXECUTE stmt;
    DEALLOCATE PREPARE stmt;
  END IF;
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

CREATE DEFINER=root@localhost PROCEDURE warpsql.queue_ll(
  IN v_sql_text LONGTEXT,
  IN v_partname LONGTEXT
)
MODIFIES SQL DATA
SQL SECURITY DEFINER
BEGIN
  INSERT INTO q(sql_text,partition_filter) values(v_sql_text, v_partname);
	SET @query_number := LAST_INSERT_ID();
  IF(@query_list != '' AND @query_list IS NOT NULL) THEN
    SET @query_list := CONCAT(@query_list,',');
  ELSE
    SET @query_list :=  '';
  END IF;
  SET @query_list := CONCAT(@query_list, @query_number);
	-- SELECT @query_number as QUERY_NUMBER;
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
BEGIN
CALL wait_list(@query_list);
SET @query_list = '';
END;;

SELECT 'Creating parallel query worker using events' as message;;
CREATE EVENT IF NOT EXISTS start_async_worker
ON SCHEDULE EVERY 1 SECOND
DO CALL worker;;


SELECT 'Creating parallel query tables' as message;;
call warpsql.setup();;

drop procedure if exists warpsql.parallel_query;;
create definer=root@localhost 
procedure warpsql.parallel_query (
 IN v_ll_select     longtext,
 IN v_coord_select  longtext,
 IN v_ll_group      longtext,
 IN v_coord_group   longtext,
 IN v_ll_from       longtext,
 IN v_ll_where      longtext,
 IN v_coord_having  longtext,
 in v_coord_order   longtext,
 IN v_partitions    longtext,
 IN v_straight_join boolean
)
begin
  -- this is the query that will be executed for each partition of the largest
  -- table in the query ("generally the fact table")
  declare v_ll_query LONGTEXT DEFAULT '';

  -- this is the table that the results of v_ll_query go into
  declare v_coord_table TEXT DEFAULT '';

  -- this is the query over the v_coord_table that returns that aggregated
  -- results.  This is the 'coordinator query'
  declare v_coord_query LONGTEXT DEFAULT '';

  -- this is the partition that will be scanned by v_ll_query
  declare v_selected_partition TEXT DEFAULT '';
  -- END DECLARATIONS

  -- the queries executed by this stored procedure can't be parallelized
  -- or there will be an infinite loop!
  set warp_rewriter_parallel_query = OFF;
  set @v_coord_query = NULL;
  -- for DEBUG purposes
  -- select v_ll_select, v_coord_query, v_ll_group, v_coord_group, v_ll_from, v_ll_where, v_coord_having, v_partitions;
  if v_straight_join = 1 then
    set v_ll_query := CONCAT("SELECT STRAIGHT_JOIN ", v_ll_select, '\n  ', v_ll_from);
  else 
    set v_ll_query := CONCAT("SELECT ", v_ll_select, '\n  ', v_ll_from);
  end if;
  -- 1=1 is added to the WHERE clause 
  if(v_ll_where != "") then
    SET v_ll_where := CONCAT("(" , v_ll_where, ") AND 1=1 ");
    set v_ll_query := CONCAT(v_ll_query, '\n WHERE ', v_ll_where);
  else 
    set v_ll_query := CONCAT(v_ll_query, '\n WHERE 1=1 ', v_ll_where);
  end if;

  if(v_ll_group != "") then
    set v_ll_query := CONCAT(v_ll_query, '\nGROUP BY ', v_ll_group);
  end if;

  -- select v_ll_query;
  
  SET v_coord_table := CONCAT('r_', md5(concat(sysdate(6),rand())));

  set @v_sql = CONCAT(
    'CREATE TABLE warpsql.', v_coord_table , '\nAS\n',
    "SELECT * from ( ", 
    replace(v_ll_query, "1=1","1=0"), " ) sq LIMIT 0;"
  );

  -- select @v_sql; 

  PREPARE stmt FROM @v_sql;
  EXECUTE stmt;
  DEALLOCATE PREPARE stmt;

  -- turn the parallel query into an INSERT statement
  set v_ll_query := CONCAT(
    'INSERT INTO warpsql.',v_coord_table,'\n', v_ll_query
  );

  --  select v_ll_query;

  set v_coord_query := CONCAT('SELECT ',
    v_coord_select, '\nFROM warpsql.',
    v_coord_table, ' ' 
  );

  if(v_coord_group != "") then
    set v_coord_query := CONCAT(v_coord_query, 'GROUP BY ', v_coord_group);
  end if;

  if(v_coord_having != "") then
    set v_coord_query := CONCAT(v_coord_query, ' HAVING ', v_coord_having);
  end if;

  if(v_coord_order != "") then
    set v_coord_query := CONCAT(v_coord_query, ' ORDER BY ', v_coord_order);
  end if;
  
  -- select v_coord_query;

  drop temporary table if exists partitions;

  create temporary table partitions (
    p_name text
  );

  set @v_pos      := POSITION(':' IN v_partitions);
  set @v_alias    := SUBSTR(v_partitions, 1, @v_pos-1);
  set @v_partlist := SUBSTR(v_partitions, @v_pos+1);
  SET @warp_async_query = true;

  set @v_sql := CONCAT('INSERT INTO partitions VALUES ', @v_partlist);
  prepare stmt from @v_sql;
  execute stmt;
  deallocate prepare stmt;
  -- select 'submitting parallel workers';
  BEGIN
    declare v_done boolean default false;
    declare v_partname text default '';
    DECLARE parts CURSOR
    FOR
    SELECT * from partitions;
    declare continue handler for not found
      set v_done := true;
    
    OPEN parts;
    partLoop: LOOP 
      FETCH parts 
       INTO v_partname;
      IF v_done = true then
        leave partLoop;
      END IF;
      CALL warpsql.queue_ll(v_ll_query, CONCAT(@v_alias, ': ', v_partname));
      -- leave partLoop;
    END LOOP partLoop;   
  END; 
  
  set transaction_isolation = 'read-committed';
  -- select @query_list;
  set @v_sql := CONCAT("select count(*) into @wait_cnt from warpsql.q where q_id in (", @query_list, ") and state NOT IN ('COMPLETED','ERROR')");
  waitLoop: LOOP
    prepare wait_stmt from @v_sql;
    execute wait_stmt;  
    DEALLOCATE PREPARE wait_stmt;
    if(@wait_cnt = 0) THEN
      LEAVE waitLoop;
    end if;
    
    -- select @wait_cnt;
    do sleep(.1);
  END LOOP waitLoop;
  set @warp_async_query = NULL;
  
  set @query_list = '';
  set @v_coord_query := v_coord_query;
  PREPARE coord_stmt FROM @v_coord_query;
  EXECUTE coord_stmt;
  DEALLOCATE PREPARE coord_stmt;
  -- SELECT v_coord_query;
  
  -- turn the parallel query plugin back on
  set warp_rewriter_parallel_query = ON;

end;;

SELECT 'Installation complete' as message;;

SELECT IF(@@event_scheduler=1 OR @@event_scheduler='ON','The event scheduler is enabled.  The default of 8 running background threads of execution is currently being used.  Execute CALL warpsql.set_concurrency(X) to set the number of threads manually.',
                              'You must enable the event scheduler ( SET GLOBAL event_scheduler=1 ) to enable background parallel execution threads.') 
as message;;


DELIMITER ;

INSTALL PLUGIN warp_rewriter SONAME 'warp_rewriter.so';
