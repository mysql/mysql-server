delimiter |;
CREATE PROCEDURE oj_schema_mod(db varchar(64))
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
       ## Drop the original index 'col_int_unique' - recreate a richer set of indexes below
       set @ddl = CONCAT('DROP INDEX col_int_unique ON ', db, '.', tabname);
       select @ddl;
       PREPARE stmt from @ddl;
       EXECUTE stmt;

       set cnt = cnt+1;

       set _unique = '';
       set indextype = '';
       ## Create a mix if 'CREATE INDEX' | 'CREATE UNIQUE INDEX [USING HASH] '

       if ((cnt%3) <> 0) then
         set _unique = ' UNIQUE';

         if ((cnt%2) = 0) then
           set indextype = ' USING HASH';
         else
           set indextype = '';
         end if;
       end if;

       ## Add some composite, possibly unique, indexes
       if tabname > 'T' then
          set @ddl = CONCAT('DROP INDEX col_varchar_10_unique ON ', db, '.', tabname);
          select @ddl;
          PREPARE stmt from @ddl;
          EXECUTE stmt;

          set @ddl = CONCAT('CREATE', _unique, ' INDEX ix1 ', indextype,
                            ' ON ', db, '.', tabname, 
                            '(col_char_16,col_char_16_unique)');
          select @ddl;
          PREPARE stmt from @ddl;
          EXECUTE stmt;
          set @ddl = CONCAT('CREATE', _unique, ' INDEX ix2 ', indextype,
                            ' ON ', db, '.', tabname, 
                            '(col_varchar_256,col_varchar_10_unique)');
          select @ddl;
          PREPARE stmt from @ddl;
          EXECUTE stmt;

       elseif tabname > 'O' then
          set @ddl = CONCAT('CREATE', _unique, ' INDEX ix1 ', indextype,
                            ' ON ', db, '.', tabname, 
                            '(col_int,col_int_unique)');
          select @ddl;
          PREPARE stmt from @ddl;
          EXECUTE stmt;
          set @ddl = CONCAT('CREATE', _unique, ' INDEX ix2 ', indextype,
                            ' ON ', db, '.', tabname, 
                            '(col_int_key,col_int_unique)');
          select @ddl;
          PREPARE stmt from @ddl;
          EXECUTE stmt;

       elseif tabname > 'H' then
          set @ddl = CONCAT('CREATE', _unique, ' INDEX ix3 ', indextype,
                            ' ON ', db, '.', tabname, 
                            '(col_int,col_int_key,col_int_unique)');
          select @ddl;
          PREPARE stmt from @ddl;
          EXECUTE stmt;

       else
          set @ddl = CONCAT('CREATE', _unique, ' INDEX ix1', indextype,
                            ' ON ', db, '.', tabname, 
                            '(col_int_unique)');
          select @ddl;
          PREPARE stmt from @ddl;
          EXECUTE stmt;
       end if;

       ## Modify primary key & partition for some tables
       if ((cnt%3) = 0) then
           set @ddl = CONCAT('UPDATE ', db, '.', tabname, ' SET col_int=0 WHERE col_int IS NULL');
	   select @ddl;
           PREPARE stmt from @ddl;
           EXECUTE stmt;

           set @ddl = CONCAT('ALTER TABLE ', db, '.', tabname,
                               '  CHANGE COLUMN pk pk INT(11) NOT NULL',
                               ', DROP PRIMARY KEY',
                               ', ADD PRIMARY KEY ', '(col_int,pk)');
           select @ddl;
           PREPARE stmt from @ddl;
           EXECUTE stmt;

        elseif ((cnt%5) = 0) then
       	   # NOTE DML needs to be run on both new/org db
           set @ddl = CONCAT('UPDATE ', db, '.', tabname, ' SET col_int=0 WHERE col_int IS NULL');
           select @ddl;
           PREPARE stmt from @ddl;
           EXECUTE stmt;

           set @ddl = CONCAT('ALTER TABLE ', db, '.', tabname,
                               '  CHANGE COLUMN pk pk INT(11) NOT NULL',
                               ', DROP PRIMARY KEY',
                               ', ADD PRIMARY KEY', indextype, '(col_int,pk)');
           select @ddl;
           PREPARE stmt from @ddl;
           EXECUTE stmt;

        elseif ((cnt%7) = 0) then
       	   # NOTE DML needs to be run on both new/org db
           set @ddl = CONCAT('DELETE FROM ', db, '.', tabname, ' WHERE col_char_16 IS NULL');
           select @ddl;
           PREPARE stmt from @ddl;
           EXECUTE stmt;

           set @ddl = CONCAT('ALTER TABLE ', db, '.', tabname,
                               '  CHANGE COLUMN pk pk INT(11) NOT NULL',
                               ', DROP PRIMARY KEY',
                               ', ADD PRIMARY KEY', indextype, '(col_char_16,pk)');
           select @ddl;
           PREPARE stmt from @ddl;
           EXECUTE stmt;

        elseif ((cnt%11) = 0) then
       	   # NOTE DML needs to be run on both new/org db
           set @ddl = CONCAT('DELETE FROM ', db, '.', tabname, ' WHERE col_varchar_10 IS NULL');
           select @ddl;
           PREPARE stmt from @ddl;
           EXECUTE stmt;

           set @ddl = CONCAT('ALTER TABLE ', db, '.', tabname,
                               '  CHANGE COLUMN pk pk INT(11) NOT NULL',
                               ', DROP PRIMARY KEY',
                               ', ADD PRIMARY KEY', indextype, '(pk,col_varchar_256)');
           select @ddl;
           PREPARE stmt from @ddl;
           EXECUTE stmt;
        end if;
    end if;
  until done end repeat;
  close c;
END
\G
