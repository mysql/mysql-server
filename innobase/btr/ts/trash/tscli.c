/************************************************************************
Test for the client

(c) 1996-1997 Innobase Oy

Created 2/16/1996 Heikki Tuuri
*************************************************************************/

#include "ib_odbc.h"

/*********************************************************************
Test for TPC-C. */

ulint
test_c(
/*===*/
	void*	arg)
{
	HSTMT*		query;
	HSTMT*		commit_query;
	HSTMT*		new_order_query;
	HSTMT*		payment_query;
	HSTMT*		order_status_query;
	HSTMT*		delivery_query;
	HSTMT*		stock_level_query;
	HSTMT*		print_query;
	ulint		tm, oldtm;
	char*		str;
	char*		str1;
	char*		str2;
	char*		str3;
	char*		str4;
	char*		str5;
	char*		str6;
	ulint		i;

	UT_NOT_USED(arg);

	printf("-------------------------------------------------\n");
	printf("TEST. CREATE TABLES FOR TPC-C\n");

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
"				C_DATA CHAR);"
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
"				S_DATA CHAR);"
""	
"	CREATE UNIQUE CLUSTERED INDEX S_IND ON STOCK (S_W_ID, S_I_ID);"
"	END;"
	;

	str = ut_str_catenate(str1, str2);

	query = pars_sql(str);

	mem_free(str);

	tm = ut_clock();
	printf("Wall time for test %lu milliseconds\n", tm - oldtm);

	/*-----------------------------------------------------------*/
	printf("\n\nPopulate TPC-C tables\n\n");

	str1 = 

"	PROCEDURE POPULATE_TABLES () IS"
""
"	i INT;"
"	j INT;"
"	k INT;"
"	t INT;"
"	string CHAR;"
"	rnd1 INT;"
"	rnd2 INT;"
"	n_items INT;"
"	n_warehouses INT;"
"	n_districts INT;"
"	n_customers INT;"
""
"	BEGIN"
""
"	n_items := 200;"
"	n_warehouses := 1;"
"	n_districts := 10;"
"	n_customers := 200;"
""
"	PRINTF('Starting to populate ITEMs');"
""
"	FOR i IN 1 .. n_items LOOP"
"		rnd1 := RND(26, 50);"
"		string := RND_STR(rnd1);"
""
"		IF (RND(0, 9) = 0) THEN"
"			rnd2 := RND(0, rnd1 - 8);"
"			REPLSTR(string, 'ORIGINAL', rnd2, 8);"
"			COMMIT WORK;"
"		END IF;"
""
"		INSERT INTO ITEM VALUES (TO_BINARY(i, 3),"
"					TO_BINARY(RND(1, 10000), 3),"
"					RND_STR(RND(14, 24)),"
"					RND(100, 10000),"
"					string);"
"	END LOOP;"
""
"	FOR i IN 1 .. n_warehouses LOOP"
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
"			IF (RND(0, 9) = 0) THEN"
"				rnd2 := RND(0, rnd1 - 8);"
"				REPLSTR(string, 'ORIGINAL', rnd2, 8);"
"				COMMIT WORK;"
"			END IF; "
""
"			INSERT INTO STOCK VALUES (TO_BINARY(j, 3),"
"						TO_BINARY(i, 2),"
"						RND(10, 100),"
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
"			COMMIT WORK;"
"			PRINTF('Starting to populate district number ', j);"
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
"				IF (RND(0, 9) = 7) THEN"
"					COMMIT WORK;"
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
"							RND(0, n_items - 1),"
"							3),"
"						TO_BINARY(i, 2),"
"						SYSDATE(),"
"						RND(0, 99),"
"						RND(0, 9999),"
"						RND_STR(24));"
"				END LOOP;"
"			END LOOP;"
"			"
"			FOR k IN (2 * n_customers) / 3 .. n_customers LOOP"
"				"
"				INSERT INTO NEW_ORDER VALUES ("
"						k,"
"						TO_BINARY(j + 47, 1),"
"						TO_BINARY(i, 2));"
"			END LOOP;"
"		END LOOP;"
"	END LOOP;	"
"	"
"	COMMIT WORK;"
"	END;"
	;

	str4 = ut_str_catenate(str1, str2);
	str = ut_str_catenate(str4, str3);
	
	query = pars_sql(str);

	mem_free(str);
	mem_free(str4);

	
	tm = ut_clock();
	printf("Wall time for test %lu milliseconds\n", tm - oldtm);

	/*-----------------------------------------------------------*/
	str = 

"	PROCEDURE PRINT_TABLES () IS"
"	BEGIN"
""
"	/* PRINTF('Printing ITEM table:');"
""
"	ROW_PRINTF"
"		SELECT *"
"		FROM ITEM;"
""
"	PRINTF('Printing WAREHOUSE table:');"
""
"	ROW_PRINTF"
"		SELECT *"
"		FROM WAREHOUSE;"
""
"	PRINTF('Printing STOCK table:');"
""
"	ROW_PRINTF"
"		SELECT *"
"		FROM STOCK;"
""
"	PRINTF('Printing DISTRICT table:');"
""
"	ROW_PRINTF"
"		SELECT *"
"		FROM DISTRICT;"
""
"	PRINTF('Printing CUSTOMER table:');"
""
"	ROW_PRINTF"
"		SELECT *"
"		FROM CUSTOMER;"
""
"	PRINTF('Printing HISTORY table:');"
""
"	ROW_PRINTF"
"		SELECT *"
"		FROM HISTORY;"
""
"	PRINTF('Printing ORDERS table:');"
""
"	ROW_PRINTF"
"		SELECT *"
"		FROM ORDERS;"
""
"	PRINTF('Printing ORDER_LINE table:');"
""
"	ROW_PRINTF"
"		SELECT *"
"		FROM ORDER_LINE"
"			WHERE OL_O_ID >= 3000; */"
""
"	PRINTF('Printing NEW_ORDER table:');"
""
"	ROW_PRINTF"
"		SELECT *"
"		FROM NEW_ORDER;"
""
"	COMMIT WORK;"
"	END;"
	;

	print_query = pars_sql(str);

	/*-----------------------------------------------------------*/
	commit_query = pars_sql(

"	PROCEDURE COMMIT_TEST () IS"
"	"
"	BEGIN"
"	COMMIT WORK;"
"	END;"
	);
	

	/*-----------------------------------------------------------*/

	str1 =
	
"	PROCEDURE NEW_ORDER () IS"
""
" 	i INT;"
" 	n_items INT;"
" 	n_warehouses INT;"
" 	n_districts INT;"
" 	n_customers INT;"
"	w_tax INT;"
" 	c_w_id CHAR;"
" 	c_d_id CHAR;"
" 	c_id CHAR;"
" 	c_discount INT;"
" 	c_last CHAR;"
" 	c_credit CHAR;"
" 	d_tax INT;"
" 	o_id INT;"
" 	o_ol_cnt INT;"
" 	ol_i_id CHAR;"
" 	o_entry_d INT;"
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
" 	bg CHAR;"
" 	ol_quantity INT;"
" 	ol_amount INT;"
" 	ol_supply_w_id CHAR;"
" 	ol_dist_info CHAR;"
" 	total INT;"
""
" 	DECLARE CURSOR district_cursor IS"
" 		SELECT D_NEXT_O_ID, D_TAX"
" 		FROM DISTRICT"
" 			WHERE D_ID = c_d_id AND D_W_ID = c_w_id"
" 			FOR UPDATE;"
""
" 	DECLARE CURSOR stock_cursor IS"
" 		SELECT S_QUANTITY, S_DATA,"
"				S_DIST_01, S_DIST_02, S_DIST_03, S_DIST_04,"
"				S_DIST_05, S_DIST_06, S_DIST_07, S_DIST_08,"
"				S_DIST_09, S_DIST_10"
" 		FROM STOCK"
" 			WHERE S_W_ID = ol_supply_w_id AND S_I_ID = ol_i_id"
" 			FOR UPDATE;"
	;
	str2 =
	
" 	BEGIN"
" 	"
" 	n_items := 200;"
" 	n_warehouses := 1;"
" 	n_districts := 10;"
" 	n_customers := 200;"
" 	"
" 	c_w_id := TO_BINARY(RND(1, n_warehouses), 2);"
" 	c_d_id := TO_BINARY(RND(1, n_districts) + 47, 1);"
" 	c_id := TO_BINARY(RND(1, n_customers), 3);"
""
" 	o_ol_cnt := RND(5, 15);"
" 	o_all_local := '1';"
" 	bg := 'GGGGGGGGGGGGGGG';"
"	total := 0;"
"	"
"	SELECT W_TAX INTO w_tax"
"	FROM WAREHOUSE"
"		WHERE W_ID = c_w_id;"
""
"	OPEN district_cursor;"
""
"	FETCH district_cursor INTO o_id, d_tax;"
""
"	/* PRINTF('C-warehouse id ', BINARY_TO_NUMBER(c_w_id),"
"		' C-district id ', c_d_id,"
"		' order id ', o_id, ' linecount ', o_ol_cnt); */"
""
"	UPDATE DISTRICT SET D_NEXT_O_ID = o_id + 1"
"		WHERE CURRENT OF district_cursor;"
""
"	CLOSE district_cursor;"
""
"	SELECT C_DISCOUNT, C_LAST, C_CREDIT INTO c_discount, c_last, c_credit"
"	FROM CUSTOMER"
"		WHERE C_W_ID = c_w_id AND C_D_ID = c_d_id AND C_ID = c_id;"
""
	;
	str3 =

" 	FOR i IN 1 .. o_ol_cnt LOOP"
""
"		ol_i_id := TO_BINARY(RND(1, n_items), 3);"
""
"		ol_supply_w_id := c_w_id;"
""
"		ol_quantity := RND(1, 10);"
""
"		SELECT I_PRICE, I_NAME, I_DATA INTO i_price, i_name, i_data"
"		FROM ITEM"
"			WHERE I_ID = ol_i_id;"
""
"		IF (SQL % NOTFOUND) THEN"
"			PRINTF('Rolling back');"
"			ROLLBACK WORK;"
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
"			REPLSTR(bg, 'B', i - 1, 1);"
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
"					TO_BINARY(i, 1), ol_i_id,"
"					ol_supply_w_id, NULL, ol_quantity,"
"					ol_amount, ol_dist_info); "
"	END LOOP;"
""
"	total := (((total * (10000 + w_tax + d_tax)) / 10000)"
"			  * (10000 - c_discount)) / 10000;"
""
"	o_entry_d := SYSDATE();"
""
"	INSERT INTO ORDERS VALUES (o_id, c_d_id, c_w_id, c_id, o_entry_d,"
"					NULL, o_ol_cnt, o_all_local);"
"	INSERT INTO NEW_ORDER VALUES (o_id, c_d_id, c_w_id);"
""
"	/* PRINTF('Inserted order lines:');"
"	ROW_PRINTF"
"		SELECT * FROM ORDER_LINE WHERE OL_O_ID = o_id AND"
"						OL_D_ID = c_d_id"
"						AND OL_W_ID = c_w_id; */"
" 	/* COMMIT WORK; */"
" 	END;"
	;

	str5 = ut_str_catenate(str1, str2);
	str6 = ut_str_catenate(str3, str4);

	str = ut_str_catenate(str5, str6);
	
	new_order_query = pars_sql(str);

	mem_free(str);
	mem_free(str5);
	mem_free(str6);

	/*-----------------------------------------------------------*/

	str1 =
	
"	PROCEDURE PAYMENT () IS"
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
"	c_w_id CHAR;"
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
"		FROM WAREHOUSE"
"			WHERE W_ID = w_id"
"			FOR UPDATE;"
""
"	DECLARE CURSOR district_cursor IS"
"		SELECT D_STREET_1, D_STREET_2, D_CITY, D_STATE, D_ZIP, D_NAME"
"		FROM DISTRICT"
"			WHERE D_W_ID = w_id AND D_ID = d_id"
"			FOR UPDATE;"
""
"	DECLARE CURSOR customer_by_name_cursor IS"
"		SELECT C_ID"
"		FROM CUSTOMER"
"			WHERE C_W_ID = c_w_id AND C_D_ID = c_d_id"
"				AND C_LAST = c_last"
"			ORDER BY C_FIRST ASC;"
""
"	DECLARE CURSOR customer_cursor IS"
"		SELECT C_FIRST, C_MIDDLE, C_LAST, C_STREET_1, C_STREET_2,"
"				C_CITY, C_STATE, C_ZIP, C_PHONE, C_CREDIT,"
"				C_CREDIT_LIM, C_DISCOUNT, C_BALANCE,"
"				C_SINCE"
"		FROM CUSTOMER"
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
" 	n_customers := 200;"
""
"	byname := RND(1, 100);"
"	amount := RND(1, 1000);"
"	h_date := SYSDATE();"
"	w_id := TO_BINARY(RND(1, n_warehouses), 2);"
"	d_id := TO_BINARY(47 + RND(1, n_districts), 1);"
"	c_w_id := TO_BINARY(RND(1, n_warehouses), 2);"
"	c_d_id := TO_BINARY(47 + RND(1, n_districts), 1);"
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
"	OPEN district_cursor;"
""
"	FETCH district_cursor INTO d_street_1, d_street_2, d_city, d_state,"
"							d_zip, d_name;"
"	UPDATE DISTRICT SET D_YTD = D_YTD + amount"
"		WHERE CURRENT OF district_cursor;"
""
"	CLOSE district_cursor;"
""
"	IF (byname <= 60) THEN"
"		c_last := CONCAT('NAME', TO_CHAR(RND(1, n_customers) / 3));"
""
"		SELECT COUNT(*) INTO namecnt"
"		FROM CUSTOMER"
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
"	h_data := CONCAT(w_name, '    ', d_name);"
"	"
"	IF (c_credit = 'BC') THEN"
"		/* PRINTF('Bad customer pays'); */"
""
"		SELECT C_DATA INTO c_data"
"		FROM CUSTOMER"
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
"	/* COMMIT WORK; */"
""
"	END;"

	;

	str4 = ut_str_catenate(str1, str2);
	str = ut_str_catenate(str4, str3);
	
	payment_query = pars_sql(str);

	mem_free(str);
	mem_free(str4);

	/*-----------------------------------------------------------*/

	str1 =
	
"	PROCEDURE ORDER_STATUS () IS"
""
" 	i INT;"
" 	n_items INT;"
" 	n_warehouses INT;"
" 	n_districts INT;"
"	n_customers INT;"
"	d_id CHAR;"
"	namecnt INT;"
"	c_w_id CHAR;"
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
"		FROM ORDERS"
"			WHERE O_W_ID = c_w_id AND O_D_ID = c_d_id"
"							AND O_C_ID = c_id"
"			ORDER BY O_ID DESC;"
""
"	DECLARE CURSOR order_line_cursor IS"
"		SELECT OL_I_ID, OL_SUPPLY_W_ID, OL_QUANTITY, OL_AMOUNT,"
"							OL_DELIVERY_D"
"		FROM ORDER_LINE"
"			WHERE OL_W_ID = c_w_id AND OL_D_ID = c_d_id"
"							AND OL_O_ID = o_id;"
"	DECLARE CURSOR customer_by_name_cursor IS"
"		SELECT C_ID"
"		FROM CUSTOMER"
"			WHERE C_W_ID = c_w_id AND C_D_ID = c_d_id"
"				AND C_LAST = c_last"
"			ORDER BY C_FIRST ASC;"
"	BEGIN"
""
" 	n_items := 200;"
" 	n_warehouses := 1;"
" 	n_districts := 10;"
" 	n_customers := 200;"
""
"	c_w_id := TO_BINARY(RND(1, n_warehouses), 2);"
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
"		FROM CUSTOMER"
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
"	FROM CUSTOMER"
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
"		/* COMMIT WORK; */"
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
"			/* COMMIT WORK; */"
""
"			RETURN;"
"		END IF;"
"	END LOOP;"
"	ASSERT(0 = 1);"
"	"
"	END;"
	;

	str = ut_str_catenate(str1, str2);
	
	order_status_query = pars_sql(str);

	mem_free(str);

	/*-----------------------------------------------------------*/

	str1 =
	
"	PROCEDURE DELIVERY () IS"
""
" 	i INT;"
" 	n_items INT;"
" 	n_warehouses INT;"
" 	n_districts INT;"
"	n_customers INT;"
"	d_id CHAR;"
"	w_id CHAR;"
"	c_id CHAR;"
"	o_id INT;"
"	o_carrier_id INT;"
"	ol_delivery_d INT;"
"	ol_total INT;"
""
"	DECLARE CURSOR new_order_cursor IS"
"		SELECT NO_O_ID"
"		FROM NEW_ORDER"
"			WHERE NO_W_ID = w_id AND NO_D_ID = d_id"
"			ORDER BY NO_O_ID ASC;"
""
"	DECLARE CURSOR orders_cursor IS"
"		SELECT O_C_ID"
"		FROM ORDERS"
"			WHERE O_W_ID = w_id AND O_D_ID = d_id"
"							AND O_ID = o_id"
"			FOR UPDATE;"
"	BEGIN"
""
" 	n_items := 200;"
" 	n_warehouses := 1;"
" 	n_districts := 10;"
" 	n_customers := 200;"
""
"	w_id := TO_BINARY(RND(1, n_warehouses), 2);"
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
"			/* PRINTF('Order to deliver'); */"
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
"			FROM ORDER_LINE"
"				WHERE OL_W_ID = w_id AND OL_D_ID = d_id"
"						AND OL_O_ID = o_id;"
""
"			UPDATE CUSTOMER SET C_BALANCE = C_BALANCE - ol_total"
"				WHERE C_W_ID = w_id AND C_D_ID = d_id"
"						AND C_ID = c_id;"
"		END IF;"
"	END LOOP;"
""
"	/* COMMIT WORK; */"
"	"
"	END;"
	;

	str = ut_str_catenate(str1, str2);
	
	delivery_query = pars_sql(str);

	mem_free(str);

	/*-----------------------------------------------------------*/

	/* NOTE: COUNT(DISTINCT ...) not implemented yet */

	str =
	
"	PROCEDURE STOCK_LEVEL () IS"
""
" 	n_items INT;"
" 	n_warehouses INT;"
" 	n_districts INT;"
"	n_customers INT;"
"	d_id CHAR;"
"	w_id CHAR;"
"	o_id INT;"
"	stock_count INT;"
"	threshold INT;"
""
"	BEGIN"
""
" 	n_items := 200;"
" 	n_warehouses := 1;"
" 	n_districts := 10;"
" 	n_customers := 200;"
""
"	w_id := TO_BINARY(RND(1, n_warehouses), 2);"
"	d_id := TO_BINARY(47 + 4, 1);"
""
"	threshold := RND(10, 20);"
""
"	SELECT D_NEXT_O_ID INTO o_id"
"	FROM DISTRICT"
"		WHERE D_W_ID = w_id AND D_ID = d_id;"
""
"	SELECT COUNT(*) INTO stock_count"
"	FROM ORDER_LINE, STOCK"
"		WHERE OL_W_ID = w_id AND OL_D_ID = d_id"
"			AND OL_O_ID >= o_id - 20 AND OL_O_ID < o_id"
"			AND S_W_ID = w_id AND S_I_ID = OL_I_ID"
"			AND S_QUANTITY < threshold;"
"	/* PRINTF(stock_count, ' items under threshold ', threshold); */"
"	/* COMMIT WORK; */"
""	
"	END;"
	;
	
	stock_level_query = pars_sql(str);
	/*-----------------------------------------------------------*/

	oldtm = ut_clock();

	for (i = 0; i < 10; i++) {
		mutex_enter(&kernel_mutex);
	
		thr = que_fork_start_command(new_order_query,
							SESS_COMM_EXECUTE, 0);
		mutex_exit(&kernel_mutex);
		
		que_run_threads(thr);			
		
		mutex_enter(&kernel_mutex);
	
		thr = que_fork_start_command(payment_query,
							SESS_COMM_EXECUTE, 0);
		mutex_exit(&kernel_mutex);
		
		que_run_threads(thr);

		if (i % 10 == 3) {
			mutex_enter(&kernel_mutex);
	
			thr = que_fork_start_command(order_status_query,
							SESS_COMM_EXECUTE, 0);
			mutex_exit(&kernel_mutex);
		
			que_run_threads(thr);
		}

		if ((i % 10 == 6) || (i % 100 == 60)) {
			mutex_enter(&kernel_mutex);
	
			thr = que_fork_start_command(delivery_query,
							SESS_COMM_EXECUTE, 0);
			mutex_exit(&kernel_mutex);
		
			que_run_threads(thr);
		}

		if (i % 10 == 9) {
			mutex_enter(&kernel_mutex);
	
			thr = que_fork_start_command(stock_level_query,
							SESS_COMM_EXECUTE, 0);
			mutex_exit(&kernel_mutex);
		
			que_run_threads(thr);
		}

		if ((i > 0) && (i % 200 == 0)) {
			mutex_enter(&kernel_mutex);
	
			thr = que_fork_start_command(commit_query,
							SESS_COMM_EXECUTE, 0);
			mutex_exit(&kernel_mutex);
		
			que_run_threads(thr);
		}
	}
	
	tm = ut_clock();
	printf("Wall time for test %lu milliseconds\n", tm - oldtm);

	return(0);
}

#ifdef notdefined

/*********************************************************************
General test. */

ulint
test1(
/*==*/
	void*	arg)
{
	sess_t*		sess;
	sess_t*		sess2;
	com_endpoint_t*	com_endpoint;
	mem_heap_t*	heap;
	que_fork_t*	query;
	que_thr_t*	thr;
	trx_t*		trx;
	trx_t*		trx2;
	ulint		tm, oldtm;
	ulint		j;
	
	UT_NOT_USED(arg);

	printf("-------------------------------------------------\n");
	printf("TEST 1. GENERAL TEST\n");

	heap = mem_heap_create(512);

	com_endpoint = (com_endpoint_t*)heap;	/* This is a dummy non-NULL
						value */
	mutex_enter(&kernel_mutex);	

	sess = sess_open(ut_dulint_zero, com_endpoint, (byte*)"user1", 6);
	sess2 = sess_open(ut_dulint_zero, com_endpoint, (byte*)"user2", 6);
	
	trx = sess->trx;
	trx2 = sess2->trx;

	mutex_exit(&kernel_mutex);

	/*------------------------------------------------------*/
	query = pars_sql(
"	PROCEDURE CREATE_TABLE () IS"
"	BEGIN"
"	CREATE TABLE TS_TABLE1 (COL1 CHAR, COL2 CHAR, COL3 CHAR);"
"	CREATE TABLE TS_TABLE2 (COL21 INT, COL22 INT, COL23 CHAR);"
"	CREATE TABLE TS_TABLE3 (COL31 INT, COL32 INT, COL33 CHAR);"
"	CREATE TABLE TS_TABLE4 (COL41 INT, COL42 INT, COL43 CHAR);"
"	CREATE UNIQUE CLUSTERED INDEX IND1 ON TS_TABLE1 (COL1);"
"	CREATE UNIQUE CLUSTERED INDEX IND21 ON TS_TABLE2 (COL21);"
"	CREATE UNIQUE CLUSTERED INDEX IND31 ON TS_TABLE3 (COL31);"
"	CREATE CLUSTERED INDEX IND41 ON TS_TABLE4 (COL41);"
"	END;"
	);

	query->trx = trx;

	thr = UT_LIST_GET_FIRST(query->thrs);

	mutex_enter(&kernel_mutex);	

	ut_a(thr == que_fork_start_command(query, SESS_COMM_EXECUTE, 0));
	
	mutex_exit(&kernel_mutex);

	que_run_threads(thr);

	/*------------------------------------------------------*/
	/*------------------------------------------------------*/
	
	printf("Will start insert test\n");

	query = pars_sql(

"	PROCEDURE INSERT_SPEED_TEST () IS"
"	int1 INT;"
"	int2 INT;"
"	"
"	BEGIN"
"	int2 := 0;"
"	int1 := 0;"
"	WHILE int1 < 40 LOOP"
"		INSERT INTO TS_TABLE2 VALUES (int1, int1 - 100 * (int1 / 100),"
"					'123456789012345678901234567890');"
"		int1 := int1 + 1;"
"	"
"	END LOOP;"
"	"
"	END;"
	);
	
	query->trx = trx;

	thr = UT_LIST_GET_FIRST(query->thrs);

	mutex_enter(&kernel_mutex);	

	ut_a(thr == que_fork_start_command(query, SESS_COMM_EXECUTE, 0));
	
	mutex_exit(&kernel_mutex);

	oldtm = ut_clock();
	
	que_run_threads(thr);
	
	tm = ut_clock();
	printf("Wall time for insert test %lu milliseconds\n", tm - oldtm);

	
	/*------------------------------------------------------*/
	/*------------------------------------------------------*/
	
	query = pars_sql(

"	PROCEDURE COMMIT_SPEED_TEST () IS"
"	"
"	BEGIN"
"	COMMIT WORK;"
"	END;"
	);
	
	query->trx = trx;

	thr = UT_LIST_GET_FIRST(query->thrs);

	mutex_enter(&kernel_mutex);	

	ut_a(thr == que_fork_start_command(query, SESS_COMM_EXECUTE, 0));
	
	mutex_exit(&kernel_mutex);

	oldtm = ut_clock();
	
	que_run_threads(thr);
	
	tm = ut_clock();
	printf("Wall time for commit %lu milliseconds\n", tm - oldtm);

/*	dict_table_print_by_name("TS_TABLE2"); */
	/*------------------------------------------------------*/
	/*------------------------------------------------------*/
	printf("Will start insert test2\n");

	query = pars_sql(

"	PROCEDURE INSERT_SPEED_TEST2 () IS"
"	int1 INT;"
"	int2 INT;"
"	"
"	BEGIN"
"	INSERT INTO TS_TABLE3 SELECT * FROM TS_TABLE2;"
"	"
"	"
"	END;"
	);
	
	query->trx = trx;

	thr = UT_LIST_GET_FIRST(query->thrs);

	mutex_enter(&kernel_mutex);	

	ut_a(thr == que_fork_start_command(query, SESS_COMM_EXECUTE, 0));
	
	mutex_exit(&kernel_mutex);

	oldtm = ut_clock();
	
	que_run_threads(thr);
	
	tm = ut_clock();
	printf("Wall time for insert test2 %lu milliseconds\n", tm - oldtm);

	/*------------------------------------------------------*/
	
	query = pars_sql(

"	PROCEDURE COMMIT_SPEED_TEST () IS"
"	"
"	BEGIN"
"	COMMIT WORK;"
"	END;"
	);
	
	query->trx = trx;

	thr = UT_LIST_GET_FIRST(query->thrs);

	mutex_enter(&kernel_mutex);	

	ut_a(thr == que_fork_start_command(query, SESS_COMM_EXECUTE, 0));
	
	mutex_exit(&kernel_mutex);

	oldtm = ut_clock();
	
	que_run_threads(thr);
	
	tm = ut_clock();
	printf("Wall time for commit %lu milliseconds\n", tm - oldtm);

/*	dict_table_print_by_name("TS_TABLE2"); */
	/*------------------------------------------------------*/
	/*------------------------------------------------------*/
/*	os_thread_sleep(1000000); */
	
/*	btr_search_table_print_info("TS_TABLE3"); */

	query = pars_sql(

"	PROCEDURE JOIN_SPEED_TEST () IS"
"	int1 INT;"
"	"
"	BEGIN"
"	SELECT COUNT(*) INTO int1"
"			FROM TS_TABLE2, TS_TABLE3"
"			WHERE COL21 = COL31"
"			CONSISTENT READ;"
"	PRINTF(int1);"
"	END;"
	);
	
	query->trx = trx;

	thr = UT_LIST_GET_FIRST(query->thrs);

  	for (j = 0; j < 20; j++) {

		mutex_enter(&kernel_mutex);	
		
		ut_a(thr == que_fork_start_command(query, SESS_COMM_EXECUTE,
									0));
		mutex_exit(&kernel_mutex);
		
		oldtm = ut_clock();
		
		que_run_threads(thr);
		
		tm = ut_clock();

		printf("Wall time for join test %lu milliseconds\n",
								tm - oldtm);
  	}

/*	btr_search_table_print_info("TS_TABLE3"); */

	/*------------------------------------------------------*/
	printf("Will start update test\n");

	os_thread_sleep(2000000);

	query = pars_sql(

"	PROCEDURE UPDATE_SPEED_TEST () IS"
"	int1 INT;"
"	BEGIN"
"	UPDATE TS_TABLE2 SET COL22 = COL22 + 1;"
"	END;"
	);
	
	query->trx = trx;

	thr = UT_LIST_GET_FIRST(query->thrs);

	mutex_enter(&kernel_mutex);	

	ut_a(thr == que_fork_start_command(query, SESS_COMM_EXECUTE, 0));
	
	mutex_exit(&kernel_mutex);

	oldtm = ut_clock();
	
	que_run_threads(thr);
	
	tm = ut_clock();
	printf("Wall time for update test %lu milliseconds\n", tm - oldtm);

	/*------------------------------------------------------*/
	/*------------------------------------------------------*/
	
	query = pars_sql(

"	PROCEDURE COMMIT_SPEED_TEST () IS"
"	"
"	BEGIN"
"	COMMIT WORK;"
"	END;"
	);
	
	query->trx = trx;

	thr = UT_LIST_GET_FIRST(query->thrs);

	mutex_enter(&kernel_mutex);	

	ut_a(thr == que_fork_start_command(query, SESS_COMM_EXECUTE, 0));
	
	mutex_exit(&kernel_mutex);

	oldtm = ut_clock();
	
	que_run_threads(thr);
	
	tm = ut_clock();
	printf("Wall time for commit %lu milliseconds\n", tm - oldtm);

/*	dict_table_print_by_name("TS_TABLE2"); */
	/*------------------------------------------------------*/
	/*------------------------------------------------------*/
	printf("Will start TPC-A\n");
	os_thread_sleep(2000000);

	query = pars_sql(
"	PROCEDURE TPC_A_SPEED_TEST () IS"
"	int1 INT;"
"	"
"	BEGIN"
"	int1 := 0;"
"	WHILE int1 < 1000 LOOP"
"		INSERT INTO TS_TABLE4 VALUES (int1, int1,"
"					'123456789012345678901234567890');"
"		UPDATE TS_TABLE2 SET COL22 = COL22 + 1"
"			WHERE COL21 = int1;"
"		UPDATE TS_TABLE2 SET COL22 = COL22 + 1"
"			WHERE COL21 = int1 + 1;"
"		UPDATE TS_TABLE2 SET COL22 = COL22 + 1"
"			WHERE COL21 = int1 + 2;"
"		int1 := int1 + 1;"
"	END LOOP;"
"	"
"	END;"
	);
	
/*"	SELECT COUNT(*) INTO int1 FROM TS_TABLE2 WHERE COL22 = COL21 + 4;"
"	PRINTF(int1);"
"	SELECT COUNT(*) INTO int1 FROM TS_TABLE4;"
"	PRINTF(int1);"
*/
	query->trx = trx;

	thr = UT_LIST_GET_FIRST(query->thrs);

	mutex_enter(&kernel_mutex);	

	ut_a(thr == que_fork_start_command(query, SESS_COMM_EXECUTE, 0));
	
	mutex_exit(&kernel_mutex);

	oldtm = ut_clock();
	
	que_run_threads(thr);
	
	tm = ut_clock();
	printf("Wall time for TPC-A test %lu milliseconds\n", tm - oldtm);

	/*------------------------------------------------------*/
	
	query = pars_sql(

"	PROCEDURE COMMIT_SPEED_TEST () IS"
"	"
"	BEGIN"
"	COMMIT WORK;"
"	END;"
	);
	
	query->trx = trx;

	thr = UT_LIST_GET_FIRST(query->thrs);

	mutex_enter(&kernel_mutex);	

	ut_a(thr == que_fork_start_command(query, SESS_COMM_EXECUTE, 0));
	
	mutex_exit(&kernel_mutex);

	oldtm = ut_clock();
	
	que_run_threads(thr);
	
	tm = ut_clock();
	printf("Wall time for commit %lu milliseconds\n", tm - oldtm);

	/*------------------------------------------------------*/
	printf("Will start insert test\n");

	os_thread_sleep(2000000);

	query = pars_sql(

"	PROCEDURE INSERT_SPEED_TEST () IS"
"	int1 INT;"
"	int2 INT;"
"	"
"	BEGIN"
"	int2 := 0;"
"	int1 := 0;"
"	WHILE int1 < 1000 LOOP"
"		INSERT INTO TS_TABLE2 VALUES (int1, int1,"
"					'123456789012345678901234567890');"
"		int1 := int1 + 1;"
"	"
"	END LOOP;"
"	SELECT COUNT(*) INTO int2"
"			FROM TS_TABLE2;"
"	ASSERT(int1 = int2);"
"	"
"	COMMIT WORK;"
"	"
"	END;"
	);
	
	query->trx = trx;

	thr = UT_LIST_GET_FIRST(query->thrs);

	mutex_enter(&kernel_mutex);	

	ut_a(thr == que_fork_start_command(query, SESS_COMM_EXECUTE, 0));
	
	mutex_exit(&kernel_mutex);

	oldtm = ut_clock();
	
	que_run_threads(thr);
	
	tm = ut_clock();
	printf("Wall time for insert test %lu milliseconds\n", tm - oldtm);

	/*------------------------------------------------------*/
	/*------------------------------------------------------*/

	query = pars_sql(

"	PROCEDURE DELETE_SPEED_TEST () IS"
"	int1 INT;"
"	int2 INT;"
"	"
"	BEGIN"
"	SELECT COUNT(*), SUM(COL22) INTO int1, int2"
"			FROM TS_TABLE2"
"			CONSISTENT READ;"
"	ASSERT(int1 = 1000);"
"	ASSERT(int2 = 999 * 500);"
"	DELETE FROM TS_TABLE2;"
"	"
"	SELECT COUNT(*), SUM(COL22) INTO int1, int2"
"			FROM TS_TABLE2"
"			CONSISTENT READ;"
"	ASSERT(int1 = 0);"
"	ASSERT(int2 = 0);"
"	"
"	END;"
	);
	
	query->trx = trx;

	thr = UT_LIST_GET_FIRST(query->thrs);

	mutex_enter(&kernel_mutex);	

	ut_a(thr == que_fork_start_command(query, SESS_COMM_EXECUTE, 0));
	
	mutex_exit(&kernel_mutex);

	oldtm = ut_clock();
	
	que_run_threads(thr);
	
	tm = ut_clock();
	printf("Wall time for delete test %lu milliseconds\n", tm - oldtm);
	/*------------------------------------------------------*/
	/*------------------------------------------------------*/
	os_thread_sleep(2000000);

	query = pars_sql(

"	PROCEDURE CONSISTENT_READ_TEST () IS"
"	int1 INT;"
"	int2 INT;"
"	"
"	BEGIN"
"	SELECT COUNT(*), SUM(COL22) INTO int1, int2"
"			FROM TS_TABLE2"
"			CONSISTENT READ;"
"	ASSERT(int2 = 999 * 500);"
"	ASSERT(int1 = 1000);"
"	"
"	"
"	END;"
	);
	
	query->trx = trx2;

	thr = UT_LIST_GET_FIRST(query->thrs);

	mutex_enter(&kernel_mutex);	

	ut_a(thr == que_fork_start_command(query, SESS_COMM_EXECUTE, 0));
	
	mutex_exit(&kernel_mutex);

	oldtm = ut_clock();
	
	que_run_threads(thr);
	
	tm = ut_clock();
	printf("Wall time for consistent read test %lu milliseconds\n",
								tm - oldtm);
	/*------------------------------------------------------*/
	
	query = pars_sql(

"	PROCEDURE ROLLBACK_SPEED_TEST () IS"
"	"
"	BEGIN"
"	ROLLBACK WORK;"
"	END;"
	);
	
	query->trx = trx;

	thr = UT_LIST_GET_FIRST(query->thrs);

	mutex_enter(&kernel_mutex);	

	ut_a(thr == que_fork_start_command(query, SESS_COMM_EXECUTE, 0));
	
	mutex_exit(&kernel_mutex);

	oldtm = ut_clock();
	
	que_run_threads(thr);
	
	tm = ut_clock();
	printf("Wall time for rollback %lu milliseconds\n", tm - oldtm);

	/*------------------------------------------------------*/

	query = pars_sql(

"	PROCEDURE UPDATE_SPEED_TEST () IS"
"	int1 INT;"
"	int2 INT;"
"	"
"	BEGIN"
"	UPDATE TS_TABLE2 SET COL21 = COL21 + 1000, COL22 = COL22 + 1"
"			WHERE COL21 < 1000;"
"	UPDATE TS_TABLE2 SET COL21 = COL21, COL22 = COL22;"
"	UPDATE TS_TABLE2 SET COL21 = COL21, COL22 = COL22;"
"	UPDATE TS_TABLE2 SET COL21 = COL21, COL22 = COL22;"
"	UPDATE TS_TABLE2 SET COL21 = COL21, COL22 = COL22;"
"	UPDATE TS_TABLE2 SET COL21 = COL21, COL22 = COL22;"
"	UPDATE TS_TABLE2 SET COL21 = COL21, COL22 = COL22;"
"	UPDATE TS_TABLE2 SET COL21 = COL21, COL22 = COL22;"
"	UPDATE TS_TABLE2 SET COL21 = COL21, COL22 = COL22;"
"	"
"	SELECT SUM(COL21), SUM(COL22) INTO int1, int2"
"			FROM TS_TABLE2"
"			CONSISTENT READ;"
"	ASSERT(int2 = 1000 + 999 * 500);"
"	ASSERT(int1 = 1000000 + 999 * 500);"
"	UPDATE TS_TABLE2 SET COL21 = COL21 + 1000, COL22 = COL22 + 1"
"			WHERE COL21 < 2000;"
"	UPDATE TS_TABLE2 SET COL21 = COL21 + 1000, COL22 = COL22 + 1"
"			WHERE COL21 < 3000;"
"	COMMIT WORK;"
"	END;"
	);
	
	query->trx = trx;

	thr = UT_LIST_GET_FIRST(query->thrs);

	mutex_enter(&kernel_mutex);	

	ut_a(thr == que_fork_start_command(query, SESS_COMM_EXECUTE, 0));
	
	mutex_exit(&kernel_mutex);

	oldtm = ut_clock();
	
	que_run_threads(thr);
	
	tm = ut_clock();
	printf("Wall time for update test %lu milliseconds\n", tm - oldtm);
	/*------------------------------------------------------*/
	/*------------------------------------------------------*/
	os_thread_sleep(2000000);

	query = pars_sql(

"	PROCEDURE CONSISTENT_READ_TEST () IS"
"	int1 INT;"
"	int2 INT;"
"	"
"	BEGIN"
"	SELECT COUNT(*), SUM(COL22) INTO int1, int2"
"			FROM TS_TABLE2"
"			CONSISTENT READ;"
"	ASSERT(int1 = 1000);"
"	ASSERT(int2 = 999 * 500);"
"	"
"	"
"	END;"
	);
	
	query->trx = trx2;

	thr = UT_LIST_GET_FIRST(query->thrs);

	mutex_enter(&kernel_mutex);	

	ut_a(thr == que_fork_start_command(query, SESS_COMM_EXECUTE, 0));
	
	mutex_exit(&kernel_mutex);

	oldtm = ut_clock();
	
	que_run_threads(thr);
	
	tm = ut_clock();
	printf("Wall time for consistent read test %lu milliseconds\n",
								tm - oldtm);
	/*------------------------------------------------------*/
	/*------------------------------------------------------*/
	
	query = pars_sql(

"	PROCEDURE COMMIT_SPEED_TEST () IS"
"	"
"	BEGIN"
"	COMMIT WORK;"
"	END;"
	);
	
	query->trx = trx;

	thr = UT_LIST_GET_FIRST(query->thrs);

	mutex_enter(&kernel_mutex);	

	ut_a(thr == que_fork_start_command(query, SESS_COMM_EXECUTE, 0));
	
	mutex_exit(&kernel_mutex);

	oldtm = ut_clock();
	
	que_run_threads(thr);
	
	tm = ut_clock();
	printf("Wall time for commit %lu milliseconds\n", tm - oldtm);

/*	dict_table_print_by_name("TS_TABLE2"); */
	/*------------------------------------------------------*/
	/*------------------------------------------------------*/
	os_thread_sleep(2000000);

	query = pars_sql(

"	PROCEDURE CONSISTENT_READ_TEST () IS"
"	int1 INT;"
"	int2 INT;"
"	"
"	BEGIN"
"	SELECT COUNT(*), SUM(COL22) INTO int1, int2"
"			FROM TS_TABLE2"
"			CONSISTENT READ;"
"	ASSERT(int1 = 1000);"
"	ASSERT(int2 = 999 * 500);"
"	"
"	"
"	END;"
	);
	
	query->trx = trx2;

	thr = UT_LIST_GET_FIRST(query->thrs);

	mutex_enter(&kernel_mutex);	

	ut_a(thr == que_fork_start_command(query, SESS_COMM_EXECUTE, 0));
	
	mutex_exit(&kernel_mutex);

	oldtm = ut_clock();
	
	que_run_threads(thr);
	
	tm = ut_clock();
	printf("Wall time for consistent read test %lu milliseconds\n",
								tm - oldtm);
	/*------------------------------------------------------*/
	printf("Will start insert test2\n");
	os_thread_sleep(2000000);

	query = pars_sql(

"	PROCEDURE INSERT_SPEED_TEST2 () IS"
"	int1 INT;"
"	int2 INT;"
"	"
"	BEGIN"
"	INSERT INTO TS_TABLE3 SELECT * FROM TS_TABLE2;"
"	"
"	SELECT COUNT(*) INTO int1"
"			FROM TS_TABLE2;"
"	SELECT COUNT(*) INTO int2"
"			FROM TS_TABLE3;"
"	ASSERT(int1 = int2);"
"	"
"	COMMIT WORK;"
"	"
"	END;"
	);
	
	query->trx = trx;

	thr = UT_LIST_GET_FIRST(query->thrs);

	mutex_enter(&kernel_mutex);	

	ut_a(thr == que_fork_start_command(query, SESS_COMM_EXECUTE, 0));
	
	mutex_exit(&kernel_mutex);

	oldtm = ut_clock();
	
	que_run_threads(thr);
	
	tm = ut_clock();
	printf("Wall time for insert test2 %lu milliseconds\n", tm - oldtm);

/*	sync_print(); */
	
	/*------------------------------------------------------*/
	
	query = pars_sql(

"	PROCEDURE COMMIT_SPEED_TEST () IS"
"	"
"	BEGIN"
"	COMMIT WORK;"
"	END;"
	);
	
	query->trx = trx;

	thr = UT_LIST_GET_FIRST(query->thrs);

	mutex_enter(&kernel_mutex);	

	ut_a(thr == que_fork_start_command(query, SESS_COMM_EXECUTE, 0));
	
	mutex_exit(&kernel_mutex);

	oldtm = ut_clock();
	
	que_run_threads(thr);
	
	tm = ut_clock();
	printf("Wall time for commit %lu milliseconds\n", tm - oldtm);

	/*------------------------------------------------------*/

	query = pars_sql(

"	PROCEDURE JOIN_SPEED_TEST () IS"
"	int1 INT;"
"	"
"	BEGIN"
"	SELECT COUNT(*) INTO int1"
"			FROM TS_TABLE2, TS_TABLE3"
"			WHERE COL21 = COL31;"
"	ASSERT(int1 = 1000);"
"	"
"	COMMIT WORK;"
"	"
"	END;"
	);
	
	query->trx = trx;

	thr = UT_LIST_GET_FIRST(query->thrs);

	mutex_enter(&kernel_mutex);	

	ut_a(thr == que_fork_start_command(query, SESS_COMM_EXECUTE, 0));
	
	mutex_exit(&kernel_mutex);

	oldtm = ut_clock();
	
	que_run_threads(thr);
	
	tm = ut_clock();
	printf("Wall time for join test %lu milliseconds\n", tm - oldtm);

	/*------------------------------------------------------*/
	
	dict_table_print_by_name("TS_TABLE1");
	dict_table_print_by_name("TS_TABLE2");

/*
	dict_table_print_by_name("SYS_TABLES");
	dict_table_print_by_name("SYS_COLUMNS");
	dict_table_print_by_name("SYS_INDEXES");
	dict_table_print_by_name("SYS_FIELDS");
*/
	query = pars_sql(

"	PROCEDURE INSERT_TEST () IS"
"	var1 CHAR;"
"	var2 CHAR;"
"	int1 INT;"
"	int2 INT;"
"	sum1 INT;"
"	finished INT;"
"	rnd_var1 INT;"
"	rnd_var2 INT;"
"	"
"	DECLARE CURSOR cursor2"
"	IS	SELECT COL21, COL22"
"			FROM TS_TABLE2"
"			WHERE COL21 > 5;"
"	"
"	BEGIN"
"	int1 := 0;"
"	WHILE int1 < 10 LOOP"
"		rnd_var1 := int1;"
"		PRINTF('Round '); PRINTF(int1);"
"		INSERT INTO TS_TABLE2 VALUES (int1, rnd_var1,"
"					'123456789012345678901234567890');"
"		SELECT COL22 INTO rnd_var2 FROM TS_TABLE2"
"						WHERE COL21 = int1;"
"		ASSERT(rnd_var1 = rnd_var2);"
"		int1 := int1 + 1;"
"	END LOOP;"
"	"
"	PRINTF('First explicit cursor loop:');"
"	OPEN cursor2;" 
"	finished := 0;"
"	"
"	WHILE finished = 0 LOOP"
"		FETCH cursor2 INTO int1, int2;"
"		IF cursor2 % NOTFOUND THEN"
"			finished := 1;"
"			PRINTF('Loop now finished');"
"		ELSE"
"			PRINTF('Row fetched, values:');"
"			PRINTF(int1); PRINTF(int2);"
"			ASSERT(int1 = int2);"
"			UPDATE TS_TABLE2 SET COL22 = COL22 + 100"
"				WHERE CURRENT OF cursor2;"
"		END IF;"
"	END LOOP;"
"	CLOSE cursor2;"
"	"
"	PRINTF('Second explicit cursor loop:');"
"	OPEN cursor2;" 
"	finished := 0;"
"	"
"	WHILE finished = 0 LOOP"
"		FETCH cursor2 INTO int1, int2;"
"		IF cursor2 % NOTFOUND THEN"
"			finished := 1;"
"		ELSE"
"			PRINTF('Row fetched, values:');"
"			PRINTF(int1); PRINTF(int2);"
"			ASSERT(int1 + 100 = int2);"
"			UPDATE TS_TABLE2 SET COL22 = int2 + 100"
"				WHERE CURRENT OF cursor2;"
"		END IF;"
"	END LOOP;"
"	CLOSE cursor2;"
"	"
"	SELECT COUNT(*), SUM(COL22) INTO int1, sum1"
"			FROM TS_TABLE2;"
"	PRINTF('Now table 2 has this many rows: '); PRINTF(int1);"
"	PRINTF('and the sum of COL22: '); PRINTF(sum1);"
"	"
"	INSERT INTO TS_TABLE3"
"			SELECT COL21, COL22 + 10, COL23 FROM TS_TABLE2;"
"	"
"	SELECT COUNT(*), SUM(COL32) INTO int1, sum1"
"			FROM TS_TABLE2, TS_TABLE3"
"			WHERE COL21 + 2 = COL31;"
"	PRINTF('Join table has this many rows: '); PRINTF(int1);"
"	PRINTF('and the sum of COL32: '); PRINTF(sum1);"
"	"
"	ROLLBACK WORK;"
"	"
"	SELECT COUNT(*), SUM(COL21) INTO int1, sum1 FROM TS_TABLE2;"
"	PRINTF('Now table 2 has this many rows: '); PRINTF(int1);"
"	PRINTF('and the sum of COL21: '); PRINTF(sum1);"
"	"
"	"
"	END;"
	);
	
	query->trx = trx;

	thr = UT_LIST_GET_FIRST(query->thrs);

	mutex_enter(&kernel_mutex);	

	ut_a(thr == que_fork_start_command(query, SESS_COMM_EXECUTE, 0));
	
	mutex_exit(&kernel_mutex);

	que_run_threads(thr);

	dict_table_print_by_name("TS_TABLE1");
	dict_table_print_by_name("TS_TABLE2");

	query = pars_sql(

"	PROCEDURE DELETE_TEST () IS"
"	int1 INT;"
"	sum1 INT;"
"	finished INT;"
"	"
"	DECLARE CURSOR cursor2"
"	IS	SELECT"
"			FROM TS_TABLE2"
"			WHERE COL21 < 10;"
"	"
"	BEGIN"
"	int1 := 0;"
"	WHILE int1 < 10 LOOP"
"		PRINTF('Round '); PRINTF(int1);"
"		INSERT INTO TS_TABLE2 VALUES (int1, int1, TO_CHAR(int1));"
"		int1 := int1 + 1;"
"	END LOOP;"
"	COMMIT WORK;"
"	PRINTF('Delete all the rows:');"
"	OPEN cursor2;" 
"	finished := 0;"
"	"
"	WHILE finished = 0 LOOP"
"		FETCH cursor2 INTO;"
"		IF cursor2 % NOTFOUND THEN"
"			finished := 1;"
"			PRINTF('Loop now finished: all rows deleted');"
"		ELSE"
"			DELETE FROM TS_TABLE2"
"				WHERE CURRENT OF cursor2;"
"		END IF;"
"	END LOOP;"
"	CLOSE cursor2;"
"	"
"	SELECT COUNT(*), SUM(COL22) INTO int1, sum1"
"			FROM TS_TABLE2;"
"	PRINTF('Now table 2 has this many rows, and their sum is: ');"
"	PRINTF(int1); PRINTF(sum1);"
"	ASSERT((int1 = 0) AND (sum1 = 0));"
"	"
"	ROLLBACK WORK;"
"	"
"	SELECT COUNT(*), SUM(COL22) INTO int1, sum1"
"			FROM TS_TABLE2;"
"	"
"	PRINTF(int1); PRINTF(sum1);"
"	ASSERT((int1 = 10) AND (sum1 = 45));"
"	COMMIT WORK;"
"	DELETE FROM TS_TABLE2 WHERE COL22 = 5;"
"	SELECT COUNT(*), SUM(COL22) INTO int1, sum1"
"			FROM TS_TABLE2;"
"	PRINTF(int1); PRINTF(sum1);"
"	ASSERT((int1 = 9) AND (sum1 = 40));"
"	DELETE FROM TS_TABLE2 WHERE COL23 = TO_CHAR(6);"
"	SELECT COUNT(*), SUM(COL22) INTO int1, sum1"
"			FROM TS_TABLE2;"
"	PRINTF(int1);"
"	PRINTF(sum1);"
"	ASSERT((int1 = 8) AND (sum1 = 34));"
"	DELETE FROM TS_TABLE2 WHERE COL23 = TO_CHAR(6);"
"	SELECT COUNT(*), SUM(COL22) INTO int1, sum1"
"			FROM TS_TABLE2;"
"	PRINTF(int1);"
"	PRINTF(sum1);"
"	ASSERT((int1 = 8) AND (sum1 = 34));"
"	COMMIT WORK;"
"	END;"
	);
	
	query->trx = trx;

	thr = UT_LIST_GET_FIRST(query->thrs);

	mutex_enter(&kernel_mutex);	

	ut_a(thr == que_fork_start_command(query, SESS_COMM_EXECUTE, 0));
	
	mutex_exit(&kernel_mutex);

	que_run_threads(thr);

	return(0);
}

#endif

/********************************************************************
Main test function. */

void 
main(void) 
/*======*/
{
	ulint	tm, oldtm;

	oldtm = ut_clock();
	
	test_c(NULL);
	
	tm = ut_clock();

	printf("Wall time for test %lu milliseconds\n", tm - oldtm);
	printf("TESTS COMPLETED SUCCESSFULLY!\n");
}
 
