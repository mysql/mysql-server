/*
  Map handler error message to sql states. Note that this list MUST be in
  increasing order!
  See sql_state.c for usage
*/

{ HA_ERR_KEY_NOT_FOUND, 	"02000", "" },
{ HA_ERR_FOUND_DUPP_KEY,	"23000", "" },
{ HA_ERR_WRONG_COMMAND, 	"0A000", "" },
{ HA_ERR_UNSUPPORTED,		"0A000", "" },
{ HA_WRONG_CREATE_OPTION,	"0A000", "" },
{ HA_ERR_FOUND_DUPP_UNIQUE,	"23000", "" },
{ HA_ERR_UNKNOWN_CHARSET,	"0A000", "" },
{ HA_ERR_READ_ONLY_TRANSACTION,	"25000", "" },
{ HA_ERR_LOCK_DEADLOCK,		"40001", "" },
{ HA_ERR_NO_REFERENCED_ROW,	"23000", "" },
{ HA_ERR_ROW_IS_REFERENCED,	"23000", "" },
{ HA_ERR_TABLE_EXIST,		"42S01", "" },
{ HA_ERR_FOREIGN_DUPLICATE_KEY,	"23000", "" },
{ HA_ERR_TABLE_READONLY,        "25000", "" }, 
{ HA_ERR_AUTOINC_ERANGE,	"22003", "" },
