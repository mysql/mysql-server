delimiter |;
CREATE PROCEDURE oj_schema_mod_ndb(db varchar(64))
BEGIN

  declare tabname varchar(255);
  declare indextype varchar(32);
  declare _unique varchar(16);
  declare done integer default 0;
  declare cnt integer default 0;
  declare c cursor for 
    SELECT table_name
    FROM INFORMATION_SCHEMA.TABLES where table_schema = db;
  declare continue handler for not found set done = 1;

  open c;
  
  repeat
    fetch c into tabname;
    if not done then
       set cnt = cnt+1;
       ## Modify primary key & partition for some tables
       if ((cnt%3) = 0) then
           set @ddl = CONCAT('ALTER TABLE ', db, '.', tabname, ' PARTITION BY KEY(col_int)');
           select @ddl;
           PREPARE stmt from @ddl;
           EXECUTE stmt;

        elseif ((cnt%5) = 0) then
           set @ddl = CONCAT('ALTER TABLE ', db, '.', tabname, ' PARTITION BY KEY(pk)');
           select @ddl;
           PREPARE stmt from @ddl;
           EXECUTE stmt;

        elseif ((cnt%7) = 0) then
           set @ddl = CONCAT('ALTER TABLE ', db, '.', tabname, ' PARTITION BY KEY(col_char_16)');
           select @ddl;
           PREPARE stmt from @ddl;
           EXECUTE stmt;

        elseif ((cnt%11) = 0) then
           set @ddl = CONCAT('ALTER TABLE ', db, '.', tabname, ' PARTITION BY KEY(col_varchar_256)');
           select @ddl;
           PREPARE stmt from @ddl;
           EXECUTE stmt;
        end if;
    end if;
  until done end repeat;
  close c;
END
\G
