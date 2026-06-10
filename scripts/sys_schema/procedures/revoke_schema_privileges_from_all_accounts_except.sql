-- Copyright (c) 2025, 2026, Oracle and/or its affiliates.
--
-- This program is free software; you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation; version 2 of the License.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program; if not, write to the Free Software
-- Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

DROP PROCEDURE IF EXISTS revoke_schema_privileges_from_all_accounts_except;

DELIMITER $$

CREATE DEFINER='mysql.sys'@'localhost' PROCEDURE revoke_schema_privileges_from_all_accounts_except(
                                                     IN in_schema_name CHAR(255),
                                                     IN in_privileges JSON,
                                                     IN in_exclude_users JSON)
  COMMENT '
           Description
           -----------

           Revoke specified privileges for all users except those specified with
           the exclude_users argument.

           Parameters
           ----------

           in_schema_name (CHAR(255)):
             Schema name on which the privileges are revoked.

           in_privileges (JSON):
             Privileges to revoke.
             Privileges are case-insensitive.

           in_exclude_users (JSON):
             Do not exclude privileges from these users.
             Host part of the user is case-insensitive.

           Example
           -------

           mysql> CALL sys.revoke_schema_privileges_from_all_accounts_except(
                  "my_schema",
                  JSON_ARRAY("SELECT", "INSERT"),
                  JSON_ARRAY("\'root\'@\'localhost\'"));
          '
  SQL SECURITY INVOKER
BEGIN
  DECLARE done INT DEFAULT 0;
  DECLARE schema_count INT DEFAULT 0;
  DECLARE grantee_out VARCHAR(288);
  DECLARE privilege_out CHAR(64);
  DECLARE schema_cursor CURSOR FOR
    SELECT COUNT(*) AS `schema_count` FROM INFORMATION_SCHEMA.SCHEMATA WHERE `SCHEMA_NAME` = in_schema_name;
  # schema grants cursor
  DECLARE schema_grants CURSOR FOR
    SELECT grantee, privilege_type
      FROM INFORMATION_SCHEMA.SCHEMA_PRIVILEGES
        WHERE table_schema = in_schema_name AND
              privilege_type MEMBER OF (UPPER(in_privileges)) AND
              CONCAT(SUBSTR(grantee, 1, LOCATE('@', grantee)), UPPER(SUBSTR(grantee, LOCATE('@', grantee) + 1))) NOT IN
                (SELECT CONCAT(SUBSTR(grantee, 1, LOCATE('@', grantee)), UPPER(SUBSTR(grantee, LOCATE('@', grantee) + 1))) AS 'grantee'
                   FROM JSON_TABLE(in_exclude_users, '$[*]' COLUMNS (grantee TEXT PATH '$')) AS users);
  # global grants cursor
  DECLARE global_grants CURSOR FOR
    SELECT grantee, privilege_type
      FROM INFORMATION_SCHEMA.USER_PRIVILEGES
        WHERE privilege_type MEMBER OF (UPPER(in_privileges)) AND
          CONCAT(SUBSTR(grantee, 1, LOCATE('@', grantee)), UPPER(SUBSTR(grantee, LOCATE('@', grantee) + 1))) NOT IN
                (SELECT CONCAT(SUBSTR(grantee, 1, LOCATE('@', grantee)), UPPER(SUBSTR(grantee, LOCATE('@', grantee) + 1))) AS 'grantee'
                   FROM JSON_TABLE(in_exclude_users, '$[*]' COLUMNS (grantee TEXT PATH '$')) AS users);
  DECLARE CONTINUE HANDLER FOR NOT FOUND SET done = 1;

  OPEN schema_cursor;

  read_loop: LOOP
    FETCH schema_cursor INTO schema_count;
    LEAVE read_loop;
  END LOOP;
  CLOSE schema_cursor;

  IF schema_count = 0 THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'The schema does not exist';
  END IF;

  SET done = 0;

  OPEN schema_grants;

  read_loop: LOOP
    FETCH schema_grants INTO grantee_out, privilege_out;
    IF done > 0 THEN
      LEAVE read_loop;
    END IF;
    SET @sql = CONCAT('REVOKE ', privilege_out, ' ON ',
                      in_schema_name, '.* FROM ', grantee_out);
    prepare statement from @sql;
    execute statement;
    deallocate prepare statement;
  END LOOP;
  CLOSE schema_grants;

  SET done = 0;

  OPEN global_grants;

  read_loop: LOOP
    FETCH global_grants INTO grantee_out, privilege_out;
    IF done > 0 THEN
      LEAVE read_loop;
    END IF;
    SET @sql = CONCAT('REVOKE ', privilege_out, ' ON ',
                      in_schema_name, '.* FROM ', grantee_out);
    prepare statement from @sql;
    execute statement;
    deallocate prepare statement;
  END LOOP;
  CLOSE global_grants;
END$$
DELIMITER ;
