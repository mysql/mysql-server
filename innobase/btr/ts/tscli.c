/************************************************************************
Tests for the client, TPC-C, and TPC-D Query 5

(c) 1996-1998 Innobase Oy

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

/* Disk wait simulation array */
typedef struct srv_sim_disk_struct	srv_sim_disk_t;
struct srv_sim_disk_struct{
	os_event_t	event;	/* OS event to wait */
	bool		event_set;/* TRUE if the event is in the set state */
	bool		empty;	/* TRUE if this cell not reserved */
};

#define SRV_N_SIM_DISK_ARRAY	150

srv_sim_disk_t	srv_sim_disk[SRV_N_SIM_DISK_ARRAY];

/* Random counter used in disk wait simulation */
ulint	srv_disk_rnd	= 982364761;
ulint	srv_disk_n_active_threads	= 0;

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

/*********************************************************************
Test for TPC-C. */

ulint
test_init(
/*======*/
	void*	arg)
{
	HENV	env;
	HDBC	conn;
	RETCODE	ret;
	HSTMT	stat;
	HSTMT	create_query;
	HSTMT	populate_query;
	char*	str;
	char*	str1;
	char*	str2;
	char*	str3;
	char*	str4;
	char*	str5;
	char*	str6;
	char*	create_str;
	char*	populate_str;
	char*	commit_str;
	char*	new_order_str;
	char*	payment_str;
	char*	order_status_str;
	char*	delivery_str;
	char*	stock_level_str;
	char*	consistency_str;
	char*	query_5_str;
	char*	print_str;
	char*	lock_wait_str;
	char*	join_test_str;
	char*	test_errors_str;
	char*	test_group_commit_str;
	char*	test_single_row_select_str;
	char*	rollback_str;
	char*	ibuf_test_str;
	SDWORD	n_warehouses_buf;
	SDWORD	n_warehouses_len;
	SDWORD	n_customers_d_buf;
	SDWORD	n_customers_d_len;
	
	UT_NOT_USED(arg);
	
	/*------------------------------------------------------*/

	str1 =
	
"	PROCEDURE CREATE_TABLES () IS"
"	BEGIN"
"	CREATE TABLE WAREHOUSE (W_ID CHAR, W_NAME CHAR,"
"				W_STREET_1 CHAR, W_STREET_2 CHAR,"
"				W_CITY CHAR,"
"				W_STATE CHAR, W_ZIP CHAR,"
"				W_TAX INT,"
"				W_YTD_HIGH INT,"
"				W_YTD INT);"
""	
"	CREATE UNIQUE CLUSTERED INDEX W_IND ON WAREHOUSE (W_ID);"
""	
"	CREATE TABLE DISTRICT (D_ID CHAR, D_W_ID CHAR,"
"				D_NAME CHAR,"
"				D_STREET_1 CHAR, D_STREET_2 CHAR,"
"				D_CITY CHAR,"
"				D_STATE CHAR, D_ZIP CHAR,"
"				D_TAX INT,"
"				D_YTD_HIGH INT,"
"				D_YTD INT,"
"				D_NEXT_O_ID INT);"
""	
"	CREATE UNIQUE CLUSTERED INDEX D_IND ON DISTRICT (D_W_ID, D_ID);"
""	
"	CREATE TABLE CUSTOMER (C_ID CHAR, C_D_ID CHAR, C_W_ID CHAR,"
"				C_FIRST CHAR, C_MIDDLE CHAR,"
"				C_LAST CHAR,"
"				C_STREET_1 CHAR, C_STREET_2 CHAR,"
"				C_CITY CHAR,"
"				C_STATE CHAR, C_ZIP CHAR,"
"				C_PHONE CHAR,"
"				C_SINCE_TIME INT,"
"				C_SINCE INT,"
"				C_CREDIT CHAR,"
"				C_CREDIT_LIM_HIGH INT,"
"				C_CREDIT_LIM INT,"
"				C_DISCOUNT INT,"
"				C_BALANCE_HIGH INT,"
"				C_BALANCE INT,"
"				C_YTD_PAYMENT_HIGH INT,"
"				C_YTD_PAYMENT INT,"
"				C_PAYMENT_CNT INT,"
"				C_DELIVERY_CNT INT,"
"				C_DATA CHAR) /*DOES_NOT_FIT_IN_MEMORY*/;"
""	
"	CREATE UNIQUE CLUSTERED INDEX C_IND ON CUSTOMER (C_W_ID, C_D_ID,"
"								C_ID);"
""	
"	CREATE INDEX C_LAST_IND ON CUSTOMER (C_W_ID, C_D_ID, C_LAST,"
"							C_FIRST);"
""	
"	CREATE TABLE HISTORY (H_C_ID CHAR, H_C_D_ID CHAR, H_C_W_ID CHAR,"
"				H_D_ID CHAR, H_W_ID CHAR,"
"				H_DATE INT,"
"				H_AMOUNT INT,"
"				H_DATA CHAR);"
""	
"	CREATE CLUSTERED INDEX H_IND ON HISTORY (H_W_ID);"
""	
"	CREATE TABLE NEW_ORDER (NO_O_ID INT,"
"				NO_D_ID CHAR,"
"				NO_W_ID CHAR);"
""	
"	CREATE UNIQUE CLUSTERED INDEX NO_IND ON NEW_ORDER (NO_W_ID, NO_D_ID,"
"								NO_O_ID);"
	;

	str2 =
	  							
"	CREATE TABLE ORDERS (O_ID INT, O_D_ID CHAR, O_W_ID CHAR,"
"	 			O_C_ID CHAR,"
"				O_ENTRY_D INT,"
"				O_CARRIER_ID INT,"
"				O_OL_CNT INT,"
"				O_ALL_LOCAL CHAR);"
""	
"	CREATE UNIQUE CLUSTERED INDEX O_IND ON ORDERS (O_W_ID, O_D_ID,"
"								O_ID);"
"	CREATE INDEX O_C_IND ON ORDERS (O_W_ID, O_D_ID, O_C_ID);"
""	
"	CREATE TABLE ORDER_LINE (OL_O_ID INT, OL_D_ID CHAR, OL_W_ID CHAR,"
"				OL_NUMBER CHAR,"
"				OL_I_ID CHAR,"
"				OL_SUPPLY_W_ID CHAR,"
"				OL_DELIVERY_D INT,"
"				OL_QUANTITY INT,"
"				OL_AMOUNT INT,"
"				OL_DIST_INFO CHAR);"
""	
"	CREATE UNIQUE CLUSTERED INDEX OL_IND ON ORDER_LINE"
"				(OL_W_ID, OL_D_ID, OL_O_ID, OL_NUMBER);"
""	
"	CREATE TABLE ITEM (I_ID CHAR, I_IM_ID CHAR, I_NAME CHAR,"
"				I_PRICE INT,"
"				I_DATA CHAR);"
""	
"	CREATE UNIQUE CLUSTERED INDEX I_IND ON ITEM (I_ID);"
""	
"	CREATE TABLE STOCK (S_I_ID CHAR,"
"				S_W_ID CHAR,"
"				S_QUANTITY INT,"
"				S_DIST_01 CHAR,"
"				S_DIST_02 CHAR,"
"				S_DIST_03 CHAR,"
"				S_DIST_04 CHAR,"
"				S_DIST_05 CHAR,"
"				S_DIST_06 CHAR,"
"				S_DIST_07 CHAR,"
"				S_DIST_08 CHAR,"
"				S_DIST_09 CHAR,"
"				S_DIST_10 CHAR,"
"				S_YTD INT,"
"				S_ORDER_CNT INT,"
"				S_REMOTE_CNT INT,"
"				S_DATA CHAR) /*DOES_NOT_FIT_IN_MEMORY*/;"
""	
"	CREATE UNIQUE CLUSTERED INDEX S_IND ON STOCK (S_W_ID, S_I_ID);"
""
""
"	CREATE TABLE REGION (R_REGIONKEY INT, R_NAME CHAR, R_COMMENT CHAR);"
""
"	CREATE UNIQUE CLUSTERED INDEX R_IND ON REGION (R_REGIONKEY);"
""
"	CREATE TABLE NATION (N_NATIONKEY INT, N_NAME CHAR, N_REGIONKEY INT,"
"							N_COMMENT CHAR);"
"	CREATE UNIQUE CLUSTERED INDEX N_IND ON NATION (N_NATIONKEY);"
""
"	CREATE TABLE NATION_2 (N2_NATIONKEY INT, N2_NAME CHAR,"
"					N2_REGIONKEY INT, N2_COMMENT CHAR);"
"	CREATE UNIQUE CLUSTERED INDEX N2_IND ON NATION_2 (N2_NAME);"
""
"	CREATE TABLE SUPPLIER (S_SUPPKEY INT, S_NAME CHAR, S_ADDRESS CHAR,"
"					S_NATIONKEY INT, S_PHONE CHAR,"
"					S_ACCTBAL INT, S_COMMENT CHAR);"
"	CREATE UNIQUE CLUSTERED INDEX SU_IND ON SUPPLIER (S_SUPPKEY);"
""
"	CREATE TABLE CUSTOMER_D (C_CUSTKEY INT, C_NAME CHAR, C_ADDRESS CHAR,"
"					C_NATIONKEY INT, C_PHONE CHAR,"
"					C_ACCTBAL INT, C_MKTSEGMENT CHAR,"
"					C_COMMENT CHAR);"
"	CREATE UNIQUE CLUSTERED INDEX CU_IND ON CUSTOMER_D (C_CUSTKEY);"
""
"	CREATE TABLE ORDERS_D (O_ORDERKEY INT, O_CUSTKEY INT,"
"					O_ORDERSTATUS CHAR, O_TOTALPRICE INT,"
"					O_ORDERDATE INT,"
"					O_ORDERPRIORITY CHAR,"
"					O_CLERK CHAR, O_SHIPPRIORITY INT,"
"					O_COMMENT CHAR);"
""
"	CREATE UNIQUE CLUSTERED INDEX OR_IND ON ORDERS_D (O_ORDERKEY);"
""
"	CREATE INDEX OR_D_IND ON ORDERS_D (O_ORDERDATE, O_ORDERKEY,"
"								O_CUSTKEY);"
""
"	CREATE TABLE LINEITEM (L_ORDERKEY INT, L_PARTKEY INT, L_SUPPKEY INT,"
"					L_LINENUMBER INT, L_QUANTITY INT,"
"					L_EXTENDEDPRICE INT,"
"					L_DISCOUNT INT, L_TAX INT,"
"					L_RETURNFLAG CHAR,"
"					L_LINESTATUS CHAR,"
"					L_SHIPDATE INT, L_COMMITDATE INT,"
"					L_RECEIPTDATE INT,"
"					L_SHIPINSTRUCT CHAR,"
"					L_SHIPMODE CHAR, L_COMMENT CHAR);"
""	
"	CREATE UNIQUE CLUSTERED INDEX L_IND ON LINEITEM (L_ORDERKEY,"
"							L_LINENUMBER);"
""
"	CREATE TABLE ACCOUNTA (A_NUM INT, A_BAL INT);"
""	
"	CREATE UNIQUE CLUSTERED INDEX ACCOUNTA_IND ON ACCOUNTA (A_NUM);"
""
"	CREATE TABLE TELLERA (T_NUM INT, T_BAL INT);"
""	
"	CREATE UNIQUE CLUSTERED INDEX TELLERA_IND ON TELLERA (T_NUM);"
""
"	CREATE TABLE BRANCHA (B_NUM INT, B_BAL INT);"
""	
"	CREATE UNIQUE CLUSTERED INDEX BRANCHA_IND ON BRANCHA (B_NUM);"
""
"	CREATE TABLE HISTORYA (H_NUM INT, H_TEXT CHAR);"
""	
"	CREATE CLUSTERED INDEX HISTORYA_IND ON HISTORYA (H_NUM);"
""
"	CREATE TABLE JTEST1 (JT1_A INT, JT1_B INT);"
""	
"	CREATE UNIQUE CLUSTERED INDEX JT_IND1 ON JTEST1 (JT1_A);"
""
"	CREATE TABLE JTEST2 (JT2_A INT, JT2_B INT);"
""	
"	CREATE UNIQUE CLUSTERED INDEX JT_IND2 ON JTEST2 (JT2_A);"
""
"	CREATE TABLE IBUF_TEST (IB_A INT, IB_B CHAR) DOES_NOT_FIT_IN_MEMORY;"
""	
"	CREATE UNIQUE CLUSTERED INDEX IBUF_IND ON IBUF_TEST (IB_A);"
"	END;"
	;

	create_str = ut_str_catenate(str1, str2);
	/*-----------------------------------------------------------*/

	str1 = 

"	PROCEDURE POPULATE_TABLES (n_warehouses IN INT, n_customers_d"
"								IN INT) IS"
""
"	i INT;"
"	j INT;"
"	k INT;"
"	t INT;"
"	string CHAR;"
"	rnd1 INT;"
"	rnd2 INT;"
"	rnd INT;"
"	n_items INT;"
"	n_districts INT;"
"	n_customers INT;"
""
"	BEGIN"
""
"/**********************************************************/"
"	PRINTF('Starting Mikko-test');"
""
"	FOR i IN 1 .. 5 LOOP"
"		INSERT INTO IBUF_TEST VALUES (i, 'Mikko');"
"	END LOOP;"
""
"	/* PRINTF('Printing rows from Mikko-test:');"
""
"	ROW_PRINTF SELECT * FROM IBUF_TEST; */"
""
"	SELECT SUM(IB_A) INTO t FROM IBUF_TEST;"
""
"	PRINTF('Sum of 1 to ', i, ' is ', t);"
"	ASSERT(t = (i * (i + 1)) / 2);"
""
"	ROLLBACK WORK;"
""
"	PRINTF('Printing rows from Mikko-test after rollback:');"
""
"	ROW_PRINTF SELECT * FROM IBUF_TEST;"
""
"/**********************************************************/"
"	FOR i IN 0 .. 100 LOOP"
"		INSERT INTO ACCOUNTA VALUES (i, i);"
"		INSERT INTO TELLERA VALUES (i, i);"
"		INSERT INTO BRANCHA VALUES (i, i);"
"		INSERT INTO HISTORYA VALUES (i, '12345678901234567890');"
"	END LOOP;"
""
"	COMMIT WORK;"
"/**********************************************************/"
"/*	PRINTF('Populating ibuf test tables');"
"	FOR i IN 1 .. 1000 LOOP"
"		INSERT INTO IBUF_TEST VALUES (i, RND_STR(RND(1, 2000)));"
"	END LOOP;"
"	PRINTF('Ibuf test tables populated');"
"	COMMIT WORK; */"
""
"	n_items := 200;"
"	n_districts := 10;"
"	n_customers := 20;"
""
"	PRINTF('Starting to populate ITEMs');"
""
"	FOR i IN 1 .. n_items LOOP"
"		rnd1 := RND(26, 50);"
"		string := RND_STR(rnd1);"
""
"		IF (RND(0, 99) < 10) THEN"
"			rnd2 := RND(0, rnd1 - 8);"
"			REPLSTR(string, 'ORIGINAL', rnd2, 8);"
"		END IF;"
""
"		INSERT INTO ITEM VALUES (TO_BINARY(i, 3),"
"					TO_BINARY(RND(1, 10000), 3),"
"					RND_STR(RND(14, 24)),"
"					RND(100, 10000),"
"					string);"
"	END LOOP;"
"	COMMIT WORK;"
""
"	FOR i IN 1 .. n_warehouses LOOP"
"		COMMIT WORK;"
"		PRINTF('Starting to populate warehouse number ', i);"
"		INSERT INTO WAREHOUSE VALUES (TO_BINARY(i, 2),"
"					RND_STR(RND(6, 10)),"
"					RND_STR(RND(10, 20)),"
"					RND_STR(RND(10, 20)),"
"					RND_STR(RND(10, 20)),"
"					RND_STR(2),"
"					CONCAT(SUBSTR(TO_CHAR(RND(0, 9999)),"
"							6, 4),"
"						'11111'),"
"					RND(0, 2000),"
"					0,"
"					0);"
"		FOR j IN 1 .. n_items LOOP"
""
"			rnd1 := RND(26, 50);"
"			string := RND_STR(rnd1);"
""
"			IF (RND(0, 99) < 10) THEN"
"				rnd2 := RND(0, rnd1 - 8);"
"				REPLSTR(string, 'ORIGINAL', rnd2, 8);"
"			END IF; "
""
"			INSERT INTO STOCK VALUES (TO_BINARY(j, 3),"
"						TO_BINARY(i, 2),"
"						91,"
"						RND_STR(24),"
"						RND_STR(24),"
"						RND_STR(24),"
"						RND_STR(24),"
"						RND_STR(24),"
"						RND_STR(24),"
"						RND_STR(24),"
"						RND_STR(24),"
"						RND_STR(24),"
"						RND_STR(24),"
"						0, 0, 0,"
"						string);"
"		END LOOP;"
	;

	str2 =
"		FOR j IN 1 .. n_districts LOOP"
""
"		/* PRINTF('Starting to populate district number ', j); */"
"			INSERT INTO DISTRICT VALUES (TO_BINARY(j + 47, 1),"
"						TO_BINARY(i, 2),"
"						RND_STR(RND(6, 10)),"
"						RND_STR(RND(10, 20)),"
"						RND_STR(RND(10, 20)),"
"						RND_STR(RND(10, 20)),"
"						RND_STR(2),"
"						CONCAT(SUBSTR("
"							TO_CHAR(RND(0, 9999)),"
"							6, 4),"
"						'11111'),"
"						RND(0, 2000),"
"						0,"
"						0,"
"						3001);"
""
"			FOR k IN 1 .. n_customers LOOP"
""
"				string := 'GC';"
""
"				IF (RND(0, 99) < 10) THEN"
"					string := 'BC';"
"				END IF;"
"				"
"				INSERT INTO CUSTOMER VALUES ("
"						TO_BINARY(k, 3),"
"						TO_BINARY(j + 47, 1),"
"						TO_BINARY(i, 2),"
"						RND_STR(RND(8, 16)),"
"						'OE',"
"						CONCAT('NAME',"
"							TO_CHAR(k / 3)),"
"						RND_STR(RND(10, 20)),"
"						RND_STR(RND(10, 20)),"
"						RND_STR(RND(10, 20)),"
"						RND_STR(2),"
"						CONCAT(SUBSTR("
"							TO_CHAR(RND(0, 9999)),"
"							6, 4),"
"						'11111'),"
"						RND_STR(16),"
"						SYSDATE(), 0,"
"						string,"
"						0, 5000000,"
"						RND(0, 5000),"
"						0, 0, 0, 0, 0, 0,"
"						RND_STR(RND(300, 500)));"
	;

	str3 =
"				INSERT INTO HISTORY VALUES ("
"						TO_BINARY(k, 3),"
"						TO_BINARY(j + 47, 1),"
"						TO_BINARY(i, 2),"
"						TO_BINARY(j + 47, 1),"
"						TO_BINARY(i, 2),"
"						SYSDATE(),"
"						1000,"
"						RND_STR(RND(12, 24)));"
""
"				rnd1 := RND(5, 15);"
""
"				INSERT INTO ORDERS VALUES ("
"						k,"
"						TO_BINARY(j + 47, 1),"
"						TO_BINARY(i, 2),"
"						TO_BINARY(k, 3),"
"						SYSDATE(),"
"						RND(1, 10),"
"						rnd1,"
"						'1');"
""
"				FOR t IN 1 .. rnd1 LOOP"
"					INSERT INTO ORDER_LINE VALUES ("
"						k,"
"						TO_BINARY(j + 47, 1),"
"						TO_BINARY(i, 2),"
"						TO_BINARY(t, 1),"
"						TO_BINARY("
"							RND(1, n_items),"
"							3),"
"						TO_BINARY(i, 2),"
"						NULL,"
"						91,"
"						RND(0, 9999),"
"						RND_STR(24));"
"				END LOOP;"
"			END LOOP;"
"			"
"			FOR k IN 1   /* + (2 * n_customers) / 3 */"
"							.. n_customers LOOP"
"				"
"				INSERT INTO NEW_ORDER VALUES ("
"						k,"
"						TO_BINARY(j + 47, 1),"
"						TO_BINARY(i, 2));"
"			END LOOP;"
"		END LOOP;"
"	END LOOP;"
""
"	COMMIT WORK;"
""
"	PRINTF('Populating TPC-D tables');"
""
"	FOR i IN 0 .. 4 LOOP"
"		/* We set the last columns to a long character string, to"
"		reduce latch contention on region and nation database pages."
"		A similar effect could be achieved by setting the page"
"		fillfactor in these tables low. */"
""
"		INSERT INTO REGION VALUES (i, CONCAT('Region', TO_CHAR(i),"
"							'               '),"
"						RND_STR(1500 + RND(1, 152)));"
"		FOR j IN i * 5 .. i * 5 + 4 LOOP"
"			INSERT INTO NATION VALUES (j,"
"					CONCAT('Nation', TO_CHAR(j),"
"							'               '),"
"					i, RND_STR(1500 + RND(1, 152)));"
"			INSERT INTO NATION_2 VALUES (j,"
"					CONCAT('Nation', TO_CHAR(j),"
"							'               '),"
"					i, RND_STR(1500 + RND(1, 152)));"
"		END LOOP;"
"	END LOOP;"
""
"	COMMIT WORK;"
""
"	FOR i IN 0 .. n_customers_d / 15 LOOP"
"		INSERT INTO SUPPLIER VALUES (i,"
"					CONCAT('Supplier', TO_CHAR(i)),"
"					RND_STR(RND(20, 30)),"
"					RND(0, 24),"
"					RND_STR(15),"
"					RND(1, 1000),"
"					RND_STR(RND(40, 80)));"
"	END LOOP;"
""
"	COMMIT WORK;"
""
"	FOR i IN 0 .. n_customers_d - 1 LOOP"
"		IF ((i / 100) * 100 = i) THEN"
"			COMMIT WORK;"
"			PRINTF('Populating customer ', i);"
"		END IF;"
""
"		INSERT INTO CUSTOMER_D VALUES (i,"
"					CONCAT('Customer', TO_CHAR(i)),"
"					RND_STR(RND(20, 30)),"
"					RND(0, 24),"
"					RND_STR(15),"
"					RND(1, 1000),"
"					RND_STR(10),"
"					RND_STR(RND(50, 100)));"
""
"		FOR j IN i * 10 .. i * 10 + 9 LOOP"
""
"			rnd := (j * 2400) / (10 * n_customers_d);"
""
"			INSERT INTO ORDERS_D VALUES (j,"
"					3 * RND(0, (n_customers_d / 3) - 1)"
"					+ RND(1, 2),"
"					'F', 1000,"
"					rnd,"
"					RND_STR(10),"
"				     CONCAT('Clerk', TO_CHAR(RND(0, 1000))),"
"				     	0, RND_STR(RND(3, 7)));"
""
"			FOR k IN 0 .. RND(0, 6) LOOP"
"				INSERT INTO LINEITEM VALUES (j,"
"					RND(1, 1000),"
"					RND(0, n_customers_d / 15),"
"					k,"
"					RND(1, 50),"
"					100,"
"					5,"
"					RND(0, 8),"
"					'N',"
"					'F',"
"					rnd + RND(1, 100),"
"					rnd + RND(1, 100),"
"					rnd + RND(1, 100),"
"					RND_STR(1),"
"					RND_STR(1),"
"					RND_STR(RND(1, 3)));"
"			END LOOP;"
"		END LOOP;"
""
"	END LOOP;"
""
"	COMMIT WORK;"
"	PRINTF('TPC-D tables populated');"
""
"	PRINTF('Populating join test tables');"
"	FOR i IN 1 .. 1 LOOP"
"		INSERT INTO JTEST1 VALUES (i, i);"
"		INSERT INTO JTEST2 VALUES (i, i);"
"	END LOOP;"
"	PRINTF('Join test tables populated');"
""
"	COMMIT WORK;"
"	END;"
	;

	str4 = ut_str_catenate(str1, str2);
	populate_str = ut_str_catenate(str4, str3);

	/*-----------------------------------------------------------*/
	str = 

"	PROCEDURE PRINT_TABLES () IS"
"	i INT;"	
"	BEGIN"
""
"	/* PRINTF('Printing ITEM table:');"
""
"	ROW_PRINTF"
"		SELECT *"
"			FROM ITEM;"
""
"	PRINTF('Printing WAREHOUSE table:');"
""
"	ROW_PRINTF"
"		SELECT *"
"			FROM WAREHOUSE;"
""
"	PRINTF('Printing STOCK table:');"
""
"	ROW_PRINTF"
"		SELECT *"
"			FROM STOCK;"
""
"	PRINTF('Printing DISTRICT table:');"
""
"	ROW_PRINTF"
"		SELECT *"
"			FROM DISTRICT;"
""
"	PRINTF('Printing CUSTOMER table:');"
""
"	ROW_PRINTF"
"		SELECT *"
"			FROM CUSTOMER;"
""
"	PRINTF('Printing HISTORY table:');"
""
"	ROW_PRINTF"
"		SELECT *"
"			FROM HISTORY;"
""
"	PRINTF('Printing ORDERS table:');"
""
"	ROW_PRINTF"
"		SELECT *"
"			FROM ORDERS;"
""
"	PRINTF('Printing ORDER_LINE table:');"
""
"	ROW_PRINTF"
"		SELECT *"
"			FROM ORDER_LINE"
"			WHERE OL_O_ID >= 3000; */"
""
"	PRINTF('Printing NEW_ORDER table:');"
""
"	ROW_PRINTF"
"		SELECT *"
"			FROM NEW_ORDER;"
""
"	COMMIT WORK;"
"	END;"
	;

	print_str = str;
	/*-----------------------------------------------------------*/
	commit_str =

"	PROCEDURE COMMIT_TEST () IS"
"	"
"	BEGIN"
"	COMMIT WORK;"
"	END;"
	;

	/*-----------------------------------------------------------*/

	str1 =
	
"	PROCEDURE NEW_ORDER (c_w_id IN CHAR,"
"				c_d_id IN CHAR,"
"				c_id IN CHAR,"
"				ol_supply_w_ids IN CHAR,"
"				ol_i_ids IN CHAR,"
"				ol_quantities IN CHAR,"
"				c_last OUT CHAR,"
"				c_credit OUT CHAR,"
"				c_discount OUT INT,"
"				w_tax OUT INT,"
"				d_tax OUT INT,"
"				o_ol_count OUT INT,"
"				o_id OUT INT,"
"				o_entry_d OUT INT,"
"				total OUT INT,"
"				i_names OUT CHAR,"
"				s_quantities OUT CHAR,"
"				bg OUT CHAR,"
"				i_prices OUT CHAR,"
"				ol_amounts OUT CHAR) IS"
""
" 	i INT;"
" 	j INT;"
" 	o_all_local CHAR;"
" 	i_price INT;"
" 	i_name CHAR;"
" 	i_data CHAR;"
" 	s_quantity INT;"
" 	s_data CHAR;"
" 	s_dist_01 CHAR;"
" 	s_dist_02 CHAR;"
" 	s_dist_03 CHAR;"
" 	s_dist_04 CHAR;"
" 	s_dist_05 CHAR;"
" 	s_dist_06 CHAR;"
" 	s_dist_07 CHAR;"
" 	s_dist_08 CHAR;"
" 	s_dist_09 CHAR;"
" 	s_dist_10 CHAR;"
" 	ol_i_id CHAR;"
" 	ol_quantity INT;"
" 	ol_amount INT;"
" 	ol_supply_w_id CHAR;"
" 	ol_dist_info CHAR;"
""
" 	DECLARE CURSOR district_cursor IS"
" 		SELECT D_NEXT_O_ID, D_TAX"
" 			FROM DISTRICT"
" 			WHERE D_ID = c_d_id AND D_W_ID = c_w_id"
" 			FOR UPDATE;"
""
" 	DECLARE CURSOR stock_cursor IS"
" 		SELECT S_QUANTITY, S_DATA,"
"				S_DIST_01, S_DIST_02, S_DIST_03, S_DIST_04,"
"				S_DIST_05, S_DIST_06, S_DIST_07, S_DIST_08,"
"				S_DIST_09, S_DIST_10"
"			FROM STOCK"
"			WHERE S_W_ID = ol_supply_w_id AND S_I_ID = ol_i_id"
"			FOR UPDATE;"
	;
	str2 =
	
" 	BEGIN"
"	FOR j IN 1 .. 1 LOOP"
""
"	/* PRINTF('Warehouse ', BINARY_TO_NUMBER(c_w_id)); */"
" 	o_all_local := '1';"
"	i_names := '12345678901234567890123456789012345678901234567890"
			"12345678901234567890123456789012345678901234567890"
			"12345678901234567890123456789012345678901234567890"
			"12345678901234567890123456789012345678901234567890"
			"12345678901234567890123456789012345678901234567890"
			"12345678901234567890123456789012345678901234567890"
			"12345678901234567890123456789012345678901234567890"
			"1234567890';"
"	s_quantities := '12345678901234567890123456789012345678901234567890"
			"1234567890';"
"	i_prices := '12345678901234567890123456789012345678901234567890"
			"1234567890';"
"	ol_amounts := '12345678901234567890123456789012345678901234567890"
			"1234567890';"
" 	bg := 'GGGGGGGGGGGGGGG';"
"	total := 0;"
""
"	SELECT C_DISCOUNT, C_LAST, C_CREDIT INTO c_discount, c_last, c_credit"
"		FROM CUSTOMER"
"		WHERE C_W_ID = c_w_id AND C_D_ID = c_d_id AND C_ID = c_id;"
""
"	OPEN district_cursor;"
""
"	FETCH district_cursor INTO o_id, d_tax;"
""
"	UPDATE DISTRICT SET D_NEXT_O_ID = o_id + 1"
"		WHERE CURRENT OF district_cursor;"
""
"	CLOSE district_cursor;"
""
""
	;
	str3 =

"	o_ol_count := LENGTH(ol_quantities);"
""
"	/* PRINTF('C-WAREHOUSE id ', BINARY_TO_NUMBER(c_w_id),"
"		' C-district id ', c_d_id,"
"		' order id ', o_id, ' linecount ', o_ol_count); */"
""
" 	FOR i IN 0 .. (o_ol_count - 1) LOOP"
""
"		ol_i_id := SUBSTR(ol_i_ids, 3 * i, 3);"
"		ol_supply_w_id := SUBSTR(ol_supply_w_ids, 2 * i, 2);"
"		ol_quantity := BINARY_TO_NUMBER(SUBSTR(ol_quantities, i, 1));"
""
"		/* PRINTF('ol_i_id ', BINARY_TO_NUMBER(ol_i_id),"
"			' ol_supply_w_id ', BINARY_TO_NUMBER(ol_supply_w_id),"
"			' ol_quantity ', ol_quantity); */"
"" 
"		SELECT I_PRICE, I_NAME, I_DATA INTO i_price, i_name, i_data"
"			FROM ITEM"
"			WHERE I_ID = ol_i_id"
"			CONSISTENT READ;"
""
"		IF (SQL % NOTFOUND) THEN"
"			/* PRINTF('Rolling back; item not found: ',"
"					BINARY_TO_NUMBER(ol_i_id)); */"
"			ROLLBACK WORK;"
"			o_ol_count := 0;"
""
"			RETURN;"
"		END IF;"
""
"		OPEN stock_cursor;"
""
"		FETCH stock_cursor INTO s_quantity, s_data,"
"					s_dist_01, s_dist_02, s_dist_03,"
"					s_dist_04, s_dist_05, s_dist_06,"
"					s_dist_07, s_dist_08, s_dist_09,"
"					s_dist_10;"
""
"		/* PRINTF('Stock quantity ', s_quantity); */"
""
"		IF (s_quantity >= ol_quantity + 10) THEN"
"			s_quantity := s_quantity - ol_quantity;"
"		ELSE"
"			s_quantity := (s_quantity + 91) - ol_quantity;"
"		END IF;"
""
"		UPDATE STOCK SET S_QUANTITY = s_quantity,"
"				S_YTD = S_YTD + ol_quantity,"
"				S_ORDER_CNT = S_ORDER_CNT + 1"
 "			WHERE CURRENT OF stock_cursor;"
""
"		IF (ol_supply_w_id <> c_w_id) THEN"
""
"			o_all_local := '0';"
"			PRINTF('Remote order ',"
"				BINARY_TO_NUMBER(ol_supply_w_id), ' ',"
"				BINARY_TO_NUMBER(c_w_id));"
""
"			UPDATE STOCK SET S_REMOTE_CNT = S_REMOTE_CNT + 1"
"				WHERE CURRENT OF stock_cursor;"
"		END IF;"
""
"		CLOSE stock_cursor;"
""
"		IF ((INSTR(i_data, 'ORIGINAL') > 0)"
"				OR (INSTR(s_data, 'ORIGINAL') > 0)) THEN"
"			REPLSTR(bg, 'B', i, 1);"
"		END IF;"
""
"		ol_amount := ol_quantity * i_price;"
""
"		total := total + ol_amount;"
	;
	str4 =
"		IF (c_d_id = '0') THEN"
"			ol_dist_info := s_dist_01;"
"		ELSIF (c_d_id = '1') THEN"
"			ol_dist_info := s_dist_02;"
"		ELSIF (c_d_id = '2') THEN"
"			ol_dist_info := s_dist_03;"
"		ELSIF (c_d_id = '3') THEN"
"			ol_dist_info := s_dist_04;"
"		ELSIF (c_d_id = '4') THEN"
"			ol_dist_info := s_dist_05;"
"		ELSIF (c_d_id = '5') THEN"
"			ol_dist_info := s_dist_06;"
"		ELSIF (c_d_id = '6') THEN"
"			ol_dist_info := s_dist_07;"
"		ELSIF (c_d_id = '7') THEN"
"			ol_dist_info := s_dist_08;"
"		ELSIF (c_d_id = '8') THEN"
"			ol_dist_info := s_dist_09;"
"		ELSIF (c_d_id = '9') THEN"
"			ol_dist_info := s_dist_10;"
"		END IF;"
""
"		INSERT INTO ORDER_LINE VALUES (o_id, c_d_id, c_w_id,"
"					TO_BINARY(i + 1, 1), ol_i_id,"
"					ol_supply_w_id, NULL, ol_quantity,"
"					ol_amount, ol_dist_info);"
""
"		REPLSTR(i_names, i_name, i * 24, LENGTH(i_name));"
"		REPLSTR(s_quantities, TO_BINARY(s_quantity, 4), i * 4, 4);"
"		REPLSTR(i_prices, TO_BINARY(i_price, 4), i * 4, 4);"
"		REPLSTR(ol_amounts, TO_BINARY(ol_amount, 4), i * 4, 4);"
""
"		/* PRINTF('i_name ', i_name, ' s_quantity ', s_quantity,"
"			' i_price ', i_price, ' ol_amount ', ol_amount); */"
"	END LOOP;"
""
"	SELECT W_TAX INTO w_tax"
"		FROM WAREHOUSE"
"		WHERE W_ID = c_w_id;"
""
"	total := (((total * (10000 + w_tax + d_tax)) / 10000)"
"			  * (10000 - c_discount)) / 10000;"
""
"	o_entry_d := SYSDATE();"
""
"	INSERT INTO ORDERS VALUES (o_id, c_d_id, c_w_id, c_id, o_entry_d,"
"					NULL, o_ol_count, o_all_local);"
"	INSERT INTO NEW_ORDER VALUES (o_id, c_d_id, c_w_id);"
""
"	/* PRINTF('Inserted order lines:');"
"	ROW_PRINTF"
"		SELECT * FROM ORDER_LINE WHERE OL_O_ID = o_id AND"
"						OL_D_ID = c_d_id"
"						AND OL_W_ID = c_w_id; */"
" 	COMMIT WORK;"
"	END LOOP;"
" 	END;"
	;

	str5 = ut_str_catenate(str1, str2);
	str6 = ut_str_catenate(str3, str4);

	new_order_str = ut_str_catenate(str5, str6);

	/*-----------------------------------------------------------*/

	str1 =
	
"	PROCEDURE PAYMENT (c_w_id IN CHAR) IS"
""
" 	i INT;"
" 	n_items INT;"
" 	n_warehouses INT;"
" 	n_districts INT;"
"	n_customers INT;"
"	w_id CHAR;"
"	w_street_1 CHAR;"
"	w_street_2 CHAR;"
"	w_city CHAR;"
"	w_state CHAR;"
"	w_zip CHAR;"
"	w_name CHAR;"
"	d_id CHAR;"
"	d_street_1 CHAR;"
"	d_street_2 CHAR;"
"	d_city CHAR;"
"	d_state CHAR;"
"	d_zip CHAR;"
"	d_name CHAR;"
"	c_d_id CHAR;"
"	c_street_1 CHAR;"
"	c_street_2 CHAR;"
"	c_city CHAR;"
"	c_state CHAR;"
"	c_zip CHAR;"
"	c_id CHAR;"
"	c_last CHAR;"
"	c_first CHAR;"
"	c_middle CHAR;"
"	c_phone CHAR;"
"	c_credit CHAR;"
"	c_credit_lim INT;"
"	c_discount INT;"
"	c_balance INT;"
"	c_since INT;"
"	c_data CHAR;"
"	byname INT;"
"	namecnt INT;"
"	amount INT;"
"	h_data CHAR;"
"	h_date INT;"
"	c_more_data CHAR;"
"	more_len INT;"
"	data_len INT;"
""
"	DECLARE CURSOR warehouse_cursor IS"
"		SELECT W_STREET_1, W_STREET_2, W_CITY, W_STATE, W_ZIP, W_NAME"
"			FROM WAREHOUSE"
"			WHERE W_ID = w_id"
"			FOR UPDATE;"
""
"	DECLARE CURSOR district_cursor IS"
"		SELECT D_STREET_1, D_STREET_2, D_CITY, D_STATE, D_ZIP, D_NAME"
"			FROM DISTRICT"
"			WHERE D_W_ID = w_id AND D_ID = d_id"
"			FOR UPDATE;"
""
"	DECLARE CURSOR customer_by_name_cursor IS"
"		SELECT C_ID"
"			FROM CUSTOMER"
"			WHERE C_W_ID = c_w_id AND C_D_ID = c_d_id"
"				AND C_LAST = c_last"
"			ORDER BY C_FIRST ASC;"
""
"	DECLARE CURSOR customer_cursor IS"
"		SELECT C_FIRST, C_MIDDLE, C_LAST, C_STREET_1, C_STREET_2,"
"				C_CITY, C_STATE, C_ZIP, C_PHONE, C_CREDIT,"
"				C_CREDIT_LIM, C_DISCOUNT, C_BALANCE,"
"				C_SINCE"
"			FROM CUSTOMER"
"			WHERE C_W_ID = c_w_id AND C_D_ID = c_d_id"
"							AND C_ID = c_id"
"			FOR UPDATE;"
	;

	str2 =
	
"	BEGIN"
""
" 	n_items := 200;"
" 	n_warehouses := 1;"
" 	n_districts := 10;"
" 	n_customers := 20;"
""
"	byname := RND(1, 100);"
"	amount := RND(1, 1000);"
"	h_date := SYSDATE();"
"	w_id := c_w_id;"
"	d_id := TO_BINARY(47 + RND(1, n_districts), 1);"
"	c_d_id := TO_BINARY(47 + RND(1, n_districts), 1);"
""
"	IF (byname <= 60) THEN"
"		c_last := CONCAT('NAME', TO_CHAR(RND(1, n_customers) / 3));"
""
"		SELECT COUNT(*) INTO namecnt"
"			FROM CUSTOMER"
"			WHERE C_W_ID = c_w_id AND C_D_ID = c_d_id"
"							AND C_LAST = c_last;"
"		/* PRINTF('Payment trx: Customer name ', c_last,"
"						' namecount ', namecnt); */"
"		OPEN customer_by_name_cursor;"
""
"		FOR i IN 1 .. (namecnt + 1) / 2 LOOP"
"			FETCH customer_by_name_cursor INTO c_id;"
"		END LOOP;"
"		/* ASSERT(NOT (customer_by_name_cursor % NOTFOUND)); */"
"		"
"		CLOSE customer_by_name_cursor;"
"	ELSE"
"		c_id := TO_BINARY(RND(1, n_customers), 3);"
"	END IF;"

	;
	str3 =
""
"	/* PRINTF('Payment for customer ', BINARY_TO_NUMBER(c_w_id), ' ',"
"				c_d_id, ' ', BINARY_TO_NUMBER(c_id)); */"
"	OPEN customer_cursor;"
""
"	FETCH customer_cursor INTO c_first, c_middle, c_last, c_street_1,"
"					c_street_2, c_city, c_state, c_zip,"
"					c_phone, c_credit, c_credit_lim,"
"					c_discount, c_balance, c_since;"
"	c_balance := c_balance - amount;"
""
"	OPEN district_cursor;"
""
"	FETCH district_cursor INTO d_street_1, d_street_2, d_city, d_state,"
"							d_zip, d_name;"
"	UPDATE DISTRICT SET D_YTD = D_YTD + amount"
"		WHERE CURRENT OF district_cursor;"
""
"	CLOSE district_cursor;"
""
"	OPEN warehouse_cursor;"
""
"	FETCH warehouse_cursor INTO w_street_1, w_street_2, w_city, w_state,"
"							w_zip, w_name;"
"	UPDATE WAREHOUSE SET W_YTD = W_YTD + amount"
"		WHERE CURRENT OF warehouse_cursor;"
""
"	CLOSE warehouse_cursor;"
""
"	h_data := CONCAT(w_name, '    ', d_name);"
"	"
"	IF (c_credit = 'BC') THEN"
"		/* PRINTF('Bad customer pays'); */"
""
"		SELECT C_DATA INTO c_data"
"			FROM CUSTOMER"
"			WHERE C_W_ID = c_w_id AND C_D_ID = c_d_id"
"							AND C_ID = c_id;"
"		c_more_data := CONCAT("
"				' ', TO_CHAR(BINARY_TO_NUMBER(c_id)),"
"				' ', c_d_id,"
"				' ', TO_CHAR(BINARY_TO_NUMBER(c_w_id)),"
"				' ', d_id,"
"				' ', TO_CHAR(BINARY_TO_NUMBER(w_id)),"
"				TO_CHAR(amount),"
"				TO_CHAR(h_date),"
"				' ', h_data);"
""
"		more_len := LENGTH(c_more_data);"
"		data_len := LENGTH(c_data);"
"		"
"		IF (more_len + data_len > 500) THEN"
"			data_len := 500 - more_len;"
"		END IF;"
"		"
"		c_data := CONCAT(c_more_data, SUBSTR(c_data, 0, data_len));"
"						"
"		UPDATE CUSTOMER SET C_BALANCE = c_balance,"
"				C_PAYMENT_CNT = C_PAYMENT_CNT + 1,"
"				C_YTD_PAYMENT = C_YTD_PAYMENT + amount,"
"				C_DATA = c_data"
"			WHERE CURRENT OF customer_cursor;"
"	ELSE"
"		UPDATE CUSTOMER SET C_BALANCE = c_balance,"
"				C_PAYMENT_CNT = C_PAYMENT_CNT + 1,"
"				C_YTD_PAYMENT = C_YTD_PAYMENT + amount"
"			WHERE CURRENT OF customer_cursor;"
"	END IF;"
""
"	CLOSE customer_cursor;"
"	"
"	INSERT INTO HISTORY VALUES (c_d_id, c_w_id, c_id, d_id, w_id,"
"						h_date, amount, h_data);"
"	COMMIT WORK;"
""
"	END;"

	;

	str4 = ut_str_catenate(str1, str2);
	payment_str = ut_str_catenate(str4, str3);

	/*-----------------------------------------------------------*/

	str1 =
	
"	PROCEDURE ORDER_STATUS (c_w_id IN CHAR) IS"
""
" 	i INT;"
" 	n_items INT;"
" 	n_warehouses INT;"
" 	n_districts INT;"
"	n_customers INT;"
"	d_id CHAR;"
"	namecnt INT;"
"	c_d_id CHAR;"
"	c_id CHAR;"
"	c_last CHAR;"
"	c_first CHAR;"
"	c_middle CHAR;"
"	c_balance INT;"
"	byname INT;"
"	o_id INT;"
"	o_carrier_id CHAR;"
"	o_entry_d INT;"
"	ol_i_id CHAR;"
"	ol_supply_w_id CHAR;"
"	ol_quantity INT;"
"	ol_amount INT;"
"	ol_delivery_d INT;"
""
"	DECLARE CURSOR orders_cursor IS"
"		SELECT O_ID, O_CARRIER_ID, O_ENTRY_D"
"			FROM ORDERS"
"			WHERE O_W_ID = c_w_id AND O_D_ID = c_d_id"
"							AND O_C_ID = c_id"
"			ORDER BY O_ID DESC;"
""
"	DECLARE CURSOR order_line_cursor IS"
"		SELECT OL_I_ID, OL_SUPPLY_W_ID, OL_QUANTITY, OL_AMOUNT,"
"							OL_DELIVERY_D"
"			FROM ORDER_LINE"
"			WHERE OL_W_ID = c_w_id AND OL_D_ID = c_d_id"
"							AND OL_O_ID = o_id;"
"	DECLARE CURSOR customer_by_name_cursor IS"
"		SELECT C_ID"
"			FROM CUSTOMER"
"			WHERE C_W_ID = c_w_id AND C_D_ID = c_d_id"
"				AND C_LAST = c_last"
"			ORDER BY C_FIRST ASC;"
"	BEGIN"
""
" 	n_items := 200;"
" 	n_warehouses := 1;"
" 	n_districts := 10;"
" 	n_customers := 20;"
""
"	byname := RND(1, 100);"
""
	;

	str2 =

"	IF (byname <= 60) THEN"
"		d_id := TO_BINARY(47 + RND(1, n_districts), 1);	"
""
"		c_d_id := d_id;"
""
"		c_last := CONCAT('NAME', TO_CHAR(RND(1, n_customers) / 3));"
""
"		SELECT COUNT(*) INTO namecnt"
"			FROM CUSTOMER"
"			WHERE C_W_ID = c_w_id AND C_D_ID = c_d_id"
"							AND C_LAST = c_last;"
"		OPEN customer_by_name_cursor;"
""
"		/* PRINTF('Order status trx: Customer name ', c_last,"
"						' namecount ', namecnt); */"
"		FOR i IN 1 .. (namecnt + 1) / 2 LOOP"
"			FETCH customer_by_name_cursor INTO c_id;"
"		END LOOP;"
"		/* ASSERT(NOT (customer_by_name_cursor % NOTFOUND)); */"
""
"		CLOSE customer_by_name_cursor;"
"	ELSE"
"		c_d_id := TO_BINARY(47 + RND(1, n_districts), 1);"
"		c_id := TO_BINARY(RND(1, n_customers), 3);"
"	END IF;"
""
"	SELECT C_BALANCE, C_FIRST, C_MIDDLE, C_LAST INTO c_balance, c_first,"
"							c_middle, c_last"
"		FROM CUSTOMER"
"		WHERE C_W_ID = c_w_id AND C_D_ID = c_d_id AND C_ID = c_id;"
""
"	OPEN orders_cursor;"
""
"	FETCH orders_cursor INTO o_id, o_carrier_id, o_entry_d;"
""
"	IF (orders_cursor % NOTFOUND) THEN"
"		PRINTF('Order status trx: customer has no order');"
"		CLOSE orders_cursor;"
""
"		COMMIT WORK;"
""
"		RETURN;"
"	END IF;"
""
"	CLOSE orders_cursor;"
""
"	OPEN order_line_cursor;"
""
"	FOR i IN 0 .. 15 LOOP"
"		FETCH order_line_cursor INTO ol_i_id, ol_supply_w_id,"
"						ol_quantity, ol_amount,"
"						ol_delivery_d;"
""
"		IF (order_line_cursor % NOTFOUND) THEN"
"			CLOSE order_line_cursor;"
""
"			COMMIT WORK;"
""
"			RETURN;"
"		END IF;"
"	END LOOP;"
"	ASSERT(0 = 1);"
"	"
"	END;"
	;

	order_status_str = ut_str_catenate(str1, str2);
	/*-----------------------------------------------------------*/

	str1 =
	
"	PROCEDURE DELIVERY (w_id IN CHAR) IS"
""
" 	i INT;"
" 	n_items INT;"
" 	n_warehouses INT;"
" 	n_districts INT;"
"	n_customers INT;"
"	d_id CHAR;"
"	c_id CHAR;"
"	o_id INT;"
"	o_carrier_id INT;"
"	ol_delivery_d INT;"
"	ol_total INT;"
""
"	DECLARE CURSOR new_order_cursor IS"
"		SELECT NO_O_ID"
"			FROM NEW_ORDER"
"			WHERE NO_W_ID = w_id AND NO_D_ID = d_id"
"			ORDER BY NO_O_ID ASC;"
""
"	DECLARE CURSOR orders_cursor IS"
"		SELECT O_C_ID"
"			FROM ORDERS"
"			WHERE O_W_ID = w_id AND O_D_ID = d_id"
"							AND O_ID = o_id"
"			FOR UPDATE;"
"	BEGIN"
""
" 	n_items := 200;"
" 	n_warehouses := 1;"
" 	n_districts := 10;"
" 	n_customers := 20;"
""
"	o_carrier_id := RND(1, 10);"
"	ol_delivery_d := SYSDATE();"

	;

	str2 =

"	FOR i IN 1 .. n_districts LOOP"
""
"		d_id := TO_BINARY(47 + i, 1);"
""
"		OPEN new_order_cursor;"
""
"		FETCH new_order_cursor INTO o_id;"
""
"		IF (new_order_cursor % NOTFOUND) THEN"
"			/* PRINTF('No order to deliver'); */"
""
"			CLOSE new_order_cursor;"
"		ELSE"
"			CLOSE new_order_cursor;"
""
"			DELETE FROM NEW_ORDER"
"				WHERE NO_W_ID = w_id AND NO_D_ID = d_id"
"						AND NO_O_ID = o_id;"
"			OPEN orders_cursor;"
""
"			FETCH orders_cursor INTO c_id;"
""
"			UPDATE ORDERS SET O_CARRIER_ID = o_carrier_id"
"				WHERE CURRENT OF orders_cursor;"
""
"			CLOSE orders_cursor;"
""
"			UPDATE ORDER_LINE SET OL_DELIVERY_D = ol_delivery_d"
"				WHERE OL_W_ID = w_id AND OL_D_ID = d_id"
"						AND OL_O_ID = o_id;"
""
"			SELECT SUM(OL_AMOUNT) INTO ol_total"
"				FROM ORDER_LINE"
"				WHERE OL_W_ID = w_id AND OL_D_ID = d_id"
"						AND OL_O_ID = o_id;"
""
"			UPDATE CUSTOMER SET C_BALANCE = C_BALANCE - ol_total"
"				WHERE C_W_ID = w_id AND C_D_ID = d_id"
"						AND C_ID = c_id;"
"		END IF;"
"	END LOOP;"
"	COMMIT WORK;"
""
"	"
"	END;"
	;

	delivery_str = ut_str_catenate(str1, str2);

	/*-----------------------------------------------------------*/

	str =
	
"	PROCEDURE STOCK_LEVEL (w_id IN CHAR) IS"
""
" 	n_items INT;"
" 	n_warehouses INT;"
" 	n_districts INT;"
"	n_customers INT;"
"	d_id CHAR;"
"	o_id INT;"
"	stock_count INT;"
"	threshold INT;"
""
"	BEGIN"
""
" 	n_items := 200;"
" 	n_warehouses := 1;"
" 	n_districts := 10;"
" 	n_customers := 20;"
""
"	d_id := TO_BINARY(47 + 4, 1);"
""
"	threshold := RND(10, 20);"
""
"	SELECT D_NEXT_O_ID INTO o_id"
"		FROM DISTRICT"
"		WHERE D_W_ID = w_id AND D_ID = d_id;"
""
"	/* NOTE: COUNT(DISTINCT ...) not implemented yet: if we used a hash"
"	table, the DISTINCT operation should take at most 15 % more time */"
""
"	SELECT COUNT(*) INTO stock_count"
"		FROM ORDER_LINE, STOCK"
"		WHERE OL_W_ID = w_id AND OL_D_ID = d_id"
"			AND OL_O_ID >= o_id - 10 AND OL_O_ID < o_id"
"			AND S_W_ID = w_id AND S_I_ID = OL_I_ID"
"			AND S_QUANTITY < threshold"
"		CONSISTENT READ;"
"	/* PRINTF(stock_count, ' items under threshold ', threshold); */"
"	COMMIT WORK;"
""	
"	END;"
	;

	stock_level_str = str;

	/*-----------------------------------------------------------*/

	str =
	
"	PROCEDURE TPC_CONSISTENCY () IS"
""
" 	n_items INT;"
" 	n_warehouses INT;"
" 	n_districts INT;"
"	n_customers INT;"
"	n_orders INT;"
"	n_new_orders INT;"
"	n_order_lines INT;"
"	n_history INT;"
"	sum_order_quant INT;"
"	sum_stock_quant INT;"
"	n_delivered INT;"
"	n INT;"
"	n_new_order_lines INT;"
"	n_customers_d INT;"
"	n_regions INT;"
"	n_nations INT;"
"	n_suppliers INT;"
"	n_orders_d INT;"
"	n_lineitems INT;"
""
"	BEGIN"
""
"	PRINTF('TPC-C consistency check begins');"
""
"	SELECT COUNT(*) INTO n_warehouses"
"		FROM WAREHOUSE;"
"	SELECT COUNT(*) INTO n_items"
"		FROM ITEM;"
"	SELECT COUNT(*) INTO n_customers"
"		FROM CUSTOMER;"
"	SELECT COUNT(*) INTO n_districts"
"		FROM DISTRICT;"
"	SELECT COUNT(*) INTO n_orders"
"		FROM ORDERS;"	
"	SELECT COUNT(*) INTO n_new_orders"
"		FROM NEW_ORDER;"	
"	SELECT COUNT(*) INTO n_order_lines"
"		FROM ORDER_LINE;"	
"	SELECT COUNT(*) INTO n_history"
"		FROM HISTORY;"	
""
"	PRINTF('N warehouses ', n_warehouses);"
""
"	PRINTF('N items ', n_items, ' : ', n_items / n_warehouses,"
"							' per warehouse');"
"	PRINTF('N districts ', n_districts, ' : ', n_districts / n_warehouses,"
"							' per warehouse');"
"	PRINTF('N customers ', n_customers, ' : ', n_customers / n_districts,"
"							' per district');"
"	PRINTF('N orders ', n_orders, ' : ', n_orders / n_customers,"
"							' per customer');"
"	PRINTF('N new orders ', n_new_orders, ' : ',"
"				n_new_orders / n_customers, ' per customer');"
"	PRINTF('N order lines ', n_order_lines, ' : ',"
"				n_order_lines / n_orders, ' per order');"
"	PRINTF('N history ', n_history, ' : ',"
"				n_history / n_customers, ' per customer');"
"	SELECT COUNT(*) INTO n_delivered"
"		FROM ORDER_LINE"
"		WHERE OL_DELIVERY_D < NULL;"
""
"	PRINTF('N delivered order lines ', n_delivered);"
""
"	SELECT COUNT(*) INTO n_new_order_lines"
"		FROM NEW_ORDER, ORDER_LINE"
"		WHERE NO_O_ID = OL_O_ID AND NO_D_ID = OL_D_ID"
"						AND NO_W_ID = OL_W_ID;"
"	PRINTF('N new order lines ', n_new_order_lines);"
""
"	SELECT COUNT(*) INTO n"
"		FROM NEW_ORDER, ORDER_LINE"
"		WHERE NO_O_ID = OL_O_ID AND NO_D_ID = OL_D_ID"
"			AND NO_W_ID = OL_W_ID AND OL_DELIVERY_D < NULL;"
"	PRINTF('Assertion 1');"
"	ASSERT(n = 0);"
""
"	SELECT COUNT(*) INTO n"
"		FROM NEW_ORDER, ORDER_LINE"
"		WHERE NO_O_ID = OL_O_ID AND NO_D_ID = OL_D_ID"
"			AND NO_W_ID = OL_W_ID AND OL_DELIVERY_D = NULL;"
""				
"	PRINTF('Assertion 2');"
"	ASSERT(n = n_new_order_lines);"
"	PRINTF('Assertion 2B');"
"	ASSERT(n_delivered + n_new_order_lines = n_order_lines);"
""
"	PRINTF('Assertion 3');"
"	/* ASSERT(n_orders <= n_history); */"
"	PRINTF('Assertion 4');"
"	ASSERT(n_order_lines <= 15 * n_orders);"
"	PRINTF('Assertion 5');"
"	ASSERT(n_order_lines >= 5 * n_orders);"
"	PRINTF('Assertion 6');"
"	ASSERT(n_new_orders <= n_orders);"
""
"	SELECT SUM(OL_QUANTITY) INTO sum_order_quant"
"		FROM ORDER_LINE;"
"	SELECT SUM(S_QUANTITY) INTO sum_stock_quant"
"		FROM STOCK;"
"	PRINTF('Sum order quant ', sum_order_quant, ' sum stock quant ',"
"							sum_stock_quant);"
""
"	PRINTF('Assertion 7');"
"	ASSERT(((sum_stock_quant + sum_order_quant) / 91) * 91"
"				= sum_stock_quant + sum_order_quant);"
"	COMMIT WORK;"
"	PRINTF('TPC-C consistency check passed');"
""
"	PRINTF('TPC-D consistency check begins');"
""
"	SELECT COUNT(*) INTO n_customers_d"
"		FROM CUSTOMER_D"
"		CONSISTENT READ;"
"	SELECT COUNT(*) INTO n_nations"
"		FROM NATION"
"		CONSISTENT READ;"
"	SELECT COUNT(*) INTO n_regions"
"		FROM REGION"
"		CONSISTENT READ;"
"	SELECT COUNT(*) INTO n_suppliers"
"		FROM SUPPLIER"
"		CONSISTENT READ;"
"	SELECT COUNT(*) INTO n_orders_d"
"		FROM ORDERS_D"	
"		CONSISTENT READ;"
"	SELECT COUNT(*) INTO n_lineitems"
"		FROM LINEITEM"	
"		CONSISTENT READ;"
""
"	PRINTF('N customers TPC-D ', n_customers_d);"
""
"	PRINTF('N nations ', n_nations);"
"	PRINTF('N regions ', n_regions);"
""
"	PRINTF('N suppliers ', n_suppliers);"
"	PRINTF('N orders TPC-D ', n_orders_d);"
""
"	PRINTF('N lineitems ', n_lineitems, ' : ',"
"				n_lineitems / n_orders_d, ' per order');"
"	SELECT COUNT(*) INTO n"
"		FROM NATION, NATION_2"
"		WHERE N_NAME = N2_NAME"
"		CONSISTENT READ;"
""
"	PRINTF('Assertion D1');"
"	ASSERT(n = n_nations);"
""
"	SELECT COUNT(*) INTO n"
"		FROM NATION, REGION"
"		WHERE N_REGIONKEY = R_REGIONKEY"
"		CONSISTENT READ;"
""
"	PRINTF('Assertion D2');"
"	ASSERT(n = n_nations);"
""
"	SELECT COUNT(*) INTO n"
"		FROM ORDERS_D, CUSTOMER_D"
"		WHERE O_CUSTKEY = C_CUSTKEY"
"		CONSISTENT READ;"
""
"	PRINTF('Assertion D3');"
"	ASSERT(n = n_orders_d);"
""
"	SELECT COUNT(*) INTO n"
"		FROM LINEITEM, SUPPLIER"
"		WHERE L_SUPPKEY = S_SUPPKEY"
"		CONSISTENT READ;"
""
"	PRINTF('Assertion D4');"
"	ASSERT(n = n_lineitems);"
""
"	SELECT COUNT(*) INTO n"
"		FROM ORDERS_D"
"		WHERE O_ORDERDATE >= 0"
"			AND O_ORDERDATE <= 2500"
"		CONSISTENT READ;"
""
"	PRINTF('Assertion D5');"
"	ASSERT(n = n_orders_d);"
""
"	COMMIT WORK;"
"	PRINTF('TPC-D consistency check passed');"
""
"	END;"
	;

	consistency_str = str;

	/*-----------------------------------------------------------*/
	str =
	
"	PROCEDURE TPC_D_QUERY_5 (startday IN INT, endday IN INT) IS"
""
"	revenue INT;"
"	r_name CHAR;"
""
"	BEGIN"
""
"	r_name := CONCAT('Region', TO_CHAR(3), '               ');"
""
"	/* The last join to NATION_2 corresponds to calculating"
"	GROUP BY N_NAME in the original TPC-D query. It should take"
"	approximately the same amount of CPU time as GROUP BY. */"
""
"	SELECT SUM((L_EXTENDEDPRICE * (100 - L_DISCOUNT)) / 100)"
"					INTO revenue"
"		FROM REGION, ORDERS_D, CUSTOMER_D, NATION,"
"					LINEITEM, SUPPLIER, NATION_2"
"		WHERE R_NAME = r_name"
"			AND O_ORDERDATE >= startday"
"			AND O_ORDERDATE < endday"
"			AND O_CUSTKEY = C_CUSTKEY"
"			AND C_NATIONKEY = N_NATIONKEY"
"			AND N_REGIONKEY = R_REGIONKEY"
"			AND O_ORDERKEY = L_ORDERKEY"
"			AND L_SUPPKEY = S_SUPPKEY"
"			AND S_NATIONKEY = C_NATIONKEY"
"			AND N_NAME = N2_NAME"
"		CONSISTENT READ;"
""
"	PRINTF('Startdate ', startday, '; enddate ', endday,"
"						': revenue ', revenue);"
"	COMMIT WORK;"
""	
"	END;"
	;

	query_5_str = str;
	/*-----------------------------------------------------------*/
	str =
	
"	PROCEDURE ROLLBACK_QUERY () IS"
""
"	BEGIN"
""
"	ROLLBACK WORK;"
""	
"	END;"
	;

	rollback_str = str;
	/*-----------------------------------------------------------*/
	str =
	
"	PROCEDURE TEST_LOCK_WAIT () IS"
""
"	w_id CHAR;"
"	BEGIN"
""
"	w_id := TO_BINARY(1, 2);"
"	UPDATE WAREHOUSE SET W_YTD = W_YTD + 1 WHERE W_ID = w_id;"
""	
"	END;"
	;

	lock_wait_str = str;
	/*-----------------------------------------------------------*/
	str =
	
"	PROCEDURE TEST_IBUF () IS"
""
"	i INT;"
"	rnd INT;"
"	j INT;"
"	found INT;"
""
"	DECLARE CURSOR desc_cursor IS"
"		SELECT IB_A"
"		FROM IBUF_TEST"
"			WHERE IB_A >= rnd AND IB_A < rnd + 50"
"			ORDER BY IB_A DESC;"
""
"	BEGIN"
""
"	PRINTF('Ibuf QUERY starts!!!!!!');"
"	rnd := RND(1, 1000);"
""
"	FOR i IN 1 .. 50 LOOP"
"		INSERT INTO IBUF_TEST VALUES (rnd + i,"
"						RND_STR(RND(1, 2000)));"
"	END LOOP;"
"	IF (RND(1, 100) < 30) THEN"
"		PRINTF('Ibuf rolling back ---!!!');"
"		ROLLBACK WORK;"
"	END IF;"
""
""
"	IF (RND(1, 100) < 101) THEN"
"		rnd := RND(1, 1000);"
"		DELETE FROM IBUF_TEST WHERE IB_A >= rnd "
"						AND IB_A <= rnd + 50;"
"	END IF;"
""
"	rnd := RND(1, 1000);"
"	SELECT COUNT(*) INTO j"
"		FROM IBUF_TEST"
"		WHERE IB_A >= rnd AND IB_A < rnd + 50;"
""
"	PRINTF('Count: ', j);"
""
"	rnd := RND(1, 1000);"
"	UPDATE IBUF_TEST"
"		SET IB_B = RND_STR(RND(1, 2000))"
"		WHERE IB_A >= rnd AND IB_A < rnd + 50;"
""
"	OPEN desc_cursor;"
""
"	rnd := RND(1, 1000);"
"	found := 1;"
"	WHILE (found > 0) LOOP"
""
"		FETCH desc_cursor INTO j;"
""
"		IF (desc_cursor % NOTFOUND) THEN"
"			found := 0;"
"		END IF;"
"	END LOOP;"
""
"	CLOSE desc_cursor;"
""
"	IF (RND(1, 100) < 30) THEN"
"		PRINTF('Ibuf rolling back!!!');"
"		ROLLBACK WORK;"
"	ELSE"
"		COMMIT WORK;"
"	END IF;"
""
"	PRINTF('Ibuf QUERY ends!!!!!!');"
"	END;"
	;

	ibuf_test_str = str;
	/*-----------------------------------------------------------*/
	str =
	
"	PROCEDURE TEST_GROUP_COMMIT (w_id IN CHAR) IS"
""
"	i INT;"
""
"	BEGIN"
""
"	FOR i IN 1 .. 200 LOOP"
"		UPDATE WAREHOUSE SET W_YTD = W_YTD + 1 WHERE W_ID = w_id;"
"		COMMIT WORK;"
"	END LOOP;"
"	END;"
	;

	test_group_commit_str = str;
	/*-----------------------------------------------------------*/
	str =
	
"	PROCEDURE TEST_SINGLE_ROW_SELECT ("
"				i_id IN CHAR,"
"				i_name OUT CHAR) IS"
"	BEGIN"
"	SELECT I_NAME INTO i_name"
"	FROM ITEM"
"		WHERE I_ID = i_id"
"		CONSISTENT READ;"
"	END;"
	;

	test_single_row_select_str = str;
	/*-----------------------------------------------------------*/
	str =
	
"	PROCEDURE JOIN_TEST () IS"
""
" 	n_rows INT;"
"	i INT;"
""
"	BEGIN"
""
"	FOR i IN 0 .. 0 LOOP"
"		SELECT COUNT(*) INTO n_rows"
"		FROM JTEST1, JTEST2"
"			WHERE JT2_A = JT1_B"
"			CONSISTENT READ;"
"		PRINTF(n_rows);"
""
"		COMMIT WORK;"
"	END LOOP;"
""
"	END;"
	;

	join_test_str = str;

	/*-----------------------------------------------------------*/
	str =
	
"	PROCEDURE TEST_ERRORS (switch IN CHAR) IS"
""
"	count	INT;"
"	val	INT;"
""
"	BEGIN"
""
"	IF (switch = '01') THEN"
"		/* Test duplicate key error: run this first */"
"		ROW_PRINTF SELECT * FROM JTEST1;"
"		PRINTF('To insert first');"
"		INSERT INTO JTEST1 VALUES (1, 1);"
"		PRINTF('To insert second');"
"		INSERT INTO JTEST1 VALUES (2, 2);"
"	END IF;"
""
"	IF (switch = '02') THEN"
"		/* Test duplicate key error: run this second */"
"		ROW_PRINTF SELECT * FROM JTEST1;"
"		PRINTF('To insert third');"
"		INSERT INTO JTEST1 VALUES (3, 3);"
"		ROW_PRINTF SELECT * FROM JTEST1;"
"		PRINTF('To insert fourth');"
"		INSERT INTO JTEST1 VALUES (1, 1);"
"	END IF;"
""
"	IF (switch = '03') THEN"
"		/* Test duplicate key error: run this third */"
"		ROW_PRINTF SELECT * FROM JTEST1;"
"		PRINTF('Testing assert');"
"		SELECT COUNT(*) INTO count FROM JTEST1;"
"		ASSERT(count = 2);"
"	END IF;"
""
"	IF (switch = '04') THEN"
"		/* Test duplicate key error: run this fourth */"
"		ROW_PRINTF SELECT * FROM JTEST1;"
"		PRINTF('Testing update');"
"		UPDATE JTEST1 SET JT1_A = 3 WHERE JT1_A = 2;" 
"		PRINTF('Testing update');"
"		UPDATE JTEST1 SET JT1_A = 1 WHERE JT1_A = 3;" 
"	END IF;"
""	
"	IF (switch = '05') THEN"
"		/* Test deadlock error: run this fifth in thread 1 */"
"		COMMIT WORK;"
"		PRINTF('Testing update in thread 1');"
"		UPDATE JTEST1 SET JT1_B = 3 WHERE JT1_A = 1;" 
"	END IF;"
""
"	IF (switch = '06') THEN"
"		/* Test deadlock error: run this sixth in thread 2 */"
"		PRINTF('Testing update in thread 2');"
"		UPDATE JTEST1 SET JT1_B = 10 WHERE JT1_A = 2;" 
"		PRINTF('Testing update in thread 2');"
"		UPDATE JTEST1 SET JT1_B = 11 WHERE JT1_A = 1;" 
"		PRINTF('Update in thread 2 completed');"
"		SELECT JT1_B INTO val FROM JTEST1 WHERE JT1_A = 1;"
"		ASSERT(val = 11);"
"		SELECT JT1_B INTO val FROM JTEST1 WHERE JT1_A = 2;"
"		ASSERT(val = 10);"		
"		COMMIT WORK;"
"	END IF;"
""
"	IF (switch = '07') THEN"
"		/* Test deadlock error: run this seventh in thread 1 */"
"		PRINTF('Testing update in thread 1: deadlock');"
"		UPDATE JTEST1 SET JT1_B = 4 WHERE JT1_A = 2;" 
"	END IF;"
""	
"	IF (switch = '08') THEN"
"		/* Test deadlock error: run this eighth in thread 1 */"
"		PRINTF('Testing update in thread 1: commit');"
"		SELECT JT1_B INTO val FROM JTEST1 WHERE JT1_A = 1;"
"		ASSERT(val = 3);"
"		COMMIT WORK;"
"	END IF;"
""	
"	END;"
	;

	test_errors_str = str;
	/*-----------------------------------------------------------*/
	ret = SQLAllocEnv(&env);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLAllocConnect(env, &conn);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLAllocStmt(conn, &stat);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLAllocStmt(conn, &create_query);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLAllocStmt(conn, &populate_query);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLConnect(conn, (UCHAR*)cli_srv_endpoint_name,
				(SWORD)ut_strlen(cli_srv_endpoint_name),
				(UCHAR*)"use21", 5, (UCHAR*)"password", 8);
	ut_a(ret == SQL_SUCCESS);

	printf("Connection established\n");

	/*-----------------------------------------------------------*/
	ret = SQLPrepare(stat, (UCHAR*)create_str, ut_strlen(create_str));

	ut_a(ret == SQL_SUCCESS);

	ret = SQLExecute(stat);

	ut_a(ret == SQL_SUCCESS);

	str = "{CREATE_TABLES()}";
	
	ret = SQLPrepare(create_query, (UCHAR*)str, ut_strlen(str));

	ut_a(ret == SQL_SUCCESS);

	ret = SQLExecute(create_query);

	ut_a(ret == SQL_SUCCESS);

	/*-----------------------------------------------------------*/
	ret = SQLPrepare(stat, (UCHAR*)populate_str, ut_strlen(populate_str));

	ut_a(ret == SQL_SUCCESS);

	ret = SQLExecute(stat);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLPrepare(stat, (UCHAR*)lock_wait_str,
						ut_strlen(lock_wait_str));
	ut_a(ret == SQL_SUCCESS);

	ret = SQLExecute(stat);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLPrepare(stat, (UCHAR*)commit_str,
						ut_strlen(commit_str));
	ut_a(ret == SQL_SUCCESS);

	ret = SQLExecute(stat);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLPrepare(stat, (UCHAR*)print_str,
						ut_strlen(print_str));
	ut_a(ret == SQL_SUCCESS);

	ret = SQLExecute(stat);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLPrepare(stat, (UCHAR*)new_order_str,
						ut_strlen(new_order_str));
	ut_a(ret == SQL_SUCCESS);

	ret = SQLExecute(stat);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLPrepare(stat, (UCHAR*)payment_str,
						ut_strlen(payment_str));
	ut_a(ret == SQL_SUCCESS);

	ret = SQLExecute(stat);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLPrepare(stat, (UCHAR*)order_status_str,
						ut_strlen(order_status_str));
	ut_a(ret == SQL_SUCCESS);

	ret = SQLExecute(stat);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLPrepare(stat, (UCHAR*)delivery_str,
						ut_strlen(delivery_str));
	ut_a(ret == SQL_SUCCESS);

	ret = SQLExecute(stat);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLPrepare(stat, (UCHAR*)stock_level_str,
						ut_strlen(stock_level_str));
	ut_a(ret == SQL_SUCCESS);

	ret = SQLExecute(stat);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLPrepare(stat, (UCHAR*)query_5_str,
						ut_strlen(query_5_str));
	ut_a(ret == SQL_SUCCESS);

	ret = SQLExecute(stat);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLPrepare(stat, (UCHAR*)consistency_str,
						ut_strlen(consistency_str));
	ut_a(ret == SQL_SUCCESS);

	ret = SQLExecute(stat);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLPrepare(stat, (UCHAR*)rollback_str, ut_strlen(rollback_str));

	ut_a(ret == SQL_SUCCESS);

	ret = SQLExecute(stat);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLPrepare(stat, (UCHAR*)join_test_str,
						ut_strlen(join_test_str));
	ut_a(ret == SQL_SUCCESS);

	ret = SQLExecute(stat);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLPrepare(stat, (UCHAR*)test_errors_str,
						ut_strlen(test_errors_str));
	ut_a(ret == SQL_SUCCESS);

	ret = SQLExecute(stat);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLPrepare(stat, (UCHAR*)test_single_row_select_str,
				ut_strlen(test_single_row_select_str));
	ut_a(ret == SQL_SUCCESS);

	ret = SQLExecute(stat);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLPrepare(stat, (UCHAR*)test_group_commit_str,
					ut_strlen(test_group_commit_str));
	ut_a(ret == SQL_SUCCESS);

	ret = SQLExecute(stat);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLPrepare(stat, (UCHAR*)ibuf_test_str,
					ut_strlen(ibuf_test_str));
	ut_a(ret == SQL_SUCCESS);

	ret = SQLExecute(stat);

	ut_a(ret == SQL_SUCCESS);

	str = "{POPULATE_TABLES(?, ?)}";
	
	ret = SQLPrepare(populate_query, (UCHAR*)str, ut_strlen(str));

	ut_a(ret == SQL_SUCCESS);

	ret = SQLBindParameter(populate_query, 1, SQL_PARAM_INPUT,
				SQL_C_LONG, SQL_INTEGER, 0, 0,
				(byte*)&n_warehouses_buf,
				4, &n_warehouses_len);
	ut_a(ret == SQL_SUCCESS);

	n_warehouses_buf = n_warehouses;
	n_warehouses_len = 4;

	ret = SQLBindParameter(populate_query, 2, SQL_PARAM_INPUT,
				SQL_C_LONG, SQL_INTEGER, 0, 0,
				(byte*)&n_customers_d_buf,
				4, &n_customers_d_len);
	ut_a(ret == SQL_SUCCESS);

	n_customers_d_buf = n_customers_d;
	n_customers_d_len = 4;

	ret = SQLExecute(populate_query);

	ut_a(ret == SQL_SUCCESS);

	/*-----------------------------------------------------------*/		
	printf("TPC-C test database initialized\n");

	return(0);
}

/*********************************************************************
Iterates an SQL query until it returns SQL_SUCCESS. If it returns other
value, rolls back the trx, prints an error message, and tries again. */

void
execute_until_success(
/*==================*/
	HSTMT	query,		/* in: query */
	HSTMT	rollback_query)	/* in: trx rollback query to run if error */
{
	RETCODE	ret;
	UCHAR	sql_state[6];
	SDWORD	native_error;
	UCHAR	error_msg[512];
	SWORD	error_msg_max	= 512;
	SWORD	error_msg_len;

	for (;;) {
		ret = SQLExecute(query);

		if (ret != SQL_SUCCESS) {
			ut_a(ret == SQL_ERROR);
			
			ret = SQLError(SQL_NULL_HENV, SQL_NULL_HDBC, query,
					sql_state, &native_error, error_msg,
					error_msg_max, &error_msg_len);

			ut_a(ret == SQL_SUCCESS);

			printf("%s\n", error_msg);

			/* Roll back to release trx locks, and try again */

			ret = SQLExecute(rollback_query);
			ut_a(ret == SQL_SUCCESS);

			os_thread_sleep(ut_rnd_gen_ulint() / 1000);
		} else {

			return;
		}
	}
}

/*********************************************************************
Test for TPC-C. */

ulint
test_client(
/*=========*/
	void*	arg)	/* in: user name as a null-terminated string */
{
	ulint	n_customers = 20;
	ulint	n_items = 200;
	ulint	n_lines;
	bool	put_invalid_item;
	HENV	env;
	HDBC	conn;
	RETCODE	ret;
	HSTMT	commit_query;
	HSTMT	new_order_query;
	HSTMT	payment_query;
	HSTMT	order_status_query;
	HSTMT	delivery_query;
	HSTMT	stock_level_query;
	HSTMT	print_query;
	HSTMT	lock_wait_query;
	HSTMT	join_test_query;
	HSTMT	test_group_commit_query;
	HSTMT	rollback_query;
	HSTMT	ibuf_query;
	ulint	tm, oldtm;
	char*	str;
	byte	c_w_id_buf[2];
	byte	c_d_id_buf[1];
	byte	c_id_buf[3];
	byte	ol_supply_w_ids_buf[30];
	byte	ol_i_ids_buf[45];
	byte	ol_quantities_buf[15];
	byte	c_last_buf[51];
	byte	c_credit_buf[3];
	ulint	c_discount_buf;
	ulint	w_tax_buf;
	ulint	d_tax_buf;
	ulint	o_ol_count_buf;
	ulint	o_id_buf;
	ulint	o_entry_d_buf;
	ulint	total_buf;
	byte	i_names_buf[361];
	byte	s_quantities_buf[60];
	byte	bg_buf[16];
	byte	i_prices_buf[60];
	byte	ol_amounts_buf[60];
	SDWORD	c_w_id_len;
	SDWORD	c_d_id_len;
	SDWORD	c_id_len;
	SDWORD	ol_supply_w_ids_len;
	SDWORD	ol_i_ids_len;
	SDWORD	ol_quantities_len;
	SDWORD	c_last_len;
	SDWORD	c_credit_len;
	SDWORD	c_discount_len;
	SDWORD	w_tax_len;
	SDWORD	d_tax_len;
	SDWORD	o_ol_count_len;
	SDWORD	o_id_len;
	SDWORD	o_entry_d_len;
	SDWORD	total_len;
	SDWORD	i_names_len;
	SDWORD	s_quantities_len;
	SDWORD	bg_len;
	SDWORD	i_prices_len;
	SDWORD	ol_amounts_len;
	ulint	i;
	ulint	k;
	ulint	t;
	
	printf("Client thread %s\n", (UCHAR*)arg);

	ret = SQLAllocEnv(&env);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLAllocConnect(env, &conn);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLAllocStmt(conn, &new_order_query);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLAllocStmt(conn, &payment_query);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLAllocStmt(conn, &order_status_query);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLAllocStmt(conn, &delivery_query);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLAllocStmt(conn, &stock_level_query);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLAllocStmt(conn, &print_query);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLAllocStmt(conn, &commit_query);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLAllocStmt(conn, &lock_wait_query);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLAllocStmt(conn, &join_test_query);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLAllocStmt(conn, &test_group_commit_query);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLAllocStmt(conn, &rollback_query);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLAllocStmt(conn, &ibuf_query);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLConnect(conn, (UCHAR*)cli_srv_endpoint_name,
				(SWORD)ut_strlen(cli_srv_endpoint_name),
			(UCHAR*)arg, (SWORD)ut_strlen((char*)arg),
						(UCHAR*)"password", 8);
	ut_a(ret == SQL_SUCCESS);

	printf("Connection established\n");

	/*-----------------------------------------------------------*/		
	str =
	"{NEW_ORDER(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?,"
							" ?, ?, ?, ?)}";
							
	ret = SQLPrepare(new_order_query, (UCHAR*)str, ut_strlen(str));

	ut_a(ret == SQL_SUCCESS);

	ret = SQLBindParameter(new_order_query, 1, SQL_PARAM_INPUT,
				SQL_C_CHAR, SQL_CHAR, 0, 0, c_w_id_buf,
				2, &c_w_id_len);
	ut_a(ret == SQL_SUCCESS);

	ret = SQLBindParameter(new_order_query, 2, SQL_PARAM_INPUT,
				SQL_C_CHAR, SQL_CHAR, 0, 0, c_d_id_buf,
				1, &c_d_id_len);
	ut_a(ret == SQL_SUCCESS);

	c_d_id_len = 1;

	ret = SQLBindParameter(new_order_query, 3, SQL_PARAM_INPUT,
				SQL_C_CHAR, SQL_CHAR, 0, 0, c_id_buf,
				3, &c_id_len);
	ut_a(ret == SQL_SUCCESS);

	c_id_len = 3;

	ret = SQLBindParameter(new_order_query, 4, SQL_PARAM_INPUT,
				SQL_C_CHAR, SQL_CHAR, 0, 0, ol_supply_w_ids_buf,
				30, &ol_supply_w_ids_len);
	ut_a(ret == SQL_SUCCESS);

	ret = SQLBindParameter(new_order_query, 5, SQL_PARAM_INPUT,
				SQL_C_CHAR, SQL_CHAR, 0, 0, ol_i_ids_buf,
				45, &ol_i_ids_len);
	ut_a(ret == SQL_SUCCESS);

	ret = SQLBindParameter(new_order_query, 6, SQL_PARAM_INPUT,
				SQL_C_CHAR, SQL_CHAR, 0, 0, ol_quantities_buf,
				15, &ol_quantities_len);
	ut_a(ret == SQL_SUCCESS);

	ret = SQLBindParameter(new_order_query, 7, SQL_PARAM_OUTPUT,
				SQL_C_CHAR, SQL_CHAR, 0, 0, c_last_buf,
				50, &c_last_len);
	ut_a(ret == SQL_SUCCESS);

	ret = SQLBindParameter(new_order_query, 8, SQL_PARAM_OUTPUT,
				SQL_C_CHAR, SQL_CHAR, 0, 0,
				(byte*)&c_credit_buf,
				2, &c_credit_len);
	ut_a(ret == SQL_SUCCESS);
	c_credit_buf[2] = '\0';
	
	ret = SQLBindParameter(new_order_query, 9, SQL_PARAM_OUTPUT,
				SQL_C_LONG, SQL_INTEGER, 0, 0,
				(byte*)&c_discount_buf,
				4, &c_discount_len);
	ut_a(ret == SQL_SUCCESS);

	ret = SQLBindParameter(new_order_query, 10, SQL_PARAM_OUTPUT,
				SQL_C_LONG, SQL_INTEGER, 0, 0,
				(byte*)&w_tax_buf,
				4, &w_tax_len);
	ut_a(ret == SQL_SUCCESS);
	
	ret = SQLBindParameter(new_order_query, 11, SQL_PARAM_OUTPUT,
				SQL_C_LONG, SQL_INTEGER, 0, 0,
				(byte*)&d_tax_buf,
				4, &d_tax_len);
	ut_a(ret == SQL_SUCCESS);
	
	ret = SQLBindParameter(new_order_query, 12, SQL_PARAM_OUTPUT,
				SQL_C_LONG, SQL_INTEGER, 0, 0,
				(byte*)&o_ol_count_buf,
				4, &o_ol_count_len);
	ut_a(ret == SQL_SUCCESS);
	
	ret = SQLBindParameter(new_order_query, 13, SQL_PARAM_OUTPUT,
				SQL_C_LONG, SQL_INTEGER, 0, 0,
				(byte*)&o_id_buf,
				4, &o_id_len);
	ut_a(ret == SQL_SUCCESS);
	
	ret = SQLBindParameter(new_order_query, 14, SQL_PARAM_OUTPUT,
				SQL_C_LONG, SQL_INTEGER, 0, 0,
				(byte*)&o_entry_d_buf,
				4, &o_entry_d_len);
	ut_a(ret == SQL_SUCCESS);
	
	ret = SQLBindParameter(new_order_query, 15, SQL_PARAM_OUTPUT,
				SQL_C_LONG, SQL_INTEGER, 0, 0,
				(byte*)&total_buf,
				4, &total_len);
	ut_a(ret == SQL_SUCCESS);

	ret = SQLBindParameter(new_order_query, 16, SQL_PARAM_OUTPUT,
				SQL_C_CHAR, SQL_CHAR, 0, 0, i_names_buf,
				360, &i_names_len);
	ut_a(ret == SQL_SUCCESS);
	i_names_buf[360] = '\0';

	ret = SQLBindParameter(new_order_query, 17, SQL_PARAM_OUTPUT,
				SQL_C_CHAR, SQL_CHAR, 0, 0, s_quantities_buf,
				60, &s_quantities_len);
	ut_a(ret == SQL_SUCCESS);

	ret = SQLBindParameter(new_order_query, 18, SQL_PARAM_OUTPUT,
				SQL_C_CHAR, SQL_CHAR, 0, 0, bg_buf,
				15, &bg_len);
	ut_a(ret == SQL_SUCCESS);
	bg_buf[15] = '\0';

	ret = SQLBindParameter(new_order_query, 19, SQL_PARAM_OUTPUT,
				SQL_C_CHAR, SQL_CHAR, 0, 0, i_prices_buf,
				60, &i_prices_len);
	ut_a(ret == SQL_SUCCESS);

	ret = SQLBindParameter(new_order_query, 20, SQL_PARAM_OUTPUT,
				SQL_C_CHAR, SQL_CHAR, 0, 0, ol_amounts_buf,
				60, &ol_amounts_len);
	ut_a(ret == SQL_SUCCESS);
	
	c_w_id_len = 2;
	c_w_id_buf[1] = (byte)(2 * atoi((char*)arg + 4));
	c_w_id_buf[0] = (byte)(2 * (atoi((char*)arg + 4) / 256));

	k = atoi((char*)arg + 4);

	printf("Client thread %lu starts\n", k); 

	/*-----------------------------------------------------------*/		
	str = "{PAYMENT(?)}";
	
	ret = SQLPrepare(payment_query, (UCHAR*)str, ut_strlen(str));

	ut_a(ret == SQL_SUCCESS);

	ret = SQLBindParameter(payment_query, 1, SQL_PARAM_INPUT,
				SQL_C_CHAR, SQL_CHAR, 0, 0, c_w_id_buf,
				2, &c_w_id_len);
	ut_a(ret == SQL_SUCCESS);

	/*-----------------------------------------------------------*/		
	str = "{ORDER_STATUS(?)}";
	
	ret = SQLPrepare(order_status_query, (UCHAR*)str, ut_strlen(str));

	ut_a(ret == SQL_SUCCESS);

	ret = SQLBindParameter(order_status_query, 1, SQL_PARAM_INPUT,
				SQL_C_CHAR, SQL_CHAR, 0, 0, c_w_id_buf,
				2, &c_w_id_len);
	ut_a(ret == SQL_SUCCESS);

	/*-----------------------------------------------------------*/		
	str = "{DELIVERY(?)}";
	
	ret = SQLPrepare(delivery_query, (UCHAR*)str, ut_strlen(str));

	ut_a(ret == SQL_SUCCESS);

	ret = SQLBindParameter(delivery_query, 1, SQL_PARAM_INPUT,
				SQL_C_CHAR, SQL_CHAR, 0, 0, c_w_id_buf,
				2, &c_w_id_len);
	ut_a(ret == SQL_SUCCESS);

	/*-----------------------------------------------------------*/		
	str = "{STOCK_LEVEL(?)}";
	
	ret = SQLPrepare(stock_level_query, (UCHAR*)str, ut_strlen(str));

	ut_a(ret == SQL_SUCCESS);

	ret = SQLBindParameter(stock_level_query, 1, SQL_PARAM_INPUT,
				SQL_C_CHAR, SQL_CHAR, 0, 0, c_w_id_buf,
				2, &c_w_id_len);
	ut_a(ret == SQL_SUCCESS);

	/*-----------------------------------------------------------*/		
	str = "{ROLLBACK_QUERY()}";
	
	ret = SQLPrepare(rollback_query, (UCHAR*)str, ut_strlen(str));

	ut_a(ret == SQL_SUCCESS);
	/*-----------------------------------------------------------*/		

	oldtm = ut_clock();

	for (i = k; i < k + n_rounds / n_users; i++) {

		/* execute_until_success(ibuf_query, rollback_query); */

		if (i % 100 == 0) {
			printf("User %s round %lu\n", (char*)arg, i);
		}

		if (!own_warehouse) {
			c_w_id_buf[1] = (byte)ut_rnd_interval(1, n_warehouses);
			c_w_id_buf[0] = (byte)(ut_rnd_interval(1, n_warehouses)
									/ 256);
		}

		mach_write_to_1(c_d_id_buf, (ut_rnd_interval(1, 10) + 47));
		mach_write_to_3(c_id_buf, ut_rnd_interval(1, n_customers));

		n_lines = ut_rnd_interval(5, 15);

		if ((15 * k + i) % 100 == 0) {
			put_invalid_item = TRUE;

			/* printf("Will put invalid item\n"); */
		} else {
			put_invalid_item = FALSE;
		}

		for (t = 0; t < n_lines; t++) {
			mach_write_to_3(ol_i_ids_buf + 3 * t,
						ut_rnd_interval(1, n_items));

			if (put_invalid_item && (t + 1 == n_lines)) {
				mach_write_to_3(ol_i_ids_buf + 3 * t,
								n_items + 1);
			}

			mach_write_to_1(ol_quantities_buf + t,
						ut_rnd_interval(10, 20));
			ut_memcpy(ol_supply_w_ids_buf + 2 * t, c_w_id_buf, 2);
		}

		ol_i_ids_len = 3 * n_lines;
		ol_quantities_len = n_lines;
		ol_supply_w_ids_len = 2 * n_lines;

		execute_until_success(new_order_query, rollback_query);

		if (put_invalid_item) {

			goto skip_prints;
		}
/*		
		c_last_buf[c_last_len] = '\0';

		printf(
	"C_LAST %s, c_credit %s, c_discount, %lu, w_tax %lu, d_tax %lu\n",
			c_last_buf, c_credit_buf, w_tax_buf, d_tax_buf);

		printf("o_ol_count %lu, o_id %lu, o_entry_d %lu, total %lu\n",
			o_ol_count_buf, o_id_buf, o_entry_d_buf,
			total_buf);

		ut_a(c_credit_len == 2);
		ut_a(c_discount_len == 4);
		ut_a(i_names_len == 360);
			
		printf("i_names %s, bg %s\n", i_names_buf, bg_buf);
		
		for (t = 0; t < n_lines; t++) {
			printf("s_quantity %lu, i_price %lu, ol_amount %lu\n",
				mach_read_from_4(s_quantities_buf + 4 * t),
				mach_read_from_4(i_prices_buf + 4 * t),
				mach_read_from_4(ol_amounts_buf + 4 * t));
		}
*/
	skip_prints:
		;
	
		execute_until_success(payment_query, rollback_query);

		if (i % 10 == 3) {
			execute_until_success(order_status_query,
							rollback_query);
		}

		if ((i % 10 == 6) || (i % 100 == 60)) {
			execute_until_success(delivery_query, rollback_query);
		}

		if (i % 10 == 9) {
			execute_until_success(stock_level_query,
							rollback_query);
		}
	}

	tm = ut_clock();

	printf("Wall time for %lu loops %lu milliseconds\n",
						(i - k), tm - oldtm);

/*	execute_until_success(print_query, rollback_query); */

	n_exited++;

	printf("Client thread %lu exits as the %luth\n", k, n_exited);
	
	return(0);
}

/*********************************************************************
Test for single row select. */

ulint
test_single_row_select(
/*===================*/
	void*	arg)	/* in: user name as a null-terminated string */
{
	ulint	n_items = 200;
	HENV	env;
	HDBC	conn;
	RETCODE	ret;
	HSTMT	single_row_select_query;
	ulint	tm, oldtm;
	char*	str;
	byte	i_id_buf[3];
	byte	i_name_buf[25];
	SDWORD	i_id_len;
	SDWORD	i_name_len;
	ulint	i;
	
	ret = SQLAllocEnv(&env);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLAllocConnect(env, &conn);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLAllocStmt(conn, &single_row_select_query);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLConnect(conn, (UCHAR*)cli_srv_endpoint_name,
				(SWORD)ut_strlen(cli_srv_endpoint_name),
					(UCHAR*)arg,
					(SWORD)ut_strlen((char*)arg),
						(UCHAR*)"password", 8);
	ut_a(ret == SQL_SUCCESS);

	printf("Connection established\n");

	/*-----------------------------------------------------------*/		
	str =
	"{TEST_SINGLE_ROW_SELECT(?, ?)}";
							
	ret = SQLPrepare(single_row_select_query, (UCHAR*)str,
							ut_strlen(str));
	ut_a(ret == SQL_SUCCESS);

	ret = SQLBindParameter(single_row_select_query, 1, SQL_PARAM_INPUT,
				SQL_C_CHAR, SQL_CHAR, 0, 0, i_id_buf,
				3, &i_id_len);
	ut_a(ret == SQL_SUCCESS);
	i_id_len = 3;
	
	ret = SQLBindParameter(single_row_select_query, 2, SQL_PARAM_OUTPUT,
				SQL_C_CHAR, SQL_CHAR, 0, 0, i_name_buf,
				24, &i_name_len);
	ut_a(ret == SQL_SUCCESS);
	i_name_buf[24] = '\0';

	oldtm = ut_clock();

	for (i = 0; i < 10000; i++) {

		mach_write_to_3(i_id_buf, ut_rnd_interval(1, n_items));

		ret = SQLExecute(single_row_select_query);
		
		ut_a(ret == SQL_SUCCESS);
	}

	tm = ut_clock();

	printf("Wall time for %lu single row selects %lu milliseconds\n",
							i, tm - oldtm);
	return(0);
}

/*********************************************************************
TPC-D query 5. */

ulint
test_tpc_d_client(
/*==============*/
	void*	arg)	/* in: pointer to an array of startdate and enddate */
{
	char	buf[20];
	HENV	env;
	HDBC	conn1;
	RETCODE	ret;
	HSTMT	query5;
	HSTMT	join_test;
	char*	str;
	SDWORD	len1;
	SDWORD	len2;
	ulint	i;
	ulint	tm, oldtm;
	
	UT_NOT_USED(arg);
	
	ret = SQLAllocEnv(&env);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLAllocConnect(env, &conn1);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLAllocStmt(conn1, &query5);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLAllocStmt(conn1, &join_test);

	ut_a(ret == SQL_SUCCESS);

	sprintf(buf, "Use2%5lu", *((ulint*)arg));
	
	ret = SQLConnect(conn1, (UCHAR*)cli_srv_endpoint_name,
				(SWORD)ut_strlen(cli_srv_endpoint_name),
					(UCHAR*)buf,
					(SWORD)9, (UCHAR*)"password", 8);
	ut_a(ret == SQL_SUCCESS);

	printf("Connection established\n");

	/*-----------------------------------------------------------*/		
	str = "{TPC_D_QUERY_5(?, ?)}";
	
	ret = SQLPrepare(query5, (UCHAR*)str, ut_strlen(str));

	ut_a(ret == SQL_SUCCESS);

	ret = SQLBindParameter(query5, 1, SQL_PARAM_INPUT,
				SQL_C_LONG, SQL_INTEGER, 0, 0,
				(byte*)arg,
				4, &len1);
	ut_a(ret == SQL_SUCCESS);

	len1 = 4;

	ret = SQLBindParameter(query5, 2, SQL_PARAM_INPUT,
				SQL_C_LONG, SQL_INTEGER, 0, 0,
				(byte*)arg + sizeof(ulint),
				4, &len2);
	ut_a(ret == SQL_SUCCESS);

	len2 = 4;

	str = "{JOIN_TEST()}";
	
	ret = SQLPrepare(join_test, (UCHAR*)str, ut_strlen(str));

	ut_a(ret == SQL_SUCCESS);

	for (i = 0; i < n_rounds; i++) {
	
		oldtm = ut_clock();

		ret = SQLExecute(query5);

		/* ret = SQLExecute(join_test); */

		ut_a(ret == SQL_SUCCESS);

		tm = ut_clock();

		printf("Wall time %lu milliseconds\n", tm - oldtm);
	}

	printf("%s exits\n", buf);

	return(0);
}

/*********************************************************************
Checks consistency of the TPC databases. */

ulint
check_tpc_consistency(
/*==================*/
	void*	arg)	/* in: user name */
{
	HENV	env;
	HDBC	conn1;
	RETCODE	ret;
	HSTMT	consistency_query1;
	char*	str;

	UT_NOT_USED(arg);
	
	ret = SQLAllocEnv(&env);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLAllocConnect(env, &conn1);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLAllocStmt(conn1, &consistency_query1);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLConnect(conn1, (UCHAR*)cli_srv_endpoint_name,
				(SWORD)ut_strlen(cli_srv_endpoint_name),
				(UCHAR*)arg,
			(SWORD)ut_strlen((char*)arg), (UCHAR*)"password", 8);
	ut_a(ret == SQL_SUCCESS);

	printf("Connection established\n");

	/*-----------------------------------------------------------*/		
	str = "{TPC_CONSISTENCY()}";
	
	ret = SQLPrepare(consistency_query1, (UCHAR*)str, ut_strlen(str));

	ut_a(ret == SQL_SUCCESS);

	ret = SQLExecute(consistency_query1);

	ut_a(ret == SQL_SUCCESS);

	printf("Consistency checked\n");

	return(0);
}

/*********************************************************************
Test for errors. */

ulint
test_client_errors2(
/*================*/
	void*	arg)	/* in: ignored */
{
	HENV	env;
	HDBC	conn1;
	RETCODE	ret;
	HSTMT	error_test_query1;
	char*	str;
	byte	buf1[2];
	SDWORD	len1;
	UCHAR	sql_state[6];
	SDWORD	native_error;
	UCHAR	error_msg[512];
	SWORD	error_msg_max	= 512;
	SWORD	error_msg_len;

	UT_NOT_USED(arg);
	
	ret = SQLAllocEnv(&env);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLAllocConnect(env, &conn1);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLAllocStmt(conn1, &error_test_query1);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLConnect(conn1, (UCHAR*)cli_srv_endpoint_name,
				(SWORD)ut_strlen(cli_srv_endpoint_name),
					(UCHAR*)"conn2",
					(SWORD)5, (UCHAR*)"password", 8);
	ut_a(ret == SQL_SUCCESS);

	printf("Connection established\n");

	/*-----------------------------------------------------------*/		
	str = "{TEST_ERRORS(?)}";
	
	ret = SQLPrepare(error_test_query1, (UCHAR*)str, ut_strlen(str));

	ut_a(ret == SQL_SUCCESS);

	ret = SQLBindParameter(error_test_query1, 1, SQL_PARAM_INPUT,
				SQL_C_CHAR, SQL_CHAR, 0, 0, buf1,
				2, &len1);
	ut_a(ret == SQL_SUCCESS);

	/*-----------------------------------------------------------*/		

	printf("Thread 2 to do update\n");

	ut_memcpy(buf1, "06", 2);
	len1 = 2;
	ret = SQLExecute(error_test_query1);
	ut_a(ret == SQL_SUCCESS);

	printf("Thread 2 has done update\n");

	ret = SQLError(SQL_NULL_HENV, SQL_NULL_HDBC, error_test_query1,
			sql_state, &native_error, error_msg, error_msg_max,
			&error_msg_len);

	ut_a(ret == SQL_NO_DATA_FOUND);

	return(0);
}

/*********************************************************************
Test for errors. */

ulint
test_client_errors(
/*===============*/
	void*	arg)	/* in: ignored */
{
	HENV	env;
	HDBC	conn1;
	RETCODE	ret;
	HSTMT	error_test_query1;
	char*	str;
	byte	buf1[2];
	SDWORD	len1;
	UCHAR	sql_state[6];
	SDWORD	native_error;
	UCHAR	error_msg[512];
	SWORD	error_msg_max	= 512;
	SWORD	error_msg_len;
	os_thread_id_t	thread_id;

	UT_NOT_USED(arg);
	
	ret = SQLAllocEnv(&env);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLAllocConnect(env, &conn1);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLAllocStmt(conn1, &error_test_query1);

	ut_a(ret == SQL_SUCCESS);

	ret = SQLConnect(conn1, (UCHAR*)"innobase", 8, (UCHAR*)"conn1",
					(SWORD)5, (UCHAR*)"password", 8);
	ut_a(ret == SQL_SUCCESS);

	printf("Connection established\n");

	/*-----------------------------------------------------------*/		
	str = "{TEST_ERRORS(?)}";
	
	ret = SQLPrepare(error_test_query1, (UCHAR*)str, ut_strlen(str));

	ut_a(ret == SQL_SUCCESS);

	ret = SQLBindParameter(error_test_query1, 1, SQL_PARAM_INPUT,
				SQL_C_CHAR, SQL_CHAR, 0, 0, buf1,
				2, &len1);
	ut_a(ret == SQL_SUCCESS);

	/*-----------------------------------------------------------*/		

	ut_memcpy(buf1, "01", 2);
	len1 = 2;
	ret = SQLExecute(error_test_query1);
	ut_a(ret == SQL_SUCCESS);

	ut_memcpy(buf1, "02", 2);
	len1 = 2;
	ret = SQLExecute(error_test_query1);
	ut_a(ret == SQL_ERROR);

	ret = SQLError(SQL_NULL_HENV, SQL_NULL_HDBC, error_test_query1,
			sql_state, &native_error, error_msg, error_msg_max,
			&error_msg_len);

	ut_a(ret == SQL_SUCCESS);

	printf("%s\n", error_msg);

	ret = SQLError(SQL_NULL_HENV, SQL_NULL_HDBC, error_test_query1,
			sql_state, &native_error, error_msg, error_msg_max,
			&error_msg_len);

	ut_a(ret == SQL_NO_DATA_FOUND);

	ut_memcpy(buf1, "03", 2);
	len1 = 2;
	ret = SQLExecute(error_test_query1);
	ut_a(ret == SQL_SUCCESS);

	ut_memcpy(buf1, "01", 2);
	len1 = 2;
	ret = SQLExecute(error_test_query1);
	ut_a(ret == SQL_ERROR);

	ret = SQLError(SQL_NULL_HENV, SQL_NULL_HDBC, error_test_query1,
			sql_state, &native_error, error_msg, error_msg_max,
			&error_msg_len);

	ut_a(ret == SQL_SUCCESS);

	printf("%s\n", error_msg);

	ut_memcpy(buf1, "03", 2);
	len1 = 2;
	ret = SQLExecute(error_test_query1);
	ut_a(ret == SQL_SUCCESS);

	ut_memcpy(buf1, "04", 2);
	len1 = 2;
	ret = SQLExecute(error_test_query1);
	ut_a(ret == SQL_ERROR);

	ret = SQLError(SQL_NULL_HENV, SQL_NULL_HDBC, error_test_query1,
			sql_state, &native_error, error_msg, error_msg_max,
			&error_msg_len);

	ut_a(ret == SQL_SUCCESS);

	printf("%s\n", error_msg);

	ut_memcpy(buf1, "03", 2);
	len1 = 2;
	ret = SQLExecute(error_test_query1);
	ut_a(ret == SQL_SUCCESS);

	ut_memcpy(buf1, "05", 2);
	len1 = 2;
	ret = SQLExecute(error_test_query1);
	ut_a(ret == SQL_SUCCESS);

	os_thread_create(&test_client_errors2, "user000", &thread_id);	

	os_thread_sleep(5000000);
	
	ut_memcpy(buf1, "07", 2);
	len1 = 2;
	ret = SQLExecute(error_test_query1);
	ut_a(ret == SQL_ERROR);

	ret = SQLError(SQL_NULL_HENV, SQL_NULL_HDBC, error_test_query1,
			sql_state, &native_error, error_msg, error_msg_max,
			&error_msg_len);

	ut_a(ret == SQL_SUCCESS);

	printf("%s\n", error_msg);

	printf("Thread 1 to commit\n");

	ut_memcpy(buf1, "08", 2);
	len1 = 2;
	ret = SQLExecute(error_test_query1);
	ut_a(ret == SQL_SUCCESS);

	return(0);
}

/*************************************************************************
Simulates disk waits: if there are at least two threads active,
puts the current thread to wait for an event. If there is just the current
thread active and another thread doing a simulated disk wait, puts the
current thread to wait and releases another thread from wait, otherwise does
nothing */

void
srv_simulate_disk_wait(void)
/*========================*/
{
	os_event_t	event;
	ulint		wait_i;
	ulint		count;
	bool		found;
	ulint		rnd;
	ulint		i;
	ulint		j;

	mutex_enter(&kernel_mutex);

	srv_disk_rnd += 98687241;
	
	count = 0;
	found = FALSE;

	for (i = 0; i < SRV_N_SIM_DISK_ARRAY; i++) {

		if (!srv_sim_disk[i].empty) {

			count++;
		}

		if (!found && srv_sim_disk[i].empty) {

			srv_sim_disk[i].empty = FALSE;
			event = srv_sim_disk[i].event;

			os_event_reset(event);
			srv_sim_disk[i].event_set = FALSE;

			wait_i = i;

			found = TRUE;
		}
	}

	ut_a(found);

	if (srv_disk_n_active_threads == count + 1) {
		/* We have to release a thread from the disk wait array */;

		rnd = srv_disk_rnd;
		
		for (i = rnd; i < SRV_N_SIM_DISK_ARRAY + rnd; i++) {

			j = i % SRV_N_SIM_DISK_ARRAY;
				
			if (!srv_sim_disk[j].empty
					&& !srv_sim_disk[j].event_set) {

				srv_sim_disk[j].event_set = TRUE;
				os_event_set(srv_sim_disk[j].event);

				break;
			}
		}
	}

	mutex_exit(&kernel_mutex);

	os_event_wait(event);

	mutex_enter(&kernel_mutex);

	srv_sim_disk[wait_i].empty = TRUE;

	mutex_exit(&kernel_mutex);
}

/*************************************************************************
Releases a thread from the simulated disk wait array if there is any to
release. */

void
srv_simulate_disk_wait_release(void)
/*================================*/
{
	ulint	rnd;
	ulint	i;
	ulint	j;

	mutex_enter(&kernel_mutex);

	srv_disk_rnd += 98687241;
	rnd = srv_disk_rnd;
		
	for (i = rnd; i < SRV_N_SIM_DISK_ARRAY + rnd; i++) {

		j = i % SRV_N_SIM_DISK_ARRAY;
				
		if (!srv_sim_disk[j].empty
					&& !srv_sim_disk[j].event_set) {
		
			srv_sim_disk[j].event_set = TRUE;
			os_event_set(srv_sim_disk[j].event);

			break;
		}
	}

	mutex_exit(&kernel_mutex);
}

/*********************************************************************
Test for many threads and disk waits. */

ulint
test_disk_waits(
/*============*/
	void*	arg)	/* in: ignored */
{
	ulint	i;
	ulint	tm, oldtm;

	UT_NOT_USED(arg);
	
	n_exited++;

	printf("Client thread starts as the %luth\n", n_exited);
 
	oldtm = ut_clock();
	
	mutex_enter(&kernel_mutex);
	srv_disk_n_active_threads++;
	mutex_exit(&kernel_mutex);
	
	for (i = 0; i < 133; i++) {
		ut_delay(500);

/*		os_thread_yield(); */

/*		os_thread_sleep(10000); */

		srv_simulate_disk_wait();
	}

	mutex_enter(&kernel_mutex);
	srv_disk_n_active_threads--;
	mutex_exit(&kernel_mutex);

	srv_simulate_disk_wait_release();
	
	tm = ut_clock();

	printf("Wall time for %lu loops %lu milliseconds\n", i, tm - oldtm);

	n_exited++;

	printf("Client thread exits as the %luth\n", n_exited);
	
	return(0);
}

/*************************************************************************
Reads a keywords and a values from an initfile. In case of an error, exits
from the process. */

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
*/
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
	
/********************************************************************
Main test function. */

void 
main(void) 
/*======*/
{
	os_thread_t	thread_handles[1000];
	os_thread_id_t	thread_ids[1000];
	char		user_names[1000];
	ulint		tm, oldtm;
	ulint		i;
	ulint		dates[1000];

	cli_boot("cli_init");

	for (i = 1; i <= n_users; i++) {
		dates[2 * i] = startdate
				+ ((enddate - startdate) / n_users) * (i - 1);
		dates[2 * i + 1] = startdate
				+ ((enddate - startdate) / n_users) * i;
	}

	sync_init();

	mem_init(mem_pool_size);

	test_init(NULL);

	check_tpc_consistency("con21");

/*	test_client_errors(NULL); */

	os_thread_sleep(4000000);

	printf("Sleep ends\n");

	oldtm = ut_clock();

	for (i = 2; i <= n_users; i++) {
	    if (is_tpc_d) {
		thread_handles[i] = os_thread_create(&test_tpc_d_client,
					dates + 2 * i, thread_ids + i);
	    } else {
		sprintf(user_names + i * 8, "use2%3lu", i);

		thread_handles[i] = os_thread_create(&test_client,
					user_names + i * 8, thread_ids + i);
	    }

	    ut_a(thread_handles[i]);
	}

	if (is_tpc_d) {
		test_tpc_d_client(dates + 2 * 1);
	} else {
		test_client("use2  1");
	}

	for (i = 2; i <= n_users; i++) {
		os_thread_wait(thread_handles[i]);

		printf("Wait for thread %lu ends\n", i);
	}

	tm = ut_clock();

	printf("Wall time for test %lu milliseconds\n", tm - oldtm);

	os_thread_sleep(4000000);

	printf("Sleep ends\n");
	
	test_single_row_select("con99");
	
	check_tpc_consistency("con22");
	
	printf("TESTS COMPLETED SUCCESSFULLY!\n");
}
