drop table if exists t1;
drop function if exists foofct;
drop procedure if exists `empty`;
drop procedure if exists foosp;
drop procedure if exists nicesp;
drop trigger if exists t1_empty;
drop trigger if exists t1_bi;
"Pass 1 : --disable-comments"
1
1
2
2
foofct("call 1")
call 1
Function	sql_mode	Create Function	character_set_client	collation_connection	Database Collation
foofct	ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION	CREATE DEFINER=`root`@`localhost` FUNCTION `foofct`(x char(20)) RETURNS char(20) CHARSET utf8mb4\nreturn\n\n\n\nx	utf8mb4	utf8mb4_0900_ai_ci	utf8mb4_0900_ai_ci
foofct("call 2")
call 2
Function	sql_mode	Create Function	character_set_client	collation_connection	Database Collation
foofct	ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION	CREATE DEFINER=`root`@`localhost` FUNCTION `foofct`(x char(20)) RETURNS char(20) CHARSET utf8mb4\nbegin\n  \n  \n  \n\n  \n\n  \n  return x;\nend	utf8mb4	utf8mb4_0900_ai_ci	utf8mb4_0900_ai_ci
Procedure	sql_mode	Create Procedure	character_set_client	collation_connection	Database Collation
empty	ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION	CREATE DEFINER=`root`@`localhost` PROCEDURE `empty`()\nbegin\nend	utf8mb4	utf8mb4_0900_ai_ci	utf8mb4_0900_ai_ci
id	data
foo	42
Procedure	sql_mode	Create Procedure	character_set_client	collation_connection	Database Collation
foosp	ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION	CREATE DEFINER=`root`@`localhost` PROCEDURE `foosp`()\ninsert into test.t1\n\n\n\n\n  \n\n  \n  values ("foo", 42)	utf8mb4	utf8mb4_0900_ai_ci	utf8mb4_0900_ai_ci
Procedure	sql_mode	Create Procedure	character_set_client	collation_connection	Database Collation
nicesp	ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION	CREATE DEFINER=`root`@`localhost` PROCEDURE `nicesp`(a int)\nbegin\n  \n  declare b int;\n  declare c float;\n\n  \n  \n\n  \nend	utf8mb4	utf8mb4_0900_ai_ci	utf8mb4_0900_ai_ci
Trigger	sql_mode	SQL Original Statement	character_set_client	collation_connection	Database Collation	Created
t1_empty	ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION	CREATE DEFINER=`root`@`localhost` TRIGGER `t1_empty` AFTER DELETE ON `t1` FOR EACH ROW begin\nend	utf8mb4	utf8mb4_0900_ai_ci	utf8mb4_0900_ai_ci	--TIMESTAMP--
Trigger	sql_mode	SQL Original Statement	character_set_client	collation_connection	Database Collation	Created
t1_bi	ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION	CREATE DEFINER=`root`@`localhost` TRIGGER `t1_bi` BEFORE INSERT ON `t1` FOR EACH ROW begin\n\n\n\n  \n  declare b int;\n  declare c float;\n\n  \n  \n\n  \n  set NEW.data := 12;\nend	utf8mb4	utf8mb4_0900_ai_ci	utf8mb4_0900_ai_ci	--TIMESTAMP--
id	data
trig	12
"Pass 2 : --enable-comments"
1
1
2
2
foofct("call 1")
call 1
Function	sql_mode	Create Function	character_set_client	collation_connection	Database Collation
foofct	ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION	CREATE DEFINER=`root`@`localhost` FUNCTION `foofct`(x char(20)) RETURNS char(20) CHARSET utf8mb4\nreturn\n-- comment 1a\n# comment 1b\n/* comment 1c */\nx # after body, on same line	utf8mb4	utf8mb4_0900_ai_ci	utf8mb4_0900_ai_ci
foofct("call 2")
call 2
Function	sql_mode	Create Function	character_set_client	collation_connection	Database Collation
foofct	ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION	CREATE DEFINER=`root`@`localhost` FUNCTION `foofct`(x char(20)) RETURNS char(20) CHARSET utf8mb4\nbegin\n  -- comment 1a\n  # comment 1b\n  /*\n     comment 1c\n  */\n\n  -- empty line below\n\n  -- empty line above\n  return x;\nend	utf8mb4	utf8mb4_0900_ai_ci	utf8mb4_0900_ai_ci
Procedure	sql_mode	Create Procedure	character_set_client	collation_connection	Database Collation
empty	ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION	CREATE DEFINER=`root`@`localhost` PROCEDURE `empty`()\nbegin\nend	utf8mb4	utf8mb4_0900_ai_ci	utf8mb4_0900_ai_ci
id	data
foo	42
Procedure	sql_mode	Create Procedure	character_set_client	collation_connection	Database Collation
foosp	ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION	CREATE DEFINER=`root`@`localhost` PROCEDURE `foosp`()\ninsert into test.t1\n## These comments are part of the procedure body, and should be kept.\n# Comment 2a\n-- Comment 2b\n/* Comment 2c */\n  -- empty line below\n\n  -- empty line above\n  values ("foo", 42) # comment 3, still part of the body	utf8mb4	utf8mb4_0900_ai_ci	utf8mb4_0900_ai_ci
Procedure	sql_mode	Create Procedure	character_set_client	collation_connection	Database Collation
nicesp	ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION	CREATE DEFINER=`root`@`localhost` PROCEDURE `nicesp`(a int)\nbegin\n  -- declare some variables here\n  declare b int;\n  declare c float;\n\n  -- do more stuff here\n  -- commented nicely and so on\n\n  -- famous last words ...\nend	utf8mb4	utf8mb4_0900_ai_ci	utf8mb4_0900_ai_ci
Trigger	sql_mode	SQL Original Statement	character_set_client	collation_connection	Database Collation	Created
t1_empty	ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION	CREATE DEFINER=`root`@`localhost` TRIGGER `t1_empty` AFTER DELETE ON `t1` FOR EACH ROW begin\nend	utf8mb4	utf8mb4_0900_ai_ci	utf8mb4_0900_ai_ci	--TIMESTAMP--
Trigger	sql_mode	SQL Original Statement	character_set_client	collation_connection	Database Collation	Created
t1_bi	ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION	CREATE DEFINER=`root`@`localhost` TRIGGER `t1_bi` BEFORE INSERT ON `t1` FOR EACH ROW begin\n# comment 1a\n-- comment 1b\n/*\n   comment 1c\n*/\n  -- declare some variables here\n  declare b int;\n  declare c float;\n\n  -- do more stuff here\n  -- commented nicely and so on\n\n  -- famous last words ...\n  set NEW.data := 12;\nend	utf8mb4	utf8mb4_0900_ai_ci	utf8mb4_0900_ai_ci	--TIMESTAMP--
id	data
trig	12
End of 5.0 tests
