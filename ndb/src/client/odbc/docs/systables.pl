# usage: perl systables.pl {typeinfo|tables|columns|primarykeys} {-l|-c}
use strict;
my $what = shift;
my $opt = shift;
my $listWhat = {};

#
# odbcsqlgettypeinfo.htm
#
$listWhat->{typeinfo} = [
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
    {	name => "type_name",
	type => "varchar",
	length => 20,
	nullable => 0,
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
    {	name => "data_type",
	type => "smallint",
	length => undef,
	nullable => 0,
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
    {	name => "column_size",
	type => "integer",
	length => undef,
	nullable => 1,
    },
#	
#	<TR VALIGN="top">
#	<TD width=33%>LITERAL_PREFIX<BR>
#	(ODBC 2.0)</TD>
#	<TD width=14%>4</TD>
#	<TD width=15%>Varchar</TD>
#	<TD width=38%>Character or characters used to prefix a literal; for example, a single quotation mark (') for character data types or 0x for binary data types; NULL is returned for data types where a literal prefix is not applicable.</TD>
#	</TR>
    {	name => "literal_prefix",
	type => "varchar",
	length => 1,
	nullable => 1,
    },
#	
#	<TR VALIGN="top">
#	<TD width=33%>LITERAL_SUFFIX<BR>
#	(ODBC 2.0)</TD>
#	<TD width=14%>5</TD>
#	<TD width=15%>Varchar</TD>
#	<TD width=38%>Character or characters used to terminate a literal; for example, a single quotation mark (') for character data types; NULL is returned for data types where a literal suffix is not applicable.</TD>
#	</TR>
    {	name => "literal_suffix",
	type => "varchar",
	length => 1,
	nullable => 1,
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
    {	name => "create_params",
	type => "varchar",
	length => 20,
	nullable => 1,
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
    {	name => "nullable",
	type => "smallint",
	length => undef,
	nullable => 0,
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
    {	name => "case_sensitive",
	type => "smallint",
	length => undef,
	nullable => 0,
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
    {	name => "searchable",
	type => "smallint",
	length => undef,
	nullable => 0,
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
    {	name => "unsigned_attribute",
	type => "smallint",
	length => undef,
	nullable => 1,
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
    {	name => "fixed_prec_scale",
	type => "smallint",
	length => undef,
	nullable => 0,
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
    {	name => "auto_unique_value",
	type => "smallint",
	length => undef,
	nullable => 1,
    },
#	
#	<TR VALIGN="top">
#	<TD width=33%>LOCAL_TYPE_NAME<BR>
#	(ODBC 2.0)</TD>
#	<TD width=14%>13</TD>
#	<TD width=15%>Varchar</TD>
#	<TD width=38%>Localized version of the data source&#0150;dependent name of the data type. NULL is returned if a localized name is not supported by the data source. This name is intended for display only, such as in dialog boxes.</TD>
#	</TR>
    {	name => "local_type_name",
	type => "varchar",
	length => 20,
	nullable => 1,
    },
#	
#	<TR VALIGN="top">
#	<TD width=33%>MINIMUM_SCALE<BR>
#	(ODBC 2.0)</TD>
#	<TD width=14%>14</TD>
#	<TD width=15%>Smallint</TD>
#	<TD width=38%>The minimum scale of the data type on the data source. If a data type has a fixed scale, the MINIMUM_SCALE and MAXIMUM_SCALE columns both contain this value. For example, an SQL_TYPE_TIMESTAMP column might have a fixed scale for fractional seconds. NULL is returned where scale is not applicable. For more information, see "<A HREF="odbccolumn_size__decimal_digits__transfer_octet_length__and_display_size.htm">Column Size, Decimal Digits, Transfer Octet Length, and Display Size</A>" in Appendix D: Data Types.</TD>
#	</TR>
    {	name => "minimum_scale",
	type => "smallint",
	length => undef,
	nullable => 1,
    },
#	
#	<TR VALIGN="top">
#	<TD width=33%>MAXIMUM_SCALE<BR>
#	(ODBC 2.0)</TD>
#	<TD width=14%>15</TD>
#	<TD width=15%>Smallint</TD>
#	<TD width=38%>The maximum scale of the data type on the data source. NULL is returned where scale is not applicable. If the maximum scale is not defined separately on the data source, but is instead defined to be the same as the maximum precision, this column contains the same value as the COLUMN_SIZE column. For more information, see "<A HREF="odbccolumn_size__decimal_digits__transfer_octet_length__and_display_size.htm">Column Size, Decimal Digits, Transfer Octet Length, and Display Size</A>" in Appendix D: Data Types.</TD>
#	</TR>
    {	name => "maximum_scale",
	type => "smallint",
	length => undef,
	nullable => 1,
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
    {	name => "sql_data_type",
	type => "smallint",
	length => undef,
	nullable => 0,
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
    {	name => "sql_datetime_sub",
	type => "smallint",
	length => undef,
	nullable => 1,
    },
#	
#	<TR VALIGN="top">
#	<TD width=33%>NUM_PREC_RADIX<BR>
#	(ODBC 3.0)</TD>
#	<TD width=14%>18</TD>
#	<TD width=15%>Integer</TD>
#	<TD width=38%>If the data type is an approximate numeric type, this column contains the value 2 to indicate that COLUMN_SIZE specifies a number of bits. For exact numeric types, this column contains the value 10 to indicate that COLUMN_SIZE specifies a number of decimal digits. Otherwise, this column is NULL.</TD>
#	</TR>
    {	name => "num_prec_radix",
	type => "integer",
	length => undef,
	nullable => 1,
    },
#	
#	<TR VALIGN="top">
#	<TD width=33%>INTERVAL_PRECISION<BR>
#	(ODBC 3.0)</TD>
#	<TD width=14%>19</TD>
#	<TD width=15%>Smallint</TD>
#	<TD width=38%>If the data type is an interval data type, then this column contains the value of the interval leading precision. (See "<A HREF="odbcinterval_data_type_precision.htm">Interval Data Type Precision</A>" in Appendix D: Data Types.) Otherwise, this column is NULL.</TD>
#	</TR>
    {	name => "interval_precision",
	type => "smallint",
	length => undef,
	nullable => 1,
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
];

#
# odbcsqltables.htm
#
$listWhat->{tables} = [
#	<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 3.2 Final//EN">
#	<HTML DIR="LTR"><HEAD>
#	<META HTTP-EQUIV="Content-Type" Content="text/html; charset=Windows-1252">
#	<TITLE>SQLTables</TITLE>
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
#	<H1><A NAME="odbcsqltables"></A>SQLTables</H1>
#	
#	<P class="label"><B>Conformance</B></P>
#	
#	<P>Version Introduced: ODBC 1.0<BR>
#	Standards Compliance: X/Open</P>
#	
#	<P class="label"><B>Summary</B></P>
#	
#	<P><B>SQLTables</B> returns the list of table, catalog, or schema names, and table types, stored in a specific data source. The driver returns the information as a result set.</P>
#	
#	<P class="label"><B>Syntax</B></P>
#	
#	<PRE class="syntax">SQLRETURN <B>SQLTables</B>(
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLHSTMT&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>StatementHandle</I>,
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLCHAR *&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>CatalogName</I>,
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLSMALLINT&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>NameLength1</I>,
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLCHAR *&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>SchemaName</I>,
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLSMALLINT&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>NameLength2</I>,
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLCHAR *&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>TableName</I>,
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLSMALLINT&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>NameLength3</I>,
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLCHAR *&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>TableType</I>,
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLSMALLINT&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>NameLength4</I>);</PRE>
#	
#	<P class="label"><B>Arguments</B>
#	
#	<DL>
#	<DT><I>StatementHandle</I></DT>
#	
#	<DD>[Input]<BR>
#	Statement handle for retrieved results.</dd>
#	
#	<DT><I>CatalogName</I></DT>
#	
#	<DD>[Input]<BR>
#	Catalog name. The <I>CatalogName</I> argument accepts search patterns if the SQL_ODBC_VERSION environment attribute is SQL_OV_ODBC3; it does not accept search patterns if SQL_OV_ODBC2 is set. If a driver supports catalogs for some tables but not for others, such as when a driver retrieves data from different DBMSs, an empty string ("") denotes those tables that do not have catalogs.
#	
#	<P>If the SQL_ATTR_METADATA_ID statement attribute is set to SQL_TRUE, <I>CatalogName</I> is treated as an identifier and its case is not significant. If it is SQL_FALSE, <I>CatalogName</I> is a pattern value argument; it is treated literally, and its case is significant. For more information, see "<A HREF="odbcarguments_in_catalog_functions.htm">Arguments in Catalog Functions</A>" in Chapter 7: Catalog Functions.
#	</dd>
#	
#	<DT><I>NameLength1</I></DT>
#	
#	<DD>[Input]<BR>
#	Length of *<I>CatalogName</I>.</dd>
#	
#	<DT><I>SchemaName</I></DT>
#	
#	<DD>[Input]<BR>
#	String search pattern for schema names. If a driver supports schemas for some tables but not for others, such as when the driver retrieves data from different DBMSs, an empty string ("") denotes those tables that do not have schemas.
#	
#	<P>If the SQL_ATTR_METADATA_ID statement attribute is set to SQL_TRUE, <I>SchemaName</I> is treated as an identifier and its case is not significant. If it is SQL_FALSE, <I>SchemaName</I> is a pattern value argument; it is treated literally, and its case is significant.
#	</dd>
#	
#	<DT><I>NameLength2</I></DT>
#	
#	<DD>[Input]<BR>
#	Length of *<I>SchemaName</I>.</dd>
#	
#	<DT><I>TableName</I></DT>
#	
#	<DD>[Input]<BR>
#	String search pattern for table names.
#	
#	<P>If the SQL_ATTR_METADATA_ID statement attribute is set to SQL_TRUE, <I>TableName</I> is treated as an identifier and its case is not significant. If it is SQL_FALSE, <I>TableName</I> is a pattern value argument; it is treated literally, and its case is significant.
#	</dd>
#	
#	<DT><I>NameLength3</I></DT>
#	
#	<DD>[Input]<BR>
#	Length of *<I>TableName</I>.</dd>
#	
#	<DT><I>TableType</I></DT>
#	
#	<DD>[Input]<BR>
#	List of table types to match. 
#	
#	<P>Note that the SQL_ATTR_METADATA_ID statement attribute has no effect upon the <I>TableType</I> argument. <I>TableType</I> is a value list argument, no matter what the setting of SQL_ATTR_METADATA_ID.
#	</dd>
#	
#	<DT><I>NameLength4</I></DT>
#	
#	<DD>[Input]<BR>
#	Length of *<I>TableType</I>.</dd>
#	</DL>
#	
#	<P class="label"><B>Returns</B></P>
#	
#	<P>SQL_SUCCESS, SQL_SUCCESS_WITH_INFO, SQL_STILL_EXECUTING, SQL_ERROR, or SQL_INVALID_HANDLE.</P>
#	
#	<P class="label"><B>Diagnostics</B></P>
#	
#	<P>When <B>SQLTables</B> returns SQL_ERROR or SQL_SUCCESS_WITH_INFO, an associated SQLSTATE value may be obtained by calling <B>SQLGetDiagRec</B> with a <I>HandleType</I> of SQL_HANDLE_STMT and a <I>Handle</I> of <I>StatementHandle</I>. The following table lists the SQLSTATE values commonly returned by <B>SQLTables</B> and explains each one in the context of this function; the notation "(DM)" precedes the descriptions of SQLSTATEs returned by the Driver Manager. The return code associated with each SQLSTATE value is SQL_ERROR, unless noted otherwise.</P>
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
#	<TD width=22%>08S01</TD>
#	<TD width=26%>Communication link failure</TD>
#	<TD width=52%>The communication link between the driver and the data source to which the driver was connected failed before the function completed processing.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>24000</TD>
#	<TD width=26%>Invalid cursor state</TD>
#	<TD width=52%>A cursor was open on the <I>StatementHandle</I>, and <B>SQLFetch</B> or <B>SQLFetchScroll</B> had been called. This error is returned by the Driver Manager if <B>SQLFetch</B> or <B>SQLFetchScroll</B> has not returned SQL_NO_DATA and is returned by the driver if <B>SQLFetch</B> or <B>SQLFetchScroll</B> has returned SQL_NO_DATA.
#	<P>A cursor was open on the <I>StatementHandle</I>, but <B>SQLFetch</B> or <B>SQLFetchScroll</B> had not been called.</P>
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
#	<TD width=52%>The associated connection failed during the execution of this function, and the state of the transaction cannot be determined.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HY000</TD>
#	<TD width=26%>General error</TD>
#	<TD width=52%>An error occurred for which there was no specific SQLSTATE and for which no implementation-specific SQLSTATE was defined. The error message returned by <B>SQLGetDiagRec</B> in the <I>*MessageText</I> buffer describes the error and its cause.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HY001</TD>
#	<TD width=26%>Memory allocation <BR>
#	error</TD>
#	<TD width=52%>The driver was unable to allocate memory required to support execution or completion of the function.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HY008</TD>
#	<TD width=26%>Operation canceled</TD>
#	<TD width=52%>Asynchronous processing was enabled for the <I>StatementHandle</I>. The function was called, and before it completed execution, <B>SQLCancel</B> was called on the <I>StatementHandle</I>. Then the function was called again on the <I>StatementHandle</I>.
#	<P>The function was called, and before it completed execution, <B>SQLCancel</B> was called on the <I>StatementHandle</I> from a different thread in a multithread application.</P>
#	</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HY009</TD>
#	<TD width=26%>Invalid use of null pointer</TD>
#	<TD width=52%>The SQL_ATTR_METADATA_ID statement attribute was set to SQL_TRUE, the <I>CatalogName</I> argument was a null pointer, and the SQL_CATALOG_NAME <I>InfoType</I> returns that catalog names are supported.
#	<P>(DM) The SQL_ATTR_METADATA_ID statement attribute was set to SQL_TRUE, and the <I>SchemaName</I> or <I>TableName</I> argument was a null pointer.</P>
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
#	<TD width=22%>HY090</TD>
#	<TD width=26%>Invalid string or buffer length</TD>
#	<TD width=52%>(DM) The value of one of the length arguments was less than 0 but not equal to SQL_NTS.
#	<P>The value of one of the name length arguments exceeded the maximum length value for the corresponding name.</P>
#	</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HYC00</TD>
#	<TD width=26%>Optional feature not implemented</TD>
#	<TD width=52%>A catalog was specified, and the driver or data source does not support catalogs.
#	<P>A schema was specified, and the driver or data source does not support schemas.</P>
#	
#	<P>A string search pattern was specified for the catalog name, table schema, or table name, and the data source does not support search patterns for one or more of those arguments.</P>
#	
#	<P>The combination of the current settings of the SQL_ATTR_CONCURRENCY and SQL_ATTR_CURSOR_TYPE statement attributes was not supported by the driver or data source. </P>
#	
#	<P>The SQL_ATTR_USE_BOOKMARKS statement attribute was set to SQL_UB_VARIABLE, and the SQL_ATTR_CURSOR_TYPE statement attribute was set to a cursor type for which the driver does not support bookmarks.</P>
#	</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HYT00</TD>
#	<TD width=26%>Timeout expired</TD>
#	<TD width=52%>The query timeout period expired before the data source returned the requested result set. The timeout period is set through <B>SQLSetStmtAttr</B>, SQL_ATTR_QUERY_TIMEOUT.</TD>
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
#	<TD width=52%>(DM) The driver associated with the <I>StatementHandle</I> does not support the function.</TD>
#	</TR>
#	</table></div>
#	<!--TS:-->
#	<P class="label"><B>Comments</B></P>
#	
#	<P><B>SQLTables</B> lists all tables in the requested range. A user may or may not have SELECT privileges to any of these tables. To check accessibility, an application can:
#	
#	<UL type=disc>
#		<LI>Call <B>SQLGetInfo</B> and check the SQL_ACCESSIBLE_TABLES information type.</li>
#	
#		<LI>Call <B>SQLTablePrivileges</B> to check the privileges for each table.</li>
#	</UL>
#	
#	<P>Otherwise, the application must be able to handle a situation where the user selects a table for which <B>SELECT</B> privileges are not granted.</P>
#	
#	<P>The <I>SchemaName</I> and <I>TableName</I> arguments accept search patterns. The <I>CatalogName</I> argument accepts search patterns if the SQL_ODBC_VERSION environment attribute is SQL_OV_ODBC3; it does not accept search patterns if SQL_OV_ODBC2 is set. If SQL_OV_ODBC3 is set, an ODBC 3<I>.x</I> driver will require that wildcard characters in the <I>CatalogName</I> argument be escaped to be treated literally. For more information about valid search patterns, see "<A HREF="odbcpattern_value_arguments.htm">Pattern Value Arguments</A>" in Chapter 7: Catalog Functions.</P>
#	
#	<P class="indent"><b class="le">Note</b>&nbsp;&nbsp;&nbsp;For more information about the general use, arguments, and returned data of ODBC catalog functions, see <A HREF="odbccatalog_functions.htm">Chapter 7: Catalog Functions</A>.</P>
#	
#	<P>To support enumeration of catalogs, schemas, and table types, the following special semantics are defined for the <I>CatalogName</I>, <I>SchemaName</I>, <I>TableName</I>, and <I>TableType</I> arguments of <B>SQLTables</B>:
#	
#	<UL type=disc>
#		<LI>If <I>CatalogName</I> is SQL_ALL_CATALOGS and <I>SchemaName</I> and <I>TableName</I> are empty strings, the result set contains a list of valid catalogs for the data source. (All columns except the TABLE_CAT column contain NULLs.)</li>
#	
#		<LI>If <I>SchemaName</I> is SQL_ALL_SCHEMAS and <I>CatalogName</I> and <I>TableName</I> are empty strings, the result set contains a list of valid schemas for the data source. (All columns except the TABLE_SCHEM column contain NULLs.)</li>
#	
#		<LI>If <I>TableType</I> is SQL_ALL_TABLE_TYPES and <I>CatalogName</I>, <I>SchemaName</I>, and <I>TableName</I> are empty strings, the result set contains a list of valid table types for the data source. (All columns except the TABLE_TYPE column contain NULLs.)</li>
#	</UL>
#	
#	<P>If <I>TableType</I> is not an empty string, it must contain a list of comma-separated values for the types of interest; each value may be enclosed in single quotation marks (') or unquoted&#0151;for example, 'TABLE', 'VIEW' or TABLE, VIEW. An application should always specify the table type in uppercase; the driver should convert the table type to whatever case is needed by the data source. If the data source does not support a specified table type, <B>SQLTables</B> does not return any results for that type.</P>
#	
#	<P><B>SQLTables</B> returns the results as a standard result set, ordered by TABLE_TYPE, TABLE_CAT, TABLE_SCHEM, and TABLE_NAME. For information about how this information might be used, see "<A HREF="odbcuses_of_catalog_data.htm">Uses of Catalog Data</A>" in Chapter 7: Catalog Functions.</P>
#	
#	<P>To determine the actual lengths of the TABLE_CAT, TABLE_SCHEM, and TABLE_NAME columns, an application can call <B>SQLGetInfo</B> with the SQL_MAX_CATALOG_NAME_LEN, SQL_MAX_SCHEMA_NAME_LEN, and SQL_MAX_TABLE_NAME_LEN information types.</P>
#	
#	<P>The following columns have been renamed for ODBC 3<I>.x</I>. The column name changes do not affect backward compatibility because applications bind by column number.</P>
#	<!--TS:--><div class="tablediv"><table>
#	
#	<TR VALIGN="top">
#	<TH width=48%>ODBC 2.0 column</TH>
#	<TH width=52%>ODBC 3<I>.x</I> column</TH>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=48%>TABLE_QUALIFIER</TD>
#	<TD width=52%>TABLE_CAT</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=48%>TABLE_OWNER</TD>
#	<TD width=52%>TABLE_SCHEM</TD>
#	</TR>
#	</table></div>
#	<!--TS:-->
#	<P>The following table lists the columns in the result set. Additional columns beyond column 5 (REMARKS) can be defined by the driver. An application should gain access to driver-specific columns by counting down from the end of the result set rather than specifying an explicit ordinal position. For more information, see "<A HREF="odbcdata_returned_by_catalog_functions.htm">Data Returned by Catalog Functions</A>" in Chapter 7: Catalog Functions.</P>
#	<!--TS:--><div class="tablediv"><table>
#	
#	<TR VALIGN="top">
#	<TH width=27%><BR>
#	Column name</TH>
#	<TH width=15%>Column number</TH>
#	<TH width=16%><BR>
#	Data type</TH>
#	<TH width=42%><BR>
#	Comments</TH>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=27%>TABLE_CAT<BR>
#	(ODBC 1.0)</TD>
#	<TD width=15%>1</TD>
#	<TD width=16%>Varchar</TD>
#	<TD width=42%>Catalog name; NULL if not applicable to the data source. If a driver supports catalogs for some tables but not for others, such as when the driver retrieves data from different DBMSs, it returns an empty string ("") for those tables that do not have catalogs.</TD>
#	</TR>
    {	name => "table_cat",
	type => "varchar",
	length => 16,
	nullable => 1,
    },
#	
#	<TR VALIGN="top">
#	<TD width=27%>TABLE_SCHEM<BR>
#	(ODBC 1.0)</TD>
#	<TD width=15%>2</TD>
#	<TD width=16%>Varchar</TD>
#	<TD width=42%>Schema name; NULL if not applicable to the data source. If a driver supports schemas for some tables but not for others, such as when the driver retrieves data from different DBMSs, it returns an empty string ("") for those tables that do not have schemas.</TD>
#	</TR>
    {	name => "table_schem",
	type => "varchar",
	length => 16,
	nullable => 1,
    },
#	
#	<TR VALIGN="top">
#	<TD width=27%>TABLE_NAME<BR>
#	(ODBC 1.0)</TD>
#	<TD width=15%>3</TD>
#	<TD width=16%>Varchar</TD>
#	<TD width=42%>Table name.</TD>
#	</TR>
    {	name => "table_name",
	type => "varchar",
	length => 16,
	nullable => 0,
    },
#	
#	<TR VALIGN="top">
#	<TD width=27%>TABLE_TYPE<BR>
#	(ODBC 1.0)</TD>
#	<TD width=15%>4</TD>
#	<TD width=16%>Varchar</TD>
#	<TD width=42%>Table type name; one of the following: "TABLE", "VIEW", "SYSTEM TABLE", "GLOBAL TEMPORARY", "LOCAL TEMPORARY", "ALIAS", "SYNONYM", or a data source&#0150;specific type name.
#	<P>The meanings of "ALIAS" and "SYNONYM" are driver-specific.</P>
#	</TD>
#	</TR>
    {	name => "table_type",
	type => "varchar",
	length => 20,
	nullable => 0,
    },
#	
#	<TR VALIGN="top">
#	<TD width=27%>REMARKS<BR>
#	(ODBC 1.0)</TD>
#	<TD width=15%>5</TD>
#	<TD width=16%>Varchar</TD>
#	<TD width=42%>A description of the table.</TD>
#	</TR>
    {	name => "remarks",
	type => "varchar",
	length => 200,
	nullable => 1,
    },
#	</table></div>
#	<!--TS:-->
#	<P class="label"><B>Code Example</B></P>
#	
#	<P>For a code example of a similar function, see <A HREF="odbcsqlcolumns.htm">SQLColumns</A>.</P>
#	
#	<P class="label"><B>Related Functions</B></P>
#	<!--TS:--><div class="tablediv"><table>
#	
#	<TR VALIGN="top">
#	<TH width=43%>For information about</TH>
#	<TH width=57%>See</TH>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=43%>Binding a buffer to a column in a result set</TD>
#	<TD width=57%><A HREF="odbcsqlbindcol.htm">SQLBindCol</A></TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=43%>Canceling statement processing</TD>
#	<TD width=57%><A HREF="odbcsqlcancel.htm">SQLCancel</A></TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=43%>Returning privileges for a column or columns</TD>
#	<TD width=57%><A HREF="odbcsqlcolumnprivileges.htm">SQLColumnPrivileges</A></TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=43%>Returning the columns in a table or tables</TD>
#	<TD width=57%><A HREF="odbcsqlcolumns.htm">SQLColumns</A></TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=43%>Fetching a single row or a block of data in a forward-only direction</TD>
#	<TD width=57%><A HREF="odbcsqlfetch.htm">SQLFetch</A></TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=43%>Fetching a block of data or scrolling through a result set</TD>
#	<TD width=57%><A HREF="odbcsqlfetchscroll.htm">SQLFetchScroll</A></TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=43%>Returning table statistics and indexes</TD>
#	<TD width=57%><A HREF="odbcsqlstatistics.htm">SQLStatistics</A></TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=43%>Returning privileges for a table or tables</TD>
#	<TD width=57%><A HREF="odbcsqltableprivileges.htm">SQLTablePrivileges</A></TD>
#	</TR>
#	</table></div>
#	<!--TS:--><H4><A NAME="feedback"></A></H4>
#	<SPAN id="SDKFeedB"></SPAN>
#	</div>
#	
#	</BODY>
#	</HTML>
];

#
# odbcsqlcolumns.htm
#
$listWhat->{columns} = [
#	<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 3.2 Final//EN">
#	<HTML DIR="LTR"><HEAD>
#	<META HTTP-EQUIV="Content-Type" Content="text/html; charset=Windows-1252">
#	<TITLE>SQLColumns</TITLE>
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
#	<H1><A NAME="odbcsqlcolumns"></A>SQLColumns</H1>
#	
#	<P class="label"><B>Conformance</B></P>
#	
#	<P>Version Introduced: ODBC 1.0<BR>
#	Standards Compliance: X/Open</P>
#	
#	<P class="label"><B>Summary</B></P>
#	
#	<P><B>SQLColumns</B> returns the list of column names in specified tables. The driver returns this information as a result set on the specified <I>StatementHandle</I>.</P>
#	
#	<P class="label"><B>Syntax</B></P>
#	
#	<PRE class="syntax">SQLRETURN <B>SQLColumns</B>(
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLHSTMT&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>StatementHandle</I>,
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLCHAR *&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>CatalogName</I>,
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLSMALLINT&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>NameLength1</I>,
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLCHAR *&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>SchemaName</I>,
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLSMALLINT&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>NameLength2</I>,
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLCHAR *&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>TableName</I>,
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLSMALLINT&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>NameLength3</I>,
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLCHAR *&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>ColumnName</I>,
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLSMALLINT&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>NameLength4</I>);</PRE>
#	
#	<P class="label"><B>Arguments</B>
#	
#	<DL>
#	<DT><I>StatementHandle</I></DT>
#	
#	<DD>[Input]<BR>
#	Statement handle.</dd>
#	
#	<DT><I>CatalogName</I></DT>
#	
#	<DD>[Input]<BR>
#	Catalog name. If a driver supports catalogs for some tables but not for others, such as when the driver retrieves data from different DBMSs, an empty string ("") denotes those tables that do not have catalogs. <I>CatalogName</I> cannot contain a string search pattern.</dd>
#	</DL>
#	
#	<BLOCKQUOTE>
#	If the SQL_ATTR_METADATA_ID statement attribute is set to SQL_TRUE, <I>CatalogName</I> is treated as an identifier and its case is not significant. If it is SQL_FALSE, <I>CatalogName</I> is an ordinary argument; it is treated literally, and its case is significant. For more information, see "<A HREF="odbcarguments_in_catalog_functions.htm">Arguments in Catalog Functions</A>" in Chapter 7: Catalog Functions.</BLOCKQUOTE>
#	
#	<DL>
#	<DT><I>NameLength1</I></DT>
#	
#	<DD>[Input]<BR>
#	Length of *<I>CatalogName</I>.</dd>
#	
#	<DT><I>SchemaName</I></DT>
#	
#	<DD>[Input]<BR>
#	String search pattern for schema names. If a driver supports schemas for some tables but not for others, such as when the driver retrieves data from different DBMSs, an empty string ("") denotes those tables that do not have schemas.</dd>
#	</DL>
#	
#	<BLOCKQUOTE>
#	If the SQL_ATTR_METADATA_ID statement attribute is set to SQL_TRUE, <I>SchemaName</I> is treated as an identifier and its case is not significant. If it is SQL_FALSE, <I>SchemaName</I> is a pattern value argument; it is treated literally, and its case is significant.</BLOCKQUOTE>
#	
#	<DL>
#	<DT><I>NameLength2</I></DT>
#	
#	<DD>[Input]<BR>
#	Length of *<I>SchemaName</I>.</dd>
#	
#	<DT><I>TableName</I></DT>
#	
#	<DD>[Input]<BR>
#	String search pattern for table names.</dd>
#	</DL>
#	
#	<BLOCKQUOTE>
#	If the SQL_ATTR_METADATA_ID statement attribute is set to SQL_TRUE, <I>TableName</I> is treated as an identifier and its case is not significant. If it is SQL_FALSE, <I>TableName</I> is a pattern value argument; it is treated literally, and its case is significant.</BLOCKQUOTE>
#	
#	<DL>
#	<DT><I>NameLength3</I></DT>
#	
#	<DD>[Input]<BR>
#	Length of *<I>TableName</I>.</dd>
#	
#	<DT><I>ColumnName</I></DT>
#	
#	<DD>[Input]<BR>
#	String search pattern for column names. </dd>
#	</DL>
#	
#	<BLOCKQUOTE>
#	If the SQL_ATTR_METADATA_ID statement attribute is set to SQL_TRUE, <I>ColumnName</I> is treated as an identifier and its case is not significant. If it is SQL_FALSE, <I>ColumnName</I> is a pattern value argument; it is treated literally, and its case is significant.</BLOCKQUOTE>
#	
#	<DL>
#	<DT><I>NameLength4</I></DT>
#	
#	<DD>[Input]<BR>
#	Length of *<I>ColumnName</I>.</dd>
#	</DL>
#	
#	<P class="label"><B>Returns</B></P>
#	
#	<P>SQL_SUCCESS, SQL_SUCCESS_WITH_INFO, SQL_STILL_EXECUTING, SQL_ERROR, or SQL_INVALID_HANDLE.</P>
#	
#	<P class="label"><B>Diagnostics</B></P>
#	
#	<P>When <B>SQLColumns</B> returns SQL_ERROR or SQL_SUCCESS_WITH_INFO, an associated SQLSTATE value may be obtained by calling <B>SQLGetDiagRec</B> with a <I>HandleType</I> of SQL_HANDLE_STMT and a <I>Handle</I> of <I>StatementHandle</I>. The following table lists the SQLSTATE values commonly returned by <B>SQLColumns</B> and explains each one in the context of this function; the notation "(DM)" precedes the descriptions of SQLSTATEs returned by the Driver Manager. The return code associated with each SQLSTATE value is SQL_ERROR, unless noted otherwise.</P>
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
#	<TD width=22%>08S01</TD>
#	<TD width=26%>Communication link failure</TD>
#	<TD width=52%>The communication link between the driver and the data source to which the driver was connected failed before the function completed processing.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>24000</TD>
#	<TD width=26%>Invalid cursor state</TD>
#	<TD width=52%>A cursor was open on the <I>StatementHandle</I>, and <B>SQLFetch</B> or <B>SQLFetchScroll</B> had been called. This error is returned by the Driver Manager if <B>SQLFetch</B> or <B>SQLFetchScroll</B> has not returned SQL_NO_DATA, and is returned by the driver if <B>SQLFetch</B> or <B>SQLFetchScroll</B> has returned SQL_NO_DATA.
#	<P>A cursor was open on the <I>StatementHandle</I> but <B>SQLFetch</B> or <B>SQLFetchScroll</B> had not been called.</P>
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
#	<TD width=52%>The associated connection failed during the execution of this function, and the state of the transaction cannot be determined.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HY000</TD>
#	<TD width=26%>General error</TD>
#	<TD width=52%>An error occurred for which there was no specific SQLSTATE and for which no implementation-specific SQLSTATE was defined. The error message returned by <B>SQLGetDiagRec</B> in the <I>*MessageText</I> buffer describes the error and its cause.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HY001</TD>
#	<TD width=26%>Memory allocation error</TD>
#	<TD width=52%>The driver was unable to allocate memory required to support execution or completion of the function.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HY008</TD>
#	<TD width=26%>Operation canceled</TD>
#	<TD width=52%>Asynchronous processing was enabled for the <I>StatementHandle</I>. The function was called, and before it completed execution, <B>SQLCancel</B> was called on the <I>StatementHandle</I>. Then the function was called again on the <I>StatementHandle</I>.
#	<P>The function was called, and before it completed execution, <B>SQLCancel</B> was called on the <I>StatementHandle</I> from a different thread in a multithread application.</P>
#	</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HY009</TD>
#	<TD width=26%>Invalid use of null pointer</TD>
#	<TD width=52%>The SQL_ATTR_METADATA_ID statement attribute was set to SQL_TRUE, the <I>CatalogName</I> argument was a null pointer, and the SQL_CATALOG_NAME <I>InfoType</I> returns that catalog names are supported.
#	<P>(DM) The SQL_ATTR_METADATA_ID statement attribute was set to SQL_TRUE, and the <I>SchemaName</I>, <I>TableName</I>, or <I>ColumnName</I> argument was a null pointer.</P>
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
#	<TD width=22%>HY090</TD>
#	<TD width=26%>Invalid string or buffer length</TD>
#	<TD width=52%>(DM) The value of one of the name length arguments was less than 0 but not equal to SQL_NTS.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>&nbsp;</TD>
#	<TD width=26%>&nbsp;</TD>
#	<TD width=52%>The value of one of the name length arguments exceeded the maximum length value for the corresponding catalog or name. The maximum length of each catalog or name may be obtained by calling <B>SQLGetInfo</B> with the <I>InfoType</I> values. (See "Comments.")</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HYC00</TD>
#	<TD width=26%>Optional feature not implemented</TD>
#	<TD width=52%>A catalog name was specified, and the driver or data source does not support catalogs.
#	<P>A schema name was specified, and the driver or data source does not support schemas.</P>
#	
#	<P>A string search pattern was specified for the schema name, table name, or column name, and the data source does not support search patterns for one or more of those arguments.</P>
#	
#	<P>The combination of the current settings of the SQL_ATTR_CONCURRENCY and SQL_ATTR_CURSOR_TYPE statement attributes was not supported by the driver or data source. </P>
#	
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
#	<TD width=52%>(DM) The driver associated with the <I>StatementHandle</I> does not support the function.</TD>
#	</TR>
#	</table></div>
#	<!--TS:-->
#	<P class="label"><B>Comments</B></P>
#	
#	<P>This function typically is used before statement execution to retrieve information about columns for a table or tables from the data source's catalog. <B>SQLColumns</B> can be used to retrieve data for all types of items returned by <B>SQLTables</B>. In addition to base tables, this may include (but is not limited to) views, synonyms, system tables, and so on. By contrast, the functions <B>SQLColAttribute</B> and <B>SQLDescribeCol</B> describe the columns in a result set and the function <B>SQLNumResultCols</B> returns the number of columns in a result set. For more information, see "<A HREF="odbcuses_of_catalog_data.htm">Uses of Catalog Data</A>" in Chapter 7: Catalog Functions.</P>
#	
#	<P class="indent"><b class="le">Note</b>&nbsp;&nbsp;&nbsp;For more information about the general use, arguments, and returned data of ODBC catalog functions, see <A HREF="odbccatalog_functions.htm">Chapter 7: Catalog Functions</A>.</P>
#	
#	<P><B>SQLColumns</B> returns the results as a standard result set, ordered by TABLE_CAT, TABLE_SCHEM, TABLE_NAME, and ORDINAL_POSITION. </P>
#	
#	<P class="indent"><b class="le">Note</b>&nbsp;&nbsp;&nbsp;When an application works with an ODBC 2.<I>x</I> driver, no ORDINAL_POSITION column is returned in the result set. As a result, when working with ODBC 2.<I>x</I> drivers, the order of the columns in the column list returned by <B>SQLColumns</B> is not necessarily the same as the order of the columns returned when the application performs a SELECT statement on all columns in that table.</P>
#	
#	<P class="indent"><b class="le">Note</b>&nbsp;&nbsp;&nbsp;<B>SQLColumns</B> might not return all columns. For example, a driver might not return information about pseudo-columns, such as Oracle ROWID. Applications can use any valid column, whether or not it is returned by <B>SQLColumns</B>. </P>
#	
#	<P class="indent">Some columns that can be returned by <B>SQLStatistics</B> are not returned by <B>SQLColumns</B>. For example, <B>SQLColumns</B> does not return the columns in an index created over an expression or filter, such as SALARY + BENEFITS or DEPT = 0012.</P>
#	
#	<P>The lengths of VARCHAR columns are not shown in the table; the actual lengths depend on the data source. To determine the actual lengths of the TABLE_CAT, TABLE_SCHEM, TABLE_NAME, and COLUMN_NAME columns, an application can call <B>SQLGetInfo</B> with the SQL_MAX_CATALOG_NAME_LEN, SQL_MAX_SCHEMA_NAME_LEN, SQL_MAX_TABLE_NAME_LEN, and SQL_MAX_COLUMN_NAME_LEN options.</P>
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
#	<TD width=48%>TABLE_QUALIFIER</TD>
#	<TD width=52%>TABLE_CAT</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=48%>TABLE_OWNER</TD>
#	<TD width=52%>TABLE_SCHEM</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=48%>PRECISION</TD>
#	<TD width=52%>COLUMN_SIZE</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=48%>LENGTH</TD>
#	<TD width=52%>BUFFER_LENGTH</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=48%>SCALE</TD>
#	<TD width=52%>DECIMAL_DIGITS</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=48%>RADIX</TD>
#	<TD width=52%>NUM_PREC_RADIX</TD>
#	</TR>
#	</table></div>
#	<!--TS:-->
#	<P>The following columns have been added to the result set returned by <B>SQLColumns</B> for ODBC 3.<I>x</I>:</P>
#	<!--TS:--><div class="tablediv"><table>
#	
#	<TR VALIGN="top">
#	<TD width=50%>&nbsp;&nbsp;&nbsp;CHAR_OCTET_LENGTH </TD>
#	<TD width=50%>ORDINAL_POSITION</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>&nbsp;&nbsp;&nbsp;COLUMN_DEF</TD>
#	<TD width=50%>SQL_DATA_TYPE</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>&nbsp;&nbsp;&nbsp;IS_NULLABLE </TD>
#	<TD width=50%>SQL_DATETIME_SUB</TD>
#	</TR>
#	</table></div>
#	<!--TS:-->
#	<P>The following table lists the columns in the result set. Additional columns beyond column 18 (IS_NULLABLE) can be defined by the driver. An application should gain access to driver-specific columns by counting down from the end of the result set rather than specifying an explicit ordinal position. For more information, see "<A HREF="odbcdata_returned_by_catalog_functions.htm">Data Returned by Catalog Functions</A>" in Chapter 7: Catalog Functions.</P>
#	<!--TS:--><div class="tablediv"><table>
#	
#	<TR VALIGN="top">
#	<TH width=24%><BR>
#	Column name</TH>
#	<TH width=17%>Column<BR>
#	number</TH>
#	<TH width=16%><BR>
#	Data type</TH>
#	<TH width=43%><BR>
#	Comments</TH>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=24%>TABLE_CAT<BR>
#	(ODBC 1.0)</TD>
#	<TD width=17%>1</TD>
#	<TD width=16%>Varchar</TD>
#	<TD width=43%>Catalog name; NULL if not applicable to the data source. If a driver supports catalogs for some tables but not for others, such as when the driver retrieves data from different DBMSs, it returns an empty string ("") for those tables that do not have catalogs.</TD>
#	</TR>
    {	name => "table_cat",
	type => "varchar",
	length => 16,
	nullable => 1,
    },
#	
#	<TR VALIGN="top">
#	<TD width=24%>TABLE_SCHEM<BR>
#	(ODBC 1.0)</TD>
#	<TD width=17%>2</TD>
#	<TD width=16%>Varchar </TD>
#	<TD width=43%>Schema name; NULL if not applicable to the data source. If a driver supports schemas for some tables but not for others, such as when the driver retrieves data from different DBMSs, it returns an empty string ("") for those tables that do not have schemas.</TD>
#	</TR>
    {	name => "table_schem",
	type => "varchar",
	length => 16,
	nullable => 1,
    },
#	
#	<TR VALIGN="top">
#	<TD width=24%>TABLE_NAME<BR>
#	(ODBC 1.0)</TD>
#	<TD width=17%>3</TD>
#	<TD width=16%>Varchar not NULL</TD>
#	<TD width=43%>Table name.</TD>
#	</TR>
    {	name => "table_name",
	type => "varchar",
	length => 16,
	nullable => 0,
    },
#	
#	<TR VALIGN="top">
#	<TD width=24%>COLUMN_NAME<BR>
#	(ODBC 1.0)</TD>
#	<TD width=17%>4</TD>
#	<TD width=16%>Varchar not NULL</TD>
#	<TD width=43%>Column name. The driver returns an empty string for a column that does not have a name.</TD>
#	</TR>
    {	name => "column_name",
	type => "varchar",
	length => 16,
	nullable => 0,
    },
#	
#	<TR VALIGN="top">
#	<TD width=24%>DATA_TYPE<BR>
#	(ODBC 1.0)</TD>
#	<TD width=17%>5</TD>
#	<TD width=16%>Smallint not NULL</TD>
#	<TD width=43%>SQL data type. This can be an ODBC SQL data type or a driver-specific SQL data type. For datetime and interval data types, this column returns the concise data type (such as SQL_TYPE_DATE or SQL_INTERVAL_YEAR_TO_MONTH, rather than the nonconcise data type such as SQL_DATETIME or SQL_INTERVAL). For a list of valid ODBC SQL data types, see "<A HREF="odbcsql_data_types.htm">SQL Data Types</A>" in Appendix D: Data Types. For information about driver-specific SQL data types, see the driver's documentation.
#	<P>The data types returned for ODBC 3.<I>x</I> and ODBC 2.<I>x</I> applications may be different. For more information, see "<A HREF="odbcbackward_compatibility_and_standards_compliance.htm">Backward Compatibility and Standards Compliance</A>" in Chapter 17: Programming Considerations.</P>
#	</TD>
#	</TR>
    {	name => "data_type",
	type => "smallint",
	length => undef,
	nullable => 0,
    },
#	
#	<TR VALIGN="top">
#	<TD width=24%>TYPE_NAME<BR>
#	(ODBC 1.0)</TD>
#	<TD width=17%>6</TD>
#	<TD width=16%>Varchar not NULL</TD>
#	<TD width=43%>Data source&#0150;dependent data type name; for example, "CHAR", "VARCHAR", "MONEY", "LONG VARBINAR", or "CHAR ( ) FOR BIT DATA".</TD>
#	</TR>
    {	name => "type_name",
	type => "varchar",
	length => 20,
	nullable => 0,
    },
#	
#	<TR VALIGN="top">
#	<TD width=24%>COLUMN_SIZE<BR>
#	(ODBC 1.0) </TD>
#	<TD width=17%>7</TD>
#	<TD width=16%>Integer</TD>
#	<TD width=43%>If DATA_TYPE is SQL_CHAR or SQL_VARCHAR, this column contains the maximum length in characters of the column. For datetime data types, this is the total number of characters required to display the value when converted to characters. For numeric data types, this is either the total number of digits or the total number of bits allowed in the column, according to the NUM_PREC_RADIX column. For interval data types, this is the number of characters in the character representation of the interval literal (as defined by the interval leading precision, see "<A HREF="odbcinterval_data_type_length.htm">Interval Data Type Length</A>" in Appendix D: Data Types). For more information, see "<A HREF="odbccolumn_size__decimal_digits__transfer_octet_length__and_display_size.htm">Column Size, Decimal Digits, Transfer Octet Length, and Display Size</A>" in Appendix D: Data Types.</TD>
#	</TR>
    {	name => "column_size",
	type => "integer",
	length => undef,
	nullable => 1,
    },
#	
#	<TR VALIGN="top">
#	<TD width=24%>BUFFER_LENGTH<BR>
#	(ODBC 1.0)</TD>
#	<TD width=17%>8</TD>
#	<TD width=16%>Integer</TD>
#	<TD width=43%>The length in bytes of data transferred on an SQLGetData, SQLFetch, or SQLFetchScroll operation if SQL_C_DEFAULT is specified. For numeric data, this size may be different than the size of the data stored on the data source. This value might be different than COLUMN_SIZE column for character data. For more information about length, see "<A HREF="odbccolumn_size__decimal_digits__transfer_octet_length__and_display_size.htm">Column Size, Decimal Digits, Transfer Octet Length, and Display Size</A>" in Appendix D: Data Types.</TD>
#	</TR>
    {	name => "buffer_length",
	type => "integer",
	length => 16,
	nullable => 1,
    },
#	
#	<TR VALIGN="top">
#	<TD width=24%>DECIMAL_DIGITS<BR>
#	(ODBC 1.0)</TD>
#	<TD width=17%>9</TD>
#	<TD width=16%>Smallint</TD>
#	<TD width=43%>The total number of significant digits to the right of the decimal point. For SQL_TYPE_TIME and SQL_TYPE_TIMESTAMP, this column contains the number of digits in the fractional seconds component. For the other data types, this is the decimal digits of the column on the data source. For interval data types that contain a time component, this column contains the number of digits to the right of the decimal point (fractional seconds). For interval data types that do not contain a time component, this column is 0. For more information about decimal digits, see "<A HREF="odbccolumn_size__decimal_digits__transfer_octet_length__and_display_size.htm">Column Size, Decimal Digits, Transfer Octet Length, and Display Size</A>" in Appendix D: Data Types. NULL is returned for data types where DECIMAL_DIGITS is not applicable.</TD>
#	</TR>
    {	name => "decimal_digits",
	type => "smallint",
	length => undef,
	nullable => 1,
    },
#	
#	<TR VALIGN="top">
#	<TD width=24%>NUM_PREC_RADIX<BR>
#	(ODBC 1.0)</TD>
#	<TD width=17%>10</TD>
#	<TD width=16%>Smallint</TD>
#	<TD width=43%>For numeric data types, either 10 or 2. If it is 10, the values in COLUMN_SIZE and DECIMAL_DIGITS give the number of decimal digits allowed for the column. For example, a DECIMAL(12,5) column would return a NUM_PREC_RADIX of 10, a COLUMN_SIZE of 12, and a DECIMAL_DIGITS of 5; a FLOAT column could return a NUM_PREC_RADIX of 10, a COLUMN_SIZE of 15, and a DECIMAL_DIGITS of NULL.
#	<P>If it is 2, the values in COLUMN_SIZE and DECIMAL_DIGITS give the number of bits allowed in the column. For example, a FLOAT column could return a RADIX of 2, a COLUMN_SIZE of 53, and a DECIMAL_DIGITS of NULL.</P>
#	
#	<P>NULL is returned for data types where NUM_PREC_RADIX is not applicable.</P>
#	</TD>
#	</TR>
    {	name => "num_prec_radix",
	type => "smallint",
	length => undef,
	nullable => 1,
    },
#	
#	<TR VALIGN="top">
#	<TD width=24%>NULLABLE<BR>
#	(ODBC 1.0)</TD>
#	<TD width=17%>11</TD>
#	<TD width=16%>Smallint not NULL</TD>
#	<TD width=43%>SQL_NO_NULLS if the column could not include NULL values.
#	<P>SQL_NULLABLE if the column accepts NULL values.</P>
#	
#	<P>SQL_NULLABLE_UNKNOWN if it is not known whether the column accepts NULL values.</P>
#	
#	<P>The value returned for this column is different from the value returned for the IS_NULLABLE column. The NULLABLE column indicates with certainty that a column can accept NULLs, but cannot indicate with certainty that a column does not accept NULLs. The IS_NULLABLE column indicates with certainty that a column cannot accept NULLs, but cannot indicate with certainty that a column accepts NULLs.</P>
#	</TD>
#	</TR>
    {	name => "nullable",
	type => "smallint",
	length => 16,
	nullable => 0,
    },
#	
#	<TR VALIGN="top">
#	<TD width=24%>REMARKS<BR>
#	(ODBC 1.0)</TD>
#	<TD width=17%>12</TD>
#	<TD width=16%>Varchar</TD>
#	<TD width=43%>A description of the column.</TD>
#	</TR>
    {	name => "remarks",
	type => "varchar",
	length => 200,
	nullable => 1,
    },
#	
#	<TR VALIGN="top">
#	<TD width=24%>COLUMN_DEF<BR>
#	(ODBC 3.0)</TD>
#	<TD width=17%>13</TD>
#	<TD width=16%>Varchar</TD>
#	<TD width=43%>The default value of the column. The value in this column should be interpreted as a string if it is enclosed in quotation marks.
#	<P>If NULL was specified as the default value, then this column is the word NULL, not enclosed in quotation marks. If the default value cannot be represented without truncation, then this column contains TRUNCATED, with no enclosing single quotation marks. If no default value was specified, then this column is NULL.</P>
#	
#	<P>The value of COLUMN_DEF can be used in generating a new column definition, except when it contains the value TRUNCATED.</P>
#	</TD>
#	</TR>
    {	name => "column_def",
	type => "varchar",
	length => 100,
	nullable => 1,
    },
#	
#	<TR VALIGN="top">
#	<TD width=24%>SQL_DATA_TYPE<BR>
#	(ODBC 3.0)</TD>
#	<TD width=17%>14</TD>
#	<TD width=16%>Smallint not NULL</TD>
#	<TD width=43%>SQL data type, as it appears in the SQL_DESC_TYPE record field in the IRD. This can be an ODBC SQL data type or a driver-specific SQL data type. This column is the same as the DATA_TYPE column, with the exception of datetime and interval data types. This column returns the nonconcise data type (such as SQL_DATETIME or SQL_INTERVAL), rather than the concise data type (such as SQL_TYPE_DATE or SQL_INTERVAL_YEAR_TO_MONTH) for datetime and interval data types. If this column returns SQL_DATETIME or SQL_INTERVAL, the specific data type can be determined from the SQL_DATETIME_SUB column. For a list of valid ODBC SQL data types, see "<A HREF="odbcsql_data_types.htm">SQL Data Types</A>" in Appendix D: Data Types. For information about driver-specific SQL data types, see the driver's documentation.
#	<P>The data types returned for ODBC 3.<I>x</I> and ODBC 2.<I>x</I> applications may be different. For more information, see "<A HREF="odbcbackward_compatibility_and_standards_compliance.htm">Backward Compatibility and Standards Compliance</A>" in Chapter 17: Programming Considerations.</P>
#	</TD>
#	</TR>
    {	name => "sql_data_type",
	type => "smallint",
	length => undef,
	nullable => 0,
    },
#	
#	<TR VALIGN="top">
#	<TD width=24%>SQL_DATETIME_SUB<BR>
#	(ODBC 3.0)</TD>
#	<TD width=17%>15</TD>
#	<TD width=16%>Smallint</TD>
#	<TD width=43%>The subtype code for datetime and interval data types. For other data types, this column returns a NULL. For more information about datetime and interval subcodes, see "SQL_DESC_DATETIME_INTERVAL_CODE" in <A HREF="odbcsqlsetdescfield.htm">SQLSetDescField</A>.</TD>
#	</TR>
    {	name => "sql_datetime_sub",
	type => "smallint",
	length => undef,
	nullable => 1,
    },
#	
#	<TR VALIGN="top">
#	<TD width=24%>CHAR_OCTET_LENGTH<BR>
#	(ODBC 3.0)</TD>
#	<TD width=17%>16</TD>
#	<TD width=16%>Integer</TD>
#	<TD width=43%>The maximum length in bytes of a character or binary data type column. For all other data types, this column returns a NULL.</TD>
#	</TR>
    {	name => "char_octet_length",
	type => "integer",
	length => undef,
	nullable => 1,
    },
#	
#	<TR VALIGN="top">
#	<TD width=24%>ORDINAL_POSITION<BR>
#	(ODBC 3.0)</TD>
#	<TD width=17%>17</TD>
#	<TD width=16%>Integer not NULL</TD>
#	<TD width=43%>The ordinal position of the column in the table. The first column in the table is number 1.</TD>
#	</TR>
    {	name => "ordinal_position",
	type => "integer",
	length => undef,
	nullable => 0,
    },
#	
#	<TR VALIGN="top">
#	<TD width=24%>IS_NULLABLE<BR>
#	(ODBC 3.0)</TD>
#	<TD width=17%>18</TD>
#	<TD width=16%>Varchar</TD>
#	<TD width=43%>"NO" if the column does not include NULLs.
#	<P>"YES" if the column could include NULLs.</P>
#	
#	<P>This column returns a zero-length string if nullability is unknown. </P>
#	
#	<P>ISO rules are followed to determine nullability. An ISO SQL&#0150;compliant DBMS cannot return an empty string. </P>
#	
#	<P>The value returned for this column is different from the value returned for the NULLABLE column. (See the description of the NULLABLE column.)</P>
#	</TD>
#	</TR>
    {	name => "is_nullable",
	type => "varchar",
	length => 3,
	nullable => 1,
    },
#	</table></div>
#	<!--TS:-->
#	<P class="label"><B>Code Example</B></P>
#	
#	<P>In the following example, an application declares buffers for the result set returned by <B>SQLColumns</B>. It calls <B>SQLColumns</B> to return a result set that describes each column in the EMPLOYEE table. It then calls <B>SQLBindCol</B> to bind the columns in the result set to the buffers. Finally, the application fetches each row of data with <B>SQLFetch</B> and processes it.</P>
#	
#	<PRE class="code">#define STR_LEN 128+1
#	#define REM_LEN 254+1
#	
#	/* Declare buffers for result set data */
#	
#	SQLCHAR&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;szCatalog[STR_LEN], szSchema[STR_LEN];
#	SQLCHAR&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;szTableName[STR_LEN], szColumnName[STR_LEN];
#	SQLCHAR&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;szTypeName[STR_LEN], szRemarks[REM_LEN];
#	SQLCHAR&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;szColumnDefault[STR_LEN], szIsNullable[STR_LEN];
#	SQLINTEGER&nbsp;&nbsp;&nbsp;&nbsp;ColumnSize, BufferLength, CharOctetLength, OrdinalPosition;
#	SQLSMALLINT&nbsp;&nbsp;&nbsp;DataType, DecimalDigits, NumPrecRadix, Nullable;
#	SQLSMALLINT&nbsp;&nbsp;&nbsp;SQLDataType, DatetimeSubtypeCode;
#	SQLRETURN&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;retcode;
#	SQLHSTMT&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;hstmt;
#	
#	/* Declare buffers for bytes available to return */
#	
#	SQLINTEGER cbCatalog, cbSchema, cbTableName, cbColumnName;
#	SQLINTEGER cbDataType, cbTypeName, cbColumnSize, cbBufferLength;
#	SQLINTEGER cbDecimalDigits, cbNumPrecRadix, cbNullable, cbRemarks;
#	SQLINTEGER cbColumnDefault, cbSQLDataType, cbDatetimeSubtypeCode, cbCharOctetLength;
#	SQLINTEGER cbOrdinalPosition, cbIsNullable;
#	
#	retcode = SQLColumns(hstmt,
#	         NULL, 0,&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;/* All catalogs */
#	         NULL, 0,&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;/* All schemas */
#	         "CUSTOMERS", SQL_NTS,&nbsp;&nbsp;&nbsp;/* CUSTOMERS table */
#	         NULL, 0);&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;/* All columns */
#	
#	if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
#	
#	   /* Bind columns in result set to buffers */
#	
#	   SQLBindCol(hstmt, 1, SQL_C_CHAR, szCatalog, STR_LEN,&amp;cbCatalog);
#	   SQLBindCol(hstmt, 2, SQL_C_CHAR, szSchema, STR_LEN, &amp;cbSchema);
#	   SQLBindCol(hstmt, 3, SQL_C_CHAR, szTableName, STR_LEN,&amp;cbTableName);
#	   SQLBindCol(hstmt, 4, SQL_C_CHAR, szColumnName, STR_LEN, &amp;cbColumnName);
#	   SQLBindCol(hstmt, 5, SQL_C_SSHORT, &amp;DataType, 0, &amp;cbDataType);
#	   SQLBindCol(hstmt, 6, SQL_C_CHAR, szTypeName, STR_LEN, &amp;cbTypeName);
#	   SQLBindCol(hstmt, 7, SQL_C_SLONG, &amp;ColumnSize, 0, &amp;cbColumnSize);
#	   SQLBindCol(hstmt, 8, SQL_C_SLONG, &amp;BufferLength, 0, &amp;cbBufferLength);
#	   SQLBindCol(hstmt, 9, SQL_C_SSHORT, &amp;DecimalDigits, 0, &amp;cbDecimalDigits);
#	   SQLBindCol(hstmt, 10, SQL_C_SSHORT, &amp;NumPrecRadix, 0, &amp;cbNumPrecRadix);
#	   SQLBindCol(hstmt, 11, SQL_C_SSHORT, &amp;Nullable, 0, &amp;cbNullable);
#	   SQLBindCol(hstmt, 12, SQL_C_CHAR, szRemarks, REM_LEN, &amp;cbRemarks);
#	   SQLBindCol(hstmt, 13, SQL_C_CHAR, szColumnDefault, STR_LEN, &amp;cbColumnDefault);
#	SQLBindCol(hstmt, 14, SQL_C_SSHORT, &amp;SQLDataType, 0, &amp;cbSQLDataType);
#	   SQLBindCol(hstmt, 15, SQL_C_SSHORT, &amp;DatetimeSubtypeCode, 0,
#	      &amp;cbDatetimeSubtypeCode);
#	   SQLBindCol(hstmt, 16, SQL_C_SLONG, &amp;CharOctetLength, 0, &amp;cbCharOctetLength);
#	   SQLBindCol(hstmt, 17, SQL_C_SLONG, &amp;OrdinalPosition, 0, &amp;cbOrdinalPosition);
#	   SQLBindCol(hstmt, 18, SQL_C_CHAR, szIsNullable, STR_LEN, &amp;cbIsNullable);
#	   while(TRUE) {
#	      retcode = SQLFetch(hstmt);
#	      if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) {
#	         show_error( );
#	      }
#	      if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO){
#	            ;   /* Process fetched data */
#	      } else {
#	         break;
#	      }
#	   }
#	}</PRE>
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
#	<TD width=50%>Returning privileges for a column or columns</TD>
#	<TD width=50%><A HREF="odbcsqlcolumnprivileges.htm">SQLColumnPrivileges</A></TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>Fetching a block of data or scrolling through a result set</TD>
#	<TD width=50%><A HREF="odbcsqlfetchscroll.htm">SQLFetchScroll</A></TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>Fetching multiple rows of data</TD>
#	<TD width=50%><A HREF="odbcsqlfetch.htm">SQLFetch</A></TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>Returning columns that uniquely identify a row, or columns automatically updated by a transaction</TD>
#	<TD width=50%><A HREF="odbcsqlspecialcolumns.htm">SQLSpecialColumns</A></TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>Returning table statistics and indexes</TD>
#	<TD width=50%><A HREF="odbcsqlstatistics.htm">SQLStatistics</A></TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>Returning a list of tables in a data source</TD>
#	<TD width=50%><A HREF="odbcsqltables.htm">SQLTables</A></TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>Returning privileges for a table or tables</TD>
#	<TD width=50%><A HREF="odbcsqltableprivileges.htm">SQLTablePrivileges</A></TD>
#	</TR>
#	</table></div>
#	<!--TS:--><H4><A NAME="feedback"></A></H4>
#	<SPAN id="SDKFeedB"></SPAN>
#	</div>
#	
#	</BODY>
#	</HTML>
];

#
# odbcsqlprimarykeys.htm
#
$listWhat->{primarykeys} = [
#	<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 3.2 Final//EN">
#	<HTML DIR="LTR"><HEAD>
#	<META HTTP-EQUIV="Content-Type" Content="text/html; charset=Windows-1252">
#	<TITLE>SQLPrimaryKeys</TITLE>
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
#	<H1><A NAME="odbcsqlprimarykeys"></A>SQLPrimaryKeys</H1>
#	
#	<P class="label"><B>Conformance</B></P>
#	
#	<P>Version Introduced: ODBC 1.0<BR>
#	Standards Compliance: ODBC</P>
#	
#	<P class="label"><B>Summary</B></P>
#	
#	<P><B>SQLPrimaryKeys</B> returns the column names that make up the primary key for a table. The driver returns the information as a result set. This function does not support returning primary keys from multiple tables in a single call.</P>
#	
#	<P class="label"><B>Syntax</B></P>
#	
#	<PRE class="syntax">SQLRETURN <B>SQLPrimaryKeys</B>(
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLHSTMT&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>StatementHandle</I>,
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLCHAR *&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>CatalogName</I>,
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLSMALLINT&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>NameLength1</I>,
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLCHAR *&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>SchemaName</I>,
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLSMALLINT&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>NameLength2</I>,
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLCHAR *&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>TableName</I>,
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLSMALLINT&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>NameLength3</I>);</PRE>
#	
#	<P class="label"><B>Arguments</B>
#	
#	<DL>
#	<DT><I>StatementHandle</I></DT>
#	
#	<DD>[Input]<BR>
#	Statement handle.</dd>
#	
#	<DT><I>CatalogName</I></DT>
#	
#	<DD>[Input]<BR>
#	Catalog name. If a driver supports catalogs for some tables but not for others, such as when the driver retrieves data from different DBMSs, an empty string ("") denotes those tables that do not have catalogs. <I>CatalogName </I>cannot contain a string search pattern.
#	
#	<P>If the SQL_ATTR_METADATA_ID statement attribute is set to SQL_TRUE, <I>CatalogName</I> is treated as an identifier and its case is not significant. If it is SQL_FALSE, <I>CatalogName</I> is an ordinary argument; it is treated literally, and its case is significant. For more information, see "<A HREF="odbcarguments_in_catalog_functions.htm">Arguments in Catalog Functions</A>" in Chapter 7: Catalog Functions.
#	</dd>
#	
#	<DT><I>NameLength1</I></DT>
#	
#	<DD>[Input]<BR>
#	Length in bytes of *<I>CatalogName</I>.</dd>
#	
#	<DT><I>SchemaName</I></DT>
#	
#	<DD>[Input]<BR>
#	Schema name. If a driver supports schemas for some tables but not for others, such as when the driver retrieves data from different DBMSs, an empty string ("") denotes those tables that do not have schemas. <I>SchemaName </I>cannot contain a string search pattern.
#	
#	<P>If the SQL_ATTR_METADATA_ID statement attribute is set to SQL_TRUE, <I>SchemaName</I> is treated as an identifier and its case is not significant. If it is SQL_FALSE, <I>SchemaName</I> is an ordinary argument; it is treated literally, and its case is not significant.
#	</dd>
#	
#	<DT><I>NameLength2</I></DT>
#	
#	<DD>[Input]<BR>
#	Length in bytes of *<I>SchemaName</I>.</dd>
#	
#	<DT><I>TableName</I></DT>
#	
#	<DD>[Input]<BR>
#	Table name. This argument cannot be a null pointer. <I>TableName </I>cannot contain a string search pattern.
#	
#	<P>If the SQL_ATTR_METADATA_ID statement attribute is set to SQL_TRUE, <I>TableName</I> is treated as an identifier and its case is not significant. If it is SQL_FALSE, <I>TableName</I> is an ordinary argument; it is treated literally, and its case is not significant.
#	</dd>
#	
#	<DT><I>NameLength3</I></DT>
#	
#	<DD>[Input]<BR>
#	Length in bytes of *<I>TableName</I>.</dd>
#	</DL>
#	
#	<P class="label"><B>Returns</B></P>
#	
#	<P>SQL_SUCCESS, SQL_SUCCESS_WITH_INFO, SQL_STILL_EXECUTING, SQL_ERROR, or SQL_INVALID_HANDLE.</P>
#	
#	<P class="label"><B>Diagnostics</B></P>
#	
#	<P>When <B>SQLPrimaryKeys</B> returns SQL_ERROR or SQL_SUCCESS_WITH_INFO, an associated SQLSTATE value can be obtained by calling <B>SQLGetDiagRec</B> with a <I>HandleType</I> of SQL_HANDLE_STMT and a <I>Handle</I> of <I>StatementHandle</I>. The following table lists the SQLSTATE values commonly returned by <B>SQLPrimaryKeys</B> and explains each one in the context of this function; the notation "(DM)" precedes the descriptions of SQLSTATEs returned by the Driver Manager. The return code associated with each SQLSTATE value is SQL_ERROR, unless noted otherwise.</P>
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
#	<TD width=22%>08S01</TD>
#	<TD width=26%>Communication link failure</TD>
#	<TD width=52%>The communication link between the driver and the data source to which the driver was connected failed before the function completed processing.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>24000</TD>
#	<TD width=26%>Invalid cursor state</TD>
#	<TD width=52%>(DM) A cursor was open on the <I>StatementHandle</I>, and <B>SQLFetch</B> or <B>SQLFetchScroll</B> had been called.
#	<P>A cursor was open on the <I>StatementHandle</I>, but <B>SQLFetch</B> or <B>SQLFetchScroll</B> had not been called.</P>
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
#	<TD width=52%>The associated connection failed during the execution of this function, and the state of the transaction cannot be determined.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HY000</TD>
#	<TD width=26%>General error</TD>
#	<TD width=52%>An error occurred for which there was no specific SQLSTATE and for which no implementation-specific SQLSTATE was defined. The error message returned by <B>SQLGetDiagRec</B> in the <I>*MessageText</I> buffer describes the error and its cause.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HY001</TD>
#	<TD width=26%>Memory allocation error</TD>
#	<TD width=52%>The driver was unable to allocate memory required to support execution or completion of the function.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HY008</TD>
#	<TD width=26%>Operation canceled</TD>
#	<TD width=52%>Asynchronous processing was enabled for the <I>StatementHandle</I>. The function was called, and before it completed execution, <B>SQLCancel</B> was called on the <I>StatementHandle</I>. Then the function was called again on the <I>StatementHandle</I>.
#	<P>The function was called, and before it completed execution, <B>SQLCancel</B> was called on the <I>StatementHandle</I> from a different thread in a multithread application.</P>
#	</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HY009</TD>
#	<TD width=26%>Invalid use of null pointer</TD>
#	<TD width=52%>(DM) The <I>TableName</I> argument was a null pointer.
#	<P>The SQL_ATTR_METADATA_ID statement attribute was set to SQL_TRUE, the <I>CatalogName</I> argument was a null pointer, and <B>SQLGetInfo</B> with the SQL_CATALOG_NAME information type returns that catalog names are supported.</P>
#	
#	<P>(DM) The SQL_ATTR_METADATA_ID statement attribute was set to SQL_TRUE, and the <I>SchemaName</I> argument was a null pointer.</P>
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
#	<TD width=22%>HY090</TD>
#	<TD width=26%>Invalid string or buffer length</TD>
#	<TD width=52%>(DM) The value of one of the name length arguments was less than 0 but not equal to SQL_NTS, and the associated name argument is not a null pointer.
#	<P>The value of one of the name length arguments exceeded the maximum length value for the corresponding name.</P>
#	</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HYC00</TD>
#	<TD width=26%>Optional feature not implemented</TD>
#	<TD width=52%>A catalog was specified, and the driver or data source does not support catalogs.
#	<P>A schema was specified and the driver or data source does not support schemas.</P>
#	
#	<P>The combination of the current settings of the SQL_ATTR_CONCURRENCY and SQL_ATTR_CURSOR_TYPE statement attributes was not supported by the driver or data source.</P>
#	
#	<P>The SQL_ATTR_USE_BOOKMARKS statement attribute was set to SQL_UB_VARIABLE, and the SQL_ATTR_CURSOR_TYPE statement attribute was set to a cursor type for which the driver does not support bookmarks.</P>
#	</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HYT00</TD>
#	<TD width=26%>Timeout expired</TD>
#	<TD width=52%>The timeout period expired before the data source returned the requested result set. The timeout period is set through <B>SQLSetStmtAttr</B>, SQL_ATTR_QUERY_TIMEOUT.</TD>
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
#	<TD width=52%>(DM) The driver associated with the <I>StatementHandle</I> does not support the function.</TD>
#	</TR>
#	</table></div>
#	<!--TS:-->
#	<P class="label"><B>Comments</B></P>
#	
#	<P><B>SQLPrimaryKeys</B> returns the results as a standard result set, ordered by TABLE_CAT, TABLE_SCHEM, TABLE_NAME, and KEY_SEQ. For information about how this information might be used, see "<A HREF="odbcuses_of_catalog_data.htm">Uses of Catalog Data</A>" in Chapter 7: Catalog Functions.</P>
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
#	<TD width=48%>TABLE_QUALIFIER</TD>
#	<TD width=52%>TABLE_CAT</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=48%>TABLE_OWNER</TD>
#	<TD width=52%>TABLE_SCHEM</TD>
#	</TR>
#	</table></div>
#	<!--TS:-->
#	<P>To determine the actual lengths of the TABLE_CAT, TABLE_SCHEM, TABLE_NAME, and COLUMN_NAME columns, call <B>SQLGetInfo</B> with the SQL_MAX_CATALOG_NAME_LEN, SQL_MAX_SCHEMA_NAME_LEN, SQL_MAX_TABLE_NAME_LEN, and SQL_MAX_COLUMN_NAME_LEN options.</P>
#	
#	<P class="indent"><b class="le">Note</b>&nbsp;&nbsp;&nbsp;For more information about the general use, arguments, and returned data of ODBC catalog functions, see <A HREF="odbccatalog_functions.htm">Chapter 7: Catalog Functions</A>.</P>
#	
#	<P>The following table lists the columns in the result set. Additional columns beyond column 6 (PK_NAME) can be defined by the driver. An application should gain access to driver-specific columns by counting down from the end of the result set rather than specifying an explicit ordinal position. For more information, see "<A HREF="odbcdata_returned_by_catalog_functions.htm">Data Returned by Catalog Functions</A>" in Chapter 7: Catalog Functions.</P>
#	<!--TS:--><div class="tablediv"><table>
#	
#	<TR VALIGN="top">
#	<TH width=27%><BR>
#	Column name</TH>
#	<TH width=15%>Column number</TH>
#	<TH width=16%><BR>
#	Data type</TH>
#	<TH width=42%><BR>
#	Comments</TH>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=27%>TABLE_CAT<BR>
#	(ODBC 1.0)</TD>
#	<TD width=15%>1</TD>
#	<TD width=16%>Varchar</TD>
#	<TD width=42%>Primary key table catalog name; NULL if not applicable to the data source. If a driver supports catalogs for some tables but not for others, such as when the driver retrieves data from different DBMSs, it returns an empty string ("") for those tables that do not have catalogs.</TD>
#	</TR>
    {	name => "table_cat",
	type => "varchar",
	length => 16,
	nullable => 1,
    },
#	
#	<TR VALIGN="top">
#	<TD width=27%>TABLE_SCHEM<BR>
#	(ODBC 1.0)</TD>
#	<TD width=15%>2</TD>
#	<TD width=16%>Varchar</TD>
#	<TD width=42%>Primary key table schema name; NULL if not applicable to the data source. If a driver supports schemas for some tables but not for others, such as when the driver retrieves data from different DBMSs, it returns an empty string ("") for those tables that do not have schemas.</TD>
#	</TR>
    {	name => "table_schem",
	type => "varchar",
	length => 16,
	nullable => 1,
    },
#	
#	<TR VALIGN="top">
#	<TD width=27%>TABLE_NAME<BR>
#	(ODBC 1.0)</TD>
#	<TD width=15%>3</TD>
#	<TD width=16%>Varchar<BR>
#	not NULL</TD>
#	<TD width=42%>Primary key table name.</TD>
#	</TR>
    {	name => "table_name",
	type => "varchar",
	length => 16,
	nullable => 0,
    },
#	
#	<TR VALIGN="top">
#	<TD width=27%>COLUMN_NAME<BR>
#	(ODBC 1.0)</TD>
#	<TD width=15%>4</TD>
#	<TD width=16%>Varchar<BR>
#	not NULL</TD>
#	<TD width=42%>Primary key column name. The driver returns an empty string for a column that does not have a name.</TD>
#	</TR>
    {	name => "column_name",
	type => "varchar",
	length => 16,
	nullable => 0,
    },
#	
#	<TR VALIGN="top">
#	<TD width=27%>KEY_SEQ<BR>
#	(ODBC 1.0)</TD>
#	<TD width=15%>5</TD>
#	<TD width=16%>Smallint<BR>
#	not NULL</TD>
#	<TD width=42%>Column sequence number in key (starting with 1).</TD>
#	</TR>
    {	name => "key_seq",
	type => "smallint",
	length => undef,
	nullable => 0,
    },
#	
#	<TR VALIGN="top">
#	<TD width=27%>PK_NAME<BR>
#	(ODBC 2.0)</TD>
#	<TD width=15%>6</TD>
#	<TD width=16%>Varchar</TD>
#	<TD width=42%>Primary key name. NULL if not applicable to the data source.</TD>
#	</TR>
    {	name => "pk_name",
	type => "varchar",
	length => 16,
	nullable => 1,
    },
#	</table></div>
#	<!--TS:-->
#	<P class="label"><B>Code Example</B></P>
#	
#	<P>See <A HREF="odbcsqlforeignkeys.htm">SQLForeignKeys</A>.</P>
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
#	<TD width=50%>Returning the columns of foreign keys</TD>
#	<TD width=50%><A HREF="odbcsqlforeignkeys.htm">SQLForeignKeys</A></TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>Returning table statistics and indexes</TD>
#	<TD width=50%><A HREF="odbcsqlstatistics.htm">SQLStatistics</A></TD>
#	</TR>
#	</table></div>
#	<!--TS:--><H4><A NAME="feedback"></A></H4>
#	<SPAN id="SDKFeedB"></SPAN>
#	</div>
#	
#	</BODY>
#	</HTML>
];

my $list = $listWhat->{$what} or die "$what?";
my $i4 = " " x 4;
if ($opt eq '-l') {
    print join(", ", map($_->{name}, @$list)), "\n";
    exit;
}
if ($opt eq '-c') {
    my $pos = 0;
    for my $p (@$list) {
	print "${i4}ConnSys::Column::Column(\n";
	$pos++;
	print "\t$pos,\n";
	print "\t\"" . uc($p->{name}) . "\",\n";
	print "\tfalse,\n";
	print "\tNdbType(NdbType::";
	if ($p->{type} eq 'varchar') {
	    print "String, 8, $p->{length}";
	} else {
	    print "Signed, 32, 1";
	}
	print ", " . ($p->{nullable} ? "true" : "false");
	print ")\n";
	print "${i4}),\n";
    }
    exit;
}
print "$opt?\n";

# vim: set sw=4:
