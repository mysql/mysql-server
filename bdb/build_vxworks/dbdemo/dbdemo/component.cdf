/* component.cdf - dynamically updated configuration */

/*
 * NOTE: you may edit this file to alter the configuration
 * But all non-configuration information, including comments,
 * will be lost upon rebuilding this project.
 */

/* Component information */

Component INCLUDE_DBDEMO {
	ENTRY_POINTS	ALL_GLOBAL_SYMBOLS 
	MODULES		dbdemo.o 
	NAME		dbdemo
	PREF_DOMAIN	ANY
	_INIT_ORDER	usrComponentsInit
}

/* EntryPoint information */

/* Module information */

Module dbdemo.o {

	NAME		dbdemo.o
	SRC_PATH_NAME	$PRJ_DIR/../dbdemo.c
}

/* Parameter information */

