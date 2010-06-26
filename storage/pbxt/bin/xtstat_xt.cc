/* Copyright (c) 2005 PrimeBase Technologies GmbH
 *
 * PrimeBase XT
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * 2008-11-19	Paul McCullagh
 *
 * H&G2JCtL
 */

#include "xt_config.h"

#include <mysql.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "strutil_xt.h"
#include "util_xt.h"

//#define DEBUG_INTERRUPT

#define OPT_NONE		-1
#define OPT_HELP		0
#define OPT_HOST		1
#define OPT_USER		2
#define OPT_PASSWORD	3
#define OPT_DATABASE	4
#define OPT_PORT		5
#define OPT_SOCKET		6
#define OPT_DELAY		7
#define OPT_PROTOCOL	8
#define OPT_DISPLAY		9

#define OPT_HAS_VALUE	1
#define OPT_OPTIONAL	2
#define OPT_INTEGER		4

llong		record_cache_size;
llong		index_cache_size;
llong		log_cache_size;

llong		accumulative_values[XT_STAT_CURRENT_MAX];
int			columns_used;
int			use_i_s = 0;

struct DisplayOrder {
	int			do_statistic;
	bool		do_combo;
} display_order[XT_STAT_CURRENT_MAX];

struct Options {
	int			opt_id;
	const char	opt_char;
	const char	*opt_name;
	int			opt_flags;
	const char	*opt_desc;
	const char	*opt_value_str;
	int			opt_value_int;
	bool		opt_value_bool;
} options[] = {
	{ OPT_HELP,		'?', "help",		0,
		"Prints help text", NULL, 0, false },
	{ OPT_HOST,		'h', "host",		OPT_HAS_VALUE,
		"Connect to host", NULL, 0, false },
	{ OPT_USER,		'u', "user",		OPT_HAS_VALUE,
		"User for login if not current user", NULL, 0, false },
	{ OPT_PASSWORD, 'p', "password",	OPT_HAS_VALUE | OPT_OPTIONAL,
		"Password to use when connecting to server. If password is not given it's asked from the tty", NULL, 0, false },
	{ OPT_DATABASE, 'd', "database",	OPT_HAS_VALUE,
		"Database to be used (pbxt or information_schema required), default is information_schema", "information_schema", 0, false },
	{ OPT_PORT,		'P', "port",		OPT_HAS_VALUE | OPT_INTEGER,
		"Port number to use for connection", NULL, 3306, false },
	{ OPT_SOCKET,	'S', "socket",		OPT_HAS_VALUE,
		"Socket file to use for connection", NULL, 0, false },
	{ OPT_DELAY,	'D', "delay",		OPT_HAS_VALUE | OPT_INTEGER,
		"Delay in seconds between polls of the database", NULL, 1, false },
	{ OPT_PROTOCOL,	0, "protocol",		OPT_HAS_VALUE,
		"Connection protocol to use: default/tcp/socket/pipe/memory", "default", MYSQL_PROTOCOL_DEFAULT, false },
	{ OPT_DISPLAY,	0, "display",		OPT_HAS_VALUE,
		"Columns to display: use short names separated by |, partial match allowed", "time-msec,commt,row-ins,rec,ind,ilog,xlog,data,to,dirty", 0, false },
	{ OPT_NONE,		0, NULL, 0, NULL, NULL, 0, false }
};

#ifdef XT_WIN
#define atoll _atoi64
#endif

void add_statistic(int stat)
{
	/* Check if column has already been added: */
	for (int i=0; i<columns_used; i++) {
		if (display_order[i].do_statistic == stat)
			return;
	}
	display_order[columns_used].do_statistic = stat;
	display_order[columns_used].do_combo = false;
	columns_used++;
}

void determine_display_order()
{
	const char			*cols = options[OPT_DISPLAY].opt_value_str;
	char				column_1[21], column_2[21];
	int					i;
	bool				add, added, add_combo;
	XTStatMetaDataPtr	meta, meta2;

	if (strcmp(cols, "all") == 0)
		cols = "time,xact,stat,rec,ind,ilog,xlog,data,to,sweep,scan,row";
	columns_used = 0;
	while (*cols) {
		i = 0;
		while (*cols && *cols != '-' && *cols != ',') {
			if (i < 20) {
				column_1[i] = *cols;
				i++;
			}
			cols++;
		}
		column_1[i] = 0;
		
		i = 0;
		if (*cols == '-') {
			cols++;
			while (*cols && *cols != '-' && *cols != ',') {
				if (i < 20) {
					column_2[i] = *cols;
					i++;
				}
				cols++;
			}
		}
		column_2[i] = 0;

		if (*cols == ',')
			cols++;

		if (strcmp(column_1, "ms") == 0)
			strcpy(column_1, "msec");
		if (strcmp(column_2, "ms") == 0)
			strcpy(column_2, "msec");
		add_combo = false;
		if (strcmp(column_1, "syncs/ms") == 0) {
			strcpy(column_1, "syncs");
			add_combo = true;
		}
		if (strcmp(column_2, "syncs/ms") == 0) {
			strcpy(column_2, "syncs");
			add_combo = true;
		}

		added = false;
		for (i=0; i<XT_STAT_MAXIMUM; i++) {
			meta = xt_get_stat_meta_data(i);
			add = false;
			if (strcmp(meta->sm_short_line_1, column_1) == 0) {
				if (column_2[0]) {
					if (strcmp(meta->sm_short_line_2, column_2) == 0)
						add = true;
				}
				else {
					if (i != XT_STAT_XLOG_CACHE_USAGE)
						add = true;
				}
			}
			else if (!column_2[0]) {
				if (strcmp(meta->sm_short_line_2, column_1) == 0) {
					/* XT_STAT_XLOG_CACHE_USAGE is ignored, unless explicity listed! */
					if (i != XT_STAT_XLOG_CACHE_USAGE)
						add = true;
				}
			}
			if (add) {
				added = true;
				add_statistic(i);
				if (add_combo)
					add_statistic(i+1);
			}
		}
		if (!added) {
			if (column_2[0])
				fprintf(stderr, "ERROR: No statistic matches display option: '%s-%s'\n", column_1, column_2);
			else
				fprintf(stderr, "ERROR: No statistic matches display option: '%s'\n", column_1);
			fprintf(stderr, "Display options: %s\n", options[OPT_DISPLAY].opt_value_str);
			exit(1);
		}
	}

	/* Setup "combo" fields: */
	for (i=0; i<columns_used; i++) {
		meta = xt_get_stat_meta_data(display_order[i].do_statistic);
		if (meta->sm_flags & XT_STAT_COMBO_FIELD) {
			if (i+1 < columns_used) {
				meta2 = xt_get_stat_meta_data(display_order[i+1].do_statistic);
				if (meta2->sm_flags & XT_STAT_COMBO_FIELD_2) {
					if (strcmp(meta->sm_short_line_1, meta2->sm_short_line_1) == 0)
						display_order[i].do_combo = true;
				}
			}
		}
	}
}

void format_percent_value(char *buffer, double value, double perc)
{
	value = value * (double) 100 / (double) perc;
	if (value >= 100)
		sprintf(buffer, "%.0f", value);
	else
		sprintf(buffer, "%.1f", value);
	buffer[4] = 0;
	if (buffer[3] == '.')
		buffer[3] = 0;
}

#define XT_1_K				((double) 1024)
#define XT_1_M				((double) 1024 * (double) 1024)
#define XT_1_G				((double) 1024 * (double) 1024 * (double) 1024)
#define XT_1_T				((double) 1024 * (double) 1024 * (double) 1024 * (double) 1024)
#define XT_10000_K			((double) 10000 * XT_1_K)
#define XT_10000_M			((double) 10000 * XT_1_M)
#define XT_10000_G			((double) 10000 * XT_1_G)

void format_byte_value(char *buffer, double value)
{
	double	dval;
	char	string[100];
	char	ch;

	if (value < (double) 100000) {
		/* byte value from 0 to 99999: */
		sprintf(buffer, "%.0f", value);
		return;
	}

	if (value < XT_10000_K) {
		dval = value / XT_1_K;
		ch = 'K';
	}
	else if (value < XT_10000_M) {
		dval = value / XT_1_M;
		ch = 'M';
	}
	else if (value < XT_10000_G) {
		dval = value / XT_1_G;
		ch = 'G';
	}
	else {
		dval = value / XT_1_T;
		ch = 'T';
	}

	if (dval < (double) 10.0)
		sprintf(string, "%.2f", dval);
	else if (dval < (double) 100.0)
		sprintf(string, "%.1f", dval);
	else
		sprintf(string, "%.0f", dval);
	if (string[3] == '.')
		string[3] = 0;
	else
		string[4] = 0;
	sprintf(buffer, "%s%c", string, ch);
}

/*
 * Uses:
 * t = thousands
 * m = millions
 * b = billions
 */
void format_mini_count_value(char *buffer, double value)
{
	double	dval;
	char	string[100];
	char	ch;

	if (value < (double) 100) {
		/* Value from 0 to 99: */
		sprintf(buffer, "%.0f", value);
		return;
	}

	if (value < (double) 1000) {
		sprintf(buffer, "<t");
		return;
	}

	if (value < (double) 10000) {
		/* Value is less than 1m */
		dval = value / (double) 1000.0;
		ch = 't';
	}
	else if (value < (double) 1000000) {
		sprintf(buffer, "<m");
		return;
	}
	else if (value < (double) 10000000) {
		/* Value is less than 1b */
		dval = value / (double) 1000000.0;
		ch = 'm';
	}
	else if (value < (double) 1000000000) {
		sprintf(buffer, "<b");
		return;
	}
	else {
		/* Value is greater than 1 billion  */
		dval = value / (double) 1000000000.0;
		ch = 'b';
	}

	sprintf(string, "%1.0f", dval);
	string[1] = 0;
	sprintf(buffer, "%s%c", string, ch);
}

#define XT_1_THOUSAND		((double) 1000)
#define XT_1_MILLION		((double) 1000 * (double) 1000)
#define XT_1_BILLION		((double) 1000 * (double) 1000 * (double) 1000)
#define XT_1_TRILLION		((double) 1000 * (double) 1000 * (double) 1000 * (double) 1000)
#define XT_10_THOUSAND		((double) 10 * (double) 1000)
#define XT_10_MILLION		((double) 10 * (double) 1000 * (double) 1000)
#define XT_10_BILLION		((double) 10 * (double) 1000 * (double) 1000 * (double) 1000)
#define XT_10_TRILLION		((double) 10 * (double) 1000 * (double) 1000 * (double) 1000 * (double) 1000)

void format_count_value(char *buffer, double value)
{
	double	dval;
	char	string[100];
	char	ch;

	if (value < (double) 0) {
		strcpy(buffer, "0");
		return;
	}

	if (value < XT_10_THOUSAND) {
		/* byte value from 0 to 99999: */
		sprintf(buffer, "%.0f", value);
		return;
	}

	if (value < XT_10_MILLION) {
		/* Value is less than 10 million */
		dval = value / XT_1_THOUSAND;
		ch = 't';
	}
	else if (value < XT_10_BILLION) {
		/* Value is less than 10 million */
		dval = value / XT_1_MILLION;
		ch = 'm';
	}
	else if (value < XT_10_TRILLION) {
		/* Value is less than 10 trillion */
		dval = value / XT_1_BILLION;
		ch = 'b';
	}
	else {
		dval = value / XT_1_TRILLION;
		ch = 't';
	}

	if (dval < (double) 10.0)
		sprintf(string, "%.2f", dval);
	else if (dval < (double) 100.0)
		sprintf(string, "%.1f", dval);
	else
		sprintf(string, "%.0f", dval);
	if (string[3] == '.')
		string[3] = 0;
	else
		string[4] = 0;
	sprintf(buffer, "%s%c", string, ch);
}

void print_help()
{
	struct Options	*opt;
	char			command[100];

	printf("Usage: xtstat [ options ]\n");
	printf("e.g. xtstat -D10 : Poll every 10 seconds\n");
	opt = options;
	printf("Options :-\n");
	while (opt->opt_id != OPT_NONE) {
		strcpy(command, opt->opt_name);
		if (opt->opt_flags & OPT_HAS_VALUE) {
			if (opt->opt_flags & OPT_OPTIONAL)
				strcat(command, "[=value]");
			else
				strcat(command, "=value");
		}
		if (opt->opt_char)
			printf("-%c, --%-16s %s.\n", opt->opt_char, command, opt->opt_desc);
		else
			printf("    --%-16s %s.\n", command, opt->opt_desc);
		opt++;
	}
}

void print_stat_key()
{
	printf("Key :-\n");
	printf("K = Kilobytes (1,024 bytes)\n");
	printf("M = Megabytes (1,048,576 bytes)\n");
	printf("G = Gigabytes (1,073,741,024 bytes)\n");
	printf("T = Terabytes (1,099,511,627,776 bytes)\n");
	printf("t = thousands (1,000s)\n");
	printf("m = millions  (1,000,000s)\n");
	printf("b = billions  (1,000,000,000s)\n");
}

void print_stat_info()
{
	XTStatMetaDataPtr	meta;
	char				buffer[40];
	char				desc[400];

	printf("Statistics :-\n");
	for (int i=0; i<XT_STAT_CURRENT_MAX; i++) {
		meta = xt_get_stat_meta_data(i);
		sprintf(desc, meta->sm_description, "milli");
		sprintf(buffer, "%s-%s", meta->sm_short_line_1, meta->sm_short_line_2);
		if (meta->sm_flags & XT_STAT_COMBO_FIELD) {
			/* Combine next 2 fields: */
			i++;
			strcat(buffer, "/ms");
			strcat(desc, "/time taken in milliseconds");
		}
		printf("%-13s %-21s - %s.\n", buffer, meta->sm_name, desc);
	}
}

bool match_arg(char *what, const char *opt, char **value)
{
	while (*what && *opt && isalpha(*what)) {
		if (*what != *opt)
			return false;
		what++;
		opt++;
	}
	if (*opt)
		return false;
	if (*what == '=')
		*value = what + 1;
	else if (*what)
		return false;
	else
		*value = NULL;
	return true;
}

void parse_args(int argc, char **argv)
{
	char			*ptr;
	char			*value;
	int				i = 1;
	struct Options	*opt;
	bool			found;

	while (i < argc) {
		ptr = argv[i];
		found = false;
		if (*ptr == '-') {
			ptr++;
			if (*ptr == '-') {
				ptr++;
				opt = options;
				while (opt->opt_id != OPT_NONE) {
					if (match_arg(ptr, opt->opt_name, &value)) {
						found = true;
						opt->opt_value_str = value;
						opt->opt_value_bool = true;
						break;
					}
					opt++;
				}
			}
			else {
				opt = options;
				while (opt->opt_id != OPT_NONE) {
					if (*ptr == opt->opt_char) {
						ptr++;
						if (*ptr)
							opt->opt_value_str = ptr;
						else {
							opt->opt_value_str = NULL;
							if (i+1 < argc) {
								ptr = argv[i+1];
								if (*ptr != '-') {
									opt->opt_value_str = ptr;
									i++;
								}
							}
						}
						found = true;
						opt->opt_value_bool = true;
						break;
					}
					opt++;
				}
			}
		}
		
		if (!found) {
			fprintf(stderr, "Unknown option: %s\n", argv[i]);
			print_help();
			exit(1);
		}

		if (opt->opt_flags & OPT_HAS_VALUE) {
			if (!(opt->opt_flags & OPT_OPTIONAL)) {
				if (!opt->opt_value_str) {
					fprintf(stderr, "Option requires a value: %s\n", argv[i]);
					printf("Use --help for help on commands and usage\n");
					exit(1);
				}
			}
		}
		else {
			if (opt->opt_value_str) {
				fprintf(stderr, "Option does not accept a value: %s\n", argv[i]);
				printf("Use --help for help on commands and usage\n");
				exit(1);
			}
		}

		if (opt->opt_value_str && (opt->opt_flags & OPT_INTEGER))
			opt->opt_value_int = atoi(opt->opt_value_str);

		if (opt->opt_id == OPT_HELP) {
			print_help();
			print_stat_key();
			print_stat_info();
			exit(1);
		}

		i++;
	}
}

#ifdef DEBUG_INTERRUPT
void interrupt_pbxt(MYSQL *conn)
{
	MYSQL_RES *res;

	if (mysql_query(conn, "show engine pbxt status")) {
		fprintf(stderr, "%s\n", mysql_error(conn));
		exit(1);
	}

	res = mysql_use_result(conn);
	mysql_free_result(res);
}
#endif

static bool display_parameters(MYSQL *conn)
{
	MYSQL_RES		*res;
	MYSQL_ROW		row;

	/* send SQL query */
	if (mysql_query(conn, "show variables like 'pbxt_%'"))
		return false;

	if (!(res = mysql_use_result(conn)))
		return false;

	/* output table name */
	printf("-- PBXT System Variables --\n");
	while ((row = mysql_fetch_row(res)) != NULL) {
		if (strcmp(row[0], "pbxt_index_cache_size") == 0)
			index_cache_size = xt_byte_size_to_int8(row[1]);
		else if (strcmp(row[0], "pbxt_record_cache_size") == 0)
			record_cache_size = xt_byte_size_to_int8(row[1]);
		else if (strcmp(row[0], "pbxt_log_cache_size") == 0)
			log_cache_size = xt_byte_size_to_int8(row[1]);
		printf("%-29s= %s\n", row[0], row[1]);
	}

	mysql_free_result(res);

	for (int i=0; i<XT_STAT_CURRENT_MAX; i++)
		accumulative_values[i] = 0;

	printf("Display options: %s\n", options[OPT_DISPLAY].opt_value_str);
	return true;
}

static bool connect(MYSQL *conn)
{
	unsigned int	type;

	if (strcasecmp(options[OPT_PROTOCOL].opt_value_str, "tcp") == 0)
		type = MYSQL_PROTOCOL_TCP;
	else if (strcasecmp(options[OPT_PROTOCOL].opt_value_str, "socket") == 0)
		type = MYSQL_PROTOCOL_SOCKET;
	else if (strcasecmp(options[OPT_PROTOCOL].opt_value_str, "pipe") == 0)
		type = MYSQL_PROTOCOL_PIPE;
	else if (strcasecmp(options[OPT_PROTOCOL].opt_value_str, "memory") == 0)
		type = MYSQL_PROTOCOL_MEMORY;
	else
		type = MYSQL_PROTOCOL_DEFAULT;

	if (mysql_options(conn, MYSQL_OPT_PROTOCOL, (char *) &type))
		return false;

	if (mysql_options(conn, MYSQL_READ_DEFAULT_GROUP, "xtstat"))
		return false;

	if (strcasecmp(options[OPT_DATABASE].opt_value_str, "pbxt") == 0)
		use_i_s = FALSE;
	else if (strcasecmp(options[OPT_DATABASE].opt_value_str, "information_schema") == 0)
		use_i_s = TRUE;
	else
		use_i_s = TRUE;

	/* Connect to database */
	if (!mysql_real_connect(conn,
			options[OPT_HOST].opt_value_str,
			options[OPT_USER].opt_value_str,
			options[OPT_PASSWORD].opt_value_str,
			options[OPT_DATABASE].opt_value_str,
			options[OPT_PORT].opt_value_int,
			options[OPT_SOCKET].opt_value_str,
			0))
		return false;

	return true;
}

int main(int argc, char **argv)
{
	MYSQL				*conn;
	MYSQL_RES			*res;
	MYSQL_ROW			row;
	llong				current_values[XT_STAT_CURRENT_MAX];
	double				value;
	char				str_value[100];
	XTStatMetaDataPtr	meta;
	int					len;
	int					stat;
	int					err;
	bool				select_worked = true;

	xt_set_time_unit("msec");
	parse_args(argc, argv);

	determine_display_order();

	if (!(conn = mysql_init(NULL))) {
		fprintf(stderr, "Insufficient memory\n");
		exit(1);
	}

	if (!connect(conn) || !display_parameters(conn)) {
		fprintf(stderr, "%s\n", mysql_error(conn));
		exit(1);
	}

	retry:
	for (int loop = 0; ; loop++) {
		if (use_i_s)
			err = mysql_query(conn, "select id, Value from information_schema.pbxt_statistics order by ID");
		else
			err = mysql_query(conn, "select id, Value from pbxt.statistics order by ID");
		if (err)
			goto reconnect;

		if (!(res = mysql_use_result(conn)))
			goto reconnect;
		select_worked = true;

		while ((row = mysql_fetch_row(res)) != NULL) {
			stat = atoi(row[0])-1;
			current_values[stat] = atoll(row[1]);
		}
		mysql_free_result(res);

#ifdef DEBUG_INTERRUPT
		if (current_values[XT_STAT_STAT_WRITES] - accumulative_values[XT_STAT_STAT_WRITES] == 0 &&
			current_values[XT_STAT_REC_SYNC_TIME] - accumulative_values[XT_STAT_REC_SYNC_TIME] == 0 &&
			current_values[XT_STAT_IND_SYNC_TIME] - accumulative_values[XT_STAT_IND_SYNC_TIME] == 0)
			interrupt_pbxt();
#endif

		if ((loop % 25) == 0) {
			for (int column=0; column<columns_used; column++) {
				len = 5;
				meta = xt_get_stat_meta_data(display_order[column].do_statistic);
				strcpy(str_value, meta->sm_short_line_1);
				if (display_order[column].do_combo) {
					/* Combine next 2 fields: */
					len = 8;
					column++;
				}
				else if (meta->sm_flags & XT_STAT_PERCENTAGE)
					len = 4;
				else if (meta->sm_flags & XT_STAT_DATE)
					len = 15;
				printf("%*s ", len, str_value);
			}
			printf("\n");
			for (int column=0; column<columns_used; column++) {
				len = 5;
				meta = xt_get_stat_meta_data(display_order[column].do_statistic);
				strcpy(str_value, meta->sm_short_line_2);
				if (display_order[column].do_combo) {
					/* Combine next 2 fields: */
					len = 8;
					column++;
					strcat(str_value, "/ms");
				}
				else if (meta->sm_flags & XT_STAT_PERCENTAGE)
					len = 4;
				else if (meta->sm_flags & XT_STAT_DATE)
					len = 15;
				printf("%*s ", len, str_value);
			}
			printf("\n");
		}

		for (int column=0; column<columns_used; column++) {
			len = 5;
			stat = display_order[column].do_statistic;
			meta = xt_get_stat_meta_data(stat);
			if (meta->sm_flags & XT_STAT_ACCUMULATIVE) {
				/* Take care of overflow! */
				if (current_values[stat] < accumulative_values[stat])
					value = (double) (0xFFFFFFFF - (accumulative_values[stat] - current_values[stat]));
				else
					value = (double) (current_values[stat] - accumulative_values[stat]);
			}
			else
				value = (double) current_values[stat];
			accumulative_values[stat] = current_values[stat];
			if (meta->sm_flags & XT_STAT_TIME_VALUE)
				value = value / (double) 1000;
			if (display_order[column].do_combo) {
				format_mini_count_value(str_value, value);
				strcat(str_value, "/");
				column++;
				stat = display_order[column].do_statistic;
				value = (double) (current_values[stat] - accumulative_values[stat]);
				accumulative_values[stat] = current_values[stat];
				value = value / (double) 1000;
				format_count_value(&str_value[strlen(str_value)], value);
				len = 8;
			}
			else if (meta->sm_flags & XT_STAT_PERCENTAGE) {
				double perc = 100;
				switch (stat) {
					case XT_STAT_REC_CACHE_USAGE:	perc = (double)record_cache_size; break;
					case XT_STAT_IND_CACHE_USAGE:	perc = (double)index_cache_size; break;
					case XT_STAT_XLOG_CACHE_USAGE:	perc = (double)log_cache_size; break;
				}
				format_percent_value(str_value, value, perc);
				len = 4;
			}
			else if (meta->sm_flags & XT_STAT_DATE) {
				time_t ticks = (time_t) value;
				const struct tm *ltime = localtime(&ticks);
				strftime(str_value, 99, "%y%m%d %H:%M:%S", ltime);
				len = 15;
			}
			else if (meta->sm_flags & XT_STAT_BYTE_COUNT)
				format_byte_value(str_value, value);
			else
				format_count_value(str_value, value);
			if (column == columns_used-1)
				printf("%*s\n", len, str_value);
			else
				printf("%*s ", len, str_value);
		}

		sleep(options[OPT_DELAY].opt_value_int);
	}

	/* close connection */
	mysql_close(conn);
	return 0;

	reconnect:
	/* Reconnect... */
	if (select_worked) {
		/* Only print message if the SELECT worked.
		 * or we will get a screen full of messages:
		 */
		fprintf(stderr, "%s\n", mysql_error(conn));
		printf("Reconnecting...\n");
	}
	mysql_close(conn);
	if (!(conn = mysql_init(NULL))) {
		fprintf(stderr, "Insufficient memory\n");
		exit(1);
	}
	do {
		sleep(2);
	} while (!connect(conn));
	select_worked = false;
	goto retry;
}
