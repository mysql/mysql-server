/* component.cdf - dynamically updated configuration */

/*
 * NOTE: you may edit this file to alter the configuration
 * But all non-configuration information, including comments,
 * will be lost upon rebuilding this project.
 */

/* Component information */

Component INCLUDE___DB_CAPAPPL_NAME__ {
	ENTRY_POINTS	ALL_GLOBAL_SYMBOLS 
	MODULES		__DB_APPLICATION_NAME__.o 
	NAME		__DB_APPLICATION_NAME__
	PREF_DOMAIN	ANY
	_INIT_ORDER	usrComponentsInit
}

/* EntryPoint information */

/* Module information */

Module __DB_APPLICATION_NAME__.o {

	NAME		__DB_APPLICATION_NAME__.o
	SRC_PATH_NAME	$PRJ_DIR/../__DB_APPLICATION_NAME__.c
}

/* Parameter information */

