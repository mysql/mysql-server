/************************************************************************
Test for the client: interactive SQL

(c) 1996-1997 Innobase Oy

Created 2/16/1996 Heikki Tuuri
*************************************************************************/

#include "univ.i"
#include "ib_odbc.h"
#include "mem0mem.h"
#include "sync0sync.h"
#include "os0thread.h"
#include "os0proc.h"
#include "os0sync.h"
#include "srv0srv.h"

ulint	n_exited	= 0;

char	cli_srv_endpoint_name[100];
char	cli_user_name[100];

ulint	n_warehouses	= ULINT_MAX;
ulint	n_customers_d	= ULINT_MAX;
bool	is_tpc_d	= FALSE;
ulint	n_rounds	= ULINT_MAX;
ulint	n_users		= ULINT_MAX;
ulint	startdate	= 0;
ulint	enddate		= 0;
bool	own_warehouse	= FALSE;

ulint	mem_pool_size	= ULINT_MAX;

/*************************************************************************
Reads a keywords and a values from an initfile. In case of an error, exits
from the process. */
static
void
cli_read_initfile(
/*==============*/
	FILE*	initfile)	/* in: file pointer */
{
	char	str_buf[10000];
	ulint	ulint_val;

	srv_read_init_val(initfile, FALSE, "SRV_ENDPOINT_NAME", str_buf,
								&ulint_val);

	ut_a(ut_strlen(str_buf) < COM_MAX_ADDR_LEN);
	
	ut_memcpy(cli_srv_endpoint_name, str_buf, COM_MAX_ADDR_LEN);

	srv_read_init_val(initfile, FALSE, "USER_NAME", str_buf,
								&ulint_val);
	ut_a(ut_strlen(str_buf) < COM_MAX_ADDR_LEN);
	
	ut_memcpy(cli_user_name, str_buf, COM_MAX_ADDR_LEN);

	srv_read_init_val(initfile, TRUE, "MEM_POOL_SIZE", str_buf,
							&mem_pool_size);
	
	srv_read_init_val(initfile, TRUE, "N_WAREHOUSES", str_buf,
							&n_warehouses);
	
	srv_read_init_val(initfile, TRUE, "N_CUSTOMERS_D", str_buf,
							&n_customers_d);

	srv_read_init_val(initfile, TRUE, "IS_TPC_D", str_buf,
							&is_tpc_d);

	srv_read_init_val(initfile, TRUE, "N_ROUNDS", str_buf,
							&n_rounds);

	srv_read_init_val(initfile, TRUE, "N_USERS", str_buf,
							&n_users);

	srv_read_init_val(initfile, TRUE, "STARTDATE", str_buf,
							&startdate);

	srv_read_init_val(initfile, TRUE, "ENDDATE", str_buf,
							&enddate);	

	srv_read_init_val(initfile, TRUE, "OWN_WAREHOUSE", str_buf,
							&own_warehouse);	
}

/*************************************************************************
Reads configuration info for the client. */
static
void
cli_boot(
/*=====*/
	char*	name)	/* in: the initialization file name */
{
	FILE*	initfile;
	
	initfile = fopen(name, "r");

	if (initfile == NULL) {
		printf(
	"Error in client booting: could not open initfile whose name is %s!\n",
									name);
		os_process_exit(1);
	}			

	cli_read_initfile(initfile);

	fclose(initfile);
}
	
/*********************************************************************
Interactive SQL loop. */
static
void
isql(
/*=*/
	FILE*	inputfile)	/* in: input file containing SQL strings,
				or stdin */
{
	HENV	env;
	HDBC	conn;
	RETCODE	ret;
	HSTMT	sql_query;
	ulint	tm, oldtm;
	char	buf[1000];
	char*	str;
	ulint	count;
	ulint	n_begins;
	ulint	len;
	ulint	n;
	ulint	i;
	ulint	n_lines;
	
	ret = SQLAllocEnv(&env);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLAllocConnect(env, &conn);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLConnect(conn, (UCHAR*)cli_srv_endpoint_name,
				(SWORD)ut_strlen(cli_srv_endpoint_name),
				cli_user_name,
				(SWORD)ut_strlen(cli_user_name),
						(UCHAR*)"password", 8);
	ut_a(ret == SQL_SUCCESS);

	printf("Connection established\n");

	printf("Interactive SQL performs queries by first making a stored\n");
	printf("procedure from them, and then calling the procedure.\n");
	printf("Put a semicolon after each statement and\n");
	printf("end your query with two <enter>s.\n\n");
	printf("You can also give a single input file\n");
	printf("as a command line argument to isql.\n\n");
	printf("In the file separate SQL queries and procedure bodies\n");
	printf("by a single empty line. Do not write the final END; into\n");
	printf("a procedure body.\n\n");

	count = 0;
loop:
	count++;
	n = 0;
	n_lines = 0;
	
	sprintf(buf, "PROCEDURE P%s%lu () IS\nBEGIN ", cli_user_name,
								count);
	for (;;) {
		len = ut_strlen(buf + n) - 1;
		n += len;

		if (len == 0) {
			break;
		} else {
			sprintf(buf + n, "\n");
			n++;
			n_lines++;
		}
 
		str = fgets(buf + n, 1000, inputfile);

		if ((str == NULL) && (inputfile != stdin)) {
			/* Reached end-of-file: switch to input from
			keyboard */

			inputfile = stdin;

			break;
		}
		
		ut_a(str); 
	}

	if (n_lines == 1) {
		/* Empty procedure */

		goto loop;
	}	

	/* If the statement is actually the body of a procedure,
	erase the first BEGIN from the string: */

	n_begins = 0;
	
	for (i = 0; i < n - 5; i++) {

		if (ut_memcmp(buf + i, "BEGIN", 5) == 0) {

			n_begins++;
		}
	}

	if (n_begins > 1) {

		for (i = 0; i < n - 5; i++) {

			if (ut_memcmp(buf + i, "BEGIN", 5) == 0) {

				/* Erase the first BEGIN: */
				ut_memcpy(buf + i, "     ", 5);

				break;
			}
		}
	}
	
	sprintf(buf + n, "END;\n");
	
	printf("SQL procedure to execute:\n%s\n", buf);

	ret = SQLAllocStmt(conn, &sql_query);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLPrepare(sql_query, (UCHAR*)buf, ut_strlen(buf));

	ut_a(ret == SQL_SUCCESS);

	ret = SQLExecute(sql_query);

	ut_a(ret == SQL_SUCCESS);

	sprintf(buf, "{P%s%lu ()}", cli_user_name, count);
							
	ret = SQLAllocStmt(conn, &sql_query);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLPrepare(sql_query, (UCHAR*)buf, ut_strlen(buf));

	ut_a(ret == SQL_SUCCESS);

	printf("Starting to execute the query\n");

	oldtm = ut_clock();

	ret = SQLExecute(sql_query);

	tm = ut_clock();

	printf("Wall time for query %lu milliseconds\n\n", tm - oldtm);

	ut_a(ret == SQL_SUCCESS);

	goto loop;
}

/********************************************************************
Main test function. */

void 
main(int argc, char* argv[]) 
/*========================*/
{
	ulint	tm, oldtm;
	FILE*	inputfile;
	
	if (argc > 2) {
		printf("Only one input file allowed\n");

		os_process_exit(1);

	} else if (argc == 2) {
		inputfile = fopen(argv[1], "r");

		if (inputfile == NULL) {
			printf(
	"Error: could not open the inputfile whose name is %s!\n",
							argv[1]);
		os_process_exit(1);
		}
	} else {
		inputfile = stdin;
	}

	cli_boot("cli_init");

	sync_init();

	mem_init(mem_pool_size);

	oldtm = ut_clock();

	isql(inputfile);

	tm = ut_clock();

	printf("Wall time for test %lu milliseconds\n", tm - oldtm);
	
	printf("TESTS COMPLETED SUCCESSFULLY!\n");
}
