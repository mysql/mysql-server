#ifndef HA_INNODB_PROTOTYPES_H
#define HA_INNODB_PROTOTYPES_H

/* Prototypes for global functions in ha_innodb.cc that are called by
InnoDB's C-code. */

/*************************************************************************
Wrapper around MySQL's copy_and_convert function, see it for
documentation. */

ulint
innobase_convert_string(
/*====================*/
	void*		to,
	ulint		to_length,
	CHARSET_INFO*	to_cs,
	const void*	from,
	ulint		from_length,
	CHARSET_INFO*	from_cs,
	uint*		errors);

#endif
