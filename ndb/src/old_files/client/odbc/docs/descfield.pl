# usage perl Desc.data
# prints template for DescSpec.cpp
use strict;
my $order = 0;

# XXX do it later

#
# odbcsqlsetdescfield.htm
#
my $descSpec = {
#	<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 3.2 Final//EN">
#	<HTML DIR="LTR"><HEAD>
#	<META HTTP-EQUIV="Content-Type" Content="text/html; charset=Windows-1252">
#	<TITLE>SQLSetDescField</TITLE>
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
#	<H1><A NAME="odbcsqlsetdescfield"></A>SQLSetDescField</H1>
#	
#	<P class="label"><B>Conformance</B></P>
#	
#	<P>Version Introduced: ODBC 3.0<BR>
#	Standards Compliance: ISO 92</P>
#	
#	<P class="label"><B>Summary</B></P>
#	
#	<P><B>SQLSetDescField</B> sets the value of a single field of a descriptor record.</P>
#	
#	<P class="label"><B>Syntax</B></P>
#	
#	<PRE class="syntax">SQLRETURN <B>SQLSetDescField</B>(
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLHDESC&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>DescriptorHandle</I>,
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLSMALLINT&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>RecNumber</I>,
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLSMALLINT&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>FieldIdentifier</I>,
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLPOINTER&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>ValuePtr</I>,
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLINTEGER&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>BufferLength</I>);</PRE>
#	
#	<P class="label"><B>Arguments</B>
#	
#	<DL>
#	<DT><I>DescriptorHandle</I></DT>
#	
#	<DD>[Input]<BR>
#	Descriptor handle.</dd>
#	
#	<DT><I>RecNumber</I></DT>
#	
#	<DD>[Input]<BR>
#	Indicates the descriptor record containing the field that the application seeks to set. Descriptor records are numbered from 0, with record number 0 being the bookmark record. The <I>RecNumber</I> argument is ignored for header fields.</dd>
#	
#	<DT><I>FieldIdentifier</I></DT>
#	
#	<DD>[Input]<BR>
#	Indicates the field of the descriptor whose value is to be set. For more information, see "<I>FieldIdentifier</I> Argument" in the "Comments" section.</dd>
#	
#	<DT><I>ValuePtr</I></DT>
#	
#	<DD>[Input]<BR>
#	Pointer to a buffer containing the descriptor information, or a 4-byte value. The data type depends on the value of <I>FieldIdentifier</I>. If <I>ValuePtr</I> is a 4-byte value, either all four of the bytes are used or just two of the four are used, depending on the value of the <I>FieldIdentifier</I> argument.</dd>
#	
#	<DT><I>BufferLength</I></DT>
#	
#	<DD>[Input]<BR>
#	If <I>FieldIdentifier</I> is an ODBC-defined field and <I>ValuePtr</I> points to a character string or a binary buffer, this argument should be the length of *<I>ValuePtr</I>. If <I>FieldIdentifier</I> is an ODBC-defined field and <I>ValuePtr</I> is an integer, <I>BufferLength</I> is ignored.
#	
#	<P>If <I>FieldIdentifier</I> is a driver-defined field, the application indicates the nature of the field to the Driver Manager by setting the <I>BufferLength</I> argument. <I>BufferLength</I> can have the following values:
#	
#	
#	<UL type=disc>
#		<LI>If <I>ValuePtr</I> is a pointer to a character string, then <I>BufferLength</I> is the length of the string or SQL_NTS.</li>
#	
#		<LI>If <I>ValuePtr</I> is a pointer to a binary buffer, then the application places the result of the SQL_LEN_BINARY_ATTR(<I>length</I>) macro in <I>BufferLength</I>. This places a negative value in <I>BufferLength</I>.</li>
#	
#		<LI>If <I>ValuePtr</I> is a pointer to a value other than a character string or a binary string, then <I>BufferLength</I> should have the value SQL_IS_POINTER. </li>
#	
#		<LI>If <I>ValuePtr</I> contains a fixed-length value, then <I>BufferLength</I> is either SQL_IS_INTEGER, SQL_IS_UINTEGER, SQL_IS_SMALLINT, or SQL_IS_USMALLINT, as appropriate.</li>
#	</UL>
#	</dd>
#	</DL>
#	
#	<P class="label"><B>Returns</B></P>
#	
#	<P>SQL_SUCCESS, SQL_SUCCESS_WITH_INFO, SQL_ERROR, or SQL_INVALID_HANDLE.</P>
#	
#	<P class="label"><B>Diagnostics</B></P>
#	
#	<P>When <B>SQLSetDescField</B> returns SQL_ERROR or SQL_SUCCESS_WITH_INFO, an associated SQLSTATE value can be obtained by calling <B>SQLGetDiagRec</B> with a <I>HandleType</I> of SQL_HANDLE_DESC and a <I>Handle</I> of <I>DescriptorHandle</I>. The following table lists the SQLSTATE values commonly returned by <B>SQLSetDescField</B> and explains each one in the context of this function; the notation "(DM)" precedes the descriptions of SQLSTATEs returned by the Driver Manager. The return code associated with each SQLSTATE value is SQL_ERROR, unless noted otherwise.</P>
#	<!--TS:--><div class="tablediv"><table>
#	
#	<TR VALIGN="top">
#	<TH width=16%>SQLSTATE</TH>
#	<TH width=30%>Error</TH>
#	<TH width=54%>Description</TH>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=16%>01000</TD>
#	<TD width=30%>General warning</TD>
#	<TD width=54%>Driver-specific informational message. (Function returns SQL_SUCCESS_WITH_INFO.)</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=16%>01S02</TD>
#	<TD width=30%>Option value changed</TD>
#	<TD width=54%>The driver did not support the value specified in <I>*ValuePtr</I> (if <I>ValuePtr</I> was a pointer) or the value in <I>ValuePtr</I> (if <I>ValuePtr </I>was a 4-byte value), or <I>*ValuePtr</I> was invalid because of implementation working conditions, so the driver substituted a similar value. (Function returns SQL_SUCCESS_WITH_INFO.)</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=16%>07009</TD>
#	<TD width=30%>Invalid descriptor index</TD>
#	<TD width=54%>The <I>FieldIdentifier</I> argument was a record field, the <I>RecNumber</I> argument was 0, and the <I>DescriptorHandle</I> argument referred to an IPD handle.
#	<P>The <I>RecNumber</I> argument was less than 0, and the <I>DescriptorHandle</I> argument referred to an ARD or an APD.</P>
#	
#	<P>The <I>RecNumber</I> argument was greater than the maximum number of columns or parameters that the data source can support, and the <I>DescriptorHandle</I> argument referred to an APD or ARD.</P>
#	
#	<P>(DM) The <I>FieldIdentifier</I> argument was SQL_DESC_COUNT, and <I>*ValuePtr</I> argument was less than 0.</P>
#	
#	<P>The <I>RecNumber</I> argument was equal to 0, and the <I>DescriptorHandle</I> argument referred to an implicitly allocated APD. (This error does not occur with an explicitly allocated application descriptor, because it is not known whether an explicitly allocated application descriptor is an APD or ARD until execute time.)</P>
#	</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=16%>08S01</TD>
#	<TD width=30%>Communication link failure</TD>
#	<TD width=54%>The communication link between the driver and the data source to which the driver was connected failed before the function completed processing.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=16%>22001</TD>
#	<TD width=30%>String data, right <BR>
#	truncated</TD>
#	<TD width=54%>The <I>FieldIdentifier</I> argument was SQL_DESC_NAME, and the <I>BufferLength</I> argument was a value larger than SQL_MAX_IDENTIFIER_LEN.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=16%>HY000</TD>
#	<TD width=30%>General error</TD>
#	<TD width=54%>An error occurred for which there was no specific SQLSTATE and for which no implementation-specific SQLSTATE was defined. The error message returned by <B>SQLGetDiagRec</B> in the <I>*MessageText</I> buffer describes the error and its cause.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=16%>HY001</TD>
#	<TD width=30%>Memory allocation <BR>
#	error</TD>
#	<TD width=54%>The driver was unable to allocate memory required to support execution or completion of the function.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=16%>HY010</TD>
#	<TD width=30%>Function sequence error</TD>
#	<TD width=54%>(DM) The <I>DescriptorHandle</I> was associated with a <I>StatementHandle</I> for which an asynchronously executing function (not this one) was called and was still executing when this function was called.
#	<P>(DM) <B>SQLExecute</B>, <B>SQLExecDirect</B>, <B>SQLBulkOperations</B>, or <B>SQLSetPos</B> was called for the <I>StatementHandle</I> with which the <I>DescriptorHandle</I> was associated and returned SQL_NEED_DATA. This function was called before data was sent for all data-at-execution parameters or columns.</P>
#	</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=16%>HY013</TD>
#	<TD width=30%>Memory management error</TD>
#	<TD width=54%>The function call could not be processed because the underlying memory objects could not be accessed, possibly because of low memory conditions.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=16%>HY016</TD>
#	<TD width=30%>Cannot modify an implementation row descriptor</TD>
#	<TD width=54%>The <I>DescriptorHandle</I> argument was associated with an IRD, and the <I>FieldIdentifier</I> argument was not SQL_DESC_ARRAY_STATUS_PTR or SQL_DESC_ROWS_PROCESSED_PTR.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=16%>HY021</TD>
#	<TD width=30%>Inconsistent descriptor information</TD>
#	<TD width=54%>The SQL_DESC_TYPE and SQL_DESC_DATETIME_INTERVAL_CODE fields do not form a valid ODBC SQL type or a valid driver-specific SQL type (for IPDs) or a valid ODBC C type (for APDs or ARDs).
#	<P>Descriptor information checked during a consistency check was not consistent. (See "Consistency Check" in <B>SQLSetDescRec</B>.)</P>
#	</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=16%>HY090</TD>
#	<TD width=30%>Invalid string or buffer length</TD>
#	<TD width=54%>(DM) <I>*ValuePtr</I> is a character string, and <I>BufferLength</I> was less than zero but was not equal to SQL_NTS. 
#	<P>(DM) The driver was an ODBC 2<I>.x</I> driver, the descriptor was an ARD, the <I>ColumnNumber</I> argument was set to 0, and the value specified for the argument <I>BufferLength</I> was not equal to 4. </P>
#	</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=16%>HY091</TD>
#	<TD width=30%>Invalid descriptor field identifier</TD>
#	<TD width=54%>The value specified for the <I>FieldIdentifier</I> argument was not an ODBC-defined field and was not an implementation-defined value. 
#	<P>The <I>FieldIdentifier</I> argument was invalid for the <I>DescriptorHandle</I> argument.</P>
#	
#	<P>The <I>FieldIdentifier</I> argument was a read-only, ODBC-defined field.</P>
#	</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=16%>HY092</TD>
#	<TD width=30%>Invalid attribute/option identifier</TD>
#	<TD width=54%>The value in <I>*ValuePtr</I> was not valid for the <I>FieldIdentifier</I> argument.
#	<P>The <I>FieldIdentifier</I> argument was SQL_DESC_UNNAMED, and <I>ValuePtr</I> was SQL_NAMED.</P>
#	</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=16%>HY105</TD>
#	<TD width=30%>Invalid parameter type</TD>
#	<TD width=54%>(DM) The value specified for the SQL_DESC_PARAMETER_TYPE field was invalid. (For more information, see the "<I>InputOutputType</I> Argument" section in <B>SQLBindParameter</B>.)</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=16%>HYT01</TD>
#	<TD width=30%>Connection timeout expired</TD>
#	<TD width=54%>The connection timeout period expired before the data source responded to the request. The connection timeout period is set through <B>SQLSetConnectAttr</B>, SQL_ATTR_CONNECTION_TIMEOUT.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=16%>IM001</TD>
#	<TD width=30%>Driver does not support this function</TD>
#	<TD width=54%>(DM) The driver associated with the <I>DescriptorHandle</I> does not support the function.</TD>
#	</TR>
#	</table></div>
#	<!--TS:-->
#	<P class="label"><B>Comments</B></P>
#	
#	<P>An application can call <B>SQLSetDescField</B> to set any descriptor field one at a time. One call to <B>SQLSetDescField</B> sets a single field in a single descriptor. This function can be called to set any field in any descriptor type, provided the field can be set. (See the table later in this section.)</P>
#	
#	<P class="indent"><b class="le">Note</b>&nbsp;&nbsp;&nbsp;If a call to <B>SQLSetDescField</B> fails, the contents of the descriptor record identified by the <I>RecNumber</I> argument are undefined.</P>
#	
#	<P>Other functions can be called to set multiple descriptor fields with a single call of the function. The <B>SQLSetDescRec</B> function sets a variety of fields that affect the data type and buffer bound to a column or parameter (the SQL_DESC_TYPE, SQL_DESC_DATETIME_INTERVAL_CODE, SQL_DESC_OCTET_LENGTH, SQL_DESC_PRECISION, SQL_DESC_SCALE, SQL_DESC_DATA_PTR, SQL_DESC_OCTET_LENGTH_PTR, and SQL_DESC_INDICATOR_PTR fields). <B>SQLBindCol </B>or <B>SQLBindParameter</B> can be used to make a complete specification for the binding of a column or parameter. These functions set a specific group of descriptor fields with one function call.</P>
#	
#	<P><B>SQLSetDescField</B> can be called to change the binding buffers by adding an offset to the binding pointers (SQL_DESC_DATA_PTR, SQL_DESC_INDICATOR_PTR, or SQL_DESC_OCTET_LENGTH_PTR). This changes the binding buffers without calling <B>SQLBindCol </B>or <B>SQLBindParameter</B>, which allows an application to change SQL_DESC_DATA_PTR without changing other fields, such as SQL_DESC_DATA_TYPE.</P>
#	
#	<P>If an application calls <B>SQLSetDescField</B> to set any field other than SQL_DESC_COUNT or the deferred fields SQL_DESC_DATA_PTR, SQL_DESC_OCTET_LENGTH_PTR, or SQL_DESC_INDICATOR_PTR, the record becomes unbound.</P>
#	
#	<P>Descriptor header fields are set by calling <B>SQLSetDescField </B>with the appropriate <I>FieldIdentifier</I>. Many header fields are also statement attributes, so they can also be set by a call to <B>SQLSetStmtAttr</B>. This allows applications to set a descriptor field without first obtaining a descriptor handle. When <B>SQLSetDescField</B> is called to set a header field, the <I>RecNumber</I> argument is ignored.</P>
#	
#	<P>A <I>RecNumber</I> of 0 is used to set bookmark fields.</P>
#	
#	<P class="indent"><b class="le">Note</b>&nbsp;&nbsp;&nbsp;The statement attribute SQL_ATTR_USE_BOOKMARKS should always be set before calling <B>SQLSetDescField</B> to set bookmark fields. While this is not mandatory, it is strongly recommended.</P>
#	
#	<H1>Sequence of Setting Descriptor Fields</H1>
#	
#	<P>When setting descriptor fields by calling <B>SQLSetDescField</B>, the application must follow a specific sequence:
#	
#	<OL type=1>
#		<LI>The application must first set the SQL_DESC_TYPE, SQL_DESC_CONCISE_TYPE, or SQL_DESC_DATETIME_INTERVAL_CODE field. </li>
#	
#		<LI>After one of these fields has been set, the application can set an attribute of a data type, and the driver sets data type attribute fields to the appropriate default values for the data type. Automatic defaulting of type attribute fields ensures that the descriptor is always ready to use once the application has specified a data type. If the application explicitly sets a data type attribute, it is overriding the default attribute.</li>
#	
#		<LI>After one of the fields listed in step 1 has been set, and data type attributes have been set, the application can set SQL_DESC_DATA_PTR. This prompts a consistency check of descriptor fields. If the application changes the data type or attributes after setting the SQL_DESC_DATA_PTR field, the driver sets SQL_DESC_DATA_PTR to a null pointer, unbinding the record. This forces the application to complete the proper steps in sequence, before the descriptor record is usable.</li>
#	</OL>
#	
#	<H1>Initialization of Descriptor Fields</H1>
#	
#	<P>When a descriptor is allocated, the fields in the descriptor can be initialized to a default value, be initialized without a default value, or be undefined for the type of descriptor. The following tables indicate the initialization of each field for each type of descriptor, with "D" indicating that the field is initialized with a default, and "ND" indicating that the field is initialized without a default. If a number is shown, the default value of the field is that number. The tables also indicate whether a field is read/write (R/W) or read-only (R). </P>
#	
#	<P>The fields of an IRD have a default value only after the statement has been prepared or executed and the IRD has been populated, not when the statement handle or descriptor has been allocated. Until the IRD has been populated, any attempt to gain access to a field of an IRD will return an error.</P>
#	
#	<P>Some descriptor fields are defined for one or more, but not all, of the descriptor types (ARDs and IRDs, and APDs and IPDs). When a field is undefined for a type of descriptor, it is not needed by any of the functions that use that descriptor.</P>
#	
#	<P>The fields that can be accessed by <B>SQLGetDescField</B> cannot necessarily be set by <B>SQLSetDescField</B>. Fields that can be set by <B>SQLSetDescField</B> are listed in the following tables.</P>
#	
#	<P>The initialization of header fields is outlined in the table that follows.</P>
#	<!--TS:--><div class="tablediv"><table>
#	
#	<TR VALIGN="top">
#	<TH width=27%>Header field name</TH>
#	<TH width=21%>Type</TH>
#	<TH width=19%>R/W</TH>
#	<TH width=33%>Default</TH>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=27%>SQL_DESC_ALLOC_TYPE</TD>
#	<TD width=21%>SQLSMALLINT</TD>
#	<TD width=19%>ARD: R<BR>
#	APD: R<BR>
#	IRD: R<BR>
#	IPD: R </TD>
#	<TD width=33%>ARD: SQL_DESC_ALLOC_AUTO for implicit or SQL_DESC_ALLOC_USER for explicit
#	<P>APD: SQL_DESC_ALLOC_AUTO for implicit or SQL_DESC_ALLOC_USER for explicit</P>
#	
#	<P>IRD: SQL_DESC_ALLOC_AUTO</P>
#	
#	<P>IPD: SQL_DESC_ALLOC_AUTO</P>
#	</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=27%>SQL_DESC_ARRAY_SIZE</TD>
#	<TD width=21%>SQLUINTEGER</TD>
#	<TD width=19%>ARD: R/W<BR>
#	APD: R/W<BR>
#	IRD: Unused<BR>
#	IPD: Unused</TD>
#	<TD width=33%>ARD:<SUP>[1]</SUP><BR>
#	APD:<SUP>[1]</SUP><BR>
#	IRD: Unused<BR>
#	IPD: Unused</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=27%>SQL_DESC_ARRAY_STATUS_PTR</TD>
#	<TD width=21%>SQLUSMALLINT*</TD>
#	<TD width=19%>ARD: R/W<BR>
#	APD: R/W<BR>
#	IRD: R/W<BR>
#	IPD: R/W</TD>
#	<TD width=33%>ARD: Null ptr<BR>
#	APD: Null ptr<BR>
#	IRD: Null ptr<BR>
#	IPD: Null ptr</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=27%>SQL_DESC_BIND_OFFSET_PTR</TD>
#	<TD width=21%>SQLINTEGER*</TD>
#	<TD width=19%>ARD: R/W<BR>
#	APD: R/W<BR>
#	IRD: Unused<BR>
#	IPD: Unused</TD>
#	<TD width=33%>ARD: Null ptr<BR>
#	APD: Null ptr<BR>
#	IRD: Unused<BR>
#	IPD: Unused</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=27%>SQL_DESC_BIND_TYPE</TD>
#	<TD width=21%>SQLINTEGER</TD>
#	<TD width=19%>ARD: R/W<BR>
#	APD: R/W<BR>
#	IRD: Unused<BR>
#	IPD: Unused</TD>
#	<TD width=33%>ARD: SQL_BIND_BY_COLUMN
#	<P>APD: SQL_BIND_BY_COLUMN</P>
#	
#	<P>IRD: Unused</P>
#	
#	<P>IPD: Unused</P>
#	</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=27%>SQL_DESC_COUNT</TD>
#	<TD width=21%>SQLSMALLINT</TD>
#	<TD width=19%>ARD: R/W<BR>
#	APD: R/W<BR>
#	IRD: R<BR>
#	IPD: R/W</TD>
#	<TD width=33%>ARD: 0<BR>
#	APD: 0<BR>
#	IRD: D<BR>
#	IPD: 0</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=27%>SQL_DESC_ROWS_PROCESSED_PTR</TD>
#	<TD width=21%>SQLUINTEGER*</TD>
#	<TD width=19%>ARD: Unused<BR>
#	APD: Unused<BR>
#	IRD: R/W<BR>
#	IPD: R/W</TD>
#	<TD width=33%>ARD: Unused<BR>
#	APD: Unused<BR>
#	IRD: Null ptr<BR>
#	IPD: Null ptr</TD>
#	</TR>
#	</table></div>
#	
#	<P class="fineprint">[1]&nbsp;&nbsp;&nbsp;These fields are defined only when the IPD is automatically populated by the driver. If not, they are undefined. If an application attempts to set these fields, SQLSTATE HY091 (Invalid descriptor field identifier) will be returned.</p>
#	<P>The initialization of record fields is as shown in the following table.</P>
#	<!--TS:--><div class="tablediv"><table>
#	
#	<TR VALIGN="top">
#	<TH width=27%>Record field name</TH>
#	<TH width=21%>Type</TH>
#	<TH width=19%>R/W</TH>
#	<TH width=33%>Default</TH>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=27%>SQL_DESC_AUTO_UNIQUE_VALUE</TD>
#	<TD width=21%>SQLINTEGER</TD>
#	<TD width=19%>ARD: Unused<BR>
#	APD: Unused<BR>
#	IRD: R<BR>
#	IPD: Unused</TD>
#	<TD width=33%>ARD: Unused<BR>
#	APD: Unused<BR>
#	IRD: D<BR>
#	IPD: Unused</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=27%>SQL_DESC_BASE_COLUMN_NAME</TD>
#	<TD width=21%>SQLCHAR *</TD>
#	<TD width=19%>ARD: Unused<BR>
#	APD: Unused<BR>
#	IRD: R<BR>
#	IPD: Unused</TD>
#	<TD width=33%>ARD: Unused<BR>
#	APD: Unused<BR>
#	IRD: D<BR>
#	IPD: Unused</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=27%>SQL_DESC_BASE_TABLE_NAME</TD>
#	<TD width=21%>SQLCHAR *</TD>
#	<TD width=19%>ARD: Unused<BR>
#	APD: Unused<BR>
#	IRD: R<BR>
#	IPD: Unused</TD>
#	<TD width=33%>ARD: Unused<BR>
#	APD: Unused<BR>
#	IRD: D<BR>
#	IPD: Unused</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=27%>SQL_DESC_CASE_SENSITIVE</TD>
#	<TD width=21%>SQLINTEGER</TD>
#	<TD width=19%>ARD: Unused<BR>
#	APD: Unused<BR>
#	IRD: R<BR>
#	IPD: R</TD>
#	<TD width=33%>ARD: Unused<BR>
#	APD: Unused<BR>
#	IRD: D<BR>
#	IPD: D<SUP>[1]</SUP></TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=27%>SQL_DESC_CATALOG_NAME</TD>
#	<TD width=21%>SQLCHAR *</TD>
#	<TD width=19%>ARD: Unused<BR>
#	APD: Unused<BR>
#	IRD: R<BR>
#	IPD: Unused</TD>
#	<TD width=33%>ARD: Unused<BR>
#	APD: Unused<BR>
#	IRD: D<BR>
#	IPD: Unused</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=27%>SQL_DESC_CONCISE_TYPE</TD>
#	<TD width=21%>SQLSMALLINT</TD>
#	<TD width=19%>ARD: R/W<BR>
#	APD: R/W<BR>
#	IRD: R<BR>
#	IPD: R/W</TD>
#	<TD width=33%>ARD: SQL_C_<BR>
#	DEFAULT<BR>
#	APD: SQL_C_<BR>
#	DEFAULT<BR>
#	IRD: D<BR>
#	IPD: ND</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=27%>SQL_DESC_DATA_PTR</TD>
#	<TD width=21%>SQLPOINTER</TD>
#	<TD width=19%>ARD: R/W<BR>
#	APD: R/W<BR>
#	IRD: Unused<BR>
#	IPD: Unused</TD>
#	<TD width=33%>ARD: Null ptr<BR>
#	APD: Null ptr<BR>
#	IRD: Unused<BR>
#	IPD: Unused<SUP>[2]</SUP></TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=27%>SQL_DESC_DATETIME_INTERVAL_CODE</TD>
#	<TD width=21%>SQLSMALLINT</TD>
#	<TD width=19%>ARD: R/W<BR>
#	APD: R/W<BR>
#	IRD: R<BR>
#	IPD: R/W</TD>
#	<TD width=33%>ARD: ND<BR>
#	APD: ND<BR>
#	IRD: D<BR>
#	IPD: ND</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=27%>SQL_DESC_DATETIME_INTERVAL_PRECISION</TD>
#	<TD width=21%>SQLINTEGER</TD>
#	<TD width=19%>ARD: R/W<BR>
#	APD: R/W<BR>
#	IRD: R<BR>
#	IPD: R/W</TD>
#	<TD width=33%>ARD: ND<BR>
#	APD: ND<BR>
#	IRD: D<BR>
#	IPD: ND</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=27%>SQL_DESC_DISPLAY_SIZE</TD>
#	<TD width=21%>SQLINTEGER</TD>
#	<TD width=19%>ARD: Unused<BR>
#	APD: Unused<BR>
#	IRD: R<BR>
#	IPD: Unused</TD>
#	<TD width=33%>ARD: Unused<BR>
#	APD: Unused<BR>
#	IRD: D<BR>
#	IPD: Unused</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=27%>SQL_DESC_FIXED_PREC_SCALE</TD>
#	<TD width=21%>SQLSMALLINT</TD>
#	<TD width=19%>ARD: Unused<BR>
#	APD: Unused<BR>
#	IRD: R<BR>
#	IPD: R</TD>
#	<TD width=33%>ARD: Unused<BR>
#	APD: Unused<BR>
#	IRD: D<BR>
#	IPD: D<SUP>[1]</SUP></TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=27%>SQL_DESC_INDICATOR_PTR</TD>
#	<TD width=21%>SQLINTEGER *</TD>
#	<TD width=19%>ARD: R/W<BR>
#	APD: R/W<BR>
#	IRD: Unused<BR>
#	IPD: Unused</TD>
#	<TD width=33%>ARD: Null ptr<BR>
#	APD: Null ptr<BR>
#	IRD: Unused<BR>
#	IPD: Unused</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=27%>SQL_DESC_LABEL</TD>
#	<TD width=21%>SQLCHAR *</TD>
#	<TD width=19%>ARD: Unused<BR>
#	APD: Unused<BR>
#	IRD: R<BR>
#	IPD: Unused</TD>
#	<TD width=33%>ARD: Unused<BR>
#	APD: Unused<BR>
#	IRD: D<BR>
#	IPD: Unused</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=27%>SQL_DESC_LENGTH</TD>
#	<TD width=21%>SQLUINTEGER</TD>
#	<TD width=19%>ARD: R/W<BR>
#	APD: R/W<BR>
#	IRD: R<BR>
#	IPD: R/W</TD>
#	<TD width=33%>ARD: ND<BR>
#	APD: ND<BR>
#	IRD: D<BR>
#	IPD: ND</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=27%>SQL_DESC_LITERAL_PREFIX</TD>
#	<TD width=21%>SQLCHAR *</TD>
#	<TD width=19%>ARD: Unused<BR>
#	APD: Unused<BR>
#	IRD: R<BR>
#	IPD: Unused</TD>
#	<TD width=33%>ARD: Unused<BR>
#	APD: Unused<BR>
#	IRD: D<BR>
#	IPD: Unused</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=27%>SQL_DESC_LITERAL_SUFFIX</TD>
#	<TD width=21%>SQLCHAR *</TD>
#	<TD width=19%>ARD: Unused<BR>
#	APD: Unused<BR>
#	IRD: R<BR>
#	IPD: Unused</TD>
#	<TD width=33%>ARD: Unused<BR>
#	APD: Unused<BR>
#	IRD: D<BR>
#	IPD: Unused</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=27%>SQL_DESC_LOCAL_TYPE_NAME</TD>
#	<TD width=21%>SQLCHAR *</TD>
#	<TD width=19%>ARD: Unused<BR>
#	APD: Unused<BR>
#	IRD: R<BR>
#	IPD: R</TD>
#	<TD width=33%>ARD: Unused<BR>
#	APD: Unused<BR>
#	IRD: D<BR>
#	IPD: D<SUP>[1]</SUP></TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=27%>SQL_DESC_NAME</TD>
#	<TD width=21%>SQLCHAR *</TD>
#	<TD width=19%>ARD: Unused<BR>
#	APD: Unused<BR>
#	IRD: R<BR>
#	IPD: R/W</TD>
#	<TD width=33%>ARD: ND<BR>
#	APD: ND<BR>
#	IRD: D<BR>
#	IPD: ND</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=27%>SQL_DESC_NULLABLE</TD>
#	<TD width=21%>SQLSMALLINT</TD>
#	<TD width=19%>ARD: Unused<BR>
#	APD: Unused<BR>
#	IRD: R<BR>
#	IPD: R</TD>
#	<TD width=33%>ARD: ND<BR>
#	APD: ND<BR>
#	IRD: D<BR>
#	IPD: ND</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=27%>SQL_DESC_NUM_PREC_RADIX</TD>
#	<TD width=21%>SQLINTEGER</TD>
#	<TD width=19%>ARD: R/W<BR>
#	APD: R/W<BR>
#	IRD: R<BR>
#	IPD: R/W</TD>
#	<TD width=33%>ARD: ND<BR>
#	APD: ND<BR>
#	IRD: D<BR>
#	IPD: ND</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=27%>SQL_DESC_OCTET_LENGTH</TD>
#	<TD width=21%>SQLINTEGER</TD>
#	<TD width=19%>ARD: R/W<BR>
#	APD: R/W<BR>
#	IRD: R<BR>
#	IPD: R/W</TD>
#	<TD width=33%>ARD: ND<BR>
#	APD: ND<BR>
#	IRD: D<BR>
#	IPD: ND</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=27%>SQL_DESC_OCTET_LENGTH_PTR</TD>
#	<TD width=21%>SQLINTEGER *</TD>
#	<TD width=19%>ARD: R/W<BR>
#	APD: R/W<BR>
#	IRD: Unused<BR>
#	IPD: Unused</TD>
#	<TD width=33%>ARD: Null ptr<BR>
#	APD: Null ptr<BR>
#	IRD: Unused<BR>
#	IPD: Unused</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=27%>SQL_DESC_PARAMETER_TYPE</TD>
#	<TD width=21%>SQLSMALLINT</TD>
#	<TD width=19%>ARD: Unused<BR>
#	APD: Unused<BR>
#	IRD: Unused<BR>
#	IPD: R/W</TD>
#	<TD width=33%>ARD: Unused<BR>
#	APD: Unused<BR>
#	IRD: Unused<BR>
#	IPD: D=SQL_PARAM_INPUT</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=27%>SQL_DESC_PRECISION</TD>
#	<TD width=21%>SQLSMALLINT</TD>
#	<TD width=19%>ARD: R/W<BR>
#	APD: R/W<BR>
#	IRD: R<BR>
#	IPD: R/W</TD>
#	<TD width=33%>ARD: ND<BR>
#	APD: ND<BR>
#	IRD: D<BR>
#	IPD: ND</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=27%>SQL_DESC_ROWVER</TD>
#	<TD width=21%>SQLSMALLINT</TD>
#	<TD width=19%>ARD: Unused
#	<P>APD: Unused</P>
#	
#	<P>IRD: R</P>
#	
#	<P>IPD: R</P>
#	</TD>
#	<TD width=33%>ARD: Unused
#	<P>APD: Unused</P>
#	
#	<P>IRD: ND</P>
#	
#	<P>IPD: ND</P>
#	</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=27%>SQL_DESC_SCALE</TD>
#	<TD width=21%>SQLSMALLINT</TD>
#	<TD width=19%>ARD: R/W<BR>
#	APD: R/W<BR>
#	IRD: R<BR>
#	IPD: R/W</TD>
#	<TD width=33%>ARD: ND<BR>
#	APD: ND<BR>
#	IRD: D<BR>
#	IPD: ND</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=27%>SQL_DESC_SCHEMA_NAME</TD>
#	<TD width=21%>SQLCHAR *</TD>
#	<TD width=19%>ARD: Unused<BR>
#	APD: Unused<BR>
#	IRD: R<BR>
#	IPD: Unused</TD>
#	<TD width=33%>ARD: Unused<BR>
#	APD: Unused<BR>
#	IRD: D<BR>
#	IPD: Unused</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=27%>SQL_DESC_SEARCHABLE</TD>
#	<TD width=21%>SQLSMALLINT</TD>
#	<TD width=19%>ARD: Unused<BR>
#	APD: Unused<BR>
#	IRD: R<BR>
#	IPD: Unused</TD>
#	<TD width=33%>ARD: Unused<BR>
#	APD: Unused<BR>
#	IRD: D<BR>
#	IPD: Unused</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=27%>SQL_DESC_TABLE_NAME</TD>
#	<TD width=21%>SQLCHAR *</TD>
#	<TD width=19%>ARD: Unused<BR>
#	APD: Unused<BR>
#	IRD: R<BR>
#	IPD: Unused</TD>
#	<TD width=33%>ARD: Unused<BR>
#	APD: Unused<BR>
#	IRD: D<BR>
#	IPD: Unused</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=27%>SQL_DESC_TYPE</TD>
#	<TD width=21%>SQLSMALLINT</TD>
#	<TD width=19%>ARD: R/W<BR>
#	APD: R/W<BR>
#	IRD: R<BR>
#	IPD: R/W</TD>
#	<TD width=33%>ARD: SQL_C_DEFAULT<BR>
#	APD: SQL_C_DEFAULT<BR>
#	IRD: D<BR>
#	IPD: ND</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=27%>SQL_DESC_TYPE_NAME</TD>
#	<TD width=21%>SQLCHAR *</TD>
#	<TD width=19%>ARD: Unused<BR>
#	APD: Unused<BR>
#	IRD: R<BR>
#	IPD: R</TD>
#	<TD width=33%>ARD: Unused<BR>
#	APD: Unused<BR>
#	IRD: D<BR>
#	IPD: D<SUP>[1]</SUP></TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=27%>SQL_DESC_UNNAMED</TD>
#	<TD width=21%>SQLSMALLINT</TD>
#	<TD width=19%>ARD: Unused<BR>
#	APD: Unused<BR>
#	IRD: R<BR>
#	IPD: R/W</TD>
#	<TD width=33%>ARD: ND<BR>
#	APD: ND<BR>
#	IRD: D<BR>
#	IPD: ND</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=27%>SQL_DESC_UNSIGNED</TD>
#	<TD width=21%>SQLSMALLINT</TD>
#	<TD width=19%>ARD: Unused<BR>
#	APD: Unused<BR>
#	IRD: R<BR>
#	IPD: R</TD>
#	<TD width=33%>ARD: Unused<BR>
#	APD: Unused<BR>
#	IRD: D<BR>
#	IPD: D<SUP>[1]</SUP></TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=27%>SQL_DESC_UPDATABLE</TD>
#	<TD width=21%>SQLSMALLINT</TD>
#	<TD width=19%>ARD: Unused<BR>
#	APD: Unused<BR>
#	IRD: R<BR>
#	IPD: Unused</TD>
#	<TD width=33%>ARD: Unused<BR>
#	APD: Unused<BR>
#	IRD: D<BR>
#	IPD: Unused</TD>
#	</TR>
#	</table></div>
#	
#	<P class="fineprint">[1]&nbsp;&nbsp;&nbsp;These fields are defined only when the IPD is automatically populated by the driver. If not, they are undefined. If an application attempts to set these fields, SQLSTATE HY091 (Invalid descriptor field identifier) will be returned.</p>
#	<P class="fineprint">[2]&nbsp;&nbsp;&nbsp;The SQL_DESC_DATA_PTR field in the IPD can be set to force a consistency check. In a subsequent call to <B>SQLGetDescField</B> or <B>SQLGetDescRec</B>, the driver is not required to return the value that SQL_DESC_DATA_PTR was set to.</p>
#	<P class="label"><B><I>FieldIdentifier</I> Argument</B></P>
#	
#	<P>The <I>FieldIdentifier</I> argument indicates the descriptor field to be set. A descriptor contains the <I>descriptor header,</I> consisting of the header fields described in the next section, "Header Fields," and zero or more <I>descriptor records,</I> consisting of the record fields described in the section following the "Header Fields" section.</P>
#	
#	<H1>Header Fields</H1>
#	
#	<P>Each descriptor has a header consisting of the following fields:
#	
#	<DL>
#	<DT><B>SQL_DESC_ALLOC_TYPE [All]</B></DT>
#	
#	<DD>This read-only SQLSMALLINT header field specifies whether the descriptor was allocated automatically by the driver or explicitly by the application. The application can obtain, but not modify, this field. The field is set to SQL_DESC_ALLOC_AUTO by the driver if the descriptor was automatically allocated by the driver. It is set to SQL_DESC_ALLOC_USER by the driver if the descriptor was explicitly allocated by the application.</dd>
#	
#	<DT><B>SQL_DESC_ARRAY_SIZE [Application descriptors]</B></DT>
#	
#	<DD>In ARDs, this SQLUINTEGER header field specifies the number of rows in the rowset. This is the number of rows to be returned by a call to <B>SQLFetch</B> or <B>SQLFetchScroll</B> or to be operated on by a call to <B>SQLBulkOperations</B> or <B>SQLSetPos</B>.
#	
#	<P>In APDs, this SQLUINTEGER header field specifies the number of values for each parameter. 
#	
#	
#	<P>The default value of this field is 1. If SQL_DESC_ARRAY_SIZE is greater than 1, SQL_DESC_DATA_PTR, SQL_DESC_INDICATOR_PTR, and SQL_DESC_OCTET_LENGTH_PTR of the APD or ARD point to arrays. The cardinality of each array is equal to the value of this field.
#	
#	
#	<P>This field in the ARD can also be set by calling <B>SQLSetStmtAttr</B> with the SQL_ATTR_ROW_ARRAY_SIZE attribute. This field in the APD can also be set by calling <B>SQLSetStmtAttr</B> with the SQL_ATTR_PARAMSET_SIZE attribute.
#	</dd>
#	
#	<DT><B>SQL_DESC_ARRAY_STATUS_PTR [All]</B></DT>
#	
#	<DD>For each descriptor type, this SQLUSMALLINT * header field points to an array of SQLUSMALLINT values. These arrays are named as follows: row status array (IRD), parameter status array (IPD), row operation array (ARD), and parameter operation array (APD).
#	
#	<P>In the IRD, this header field points to a row status array containing status values after a call to <B>SQLBulkOperations</B>, <B>SQLFetch</B>, <B>SQLFetchScroll</B>, or <B>SQLSetPos</B>. The array has as many elements as there are rows in the rowset. The application must allocate an array of SQLUSMALLINTs and set this field to point to the array. The field is set to a null pointer by default. The driver will populate the array&#0151;unless the SQL_DESC_ARRAY_STATUS_PTR field is set to a null pointer, in which case no status values are generated and the array is not populated. 
#	
#	
#	<P class="indent"><b class="le">Caution</b>&nbsp;&nbsp;&nbsp;Driver behavior is undefined if the application sets the elements of the row status array pointed to by the SQL_DESC_ARRAY_STATUS_PTR field of the IRD.
#	
#	
#	<P>The array is initially populated by a call to <B>SQLBulkOperations</B>, <B>SQLFetch</B>, <B>SQLFetchScroll</B>, or <B>SQLSetPos</B>. If the call did not return SQL_SUCCESS or SQL_SUCCESS_WITH_INFO, the contents of the array pointed to by this field are undefined. The elements in the array can contain the following values:
#	
#	
#	<UL type=disc>
#		<LI>SQL_ROW_SUCCESS: The row was successfully fetched and has not changed since it was last fetched.</li>
#	
#		<LI>SQL_ROW_SUCCESS_WITH_INFO: The row was successfully fetched and has not changed since it was last fetched. However, a warning was returned about the row.</li>
#	
#		<LI>SQL_ROW_ERROR: An error occurred while fetching the row.</li>
#	
#		<LI>SQL_ROW_UPDATED: The row was successfully fetched and has been updated since it was last fetched. If the row is fetched again, its status is SQL_ROW_SUCCESS.</li>
#	
#		<LI>SQL_ROW_DELETED: The row has been deleted since it was last fetched.</li>
#	
#		<LI>SQL_ROW_ADDED: The row was inserted by <B>SQLBulkOperations</B>. If the row is fetched again, its status is SQL_ROW_SUCCESS.</li>
#	
#		<LI>SQL_ROW_NOROW: The rowset overlapped the end of the result set, and no row was returned that corresponded to this element of the row status array.</li>
#	</UL>
#	
#	
#	<P>This field in the IRD can also be set by calling <B>SQLSetStmtAttr</B> with the SQL_ATTR_ROW_STATUS_PTR attribute.
#	
#	
#	<P>The SQL_DESC_ARRAY_STATUS_PTR field of the IRD is valid only after SQL_SUCCESS or SQL_SUCCESS_WITH_INFO has been returned. If the return code is not one of these, the location pointed to by SQL_DESC_ROWS_PROCESSED_PTR is undefined.
#	
#	
#	<P>In the IPD, this header field points to a parameter status array containing status information for each set of parameter values after a call to <B>SQLExecute</B> or <B>SQLExecDirect</B>. If the call to <B>SQLExecute</B> or <B>SQLExecDirect</B> did not return SQL_SUCCESS or SQL_SUCCESS_WITH_INFO, the contents of the array pointed to by this field are undefined. The application must allocate an array of SQLUSMALLINTs and set this field to point to the array. The driver will populate the array&#0151;unless the SQL_DESC_ARRAY_STATUS_PTR field is set to a null pointer, in which case no status values are generated and the array is not populated. The elements in the array can contain the following values:
#	
#	
#	<UL type=disc>
#		<LI>SQL_PARAM_SUCCESS: The SQL statement was successfully executed for this set of parameters.</li>
#	
#		<LI>SQL_PARAM_SUCCESS_WITH_INFO: The SQL statement was successfully executed for this set of parameters; however, warning information is available in the diagnostics data structure.</li>
#	
#		<LI>SQL_PARAM_ERROR: An error occurred in processing this set of parameters. Additional error information is available in the diagnostics data structure.</li>
#	
#		<LI>SQL_PARAM_UNUSED: This parameter set was unused, possibly due to the fact that some previous parameter set caused an error that aborted further processing, or because SQL_PARAM_IGNORE was set for that set of parameters in the array specified by the SQL_DESC_ARRAY_STATUS_PTR field of the APD.</li>
#	
#		<LI>SQL_PARAM_DIAG_UNAVAILABLE: Diagnostic information is not available. An example of this is when the driver treats arrays of parameters as a monolithic unit and so does not generate this level of error information.</li>
#	</UL>
#	
#	
#	<P>This field in the IPD can also be set by calling <B>SQLSetStmtAttr</B> with the SQL_ATTR_PARAM_STATUS_PTR attribute.
#	
#	
#	<P>In the ARD, this header field points to a row operation array of values that can be set by the application to indicate whether this row is to be ignored for <B>SQLSetPos</B> operations. The elements in the array can contain the following values:
#	
#	
#	<UL type=disc>
#		<LI>SQL_ROW_PROCEED: The row is included in the bulk operation using <B>SQLSetPos</B>. (This setting does not guarantee that the operation will occur on the row. If the row has the status SQL_ROW_ERROR in the IRD row status array, the driver might not be able to perform the operation in the row.)</li>
#	
#		<LI>SQL_ROW_IGNORE: The row is excluded from the bulk operation using <B>SQLSetPos</B>.</li>
#	</UL>
#	
#	
#	<P>If no elements of the array are set, all rows are included in the bulk operation. If the value in the SQL_DESC_ARRAY_STATUS_PTR field of the ARD is a null pointer, all rows are included in the bulk operation; the interpretation is the same as if the pointer pointed to a valid array and all elements of the array were SQL_ROW_PROCEED. If an element in the array is set to SQL_ROW_IGNORE, the value in the row status array for the ignored row is not changed.
#	
#	
#	<P>This field in the ARD can also be set by calling <B>SQLSetStmtAttr</B> with the SQL_ATTR_ROW_OPERATION_PTR attribute.
#	
#	
#	<P>In the APD, this header field points to a parameter operation array of values that can be set by the application to indicate whether this set of parameters is to be ignored when <B>SQLExecute</B> or<B> SQLExecDirect</B> is called. The elements in the array can contain the following values:
#	
#	
#	<UL type=disc>
#		<LI>SQL_PARAM_PROCEED: The set of parameters is included in the <B>SQLExecute</B> or <B>SQLExecDirect</B> call.</li>
#	
#		<LI>SQL_PARAM_IGNORE: The set of parameters is excluded from the <B>SQLExecute</B> or <B>SQLExecDirect</B> call.</li>
#	</UL>
#	
#	
#	<P>If no elements of the array are set, all sets of parameters in the array are used in the <B>SQLExecute</B> or <B>SQLExecDirect</B> calls. If the value in the SQL_DESC_ARRAY_STATUS_PTR field of the APD is a null pointer, all sets of parameters are used; the interpretation is the same as if the pointer pointed to a valid array and all elements of the array were SQL_PARAM_PROCEED.
#	
#	
#	<P>This field in the APD can also be set by calling <B>SQLSetStmtAttr</B> with the SQL_ATTR_PARAM_OPERATION_PTR attribute.
#	</dd>
#	
#	<DT><B>SQL_DESC_BIND_OFFSET_PTR [Application descriptors]</B></DT>
#	
#	<DD>This SQLINTEGER * header field points to the binding offset. It is set to a null pointer by default. If this field is not a null pointer, the driver dereferences the pointer and adds the dereferenced value to each of the deferred fields that has a non-null value in the descriptor record (SQL_DESC_DATA_PTR, SQL_DESC_INDICATOR_PTR, and SQL_DESC_OCTET_LENGTH_PTR) at fetch time and uses the new pointer values when binding.
#	
#	<P>The binding offset is always added directly to the values in the SQL_DESC_DATA_PTR, SQL_DESC_INDICATOR_PTR, and SQL_DESC_OCTET_LENGTH_PTR fields. If the offset is changed to a different value, the new value is still added directly to the value in each descriptor field. The new offset is not added to the field value plus any earlier offset.
#	
#	
#	<P>This field is a <I>deferred field</I>: It is not used at the time it is set but is used at a later time by the driver when it needs to determine addresses for data buffers.
#	
#	
#	<P>This field in the ARD can also be set by calling <B>SQLSetStmtAttr</B> with the SQL_ATTR_ROW_BIND_OFFSET_PTR attribute. This field in the ARD can also be set by calling <B>SQLSetStmtAttr</B> with the SQL_ATTR_PARAM_BIND_OFFSET_PTR attribute.
#	
#	
#	<P>For more information, see the description of row-wise binding in <A HREF="odbcsqlfetchscroll.htm">SQLFetchScroll</A> and <A HREF="odbcsqlbindparameter.htm">SQLBindParameter</A>.
#	</dd>
#	
#	<DT><B>SQL_DESC_BIND_TYPE [Application descriptors]</B></DT>
#	
#	<DD>This SQLUINTEGER header field sets the binding orientation to be used for binding either columns or parameters.
#	
#	<P>In ARDs, this field specifies the binding orientation when <B>SQLFetchScroll</B> or <B>SQLFetch</B> is called on the associated statement handle.
#	
#	
#	<P>To select column-wise binding for columns, this field is set to SQL_BIND_BY_COLUMN (the default).
#	
#	
#	<P>This field in the ARD can also be set by calling <B>SQLSetStmtAttr</B> with the SQL_ATTR_ROW_BIND_TYPE <I>Attribute</I>.
#	
#	
#	<P>In APDs, this field specifies the binding orientation to be used for dynamic parameters.
#	
#	
#	<P>To select column-wise binding for parameters, this field is set to SQL_BIND_BY_COLUMN (the default).
#	
#	
#	<P>This field in the APD can also be set by calling <B>SQLSetStmtAttr</B> with the SQL_ATTR_PARAM_BIND_TYPE <I>Attribute</I>.
#	</dd>
#	
#	<DT><B>SQL_DESC_COUNT [All]</B></DT>
#	
#	<DD>This SQLSMALLINT header field specifies the 1-based index of the highest-numbered record that contains data. When the driver sets the data structure for the descriptor, it must also set the SQL_DESC_COUNT field to show how many records are significant. When an application allocates an instance of this data structure, it does not have to specify how many records to reserve room for. As the application specifies the contents of the records, the driver takes any required action to ensure that the descriptor handle refers to a data structure of the adequate size.
#	
#	<P>SQL_DESC_COUNT is not a count of all data columns that are bound (if the field is in an ARD) or of all parameters that are bound (if the field is in an APD), but the number of the highest-numbered record. If the highest-numbered column or parameter is unbound, then SQL_DESC_COUNT is changed to the number of the next highest-numbered column or parameter. If a column or a parameter with a number that is less than the number of the highest-numbered column is unbound (by calling <B>SQLBindCol</B> with the <I>TargetValuePtr</I> argument set to a null pointer, or <B>SQLBindParameter</B> with the <I>ParameterValuePtr</I> argument set to a null pointer), SQL_DESC_COUNT is not changed. If additional columns or parameters are bound with numbers greater than the highest-numbered record that contains data, the driver automatically increases the value in the SQL_DESC_COUNT field. If all columns are unbound by calling <B>SQLFreeStmt</B> with the SQL_UNBIND option, the SQL_DESC_COUNT fields in the ARD and IRD are set to 0. If <B>SQLFreeStmt</B> is called with the SQL_RESET_PARAMS option, the SQL_DESC_COUNT fields in the APD and IPD are set to 0.
#	
#	
#	<P>The value in SQL_DESC_COUNT can be set explicitly by an application by calling <B>SQLSetDescField</B>. If the value in SQL_DESC_COUNT is explicitly decreased, all records with numbers greater than the new value in SQL_DESC_COUNT are effectively removed. If the value in SQL_DESC_COUNT is explicitly set to 0 and the field is in an ARD, all data buffers except a bound bookmark column are released.
#	
#	
#	<P>The record count in this field of an ARD does not include a bound bookmark column. The only way to unbind a bookmark column is to set the SQL_DESC_DATA_PTR field to a null pointer.
#	</dd>
#	
#	<DT><B>SQL_DESC_ROWS_PROCESSED_PTR [Implementation descriptors]</B></DT>
#	
#	<DD>In an IRD, this SQLUINTEGER * header field points to a buffer containing the number of rows fetched after a call to <B>SQLFetch</B> or <B>SQLFetchScroll</B>, or the number of rows affected in a bulk operation performed by a call to <B>SQLBulkOperations</B> or <B>SQLSetPos</B>, including error rows.
#	
#	<P>In an IPD, this SQLUINTEGER * header field points to a buffer containing the number of sets of parameters that have been processed, including error sets. No number will be returned if this is a null pointer.
#	
#	
#	<P>SQL_DESC_ROWS_PROCESSED_PTR is valid only after SQL_SUCCESS or SQL_SUCCESS_WITH_INFO has been returned after a call to <B>SQLFetch</B> or <B>SQLFetchScroll </B>(for an IRD field) or <B>SQLExecute</B>, <B>SQLExecDirect</B>, or <B>SQLParamData</B> (for an IPD field). If the call that fills in the buffer pointed to by this field does not return SQL_SUCCESS or SQL_SUCCESS_WITH_INFO, the contents of the buffer are undefined, unless it returns SQL_NO_DATA, in which case the value in the buffer is set to 0.
#	
#	
#	<P>This field in the ARD can also be set by calling <B>SQLSetStmtAttr</B> with the SQL_ATTR_ROWS_FETCHED_PTR attribute. This field in the APD can also be set by calling <B>SQLSetStmtAttr</B> with the SQL_ATTR_PARAMS_PROCESSED_PTR attribute.
#	
#	
#	<P>The buffer pointed to by this field is allocated by the application. It is a deferred output buffer that is set by the driver. It is set to a null pointer by default.
#	</dd>
#	</DL>
#	
#	<H1>Record Fields</H1>
#	
#	<P>Each descriptor contains one or more records consisting of fields that define either column data or dynamic parameters, depending on the type of descriptor. Each record is a complete definition of a single column or parameter.
#	
#	<DL>
#	<DT><B>SQL_DESC_AUTO_UNIQUE_VALUE [IRDs]</B></DT>
#	
#	<DD>This read-only SQLINTEGER record field contains SQL_TRUE if the column is an auto-incrementing column, or SQL_FALSE if the column is not an auto-incrementing column. This field is read-only, but the underlying auto-incrementing column is not necessarily read-only.</dd>
#	
#	<DT><B>SQL_DESC_BASE_COLUMN_NAME [IRDs]</B></DT>
#	
#	<DD>This read-only SQLCHAR * record field contains the base column name for the result set column. If a base column name does not exist (as in the case of columns that are expressions), this variable contains an empty string.</dd>
#	
#	<DT><B>SQL_DESC_BASE_TABLE_NAME [IRDs]</B></DT>
#	
#	<DD>This read-only SQLCHAR * record field contains the base table name for the result set column. If a base table name cannot be defined or is not applicable, this variable contains an empty string.</dd>
#	
#	<DT><B>SQL_DESC_CASE_SENSITIVE [Implementation descriptors]</B></DT>
#	
#	<DD>This read-only SQLINTEGER record field contains SQL_TRUE if the column or parameter is treated as case-sensitive for collations and comparisons, or SQL_FALSE if the column is not treated as case-sensitive for collations and comparisons or if it is a noncharacter column.</dd>
#	
#	<DT><B>SQL_DESC_CATALOG_NAME [IRDs]</B></DT>
#	
#	<DD>This read-only SQLCHAR * record field contains the catalog for the base table that contains the column. The return value is driver-dependent if the column is an expression or if the column is part of a view. If the data source does not support catalogs or the catalog cannot be determined, this variable contains an empty string.</dd>
#	
#	<DT><B>SQL_DESC_CONCISE_TYPE [All]</B></DT>
#	
#	<DD>This SQLSMALLINT header field specifies the concise data type for all data types, including the datetime and interval data types.
#	
#	<P>The values in the SQL_DESC_CONCISE_TYPE, SQL_DESC_TYPE, and SQL_DESC_DATETIME_INTERVAL_CODE fields are interdependent. Each time one of the fields is set, the other must also be set. SQL_DESC_CONCISE_TYPE can be set by a call to <B>SQLBindCol</B> or <B>SQLBindParameter</B>, or <B>SQLSetDescField</B>. SQL_DESC_TYPE can be set by a call to <B>SQLSetDescField</B> or <B>SQLSetDescRec</B>. 
#	
#	
#	<P>If SQL_DESC_CONCISE_TYPE is set to a concise data type other than an interval or datetime data type, the SQL_DESC_TYPE field is set to the same value and the SQL_DESC_DATETIME_INTERVAL_CODE field is set to 0.
#	
#	
#	<P>If SQL_DESC_CONCISE_TYPE is set to the concise datetime or interval data type, the SQL_DESC_TYPE field is set to the corresponding verbose type (SQL_DATETIME or SQL_INTERVAL) and the SQL_DESC_DATETIME_INTERVAL_CODE field is set to the appropriate subcode.
#	</dd>
#	
#	<DT><B>SQL_DESC_DATA_PTR [Application descriptors and IPDs]</B></DT>
#	
#	<DD>This SQLPOINTER record field points to a variable that will contain the parameter value (for APDs) or the column value (for ARDs). This field is a <I>deferred field</I>. It is not used at the time it is set but is used at a later time by the driver to retrieve data.
#	
#	<P>The column specified by the SQL_DESC_DATA_PTR field of the ARD is unbound if the <I>TargetValuePtr</I> argument in a call to <B>SQLBindCol</B> is a null pointer or if the SQL_DESC_DATA_PTR field in the ARD is set by a call to <B>SQLSetDescField</B> or <B>SQLSetDescRec</B> to a null pointer. Other fields are not affected if the SQL_DESC_DATA_PTR field is set to a null pointer. 
#	
#	
#	<P>If the call to <B>SQLFetch</B> or <B>SQLFetchScroll </B>that fills in the buffer pointed to by this field did not return SQL_SUCCESS or SQL_SUCCESS_WITH_INFO, the contents of the buffer are undefined.
#	
#	
#	<P>Whenever the SQL_DESC_DATA_PTR field of an APD, ARD, or IPD is set, the driver checks that the value in the SQL_DESC_TYPE field contains one of the valid ODBC C data types or a driver-specific data type, and that all other fields affecting the data types are consistent. Prompting a consistency check is the only use of the SQL_DESC_DATA_PTR field of an IPD. Specifically, if an application sets the SQL_DESC_DATA_PTR field of an IPD and later calls <B>SQLGetDescField</B> on this field, it is not necessarily returned the value that it had set. For more information, see "Consistency Checks" in <A HREF="odbcsqlsetdescrec.htm">SQLSetDescRec</A>.
#	</dd>
#	
#	<DT><B>SQL_DESC_DATETIME_INTERVAL_CODE [All]</B></DT>
#	
#	<DD>This SQLSMALLINT record field contains the subcode for the specific datetime or interval data type when the SQL_DESC_TYPE field is SQL_DATETIME or SQL_INTERVAL. This is true for both SQL and C data types. The code consists of the data type name with "CODE" substituted for either "TYPE" or "C_TYPE" (for datetime types), or "CODE" substituted for "INTERVAL" or "C_INTERVAL" (for interval types).
#	
#	<P>If SQL_DESC_TYPE and SQL_DESC_CONCISE_TYPE in an application descriptor are set to SQL_C_DEFAULT and the descriptor is not associated with a statement handle, the contents of SQL_DESC_DATETIME_INTERVAL_CODE are undefined.
#	
#	
#	<P>This field can be set for the datetime data types listed in the following table.
#	
#	<!--TS:-->
#	<div class="tablediv"><table>
#	
#	<TR VALIGN="top">
#	<TH width=50%>Datetime types</TH>
#	<TH width=50%>DATETIME_INTERVAL_CODE</TH>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_TYPE_DATE/SQL_C_TYPE_DATE</TD>
#	<TD width=50%>SQL_CODE_DATE</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_TYPE_TIME/SQL_C_TYPE_TIME</TD>
#	<TD width=50%>SQL_CODE_TIME</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_TYPE_TIMESTAMP/<BR>
#	SQL_C_TYPE_TIMESTAMP</TD>
#	<TD width=50%>SQL_CODE_TIMESTAMP</TD>
#	</TR>
#	</table></div>
#	
#	<!--TS:-->
#	
#	<P>This field can be set for the interval data types listed in the following table.
#	
#	<!--TS:-->
#	<div class="tablediv"><table>
#	
#	<TR VALIGN="top">
#	<TH width=50%>Interval type</TH>
#	<TH width=50%>DATETIME_INTERVAL_CODE</TH>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_INTERVAL_DAY/<BR>
#	SQL_C_INTERVAL_DAY</TD>
#	<TD width=50%>SQL_CODE_DAY</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_INTERVAL_DAY_TO_HOUR/<BR>
#	SQL_C_INTERVAL_DAY_TO_HOUR</TD>
#	<TD width=50%>SQL_CODE_DAY_TO_HOUR</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_INTERVAL_DAY_TO_MINUTE/<BR>
#	SQL_C_INTERVAL_DAY_TO_MINUTE</TD>
#	<TD width=50%>SQL_CODE_DAY_TO_MINUTE</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_INTERVAL_DAY_TO_SECOND/<BR>
#	SQL_C_INTERVAL_DAY_TO_SECOND</TD>
#	<TD width=50%>SQL_CODE_DAY_TO_SECOND</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_INTERVAL_HOUR/<BR>
#	SQL_C_INTERVAL_HOUR</TD>
#	<TD width=50%>SQL_CODE_HOUR</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_INTERVAL_HOUR_TO_MINUTE/<BR>
#	SQL_C_INTERVAL_HOUR_TO_MINUTE</TD>
#	<TD width=50%>SQL_CODE_HOUR_TO_MINUTE</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_INTERVAL_HOUR_TO_SECOND/<BR>
#	SQL_C_INTERVAL_HOUR_TO_SECOND</TD>
#	<TD width=50%>SQL_CODE_HOUR_TO_SECOND</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_INTERVAL_MINUTE/<BR>
#	SQL_C_INTERVAL_MINUTE</TD>
#	<TD width=50%>SQL_CODE_MINUTE</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_INTERVAL_MINUTE_TO_SECOND/<BR>
#	SQL_C_INTERVAL_MINUTE_TO_SECOND</TD>
#	<TD width=50%>SQL_CODE_MINUTE_TO_SECOND</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_INTERVAL_MONTH/<BR>
#	SQL_C_INTERVAL_MONTH</TD>
#	<TD width=50%>SQL_CODE_MONTH</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_INTERVAL_SECOND/<BR>
#	SQL_C_INTERVAL_SECOND</TD>
#	<TD width=50%>SQL_CODE_SECOND</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_INTERVAL_YEAR/<BR>
#	SQL_C_INTERVAL_YEAR</TD>
#	<TD width=50%>SQL_CODE_YEAR</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_INTERVAL_YEAR_TO_MONTH/<BR>
#	SQL_C_INTERVAL_YEAR_TO_MONTH</TD>
#	<TD width=50%>SQL_CODE_YEAR_TO_MONTH</TD>
#	</TR>
#	</table></div>
#	
#	<!--TS:-->
#	
#	<P>For more information about the data intervals and this field, see "<A HREF="odbcdata_type_identifiers_and_descriptors.htm">Data Type Identifiers and Descriptors</A>" in Appendix D: Data Types.
#	</dd>
#	
#	<DT><B>SQL_DESC_DATETIME_INTERVAL_PRECISION [All]</B></DT>
#	
#	<DD>This SQLINTEGER record field contains the interval leading precision if the SQL_DESC_TYPE field is SQL_INTERVAL. When the SQL_DESC_DATETIME_INTERVAL_CODE field is set to an interval data type, this field is set to the default interval leading precision.</dd>
#	
#	<DT><B>SQL_DESC_DISPLAY_SIZE [IRDs]</B></DT>
#	
#	<DD>This read-only SQLINTEGER record field contains the maximum number of characters required to display the data from the column. </dd>
#	
#	<DT><B>SQL_DESC_FIXED_PREC_SCALE [Implementation descriptors]</B></DT>
#	
#	<DD>This read-only SQLSMALLINT record field is set to SQL_TRUE if the column is an exact numeric column and has a fixed precision and nonzero scale, or to SQL_FALSE if the column is not an exact numeric column with a fixed precision and scale.</dd>
#	
#	<DT><B>SQL_DESC_INDICATOR_PTR [Application descriptors]</B></DT>
#	
#	<DD>In ARDs, this SQLINTEGER * record field points to the indicator variable. This variable contains SQL_NULL_DATA if the column value is a NULL. For APDs, the indicator variable is set to SQL_NULL_DATA to specify NULL dynamic arguments. Otherwise, the variable is zero (unless the values in SQL_DESC_INDICATOR_PTR and SQL_DESC_OCTET_LENGTH_PTR are the same pointer).
#	
#	<P>If the SQL_DESC_INDICATOR_PTR field in an ARD is a null pointer, the driver is prevented from returning information about whether the column is NULL or not. If the column is NULL and SQL_DESC_INDICATOR_PTR is a null pointer, SQLSTATE 22002 (Indicator variable required but not supplied) is returned when the driver attempts to populate the buffer after a call to <B>SQLFetch</B> or <B>SQLFetchScroll</B>. If the call to <B>SQLFetch</B> or <B>SQLFetchScroll </B>did not return SQL_SUCCESS or SQL_SUCCESS_WITH_INFO, the contents of the buffer are undefined.
#	
#	
#	<P>The SQL_DESC_INDICATOR_PTR field determines whether the field pointed to by SQL_DESC_OCTET_LENGTH_PTR is set. If the data value for a column is NULL, the driver sets the indicator variable to SQL_NULL_DATA. The field pointed to by SQL_DESC_OCTET_LENGTH_PTR is then not set. If a NULL value is not encountered during the fetch, the buffer pointed to by SQL_DESC_INDICATOR_PTR is set to zero and the buffer pointed to by SQL_DESC_OCTET_LENGTH_PTR is set to the length of the data.
#	
#	
#	<P>If the SQL_DESC_INDICATOR_PTR field in an APD is a null pointer, the application cannot use this descriptor record to specify NULL arguments.
#	
#	
#	<P>This field is a <I>deferred field</I>: It is not used at the time it is set but is used at a later time by the driver to indicate nullability (for ARDs) or to determine nullability (for APDs).
#	</dd>
#	
#	<DT><B>SQL_DESC_LABEL [IRDs]</B></DT>
#	
#	<DD>This read-only SQLCHAR * record field contains the column label or title. If the column does not have a label, this variable contains the column name. If the column is unnamed and unlabeled, this variable contains an empty string.</dd>
#	
#	<DT><B>SQL_DESC_LENGTH [All]</B></DT>
#	
#	<DD>This SQLUINTEGER record field is either the maximum or actual  length of a character string in characters or a binary data type in bytes. It is the maximum  length for a fixed-length data type, or the actual length for a variable-length data type. Its value always excludes the null-termination character that ends the character string. For values whose type is SQL_TYPE_DATE, SQL_TYPE_TIME, SQL_TYPE_TIMESTAMP, or one of the SQL interval data types, this field has the length in characters of the character string representation of the datetime or interval value. 
#	
#	<P>The value in this field may be different from the value for "length" as defined in ODBC 2<I>.x</I>. For more information, see <A HREF="odbcdata_types.htm">Appendix D: Data Types</A>.
#	</dd>
#	
#	<DT><B>SQL_DESC_LITERAL_PREFIX [IRDs]</B></DT>
#	
#	<DD>This read-only SQLCHAR * record field contains the character or characters that the driver recognizes as a prefix for a literal of this data type. This variable contains an empty string for a data type for which a literal prefix is not applicable.</dd>
#	
#	<DT><B>SQL_DESC_LITERAL_SUFFIX [IRDs]</B></DT>
#	
#	<DD>This read-only SQLCHAR * record field contains the character or characters that the driver recognizes as a suffix for a literal of this data type. This variable contains an empty string for a data type for which a literal suffix is not applicable.</dd>
#	
#	<DT><B>SQL_DESC_LOCAL_TYPE_NAME [Implementation descriptors]</B></DT>
#	
#	<DD>This read-only SQLCHAR * record field contains any localized (native language) name for the data type that may be different from the regular name of the data type. If there is no localized name, an empty string is returned. This field is for display purposes only.</dd>
#	
#	<DT><B>SQL_DESC_NAME [Implementation descriptors]</B></DT>
#	
#	<DD>This SQLCHAR * record field in a row descriptor contains the column alias, if it applies. If the column alias does not apply, the column name is returned. In either case, the driver sets the SQL_DESC_UNNAMED field to SQL_NAMED when it sets the SQL_DESC_NAME field. If there is no column name or a column alias, the driver returns an empty string in the SQL_DESC_NAME field and sets the SQL_DESC_UNNAMED field to SQL_UNNAMED. 
#	
#	<P>An application can set the SQL_DESC_NAME field of an IPD to a parameter name or alias to specify stored procedure parameters by name. (For more information, see "<A HREF="odbcbinding_parameters_by_name__named_parameters_.htm">Binding Parameters by Name (Named Parameters)</A>" in Chapter 9: Executing Statements.) The SQL_DESC_NAME field of an IRD is a read-only field; SQLSTATE HY091 (Invalid descriptor field identifier) will be returned if an application attempts to set it.
#	
#	
#	<P>In IPDs, this field is undefined if the driver does not support named parameters. If the driver supports named parameters and is capable of describing parameters, the parameter name is returned in this field.
#	</dd>
#	
#	<DT><B>SQL_DESC_NULLABLE [Implementation descriptors]</B></DT>
#	
#	<DD>In IRDs, this read-only SQLSMALLINT record field is SQL_NULLABLE if the column can have NULL values, SQL_NO_NULLS if the column does not have NULL values, or SQL_NULLABLE_UNKNOWN if it is not known whether the column accepts NULL values. This field pertains to the result set column, not the base column.
#	
#	<P>In IPDs, this field is always set to SQL_NULLABLE because dynamic parameters are always nullable and cannot be set by an application.
#	</dd>
#	
#	<DT><B>SQL_DESC_NUM_PREC_RADIX [All]</B></DT>
#	
#	<DD>This SQLINTEGER field contains a value of 2 if the data type in the SQL_DESC_TYPE field is an approximate numeric data type, because the SQL_DESC_PRECISION field contains the number of bits. This field contains a value of 10 if the data type in the SQL_DESC_TYPE field is an exact numeric data type, because the SQL_DESC_PRECISION field contains the number of decimal digits. This field is set to 0 for all non-numeric data types.</dd>
#	
#	<DT><B>SQL_DESC_OCTET_LENGTH [All]</B></DT>
#	
#	<DD>This SQLINTEGER record field contains the length, in bytes, of a character string or binary data type. For fixed-length character or binary types, this is the actual length in bytes. For variable-length character or binary types, this is the maximum length in bytes. This value always excludes space for the null-termination character for implementation descriptors and always includes space for the null-termination character for application descriptors. For application data, this field contains the size of the buffer. For APDs, this field is defined only for output or input/output parameters.</dd>
#	
#	<DT><B>SQL_DESC_OCTET_LENGTH_PTR [Application descriptors]</B></DT>
#	
#	<DD>This SQLINTEGER * record field points to a variable that will contain the total length in bytes of a dynamic argument (for parameter descriptors) or of a bound column value (for row descriptors). 
#	
#	<P>For an APD, this value is ignored for all arguments except character string and binary; if this field points to SQL_NTS, the dynamic argument must be null-terminated. To indicate that a bound parameter will be a data-at-execution parameter, an application sets this field in the appropriate record of the APD to a variable that, at execute time, will contain the value SQL_DATA_AT_EXEC or the result of the SQL_LEN_DATA_AT_EXEC macro. If there is more than one such field, SQL_DESC_DATA_PTR can be set to a value uniquely identifying the parameter to help the application determine which parameter is being requested.
#	
#	
#	<P>If the OCTET_LENGTH_PTR field of an ARD is a null pointer, the driver does not return length information for the column. If the SQL_DESC_OCTET_LENGTH_PTR field of an APD is a null pointer, the driver assumes that character strings and binary values are null-terminated. (Binary values should not be null-terminated but should be given a length to avoid truncation.)
#	
#	
#	<P>If the call to <B>SQLFetch</B> or <B>SQLFetchScroll</B> that fills in the buffer pointed to by this field did not return SQL_SUCCESS or SQL_SUCCESS_WITH_INFO, the contents of the buffer are undefined. This field is a <I>deferred field</I>. It is not used at the time it is set but is used at a later time by the driver to determine or indicate the octet length of the data.
#	</dd>
#	
#	<DT><B>SQL_DESC_PARAMETER_TYPE [IPDs]</B></DT>
#	
#	<DD>This SQLSMALLINT record field is set to SQL_PARAM_INPUT for an input parameter, SQL_PARAM_INPUT_OUTPUT for an input/output parameter, or SQL_PARAM_OUTPUT for an output parameter. It is set to SQL_PARAM_INPUT by default. 
#	
#	<P>For an IPD, the field is set to SQL_PARAM_INPUT by default if the IPD is not automatically populated by the driver (the SQL_ATTR_ENABLE_AUTO_IPD statement attribute is SQL_FALSE). An application should set this field in the IPD for parameters that are not input parameters.
#	</dd>
#	
#	<DT><B>SQL_DESC_PRECISION [All]</B></DT>
#	
#	<DD>This SQLSMALLINT record field contains the number of digits for an exact numeric type, the number of bits in the mantissa (binary precision) for an approximate numeric type, or the numbers of digits in the fractional seconds component for the SQL_TYPE_TIME, SQL_TYPE_TIMESTAMP, or SQL_INTERVAL_SECOND data type. This field is undefined for all other data types.
#	
#	<P>The value in this field may be different from the value for "precision" as defined in ODBC 2<I>.x</I>. For more information, see <A HREF="odbcdata_types.htm">Appendix D: Data Types</A>.
#	</dd>
#	
#	<DT><B>SQL_DESC_ROWVER [Implementation descriptors]</B></DT>
#	
#	<DD>This SQLSMALLINT<B> </B>record field indicates whether a column is automatically modified by the DBMS when a row is updated (for example, a column of the type "timestamp" in SQL Server). The value of this record field is set to SQL_TRUE if the column is a row versioning column, and to SQL_FALSE otherwise. This column attribute is similar to calling <B>SQLSpecialColumns</B> with IdentifierType of SQL_ROWVER to determine whether a column is automatically updated.</dd>
#	
#	<DT><B>SQL_DESC_SCALE [All]</B></DT>
#	
#	<DD>This SQLSMALLINT record field contains the defined scale for decimal and numeric data types. The field is undefined for all other data types.
#	
#	<P>The value in this field may be different from the value for "scale" as defined in ODBC 2<I>.x</I>. For more information, see <A HREF="odbcdata_types.htm">Appendix D: Data Types</A>.
#	</dd>
#	
#	<DT><B>SQL_DESC_SCHEMA_NAME [IRDs]</B></DT>
#	
#	<DD>This read-only SQLCHAR * record field contains the schema name of the base table that contains the column. The return value is driver-dependent if the column is an expression or if the column is part of a view. If the data source does not support schemas or the schema name cannot be determined, this variable contains an empty string.</dd>
#	
#	<DT><B>SQL_DESC_SEARCHABLE [IRDs]</B></DT>
#	
#	<DD>This read-only SQLSMALLINT record field is set to one of the following values:
#	
#	<UL type=disc>
#		<LI>SQL_PRED_NONE if the column cannot be used in a <B>WHERE</B> clause. (This is the same as the SQL_UNSEARCHABLE value in ODBC 2<I>.x</I>.)</li>
#	
#		<LI>SQL_PRED_CHAR if the column can be used in a <B>WHERE</B> clause but only with the <B>LIKE</B> predicate. (This is the same as the SQL_LIKE_ONLY value in ODBC 2<I>.x</I>.)</li>
#	
#		<LI>SQL_PRED_BASIC if the column can be used in a <B>WHERE</B> clause with all the comparison operators except <B>LIKE</B>. (This is the same as the SQL_EXCEPT_LIKE value in ODBC 2<I>.x</I>.)</li>
#	
#		<LI>SQL_PRED_SEARCHABLE if the column can be used in a <B>WHERE</B> clause with any comparison operator.</li>
#	</UL>
#	</dd>
#	
#	<DT><B>SQL_DESC_TABLE_NAME [IRDs]</B></DT>
#	
#	<DD>This read-only SQLCHAR * record field contains the name of the base table that contains this column. The return value is driver-dependent if the column is an expression or if the column is part of a view.</dd>
#	
#	<DT><B>SQL_DESC_TYPE [All]</B></DT>
#	
#	<DD>This SQLSMALLINT record field specifies the concise SQL or C data type for all data types except datetime and interval data types. For the datetime and interval data types, this field specifies the verbose data type, which is SQL_DATETIME or SQL_INTERVAL.
#	
#	<P>Whenever this field contains SQL_DATETIME or SQL_INTERVAL, the SQL_DESC_DATETIME_INTERVAL_CODE field must contain the appropriate subcode for the concise type. For datetime data types, SQL_DESC_TYPE contains SQL_DATETIME, and the SQL_DESC_DATETIME_INTERVAL_CODE field contains a subcode for the specific datetime data type. For interval data types, SQL_DESC_TYPE contains SQL_INTERVAL and the SQL_DESC_DATETIME_INTERVAL_CODE field contains a subcode for the specific interval data type.
#	
#	
#	<P>The values in the SQL_DESC_TYPE and SQL_DESC_CONCISE_TYPE fields are interdependent. Each time one of the fields is set, the other must also be set. SQL_DESC_TYPE can be set by a call to <B>SQLSetDescField</B> or <B>SQLSetDescRec</B>. SQL_DESC_CONCISE_TYPE can be set by a call to <B>SQLBindCol</B> or <B>SQLBindParameter</B>, or <B>SQLSetDescField</B>. 
#	
#	
#	<P>If SQL_DESC_TYPE is set to a concise data type other than an interval or datetime data type, the SQL_DESC_CONCISE_TYPE field is set to the same value and the SQL_DESC_DATETIME_INTERVAL_CODE field is set to 0.
#	
#	
#	<P>If SQL_DESC_TYPE is set to the verbose datetime or interval data type (SQL_DATETIME or SQL_INTERVAL) and the SQL_DESC_DATETIME_INTERVAL_CODE field is set to the appropriate subcode, the SQL_DESC_CONCISE TYPE field is set to the corresponding concise type. Trying to set SQL_DESC_TYPE to one of the concise datetime or interval types will return SQLSTATE HY021 (Inconsistent descriptor information).
#	
#	
#	<P>When the SQL_DESC_TYPE field is set by a call to <B>SQLBindCol</B>, <B>SQLBindParameter</B>, or <B>SQLSetDescField</B>, the following fields are set to the following default values, as shown in the table below. The values of the remaining fields of the same record are undefined.
#	
#	<!--TS:-->
#	<div class="tablediv"><table>
#	
#	<TR VALIGN="top">
#	<TH width=48%>Value of SQL_DESC_TYPE</TH>
#	<TH width=52%>Other fields implicitly set</TH>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=48%>SQL_CHAR, SQL_VARCHAR, SQL_C_CHAR, SQL_C_VARCHAR</TD>
#	<TD width=52%>SQL_DESC_LENGTH is set to 1. SQL_DESC_PRECISION is set to 0.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=48%>SQL_DATETIME</TD>
#	<TD width=52%>When SQL_DESC_DATETIME_INTERVAL_CODE is set to SQL_CODE_DATE or SQL_CODE_TIME, SQL_DESC_PRECISION is set to 0. When it is set to SQL_DESC_TIMESTAMP, SQL_DESC_PRECISION is set to 6.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=48%>SQL_DECIMAL, SQL_NUMERIC,<BR>
#	SQL_C_NUMERIC</TD>
#	<TD width=52%>SQL_DESC_SCALE is set to 0. SQL_DESC_PRECISION is set to the implementation-defined precision for the respective data type.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=48%>SQL_FLOAT, SQL_C_FLOAT</TD>
#	<TD width=52%>SQL_DESC_PRECISION is set to the implementation-defined default precision for SQL_FLOAT.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=48%>SQL_INTERVAL</TD>
#	<TD width=52%>When SQL_DESC_DATETIME_INTERVAL_CODE is set to an interval data type, SQL_DESC_DATETIME_INTERVAL_PRECISION is set to 2 (the default interval leading precision). When the interval has a seconds component, SQL_DESC_PRECISION is set to 6 (the default interval seconds precision).</TD>
#	</TR>
#	</table></div>
#	
#	<!--TS:-->
#	
#	<P>When an application calls <B>SQLSetDescField</B> to set fields of a descriptor rather than calling <B>SQLSetDescRec</B>, the application must first declare the data type. When it does, the other fields indicated in the previous table are implicitly set. If any of the values implicitly set are unacceptable, the application can then call <B>SQLSetDescField</B> or <B>SQLSetDescRec</B> to set the unacceptable value explicitly.
#	</dd>
#	
#	<DT><B>SQL_DESC_TYPE_NAME [Implementation descriptors]</B></DT>
#	
#	<DD>This read-only SQLCHAR * record field contains the data source&#0150;dependent type name (for example, "CHAR", "VARCHAR", and so on). If the data type name is unknown, this variable contains an empty string.</dd>
#	
#	<DT><B>SQL_DESC_UNNAMED [Implementation descriptors]</B></DT>
#	
#	<DD>This SQLSMALLINT record field in a row descriptor is set by the driver to either SQL_NAMED or SQL_UNNAMED when it sets the SQL_DESC_NAME field. If the SQL_DESC_NAME field contains a column alias or if the column alias does not apply, the driver sets the SQL_DESC_UNNAMED field to SQL_NAMED. If an application sets the SQL_DESC_NAME field of an IPD to a parameter name or alias, the driver sets the SQL_DESC_UNNAMED field of the IPD to SQL_NAMED. If there is no column name or a column alias, the driver sets the SQL_DESC_UNNAMED field to SQL_UNNAMED. 
#	
#	<P>An application can set the SQL_DESC_UNNAMED field of an IPD to SQL_UNNAMED. A driver returns SQLSTATE HY091 (Invalid descriptor field identifier) if an application attempts to set the SQL_DESC_UNNAMED field of an IPD to SQL_NAMED. The SQL_DESC_UNNAMED field of an IRD is read-only; SQLSTATE HY091 (Invalid descriptor field identifier) will be returned if an application attempts to set it.
#	</dd>
#	
#	<DT><B>SQL_DESC_UNSIGNED [Implementation descriptors]</B></DT>
#	
#	<DD>This read-only SQLSMALLINT record field is set to SQL_TRUE if the column type is unsigned or non-numeric, or SQL_FALSE if the column type is signed.</dd>
#	
#	<DT><B>SQL_DESC_UPDATABLE [IRDs]</B></DT>
#	
#	<DD>This read-only SQLSMALLINT record field is set to one of the following values:
#	
#	<UL type=disc>
#		<LI>SQL_ATTR_READ_ONLY if the result set column is read-only.</li>
#	
#		<LI>SQL_ATTR_WRITE if the result set column is read-write.</li>
#	
#		<LI>SQL_ATTR_READWRITE_UNKNOWN if it is not known whether the result set column is updatable or not.</li>
#	</UL>
#	
#	
#	<P>SQL_DESC_UPDATABLE describes the updatability of the column in the result set, not the column in the base table. The updatability of the column in the base table on which this result set column is based may be different than the value in this field. Whether a column is updatable can be based on the data type, user privileges, and the definition of the result set itself. If it is unclear whether a column is updatable, SQL_ATTR_READWRITE_UNKNOWN should be returned.
#	</dd>
#	</DL>
#	
#	<H1>Consistency Checks</H1>
#	
#	<P>A consistency check is performed by the driver automatically whenever an application passes in a value for the SQL_DESC_DATA_PTR field of the ARD, APD, or IPD. If any of the fields is inconsistent with other fields, <B>SQLSetDescField</B> will return SQLSTATE HY021 (Inconsistent descriptor information). For more information, see "Consistency Check" in <A HREF="odbcsqlsetdescrec.htm">SQLSetDescRec</A>.</P>
#	
#	<P class="label"><B>Related Functions</B></P>
#	<!--TS:--><div class="tablediv"><table>
#	
#	<TR VALIGN="top">
#	<TH width=48%>For information about</TH>
#	<TH width=52%>See</TH>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=48%>Binding a column</TD>
#	<TD width=52%><A HREF="odbcsqlbindcol.htm">SQLBindCol</A></TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=48%>Binding a parameter</TD>
#	<TD width=52%><A HREF="odbcsqlbindparameter.htm">SQLBindParameter</A></TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=48%>Getting a descriptor field</TD>
#	<TD width=52%><A HREF="odbcsqlgetdescfield.htm">SQLGetDescField</A></TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=48%>Getting multiple descriptor fields</TD>
#	<TD width=52%><A HREF="odbcsqlgetdescrec.htm">SQLGetDescRec</A></TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=48%>Setting multiple descriptor fields</TD>
#	<TD width=52%><A HREF="odbcsqlsetdescrec.htm">SQLSetDescRec</A></TD>
#	</TR>
#	</table></div>
#	<!--TS:--><H4><A NAME="feedback"></A></H4>
#	<SPAN id="SDKFeedB"></SPAN>
#	</div>
#	
#	</BODY>
#	</HTML>
};
