#
use strict;

#
# odbcsqlgetinfo.htm
#
my $info = {
#	<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 3.2 Final//EN">
#	<HTML DIR="LTR"><HEAD>
#	<META HTTP-EQUIV="Content-Type" Content="text/html; charset=Windows-1252">
#	<TITLE>SQLGetInfo</TITLE>
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
#	<H1><A NAME="odbcsqlgetinfo"></A>SQLGetInfo</H1>
#	
#	<P class="label"><B>Conformance</B></P>
#	
#	<P>Version Introduced: ODBC 1.0<BR>
#	Standards Compliance: ISO 92</P>
#	
#	<P class="label"><B>Summary</B></P>
#	
#	<P><B>SQLGetInfo</B> returns general information about the driver and data source associated with a connection.</P>
#	
#	<P class="label"><B>Syntax</B></P>
#	
#	<PRE class="syntax">SQLRETURN <B>SQLGetInfo</B>(
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLHDBC&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>ConnectionHandle</I>,
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLUSMALLINT&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>InfoType</I>,
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLPOINTER&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>InfoValuePtr</I>,
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLSMALLINT&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>BufferLength</I>,
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLSMALLINT *&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>StringLengthPtr</I>);</PRE>
#	
#	<P class="label"><B>Arguments</B>
#	
#	<DL>
#	<DT><I>ConnectionHandle</I></DT>
#	
#	<DD>[Input]<BR>
#	Connection handle.</dd>
#	
#	<DT><I>InfoType</I></DT>
#	
#	<DD>[Input]<BR>
#	Type of information.</dd>
#	
#	<DT><I>InfoValuePtr</I></DT>
#	
#	<DD>[Output]<BR>
#	Pointer to a buffer in which to return the information. Depending on the <I>InfoType</I> requested, the information returned will be one of the following: a null-terminated character string, an SQLUSMALLINT value, an SQLUINTEGER bitmask, an SQLUINTEGER flag, or a SQLUINTEGER binary value.
#	
#	<P>If the <I>InfoType</I> argument is SQL_DRIVER_HDESC or SQL_DRIVER_HSTMT, the <I>InfoValuePtr</I> argument is both input and output. (See the SQL_DRIVER_HDESC or SQL_DRIVER_HSTMT descriptors later in this function description for more information.)
#	</dd>
#	
#	<DT><I>BufferLength</I></DT>
#	
#	<DD>[Input]<BR>
#	Length of the *<I>InfoValuePtr</I> buffer. If the value in <I>*InfoValuePtr</I> is not a character string or if <I>InfoValuePtr</I> is a null pointer,<I> </I>the <I>BufferLength</I> argument is ignored. The driver assumes that the size of <I>*InfoValuePtr</I> is SQLUSMALLINT or SQLUINTEGER, based on the <I>InfoType</I>. If <I>*InfoValuePtr</I> is a Unicode string (when calling <B>SQLGetInfoW</B>), the <I>BufferLength</I> argument must be an even number; if not, SQLSTATE HY090 (Invalid string or buffer length) is returned. </dd>
#	
#	<DT><I>StringLengthPtr</I></DT>
#	
#	<DD>[Output]<BR>
#	Pointer to a buffer in which to return the total number of bytes (excluding the null-termination character for character data) available to return in *<I>InfoValuePtr</I>.
#	
#	<P>For character data, if the number of bytes available to return is greater than or equal to <I>BufferLength</I>, the information in *<I>InfoValuePtr</I> is truncated to <I>BufferLength</I> bytes minus the length of a null-termination character and is null-terminated by the driver. 
#	
#	
#	<P>For all other types of data, the value of <I>BufferLength</I> is ignored and the driver assumes the size of *<I>InfoValuePtr</I> is SQLUSMALLINT or SQLUINTEGER, depending on the <I>InfoType</I>.
#	</dd>
#	</DL>
#	
#	<P class="label"><B>Returns</B></P>
#	
#	<P>SQL_SUCCESS, SQL_SUCCESS_WITH_INFO, SQL_ERROR, or SQL_INVALID_HANDLE.</P>
#	
#	<P class="label"><B>Diagnostics</B></P>
#	
#	<P>When <B>SQLGetInfo</B> returns either SQL_ERROR or SQL_SUCCESS_WITH_INFO, an associated SQLSTATE value can be obtained by calling <B>SQLGetDiagRec</B> with a <I>HandleType</I> of SQL_HANDLE_DBC and a <I>Handle</I> of <I>ConnectionHandle</I>. The following table lists the SQLSTATE values commonly returned by <B>SQLGetInfo</B> and explains each one in the context of this function; the notation "(DM)" precedes the descriptions of SQLSTATEs returned by the Driver Manager. The return code associated with each SQLSTATE value is SQL_ERROR, unless noted otherwise.</P>
#	<!--TS:--><div class="tablediv"><table>
#	
#	<TR VALIGN="top">
#	<TH width=23%>SQLSTATE</TH>
#	<TH width=26%>Error</TH>
#	<TH width=51%>Description</TH>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=23%>01000</TD>
#	<TD width=26%>General warning</TD>
#	<TD width=51%>Driver-specific informational message. (Function returns SQL_SUCCESS_WITH_INFO.)</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=23%>01004</TD>
#	<TD width=26%>String data, right truncated</TD>
#	<TD width=51%>The buffer *<I>InfoValuePtr</I> was not large enough to return all of the requested information, so the information was truncated. The length of the requested information in its untruncated form is returned in *<I>StringLengthPtr</I>. (Function returns SQL_SUCCESS_WITH_INFO.)</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=23%>08003</TD>
#	<TD width=26%>Connection does not exist</TD>
#	<TD width=51%>(DM) The type of information requested in <I>InfoType</I> requires an open connection. Of the information types reserved by ODBC, only SQL_ODBC_VER can be returned without an open connection.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=23%>08S01</TD>
#	<TD width=26%>Communication link failure</TD>
#	<TD width=51%>The communication link between the driver and the data source to which the driver was connected failed before the function completed processing.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=23%>HY000</TD>
#	<TD width=26%>General error</TD>
#	<TD width=51%>An error occurred for which there was no specific SQLSTATE and for which no implementation-specific SQLSTATE was defined. The error message returned by <B>SQLGetDiagRec</B> in the <I>*MessageText</I> buffer describes the error and its cause.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=23%>HY001</TD>
#	<TD width=26%>Memory allocation <BR>
#	error</TD>
#	<TD width=51%>The driver was unable to allocate memory required to support execution or completion of the function.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=23%>HY013</TD>
#	<TD width=26%>Memory management error</TD>
#	<TD width=51%>The function call could not be processed because the underlying memory objects could not be accessed, possibly because of low memory conditions.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=23%>HY024</TD>
#	<TD width=26%>Invalid attribute value</TD>
#	<TD width=51%>(DM) The <I>InfoType</I> argument was SQL_DRIVER_HSTMT, and the value pointed to by <I>InfoValuePtr</I> was not a valid statement handle.
#	<P>(DM) The <I>InfoType</I> argument was SQL_DRIVER_HDESC, and the value pointed to by <I>InfoValuePtr</I> was not a valid descriptor handle.</P>
#	</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=23%>HY090</TD>
#	<TD width=26%>Invalid string or buffer length</TD>
#	<TD width=51%>(DM) The value specified for argument <I>BufferLength</I> was less than 0.
#	<P>(DM) The value specified for <I>BufferLength</I> was an odd number, and <I>*InfoValuePtr </I>was of a Unicode data type.</P>
#	</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=23%>HY096</TD>
#	<TD width=26%>Information type out of range</TD>
#	<TD width=51%>The value specified for the argument <I>InfoType</I> was not valid for the version of ODBC supported by the driver.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=23%>HYC00</TD>
#	<TD width=26%>Optional field not implemented</TD>
#	<TD width=51%>The value specified for the argument <I>InfoType</I> was a driver-specific value that is not supported by the driver.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=23%>HYT01</TD>
#	<TD width=26%>Connection timeout expired</TD>
#	<TD width=51%>The connection timeout period expired before the data source responded to the request. The connection timeout period is set through <B>SQLSetConnectAttr</B>, SQL_ATTR_CONNECTION_TIMEOUT.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=23%>IM001</TD>
#	<TD width=26%>Driver does not support this function</TD>
#	<TD width=51%>(DM) The driver corresponding to the <I>ConnectionHandle</I> does not support the function.</TD>
#	</TR>
#	</table></div>
#	<!--TS:-->
#	<P class="label"><B>Comments</B></P>
#	
#	<P>The currently defined information types are shown in "Information Types," later in this section; it is expected that more will be defined to take advantage of different data sources. A range of information types is reserved by ODBC; driver developers must reserve values for their own driver-specific use from X/Open. <B>SQLGetInfo</B> performs no Unicode conversion or <I>thunking</I> (see <A HREF="odbcodbc_error_codes.htm">Appendix A</A> of the <I>ODBC Programmer's Reference</I>) for driver-defined <I>InfoTypes</I>. For more information, see "<A HREF="odbcdriver_specific_data_types__descriptor_types__information_types.htm">Driver-Specific Data Types, Descriptor Types, Information Types, Diagnostic Types, and Attributes</A>" in Chapter 17: Programming Considerations. The format of the information returned in *<I>InfoValuePtr</I> depends on the <I>InfoType</I> requested. <B>SQLGetInfo</B> will return information in one of five different formats:
#	
#	<UL type=disc>
#		<LI>A null-terminated character string</li>
#	
#		<LI>An SQLUSMALLINT value</li>
#	
#		<LI>An SQLUINTEGER bitmask</li>
#	
#		<LI>An SQLUINTEGER value</li>
#	
#		<LI>A SQLUINTEGER binary value</li>
#	</UL>
#	
#	<P>The format of each of the following information types is noted in the type's description. The application must cast the value returned in *<I>InfoValuePtr</I> accordingly. For an example of how an application could retrieve data from a SQLUINTEGER bitmask, see "Code Example."</P>
#	
#	<P>A driver must return a value for each of the information types defined in the tables below. If an information type does not apply to the driver or data source, the driver returns one of the values listed in the following table.</P>
#	<!--TS:--><div class="tablediv"><table>
#	
#	<TR VALIGN="top">
#	<TH width=49%>Format of *<I>InfoValuePtr</I></TH>
#	<TH width=51%>Returned value</TH>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>Character string ("Y" or "N")</TD>
#	<TD width=51%>"N"</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>Character string (not "Y" or "N")</TD>
#	<TD width=51%>Empty string</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQLUSMALLINT</TD>
#	<TD width=51%>0</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQLUINTEGER bitmask or SQLUINTEGER binary value</TD>
#	<TD width=51%>0L</TD>
#	</TR>
#	</table></div>
#	<!--TS:-->
#	<P>For example, if a data source does not support procedures, <B>SQLGetInfo</B> returns the values listed in the following table for the values of <I>InfoType</I> that are related to procedures.</P>
#	<!--TS:--><div class="tablediv"><table>
#	
#	<TR VALIGN="top">
#	<TH width=49%><I>InfoType</I></TH>
#	<TH width=51%>Returned value</TH>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_PROCEDURES</TD>
#	<TD width=51%>"N"</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_ACCESSIBLE_PROCEDURES</TD>
#	<TD width=51%>"N"</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_MAX_PROCEDURE_NAME_LEN</TD>
#	<TD width=51%>0</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_PROCEDURE_TERM</TD>
#	<TD width=51%>Empty string</TD>
#	</TR>
#	</table></div>
#	<!--TS:-->
#	<P><B>SQLGetInfo</B> returns SQLSTATE HY096 (Invalid argument value) for values of <I>InfoType</I> that are in the range of information types reserved for use by ODBC but are not defined by the version of ODBC supported by the driver. To determine what version of ODBC a driver conforms to, an application calls <B>SQLGetInfo</B> with the SQL_DRIVER_ODBC_VER information type. <B>SQLGetInfo</B> returns SQLSTATE HYC00 (Optional feature not implemented) for values of <I>InfoType</I> that are in the range of information types reserved for driver-specific use but are not supported by the driver.</P>
#	
#	<P>All calls to <B>SQLGetInfo</B> require an open connection, except when the <I>InfoType</I> is SQL_ODBC_VER, which returns the version of the Driver Manager.</P>
#	
#	<H1>Information Types</H1>
#	
#	<P>This section lists the information types supported by <B>SQLGetInfo</B>. Information types are grouped categorically and listed alphabetically. Information types that were added or renamed for ODBC 3<I>.x</I> are also listed.</P>
#	
#	<H2>Driver Information</H2>
#	
#	<P>The following values of the <I>InfoType</I> argument return information about the ODBC driver, such as the number of active statements, the data source name, and the interface standards compliance level:</P>
#	<!--TS:--><div class="tablediv"><table>
#	
#	<TR VALIGN="top">
#	<TD width=43%>SQL_ACTIVE_ENVIRONMENTS</TD>
#	<TD width=57%>SQL_GETDATA_EXTENSIONS</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=43%>SQL_ASYNC_MODE</TD>
#	<TD width=57%>SQL_INFO_SCHEMA_VIEWS</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=43%>SQL_BATCH_ROW_COUNT</TD>
#	<TD width=57%>SQL_KEYSET_CURSOR_ATTRIBUTES1</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=43%>SQL_BATCH_SUPPORT</TD>
#	<TD width=57%>SQL_KEYSET_CURSOR_ATTRIBUTES2</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=43%>SQL_DATA_SOURCE_NAME</TD>
#	<TD width=57%>SQL_MAX_ASYNC_CONCURRENT_STATEMENTS</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=43%>SQL_DRIVER_HDBC</TD>
#	<TD width=57%>SQL_MAX_CONCURRENT_ACTIVITIES</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=43%>SQL_DRIVER_HDESC</TD>
#	<TD width=57%>SQL_MAX_DRIVER_CONNECTIONS</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=43%>SQL_DRIVER_HENV</TD>
#	<TD width=57%>SQL_ODBC_INTERFACE_CONFORMANCE</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=43%>SQL_DRIVER_HLIB</TD>
#	<TD width=57%>SQL_ODBC_STANDARD_CLI_CONFORMANCE</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=43%>SQL_DRIVER_HSTMT</TD>
#	<TD width=57%>SQL_ODBC_VER</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=43%>SQL_DRIVER_NAME</TD>
#	<TD width=57%>SQL_PARAM_ARRAY_ROW_COUNTS</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=43%>SQL_DRIVER_ODBC_VER</TD>
#	<TD width=57%>SQL_PARAM_ARRAY_SELECTS</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=43%>SQL_DRIVER_VER</TD>
#	<TD width=57%>SQL_ROW_UPDATES</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=43%>SQL_DYNAMIC_CURSOR_ATTRIBUTES1</TD>
#	<TD width=57%>SQL_SEARCH_PATTERN_ESCAPE</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=43%>SQL_DYNAMIC_CURSOR_ATTRIBUTES2</TD>
#	<TD width=57%>SQL_SERVER_NAME</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=43%>SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES1</TD>
#	<TD width=57%>SQL_STATIC_CURSOR_ATTRIBUTES1</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=43%>SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES2</TD>
#	<TD width=57%>SQL_STATIC_CURSOR_ATTRIBUTES2</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=43%>SQL_FILE_USAGE</TD>
#	<TD width=57%>&nbsp;</TD>
#	</TR>
#	</table></div>
#	<!--TS:-->
#	<H2>DBMS Product Information</H2>
#	
#	<P>The following values of the <I>InfoType</I> argument return information about the DBMS product, such as the DBMS name and version:</P>
#	
#	<P>SQL_DATABASE_NAME<BR>
#	SQL_DBMS_NAME<BR>
#	SQL_DBMS_VER</P>
#	
#	<H2>Data Source Information</H2>
#	
#	<P>The following values of the <I>InfoType</I> argument return information about the data source, such as cursor characteristics and transaction capabilities:</P>
#	<!--TS:--><div class="tablediv"><table>
#	
#	<TR VALIGN="top">
#	<TD width=54%>SQL_ACCESSIBLE_PROCEDURES</TD>
#	<TD width=46%>SQL_MULT_RESULT_SETS</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=54%>SQL_ACCESSIBLE_TABLES</TD>
#	<TD width=46%>SQL_MULTIPLE_ACTIVE_TXN</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=54%>SQL_BOOKMARK_PERSISTENCE</TD>
#	<TD width=46%>SQL_NEED_LONG_DATA_LEN</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=54%>SQL_CATALOG_TERM</TD>
#	<TD width=46%>SQL_NULL_COLLATION</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=54%>SQL_COLLATION_SEQ</TD>
#	<TD width=46%>SQL_PROCEDURE_TERM</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=54%>SQL_CONCAT_NULL_BEHAVIOR</TD>
#	<TD width=46%>SQL_SCHEMA_TERM</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=54%>SQL_CURSOR_COMMIT_BEHAVIOR</TD>
#	<TD width=46%>SQL_SCROLL_OPTIONS</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=54%>SQL_CURSOR_ROLLBACK_BEHAVIOR</TD>
#	<TD width=46%>SQL_TABLE_TERM</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=54%>SQL_CURSOR_SENSITIVITY</TD>
#	<TD width=46%>SQL_TXN_CAPABLE</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=54%>SQL_DATA_SOURCE_READ_ONLY</TD>
#	<TD width=46%>SQL_TXN_ISOLATION_OPTION</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=54%>SQL_DEFAULT_TXN_ISOLATION</TD>
#	<TD width=46%>SQL_USER_NAME</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=54%>SQL_DESCRIBE_PARAMETER</TD>
#	<TD width=46%>&nbsp;</TD>
#	</TR>
#	</table></div>
#	<!--TS:-->
#	<H2>Supported SQL</H2>
#	
#	<P>The following values of the <I>InfoType</I> argument return information about the SQL statements supported by the data source. The SQL syntax of each feature described by these information types is the SQL-92 syntax. These information types do not exhaustively describe the entire SQL-92 grammar. Instead, they describe those parts of the grammar for which data sources commonly offer different levels of support. Specifically, most of the DDL statements in SQL-92 are covered.</P>
#	
#	<P>Applications should determine the general level of supported grammar from the SQL_SQL_CONFORMANCE information type and use the other information types to determine variations from the stated standards compliance level.</P>
#	<!--TS:--><div class="tablediv"><table>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_AGGREGATE_FUNCTIONS</TD>
#	<TD width=51%>SQL_DROP_TABLE</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_ALTER_DOMAIN</TD>
#	<TD width=51%>SQL_DROP_TRANSLATION</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_ALTER_SCHEMA</TD>
#	<TD width=51%>SQL_DROP_VIEW</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_ALTER_TABLE</TD>
#	<TD width=51%>SQL_EXPRESSIONS_IN_ORDERBY</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_ANSI_SQL_DATETIME_LITERALS</TD>
#	<TD width=51%>SQL_GROUP_BY</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_CATALOG_LOCATION </TD>
#	<TD width=51%>SQL_IDENTIFIER_CASE</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_CATALOG_NAME</TD>
#	<TD width=51%>SQL_IDENTIFIER_QUOTE_CHAR</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_CATALOG_NAME_SEPARATOR</TD>
#	<TD width=51%>SQL_INDEX_KEYWORDS</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_CATALOG_USAGE</TD>
#	<TD width=51%>SQL_INSERT_STATEMENT</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_COLUMN_ALIAS</TD>
#	<TD width=51%>SQL_INTEGRITY</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_CORRELATION_NAME</TD>
#	<TD width=51%>SQL_KEYWORDS</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_CREATE_ASSERTION</TD>
#	<TD width=51%>SQL_LIKE_ESCAPE_CLAUSE</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_CREATE_CHARACTER_SET</TD>
#	<TD width=51%>SQL_NON_NULLABLE_COLUMNS</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_CREATE_COLLATION</TD>
#	<TD width=51%>SQL_SQL_CONFORMANCE</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_CREATE_DOMAIN</TD>
#	<TD width=51%>SQL_OJ_CAPABILITIES</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_CREATE_SCHEMA</TD>
#	<TD width=51%>SQL_ORDER_BY_COLUMNS_IN_SELECT</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_CREATE_TABLE</TD>
#	<TD width=51%>SQL_OUTER_JOINS</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_CREATE_TRANSLATION</TD>
#	<TD width=51%>SQL_PROCEDURES</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_DDL_INDEX</TD>
#	<TD width=51%>SQL_QUOTED_IDENTIFIER_CASE</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_DROP_ASSERTION</TD>
#	<TD width=51%>SQL_SCHEMA_USAGE</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_DROP_CHARACTER_SET</TD>
#	<TD width=51%>SQL_SPECIAL_CHARACTERS</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_DROP_COLLATION</TD>
#	<TD width=51%>SQL_SUBQUERIES</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_DROP_DOMAIN</TD>
#	<TD width=51%>SQL_UNION</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_DROP_SCHEMA</TD>
#	<TD width=51%>&nbsp;</TD>
#	</TR>
#	</table></div>
#	<!--TS:-->
#	<P class="label"><B>SQL Limits</B></P>
#	
#	<P>The following values of the <I>InfoType</I> argument return information about the limits applied to identifiers and clauses in SQL statements, such as the maximum lengths of identifiers and the maximum number of columns in a select list. Limitations can be imposed by either the driver or the data source.</P>
#	<!--TS:--><div class="tablediv"><table>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_MAX_BINARY_LITERAL_LEN</TD>
#	<TD width=51%>SQL_MAX_IDENTIFIER_LEN</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_MAX_CATALOG_NAME_LEN</TD>
#	<TD width=51%>SQL_MAX_INDEX_SIZE</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_MAX_CHAR_LITERAL_LEN</TD>
#	<TD width=51%>SQL_MAX_PROCEDURE_NAME_LEN</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_MAX_COLUMN_NAME_LEN</TD>
#	<TD width=51%>SQL_MAX_ROW_SIZE</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_MAX_COLUMNS_IN_GROUP_BY</TD>
#	<TD width=51%>SQL_MAX_ROW_SIZE_INCLUDES_LONG</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_MAX_COLUMNS_IN_INDEX</TD>
#	<TD width=51%>SQL_MAX_SCHEMA_NAME_LEN</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_MAX_COLUMNS_IN_ORDER_BY</TD>
#	<TD width=51%>SQL_MAX_STATEMENT_LEN</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_MAX_COLUMNS_IN_SELECT</TD>
#	<TD width=51%>SQL_MAX_TABLE_NAME_LEN</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_MAX_COLUMNS_IN_TABLE</TD>
#	<TD width=51%>SQL_MAX_TABLES_IN_SELECT</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_MAX_CURSOR_NAME_LEN</TD>
#	<TD width=51%>SQL_MAX_USER_NAME_LEN</TD>
#	</TR>
#	</table></div>
#	<!--TS:-->
#	<P class="label"><B>Scalar Function Information</B></P>
#	
#	<P>The following values of the <I>InfoType</I> argument return information about the scalar functions supported by the data source and the driver. For more information about scalar functions, see <A HREF="odbcscalar_functions.htm">Appendix E: Scalar Functions</A>.</P>
#	<!--TS:--><div class="tablediv"><table>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_CONVERT_FUNCTIONS</TD>
#	<TD width=51%>SQL_TIMEDATE_ADD_INTERVALS</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_NUMERIC_FUNCTIONS</TD>
#	<TD width=51%>SQL_TIMEDATE_DIFF_INTERVALS</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_STRING_FUNCTIONS</TD>
#	<TD width=51%>SQL_TIMEDATE_FUNCTIONS</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_SYSTEM_FUNCTIONS</TD>
#	<TD width=51%>&nbsp;</TD>
#	</TR>
#	</table></div>
#	<!--TS:-->
#	<P class="label"><B>Conversion Information</B></P>
#	
#	<P>The following values of the <I>InfoType</I> argument return a list of the SQL data types to which the data source can convert the specified SQL data type with the <B>CONVERT</B> scalar function:</P>
#	<!--TS:--><div class="tablediv"><table>
#	
#	<TR VALIGN="top">
#	<TD width=53%>SQL_CONVERT_BIGINT</TD>
#	<TD width=47%>SQL_CONVERT_LONGVARBINARY</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=53%>SQL_CONVERT_BINARY</TD>
#	<TD width=47%>SQL_CONVERT_LONGVARCHAR</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=53%>SQL_CONVERT_BIT</TD>
#	<TD width=47%>SQL_CONVERT_NUMERIC</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=53%>SQL_CONVERT_CHAR</TD>
#	<TD width=47%>SQL_CONVERT_REAL</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=53%>SQL_CONVERT_DATE</TD>
#	<TD width=47%>SQL_CONVERT_SMALLINT</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=53%>SQL_CONVERT_DECIMAL</TD>
#	<TD width=47%>SQL_CONVERT_TIME</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=53%>SQL_CONVERT_DOUBLE</TD>
#	<TD width=47%>SQL_CONVERT_TIMESTAMP</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=53%>SQL_CONVERT_FLOAT</TD>
#	<TD width=47%>SQL_CONVERT_TINYINT</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=53%>SQL_CONVERT_INTEGER</TD>
#	<TD width=47%>SQL_CONVERT_VARBINARY</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=53%>SQL_CONVERT_INTERVAL_YEAR_MONTH</TD>
#	<TD width=47%>SQL_CONVERT_VARCHAR</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=53%>SQL_CONVERT_INTERVAL_DAY_TIME</TD>
#	<TD width=47%>&nbsp;</TD>
#	</TR>
#	</table></div>
#	<!--TS:-->
#	<P class="label"><B>Information Types Added for ODBC 3<I>.x</I></B></P>
#	
#	<P>The following values of the <I>InfoType</I> argument have been added for ODBC 3<I>.x</I>:</P>
#	<!--TS:--><div class="tablediv"><table>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_ACTIVE_ENVIRONMENTS</TD>
#	<TD width=51%>SQL_DROP_ASSERTION</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_AGGREGATE_FUNCTIONS</TD>
#	<TD width=51%>SQL_DROP_CHARACTER_SET</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_ALTER_DOMAIN</TD>
#	<TD width=51%>SQL_DROP_COLLATION</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_ALTER_SCHEMA</TD>
#	<TD width=51%>SQL_DROP_DOMAIN</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_ANSI_SQL_DATETIME_LITERALS</TD>
#	<TD width=51%>SQL_DROP_SCHEMA</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_ASYNC_MODE</TD>
#	<TD width=51%>SQL_DROP_TABLE</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_BATCH_ROW_COUNT</TD>
#	<TD width=51%>SQL_DROP_TRANSLATION</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_BATCH_SUPPORT</TD>
#	<TD width=51%>SQL_DROP_VIEW</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_CATALOG_NAME</TD>
#	<TD width=51%>SQL_DYNAMIC_CURSOR_ATTRIBUTES1</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_COLLATION_SEQ</TD>
#	<TD width=51%>SQL_DYNAMIC_CURSOR_ATTRIBUTES2</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_CONVERT_INTERVAL_YEAR_MONTH</TD>
#	<TD width=51%>SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES1</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_CONVERT_INTERVAL_DAY_TIME</TD>
#	<TD width=51%>SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES2</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_CREATE_ASSERTION</TD>
#	<TD width=51%>SQL_INFO_SCHEMA_VIEWS</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_CREATE_CHARACTER_SET</TD>
#	<TD width=51%>SQL_INSERT_STATEMENT</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_CREATE_COLLATION</TD>
#	<TD width=51%>SQL_KEYSET_CURSOR_ATTRIBUTES1</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_CREATE_DOMAIN</TD>
#	<TD width=51%>SQL_KEYSET_CURSOR_ATTRIBUTES2</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_CREATE_SCHEMA</TD>
#	<TD width=51%>SQL_MAX_ASYNC_CONCURRENT_STATEMENTS</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_CREATE_TABLE</TD>
#	<TD width=51%>SQL_MAX_IDENTIFIER_LEN</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_CREATE_TRANSLATION</TD>
#	<TD width=51%>SQL_PARAM_ARRAY_ROW_COUNTS</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_CURSOR_SENSITIVITY</TD>
#	<TD width=51%>SQL_PARAM_ARRAY_SELECTS</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_DDL_INDEX</TD>
#	<TD width=51%>SQL_STATIC_CURSOR_ATTRIBUTES1</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_DESCRIBE_PARAMETER</TD>
#	<TD width=51%>SQL_STATIC_CURSOR_ATTRIBUTES2</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_DM_VER</TD>
#	<TD width=51%>SQL_XOPEN_CLI_YEAR</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=49%>SQL_DRIVER_HDESC</TD>
#	<TD width=51%>&nbsp;</TD>
#	</TR>
#	</table></div>
#	<!--TS:-->
#	<P class="label"><B>Information Types Renamed for ODBC 3<I>.x</I></B></P>
#	
#	<P>The following values of the <I>InfoType</I> argument have been renamed for ODBC 3<I>.x</I>.</P>
#	<!--TS:--><div class="tablediv"><table>
#	
#	<TR VALIGN="top">
#	<TH width=55%>ODBC 2.0 <I>InfoType</I></TH>
#	<TH width=45%>ODBC 3<I>.x</I> <I>InfoType</I></TH>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=55%>SQL_ACTIVE_CONNECTIONS</TD>
#	<TD width=45%>SQL_MAX_DRIVER_CONNECTIONS</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=55%>SQL_ACTIVE_STATEMENTS</TD>
#	<TD width=45%>SQL_MAX_CONCURRENT_ACTIVITIES</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=55%>SQL_MAX_OWNER_NAME_LEN</TD>
#	<TD width=45%>SQL_MAX_SCHEMA_NAME_LEN</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=55%>SQL_MAX_QUALIFIER_NAME_LEN</TD>
#	<TD width=45%>SQL_MAX_CATALOG_NAME_LEN</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=55%>SQL_ODBC_SQL_OPT_IEF</TD>
#	<TD width=45%>SQL_INTEGRITY</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=55%>SQL_OWNER_TERM</TD>
#	<TD width=45%>SQL_SCHEMA_TERM</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=55%>SQL_OWNER_USAGE</TD>
#	<TD width=45%>SQL_SCHEMA_USAGE</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=55%>SQL_QUALIFIER_LOCATION</TD>
#	<TD width=45%>SQL_CATALOG_LOCATION</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=55%>SQL_QUALIFIER_NAME_SEPARATOR</TD>
#	<TD width=45%>SQL_CATALOG_NAME_SEPARATOR</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=55%>SQL_QUALIFIER_TERM</TD>
#	<TD width=45%>SQL_CATALOG_TERM</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=55%>SQL_QUALIFIER_USAGE</TD>
#	<TD width=45%>SQL_CATALOG_USAGE</TD>
#	</TR>
#	</table></div>
#	<!--TS:-->
#	<P class="label"><B>Information Types Deprecated in ODBC 3<I>.x</I></B></P>
#	
#	<P>The following values of the <I>InfoType</I> argument have been deprecated in ODBC 3<I>.x</I>. ODBC 3<I>.x</I> drivers must continue to support these information types for backward compatibility with ODBC 2<I>.x</I> applications. (For more information on these types, see "<A HREF="odbcsqlgetinfo_support.htm">SQLGetInfo Support</A>" in Appendix G: Driver Guidelines for Backward Compatibility.)</P>
#	<!--TS:--><div class="tablediv"><table>
#	
#	<TR VALIGN="top">
#	<TD width=51%>SQL_FETCH_DIRECTION</TD>
#	<TD width=49%>SQL_POS_OPERATIONS</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=51%>SQL_LOCK_TYPES</TD>
#	<TD width=49%>SQL_POSITIONED_STATEMENTS</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=51%>SQL_ODBC_API_CONFORMANCE</TD>
#	<TD width=49%>SQL_SCROLL_CONCURRENCY</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=51%>SQL_ODBC_SQL_CONFORMANCE</TD>
#	<TD width=49%>SQL_STATIC_SENSITIVITY</TD>
#	</TR>
#	</table></div>
#	<!--TS:-->
#	<H2>Information Type Descriptions</H2>
#	
#	<P>The following table alphabetically lists each information type, the version of ODBC in which it was introduced, and its description.</P>
#	<!--TS:--><div class="tablediv"><table>
#	
#	<TR VALIGN="top">
#	<TH width=50%><I>InfoType</I></TH>
#	<TH width=50%>Returns</TH>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_ACCESSIBLE_PROCEDURES<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>A character string: "Y" if the user can execute all procedures returned by <B>SQLProcedures</B>; "N" if there may be procedures returned that the user cannot execute.</TD>
#	</TR>
    SQL_ACCESSIBLE_PROCEDURES => {
	type => q(YesNo),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_ACCESSIBLE_TABLES<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>A character string: "Y" if the user is guaranteed <B>SELECT</B> privileges to all tables returned by <B>SQLTables</B>; "N" if there may be tables returned that the user cannot access.</TD>
#	</TR>
    SQL_ACCESSIBLE_TABLES => {
	type => q(YesNo),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_ACTIVE_ENVIRONMENTS<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUSMALLINT value specifying the maximum number of active environments that the driver can support. If there is no specified limit or the limit is unknown, this value is set to zero.</TD>
#	</TR>
    SQL_ACTIVE_ENVIRONMENTS => {
	type => q(Short),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_AGGREGATE_FUNCTIONS<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating support for aggregation functions:
#	<P>SQL_AF_ALL<BR>
#	SQL_AF_AVG<BR>
#	SQL_AF_COUNT<BR>
#	SQL_AF_DISTINCT<BR>
#	SQL_AF_MAX<BR>
#	SQL_AF_MIN<BR>
#	SQL_AF_SUM </P>
#	
#	<P>An SQL-92 Entry level&#0150;conformant driver will always return all of these options as supported.</P>
#	</TD>
#	</TR>
    SQL_AGGREGATE_FUNCTIONS => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_ALTER_DOMAIN<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the clauses in the <B>ALTER DOMAIN</B> statement, as defined in SQL-92, supported by the data source. An SQL-92 Full level&#0150;compliant driver will always return all of the bitmasks. A return value of "0" means that the <B>ALTER DOMAIN</B> statement is not supported. 
#	<P>The SQL-92 or FIPS conformance level at which this feature needs to be supported is shown in parentheses next to each bitmask.</P>
#	
#	<P>The following bitmasks are used to determine which clauses are supported:</P>
#	
#	<P>SQL_AD_ADD_DOMAIN_CONSTRAINT = Adding a domain constraint is supported (Full level)</P>
#	
#	<P>SQL_AD_ADD_DOMAIN_DEFAULT = &lt;alter domain&gt; &lt;set domain default clause&gt; is supported (Full level)</P>
#	
#	<P>SQL_AD_CONSTRAINT_NAME_DEFINITION = &lt;constraint name definition clause&gt; is supported for naming domain constraint (Intermediate level)</P>
#	
#	<P>SQL_AD_DROP_DOMAIN_CONSTRAINT = &lt;drop domain constraint clause&gt; is supported (Full level)</P>
#	
#	<P>SQL_AD_DROP_DOMAIN_DEFAULT = &lt;alter domain&gt; &lt;drop domain default clause&gt; is supported (Full level)</P>
#	
#	<P>The following bits specify the supported &lt;constraint attributes&gt; if &lt;add domain constraint&gt; is supported (the SQL_AD_ADD_DOMAIN_CONSTRAINT bit is set):</P>
#	
#	<P>SQL_AD_ADD_CONSTRAINT_DEFERRABLE (Full level)<BR>
#	SQL_AD_ADD_CONSTRAINT_NON_DEFERRABLE (Full level)<BR>
#	SQL_AD_ADD_CONSTRAINT_INITIALLY_DEFERRED (Full level)<BR>
#	SQL_AD_ADD_CONSTRAINT_INITIALLY_IMMEDIATE (Full level)</P>
#	</TD>
#	</TR>
    SQL_ALTER_DOMAIN => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_ALTER_TABLE<BR>
#	(ODBC 2.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the clauses in the <B>ALTER TABLE</B> statement supported by the data source. 
#	<P>The SQL-92 or FIPS conformance level at which this feature needs to be supported is shown in parentheses next to each bitmask.</P>
#	
#	<P>The following bitmasks are used to determine which clauses are supported:</P>
#	
#	<P>SQL_AT_ADD_COLUMN_COLLATION = &lt;add column&gt; clause is supported, with facility to specify column collation (Full level) (ODBC 3.0)</P>
#	
#	<P>SQL_AT_ADD_COLUMN_DEFAULT = &lt;add column&gt; clause is supported, with facility to specify column defaults (FIPS Transitional level) (ODBC 3.0)</P>
#	
#	<P>SQL_AT_ADD_COLUMN_SINGLE = &lt;add column&gt; is supported (FIPS Transitional level) (ODBC 3.0)</P>
#	
#	<P>SQL_AT_ADD_CONSTRAINT = &lt;add column&gt; clause is supported, with facility to specify column constraints (FIPS Transitional level) (ODBC 3.0)</P>
#	
#	<P>SQL_AT_ADD_TABLE_CONSTRAINT = &lt;add table constraint&gt; clause is supported (FIPS Transitional level) (ODBC 3.0)</P>
#	
#	<P>SQL_AT_CONSTRAINT_NAME_DEFINITION = &lt;constraint name definition&gt; is supported for naming column and table constraints (Intermediate level) (ODBC 3.0)</P>
#	
#	<P>SQL_AT_DROP_COLUMN_CASCADE = &lt;drop column&gt; CASCADE is supported (FIPS Transitional level) (ODBC 3.0)</P>
#	
#	<P>SQL_AT_DROP_COLUMN_DEFAULT = &lt;alter column&gt; &lt;drop column default clause&gt; is supported (Intermediate level) (ODBC 3.0)</P>
#	
#	<P>SQL_AT_DROP_COLUMN_RESTRICT = &lt;drop column&gt; RESTRICT is supported (FIPS Transitional level) (ODBC 3.0)</P>
#	
#	<P>SQL_AT_DROP_TABLE_CONSTRAINT_CASCADE (ODBC 3.0)</P>
#	
#	<P>SQL_AT_DROP_TABLE_CONSTRAINT_RESTRICT = &lt;drop column&gt; RESTRICT is supported (FIPS Transitional level) (ODBC 3.0)</P>
#	
#	<P>SQL_AT_SET_COLUMN_DEFAULT = &lt;alter column&gt; &lt;set column default clause&gt; is supported (Intermediate level) (ODBC 3.0)</P>
#	
#	<P>The following bits specify the support &lt;constraint attributes&gt; if specifying column or table constraints is supported (the SQL_AT_ADD_CONSTRAINT bit is set):</P>
#	
#	<P>SQL_AT_CONSTRAINT_INITIALLY_DEFERRED (Full level) (ODBC 3.0)<BR>
#	SQL_AT_CONSTRAINT_INITIALLY_IMMEDIATE (Full level) (ODBC 3.0)<BR>
#	SQL_AT_CONSTRAINT_DEFERRABLE (Full level) (ODBC 3.0)<BR>
#	SQL_AT_CONSTRAINT_NON_DEFERRABLE (Full level) (ODBC 3.0)</P>
#	</TD>
#	</TR>
    SQL_ALTER_TABLE => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_ASYNC_MODE<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER value indicating the level of asynchronous support in the driver:
#	<P>SQL_AM_CONNECTION = Connection level asynchronous execution is supported. Either all statement handles associated with a given connection handle are in asynchronous mode or all are in synchronous mode. A statement handle on a connection cannot be in asynchronous mode while another statement handle on the same connection is in synchronous mode, and vice versa.</P>
#	
#	<P>SQL_AM_STATEMENT = Statement level asynchronous execution is supported. Some statement handles associated with a connection handle can be in asynchronous mode, while other statement handles on the same connection are in synchronous mode.</P>
#	
#	<P>SQL_AM_NONE = Asynchronous mode is not supported.</P>
#	</TD>
#	</TR>
    SQL_ASYNC_MODE => {
	type => q(Long),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_BATCH_ROW_COUNT <BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the behavior of the driver with respect to the availability of row counts. The following bitmasks are used in conjunction with the information type:
#	<P>SQL_BRC_ROLLED_UP = Row counts for consecutive INSERT, DELETE, or UPDATE statements are rolled up into one. If this bit is not set, then row counts are available for each individual statement.</P>
#	
#	<P>SQL_BRC_PROCEDURES = Row counts, if any, are available when a batch is executed in a stored procedure. If row counts are available, they can be rolled up or individually available, depending on the SQL_BRC_ROLLED_UP bit.</P>
#	
#	<P>SQL_BRC_EXPLICIT = Row counts, if any, are available when a batch is executed directly by calling <B>SQLExecute</B> or <B>SQLExecDirect</B>. If row counts are available, they can be rolled up or individually available, depending on the SQL_BRC_ROLLED_UP bit.</P>
#	</TD>
#	</TR>
    SQL_BATCH_ROW_COUNT => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_BATCH_SUPPORT <BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the driver's support for batches. The following bitmasks are used to determine which level is supported:
#	<P>SQL_BS_SELECT_EXPLICIT = The driver supports explicit batches that can have result-set generating statements.</P>
#	
#	<P>SQL_BS_ROW_COUNT_EXPLICIT = The driver supports explicit batches that can have row-count generating statements.</P>
#	
#	<P>SQL_BS_SELECT_PROC = The driver supports explicit procedures that can have result-set generating statements.</P>
#	
#	<P>SQL_BS_ROW_COUNT_PROC = The driver supports explicit procedures that can have row-count generating statements.</P>
#	</TD>
#	</TR>
    SQL_BATCH_SUPPORT => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_BOOKMARK_PERSISTENCE<BR>
#	(ODBC 2.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the operations through which bookmarks persist.
#	<P>The following bitmasks are used in conjunction with the flag to determine through which options bookmarks persist:</P>
#	
#	<P>SQL_BP_CLOSE = Bookmarks are valid after an application calls <B>SQLFreeStmt</B> with the SQL_CLOSE option, or <B>SQLCloseCursor</B> to close the cursor associated with a statement.</P>
#	
#	<P>SQL_BP_DELETE = The bookmark for a row is valid after that row has been deleted.</P>
#	
#	<P>SQL_BP_DROP = Bookmarks are valid after an application calls <B>SQLFreeHandle</B> with a <I>HandleType</I> of SQL_HANDLE_STMT to drop a statement.</P>
#	
#	<P>SQL_BP_TRANSACTION = Bookmarks are valid after an application commits or rolls back a transaction.</P>
#	
#	<P>SQL_BP_UPDATE = The bookmark for a row is valid after any column in that row has been updated, including key columns.</P>
#	
#	<P>SQL_BP_OTHER_HSTMT = A bookmark associated with one statement can be used with another statement. Unless SQL_BP_CLOSE or SQL_BP_DROP is specified, the cursor on the first statement must be open.</P>
#	</TD>
#	</TR>
    SQL_BOOKMARK_PERSISTENCE => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_CATALOG_LOCATION<BR>
#	(ODBC 2.0)</TD>
#	<TD width=50%>An SQLUSMALLINT value indicating the position of the catalog in a qualified table name:
#	<P>SQL_CL_START<BR>
#	SQL_CL_END</P>
#	
#	<P>For example, an Xbase driver returns SQL_CL_START because the directory (catalog) name is at the start of the table name, as in \EMPDATA\EMP.DBF. An ORACLE Server driver returns SQL_CL_END because the catalog is at the end of the table name, as in ADMIN.EMP@EMPDATA.</P>
#	
#	<P>An SQL-92 Full level&#0150;conformant driver will always return SQL_CL_START. A value of 0 is returned if catalogs are not supported by the data source. To find out whether catalogs are supported, an application calls <B>SQLGetInfo</B> with the SQL_CATALOG_NAME information type.</P>
#	
#	<P>This <I>InfoType</I> has been renamed for ODBC 3.0 from the ODBC 2.0 <I>InfoType</I> SQL_QUALIFIER_LOCATION.</P>
#	</TD>
#	</TR>
    SQL_CATALOG_LOCATION => {
	type => q(Short),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_CATALOG_NAME<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>A character string: "Y" if the server supports catalog names, or "N" if it does not.
#	<P>An SQL-92 Full level&#0150;conformant driver will always return "Y".</P>
#	</TD>
#	</TR>
    SQL_CATALOG_NAME => {
	type => q(YesNo),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_CATALOG_NAME_SEPARATOR<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>A character string: the character or characters that the data source defines as the separator between a catalog name and the qualified name element that follows or precedes it. 
#	<P>An empty string is returned if catalogs are not supported by the data source. To find out whether catalogs are supported, an application calls <B>SQLGetInfo</B> with the SQL_CATALOG_NAME information type. An SQL-92 Full level&#0150;conformant driver will always return ".".</P>
#	
#	<P>This <I>InfoType</I> has been renamed for ODBC 3.0 from the ODBC 2.0 <I>InfoType</I> SQL_QUALIFIER_NAME_SEPARATOR.</P>
#	</TD>
#	</TR>
    SQL_CATALOG_NAME_SEPARATOR => {
	type => q(Char),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_CATALOG_TERM<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>A character string with the data source vendor's name for a catalog; for example, "database" or "directory". This string can be in upper, lower, or mixed case.
#	<P>An empty string is returned if catalogs are not supported by the data source. To find out whether catalogs are supported, an application calls <B>SQLGetInfo</B> with the SQL_CATALOG_NAME information type. An SQL-92 Full level&#0150;conformant driver will always return "catalog".</P>
#	
#	<P>This <I>InfoType</I> has been renamed for ODBC 3.0 from the ODBC 2.0 <I>InfoType</I> SQL_QUALIFIER_TERM.</P>
#	</TD>
#	</TR>
    SQL_CATALOG_TERM => {
	type => q(Char),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_CATALOG_USAGE<BR>
#	(ODBC 2.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the statements in which catalogs can be used.
#	<P>The following bitmasks are used to determine where catalogs can be used:</P>
#	
#	<P>SQL_CU_DML_STATEMENTS = Catalogs are supported in all Data Manipulation Language statements: <B>SELECT</B>, <B>INSERT</B>, <B>UPDATE</B>, <B>DELETE</B>, and if supported, <B>SELECT FOR UPDATE</B> and positioned update and delete statements.</P>
#	
#	<P>SQL_CU_PROCEDURE_INVOCATION = Catalogs are supported in the ODBC procedure invocation statement.</P>
#	
#	<P>SQL_CU_TABLE_DEFINITION = Catalogs are supported in all table definition statements: <B>CREATE TABLE</B>, <B>CREATE VIEW</B>, <B>ALTER TABLE</B>, <B>DROP TABLE</B>, and <B>DROP VIEW</B>.</P>
#	
#	<P>SQL_CU_INDEX_DEFINITION = Catalogs are supported in all index definition statements: <B>CREATE INDEX</B> and <B>DROP INDEX</B>.</P>
#	
#	<P>SQL_CU_PRIVILEGE_DEFINITION = Catalogs are supported in all privilege definition statements: <B>GRANT</B> and <B>REVOKE</B>. </P>
#	
#	<P>A value of 0 is returned if catalogs are not supported by the data source. To find out whether catalogs are supported, an application calls <B>SQLGetInfo</B> with the SQL_CATALOG_NAME information type. An SQL-92 Full level&#0150;conformant driver will always return a bitmask with all of these bits set.</P>
#	
#	<P>This <I>InfoType</I> has been renamed for ODBC 3.0 from the ODBC 2.0 <I>InfoType</I> SQL_QUALIFIER_USAGE.</P>
#	</TD>
#	</TR>
    SQL_CATALOG_USAGE => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_COLLATION_SEQ<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>The name of the collation sequence. This is a character string that indicates the name of the default collation for the default character set for this server (for example, 'ISO 8859-1' or EBCDIC). If this is unknown, an empty string will be returned. An SQL-92 Full level&#0150;conformant driver will always return a non-empty string.</TD>
#	</TR>
    SQL_COLLATION_SEQ => {
	type => q(Char),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_COLUMN_ALIAS<BR>
#	(ODBC 2.0)</TD>
#	<TD width=50%>A character string: "Y" if the data source supports column aliases; otherwise, "N". 
#	<P>A column alias is an alternate name that can be specified for a column in the select list by using an AS clause. An SQL-92 Entry level&#0150;conformant driver will always return "Y".</P>
#	</TD>
#	</TR>
    SQL_COLUMN_ALIAS => {
	type => q(YesNo),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_CONCAT_NULL_BEHAVIOR<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>An SQLUSMALLINT value indicating how the data source handles the concatenation of NULL valued character data type columns with non-NULL valued character data type columns:
#	<P>SQL_CB_NULL = Result is NULL valued.</P>
#	
#	<P>SQL_CB_NON_NULL = Result is concatenation of non-NULL valued column or columns. </P>
#	
#	<P>An SQL-92 Entry level&#0150;conformant driver will always return SQL_CB_NULL.</P>
#	</TD>
#	</TR>
    SQL_CONCAT_NULL_BEHAVIOR => {
	type => q(Short),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_CONVERT_BIGINT<BR>
    SQL_CONVERT_BIGINT => {
	type => q(Bitmask),
    },
#	SQL_CONVERT_BINARY<BR>
    SQL_CONVERT_BINARY => {
	type => q(Bitmask),
    },
#	SQL_CONVERT_BIT <BR>
    SQL_CONVERT_BIT => {
	type => q(Bitmask),
    },
#	SQL_CONVERT_CHAR <BR>
    SQL_CONVERT_CHAR => {
	type => q(Bitmask),
    },
#	SQL_CONVERT_GUID<BR>
    SQL_CONVERT_GUID => {
	type => q(Bitmask),
	omit => 1,
    },
#	SQL_CONVERT_DATE<BR>
    SQL_CONVERT_DATE => {
	type => q(Bitmask),
    },
#	SQL_CONVERT_DECIMAL<BR>
    SQL_CONVERT_DECIMAL => {
	type => q(Bitmask),
    },
#	SQL_CONVERT_DOUBLE<BR>
    SQL_CONVERT_DOUBLE => {
	type => q(Bitmask),
    },
#	SQL_CONVERT_FLOAT<BR>
    SQL_CONVERT_FLOAT => {
	type => q(Bitmask),
    },
#	SQL_CONVERT_INTEGER<BR>
    SQL_CONVERT_INTEGER => {
	type => q(Bitmask),
    },
#	SQL_CONVERT_INTERVAL_YEAR_MONTH<BR>
    SQL_CONVERT_INTERVAL_YEAR_MONTH => {
	type => q(Bitmask),
    },
#	SQL_CONVERT_INTERVAL_DAY_TIME<BR>
    SQL_CONVERT_INTERVAL_DAY_TIME => {
	type => q(Bitmask),
    },
#	SQL_CONVERT_LONGVARBINARY<BR>
    SQL_CONVERT_LONGVARBINARY => {
	type => q(Bitmask),
    },
#	SQL_CONVERT_LONGVARCHAR<BR>
    SQL_CONVERT_LONGVARCHAR => {
	type => q(Bitmask),
    },
#	SQL_CONVERT_NUMERIC<BR>
    SQL_CONVERT_NUMERIC => {
	type => q(Bitmask),
    },
#	SQL_CONVERT_REAL<BR>
    SQL_CONVERT_REAL => {
	type => q(Bitmask),
    },
#	SQL_CONVERT_SMALLINT<BR>
    SQL_CONVERT_SMALLINT => {
	type => q(Bitmask),
    },
#	SQL_CONVERT_TIME<BR>
    SQL_CONVERT_TIME => {
	type => q(Bitmask),
    },
#	SQL_CONVERT_TIMESTAMP<BR>
    SQL_CONVERT_TIMESTAMP => {
	type => q(Bitmask),
    },
#	SQL_CONVERT_TINYINT<BR>
    SQL_CONVERT_TINYINT => {
	type => q(Bitmask),
    },
#	SQL_CONVERT_VARBINARY<BR>
    SQL_CONVERT_VARBINARY => {
	type => q(Bitmask),
    },
#	SQL_CONVERT_VARCHAR <BR>
    SQL_CONVERT_VARCHAR => {
	type => q(Bitmask),
    },
#	(ODBC 1.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask. The bitmask indicates the conversions supported by the data source with the <B>CONVERT</B> scalar function for data of the type named in the <I>InfoType</I>. If the bitmask equals zero, the data source does not support any conversions from data of the named type, including conversion to the same data type.
#	<P>For example, to find out if a data source supports the conversion of SQL_INTEGER data to the SQL_BIGINT data type, an application calls <B>SQLGetInfo</B> with the <I>InfoType</I> of SQL_CONVERT_INTEGER. The application performs an <B>AND</B> operation with the returned bitmask and SQL_CVT_BIGINT. If the resulting value is nonzero, the conversion is supported. </P>
#	
#	<P>The following bitmasks are used to determine which conversions are supported:</P>
#	
#	<P>SQL_CVT_BIGINT (ODBC 1.0)<BR>
#	SQL_CVT_BINARY (ODBC 1.0)<BR>
#	SQL_CVT_BIT (ODBC 1.0) <BR>
#	SQL_CVT_GUID (ODBC 3.5)<BR>
#	SQL_CVT_CHAR (ODBC 1.0) <BR>
#	SQL_CVT_DATE (ODBC 1.0)<BR>
#	SQL_CVT_DECIMAL (ODBC 1.0)<BR>
#	SQL_CVT_DOUBLE (ODBC 1.0)<BR>
#	SQL_CVT_FLOAT (ODBC 1.0)<BR>
#	SQL_CVT_INTEGER (ODBC 1.0)<BR>
#	SQL_CVT_INTERVAL_YEAR_MONTH (ODBC 3.0)<BR>
#	SQL_CVT_INTERVAL_DAY_TIME (ODBC 3.0)<BR>
#	SQL_CVT_LONGVARBINARY (ODBC 1.0)<BR>
#	SQL_CVT_LONGVARCHAR (ODBC 1.0)<BR>
#	SQL_CVT_NUMERIC (ODBC 1.0)<BR>
#	SQL_CVT_REAL ODBC 1.0)<BR>
#	SQL_CVT_SMALLINT (ODBC 1.0)<BR>
#	SQL_CVT_TIME (ODBC 1.0)<BR>
#	SQL_CVT_TIMESTAMP (ODBC 1.0)<BR>
#	SQL_CVT_TINYINT (ODBC 1.0)<BR>
#	SQL_CVT_VARBINARY (ODBC 1.0)<BR>
#	SQL_CVT_VARCHAR (ODBC 1.0)</P>
#	</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_CONVERT_FUNCTIONS<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the scalar conversion functions supported by the driver and associated data source.
#	<P>The following bitmask is used to determine which conversion functions are supported:</P>
#	
#	<P>SQL_FN_CVT_CAST<BR>
#	SQL_FN_CVT_CONVERT</P>
#	</TD>
#	</TR>
    SQL_CONVERT_FUNCTIONS => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_CORRELATION_NAME<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>An SQLUSMALLINT value indicating whether table correlation names are supported:
#	<P>SQL_CN_NONE = Correlation names are not supported.</P>
#	
#	<P>SQL_CN_DIFFERENT = Correlation names are supported but must differ from the names of the tables they represent.</P>
#	
#	<P>SQL_CN_ANY = Correlation names are supported and can be any valid user-defined name. </P>
#	
#	<P>An SQL-92 Entry level&#0150;conformant driver will always return SQL_CN_ANY.</P>
#	</TD>
#	</TR>
    SQL_CORRELATION_NAME => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_CREATE_ASSERTION<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the clauses in the <B>CREATE ASSERTION</B> statement, as defined in SQL-92, supported by the data source. 
#	<P>The following bitmasks are used to determine which clauses are supported:</P>
#	
#	<P>SQL_CA_CREATE_ASSERTION</P>
#	
#	<P>The following bits specify the supported constraint attribute if the ability to specify constraint attributes explicitly is supported (see the SQL_ALTER_TABLE and SQL_CREATE_TABLE information types):</P>
#	
#	<P>SQL_CA_CONSTRAINT_INITIALLY_DEFERRED<BR>
#	SQL_CA_CONSTRAINT_INITIALLY_IMMEDIATE<BR>
#	SQL_CA_CONSTRAINT_DEFERRABLE<BR>
#	SQL_CA_CONSTRAINT_NON_DEFERRABLE</P>
#	
#	<P>An SQL-92 Full level&#0150;conformant driver will always return all of these options as supported. A return value of "0" means that the <B>CREATE ASSERTION</B> statement is not supported.</P>
#	</TD>
#	</TR>
    SQL_CREATE_ASSERTION => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_CREATE_CHARACTER_SET<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the clauses in the <B>CREATE CHARACTER SET</B> statement, as defined in SQL-92, supported by the data source. 
#	<P>The following bitmasks are used to determine which clauses are supported:</P>
#	
#	<P>SQL_CCS_CREATE_CHARACTER_SET<BR>
#	SQL_CCS_COLLATE_CLAUSE<BR>
#	SQL_CCS_LIMITED_COLLATION</P>
#	
#	<P>An SQL-92 Full level&#0150;conformant driver will always return all of these options as supported. A return value of "0" means that the <B>CREATE CHARACTER SET</B> statement is not supported.</P>
#	</TD>
#	</TR>
    SQL_CREATE_CHARACTER_SET => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_CREATE_COLLATION<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the clauses in the <B>CREATE COLLATION</B> statement, as defined in SQL-92, supported by the data source. 
#	<P>The following bitmask is used to determine which clauses are supported:</P>
#	
#	<P>SQL_CCOL_CREATE_COLLATION </P>
#	
#	<P>An SQL-92 Full level&#0150;conformant driver will always return this option as supported. A return value of "0" means that the <B>CREATE COLLATION</B> statement is not supported.</P>
#	</TD>
#	</TR>
    SQL_CREATE_COLLATION => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_CREATE_DOMAIN<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the clauses in the <B>CREATE DOMAIN</B> statement, as defined in SQL-92, supported by the data source. 
#	<P>The following bitmasks are used to determine which clauses are supported:</P>
#	
#	<P>SQL_CDO_CREATE_DOMAIN = The CREATE DOMAIN statement is supported (Intermediate level).</P>
#	
#	<P>SQL_CDO_CONSTRAINT_NAME_DEFINITION = &lt;constraint name definition&gt; is supported for naming domain constraints (Intermediate level).</P>
#	
#	<P>The following bits specify the ability to create column constraints:<BR>
#	SQL_CDO_DEFAULT = Specifying domain constraints is supported (Intermediate level)<BR>
#	SQL_CDO_CONSTRAINT = Specifying domain defaults is supported (Intermediate level)<BR>
#	SQL_CDO_COLLATION = Specifying domain collation is supported (Full level)</P>
#	
#	<P>The following bits specify the supported constraint attributes if specifying domain constraints is supported (SQL_CDO_DEFAULT is set):</P>
#	
#	<P>SQL_CDO_CONSTRAINT_INITIALLY_DEFERRED (Full level)<BR>
#	SQL_CDO_CONSTRAINT_INITIALLY_IMMEDIATE (Full level)<BR>
#	SQL_CDO_CONSTRAINT_DEFERRABLE (Full level)<BR>
#	SQL_CDO_CONSTRAINT_NON_DEFERRABLE (Full level)</P>
#	
#	<P>A return value of "0" means that the <B>CREATE DOMAIN</B> statement is not supported.</P>
#	</TD>
#	</TR>
    SQL_CREATE_DOMAIN => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_CREATE_SCHEMA<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the clauses in the <B>CREATE SCHEMA</B> statement, as defined in SQL-92, supported by the data source. 
#	<P>The following bitmasks are used to determine which clauses are supported:</P>
#	
#	<P>SQL_CS_CREATE_SCHEMA<BR>
#	SQL_CS_AUTHORIZATION<BR>
#	SQL_CS_DEFAULT_CHARACTER_SET </P>
#	
#	<P>An SQL-92 Intermediate level&#0150;conformant driver will always return the SQL_CS_CREATE_SCHEMA and SQL_CS_AUTHORIZATION options as supported. These must also be supported at the SQL-92 Entry level, but not necessarily as SQL statements. An SQL-92 Full level&#0150;conformant driver will always return all of these options as supported.</P>
#	</TD>
#	</TR>
    SQL_CREATE_SCHEMA => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_CREATE_TABLE<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the clauses in the <B>CREATE TABLE</B> statement, as defined in SQL-92, supported by the data source. 
#	<P>The SQL-92 or FIPS conformance level at which this feature needs to be supported is shown in parentheses next to each bitmask.</P>
#	
#	<P>The following bitmasks are used to determine which clauses are supported:</P>
#	
#	<P>SQL_CT_CREATE_TABLE = The CREATE TABLE statement is supported. (Entry level)</P>
#	
#	<P>SQL_CT_TABLE_CONSTRAINT = Specifying table constraints is supported (FIPS Transitional level)</P>
#	
#	<P>SQL_CT_CONSTRAINT_NAME_DEFINITION = The &lt;constraint name definition&gt; clause is supported for naming column and table constraints (Intermediate level)</P>
#	
#	<P>The following bits specify the ability to create temporary tables:</P>
#	
#	<P>SQL_CT_COMMIT_PRESERVE = Deleted rows are preserved on commit. (Full level)<BR>
#	SQL_CT_COMMIT_DELETE = Deleted rows are deleted on commit. (Full level)<BR>
#	SQL_CT_GLOBAL_TEMPORARY = Global temporary tables can be created. (Full level)<BR>
#	SQL_CT_LOCAL_TEMPORARY = Local temporary tables can be created. (Full level)</P>
#	
#	<P>The following bits specify the ability to create column constraints:</P>
#	
#	<P>SQL_CT_COLUMN_CONSTRAINT = Specifying column constraints is supported (FIPS Transitional level)<BR>
#	SQL_CT_COLUMN_DEFAULT = Specifying column defaults is supported (FIPS Transitional level)<BR>
#	SQL_CT_COLUMN_COLLATION = Specifying column collation is supported (Full level)</P>
#	
#	<P>The following bits specify the supported constraint attributes if specifying column or table constraints is supported:</P>
#	
#	<P>SQL_CT_CONSTRAINT_INITIALLY_DEFERRED (Full level)<BR>
#	SQL_CT_CONSTRAINT_INITIALLY_IMMEDIATE (Full level)<BR>
#	SQL_CT_CONSTRAINT_DEFERRABLE (Full level)<BR>
#	SQL_CT_CONSTRAINT_NON_DEFERRABLE (Full level)</P>
#	</TD>
#	</TR>
    SQL_CREATE_TABLE => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_CREATE_TRANSLATION<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the clauses in the <B>CREATE TRANSLATION</B> statement, as defined in SQL-92, supported by the data source. 
#	<P>The following bitmask is used to determine which clauses are supported:</P>
#	
#	<P>SQL_CTR_CREATE_TRANSLATION</P>
#	
#	<P>An SQL-92 Full level&#0150;conformant driver will always return these options as supported. A return value of "0" means that the <B>CREATE TRANSLATION</B> statement is not supported.</P>
#	</TD>
#	</TR>
    SQL_CREATE_TRANSLATION => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_CREATE_VIEW<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the clauses in the <B>CREATE VIEW</B> statement, as defined in SQL-92, supported by the data source. 
#	<P>The following bitmasks are used to determine which clauses are supported:</P>
#	
#	<P>SQL_CV_CREATE_VIEW<BR>
#	SQL_CV_CHECK_OPTION<BR>
#	SQL_CV_CASCADED<BR>
#	SQL_CV_LOCAL </P>
#	
#	<P>A return value of "0" means that the <B>CREATE VIEW</B> statement is not supported.</P>
#	
#	<P>An SQL-92 Entry level&#0150;conformant driver will always return the SQL_CV_CREATE_VIEW and SQL_CV_CHECK_OPTION options as supported. </P>
#	
#	<P>An SQL-92 Full level&#0150;conformant driver will always return all of these options as supported.</P>
#	</TD>
#	</TR>
    SQL_CREATE_VIEW => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_CURSOR_COMMIT_BEHAVIOR<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>An SQLUSMALLINT value indicating how a <B>COMMIT</B> operation affects cursors and prepared statements in the data source:
#	<P>SQL_CB_DELETE = Close cursors and delete prepared statements. To use the cursor again, the application must reprepare and reexecute the statement.</P>
#	
#	<P>SQL_CB_CLOSE = Close cursors. For prepared statements, the application can call <B>SQLExecute</B> on the statement without calling <B>SQLPrepare</B> again.</P>
#	
#	<P>SQL_CB_PRESERVE = Preserve cursors in the same position as before the <B>COMMIT</B> operation. The application can continue to fetch data, or it can close the cursor and reexecute the statement without repreparing it.</P>
#	</TD>
#	</TR>
    SQL_CURSOR_COMMIT_BEHAVIOR => {
	type => q(Short),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_CURSOR_ROLLBACK_BEHAVIOR<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>An SQLUSMALLINT value indicating how a <B>ROLLBACK</B> operation affects cursors and prepared statements in the data source:
#	<P>SQL_CB_DELETE = Close cursors and delete prepared statements. To use the cursor again, the application must reprepare and reexecute the statement.</P>
#	
#	<P>SQL_CB_CLOSE = Close cursors. For prepared statements, the application can call <B>SQLExecute</B> on the statement without calling <B>SQLPrepare</B> again.</P>
#	
#	<P>SQL_CB_PRESERVE = Preserve cursors in the same position as before the <B>ROLLBACK</B> operation. The application can continue to fetch data, or it can close the cursor and reexecute the statement without repreparing it.</P>
#	</TD>
#	</TR>
    SQL_CURSOR_ROLLBACK_BEHAVIOR => {
	type => q(Short),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_CURSOR_ROLLBACK_SQL_CURSOR_SENSITIVITY<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER value indicating the support for cursor sensitivity:
#	<P>SQL_INSENSITIVE = All cursors on the statement handle show the result set without reflecting any changes made to it by any other cursor within the same transaction.</P>
#	
#	<P>SQL_UNSPECIFIED = It is unspecified whether cursors on the statement handle make visible the changes made to a result set by another cursor within the same transaction. Cursors on the statement handle may make visible none, some, or all such changes.</P>
#	
#	<P>SQL_SENSITIVE = Cursors are sensitive to changes made by other cursors within the same transaction.</P>
#	
#	<P>An SQL-92 Entry level&#0150;conformant driver will always return the SQL_UNSPECIFIED option as supported. </P>
#	
#	<P>An SQL-92 Full level&#0150;conformant driver will always return the SQL_INSENSITIVE option as supported.</P>
#	</TD>
#	</TR>
    SQL_CURSOR_SENSITIVITY => {
	type => q(Long),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_DATA_SOURCE_NAME<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>A character string with the data source name used during connection. If the application called <B>SQLConnect</B>, this is the value of the <I>szDSN</I> argument. If the application called <B>SQLDriverConnect</B> or <B>SQLBrowseConnect</B>, this is the value of the DSN keyword in the connection string passed to the driver. If the connection string did not contain the <B>DSN</B> keyword (such as when it contains the <B>DRIVER</B> keyword), this is an empty string.</TD>
#	</TR>
    SQL_DATA_SOURCE_NAME => {
	type => q(Char),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_DATA_SOURCE_READ_ONLY<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>A character string. "Y" if the data source is set to READ ONLY mode, "N" if it is otherwise.
#	<P>This characteristic pertains only to the data source itself; it is not a characteristic of the driver that enables access to the data source. A driver that is read/write can be used with a data source that is read-only. If a driver is read-only, all of its data sources must be read-only and must return SQL_DATA_SOURCE_READ_ONLY.</P>
#	</TD>
#	</TR>
    SQL_DATA_SOURCE_READ_ONLY => {
	type => q(YesNo),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_DATABASE_NAME<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>A character string with the name of the current database in use, if the data source defines a named object called "database".
#	<P class="indent"><b class="le">Note</b>&nbsp;&nbsp;&nbsp;In ODBC 3<I>.x</I>, the value returned for this <I>InfoType</I> can also be returned by calling <B>SQLGetConnectAttr</B> with an <I>Attribute</I> argument of SQL_ATTR_CURRENT_CATALOG.</P>
#	</TD>
#	</TR>
    SQL_DATABASE_NAME => {
	type => q(Char),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_DATETIME_LITERALS<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the SQL-92 datetime literals supported by the data source. Note that these are the datetime literals listed in the SQL-92 specification and are separate from the datetime literal escape clauses defined by ODBC. For more information about the ODBC datetime literal escape clauses, see "Date, Time, Timestamp, and Datetime Interval Literals" in Chapter 8: SQL Statements.
#	<P>A FIPS Transitional level&#0150;conformant driver will always return the "1" value in the bitmask for the bits listed below. A value of "0" means that SQL-92 datetime literals are not supported.</P>
#	
#	<P>The following bitmasks are used to determine which literals are supported:</P>
#	
#	<P>SQL_DL_SQL92_DATE<BR>
#	SQL_DL_SQL92_TIME<BR>
#	SQL_DL_SQL92_TIMESTAMP<BR>
#	SQL_DL_SQL92_INTERVAL_YEAR<BR>
#	SQL_DL_SQL92_INTERVAL_MONTH<BR>
#	SQL_DL_SQL92_INTERVAL_DAY<BR>
#	SQL_DL_SQL92_INTERVAL_HOUR<BR>
#	SQL_DL_SQL92_INTERVAL_MINUTE<BR>
#	SQL_DL_SQL92_INTERVAL_SECOND<BR>
#	SQL_DL_SQL92_INTERVAL_YEAR_TO_MONTH<BR>
#	SQL_DL_SQL92_INTERVAL_DAY_TO_HOUR</P>
#	
#	<P>SQL_DL_SQL92_INTERVAL_DAY_TO_MINUTE<BR>
#	SQL_DL_SQL92_INTERVAL_DAY_TO_SECOND<BR>
#	SQL_DL_SQL92_INTERVAL_HOUR_TO_MINUTE<BR>
#	SQL_DL_SQL92_INTERVAL_HOUR_TO_SECOND<BR>
#	SQL_DL_SQL92_INTERVAL_MINUTE_TO_SECOND</P>
#	</TD>
#	</TR>
    SQL_DATETIME_LITERALS => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_DBMS_NAME<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>A character string with the name of the DBMS product accessed by the driver.</TD>
#	</TR>
    SQL_DBMS_NAME => {
	type => q(Char),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_DBMS_VER<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>A character string indicating the version of the DBMS product accessed by the driver. The version is of the form ##.##.####, where the first two digits are the major version, the next two digits are the minor version, and the last four digits are the release version. The driver must render the DBMS product version in this form but can also append the DBMS product-specific version as well. For example, "04.01.0000 Rdb 4.1".</TD>
#	</TR>
    SQL_DBMS_VER => {
	type => q(Char),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_DDL_INDEX<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER value that indicates support for creation and dropping of indexes:
#	<P>SQL_DI_CREATE_INDEX<BR>
#	SQL_DI_DROP_INDEX </P>
#	</TD>
#	</TR>
    SQL_DDL_INDEX => {
	type => q(Long),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_DEFAULT_TXN_ISOLATION<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>An SQLUINTEGER value that indicates the default transaction isolation level supported by the driver or data source, or zero if the data source does not support transactions. The following terms are used to define transaction isolation levels:
#	<P><B>Dirty Read   </B>Transaction 1 changes a row. Transaction 2 reads the changed row before transaction 1 commits the change. If transaction 1 rolls back the change, transaction 2 will have read a row that is considered to have never existed.</P>
#	
#	<P><B>Nonrepeatable Read   </B>Transaction 1 reads a row. Transaction 2 updates or deletes that row and commits this change. If transaction 1 attempts to reread the row, it will receive different row values or discover that the row has been deleted.</P>
#	
#	<P><B>Phantom   </B>Transaction 1 reads a set of rows that satisfy some search criteria. Transaction 2 generates one or more rows (through either inserts or updates) that match the search criteria. If transaction 1 reexecutes the statement that reads the rows, it receives a different set of rows.</P>
#	
#	<P>If the data source supports transactions, the driver returns one of the following bitmasks:</P>
#	
#	<P>SQL_TXN_READ_UNCOMMITTED = Dirty reads, nonrepeatable reads, and phantoms are possible.</P>
#	
#	<P>SQL_TXN_READ_COMMITTED = Dirty reads are not possible. Nonrepeatable reads and phantoms are possible.</P>
#	
#	<P>SQL_TXN_REPEATABLE_READ = Dirty reads and nonrepeatable reads are not possible. Phantoms are possible.</P>
#	
#	<P>SQL_TXN_SERIALIZABLE = Transactions are serializable. Serializable transactions do not allow dirty reads, nonrepeatable reads, or phantoms.</P>
#	</TD>
#	</TR>
    SQL_DEFAULT_TXN_ISOLATION => {
	type => q(Long),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_DESCRIBE_PARAMETER<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>A character string: "Y" if parameters can be described; "N", if not. 
#	<P>An SQL-92 Full level&#0150;conformant driver will usually return "Y" because it will support the <B>DESCRIBE INPUT</B> statement. Because this does not directly specify the underlying SQL support, however, describing parameters might not be supported, even in a SQL-92 Full level&#0150;conformant driver.</P>
#	</TD>
#	</TR>
    SQL_DESCRIBE_PARAMETER => {
	type => q(YesNo),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_DM_VER<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>A character string with the version of the Driver Manager. The version is of the form ##.##.####.####, where:
#	<P>The first set of two digits is the major ODBC version, as given by the constant SQL_SPEC_MAJOR.</P>
#	
#	<P>The second set of two digits is the minor ODBC version, as given by the constant SQL_SPEC_MINOR.</P>
#	
#	<P>The third set of four digits is the Driver Manager major build number.</P>
#	
#	<P>The last set of four digits is the Driver Manager minor build number.</P>
#	</TD>
#	</TR>
    SQL_DM_VER => {
	type => q(Char),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_DRIVER_HDBC<BR>
#	SQL_DRIVER_HENV<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>An SQLUINTEGER value, the driver's environment handle or connection handle, determined by the argument <I>InfoType</I>.
#	<P>These information types are implemented by the Driver Manager alone.</P>
#	</TD>
#	</TR>
    SQL_DRIVER_HDBC => {
	type => q(Long),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_DRIVER_HDESC<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER value, the driver's descriptor handle determined by the Driver Manager's descriptor handle, which must be passed on input in *<I>InfoValuePtr</I> from the application. In this case, <I>InfoValuePtr</I> is both an input and output argument. The input descriptor handle passed in *<I>InfoValuePtr</I> must have been either explicitly or implicitly allocated on the <I>ConnectionHandle</I>. 
#	<P>The application should make a copy of the Driver Manager's descriptor handle before calling <B>SQLGetInfo</B> with this information type, to ensure that the handle is not overwritten on output.</P>
#	
#	<P>This information type is implemented by the Driver Manager alone.</P>
#	</TD>
#	</TR>
    SQL_DRIVER_HDESC => {
	type => q(Long),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_DRIVER_HLIB<BR>
#	(ODBC 2.0)</TD>
#	<TD width=50%>An SQLUINTEGER value, the <I>hinst</I> from the load library returned to the Driver Manager when it loaded the driver DLL (on a Microsoft&reg; Windows&reg; platform) or equivalent on a non-Windows platform. The handle is valid only for the connection handle specified in the call to <B>SQLGetInfo</B>.
#	<P>This information type is implemented by the Driver Manager alone.</P>
#	</TD>
#	</TR>
    SQL_DRIVER_HLIB => {
	type => q(Long),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_DRIVER_HSTMT<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>An SQLUINTEGER value, the driver's statement handle determined by the Driver Manager statement handle, which must be passed on input in *<I>InfoValuePtr</I> from the application. In this case, <I>InfoValuePtr</I> is both an input and an output argument. The input statement handle passed in *<I>InfoValuePtr</I> must have been allocated on the argument <I>ConnectionHandle</I>.
#	<P>The application should make a copy of the Driver Manager's statement handle before calling <B>SQLGetInfo</B> with this information type, to ensure that the handle is not overwritten on output.</P>
#	
#	<P>This information type is implemented by the Driver Manager alone.</P>
#	</TD>
#	</TR>
    SQL_DRIVER_HSTMT => {
	type => q(Long),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_DRIVER_NAME<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>A character string with the file name of the driver used to access the data source.</TD>
#	</TR>
    SQL_DRIVER_NAME => {
	type => q(Char),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_DRIVER_ODBC_VER<BR>
#	(ODBC 2.0)</TD>
#	<TD width=50%>A character string with the version of ODBC that the driver supports. The version is of the form ##.##, where the first two digits are the major version and the next two digits are the minor version. SQL_SPEC_MAJOR and SQL_SPEC_MINOR define the major and minor version numbers. For the version of ODBC described in this manual, these are 3 and 0, and the driver should return "03.00".</TD>
#	</TR>
    SQL_DRIVER_ODBC_VER => {
	type => q(Char),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_DRIVER_VER<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>A character string with the version of the driver and optionally, a description of the driver. At a minimum, the version is of the form ##.##.####, where the first two digits are the major version, the next two digits are the minor version, and the last four digits are the release version.</TD>
#	</TR>
    SQL_DRIVER_VER => {
	type => q(Char),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_DROP_ASSERTION<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the clauses in the <B>DROP ASSERTION</B> statement, as defined in SQL-92, supported by the data source.
#	<P>The following bitmask is used to determine which clauses are supported:</P>
#	
#	<P>SQL_DA_DROP_ASSERTION </P>
#	
#	<P>An SQL-92 Full level&#0150;conformant driver will always return this option as supported.</P>
#	</TD>
#	</TR>
    SQL_DROP_ASSERTION => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_DROP_CHARACTER_SET<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the clauses in the <B>DROP CHARACTER SET</B> statement, as defined in SQL-92, supported by the data source.
#	<P>The following bitmask is used to determine which clauses are supported:</P>
#	
#	<P>SQL_DCS_DROP_CHARACTER_SET </P>
#	
#	<P>An SQL-92 Full level&#0150;conformant driver will always return this option as supported.</P>
#	</TD>
#	</TR>
    SQL_DROP_CHARACTER_SET => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_DROP_COLLATION<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the clauses in the <B>DROP COLLATION</B> statement, as defined in SQL-92, supported by the data source.
#	<P>The following bitmask is used to determine which clauses are supported:</P>
#	
#	<P>SQL_DC_DROP_COLLATION </P>
#	
#	<P>An SQL-92 Full level&#0150;conformant driver will always return this option as supported.</P>
#	</TD>
#	</TR>
    SQL_DROP_COLLATION => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_DROP_DOMAIN<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the clauses in the <B>DROP DOMAIN</B> statement, as defined in SQL-92, supported by the data source.
#	<P>The following bitmasks are used to determine which clauses are supported:</P>
#	
#	<P>SQL_DD_DROP_DOMAIN<BR>
#	SQL_DD_CASCADE<BR>
#	SQL_DD_RESTRICT </P>
#	
#	<P>An SQL-92 Intermediate level&#0150;conformant driver will always return all of these options as supported.</P>
#	</TD>
#	</TR>
    SQL_DROP_DOMAIN => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_DROP_SCHEMA<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the clauses in the <B>DROP SCHEMA</B> statement, as defined in SQL-92, supported by the data source.
#	<P>The following bitmasks are used to determine which clauses are supported:</P>
#	
#	<P>SQL_DS_DROP_SCHEMA<BR>
#	SQL_DS_CASCADE<BR>
#	SQL_DS_RESTRICT </P>
#	
#	<P>An SQL-92 Intermediate level&#0150;conformant driver will always return all of these options as supported.</P>
#	</TD>
#	</TR>
    SQL_DROP_SCHEMA => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_DROP_TABLE<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the clauses in the <B>DROP TABLE</B> statement, as defined in SQL-92, supported by the data source.
#	<P>The following bitmasks are used to determine which clauses are supported:</P>
#	
#	<P>SQL_DT_DROP_TABLE<BR>
#	SQL_DT_CASCADE<BR>
#	SQL_DT_RESTRICT </P>
#	
#	<P>An FIPS Transitional level&#0150;conformant driver will always return all of these options as supported.</P>
#	</TD>
#	</TR>
    SQL_DROP_TABLE => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_DROP_TRANSLATION<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the clauses in the <B>DROP TRANSLATION</B> statement, as defined in SQL-92, supported by the data source.
#	<P>The following bitmask is used to determine which clauses are supported:</P>
#	
#	<P>SQL_DTR_DROP_TRANSLATION </P>
#	
#	<P>An SQL-92 Full level&#0150;conformant driver will always return this option as supported.</P>
#	</TD>
#	</TR>
    SQL_DROP_TRANSLATION => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_DROP_VIEW<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the clauses in the <B>DROP VIEW</B> statement, as defined in SQL-92, supported by the data source.
#	<P>The following bitmasks are used to determine which clauses are supported:</P>
#	
#	<P>SQL_DV_DROP_VIEW<BR>
#	SQL_DV_CASCADE<BR>
#	SQL_DV_RESTRICT </P>
#	
#	<P>An FIPS Transitional level&#0150;conformant driver will always return all of these options as supported.</P>
#	</TD>
#	</TR>
    SQL_DROP_VIEW => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_DYNAMIC_CURSOR_ATTRIBUTES1<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask that describes the attributes of a dynamic cursor that are supported by the driver. This bitmask contains the first subset of attributes; for the second subset, see SQL_DYNAMIC_CURSOR_ATTRIBUTES2.
#	<P>The following bitmasks are used to determine which attributes are supported:</P>
#	
#	<P>SQL_CA1_NEXT = A <I>FetchOrientation</I> argument of SQL_FETCH_NEXT is supported in a call to <B>SQLFetchScroll</B> when the cursor is a dynamic cursor.</P>
#	
#	<P>SQL_CA1_ABSOLUTE = <I>FetchOrientation</I> arguments of SQL_FETCH_FIRST, SQL_FETCH_LAST, and SQL_FETCH_ABSOLUTE are supported in a call to <B>SQLFetchScroll</B> when the cursor is a dynamic cursor. (The rowset that will be fetched is independent of the current cursor position.) </P>
#	
#	<P>SQL_CA1_RELATIVE = <I>FetchOrientation</I> arguments of SQL_FETCH_PRIOR and SQL_FETCH_RELATIVE are supported in a call to <B>SQLFetchScroll</B> when the cursor is a dynamic cursor. (The rowset that will be fetched is dependent on the current cursor position. Note that this is separated from SQL_FETCH_NEXT because in a forward-only cursor, only SQL_FETCH_NEXT is supported.) </P>
#	
#	<P>SQL_CA1_BOOKMARK = A <I>FetchOrientation</I> argument of SQL_FETCH_BOOKMARK is supported in a call to <B>SQLFetchScroll</B> when the cursor is a dynamic cursor. </P>
#	
#	<P>SQL_CA1_LOCK_EXCLUSIVE = A <I>LockType</I> argument of SQL_LOCK_EXCLUSIVE is supported in a call to <B>SQLSetPos</B> when the cursor is a dynamic cursor.</P>
#	
#	<P>SQL_CA1_LOCK_NO_CHANGE = A <I>LockType</I> argument of SQL_LOCK_NO_CHANGE is supported in a call to <B>SQLSetPos</B> when the cursor is a dynamic cursor. </P>
#	
#	<P>SQL_CA1_LOCK_UNLOCK = A <I>LockType</I> argument of SQL_LOCK_UNLOCK is supported in a call to <B>SQLSetPos</B> when the cursor is a dynamic cursor.</P>
#	
#	<P>SQL_CA1_POS_POSITION = An <I>Operation</I> argument of SQL_POSITION is supported in a call to <B>SQLSetPos</B> when the cursor is a dynamic cursor.</P>
#	
#	<P>SQL_CA1_POS_UPDATE = An <I>Operation</I> argument of SQL_UPDATE is supported in a call to <B>SQLSetPos</B> when the cursor is a dynamic cursor. </P>
#	
#	<P>SQL_CA1_POS_DELETE = An <I>Operation</I> argument of SQL_DELETE is supported in a call to <B>SQLSetPos</B> when the cursor is a dynamic cursor. </P>
#	
#	<P>SQL_CA1_POS_REFRESH = An <I>Operation</I> argument of SQL_REFRESH is supported in a call to <B>SQLSetPos</B> when the cursor is a dynamic cursor. </P>
#	
#	<P>SQL_CA1_POSITIONED_UPDATE = An UPDATE WHERE CURRENT OF SQL statement is supported when the cursor is a dynamic cursor. (An SQL-92 Entry level&#0150;conformant driver will always return this option as supported.)</P>
#	
#	<P>SQL_CA1_POSITIONED_DELETE = A DELETE WHERE CURRENT OF SQL statement is supported when the cursor is a dynamic cursor. (An SQL-92 Entry level&#0150;conformant driver will always return this option as supported.)</P>
#	
#	<P>SQL_CA1_SELECT_FOR_UPDATE = A SELECT FOR UPDATE SQL statement is supported when the cursor is a dynamic cursor. (An SQL-92 Entry level&#0150;conformant driver will always return this option as supported.)</P>
#	
#	<P>SQL_CA1_BULK_ADD = An <I>Operation</I> argument of SQL_ADD is supported in a call to <B>SQLBulkOperations</B> when the cursor is a dynamic cursor. </P>
#	
#	<P>SQL_CA1_BULK_UPDATE_BY_BOOKMARK = An <I>Operation</I> argument of SQL_UPDATE_BY_BOOKMARK is supported in a call to <B>SQLBulkOperations</B> when the cursor is a dynamic cursor. </P>
#	
#	<P>SQL_CA1_BULK_DELETE_BY_BOOKMARK = An <I>Operation</I> argument of SQL_DELETE_BY_BOOKMARK is supported in a call to <B>SQLBulkOperations</B> when the cursor is a dynamic cursor. </P>
#	
#	<P>SQL_CA1_BULK_FETCH_BY_BOOKMARK = An <I>Operation</I> argument of SQL_FETCH_BY_BOOKMARK is supported in a call to <B>SQLBulkOperations</B> when the cursor is a dynamic cursor. </P>
#	
#	<P>An SQL-92 Intermediate level&#0150;conformant driver will usually return the SQL_CA1_NEXT, SQL_CA1_ABSOLUTE, and SQL_CA1_RELATIVE options as supported, because it supports scrollable cursors through the embedded SQL FETCH statement. Because this does not directly determine the underlying SQL support, however, scrollable cursors may not be supported, even for an SQL-92 Intermediate level&#0150;conformant driver.</P>
#	</TD>
#	</TR>
    SQL_DYNAMIC_CURSOR_ATTRIBUTES1 => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_DYNAMIC_CURSOR_ATTRIBUTES2<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask that describes the attributes of a dynamic cursor that are supported by the driver. This bitmask contains the second subset of attributes; for the first subset, see SQL_DYNAMIC_CURSOR_ATTRIBUTES1. 
#	<P>The following bitmasks are used to determine which attributes are supported:</P>
#	
#	<P>SQL_CA2_READ_ONLY_CONCURRENCY = A read-only dynamic cursor, in which no updates are allowed, is supported. (The SQL_ATTR_CONCURRENCY statement attribute can be SQL_CONCUR_READ_ONLY for a dynamic cursor). </P>
#	
#	<P>SQL_CA2_LOCK_CONCURRENCY = A dynamic cursor that uses the lowest level of locking sufficient to ensure that the row can be updated is supported. (The SQL_ATTR_CONCURRENCY statement attribute can be SQL_CONCUR_LOCK for a dynamic cursor.) These locks must be consistent with the transaction isolation level set by the SQL_ATTR_TXN_ISOLATION connection attribute.</P>
#	
#	<P>SQL_CA2_OPT_ROWVER_CONCURRENCY = A dynamic cursor that uses the optimistic concurrency control comparing row versions is supported. (The SQL_ATTR_CONCURRENCY statement attribute can be SQL_CONCUR_ROWVER for a dynamic cursor.) </P>
#	
#	<P>SQL_CA2_OPT_VALUES_CONCURRENCY = A dynamic cursor that uses the optimistic concurrency control comparing values is supported. (The SQL_ATTR_CONCURRENCY statement attribute can be SQL_CONCUR_VALUES for a dynamic cursor.) </P>
#	
#	<P>SQL_CA2_SENSITIVITY_ADDITIONS = Added rows are visible to a dynamic cursor; the cursor can scroll to those rows. (Where these rows are added to the cursor is driver-dependent.) </P>
#	
#	<P>SQL_CA2_SENSITIVITY_DELETIONS = Deleted rows are no longer available to a dynamic cursor, and do not leave a "hole" in the result set; after the dynamic cursor scrolls from a deleted row, it cannot return to that row. </P>
#	
#	<P>SQL_CA2_SENSITIVITY_UPDATES = Updates to rows are visible to a dynamic cursor; if the dynamic cursor scrolls from and returns to an updated row, the data returned by the cursor is the updated data, not the original data. </P>
#	
#	<P>SQL_CA2_MAX_ROWS_SELECT = The SQL_ATTR_MAX_ROWS statement attribute affects <B>SELECT</B> statements when the cursor is a dynamic cursor. </P>
#	
#	<P>SQL_CA2_MAX_ROWS_INSERT = The SQL_ATTR_MAX_ROWS statement attribute affects <B>INSERT</B> statements when the cursor is a dynamic cursor. </P>
#	
#	<P>SQL_CA2_MAX_ROWS_DELETE = The SQL_ATTR_MAX_ROWS statement attribute affects <B>DELETE</B> statements when the cursor is a dynamic cursor. </P>
#	
#	<P>SQL_CA2_MAX_ROWS_UPDATE = The SQL_ATTR_MAX_ROWS statement attribute affects <B>UPDATE</B> statements when the cursor is a dynamic cursor. </P>
#	
#	<P>SQL_CA2_MAX_ROWS_CATALOG = The SQL_ATTR_MAX_ROWS statement attribute affects <B>CATALOG</B> result sets when the cursor is a dynamic cursor. </P>
#	
#	<P>SQL_CA2_MAX_ROWS_AFFECTS_ALL = The SQL_ATTR_MAX_ROWS statement attribute affects <B>SELECT</B>, <B>INSERT</B>, <B>DELETE</B>, and <B>UPDATE</B> statements, and <B>CATALOG</B> result sets, when the cursor is a dynamic cursor. </P>
#	
#	<P>SQL_CA2_CRC_EXACT = The exact row count is available in the SQL_DIAG_CURSOR_ROW_COUNT diagnostic field when the cursor is a dynamic cursor. </P>
#	
#	<P>SQL_CA2_CRC_APPROXIMATE = An approximate row count is available in the SQL_DIAG_CURSOR_ROW_COUNT diagnostic field when the cursor is a dynamic cursor. </P>
#	
#	<P>SQL_CA2_SIMULATE_NON_UNIQUE = The driver does not guarantee that simulated positioned update or delete statements will affect only one row when the cursor is a dynamic cursor; it is the application's responsibility to guarantee this. (If a statement affects more than one row, <B>SQLExecute</B> or <B>SQLExecDirect</B> returns SQLSTATE 01001 [Cursor operation conflict].) To set this behavior, the application calls <B>SQLSetStmtAttr</B> with the SQL_ATTR_SIMULATE_CURSOR attribute set to SQL_SC_NON_UNIQUE. </P>
#	
#	<P>SQL_CA2_SIMULATE_TRY_UNIQUE = The driver attempts to guarantee that simulated positioned update or delete statements will affect only one row when the cursor is a dynamic cursor. The driver always executes such statements, even if they might affect more than one row, such as when there is no unique key. (If a statement affects more than one row, <B>SQLExecute</B> or <B>SQLExecDirect</B> returns SQLSTATE 01001 [Cursor operation conflict].) To set this behavior, the application calls <B>SQLSetStmtAttr</B> with the SQL_ATTR_SIMULATE_CURSOR attribute set to SQL_SC_TRY_UNIQUE. </P>
#	
#	<P>SQL_CA2_SIMULATE_UNIQUE = The driver guarantees that simulated positioned update or delete statements will affect only one row when the cursor is a dynamic cursor. If the driver cannot guarantee this for a given statement, <B>SQLExecDirect</B> or <B>SQLPrepare</B> return SQLSTATE 01001 (Cursor operation conflict). To set this behavior, the application calls <B>SQLSetStmtAttr</B> with the SQL_ATTR_SIMULATE_CURSOR attribute set to SQL_SC_UNIQUE.</P>
#	</TD>
#	</TR>
    SQL_DYNAMIC_CURSOR_ATTRIBUTES2 => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_EXPRESSIONS_IN_ORDERBY<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>A character string: "Y" if the data source supports expressions in the <B>ORDER BY</B> list; "N" if it does not.</TD>
#	</TR>
    SQL_EXPRESSIONS_IN_ORDERBY => {
	type => q(Char),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_FILE_USAGE<BR>
#	(ODBC 2.0)</TD>
#	<TD width=50%>An SQLUSMALLINT value indicating how a single-tier driver directly treats files in a data source:
#	<P>SQL_FILE_NOT_SUPPORTED = The driver is not a single-tier driver. For example, an ORACLE driver is a two-tier driver.</P>
#	
#	<P>SQL_FILE_TABLE = A single-tier driver treats files in a data source as tables. For example, an Xbase driver treats each Xbase file as a table.</P>
#	
#	<P>SQL_FILE_CATALOG = A single-tier driver treats files in a data source as a catalog. For example, a Microsoft&reg; Access driver treats each Microsoft Access file as a complete database.</P>
#	
#	<P>An application might use this to determine how users will select data. For example, Xbase users often think of data as stored in files, while ORACLE and MicrosoftAccess users generally think of data as stored in tables.</P>
#	
#	<P>When a user selects an Xbase data source, the application could display the Windows <B>File Open</B> common dialog box; when the user selects a Microsoft Access or ORACLE data source, the application could display a custom <B>Select Table</B> dialog box.</P>
#	</TD>
#	</TR>
    SQL_FILE_USAGE => {
	type => q(Short),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES1<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask that describes the attributes of a forward-only cursor that are supported by the driver. This bitmask contains the first subset of attributes; for the second subset, see SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES2. 
#	<P>The following bitmasks are used to determine which attributes are supported:</P>
#	
#	<P>SQL_CA1_NEXT<BR>
#	SQL_CA1_LOCK_EXCLUSIVE<BR>
#	SQL_CA1_LOCK_NO_CHANGE<BR>
#	SQL_CA1_LOCK_UNLOCK<BR>
#	SQL_CA1_POS_POSITION<BR>
#	SQL_CA1_POS_UPDATE<BR>
#	SQL_CA1_POS_DELETE<BR>
#	SQL_CA1_POS_REFRESH<BR>
#	SQL_CA1_POSITIONED_UPDATE<BR>
#	SQL_CA1_POSITIONED_DELETE<BR>
#	SQL_CA1_SELECT_FOR_UPDATE<BR>
#	SQL_CA1_BULK_ADD<BR>
#	SQL_CA1_BULK_UPDATE_BY_BOOKMARK<BR>
#	SQL_CA1_BULK_DELETE_BY_BOOKMARK<BR>
#	SQL_CA1_BULK_FETCH_BY_BOOKMARK</P>
#	
#	<P>For descriptions of these bitmasks, see SQL_DYNAMIC_CURSOR_ATTRIBUTES1 (and substitute "forward-only cursor" for "dynamic cursor" in the descriptions). </P>
#	</TD>
#	</TR>
    SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES1 => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES2<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask that describes the attributes of a forward-only cursor that are supported by the driver. This bitmask contains the second subset of attributes; for the first subset, see SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES1. 
#	<P>The following bitmasks are used to determine which attributes are supported:</P>
#	
#	<P>SQL_CA2_READ_ONLY_CONCURRENCY<BR>
#	SQL_CA2_LOCK_CONCURRENCY<BR>
#	SQL_CA2_OPT_ROWVER_CONCURRENCY<BR>
#	SQL_CA2_OPT_VALUES_CONCURRENCY<BR>
#	SQL_CA2_SENSITIVITY_ADDITIONS<BR>
#	SQL_CA2_SENSITIVITY_DELETIONS<BR>
#	SQL_CA2_SENSITIVITY_UPDATES<BR>
#	SQL_CA2_MAX_ROWS_SELECT<BR>
#	SQL_CA2_MAX_ROWS_INSERT<BR>
#	SQL_CA2_MAX_ROWS_DELETE<BR>
#	SQL_CA2_MAX_ROWS_UPDATE<BR>
#	SQL_CA2_MAX_ROWS_CATALOG<BR>
#	SQL_CA2_MAX_ROWS_AFFECTS_ALL<BR>
#	SQL_CA2_CRC_EXACT<BR>
#	SQL_CA2_CRC_APPROXIMATE<BR>
#	SQL_CA2_SIMULATE_NON_UNIQUE<BR>
#	SQL_CA2_SIMULATE_TRY_UNIQUE<BR>
#	SQL_CA2_SIMULATE_UNIQUE </P>
#	
#	<P>For descriptions of these bitmasks, see SQL_DYNAMIC_CURSOR_ATTRIBUTES2 (and substitute "forward-only cursor" for "dynamic cursor" in the descriptions).</P>
#	</TD>
#	</TR>
    SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES2 => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_GETDATA_EXTENSIONS<BR>
#	(ODBC 2.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating extensions to <B>SQLGetData</B>.
#	<P>The following bitmasks are used in conjunction with the flag to determine what common extensions the driver supports for <B>SQLGetData</B>:</P>
#	
#	<P>SQL_GD_ANY_COLUMN = <B>SQLGetData</B> can be called for any unbound column, including those before the last bound column. Note that the columns must be called in order of ascending column number unless SQL_GD_ANY_ORDER is also returned.</P>
#	
#	<P>SQL_GD_ANY_ORDER = <B>SQLGetData</B> can be called for unbound columns in any order. Note that <B>SQLGetData</B> can be called only for columns after the last bound column unless SQL_GD_ANY_COLUMN is also returned.</P>
#	
#	<P>SQL_GD_BLOCK = <B>SQLGetData</B> can be called for an unbound column in any row in a block (where the rowset size is greater than 1) of data after positioning to that row with <B>SQLSetPos</B>.</P>
#	
#	<P>SQL_GD_BOUND = <B>SQLGetData</B> can be called for bound columns as well as unbound columns. A driver cannot return this value unless it also returns SQL_GD_ANY_COLUMN.</P>
#	
#	<P><B>SQLGetData</B> is required to return data only from unbound columns that occur after the last bound column, are called in order of increasing column number, and are not in a row in a block of rows.</P>
#	
#	<P>If a driver supports bookmarks (either fixed-length or variable-length), it must support calling <B>SQLGetData</B> on column 0. This support is required regardless of what the driver returns for a call to <B>SQLGetInfo</B> with the SQL_GETDATA_EXTENSIONS <I>InfoType</I>.</P>
#	</TD>
#	</TR>
    SQL_GETDATA_EXTENSIONS => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_GROUP_BY<BR>
#	(ODBC 2.0)</TD>
#	<TD width=50%>An SQLUSMALLINT value specifying the relationship between the columns in the <B>GROUP BY</B> clause and the nonaggregated columns in the select list:
#	<P>SQL_GB_COLLATE = A <B>COLLATE</B> clause can be specified at the end of each grouping column. (ODBC 3.0)</P>
#	
#	<P>SQL_GB_NOT_SUPPORTED = <B>GROUP BY</B> clauses are not supported. (ODBC 2.0)</P>
#	
#	<P>SQL_GB_GROUP_BY_EQUALS_SELECT = The <B>GROUP BY</B> clause must contain all nonaggregated columns in the select list. It cannot contain any other columns. For example, <B>SELECT DEPT, MAX(SALARY) FROM EMPLOYEE GROUP BY DEPT</B>. (ODBC 2.0)</P>
#	
#	<P>SQL_GB_GROUP_BY_CONTAINS_SELECT = The <B>GROUP BY</B> clause must contain all nonaggregated columns in the select list. It can contain columns that are not in the select list. For example, <B>SELECT DEPT, MAX(SALARY) FROM EMPLOYEE GROUP BY DEPT, AGE</B>. (ODBC 2.0)</P>
#	
#	<P>SQL_GB_NO_RELATION = The columns in the <B>GROUP BY</B> clause and the select list are not related. The meaning of nongrouped, nonaggregated columns in the select list is data source&#0150;dependent. For example, <B>SELECT DEPT, SALARY FROM EMPLOYEE GROUP BY DEPT, AGE</B>. (ODBC 2.0)</P>
#	
#	<P>An SQL-92 Entry level&#0150;conformant driver will always return the SQL_GB_GROUP_BY_EQUALS_SELECT option as supported. An SQL-92 Full level&#0150;conformant driver will always return the SQL_GB_COLLATE option as supported. If none of the options is supported, the <B>GROUP BY</B> clause is not supported by the data source.</P>
#	</TD>
#	</TR>
    SQL_GROUP_BY => {
	type => q(Short),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_IDENTIFIER_CASE<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>An SQLUSMALLINT value as follows:
#	<P>SQL_IC_UPPER = Identifiers in SQL are not case-sensitive and are stored in uppercase in system catalog.</P>
#	
#	<P>SQL_IC_LOWER = Identifiers in SQL are not case-sensitive and are stored in lowercase in system catalog.</P>
#	
#	<P>SQL_IC_SENSITIVE = Identifiers in SQL are case-sensitive and are stored in mixed case in system catalog.</P>
#	
#	<P>SQL_IC_MIXED = Identifiers in SQL are not case-sensitive and are stored in mixed case in system catalog. </P>
#	
#	<P>Because identifiers in SQL-92 are never case-sensitive, a driver that conforms strictly to SQL-92 (any level) will never return the SQL_IC_SENSITIVE option as supported.</P>
#	</TD>
#	</TR>
    SQL_IDENTIFIER_CASE => {
	type => q(Short),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_IDENTIFIER_QUOTE_CHAR<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>The character string used as the starting and ending delimiter of a quoted (delimited) identifier in SQL statements. (Identifiers passed as arguments to ODBC functions do not need to be quoted.) If the data source does not support quoted identifiers, a blank is returned. 
#	<P>This character string can also be used for quoting catalog function arguments when the connection attribute SQL_ATTR_METADATA_ID is set to SQL_TRUE.</P>
#	
#	<P>Because the identifier quote character in SQL-92 is the double quotation mark ("), a driver that conforms strictly to SQL-92 will always return the double quotation mark character.</P>
#	</TD>
#	</TR>
    SQL_IDENTIFIER_QUOTE_CHAR => {
	type => q(Char),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_INDEX_KEYWORDS<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask that enumerates keywords in the CREATE INDEX statement that are supported by the driver:
#	<P>SQL_IK_NONE = None of the keywords is supported.</P>
#	
#	<P>SQL_IK_ASC = ASC keyword is supported.</P>
#	
#	<P>SQL_IK_DESC = DESC keyword is supported.</P>
#	
#	<P>SQL_IK_ALL = All keywords are supported.</P>
#	
#	<P>To see if the CREATE INDEX statement is supported, an application calls <B>SQLGetInfo</B> with the SQL_DLL_INDEX information type.</P>
#	</TD>
#	</TR>
    SQL_INDEX_KEYWORDS => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_INFO_SCHEMA_VIEWS<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the views in the INFORMATION_SCHEMA that are supported by the driver. The views in, and the contents of, INFORMATION_SCHEMA are as defined in SQL-92. 
#	<P>The SQL-92 or FIPS conformance level at which this feature needs to be supported is shown in parentheses next to each bitmask.</P>
#	
#	<P>The following bitmasks are used to determine which views are supported:</P>
#	
#	<P>SQL_ISV_ASSERTIONS = Identifies the catalog's assertions that are owned by a given user. (Full level)</P>
#	
#	<P>SQL_ISV_CHARACTER_SETS = Identifies the catalog's character sets that are accessible to a given user. (Intermediate level)</P>
#	
#	<P>SQL_ISV_CHECK_CONSTRAINTS = Identifies the CHECK constraints that are owned by a given user. (Intermediate level)</P>
#	
#	<P>SQL_ISV_COLLATIONS = Identifies the character collations for the catalog that are accessible to a given user. (Full level)</P>
#	
#	<P>SQL_ISV_COLUMN_DOMAIN_USAGE = Identifies columns for the catalog that are dependent on domains defined in the catalog and are owned by a given user. (Intermediate level)</P>
#	
#	<P>SQL_ISV_COLUMN_PRIVILEGES = Identifies the privileges on columns of persistent tables that are available to or granted by a given user. (FIPS Transitional level)</P>
#	
#	<P>SQL_ISV_COLUMNS = Identifies the columns of persistent tables that are accessible to a given user. (FIPS Transitional level)</P>
#	
#	<P>SQL_ISV_CONSTRAINT_COLUMN_USAGE = Similar to CONSTRAINT_TABLE_USAGE view, columns are identified for the various constraints that are owned by a given user. (Intermediate level)</P>
#	
#	<P>SQL_ISV_CONSTRAINT_TABLE_USAGE = Identifies the tables that are used by constraints (referential, unique, and assertions), and are owned by a given user. (Intermediate level)</P>
#	
#	<P>SQL_ISV_DOMAIN_CONSTRAINTS = Identifies the domain constraints (of the domains in the catalog) that are accessible to a given user. (Intermediate level)</P>
#	
#	<P>SQL_ISV_DOMAINS = Identifies the domains defined in a catalog that are accessible to the user. (Intermediate level)</P>
#	
#	<P>SQL_ISV_KEY_COLUMN_USAGE = Identifies columns defined in the catalog that are constrained as keys by a given user. (Intermediate level)</P>
#	
#	<P>SQL_ISV_REFERENTIAL_CONSTRAINTS = Identifies the referential constraints that are owned by a given user. (Intermediate level)</P>
#	
#	<P>SQL_ISV_SCHEMATA = Identifies the schemas that are owned by a given user. (Intermediate level)</P>
#	
#	<P>SQL_ISV_SQL_LANGUAGES = Identifies the SQL conformance levels, options, and dialects supported by the SQL implementation. (Intermediate level)</P>
#	
#	<P>SQL_ISV_TABLE_CONSTRAINTS = Identifies the table constraints that are owned by a given user. (Intermediate level)</P>
#	
#	<P>SQL_ISV_TABLE_PRIVILEGES = Identifies the privileges on persistent tables that are available to or granted by a given user. (FIPS Transitional level)</P>
#	
#	<P>SQL_ISV_TABLES = Identifies the persistent tables defined in a catalog that are accessible to a given user. (FIPS Transitional level)</P>
#	
#	<P>SQL_ISV_TRANSLATIONS = Identifies character translations for the catalog that are accessible to a given user. (Full level) </P>
#	
#	<P>SQL_ISV_USAGE_PRIVILEGES = Identifies the USAGE privileges on catalog objects that are available to or owned by a given user. (FIPS Transitional level)</P>
#	
#	<P>SQL_ISV_VIEW_COLUMN_USAGE = Identifies the columns on which the catalog's views that are owned by a given user are dependent. (Intermediate level)</P>
#	
#	<P>SQL_ISV_VIEW_TABLE_USAGE = Identifies the tables on which the catalog's views that are owned by a given user are dependent. (Intermediate level) </P>
#	
#	<P>SQL_ISV_VIEWS = Identifies the viewed tables defined in this catalog that are accessible to a given user. (FIPS Transitional level)</P>
#	</TD>
#	</TR>
    SQL_INFO_SCHEMA_VIEWS => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_INSERT_STATEMENT<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask that indicates support for <B>INSERT</B> statements:
#	<P>SQL_IS_INSERT_LITERALS</P>
#	
#	<P>SQL_IS_INSERT_SEARCHED</P>
#	
#	<P>SQL_IS_SELECT_INTO </P>
#	
#	<P>An SQL-92 Entry level&#0150;conformant driver will always return all of these options as supported.</P>
#	</TD>
#	</TR>
    SQL_INSERT_STATEMENT => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_INTEGRITY<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>A character string: "Y" if the data source supports the Integrity Enhancement Facility; "N" if it does not. 
#	<P>This <I>InfoType</I> has been renamed for ODBC 3.0 from the ODBC 2.0 <I>InfoType</I> SQL_ODBC_SQL_OPT_IEF.</P>
#	</TD>
#	</TR>
    SQL_INTEGRITY => {
	type => q(YesNo),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_KEYSET_CURSOR_ATTRIBUTES1<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask that describes the attributes of a keyset cursor that are supported by the driver. This bitmask contains the first subset of attributes; for the second subset, see SQL_KEYSET_CURSOR_ATTRIBUTES2. 
#	<P>The following bitmasks are used to determine which attributes are supported: </P>
#	
#	<P>SQL_CA1_NEXT<BR>
#	SQL_CA1_ABSOLUTE<BR>
#	SQL_CA1_RELATIVE<BR>
#	SQL_CA1_BOOKMARK<BR>
#	SQL_CA1_LOCK_EXCLUSIVE<BR>
#	SQL_CA1_LOCK_NO_CHANGE<BR>
#	SQL_CA1_LOCK_UNLOCK<BR>
#	SQL_CA1_POS_POSITION<BR>
#	SQL_CA1_POS_UPDATE<BR>
#	SQL_CA1_POS_DELETE<BR>
#	SQL_CA1_POS_REFRESH<BR>
#	SQL_CA1_POSITIONED_UPDATE<BR>
#	SQL_CA1_POSITIONED_DELETE<BR>
#	SQL_CA1_SELECT_FOR_UPDATE<BR>
#	SQL_CA1_BULK_ADD<BR>
#	SQL_CA1_BULK_UPDATE_BY_BOOKMARK<BR>
#	SQL_CA1_BULK_DELETE_BY_BOOKMARK<BR>
#	SQL_CA1_BULK_FETCH_BY_BOOKMARK</P>
#	
#	<P>For descriptions of these bitmasks, see SQL_DYNAMIC_CURSOR_ATTRIBUTES1 (and substitute "keyset-driven cursor" for "dynamic cursor" in the descriptions).</P>
#	
#	<P>An SQL-92 Intermediate level&#0150;conformant driver will usually return the SQL_CA1_NEXT, SQL_CA1_ABSOLUTE, and SQL_CA1_RELATIVE options as supported, because the driver supports scrollable cursors through the embedded SQL FETCH statement. Because this does not directly determine the underlying SQL support, however, scrollable cursors may not be supported, even for an SQL-92 Intermediate level&#0150;conformant driver.</P>
#	</TD>
#	</TR>
    SQL_KEYSET_CURSOR_ATTRIBUTES1 => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_KEYSET_CURSOR_ATTRIBUTES2<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask that describes the attributes of a keyset cursor that are supported by the driver. This bitmask contains the second subset of attributes; for the first subset, see SQL_KEYSET_CURSOR_ATTRIBUTES1. 
#	<P>The following bitmasks are used to determine which attributes are supported:</P>
#	
#	<P>SQL_CA2_READ_ONLY_CONCURRENCY<BR>
#	SQL_CA2_LOCK_CONCURRENCY<BR>
#	SQL_CA2_OPT_ROWVER_CONCURRENCY<BR>
#	SQL_CA2_OPT_VALUES_CONCURRENCY<BR>
#	SQL_CA2_SENSITIVITY_ADDITIONS<BR>
#	SQL_CA2_SENSITIVITY_DELETIONS<BR>
#	SQL_CA2_SENSITIVITY_UPDATES<BR>
#	SQL_CA2_MAX_ROWS_SELECT<BR>
#	SQL_CA2_MAX_ROWS_INSERT<BR>
#	SQL_CA2_MAX_ROWS_DELETE<BR>
#	SQL_CA2_MAX_ROWS_UPDATE<BR>
#	SQL_CA2_MAX_ROWS_CATALOG<BR>
#	SQL_CA2_MAX_ROWS_AFFECTS_ALL<BR>
#	SQL_CA2_CRC_EXACT<BR>
#	SQL_CA2_CRC_APPROXIMATE<BR>
#	SQL_CA2_SIMULATE_NON_UNIQUE<BR>
#	SQL_CA2_SIMULATE_TRY_UNIQUE<BR>
#	SQL_CA2_SIMULATE_UNIQUE</P>
#	
#	<P>For descriptions of these bitmasks, see SQL_DYNAMIC_CURSOR_ATTRIBUTES1 (and substitute "keyset-driven cursor" for "dynamic cursor" in the descriptions).</P>
#	</TD>
#	</TR>
    SQL_KEYSET_CURSOR_ATTRIBUTES2 => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_KEYWORDS<BR>
#	(ODBC 2.0)</TD>
#	<TD width=50%>A character string containing a comma-separated list of all data source&#0150;specific keywords. This list does not contain keywords specific to ODBC or keywords used by both the data source and ODBC. This list represents all the reserved keywords; interoperable applications should not use these words in object names.
#	<P>For a list of ODBC keywords, see "<A HREF="odbclist_of_reserved_keywords.htm">List of Reserved Keywords</A>" in Appendix C, "SQL Grammar." The <B>#define</B> value SQL_ODBC_KEYWORDS contains a comma-separated list of ODBC keywords.</P>
#	</TD>
#	</TR>
    SQL_KEYWORDS => {
	type => q(Char),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_LIKE_ESCAPE_CLAUSE<BR>
#	(ODBC 2.0)</TD>
#	<TD width=50%>A character string: "Y" if the data source supports an escape character for the percent character (%) and underscore character (_) in a <B>LIKE</B> predicate and the driver supports the ODBC syntax for defining a <B>LIKE</B> predicate escape character; "N" otherwise.</TD>
#	</TR>
    SQL_LIKE_ESCAPE_CLAUSE => {
	type => q(YesNo),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_MAX_ASYNC_CONCURRENT_STATEMENTS<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER value specifying the maximum number of active concurrent statements in asynchronous mode that the driver can support on a given connection. If there is no specific limit or the limit is unknown, this value is zero.</TD>
#	</TR>
    SQL_MAX_ASYNC_CONCURRENT_STATEMENTS => {
	type => q(Long),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_MAX_BINARY_LITERAL_LEN<BR>
#	(ODBC 2.0)</TD>
#	<TD width=50%>An SQLUINTEGER value specifying the maximum length (number of hexadecimal characters, excluding the literal prefix and suffix returned by <B>SQLGetTypeInfo</B>) of a binary literal in an SQL statement. For example, the binary literal 0xFFAA has a length of 4. If there is no maximum length or the length is unknown, this value is set to zero.</TD>
#	</TR>
    SQL_MAX_BINARY_LITERAL_LEN => {
	type => q(Long),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_MAX_CATALOG_NAME_LEN<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>An SQLUSMALLINT value specifying the maximum length of a catalog name in the data source. If there is no maximum length or the length is unknown, this value is set to zero. 
#	<P>An FIPS Full level&#0150;conformant driver will return at least 128.</P>
#	
#	<P>This <I>InfoType</I> has been renamed for ODBC 3.0 from the ODBC 2.0 <I>InfoType</I> SQL_MAX_QUALIFIER_NAME_LEN.</P>
#	</TD>
#	</TR>
    SQL_MAX_CATALOG_NAME_LEN => {
	type => q(Short),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_MAX_CHAR_LITERAL_LEN<BR>
#	(ODBC 2.0)</TD>
#	<TD width=50%>An SQLUINTEGER value specifying the maximum length (number of characters, excluding the literal prefix and suffix returned by <B>SQLGetTypeInfo</B>) of a character literal in an SQL statement. If there is no maximum length or the length is unknown, this value is set to zero.</TD>
#	</TR>
    SQL_MAX_CHAR_LITERAL_LEN => {
	type => q(Long),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_MAX_COLUMN_NAME_LEN<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>An SQLUSMALLINT value specifying the maximum length of a column name in the data source. If there is no maximum length or the length is unknown, this value is set to zero. 
#	<P>An FIPS Entry level&#0150;conformant driver will return at least 18. An FIPS Intermediate level&#0150;conformant driver will return at least 128.</P>
#	</TD>
#	</TR>
    SQL_MAX_COLUMN_NAME_LEN => {
	type => q(Short),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_MAX_COLUMNS_IN_GROUP_BY<BR>
#	(ODBC 2.0)</TD>
#	<TD width=50%>An SQLUSMALLINT value specifying the maximum number of columns allowed in a <B>GROUP BY</B> clause. If there is no specified limit or the limit is unknown, this value is set to zero. 
#	<P>An FIPS Entry level&#0150;conformant driver will return at least 6. An FIPS Intermediate level&#0150;conformant driver will return at least 15.</P>
#	</TD>
#	</TR>
    SQL_MAX_COLUMNS_IN_GROUP_BY => {
	type => q(Short),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_MAX_COLUMNS_IN_INDEX<BR>
#	(ODBC 2.0)</TD>
#	<TD width=50%>An SQLUSMALLINT value specifying the maximum number of columns allowed in an index. If there is no specified limit or the limit is unknown, this value is set to zero.</TD>
#	</TR>
    SQL_MAX_COLUMNS_IN_INDEX => {
	type => q(Short),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_MAX_COLUMNS_IN_ORDER_BY<BR>
#	(ODBC 2.0)</TD>
#	<TD width=50%>An SQLUSMALLINT value specifying the maximum number of columns allowed in an <B>ORDER BY</B> clause. If there is no specified limit or the limit is unknown, this value is set to zero. 
#	<P>An FIPS Entry level&#0150;conformant driver will return at least 6. An FIPS Intermediate level&#0150;conformant driver will return at least 15.</P>
#	</TD>
#	</TR>
    SQL_MAX_COLUMNS_IN_ORDER_BY => {
	type => q(Short),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_MAX_COLUMNS_IN_SELECT<BR>
#	(ODBC 2.0)</TD>
#	<TD width=50%>An SQLUSMALLINT value specifying the maximum number of columns allowed in a select list. If there is no specified limit or the limit is unknown, this value is set to zero. 
#	<P>An FIPS Entry level&#0150;conformant driver will return at least 100. An FIPS Intermediate level&#0150;conformant driver will return at least 250.</P>
#	</TD>
#	</TR>
    SQL_MAX_COLUMNS_IN_SELECT => {
	type => q(Short),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_MAX_COLUMNS_IN_TABLE<BR>
#	(ODBC 2.0)</TD>
#	<TD width=50%>An SQLUSMALLINT value specifying the maximum number of columns allowed in a table. If there is no specified limit or the limit is unknown, this value is set to zero. 
#	<P>An FIPS Entry level&#0150;conformant driver will return at least 100. An FIPS Intermediate level&#0150;conformant driver will return at least 250.</P>
#	</TD>
#	</TR>
    SQL_MAX_COLUMNS_IN_TABLE => {
	type => q(Short),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_MAX_CONCURRENT_ACTIVITIES<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>An SQLUSMALLINT value specifying the maximum number of active statements that the driver can support for a connection. A statement is defined as active if it has results pending, with the term "results" meaning rows from a <B>SELECT</B> operation or rows affected by an <B>INSERT</B>, <B>UPDATE</B>, or <B>DELETE</B> operation (such as a row count), or if it is in a NEED_DATA state. This value can reflect a limitation imposed by either the driver or the data source. If there is no specified limit or the limit is unknown, this value is set to zero. 
#	<P>This <I>InfoType</I> has been renamed for ODBC 3.0 from the ODBC 2.0 <I>InfoType</I> SQL_ACTIVE_STATEMENTS.</P>
#	</TD>
#	</TR>
    SQL_MAX_CONCURRENT_ACTIVITIES => {
	type => q(Short),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_MAX_CURSOR_NAME_LEN<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>An SQLUSMALLINT value specifying the maximum length of a cursor name in the data source. If there is no maximum length or the length is unknown, this value is set to zero. 
#	<P>An FIPS Entry level&#0150;conformant driver will return at least 18. An FIPS Intermediate level&#0150;conformant driver will return at least 128.</P>
#	</TD>
#	</TR>
    SQL_MAX_CURSOR_NAME_LEN => {
	type => q(Short),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_MAX_DRIVER_CONNECTIONS<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>An SQLUSMALLINT value specifying the maximum number of active connections that the driver can support for an environment. This value can reflect a limitation imposed by either the driver or the data source. If there is no specified limit or the limit is unknown, this value is set to zero. 
#	<P>This <I>InfoType</I> has been renamed for ODBC 3.0 from the ODBC 2.0 <I>InfoType</I> SQL_ACTIVE_CONNECTIONS.</P>
#	</TD>
#	</TR>
    SQL_MAX_DRIVER_CONNECTIONS => {
	type => q(Short),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_MAX_IDENTIFIER_LEN<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUSMALLINT that indicates the maximum size in characters that the data source supports for user-defined names. 
#	<P>An FIPS Entry level&#0150;conformant driver will return at least 18. An FIPS Intermediate level&#0150;conformant driver will return at least 128.</P>
#	</TD>
#	</TR>
    SQL_MAX_IDENTIFIER_LEN => {
	type => q(Short),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_MAX_INDEX_SIZE<BR>
#	(ODBC 2.0)</TD>
#	<TD width=50%>An SQLUINTEGER value specifying the maximum number of bytes allowed in the combined fields of an index. If there is no specified limit or the limit is unknown, this value is set to zero.</TD>
#	</TR>
    SQL_MAX_INDEX_SIZE => {
	type => q(Long),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_MAX_PROCEDURE_NAME_LEN<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>An SQLUSMALLINT value specifying the maximum length of a procedure name in the data source. If there is no maximum length or the length is unknown, this value is set to zero.</TD>
#	</TR>
    SQL_MAX_PROCEDURE_NAME_LEN => {
	type => q(Short),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_MAX_ROW_SIZE<BR>
#	(ODBC 2.0)</TD>
#	<TD width=50%>An SQLUINTEGER value specifying the maximum length of a single row in a table. If there is no specified limit or the limit is unknown, this value is set to zero. 
#	<P>An FIPS Entry level&#0150;conformant driver will return at least 2,000. An FIPS Intermediate level&#0150;conformant driver will return at least 8,000.</P>
#	</TD>
#	</TR>
    SQL_MAX_ROW_SIZE => {
	type => q(Long),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_MAX_ROW_SIZE_INCLUDES_LONG<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>A character string: "Y" if the maximum row size returned for the SQL_MAX_ROW_SIZE information type includes the length of all SQL_LONGVARCHAR and SQL_LONGVARBINARY columns in the row; "N" otherwise.</TD>
#	</TR>
    SQL_MAX_ROW_SIZE_INCLUDES_LONG => {
	type => q(YesNo),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_MAX_SCHEMA_NAME_LEN<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>An SQLUSMALLINT value specifying the maximum length of a schema name in the data source. If there is no maximum length or the length is unknown, this value is set to zero. 
#	<P>An FIPS Entry level&#0150;conformant driver will return at least 18. An FIPS Intermediate level&#0150;conformant driver will return at least 128.</P>
#	
#	<P>This <I>InfoType</I> has been renamed for ODBC 3.0 from the ODBC 2.0 <I>InfoType</I> SQL_MAX_OWNER_NAME_LEN.</P>
#	</TD>
#	</TR>
    SQL_MAX_SCHEMA_NAME_LEN => {
	type => q(Short),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_MAX_STATEMENT_LEN<BR>
#	(ODBC 2.0)</TD>
#	<TD width=50%>An SQLUINTEGER value specifying the maximum length (number of characters, including white space) of an SQL statement. If there is no maximum length or the length is unknown, this value is set to zero.</TD>
#	</TR>
    SQL_MAX_STATEMENT_LEN => {
	type => q(Long),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_MAX_TABLE_NAME_LEN<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>An SQLUSMALLINT value specifying the maximum length of a table name in the data source. If there is no maximum length or the length is unknown, this value is set to zero. 
#	<P>An FIPS Entry level&#0150;conformant driver will return at least 18. An FIPS Intermediate level&#0150;conformant driver will return at least 128.</P>
#	</TD>
#	</TR>
    SQL_MAX_TABLE_NAME_LEN => {
	type => q(Short),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_MAX_TABLES_IN_SELECT<BR>
#	(ODBC 2.0)</TD>
#	<TD width=50%>An SQLUSMALLINT value specifying the maximum number of tables allowed in the <B>FROM</B> clause of a <B>SELECT </B>statement. If there is no specified limit or the limit is unknown, this value is set to zero. 
#	<P>An FIPS Entry level&#0150;conformant driver will return at least 15. An FIPS Intermediate level&#0150;conformant driver will return at least 50.</P>
#	</TD>
#	</TR>
    SQL_MAX_TABLES_IN_SELECT => {
	type => q(Short),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_MAX_USER_NAME_LEN<BR>
#	(ODBC 2.0)</TD>
#	<TD width=50%>An SQLUSMALLINT value specifying the maximum length of a user name in the data source. If there is no maximum length or the length is unknown, this value is set to zero.</TD>
#	</TR>
    SQL_MAX_USER_NAME_LEN => {
	type => q(Short),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_MULT_RESULT_SETS<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>A character string: "Y" if the data source supports multiple result sets, "N" if it does not.
#	<P>For more information on multiple result sets, see "<A HREF="odbcmultiple_results.htm">Multiple Results</A>" in Chapter 11: Retrieving Results (Advanced).</P>
#	</TD>
#	</TR>
    SQL_MULT_RESULT_SETS => {
	type => q(YesNo),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_MULTIPLE_ACTIVE_TXN<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>A character string: "Y" if the driver supports more than one active transaction at the same time, "N" if only one transaction can be active at any time.
#	<P>The information returned for this information type does not apply in the case of distributed transactions.</P>
#	</TD>
#	</TR>
    SQL_MULTIPLE_ACTIVE_TXN => {
	type => q(YesNo),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_NEED_LONG_DATA_LEN<BR>
#	(ODBC 2.0)</TD>
#	<TD width=50%>A character string: "Y" if the data source needs the length of a long data value (the data type is SQL_LONGVARCHAR, SQL_LONGVARBINARY, or a long data source&#0150;specific data type) before that value is sent to the data source, "N" if it does not. For more information, see <B>SQLBindParameter</B> and <B>SQLSetPos</B>.</TD>
#	</TR>
    SQL_NEED_LONG_DATA_LEN => {
	type => q(YesNo),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_NON_NULLABLE_COLUMNS<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>An SQLUSMALLINT value specifying whether the data source supports NOT NULL in columns:
#	<P>SQL_NNC_NULL = All columns must be nullable.</P>
#	
#	<P>SQL_NNC_NON_NULL = Columns cannot be nullable. (The data source supports the <B>NOT NULL</B> column constraint in <B>CREATE TABLE</B> statements.) </P>
#	
#	<P>An SQL-92 Entry level&#0150;conformant driver will return SQL_NNC_NON_NULL.</P>
#	</TD>
#	</TR>
    SQL_NON_NULLABLE_COLUMNS => {
	type => q(Short),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_NULL_COLLATION<BR>
#	(ODBC 2.0)</TD>
#	<TD width=50%>An SQLUSMALLINT value specifying where NULLs are sorted in a result set:
#	<P>SQL_NC_END = NULLs are sorted at the end of the result set, regardless of the ASC or DESC keywords.</P>
#	
#	<P>SQL_NC_HIGH = NULLs are sorted at the high end of the result set, depending on the ASC or DESC keywords.</P>
#	
#	<P>SQL_NC_LOW = NULLs are sorted at the low end of the result set, depending on the ASC or DESC keywords.</P>
#	
#	<P>SQL_NC_START = NULLs are sorted at the start of the result set, regardless of the ASC or DESC keywords.</P>
#	</TD>
#	</TR>
    SQL_NULL_COLLATION => {
	type => q(Short),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_NUMERIC_FUNCTIONS<BR>
#	(ODBC 1.0)
#	<P>The information type was introduced in ODBC 1.0; each bitmask is labeled with the version in which it was introduced.</P>
#	</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the scalar numeric functions supported by the driver and associated data source. 
#	<P>The following bitmasks are used to determine which numeric functions are supported:</P>
#	
#	<P>SQL_FN_NUM_ABS (ODBC 1.0)<BR>
#	SQL_FN_NUM_ACOS (ODBC 1.0)<BR>
#	SQL_FN_NUM_ASIN (ODBC 1.0)<BR>
#	SQL_FN_NUM_ATAN (ODBC 1.0)<BR>
#	SQL_FN_NUM_ATAN2 (ODBC 1.0)<BR>
#	SQL_FN_NUM_CEILING (ODBC 1.0)<BR>
#	SQL_FN_NUM_COS (ODBC 1.0)<BR>
#	SQL_FN_NUM_COT (ODBC 1.0)<BR>
#	SQL_FN_NUM_DEGREES (ODBC 2.0)<BR>
#	SQL_FN_NUM_EXP (ODBC 1.0)<BR>
#	SQL_FN_NUM_FLOOR (ODBC 1.0)<BR>
#	SQL_FN_NUM_LOG (ODBC 1.0)<BR>
#	SQL_FN_NUM_LOG10 (ODBC 2.0)<BR>
#	SQL_FN_NUM_MOD (ODBC 1.0)<BR>
#	SQL_FN_NUM_PI (ODBC 1.0)<BR>
#	SQL_FN_NUM_POWER (ODBC 2.0)<BR>
#	SQL_FN_NUM_RADIANS (ODBC 2.0)<BR>
#	SQL_FN_NUM_RAND (ODBC 1.0)<BR>
#	SQL_FN_NUM_ROUND (ODBC 2.0)<BR>
#	SQL_FN_NUM_SIGN (ODBC 1.0)<BR>
#	SQL_FN_NUM_SIN (ODBC 1.0)<BR>
#	SQL_FN_NUM_SQRT (ODBC 1.0)<BR>
#	SQL_FN_NUM_TAN (ODBC 1.0)<BR>
#	SQL_FN_NUM_TRUNCATE (ODBC 2.0)</P>
#	</TD>
#	</TR>
    SQL_NUMERIC_FUNCTIONS => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_ODBC_INTERFACE_CONFORMANCE<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER value indicating the level of the ODBC 3<I>.x</I> interface that the driver conforms to.
#	<P>SQL_OIC_CORE: The minimum level that all ODBC drivers are expected to conform to. This level includes basic interface elements such as connection functions, functions for preparing and executing an SQL statement, basic result set metadata functions, basic catalog functions, and so on.</P>
#	
#	<P>SQL_OIC_LEVEL1: A level including the core standards compliance level functionality, plus scrollable cursors, bookmarks, positioned updates and deletes, and so on.</P>
#	
#	<P>SQL_OIC_LEVEL2: A level including level 1 standards compliance level functionality, plus advanced features such as sensitive cursors; update, delete, and refresh by bookmarks; stored procedure support; catalog functions for primary and foreign keys; multicatalog support; and so on.</P>
#	
#	<P>For more information, see "<A HREF="odbcinterface_conformance_levels.htm">Interface Conformance Levels</A>" in Chapter 4: ODBC Fundamentals.</P>
#	</TD>
#	</TR>
    SQL_ODBC_INTERFACE_CONFORMANCE => {
	type => q(Long),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_ODBC_VER<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>A character string with the version of ODBC to which the Driver Manager conforms. The version is of the form ##.##.0000, where the first two digits are the major version and the next two digits are the minor version. This is implemented solely in the Driver Manager.</TD>
#	</TR>
    SQL_ODBC_VER => {
	type => q(Char),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_OJ_CAPABILITIES<BR>
#	(ODBC 2.01)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the types of outer joins supported by the driver and data source. The following bitmasks are used to determine which types are supported:
#	<P>SQL_OJ_LEFT = Left outer joins are supported.</P>
#	
#	<P>SQL_OJ_RIGHT = Right outer joins are supported.</P>
#	
#	<P>SQL_OJ_FULL = Full outer joins are supported.</P>
#	
#	<P>SQL_OJ_NESTED = Nested outer joins are supported.</P>
#	
#	<P>SQL_OJ_NOT_ORDERED = The column names in the ON clause of the outer join do not have to be in the same order as their respective table names in the <B>OUTER JOIN </B>clause.</P>
#	
#	<P>SQL_OJ_INNER = The inner table (the right table in a left outer join or the left table in a right outer join) can also be used in an inner join. This does not apply to full outer joins, which do not have an inner table.</P>
#	
#	<P>SQL_OJ_ALL_COMPARISON_OPS = The comparison operator in the ON clause can be any of the ODBC comparison operators. If this bit is not set, only the equals (=) comparison operator can be used in outer joins.</P>
#	
#	<P>If none of these options is returned as supported, no outer join clause is supported.</P>
#	
#	<P>For information on the support of relational join operators in a SELECT statement, as defined by SQL-92, see SQL_SQL92_RELATIONAL_JOIN_OPERATORS.</P>
#	</TD>
#	</TR>
    SQL_OJ_CAPABILITIES => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_ORDER_BY_COLUMNS_IN_SELECT<BR>
#	(ODBC 2.0)</TD>
#	<TD width=50%>A character string: "Y" if the columns in the <B>ORDER BY</B> clause must be in the select list; otherwise, "N".</TD>
#	</TR>
    SQL_ORDER_BY_COLUMNS_IN_SELECT => {
	type => q(YesNo),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_PARAM_ARRAY_ROW_COUNTS<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER enumerating the driver's properties regarding the availability of row counts in a parameterized execution. Has the following values:
#	<P>SQL_PARC_BATCH = Individual row counts are available for each set of parameters. This is conceptually equivalent to the driver generating a batch of SQL statements, one for each parameter set in the array. Extended error information can be retrieved by using the SQL_PARAM_STATUS_PTR descriptor field.</P>
#	
#	<P>SQL_PARC_NO_BATCH = There is only one row count available, which is the cumulative row count resulting from the execution of the statement for the entire array of parameters. This is conceptually equivalent to treating the statement along with the entire parameter array as one atomic unit. Errors are handled the same as if one statement were executed.</P>
#	</TD>
#	</TR>
    SQL_PARAM_ARRAY_ROW_COUNTS => {
	type => q(Long),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_PARAM_ARRAY_SELECTS<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER enumerating the driver's properties regarding the availability of result sets in a parameterized execution. Has the following values:
#	<P>SQL_PAS_BATCH = There is one result set available per set of parameters. This is conceptually equivalent to the driver generating a batch of SQL statements, one for each parameter set in the array.</P>
#	
#	<P>SQL_PAS_NO_BATCH = There is only one result set available, which represents the cumulative result set resulting from the execution of the statement for the entire array of parameters. This is conceptually equivalent to treating the statement along with the entire parameter array as one atomic unit.</P>
#	
#	<P>SQL_PAS_NO_SELECT = A driver does not allow a result-set generating statement to be executed with an array of parameters.</P>
#	</TD>
#	</TR>
    SQL_PARAM_ARRAY_SELECTS => {
	type => q(Long),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_PROCEDURE_TERM<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>A character string with the data source vendor's name for a procedure; for example, "database procedure", "stored procedure", "procedure", "package", or "stored query".</TD>
#	</TR>
    SQL_PROCEDURE_TERM => {
	type => q(Char),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_PROCEDURES<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>A character string: "Y" if the data source supports procedures and the driver supports the ODBC procedure invocation syntax; "N" otherwise.</TD>
#	</TR>
    SQL_PROCEDURES => {
	type => q(YesNo),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_POS_OPERATIONS (ODBC 2.0)</TD>
#	<TD width=50%>An SQLINTEGER bitmask enumerating the support operations in <B>SQLSetPos</B>.
#	<P>The following bitmasks are used in conjunction with the flag to determine which options are supported.</P>
#	
#	<P>SQL_POS_POSITION (ODBC 2.0) SQL_POS_REFRESH (ODBC 2.0) SQL_POS_UPDATE (ODBC 2.0) SQL_POS_DELETE (ODBC 2.0) SQL_POS_ADD (ODBC 2.0) </P>
#	</TD>
#	</TR>
    SQL_POS_OPERATIONS => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_QUOTED_IDENTIFIER_CASE<BR>
#	(ODBC 2.0)</TD>
#	<TD width=50%>An SQLUSMALLINT value as follows:
#	<P>SQL_IC_UPPER = Quoted identifiers in SQL are not case-sensitive and are stored in uppercase in the system catalog.</P>
#	
#	<P>SQL_IC_LOWER = Quoted identifiers in SQL are not case-sensitive and are stored in lowercase in the system catalog.</P>
#	
#	<P>SQL_IC_SENSITIVE = Quoted identifiers in SQL are case-sensitive and are stored in mixed case in the system catalog. (In an SQL-92&#0150;compliant database, quoted identifiers are always case-sensitive.)</P>
#	
#	<P>SQL_IC_MIXED = Quoted identifiers in SQL are not case-sensitive and are stored in mixed case in the system catalog.</P>
#	
#	<P>An SQL-92 Entry level&#0150;conformant driver will always return SQL_IC_SENSITIVE.</P>
#	</TD>
#	</TR>
    SQL_QUOTED_IDENTIFIER_CASE => {
	type => q(Short),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_ROW_UPDATES<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>A character string: "Y" if a keyset-driven or mixed cursor maintains row versions or values for all fetched rows and therefore can detect any updates made to a row by any user since the row was last fetched. (This applies only to updates, not to deletions or insertions.) The driver can return the SQL_ROW_UPDATED flag to the row status array when <B>SQLFetchScroll</B> is called. Otherwise, "N".</TD>
#	</TR>
    SQL_ROW_UPDATES => {
	type => q(YesNo),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_SCHEMA_TERM<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>A character string with the data source vendor's name for an schema; for example, "owner", "Authorization ID", or "Schema".
#	<P>The character string can be returned in upper, lower, or mixed case.</P>
#	
#	<P>An SQL-92 Entry level&#0150;conformant driver will always return "schema".</P>
#	
#	<P>This <I>InfoType</I> has been renamed for ODBC 3.0 from the ODBC 2.0 <I>InfoType</I> SQL_OWNER_TERM.</P>
#	</TD>
#	</TR>
    SQL_SCHEMA_TERM => {
	type => q(Char),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_SCHEMA_USAGE<BR>
#	(ODBC 2.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the statements in which schemas can be used:
#	<P>SQL_SU_DML_STATEMENTS = Schemas are supported in all Data Manipulation Language statements: <B>SELECT</B>, <B>INSERT</B>, <B>UPDATE</B>, <B>DELETE</B>, and if supported, <B>SELECT FOR UPDATE</B> and positioned update and delete statements.</P>
#	
#	<P>SQL_SU_PROCEDURE_INVOCATION = Schemas are supported in the ODBC procedure invocation statement.</P>
#	
#	<P>SQL_SU_TABLE_DEFINITION = Schemas are supported in all table definition statements: <B>CREATE TABLE</B>, <B>CREATE VIEW</B>, <B>ALTER TABLE</B>, <B>DROP TABLE</B>, and <B>DROP VIEW</B>.</P>
#	
#	<P>SQL_SU_INDEX_DEFINITION = Schemas are supported in all index definition statements: <B>CREATE INDEX</B> and <B>DROP INDEX</B>.</P>
#	
#	<P>SQL_SU_PRIVILEGE_DEFINITION = Schemas are supported in all privilege definition statements: <B>GRANT</B> and <B>REVOKE</B>. </P>
#	
#	<P>An SQL-92 Entry level&#0150;conformant driver will always return the SQL_SU_DML_STATEMENTS, SQL_SU_TABLE_DEFINITION, and SQL_SU_PRIVILEGE_DEFINITION options, as supported.</P>
#	
#	<P>This <I>InfoType</I> has been renamed for ODBC 3.0 from the ODBC 2.0 <I>InfoType</I> SQL_OWNER_USAGE.</P>
#	</TD>
#	</TR>
    SQL_SCHEMA_USAGE => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_SCROLL_OPTIONS<BR>
#	(ODBC 1.0)
#	<P>The information type was introduced in ODBC 1.0; each bitmask is labeled with the version in which it was introduced.</P>
#	</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the scroll options supported for scrollable cursors.
#	<P>The following bitmasks are used to determine which options are supported:</P>
#	
#	<P>SQL_SO_FORWARD_ONLY = The cursor only scrolls forward. (ODBC 1.0)</P>
#	
#	<P>SQL_SO_STATIC = The data in the result set is static. (ODBC 2.0)</P>
#	
#	<P>SQL_SO_KEYSET_DRIVEN = The driver saves and uses the keys for every row in the result set. (ODBC 1.0)</P>
#	
#	<P>SQL_SO_DYNAMIC = The driver keeps the keys for every row in the rowset (the keyset size is the same as the rowset size). (ODBC 1.0)</P>
#	
#	<P>SQL_SO_MIXED = The driver keeps the keys for every row in the keyset, and the keyset size is greater than the rowset size. The cursor is keyset-driven inside the keyset and dynamic outside the keyset. (ODBC 1.0)</P>
#	
#	<P>For information about scrollable cursors, see "<A HREF="odbcscrollable_cursors.htm">Scrollable Cursors</A>" in Chapter 11: Retrieving Results (Advanced)</P>
#	</TD>
#	</TR>
    SQL_SCROLL_OPTIONS => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_SEARCH_PATTERN_ESCAPE<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>A character string specifying what the driver supports as an escape character that permits the use of the pattern match metacharacters underscore (_) and percent sign (%) as valid characters in search patterns. This escape character applies only for those catalog function arguments that support search strings. If this string is empty, the driver does not support a search-pattern escape character.
#	<P>Because this information type does not indicate general support of the escape character in the <B>LIKE</B> predicate, SQL-92 does not include requirements for this character string.</P>
#	
#	<P>This <I>InfoType</I> is limited to catalog functions. For a description of the use of the escape character in search pattern strings, see "<A HREF="odbcpattern_value_arguments.htm">Pattern Value Arguments</A>" in Chapter 7: Catalog Functions.</P>
#	</TD>
#	</TR>
    SQL_SEARCH_PATTERN_ESCAPE => {
	type => q(Char),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_SERVER_NAME<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>A character string with the actual data source&#0150;specific server name; useful when a data source name is used during <B>SQLConnect</B>, <B>SQLDriverConnect</B>, and<B> SQLBrowseConnect</B>.</TD>
#	</TR>
    SQL_SERVER_NAME => {
	type => q(Char),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_SPECIAL_CHARACTERS<BR>
#	(ODBC 2.0)</TD>
#	<TD width=50%>A character string containing all special characters (that is, all characters except a through z, A through Z, 0 through 9, and underscore) that can be used in an identifier name, such as a table name, column column name, or index name, on the data source. For example, "#$^". If an identifier contains one or more of these characters, the identifier must be a delimited identifier.</TD>
#	</TR>
    SQL_SPECIAL_CHARACTERS => {
	type => q(Char),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_SQL_CONFORMANCE<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER value indicating the level of SQL-92 supported by the driver: 
#	<P>SQL_SC_SQL92_ENTRY = Entry level SQL-92 compliant.</P>
#	
#	<P>SQL_SC_FIPS127_2_TRANSITIONAL = FIPS 127-2 transitional level compliant.</P>
#	
#	<P>SQL_SC_SQL92_FULL = Full level SQL-92 compliant.</P>
#	
#	<P>SQL_SC_ SQL92_INTERMEDIATE = Intermediate level SQL-92 compliant.</P>
#	</TD>
#	</TR>
    SQL_SQL_CONFORMANCE => {
	type => q(Long),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_SQL92_DATETIME_FUNCTIONS<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the datetime scalar functions that are supported by the driver and the associated data source, as defined in SQL-92.
#	<P>The following bitmasks are used to determine which datetime functions are supported:</P>
#	
#	<P>SQL_SDF_CURRENT_DATE<BR>
#	SQL_SDF_CURRENT_TIME<BR>
#	SQL_SDF_CURRENT_TIMESTAMP</P>
#	</TD>
#	</TR>
    SQL_SQL92_DATETIME_FUNCTIONS => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_SQL92_FOREIGN_KEY_DELETE_RULE<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the rules supported for a foreign key in a <B>DELETE</B> statement, as defined in SQL-92.
#	<P>The following bitmasks are used to determine which clauses are supported by the data source:</P>
#	
#	<P>SQL_SFKD_CASCADE<BR>
#	SQL_SFKD_NO_ACTION<BR>
#	SQL_SFKD_SET_DEFAULT<BR>
#	SQL_SFKD_SET_NULL</P>
#	
#	<P>An FIPS Transitional level&#0150;conformant driver will always return all of these options as supported.</P>
#	</TD>
#	</TR>
    SQL_SQL92_FOREIGN_KEY_DELETE_RULE => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_SQL92_FOREIGN_KEY_UPDATE_RULE<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the rules supported for a foreign key in an <B>UPDATE</B> statement, as defined in SQL-92.
#	<P>The following bitmasks are used to determine which clauses are supported by the data source:</P>
#	
#	<P>SQL_SFKU_CASCADE<BR>
#	SQL_SFKU_NO_ACTION<BR>
#	SQL_SFKU_SET_DEFAULT<BR>
#	SQL_SFKU_SET_NULL </P>
#	
#	<P>An SQL-92 Full level&#0150;conformant driver will always return all of these options as supported.</P>
#	</TD>
#	</TR>
    SQL_SQL92_FOREIGN_KEY_UPDATE_RULE => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_SQL92_GRANT<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the clauses supported in the <B>GRANT</B> statement, as defined in SQL-92. 
#	<P>The SQL-92 or FIPS conformance level at which this feature needs to be supported is shown in parentheses next to each bitmask.</P>
#	
#	<P>The following bitmasks are used to determine which clauses are supported by the data source:</P>
#	
#	<P>SQL_SG_DELETE_TABLE (Entry level)<BR>
#	SQL_SG_INSERT_COLUMN (Intermediate level)<BR>
#	SQL_SG_INSERT_TABLE (Entry level) <BR>
#	SQL_SG_REFERENCES_TABLE (Entry level)<BR>
#	SQL_SG_REFERENCES_COLUMN (Entry level)<BR>
#	SQL_SG_SELECT_TABLE (Entry level)<BR>
#	SQL_SG_UPDATE_COLUMN (Entry level)<BR>
#	SQL_SG_UPDATE_TABLE (Entry level) <BR>
#	SQL_SG_USAGE_ON_DOMAIN (FIPS Transitional level)<BR>
#	SQL_SG_USAGE_ON_CHARACTER_SET (FIPS Transitional level)<BR>
#	SQL_SG_USAGE_ON_COLLATION (FIPS Transitional level)<BR>
#	SQL_SG_USAGE_ON_TRANSLATION (FIPS Transitional level)<BR>
#	SQL_SG_WITH_GRANT_OPTION (Entry level)</P>
#	</TD>
#	</TR>
    SQL_SQL92_GRANT => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_SQL92_NUMERIC_VALUE_FUNCTIONS<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the numeric value scalar functions that are supported by the driver and the associated data source, as defined in SQL-92.
#	<P>The following bitmasks are used to determine which numeric functions are supported:</P>
#	
#	<P>SQL_SNVF_BIT_LENGTH<BR>
#	SQL_SNVF_CHAR_LENGTH<BR>
#	SQL_SNVF_CHARACTER_LENGTH<BR>
#	SQL_SNVF_EXTRACT<BR>
#	SQL_SNVF_OCTET_LENGTH<BR>
#	SQL_SNVF_POSITION</P>
#	</TD>
#	</TR>
    SQL_SQL92_NUMERIC_VALUE_FUNCTIONS => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_SQL92_PREDICATES<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the predicates supported in a <B>SELECT</B> statement, as defined in SQL-92. 
#	<P>The SQL-92 or FIPS conformance level at which this feature needs to be supported is shown in parentheses next to each bitmask.</P>
#	
#	<P>The following bitmasks are used to determine which options are supported by the data source:</P>
#	
#	<P>SQL_SP_BETWEEN (Entry level)<BR>
#	SQL_SP_COMPARISON (Entry level)<BR>
#	SQL_SP_EXISTS (Entry level)<BR>
#	SQL_SP_IN (Entry level)<BR>
#	SQL_SP_ISNOTNULL (Entry level)<BR>
#	SQL_SP_ISNULL (Entry level)<BR>
#	SQL_SP_LIKE (Entry level)<BR>
#	SQL_SP_MATCH_FULL (Full level)<BR>
#	SQL_SP_MATCH_PARTIAL(Full level)<BR>
#	SQL_SP_MATCH_UNIQUE_FULL (Full level)<BR>
#	SQL_SP_MATCH_UNIQUE_PARTIAL (Full level)<BR>
#	SQL_SP_OVERLAPS (FIPS Transitional level)<BR>
#	SQL_SP_QUANTIFIED_COMPARISON (Entry level)<BR>
#	SQL_SP_UNIQUE (Entry level)</P>
#	</TD>
#	</TR>
    SQL_SQL92_PREDICATES => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_SQL92_RELATIONAL_JOIN_OPERATORS<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the relational join operators supported in a <B>SELECT</B> statement, as defined in SQL-92. 
#	<P>The SQL-92 or FIPS conformance level at which this feature needs to be supported is shown in parentheses next to each bitmask.</P>
#	
#	<P>The following bitmasks are used to determine which options are supported by the data source:</P>
#	
#	<P>SQL_SRJO_CORRESPONDING_CLAUSE (Intermediate level)<BR>
#	SQL_SRJO_CROSS_JOIN (Full level)<BR>
#	SQL_SRJO_EXCEPT_JOIN (Intermediate level)<BR>
#	SQL_SRJO_FULL_OUTER_JOIN (Intermediate level) <BR>
#	SQL_SRJO_INNER_JOIN (FIPS Transitional level)<BR>
#	SQL_SRJO_INTERSECT_JOIN (Intermediate level)<BR>
#	SQL_SRJO_LEFT_OUTER_JOIN (FIPS Transitional level)<BR>
#	SQL_SRJO_NATURAL_JOIN (FIPS Transitional level)<BR>
#	SQL_SRJO_RIGHT_OUTER_JOIN (FIPS Transitional level)<BR>
#	SQL_SRJO_UNION_JOIN (Full level)</P>
#	
#	<P>SQL_SRJO_INNER_JOIN indicates support for the <B>INNER JOIN </B>syntax, not for the inner join capability. Support for the <B>INNER JOIN</B> syntax is FIPS TRANSITIONAL, while support for the inner join capability is <B>ENTRY</B>.</P>
#	</TD>
#	</TR>
    SQL_SQL92_RELATIONAL_JOIN_OPERATORS => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_SQL92_REVOKE<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the clauses supported in the <B>REVOKE</B> statement, as defined in SQL-92, supported by the data source. 
#	<P>The SQL-92 or FIPS conformance level at which this feature needs to be supported is shown in parentheses next to each bitmask.</P>
#	
#	<P>The following bitmasks are used to determine which clauses are supported by the data source:</P>
#	
#	<P>SQL_SR_CASCADE (FIPS Transitional level) <BR>
#	SQL_SR_DELETE_TABLE (Entry level)<BR>
#	SQL_SR_GRANT_OPTION_FOR (Intermediate level) <BR>
#	SQL_SR_INSERT_COLUMN (Intermediate level)<BR>
#	SQL_SR_INSERT_TABLE (Entry level)<BR>
#	SQL_SR_REFERENCES_COLUMN (Entry level)<BR>
#	SQL_SR_REFERENCES_TABLE (Entry level)<BR>
#	SQL_SR_RESTRICT (FIPS Transitional level)<BR>
#	SQL_SR_SELECT_TABLE (Entry level)<BR>
#	SQL_SR_UPDATE_COLUMN (Entry level)<BR>
#	SQL_SR_UPDATE_TABLE (Entry level)<BR>
#	SQL_SR_USAGE_ON_DOMAIN (FIPS Transitional level)<BR>
#	SQL_SR_USAGE_ON_CHARACTER_SET (FIPS Transitional level)<BR>
#	SQL_SR_USAGE_ON_COLLATION (FIPS Transitional level)<BR>
#	SQL_SR_USAGE_ON_TRANSLATION (FIPS Transitional level)</P>
#	</TD>
#	</TR>
    SQL_SQL92_REVOKE => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_SQL92_ROW_VALUE_CONSTRUCTOR<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the row value constructor expressions supported in a <B>SELECT</B> statement, as defined in SQL-92. The following bitmasks are used to determine which options are supported by the data source:
#	<P>SQL_SRVC_VALUE_EXPRESSION <BR>
#	SQL_SRVC_NULL <BR>
#	SQL_SRVC_DEFAULT <BR>
#	SQL_SRVC_ROW_SUBQUERY</P>
#	</TD>
#	</TR>
    SQL_SQL92_ROW_VALUE_CONSTRUCTOR => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_SQL92_STRING_FUNCTIONS<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the string scalar functions that are supported by the driver and the associated data source, as defined in SQL-92.
#	<P>The following bitmasks are used to determine which string functions are supported:</P>
#	
#	<P>SQL_SSF_CONVERT<BR>
#	SQL_SSF_LOWER<BR>
#	SQL_SSF_UPPER<BR>
#	SQL_SSF_SUBSTRING<BR>
#	SQL_SSF_TRANSLATE<BR>
#	SQL_SSF_TRIM_BOTH<BR>
#	SQL_SSF_TRIM_LEADING<BR>
#	SQL_SSF_TRIM_TRAILING</P>
#	</TD>
#	</TR>
    SQL_SQL92_STRING_FUNCTIONS => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_SQL92_VALUE_EXPRESSIONS<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the value expressions supported, as defined in SQL-92. 
#	<P>The SQL-92 or FIPS conformance level at which this feature needs to be supported is shown in parentheses next to each bitmask.</P>
#	
#	<P>The following bitmasks are used to determine which options are supported by the data source:</P>
#	
#	<P>SQL_SVE_CASE (Intermediate level)<BR>
#	SQL_SVE_CAST (FIPS Transitional level)<BR>
#	SQL_SVE_COALESCE (Intermediate level)<BR>
#	SQL_SVE_NULLIF (Intermediate level)</P>
#	</TD>
#	</TR>
    SQL_SQL92_VALUE_EXPRESSIONS => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_STANDARD_CLI_CONFORMANCE<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the CLI standard or standards to which the driver conforms. The following bitmasks are used to determine which levels the driver conforms to:
#	<P>SQL_SCC_XOPEN_CLI_VERSION1: The driver conforms to the X/Open CLI version 1.</P>
#	
#	<P>SQL_SCC_ISO92_CLI: The driver conforms to the ISO 92 CLI.</P>
#	</TD>
#	</TR>
    SQL_STANDARD_CLI_CONFORMANCE => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_STATIC_CURSOR_ATTRIBUTES1<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask that describes the attributes of a static cursor that are supported by the driver. This bitmask contains the first subset of attributes; for the second subset, see SQL_STATIC_CURSOR_ATTRIBUTES2. 
#	<P>The following bitmasks are used to determine which attributes are supported:</P>
#	
#	<P>SQL_CA1_NEXT<BR>
#	SQL_CA1_ABSOLUTE<BR>
#	SQL_CA1_RELATIVE<BR>
#	SQL_CA1_BOOKMARK<BR>
#	SQL_CA1_LOCK_NO_CHANGE<BR>
#	SQL_CA1_LOCK_EXCLUSIVE<BR>
#	SQL_CA1_LOCK_UNLOCK<BR>
#	SQL_CA1_POS_POSITION<BR>
#	SQL_CA1_POS_UPDATE<BR>
#	SQL_CA1_POS_DELETE<BR>
#	SQL_CA1_POS_REFRESH<BR>
#	SQL_CA1_POSITIONED_UPDATE<BR>
#	SQL_CA1_POSITIONED_DELETE<BR>
#	SQL_CA1_SELECT_FOR_UPDATE<BR>
#	SQL_CA1_BULK_ADD<BR>
#	SQL_CA1_BULK_UPDATE_BY_BOOKMARK<BR>
#	SQL_CA1_BULK_DELETE_BY_BOOKMARK<BR>
#	SQL_CA1_BULK_FETCH_BY_BOOKMARK</P>
#	
#	<P>For descriptions of these bitmasks, see SQL_DYNAMIC_CURSOR_ATTRIBUTES1 (and substitute "static cursor" for "dynamic cursor" in the descriptions).</P>
#	
#	<P>An SQL-92 Intermediate level&#0150;conformant driver will usually return the SQL_CA1_NEXT, SQL_CA1_ABSOLUTE, and SQL_CA1_RELATIVE options as supported, because the driver supports scrollable cursors through the embedded SQL FETCH statement. Because this does not directly determine the underlying SQL support, however, scrollable cursors may not be supported, even for an SQL-92 Intermediate level&#0150;conformant driver.</P>
#	</TD>
#	</TR>
    SQL_STATIC_CURSOR_ATTRIBUTES1 => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_STATIC_CURSOR_ATTRIBUTES2<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask that describes the attributes of a static cursor that are supported by the driver. This bitmask contains the second subset of attributes; for the first subset, see SQL_STATIC_CURSOR_ATTRIBUTES1. 
#	<P>The following bitmasks are used to determine which attributes are supported:</P>
#	
#	<P>SQL_CA2_READ_ONLY_CONCURRENCY<BR>
#	SQL_CA2_LOCK_CONCURRENCY<BR>
#	SQL_CA2_OPT_ROWVER_CONCURRENCY<BR>
#	SQL_CA2_OPT_VALUES_CONCURRENCY<BR>
#	SQL_CA2_SENSITIVITY_ADDITIONS<BR>
#	SQL_CA2_SENSITIVITY_DELETIONS<BR>
#	SQL_CA2_SENSITIVITY_UPDATES<BR>
#	SQL_CA2_MAX_ROWS_SELECT<BR>
#	SQL_CA2_MAX_ROWS_INSERT<BR>
#	SQL_CA2_MAX_ROWS_DELETE<BR>
#	SQL_CA2_MAX_ROWS_UPDATE<BR>
#	SQL_CA2_MAX_ROWS_CATALOG<BR>
#	SQL_CA2_MAX_ROWS_AFFECTS_ALL<BR>
#	SQL_CA2_CRC_EXACT<BR>
#	SQL_CA2_CRC_APPROXIMATE<BR>
#	SQL_CA2_SIMULATE_NON_UNIQUE<BR>
#	SQL_CA2_SIMULATE_TRY_UNIQUE<BR>
#	SQL_CA2_SIMULATE_UNIQUE</P>
#	
#	<P>For descriptions of these bitmasks, see SQL_DYNAMIC_CURSOR_ATTRIBUTES2 (and substitute "static cursor" for "dynamic cursor" in the descriptions).</P>
#	</TD>
#	</TR>
    SQL_STATIC_CURSOR_ATTRIBUTES2 => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_STRING_FUNCTIONS<BR>
#	(ODBC 1.0)
#	<P>The information type was introduced in ODBC 1.0; each bitmask is labeled with the version in which it was introduced.</P>
#	</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the scalar string functions supported by the driver and associated data source. 
#	<P>The following bitmasks are used to determine which string functions are supported:</P>
#	
#	<P>SQL_FN_STR_ASCII (ODBC 1.0)<BR>
#	SQL_FN_STR_BIT_LENGTH (ODBC 3.0)<BR>
#	SQL_FN_STR_CHAR (ODBC 1.0)<BR>
#	SQL_FN_STR_CHAR_<BR>
#	LENGTH (ODBC 3.0)<BR>
#	SQL_FN_STR_CHARACTER_<BR>
#	LENGTH (ODBC 3.0)<BR>
#	SQL_FN_STR_CONCAT (ODBC 1.0)<BR>
#	SQL_FN_STR_DIFFERENCE (ODBC 2.0)<BR>
#	SQL_FN_STR_INSERT (ODBC 1.0)<BR>
#	SQL_FN_STR_LCASE (ODBC 1.0)<BR>
#	SQL_FN_STR_LEFT (ODBC 1.0)<BR>
#	SQL_FN_STR_LENGTH (ODBC 1.0)<BR>
#	SQL_FN_STR_LOCATE (ODBC 1.0)<BR>
#	 SQL_FN_STR_LTRIM (ODBC 1.0) <BR>
#	SQL_FN_STR_OCTET_<BR>
#	LENGTH (ODBC 3.0) <BR>
#	SQL_FN_STR_POSITION (ODBC 3.0)<BR>
#	SQL_FN_STR_REPEAT (ODBC 1.0)<BR>
#	SQL_FN_STR_REPLACE (ODBC 1.0)<BR>
#	SQL_FN_STR_RIGHT (ODBC 1.0)<BR>
#	SQL_FN_STR_RTRIM (ODBC 1.0)<BR>
#	SQL_FN_STR_SOUNDEX (ODBC 2.0)<BR>
#	SQL_FN_STR_SPACE (ODBC 2.0)<BR>
#	SQL_FN_STR_SUBSTRING (ODBC 1.0)<BR>
#	SQL_FN_STR_UCASE (ODBC 1.0)</P>
#	
#	<P>If an application can call the <B>LOCATE</B> scalar function with the <I>string_exp1</I>, <I>string_exp2</I>, and <I>start</I> arguments, the driver returns the SQL_FN_STR_LOCATE bitmask. If an application can call the LOCATE scalar function with only the <I>string_exp1</I> and <I>string_exp2</I> arguments, the driver returns the SQL_FN_STR_LOCATE_2 bitmask. Drivers that fully support the <B>LOCATE</B> scalar function return both bitmasks.</P>
#	
#	<P>(For more information, see <A HREF="odbcstring_functions.htm">String Functions</A> in Appendix E, "Scalar Functions.")</P>
#	</TD>
#	</TR>
    SQL_STRING_FUNCTIONS => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_SUBQUERIES<BR>
#	(ODBC 2.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the predicates that support subqueries:
#	<P>SQL_SQ_CORRELATED_SUBQUERIES<BR>
#	SQL_SQ_COMPARISON<BR>
#	SQL_SQ_EXISTS<BR>
#	SQL_SQ_IN<BR>
#	SQL_SQ_QUANTIFIED</P>
#	
#	<P>The SQL_SQ_CORRELATED_SUBQUERIES bitmask indicates that all predicates that support subqueries support correlated subqueries.</P>
#	
#	<P>An SQL-92 Entry level&#0150;conformant driver will always return a bitmask in which all of these bits are set.</P>
#	</TD>
#	</TR>
    SQL_SUBQUERIES => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_SYSTEM_FUNCTIONS<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the scalar system functions supported by the driver and associated data source. 
#	<P>The following bitmasks are used to determine which system functions are supported:</P>
#	
#	<P>SQL_FN_SYS_DBNAME<BR>
#	SQL_FN_SYS_IFNULL<BR>
#	SQL_FN_SYS_USERNAME</P>
#	</TD>
#	</TR>
    SQL_SYSTEM_FUNCTIONS => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_TABLE_TERM<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>A character string with the data source vendor's name for a table; for example, "table" or "file".
#	<P>This character string can be in upper, lower, or mixed case. </P>
#	
#	<P>An SQL-92 Entry level&#0150;conformant driver will always return "table".</P>
#	</TD>
#	</TR>
    SQL_TABLE_TERM => {
	type => q(Char),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_TIMEDATE_ADD_INTERVALS<BR>
#	(ODBC 2.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the timestamp intervals supported by the driver and associated data source for the TIMESTAMPADD scalar function.
#	<P>The following bitmasks are used to determine which intervals are supported:</P>
#	
#	<P>SQL_FN_TSI_FRAC_SECOND<BR>
#	SQL_FN_TSI_SECOND<BR>
#	SQL_FN_TSI_MINUTE<BR>
#	SQL_FN_TSI_HOUR<BR>
#	SQL_FN_TSI_DAY<BR>
#	SQL_FN_TSI_WEEK<BR>
#	SQL_FN_TSI_MONTH<BR>
#	SQL_FN_TSI_QUARTER<BR>
#	SQL_FN_TSI_YEAR</P>
#	
#	<P>An FIPS Transitional level&#0150;conformant driver will always return a bitmask in which all of these bits are set.</P>
#	</TD>
#	</TR>
    SQL_TIMEDATE_ADD_INTERVALS => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_TIMEDATE_DIFF_INTERVALS<BR>
#	(ODBC 2.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the timestamp intervals supported by the driver and associated data source for the TIMESTAMPDIFF scalar function.
#	<P>The following bitmasks are used to determine which intervals are supported:</P>
#	
#	<P>SQL_FN_TSI_FRAC_SECOND<BR>
#	SQL_FN_TSI_SECOND<BR>
#	SQL_FN_TSI_MINUTE<BR>
#	SQL_FN_TSI_HOUR<BR>
#	SQL_FN_TSI_DAY<BR>
#	SQL_FN_TSI_WEEK<BR>
#	SQL_FN_TSI_MONTH<BR>
#	SQL_FN_TSI_QUARTER<BR>
#	SQL_FN_TSI_YEAR </P>
#	
#	<P>An FIPS Transitional level&#0150;conformant driver will always return a bitmask in which all of these bits are set.</P>
#	</TD>
#	</TR>
    SQL_TIMEDATE_DIFF_INTERVALS => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_TIMEDATE_FUNCTIONS<BR>
#	(ODBC 1.0)
#	<P>The information type was introduced in ODBC 1.0; each bitmask is labeled with the version in which it was introduced.</P>
#	</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the scalar date and time functions supported by the driver and associated data source. 
#	<P>The following bitmasks are used to determine which date and time functions are supported:</P>
#	
#	<P>SQL_FN_TD_CURRENT_DATE ODBC 3.0)<BR>
#	SQL_FN_TD_CURRENT_TIME (ODBC 3.0)<BR>
#	SQL_FN_TD_CURRENT_TIMESTAMP (ODBC 3.0)<BR>
#	SQL_FN_TD_CURDATE (ODBC 1.0)<BR>
#	SQL_FN_TD_CURTIME (ODBC 1.0) <BR>
#	SQL_FN_TD_DAYNAME (ODBC 2.0)<BR>
#	SQL_FN_TD_DAYOFMONTH (ODBC 1.0)<BR>
#	SQL_FN_TD_DAYOFWEEK (ODBC 1.0)<BR>
#	SQL_FN_TD_DAYOFYEAR (ODBC 1.0) <BR>
#	SQL_FN_TD_EXTRACT (ODBC 3.0)<BR>
#	SQL_FN_TD_HOUR (ODBC 1.0)<BR>
#	SQL_FN_TD_MINUTE (ODBC 1.0)<BR>
#	SQL_FN_TD_MONTH (ODBC 1.0)<BR>
#	SQL_FN_TD_MONTHNAME (ODBC 2.0)<BR>
#	SQL_FN_TD_NOW (ODBC 1.0)<BR>
#	SQL_FN_TD_QUARTER (ODBC 1.0)<BR>
#	SQL_FN_TD_SECOND (ODBC 1.0)<BR>
#	SQL_FN_TD_TIMESTAMPADD (ODBC 2.0)<BR>
#	SQL_FN_TD_TIMESTAMPDIFF (ODBC 2.0)<BR>
#	SQL_FN_TD_WEEK (ODBC 1.0)<BR>
#	SQL_FN_TD_YEAR (ODBC 1.0)</P>
#	</TD>
#	</TR>
    SQL_TIMEDATE_FUNCTIONS => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_TXN_CAPABLE<BR>
#	(ODBC 1.0)
#	<P>The information type was introduced in ODBC 1.0; each return value is labeled with the version in which it was introduced.</P>
#	</TD>
#	<TD width=50%>An SQLUSMALLINT value describing the transaction support in the driver or data source:
#	<P>SQL_TC_NONE = Transactions not supported. (ODBC 1.0)</P>
#	
#	<P>SQL_TC_DML = Transactions can contain only Data Manipulation Language (DML) statements (<B>SELECT</B>, <B>INSERT</B>, <B>UPDATE</B>, <B>DELETE</B>). Data Definition Language (DDL) statements encountered in a transaction cause an error. (ODBC 1.0)</P>
#	
#	<P>SQL_TC_DDL_COMMIT = Transactions can contain only DML statements. DDL statements (<B>CREATE TABLE</B>, <B>DROP INDEX</B>, and so on) encountered in a transaction cause the transaction to be committed. (ODBC 2.0)</P>
#	
#	<P>SQL_TC_DDL_IGNORE = Transactions can contain only DML statements. DDL statements encountered in a transaction are ignored. (ODBC 2.0)</P>
#	
#	<P>SQL_TC_ALL = Transactions can contain DDL statements and DML statements in any order. (ODBC 1.0) </P>
#	
#	<P>(Because support of transactions is mandatory in SQL-92, an SQL-92 conformant driver [any level] will never return SQL_TC_NONE.)</P>
#	</TD>
#	</TR>
    SQL_TXN_CAPABLE => {
	type => q(Short),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_TXN_ISOLATION_OPTION<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the transaction isolation levels available from the driver or data source. 
#	<P>The following bitmasks are used in conjunction with the flag to determine which options are supported:</P>
#	
#	<P>SQL_TXN_READ_UNCOMMITTED<BR>
#	SQL_TXN_READ_COMMITTED<BR>
#	SQL_TXN_REPEATABLE_READ<BR>
#	SQL_TXN_SERIALIZABLE</P>
#	
#	<P>For descriptions of these isolation levels, see the description of SQL_DEFAULT_TXN_ISOLATION.</P>
#	
#	<P>To set the transaction isolation level, an application calls <B>SQLSetConnectAttr</B> to set the SQL_ATTR_TXN_ISOLATION attribute. For more information, see <B>SQLSetConnectAttr</B>. </P>
#	
#	<P>An SQL-92 Entry level&#0150;conformant driver will always return SQL_TXN_SERIALIZABLE as supported. A FIPS Transitional level&#0150;conformant driver will always return all of these options as supported.</P>
#	</TD>
#	</TR>
    SQL_TXN_ISOLATION_OPTION => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_UNION<BR>
#	(ODBC 2.0)</TD>
#	<TD width=50%>An SQLUINTEGER bitmask enumerating the support for the <B>UNION</B> clause:
#	<P>SQL_U_UNION = The data source supports the <B>UNION</B> clause.</P>
#	
#	<P>SQL_U_UNION_ALL = The data source supports the <B>ALL</B> keyword in the <B>UNION</B> clause. (<B>SQLGetInfo</B> returns both SQL_U_UNION and SQL_U_UNION_ALL in this case.)</P>
#	
#	<P>An SQL-92 Entry level&#0150;conformant driver will always return both of these options as supported.</P>
#	</TD>
#	</TR>
    SQL_UNION => {
	type => q(Bitmask),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_USER_NAME<BR>
#	(ODBC 1.0)</TD>
#	<TD width=50%>A character string with the name used in a particular database, which can be different from the login name.</TD>
#	</TR>
    SQL_USER_NAME => {
	type => q(Char),
    },
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_XOPEN_CLI_YEAR<BR>
#	(ODBC 3.0)</TD>
#	<TD width=50%>A character string that indicates the year of publication of the X/Open specification with which the version of the ODBC Driver Manager fully complies.</TD>
#	</TR>
    SQL_XOPEN_CLI_YEAR => {
	type => q(Char),
    },
#	</table></div>
#	<!--TS:-->
#	<P class="label"><B>Code Example</B></P>
#	
#	<P><B>SQLGetInfo</B> returns lists of supported options as an SQLUINTEGER bitmask in *<I>InfoValuePtr</I>. The bitmask for each option is used in conjunction with the flag to determine whether the option is supported.</P>
#	
#	<P>For example, an application could use the following code to determine whether the SUBSTRING scalar function is supported by the driver associated with the connection:</P>
#	
#	<PRE class="code">SQLUINTEGER    fFuncs;
#	
#	SQLGetInfo(hdbc,
#	   SQL_STRING_FUNCTIONS,
#	   (SQLPOINTER)&amp;fFuncs,
#	   sizeof(fFuncs),
#	   NULL);
#	
#	if (fFuncs &amp; SQL_FN_STR_SUBSTRING)&nbsp;&nbsp;&nbsp;/* SUBSTRING supported */
#	      ;
#	else&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;/* SUBSTRING not supported */
#	      ;</PRE>
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
#	<TD width=50%>Returning the setting of a connection attribute</TD>
#	<TD width=50%><A HREF="odbcsqlgetconnectattr.htm">SQLGetConnectAttr</A></TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>Determining whether a driver supports a function</TD>
#	<TD width=50%><A HREF="odbcsqlgetfunctions.htm">SQLGetFunctions</A></TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>Returning the setting of a statement attribute</TD>
#	<TD width=50%><A HREF="odbcsqlgetstmtattr.htm">SQLGetStmtAttr</A></TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>Returning information about a data source's data types</TD>
#	<TD width=50%><A HREF="odbcsqlgettypeinfo.htm">SQLGetTypeInfo</A></TD>
#	</TR>
#	</table></div>
#	<!--TS:--><H4><A NAME="feedback"></A></H4>
#	<SPAN id="SDKFeedB"></SPAN>
#	</div>
#	
#	</BODY>
#	</HTML>
};

print <<END;
#include "HandleDbc.hpp"

HandleDbc::InfoTab
HandleDbc::m_infoTab[] = {
END

my @name = sort keys %$info;
for my $name (@name) {
    my $p = $info->{$name};
    my $type = $p->{type};
    $type =~ /^(Char|YesNo|Short|Long|Bitmask)$/
	or die "$name: bad type $type";
    my $defstr = $type eq "YesNo" ? q("N") : $type eq "Char" ? q("") : q(0);
    my $s = <<END;
    {	$name,
	InfoTab\::$type,
	0L,
	$defstr
    },
END
    if ($p->{omit}) {
	$s ="#if 0\n" . $s . "#endif\n";
    }
    print $s;
};

print <<END;
    {   0,
	InfoTab::End,
	0L,
	0
    }
};
END

# vim: set sw=4:
