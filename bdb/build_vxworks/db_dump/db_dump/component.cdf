/* component.cdf - dynamically updated configuration */

/*
 * NOTE: you may edit this file to alter the configuration
 * But all non-configuration information, including comments,
 * will be lost upon rebuilding this project.
 */

/* Component information */

Component INCLUDE_DB_DUMP {
	ENTRY_POINTS	ALL_GLOBAL_SYMBOLS 
	MODULES		db_dump.o 
	NAME		db_dump
	PREF_DOMAIN	ANY
	_INIT_ORDER	usrComponentsInit
}

/* EntryPoint information */

/* Module information */

Module db_dump.o {

	NAME		db_dump.o
	SRC_PATH_NAME	$PRJ_DIR/../db_dump.c
}

/* Parameter information */

