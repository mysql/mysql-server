/* component.cdf - dynamically updated configuration */

/*
 * NOTE: you may edit this file to alter the configuration
 * But all non-configuration information, including comments,
 * will be lost upon rebuilding this project.
 */

/* Component information */

Component INCLUDE_DB_RECOVER {
	ENTRY_POINTS	ALL_GLOBAL_SYMBOLS 
	MODULES		db_recover.o 
	NAME		db_recover
	PREF_DOMAIN	ANY
	_INIT_ORDER	usrComponentsInit
}

/* EntryPoint information */

/* Module information */

Module db_recover.o {

	NAME		db_recover.o
	SRC_PATH_NAME	$PRJ_DIR/../db_recover.c
}

/* Parameter information */

