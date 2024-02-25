SET @saved_global_sql_generate_invisible_primary_key =  @@global.sql_generate_invisible_primary_key;
SET @saved_session_sql_generate_invisible_primary_key = @@session.sql_generate_invisible_primary_key;
include/assert.inc [Default GLOBAL value is OFF]
include/assert.inc [Default SESSION value is OFF]
SET @@SESSION.sql_generate_invisible_primary_key = 0;
include/assert.inc [Reported value matches the one set (set 0 in SESSION scope)]
SET @@SESSION.sql_generate_invisible_primary_key = DEFAULT;
include/assert.inc [Reported value matches the one set (set DEFAULT in SESSION scope)]
SET @@SESSION.sql_generate_invisible_primary_key = OFF;
include/assert.inc [Reported value matches the one set (set OFF in SESSION scope)]
SET @@SESSION.sql_generate_invisible_primary_key = FALSE;
include/assert.inc [Reported value matches the one set (set FALSE in SESSION scope)]
SET @@SESSION.sql_generate_invisible_primary_key = ON;
include/assert.inc [Reported value matches the one set (set ON in SESSION scope)]
SET @@SESSION.sql_generate_invisible_primary_key = TRUE;
include/assert.inc [Reported value matches the one set (set TRUE in SESSION scope)]
SET @@SESSION.sql_generate_invisible_primary_key = 1;
include/assert.inc [Reported value matches the one set (set 1 in SESSION scope)]
SET @@GLOBAL.sql_generate_invisible_primary_key = 0;
include/assert.inc [Reported value matches the one set (set 0 in GLOBAL scope)]
SET @@GLOBAL.sql_generate_invisible_primary_key = DEFAULT;
include/assert.inc [Reported value matches the one set (set DEFAULT in GLOBAL scope)]
SET @@GLOBAL.sql_generate_invisible_primary_key = OFF;
include/assert.inc [Reported value matches the one set (set OFF in GLOBAL scope)]
SET @@GLOBAL.sql_generate_invisible_primary_key = FALSE;
include/assert.inc [Reported value matches the one set (set FALSE in GLOBAL scope)]
SET @@GLOBAL.sql_generate_invisible_primary_key = ON;
include/assert.inc [Reported value matches the one set (set ON in GLOBAL scope)]
SET @@GLOBAL.sql_generate_invisible_primary_key = TRUE;
include/assert.inc [Reported value matches the one set (set TRUE in GLOBAL scope)]
SET @@GLOBAL.sql_generate_invisible_primary_key = 1;
include/assert.inc [Reported value matches the one set (set 1 in GLOBAL scope)]
SET @@PERSIST.sql_generate_invisible_primary_key = 0;
include/assert.inc [Reported value matches the one set (set 0 in PERSIST scope)]
SET @@PERSIST.sql_generate_invisible_primary_key = DEFAULT;
include/assert.inc [Reported value matches the one set (set DEFAULT in PERSIST scope)]
SET @@PERSIST.sql_generate_invisible_primary_key = OFF;
include/assert.inc [Reported value matches the one set (set OFF in PERSIST scope)]
SET @@PERSIST.sql_generate_invisible_primary_key = FALSE;
include/assert.inc [Reported value matches the one set (set FALSE in PERSIST scope)]
SET @@PERSIST.sql_generate_invisible_primary_key = ON;
include/assert.inc [Reported value matches the one set (set ON in PERSIST scope)]
SET @@PERSIST.sql_generate_invisible_primary_key = TRUE;
include/assert.inc [Reported value matches the one set (set TRUE in PERSIST scope)]
SET @@PERSIST.sql_generate_invisible_primary_key = 1;
include/assert.inc [Reported value matches the one set (set 1 in PERSIST scope)]
SET @@PERSIST_ONLY.sql_generate_invisible_primary_key = 0;
include/assert.inc [Reported value matches the one set (set 0 in PERSIST_ONLY scope)]
SET @@PERSIST_ONLY.sql_generate_invisible_primary_key = DEFAULT;
include/assert.inc [Reported value matches the one set (set DEFAULT in PERSIST_ONLY scope)]
SET @@PERSIST_ONLY.sql_generate_invisible_primary_key = OFF;
include/assert.inc [Reported value matches the one set (set OFF in PERSIST_ONLY scope)]
SET @@PERSIST_ONLY.sql_generate_invisible_primary_key = FALSE;
include/assert.inc [Reported value matches the one set (set FALSE in PERSIST_ONLY scope)]
SET @@PERSIST_ONLY.sql_generate_invisible_primary_key = ON;
include/assert.inc [Reported value matches the one set (set ON in PERSIST_ONLY scope)]
SET @@PERSIST_ONLY.sql_generate_invisible_primary_key = TRUE;
include/assert.inc [Reported value matches the one set (set TRUE in PERSIST_ONLY scope)]
SET @@PERSIST_ONLY.sql_generate_invisible_primary_key = 1;
include/assert.inc [Reported value matches the one set (set 1 in PERSIST_ONLY scope)]
CREATE USER user1;
SET GLOBAL sql_generate_invisible_primary_key = ON;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
SET SESSION sql_generate_invisible_primary_key = ON;
ERROR 42000: Access denied; you need (at least one of) the SUPER, SYSTEM_VARIABLES_ADMIN or SESSION_VARIABLES_ADMIN privilege(s) for this operation
GRANT SYSTEM_VARIABLES_ADMIN ON *.* TO user1@'%';
SET GLOBAL sql_generate_invisible_primary_key=ON;
SET SESSION sql_generate_invisible_primary_key=ON;
REVOKE SYSTEM_VARIABLES_ADMIN ON *.* FROM user1@'%';
GRANT SESSION_VARIABLES_ADMIN ON *.* TO user1@'%';
SET SESSION sql_generate_invisible_primary_key = ON;
DROP USER user1;
RESET PERSIST;
SET SESSION sql_generate_invisible_primary_key = @saved_session_sql_generate_invisible_primary_key;
SET GLOBAL sql_generate_invisible_primary_key = @saved_global_sql_generate_invisible_primary_key;
