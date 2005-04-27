# usage: perl TypeInfo.data
# prints template for typeinfo
use strict;
my $position = 0;

#
# odbcsqlgettypeinfo.htm
#
my @typeinfo = (
    {	name => "UNDEF",
	type => q(Undef),
	nullable => q(true),
	position => 0,
    },
#	<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 3.2 Final//EN">
#	<HTML DIR="LTR"><HEAD>
#	<META HTTP-EQUIV="Content-Type" Content="text/html; charset=Windows-1252">
#	<TITLE>SQLGetTypeInfo</TITLE>
#	<SCRIPT SRC="/stylesheets/vs70link.js"></SCRIPT>
#	<SCRIPT SRC="/stylesheets/vs70.js"></SCRIPT>
#	<SCRIPT LANGUAGE="JScript" SRC="/stylesheets/odbc.js"></SCRIPT>
#	</HEAD>
#	<body topmargin=0 id="bodyID">
#	
#	<div id="nsbanner">
#	<div id="bannertitle">
#	<TABLE CLASS="bannerparthead" CELLSPACING=0>
#	<TR ID="hdr">
#	<TD CLASS="bannertitle" nowrap>
#	ODBC Programmer's Reference
#	</TD><TD valign=middle><a href="#Feedback"><IMG name="feedb" onclick=EMailStream(SDKFeedB) style="CURSOR: hand;" hspace=15 alt="" src="/stylesheets/mailto.gif" align=right></a></TD>
#	</TR>
#	</TABLE>
#	</div>
#	</div>
#	<DIV id="nstext" valign="bottom">
#	
#	<H1><A NAME="odbcsqlgettypeinfo"></A>SQLGetTypeInfo</H1>
#	
#	<P class="label"><B>Conformance</B></P>
#	
#	<P>Version Introduced: ODBC 1.0<BR>
#	Standards Compliance: ISO 92</P>
#	
#	<P class="label"><B>Summary</B></P>
#	
#	<P><B>SQLGetTypeInfo</B> returns information about data types supported by the data source. The driver returns the information in the form of an SQL result set. The data types are intended for use in Data Definition Language (DDL) statements.</P>
#	
#	<P class="indent"><b class="le">Important&nbsp;&nbsp;&nbsp;</b>Applications must use the type names returned in the TYPE_NAME column of the <B>SQLGetTypeInfo</B> result set in <B>ALTER TABLE</B> and <B>CREATE TABLE</B> statements. <B>SQLGetTypeInfo</B> may return more than one row with the same value in the DATA_TYPE column.</P>
#	
#	<P class="label"><B>Syntax</B></P>
#	
#	<PRE class="syntax">SQLRETURN <B>SQLGetTypeInfo</B>(
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLHSTMT&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>StatementHandle</I>,
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLSMALLINT&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>DataType</I>);</PRE>
#	
#	<P class="label"><B>Arguments</B>
#	
#	<DL>
#	<DT><I>StatementHandle</I></DT>
#	
#	<DD>[Input]<BR>
#	Statement handle for the result set.</dd>
#	
#	<DT><I>DataType</I></DT>
#	
#	<DD>[Input]<BR>
#	The SQL data type. This must be one of the values in the "<A HREF="odbcsql_data_types.htm">SQL Data Types</A>" section of Appendix D: Data Types, or a driver-specific SQL data type. SQL_ALL_TYPES specifies that information about all data types should be returned.</dd>
#	</DL>
#	
#	<P class="label"><B>Returns</B></P>
#	
#	<P>SQL_SUCCESS, SQL_SUCCESS_WITH_INFO, SQL_STILL_EXECUTING, SQL_ERROR, or SQL_INVALID_HANDLE.</P>
#	
#	<P class="label"><B>Diagnostics</B></P>
#	
#	<P>When <B>SQLGetTypeInfo</B> returns SQL_ERROR or SQL_SUCCESS_WITH_INFO, an associated SQLSTATE value can be obtained by calling <B>SQLGetDiagRec</B> with a <I>HandleType</I> of SQL_HANDLE_STMT and a <I>Handle</I> of <I>StatementHandle</I>. The following table lists the SQLSTATE values commonly returned by <B>SQLGetTypeInfo </B>and explains each one in the context of this function; the notation "(DM)" precedes the descriptions of SQLSTATEs returned by the Driver Manager. The return code associated with each SQLSTATE value is SQL_ERROR, unless noted otherwise.</P>
#	<!--TS:--><div class="tablediv"><table>
#	
#	<TR VALIGN="top">
#	<TH width=22%>SQLSTATE</TH>
#	<TH width=26%>Error</TH>
#	<TH width=52%>Description</TH>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>01000</TD>
#	<TD width=26%>General warning</TD>
#	<TD width=52%>Driver-specific informational message. (Function returns SQL_SUCCESS_WITH_INFO.)</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>01S02</TD>
#	<TD width=26%>Option value changed</TD>
#	<TD width=52%>A specified statement attribute was invalid because of implementation working conditions, so a similar value was temporarily substituted. (Call <B>SQLGetStmtAttr</B> to determine the temporarily substituted value.) The substitute value is valid for the <I>StatementHandle</I> until the cursor is closed. The statement attributes that can be changed are: SQL_ATTR_CONCURRENCY, SQL_ATTR_CURSOR_TYPE, SQL_ATTR_KEYSET_SIZE, SQL_ATTR_MAX_LENGTH, SQL_ATTR_MAX_ROWS, SQL_ATTR_QUERY_TIMEOUT, and SQL_ATTR_SIMULATE_CURSOR. (Function returns SQL_SUCCESS_WITH_INFO.)</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>08S01</TD>
#	<TD width=26%>Communication link failure</TD>
#	<TD width=52%>The communication link between the driver and the data source to which the driver was connected failed before the function completed processing.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>24000</TD>
#	<TD width=26%>Invalid cursor state</TD>
#	<TD width=52%>A cursor was open on the <I>StatementHandle,</I> and <B>SQLFetch</B> or <B>SQLFetchScroll</B> had been called. This error is returned by the Driver Manager if <B>SQLFetch</B> or <B>SQLFetchScroll</B> has not returned SQL_NO_DATA, and is returned by the driver if <B>SQLFetch</B> or <B>SQLFetchScroll</B> has returned SQL_NO_DATA.
#	<P>A result set was open on the <I>StatementHandle</I>, but <B>SQLFetch</B> or <B>SQLFetchScroll</B> had not been called.</P>
#	</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>40001</TD>
#	<TD width=26%>Serialization failure</TD>
#	<TD width=52%>The transaction was rolled back due to a resource deadlock with another transaction.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>40003</TD>
#	<TD width=26%>Statement completion unknown</TD>
#	<TD width=52%>The associated connection failed during the execution of this function and the state of the transaction cannot be determined.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HY000</TD>
#	<TD width=26%>General error</TD>
#	<TD width=52%>An error occurred for which there was no specific SQLSTATE and for which no implementation-specific SQLSTATE was defined. The error message returned by <B>SQLGetDiagRec</B> in the <I>*MessageText</I> buffer describes the error and its cause. </TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HY001</TD>
#	<TD width=26%>Memory allocation error</TD>
#	<TD width=52%>The driver was unable to allocate memory required to support execution or completion of the function.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HY004</TD>
#	<TD width=26%>Invalid SQL data type</TD>
#	<TD width=52%>The value specified for the argument <I>DataType</I> was neither a valid ODBC SQL data type identifier nor a driver-specific data type identifier supported by the driver.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HY008</TD>
#	<TD width=26%>Operation canceled</TD>
#	<TD width=52%>Asynchronous processing was enabled for the <I>StatementHandle</I>, then the function was called and, before it completed execution, <B>SQLCancel</B> was called on the <I>StatementHandle</I>. Then the function was called again on the <I>StatementHandle</I>.
#	<P>The function was called and, before it completed execution, <B>SQLCancel</B> was called on the <I>StatementHandle</I> from a different thread in a multithread application.</P>
#	</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HY010</TD>
#	<TD width=26%>Function sequence error</TD>
#	<TD width=52%>(DM) An asynchronously executing function (not this one) was called for the <I>StatementHandle</I> and was still executing when this function was called.
#	<P>(DM) <B>SQLExecute</B>, <B>SQLExecDirect</B>, <B>SQLBulkOperations</B>, or <B>SQLSetPos</B> was called for the <I>StatementHandle</I> and returned SQL_NEED_DATA. This function was called before data was sent for all data-at-execution parameters or columns.</P>
#	</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HY013</TD>
#	<TD width=26%>Memory management error</TD>
#	<TD width=52%>The function call could not be processed because the underlying memory objects could not be accessed, possibly because of low memory conditions.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HYC00</TD>
#	<TD width=26%>Optional feature not implemented</TD>
#	<TD width=52%>The combination of the current settings of the SQL_ATTR_CONCURRENCY and SQL_ATTR_CURSOR_TYPE statement attributes was not supported by the driver or data source.
#	<P>The SQL_ATTR_USE_BOOKMARKS statement attribute was set to SQL_UB_VARIABLE, and the SQL_ATTR_CURSOR_TYPE statement attribute was set to a cursor type for which the driver does not support bookmarks.</P>
#	</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HYT00</TD>
#	<TD width=26%>Timeout expired</TD>
#	<TD width=52%>The query timeout period expired before the data source returned the result set. The timeout period is set through <B>SQLSetStmtAttr</B>, SQL_ATTR_QUERY_TIMEOUT.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HYT01</TD>
#	<TD width=26%>Connection timeout expired</TD>
#	<TD width=52%>The connection timeout period expired before the data source responded to the request. The connection timeout period is set through <B>SQLSetConnectAttr</B>, SQL_ATTR_CONNECTION_TIMEOUT.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>IM001</TD>
#	<TD width=26%>Driver does not support this function</TD>
#	<TD width=52%>(DM) The driver corresponding to the <I>StatementHandle</I> does not support the function.</TD>
#	</TR>
#	</table></div>
#	<!--TS:-->
#	<P class="label"><B>Comments</B></P>
#	
#	<P><B>SQLGetTypeInfo</B> returns the results as a standard result set, ordered by DATA_TYPE and then by how closely the data type maps to the corresponding ODBC SQL data type. Data types defined by the data source take precedence over user-defined data types. Consequently, the sort order is not necessarily consistent but can be generalized as DATA_TYPE first, followed by TYPE_NAME, both ascending. For example, suppose that a data source defined INTEGER and COUNTER data types, where COUNTER is auto-incrementing, and that a user-defined data type WHOLENUM has also been defined. These would be returned in the order INTEGER, WHOLENUM, and COUNTER, because WHOLENUM maps closely to the ODBC SQL data type SQL_INTEGER, while the auto-incrementing data type, even though supported by the data source, does not map closely to an ODBC SQL data type. For information about how this information might be used, see "<A HREF="odbcddl_statements.htm">DDL Statements</A>" in Chapter 8: SQL Statements.</P>
#	
#	<P>If the <I>DataType</I> argument specifies a data type which is valid for the version of ODBC supported by the driver, but is not supported by the driver, then it will return an empty result set. </P>
#	
#	<P class="indent"><b class="le">Note&nbsp;&nbsp;&nbsp;</b>For more information about the general use, arguments, and returned data of ODBC catalog functions, see <A HREF="odbccatalog_functions.htm">Chapter 7: Catalog Functions</A>.</P>
#	
#	<P>The following columns have been renamed for ODBC 3.<I>x</I>. The column name changes do not affect backward compatibility because applications bind by column number.</P>
#	<!--TS:--><div class="tablediv"><table>
#	
#	<TR VALIGN="top">
#	<TH width=48%>ODBC 2.0 column</TH>
#	<TH width=52%>ODBC 3.<I>x</I> column</TH>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=48%>PRECISION</TD>
#	<TD width=52%>COLUMN_SIZE</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=48%>MONEY</TD>
#	<TD width=52%>FIXED_PREC_SCALE</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=48%>AUTO_INCREMENT</TD>
#	<TD width=52%>AUTO_UNIQUE_VALUE</TD>
#	</TR>
#	</table></div>
#	<!--TS:-->
#	<P>The following columns have been added to the results set returned by <B>SQLGetTypeInfo</B> for ODBC 3.<I>x</I>:
#	
#	<UL type=disc>
#		<LI>SQL_DATA_TYPE</li>
#	
#		<LI>INTERVAL_PRECISION</li>
#	
#		<LI>SQL_DATETIME_SUB</li>
#	
#		<LI>NUM_PREC_RADIX</li>
#	</UL>
#	
#	<P>The following table lists the columns in the result set. Additional columns beyond column 19 (INTERVAL_PRECISION) can be defined by the driver. An application should gain access to driver-specific columns by counting down from the end of the result set rather than specifying an explicit ordinal position. For more information, see "<A HREF="odbcdata_returned_by_catalog_functions.htm">Data Returned by Catalog Functions</A>" in Chapter 7: Catalog Functions.</P>
#	
#	<P class="indent"><b class="le">Note</b>&nbsp;&nbsp;&nbsp;<B>SQLGetTypeInfo</B> might not return all data types. For example, a driver might not return user-defined data types. Applications can use any valid data type, regardless of whether it is returned by <B>SQLGetTypeInfo</B>.</P>
#	
#	<P class="indent">The data types returned by <B>SQLGetTypeInfo</B> are those supported by the data source. They are intended for use in Data Definition Language (DDL) statements. Drivers can return result-set data using data types other than the types returned by <B>SQLGetTypeInfo</B>. In creating the result set for a catalog function, the driver might use a data type that is not supported by the data source. </P>
#	<!--TS:--><div class="tablediv"><table>
#	
#	<TR VALIGN="top">
#	<TH width=33%><BR>
#	Column name</TH>
#	<TH width=14%>Column <BR>
#	number</TH>
#	<TH width=15%><BR>
#	Data type</TH>
#	<TH width=38%><BR>
#	Comments</TH>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=33%>TYPE_NAME<BR>
#	(ODBC 2.0)</TD>
#	<TD width=14%>1</TD>
#	<TD width=15%>Varchar<BR>
#	not NULL</TD>
#	<TD width=38%>Data source&#0150;dependent data-type name; for example, "CHAR()", "VARCHAR()", "MONEY", "LONG VARBINARY", or "CHAR ( ) FOR BIT DATA". Applications must use this name in <B>CREATE TABLE</B> and <B>ALTER TABLE</B> statements.</TD>
#	</TR>
    {	name => "TYPE_NAME",
	type => q(Varchar),
	length => 20,
	nullable => q(false),
	position => ++$position,
    },
#	
#	<TR VALIGN="top">
#	<TD width=33%>DATA_TYPE<BR>
#	(ODBC 2.0)</TD>
#	<TD width=14%>2</TD>
#	<TD width=15%>Smallint<BR>
#	not NULL</TD>
#	<TD width=38%>SQL data type. This can be an ODBC SQL data type or a driver-specific SQL data type. For datetime or interval data types, this column returns the concise data type (such as SQL_TYPE_TIME or SQL_INTERVAL_YEAR_TO_MONTH). For a list of valid ODBC SQL data types, see "<A HREF="odbcsql_data_types.htm">SQL Data Types</A>" in Appendix D: Data Types. For information about driver-specific SQL data types, see the driver’s documentation.</TD>
#	</TR>
    {	name => "DATA_TYPE",
	type => q(Smallint),
	length => undef,
	nullable => q(false),
	position => ++$position,
    },
#	
#	<TR VALIGN="top">
#	<TD width=33%>COLUMN_SIZE<BR>
#	(ODBC 2.0)</TD>
#	<TD width=14%>3</TD>
#	<TD width=15%>Integer</TD>
#	<TD width=38%>The maximum column size that the server supports for this data type. For numeric data, this is the maximum precision. For string data, this is the length in characters. For datetime data types, this is the length in characters of the string representation (assuming the maximum allowed precision of the fractional seconds component). NULL is returned for data types where column size is not applicable. For interval data types, this is the number of characters in the character representation of the interval literal (as defined by the interval leading precision; see "<A HREF="odbcinterval_data_type_length.htm">Interval Data Type Length</A>" in Appendix D: Data Types).
#	<P>For more information on column size, see "<A HREF="odbccolumn_size__decimal_digits__transfer_octet_length__and_display_size.htm">Column Size, Decimal Digits, Transfer Octet Length, and Display Size</A>" in Appendix D: Data Types.</P>
#	</TD>
#	</TR>
    {	name => "COLUMN_SIZE",
	type => q(Integer),
	length => undef,
	nullable => q(true),
	position => ++$position,
    },
#	
#	<TR VALIGN="top">
#	<TD width=33%>LITERAL_PREFIX<BR>
#	(ODBC 2.0)</TD>
#	<TD width=14%>4</TD>
#	<TD width=15%>Varchar</TD>
#	<TD width=38%>Character or characters used to prefix a literal; for example, a single quotation mark (') for character data types or 0x for binary data types; NULL is returned for data types where a literal prefix is not applicable.</TD>
#	</TR>
    {	name => "LITERAL_PREFIX",
	type => q(Varchar),
	length => 1,
	nullable => q(true),
	position => ++$position,
    },
#	
#	<TR VALIGN="top">
#	<TD width=33%>LITERAL_SUFFIX<BR>
#	(ODBC 2.0)</TD>
#	<TD width=14%>5</TD>
#	<TD width=15%>Varchar</TD>
#	<TD width=38%>Character or characters used to terminate a literal; for example, a single quotation mark (') for character data types; NULL is returned for data types where a literal suffix is not applicable.</TD>
#	</TR>
    {	name => "LITERAL_SUFFIX",
	type => q(Varchar),
	length => 1,
	nullable => q(true),
	position => ++$position,
    },
#	
#	<TR VALIGN="top">
#	<TD width=33%>CREATE_PARAMS<BR>
#	(ODBC 2.0)</TD>
#	<TD width=14%>6</TD>
#	<TD width=15%>Varchar</TD>
#	<TD width=38%>A list of keywords, separated by commas, corresponding to each parameter that the application may specify in parentheses when using the name that is returned in the TYPE_NAME field. The keywords in the list can be any of the following: length, precision, or scale. They appear in the order that the syntax requires them to be used. For example, CREATE_PARAMS for DECIMAL would be "precision,scale"; CREATE_PARAMS for VARCHAR would equal "length." NULL is returned if there are no parameters for the data type definition; for example, INTEGER.
#	<P>The driver supplies the CREATE_PARAMS text in the language of the country where it is used.</P>
#	</TD>
#	</TR>
    {	name => "CREATE_PARAMS",
	type => q(Varchar),
	length => 20,
	nullable => q(true),
	position => ++$position,
    },
#	
#	<TR VALIGN="top">
#	<TD width=33%>NULLABLE<BR>
#	(ODBC 2.0)</TD>
#	<TD width=14%>7</TD>
#	<TD width=15%>Smallint<BR>
#	not NULL</TD>
#	<TD width=38%>Whether the data type accepts a NULL value:
#	<P>SQL_NO_NULLS if the data type does not accept NULL values.</P>
#	
#	<P>SQL_NULLABLE if the data type accepts NULL values.</P>
#	
#	<P>SQL_NULLABLE_UNKNOWN if it is not known whether the column accepts NULL values.</P>
#	</TD>
#	</TR>
    {	name => "NULLABLE",
	type => q(Smallint),
	length => undef,
	nullable => q(false),
	position => ++$position,
    },
#	
#	<TR VALIGN="top">
#	<TD width=33%>CASE_SENSITIVE<BR>
#	(ODBC 2.0)</TD>
#	<TD width=14%>8</TD>
#	<TD width=15%>Smallint<BR>
#	not NULL</TD>
#	<TD width=38%>Whether a character data type is case-sensitive in collations and comparisons:
#	<P>SQL_TRUE if the data type is a character data type and is case-sensitive.</P>
#	
#	<P>SQL_FALSE if the data type is not a character data type or is not case-sensitive.</P>
#	</TD>
#	</TR>
    {	name => "CASE_SENSITIVE",
	type => q(Smallint),
	length => undef,
	nullable => q(false),
	position => ++$position,
    },
#	
#	<TR VALIGN="top">
#	<TD width=33%>SEARCHABLE<BR>
#	(ODBC 2.0)</TD>
#	<TD width=14%>9</TD>
#	<TD width=15%>Smallint<BR>
#	not NULL</TD>
#	<TD width=38%>How the data type is used in a <B>WHERE</B> clause:
#	<P>SQL_PRED_NONE if the column cannot be used in a <B>WHERE</B> clause. (This is the same as the SQL_UNSEARCHABLE value in ODBC 2.<I>x</I>.)</P>
#	
#	<P>SQL_PRED_CHAR if the column can be used in a <B>WHERE</B> clause, but only with the <B>LIKE</B> predicate. (This is the same as the SQL_LIKE_ONLY value in ODBC 2.<I>x</I>.)</P>
#	
#	<P>SQL_PRED_BASIC if the column can be used in a <B>WHERE</B> clause with all the comparison operators except <B>LIKE</B> (comparison, quantified comparison, <B>BETWEEN</B>, <B>DISTINCT</B>, <B>IN</B>, <B>MATCH</B>, and <B>UNIQUE</B>). (This is the same as the SQL_ALL_EXCEPT_LIKE value in ODBC 2.<I>x</I>.)</P>
#	
#	<P>SQL_SEARCHABLE if the column can be used in a <B>WHERE</B> clause with any comparison operator.</P>
#	</TD>
#	</TR>
    {	name => "SEARCHABLE",
	type => q(Smallint),
	length => undef,
	nullable => q(false),
	position => ++$position,
    },
#	
#	<TR VALIGN="top">
#	<TD width=33%>UNSIGNED_ATTRIBUTE<BR>
#	(ODBC 2.0)</TD>
#	<TD width=14%>10</TD>
#	<TD width=15%>Smallint</TD>
#	<TD width=38%>Whether the data type is unsigned:
#	<P>SQL_TRUE if the data type is unsigned.</P>
#	
#	<P>SQL_FALSE if the data type is signed.</P>
#	
#	<P>NULL is returned if the attribute is not applicable to the data type or the data type is not numeric.</P>
#	</TD>
#	</TR>
    {	name => "UNSIGNED_ATTRIBUTE",
	type => q(Smallint),
	length => undef,
	nullable => q(true),
	position => ++$position,
    },
#	
#	<TR VALIGN="top">
#	<TD width=33%>FIXED_PREC_SCALE<BR>
#	(ODBC 2.0)</TD>
#	<TD width=14%>11</TD>
#	<TD width=15%>Smallint<BR>
#	not NULL</TD>
#	<TD width=38%>Whether the data type has predefined fixed precision and scale (which are data source&#0150;specific), such as a money data type:
#	<P>SQL_TRUE if it has predefined fixed precision and scale.</P>
#	
#	<P>SQL_FALSE if it does not have predefined fixed precision and scale.</P>
#	</TD>
#	</TR>
    {	name => "FIXED_PREC_SCALE",
	type => q(Smallint),
	length => undef,
	nullable => q(false),
	position => ++$position,
    },
#	
#	<TR VALIGN="top">
#	<TD width=33%>AUTO_UNIQUE_VALUE<BR>
#	(ODBC 2.0)</TD>
#	<TD width=14%>12</TD>
#	<TD width=15%>Smallint</TD>
#	<TD width=38%>Whether the data type is autoincrementing:
#	<P>SQL_TRUE if the data type is autoincrementing.</P>
#	
#	<P>SQL_FALSE if the data type is not autoincrementing.</P>
#	
#	<P>NULL is returned if the attribute is not applicable to the data type or the data type is not numeric.</P>
#	
#	<P>An application can insert values into a column having this attribute, but typically cannot update the values in the column. </P>
#	
#	<P>When an insert is made into an auto-increment column, a unique value is inserted into the column at insert time. The increment is not defined, but is data source&#0150;specific. An application should not assume that an auto-increment column starts at any particular point or increments by any particular value.</P>
#	</TD>
#	</TR>
    {	name => "AUTO_UNIQUE_VALUE",
	type => q(Smallint),
	length => undef,
	nullable => q(true),
	position => ++$position,
    },
#	
#	<TR VALIGN="top">
#	<TD width=33%>LOCAL_TYPE_NAME<BR>
#	(ODBC 2.0)</TD>
#	<TD width=14%>13</TD>
#	<TD width=15%>Varchar</TD>
#	<TD width=38%>Localized version of the data source&#0150;dependent name of the data type. NULL is returned if a localized name is not supported by the data source. This name is intended for display only, such as in dialog boxes.</TD>
#	</TR>
    {	name => "LOCAL_TYPE_NAME",
	type => q(Varchar),
	length => 20,
	nullable => q(true),
	position => ++$position,
    },
#	
#	<TR VALIGN="top">
#	<TD width=33%>MINIMUM_SCALE<BR>
#	(ODBC 2.0)</TD>
#	<TD width=14%>14</TD>
#	<TD width=15%>Smallint</TD>
#	<TD width=38%>The minimum scale of the data type on the data source. If a data type has a fixed scale, the MINIMUM_SCALE and MAXIMUM_SCALE columns both contain this value. For example, an SQL_TYPE_TIMESTAMP column might have a fixed scale for fractional seconds. NULL is returned where scale is not applicable. For more information, see "<A HREF="odbccolumn_size__decimal_digits__transfer_octet_length__and_display_size.htm">Column Size, Decimal Digits, Transfer Octet Length, and Display Size</A>" in Appendix D: Data Types.</TD>
#	</TR>
    {	name => "MINIMUM_SCALE",
	type => q(Smallint),
	length => undef,
	nullable => q(true),
	position => ++$position,
    },
#	
#	<TR VALIGN="top">
#	<TD width=33%>MAXIMUM_SCALE<BR>
#	(ODBC 2.0)</TD>
#	<TD width=14%>15</TD>
#	<TD width=15%>Smallint</TD>
#	<TD width=38%>The maximum scale of the data type on the data source. NULL is returned where scale is not applicable. If the maximum scale is not defined separately on the data source, but is instead defined to be the same as the maximum precision, this column contains the same value as the COLUMN_SIZE column. For more information, see "<A HREF="odbccolumn_size__decimal_digits__transfer_octet_length__and_display_size.htm">Column Size, Decimal Digits, Transfer Octet Length, and Display Size</A>" in Appendix D: Data Types.</TD>
#	</TR>
    {	name => "MAXIMUM_SCALE",
	type => q(Smallint),
	length => undef,
	nullable => q(true),
	position => ++$position,
    },
#	
#	<TR VALIGN="top">
#	<TD width=33%>SQL_DATA_TYPE<BR>
#	(ODBC 3.0)</TD>
#	<TD width=14%>16</TD>
#	<TD width=15%>Smallint NOT NULL</TD>
#	<TD width=38%>The value of the SQL data type as it appears in the SQL_DESC_TYPE field of the descriptor. This column is the same as the DATA_TYPE column, except for interval and datetime data types.
#	<P>For interval and datetime data types, the SQL_DATA_TYPE field in the result set will return SQL_INTERVAL or SQL_DATETIME, and the SQL_DATETIME_SUB field will return the subcode for the specific interval or datetime data type. (See <A HREF="odbcdata_types.htm">Appendix D: Data Types</A>.) </P>
#	</TD>
#	</TR>
    {	name => "SQL_DATA_TYPE",
	type => q(Smallint),
	length => undef,
	nullable => q(false),
	position => ++$position,
    },
#	
#	<TR VALIGN="top">
#	<TD width=33%>SQL_DATETIME_SUB<BR>
#	(ODBC 3.0)</TD>
#	<TD width=14%>17</TD>
#	<TD width=15%>Smallint</TD>
#	<TD width=38%>When the value of SQL_DATA_TYPE is SQL_DATETIME or SQL_INTERVAL, this column contains the datetime/interval subcode. For data types other than datetime and interval, this field is NULL.
#	<P>For interval or datetime data types, the SQL_DATA_TYPE field in the result set will return SQL_INTERVAL or SQL_DATETIME, and the SQL_DATETIME_SUB field will return the subcode for the specific interval or datetime data type. (See <A HREF="odbcdata_types.htm">Appendix D: Data Types</A>.)</P>
#	</TD>
#	</TR>
    {	name => "SQL_DATETIME_SUB",
	type => q(Smallint),
	length => undef,
	nullable => q(true),
	position => ++$position,
    },
#	
#	<TR VALIGN="top">
#	<TD width=33%>NUM_PREC_RADIX<BR>
#	(ODBC 3.0)</TD>
#	<TD width=14%>18</TD>
#	<TD width=15%>Integer</TD>
#	<TD width=38%>If the data type is an approximate numeric type, this column contains the value 2 to indicate that COLUMN_SIZE specifies a number of bits. For exact numeric types, this column contains the value 10 to indicate that COLUMN_SIZE specifies a number of decimal digits. Otherwise, this column is NULL.</TD>
#	</TR>
    {	name => "NUM_PREC_RADIX",
	type => q(Integer),
	length => undef,
	nullable => q(true),
	position => ++$position,
    },
#	
#	<TR VALIGN="top">
#	<TD width=33%>INTERVAL_PRECISION<BR>
#	(ODBC 3.0)</TD>
#	<TD width=14%>19</TD>
#	<TD width=15%>Smallint</TD>
#	<TD width=38%>If the data type is an interval data type, then this column contains the value of the interval leading precision. (See "<A HREF="odbcinterval_data_type_precision.htm">Interval Data Type Precision</A>" in Appendix D: Data Types.) Otherwise, this column is NULL.</TD>
#	</TR>
    {	name => "INTERVAL_PRECISION",
	type => q(Smallint),
	length => undef,
	nullable => q(true),
	position => ++$position,
    },
#	</table></div>
#	<!--TS:-->
#	<P>Attribute information can apply to data types or to specific columns in a result set. <B>SQLGetTypeInfo</B> returns information about attributes associated with data types; <B>SQLColAttribute</B> returns information about attributes associated with columns in a result set.</P>
#	
#	<P class="label"><B>Related Functions</B></P>
#	<!--TS:--><div class="tablediv"><table>
#	
#	<TR VALIGN="top">
#	<TH width=50%>For information about</TH>
#	<TH width=50%>See</TH>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>Binding a buffer to a column in a result set</TD>
#	<TD width=50%><A HREF="odbcsqlbindcol.htm">SQLBindCol</A></TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>Canceling statement processing</TD>
#	<TD width=50%><A HREF="odbcsqlcancel.htm">SQLCancel</A></TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>Returning information about a column in a result set</TD>
#	<TD width=50%><A HREF="odbcsqlcolattribute.htm">SQLColAttribute</A></TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>Fetching a block of data or scrolling through a result set</TD>
#	<TD width=50%><A HREF="odbcsqlfetchscroll.htm">SQLFetchScroll</A></TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>Fetching a single row or a block of data in a forward-only direction</TD>
#	<TD width=50%><A HREF="odbcsqlfetch.htm">SQLFetch</A></TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>Returning information about a driver or data source</TD>
#	<TD width=50%><A HREF="odbcsqlgetinfo.htm">SQLGetInfo</A></TD>
#	</TR>
#	</table></div>
#	<!--TS:--><H4><A NAME="feedback"></A></H4>
#	<SPAN id="SDKFeedB"></SPAN>
#	</div>
#	
#	</BODY>
#	</HTML>
);

my $i4 = " " x 4;
print "// template-begin\n";
print "#define Varchar Char\n";
print "#define Smallint Integer\n";
print "const SqlTypeInfo::Column\nSqlTypeInfo::m_columnList[] = {\n";
for my $p (@typeinfo) {
    print "$i4\{\t$p->{position},\n";
    print "\t\"$p->{name}\",\n";
    my $type = $p->{type};
    if ($p->{position} == 0) {
	print "\tSqlType()\n";
    } elsif (! $p->{length}) {
	print "\tSqlType(SqlType::$type, $p->{nullable})\n";
    } else {
	print "\tSqlType(SqlType::$type, $p->{length}, $p->{nullable})\n";
    }
    my $c = $p == $typeinfo[-1] ? "" : ",";
    print "$i4\}$c\n";
}
print "};\n";
print "#undef Varchar\n";
print "#undef Smallint\n";
print "const unsigned\nSqlTypeInfo::m_columnCount = $position;\n";
print "// template-end\n";

# vim: set sw=4:
