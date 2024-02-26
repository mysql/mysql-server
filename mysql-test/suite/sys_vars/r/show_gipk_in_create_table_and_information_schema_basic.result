SET @saved_global_show_gipk_in_create_table_and_information_schema =
@@global.show_gipk_in_create_table_and_information_schema;
SET @saved_session_show_gipk_in_create_table_and_information_schema =
@@session.show_gipk_in_create_table_and_information_schema;
include/assert.inc [Default GLOBAL value is ON]
include/assert.inc [Default SESSION value is ON]
SET @@SESSION.show_gipk_in_create_table_and_information_schema = 0;
include/assert.inc [Reported value matches the one set (set 0 in SESSION scope)]
SET @@SESSION.show_gipk_in_create_table_and_information_schema = OFF;
include/assert.inc [Reported value matches the one set (set OFF in SESSION scope)]
SET @@SESSION.show_gipk_in_create_table_and_information_schema = FALSE;
include/assert.inc [Reported value matches the one set (set FALSE in SESSION scope)]
SET @@SESSION.show_gipk_in_create_table_and_information_schema = DEFAULT;
include/assert.inc [Reported value matches the one set (set DEFAULT in SESSION scope)]
SET @@SESSION.show_gipk_in_create_table_and_information_schema = ON;
include/assert.inc [Reported value matches the one set (set ON in SESSION scope)]
SET @@SESSION.show_gipk_in_create_table_and_information_schema = TRUE;
include/assert.inc [Reported value matches the one set (set TRUE in SESSION scope)]
SET @@SESSION.show_gipk_in_create_table_and_information_schema = 1;
include/assert.inc [Reported value matches the one set (set 1 in SESSION scope)]
SET @@GLOBAL.show_gipk_in_create_table_and_information_schema = 0;
include/assert.inc [Reported value matches the one set (set 0 in GLOBAL scope)]
SET @@GLOBAL.show_gipk_in_create_table_and_information_schema = OFF;
include/assert.inc [Reported value matches the one set (set OFF in GLOBAL scope)]
SET @@GLOBAL.show_gipk_in_create_table_and_information_schema = FALSE;
include/assert.inc [Reported value matches the one set (set FALSE in GLOBAL scope)]
SET @@GLOBAL.show_gipk_in_create_table_and_information_schema = DEFAULT;
include/assert.inc [Reported value matches the one set (set DEFAULT in GLOBAL scope)]
SET @@GLOBAL.show_gipk_in_create_table_and_information_schema = ON;
include/assert.inc [Reported value matches the one set (set ON in GLOBAL scope)]
SET @@GLOBAL.show_gipk_in_create_table_and_information_schema = TRUE;
include/assert.inc [Reported value matches the one set (set TRUE in GLOBAL scope)]
SET @@GLOBAL.show_gipk_in_create_table_and_information_schema = 1;
include/assert.inc [Reported value matches the one set (set 1 in GLOBAL scope)]
SET @@PERSIST.show_gipk_in_create_table_and_information_schema = 0;
include/assert.inc [Reported value matches the one set (set 0 in PERSIST scope)]
SET @@PERSIST.show_gipk_in_create_table_and_information_schema = OFF;
include/assert.inc [Reported value matches the one set (set OFF in PERSIST scope)]
SET @@PERSIST.show_gipk_in_create_table_and_information_schema = FALSE;
include/assert.inc [Reported value matches the one set (set FALSE in PERSIST scope)]
SET @@PERSIST.show_gipk_in_create_table_and_information_schema = DEFAULT;
include/assert.inc [Reported value matches the one set (set DEFAULT in PERSIST scope)]
SET @@PERSIST.show_gipk_in_create_table_and_information_schema = ON;
include/assert.inc [Reported value matches the one set (set ON in PERSIST scope)]
SET @@PERSIST.show_gipk_in_create_table_and_information_schema = TRUE;
include/assert.inc [Reported value matches the one set (set TRUE in PERSIST scope)]
SET @@PERSIST.show_gipk_in_create_table_and_information_schema = 1;
include/assert.inc [Reported value matches the one set (set 1 in PERSIST scope)]
SET @@PERSIST_ONLY.show_gipk_in_create_table_and_information_schema = 0;
include/assert.inc [Reported value matches the one set (set 0 in PERSIST_ONLY scope)]
SET @@PERSIST_ONLY.show_gipk_in_create_table_and_information_schema = OFF;
include/assert.inc [Reported value matches the one set (set OFF in PERSIST_ONLY scope)]
SET @@PERSIST_ONLY.show_gipk_in_create_table_and_information_schema = FALSE;
include/assert.inc [Reported value matches the one set (set FALSE in PERSIST_ONLY scope)]
SET @@PERSIST_ONLY.show_gipk_in_create_table_and_information_schema = DEFAULT;
include/assert.inc [Reported value matches the one set (set DEFAULT in PERSIST_ONLY scope)]
SET @@PERSIST_ONLY.show_gipk_in_create_table_and_information_schema = ON;
include/assert.inc [Reported value matches the one set (set ON in PERSIST_ONLY scope)]
SET @@PERSIST_ONLY.show_gipk_in_create_table_and_information_schema = TRUE;
include/assert.inc [Reported value matches the one set (set TRUE in PERSIST_ONLY scope)]
SET @@PERSIST_ONLY.show_gipk_in_create_table_and_information_schema = 1;
include/assert.inc [Reported value matches the one set (set 1 in PERSIST_ONLY scope)]
CREATE USER user1;
SET GLOBAL show_gipk_in_create_table_and_information_schema = ON;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
GRANT SYSTEM_VARIABLES_ADMIN ON *.* TO user1@'%';
SET GLOBAL show_gipk_in_create_table_and_information_schema=ON;
SET SESSION show_gipk_in_create_table_and_information_schema=ON;
REVOKE SYSTEM_VARIABLES_ADMIN ON *.* FROM user1@'%';
SET SESSION show_gipk_in_create_table_and_information_schema = ON;
DROP USER user1;
RESET PERSIST;
SET GLOBAL show_gipk_in_create_table_and_information_schema =
@saved_global_show_gipk_in_create_table_and_information_schema;
SET SESSION show_gipk_in_create_table_and_information_schema =
@saved_session_show_gipk_in_create_table_and_information_schema;
