# usage: perl Attr.data X (X = Env,Dbc,Stmt)
# prints template for AttrX.cpp
use strict;
my $type = shift;
my $order = 0;

#
# odbcsqlsetenvattr.htm
#
my $attrEnv = {
#	<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 3.2 Final//EN">
#	<HTML DIR="LTR"><HEAD>
#	<META HTTP-EQUIV="Content-Type" Content="text/html; charset=Windows-1252">
#	<TITLE>SQLSetEnvAttr</TITLE>
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
#	<H1><A NAME="odbcsqlsetenvattr"></A>SQLSetEnvAttr</H1>
#	
#	<P class="label"><B>Conformance</B></P>
#	
#	<P>Version Introduced: ODBC 3.0<BR>
#	Standards Compliance: ISO 92</P>
#	
#	<P class="label"><B>Summary</B></P>
#	
#	<P><B>SQLSetEnvAttr</B> sets attributes that govern aspects of environments.</P>
#	
#	<P class="label"><B>Syntax</B></P>
#	
#	<PRE class="syntax">SQLRETURN <B>SQLSetEnvAttr</B>(
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLHENV&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>EnvironmentHandle</I>,
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLINTEGER&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>Attribute</I>,
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLPOINTER&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>ValuePtr</I>,
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLINTEGER&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>StringLength</I>);</PRE>
#	
#	<P class="label"><B>Arguments</B>
#	
#	<DL>
#	<DT><I>EnvironmentHandle</I></DT>
#	
#	<DD>[Input]<BR>
#	Environment handle.</dd>
#	
#	<DT><I>Attribute</I></DT>
#	
#	<DD>[Input]<BR>
#	Attribute to set, listed in "Comments."</dd>
#	
#	<DT><I>ValuePtr</I></DT>
#	
#	<DD>[Input]<BR>
#	Pointer to the value to be associated with <I>Attribute</I>. Depending on the value of <I>Attribute</I>, <I>ValuePtr</I> will be a 32-bit integer value or point to a null-terminated character string.</dd>
#	
#	<DT><I>StringLength</I></DT>
#	
#	<DD>[Input] If <I>ValuePtr</I> points to a character string or a binary buffer, this argument should be the length of *<I>ValuePtr</I>. If <I>ValuePtr</I> is an integer, <I>StringLength</I> is ignored.</dd>
#	</DL>
#	
#	<P class="label"><B>Returns</B></P>
#	
#	<P>SQL_SUCCESS, SQL_SUCCESS_WITH_INFO, SQL_ERROR, or SQL_INVALID_HANDLE.</P>
#	
#	<P class="label"><B>Diagnostics</B></P>
#	
#	<P>When <B>SQLSetEnvAttr</B> returns SQL_ERROR or SQL_SUCCESS_WITH_INFO, an associated SQLSTATE value can be obtained by calling <B>SQLGetDiagRec</B> with a <I>HandleType</I> of SQL_HANDLE_ENV and a <I>Handle</I> of <I>EnvironmentHandle</I>. The following table lists the SQLSTATE values commonly returned by <B>SQLSetEnvAttr</B> and explains each one in the context of this function; the notation "(DM)" precedes the descriptions of SQLSTATEs returned by the Driver Manager. The return code associated with each SQLSTATE value is SQL_ERROR, unless noted otherwise. If a driver does not support an environment attribute, the error can be returned only during connect time.</P>
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
#	<TD width=52%>The driver did not support the value specified in <I>ValuePtr</I> and substituted a similar value. (Function returns SQL_SUCCESS_WITH_INFO.)</TD>
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
#	<TD width=22%>HY009</TD>
#	<TD width=26%>Invalid use of null pointer</TD>
#	<TD width=52%>The Attribute argument identified an environment attribute that required a string value, and the <I>ValuePtr</I> argument was a null pointer.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HY010</TD>
#	<TD width=26%>Function sequence error</TD>
#	<TD width=52%>(DM) A connection handle has been allocated on <I>EnvironmentHandle</I>. </TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HY013</TD>
#	<TD width=26%>Memory management error</TD>
#	<TD width=52%>The function call could not be processed because the underlying memory objects could not be accessed, possibly because of low memory conditions.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HY024</TD>
#	<TD width=26%>Invalid attribute value</TD>
#	<TD width=52%>Given the specified <I>Attribute</I> value, an invalid value was specified in <I>ValuePtr</I>.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HY090</TD>
#	<TD width=26%>Invalid string or buffer length</TD>
#	<TD width=52%>The <I>StringLength</I> argument was less than 0 but was not SQL_NTS.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HY092</TD>
#	<TD width=26%>Invalid attribute/option identifier</TD>
#	<TD width=52%>(DM) The value specified for the argument <I>Attribute</I> was not valid for the version of ODBC supported by the driver.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HYC00</TD>
#	<TD width=26%>Optional feature not implemented</TD>
#	<TD width=52%>The value specified for the argument <I>Attribute</I> was a valid ODBC environment attribute for the version of ODBC supported by the driver, but was not supported by the driver.
#	<P>(DM) The <I>Attribute</I> argument was SQL_ATTR_OUTPUT_NTS, and <I>ValuePtr</I> was SQL_FALSE.</P>
#	</TD>
#	</TR>
#	</table></div>
#	<!--TS:-->
#	<P class="label"><B>Comments</B></P>
#	
#	<P>An application can call <B>SQLSetEnvAttr</B> only if no connection handle is allocated on the environment. All environment attributes successfully set by the application for the environment persist until <B>SQLFreeHandle</B> is called on the environment. More than one environment handle can be allocated simultaneously in ODBC 3<I>.x</I>.</P>
#	
#	<P>The format of information set through <I>ValuePtr</I> depends on the specified <I>Attribute</I>. <B>SQLSetEnvAttr</B> will accept attribute information in one of two different formats: a null-terminated character string or a 32-bit integer value. The format of each is noted in the attribute's description.</P>
#	
#	<P>There are no driver-specific environment attributes.</P>
#	
#	<P>Connection attributes cannot be set by a call to <B>SQLSetEnvAttr</B>. Attempting to do so will return SQLSTATE HY092 (Invalid attribute/option identifier).</P>
#	<!--TS:--><div class="tablediv"><table>
#	
#	<TR VALIGN="top">
#	<TH width=35%><I>Attribute</I></TH>
#	<TH width=65%><I>ValuePtr</I> contents</TH>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=35%>SQL_ATTR_CONNECTION_POOLING<BR>
#	(ODBC 3.0)</TD>
#	<TD width=65%>A 32-bit SQLUINTEGER value that enables or disables connection pooling at the environment level. The following values are used:
#	<P>SQL_CP_OFF = Connection pooling is turned off. This is the default.</P>
#	
#	<P>SQL_CP_ONE_PER_DRIVER = A single connection pool is supported for each driver. Every connection in a pool is associated with one driver.</P>
#	
#	<P>SQL_CP_ONE_PER_HENV = A single connection pool is supported for each environment. Every connection in a pool is associated with one environment.</P>
#	
#	<P>Connection pooling is enabled by calling <B>SQLSetEnvAttr</B> to set the SQL_ATTR_CONNECTION_POOLING attribute to SQL_CP_ONE_PER_DRIVER or SQL_CP_ONE_PER_HENV. This call must be made before the application allocates the shared environment for which connection pooling is to be enabled. The environment handle in the call to <B>SQLSetEnvAttr</B> is set to null, which makes SQL_ATTR_CONNECTION_POOLING a process-level attribute. After connection pooling is enabled, the application then allocates an implicit shared environment by calling <B>SQLAllocHandle</B> with the <I>InputHandle</I> argument set to SQL_HANDLE_ENV.</P>
#	
#	<P>After connection pooling has been enabled and a shared environment has been selected for an application, SQL_ATTR_CONNECTION_POOLING cannot be reset for that environment, because <B>SQLSetEnvAttr</B> is called with a null environment handle when setting this attribute. If this attribute is set while connection pooling is already enabled on a shared environment, the attribute affects only shared environments that are allocated subsequently.</P>
#	
#	<P>For more information, see "<A HREF="odbcodbc_connection_pooling.htm">ODBC Connection Pooling</A>" in Chapter 6: Connecting to a Data Source or Driver.</P>
#	</TD>
#	</TR>
    SQL_ATTR_CONNECTION_POOLING => {
	type => q(SQLUINTEGER),
	ptr => 0,
	value => [ qw(SQL_CP_OFF SQL_CP_ONE_PER_DRIVER SQL_CP_ONE_PER_HENV) ],
	default => q(SQL_CP_OFF),
	mode => 'rw',
	order => ++$order,
    },
#	<TR VALIGN="top">
#	<TD width=35%>SQL_ATTR_CP_MATCH<BR>
#	(ODBC 3.0)</TD>
#	<TD width=65%>A 32-bit SQLUINTEGER value that determines how a connection is chosen from a connection pool. When <B>SQLConnect</B> or <B>SQLDriverConnect</B> is called, the Driver Manager determines which connection is reused from the pool. The Driver Manager attempts to match the connection options in the call and the connection attributes set by the application to the keywords and connection attributes of the connections in the pool. The value of this attribute determines the level of precision of the matching criteria.
#	<P>The following values are used to set the value of this attribute:</P>
#	
#	<P>SQL_CP_STRICT_MATCH = Only connections that exactly match the connection options in the call and the connection attributes set by the application are reused. This is the default.</P>
#	
#	<P>SQL_CP_RELAXED_MATCH = Connections with matching connection string keywords can be used. Keywords must match, but not all connection attributes must match.</P>
#	
#	<P>For more information on how the Driver Manager performs the match in connecting to a pooled connection, see <A HREF="odbcsqlconnect.htm">SQLConnect</A>. For more information on connection pooling, see "<A HREF="odbcodbc_connection_pooling.htm">ODBC Connection Pooling</A>" in Chapter 6: Connecting to a Data Source or Driver.</P>
#	</TD>
#	</TR>
    SQL_ATTR_CP_MATCH => {
	type => q(SQLUINTEGER),
	ptr => 0,
	value => [ qw(SQL_CP_STRICT_MATCH SQL_CP_RELAXED_MATCH) ],
	default => q(SQL_CP_STRICT_MATCH),
	mode => 'rw',
	order => ++$order,
    },
#	<TR VALIGN="top">
#	<TD width=35%>SQL_ATTR_ODBC_VERSION<BR>
#	(ODBC 3.0)</TD>
#	<TD width=65%>A 32-bit integer that determines whether certain functionality exhibits ODBC 2<I>.x</I> behavior or ODBC 3<I>.x</I> behavior. The following values are used to set the value of this attribute:
#	<P>SQL_OV_ODBC3 = The Driver Manager and driver exhibit the following ODBC 3<I>.x</I> behavior:
#	
#	<UL type=disc>
#		<LI>The driver returns and expects ODBC 3<I>.x</I> codes for date, time, and timestamp.</li>
#	
#		<LI>The driver returns ODBC 3<I>.x</I> SQLSTATE codes when <B>SQLError</B>, <B>SQLGetDiagField</B>, or <B>SQLGetDiagRec</B> is called.</li>
#	
#		<LI>The <I>CatalogName</I> argument in a call to <B>SQLTables</B> accepts a search pattern.</li>
#	</UL>
#	
#	<P>SQL_OV_ODBC2 = The Driver Manager and driver exhibit the following ODBC 2<I>.x </I>behavior. This is especially useful for an ODBC 2<I>.x</I> application working with an ODBC 3<I>.x</I> driver.
#	
#	<UL type=disc>
#		<LI>The driver returns and expects ODBC 2<I>.x</I> codes for date, time, and timestamp.</li>
#	
#		<LI>The driver returns ODBC 2<I>.x</I> SQLSTATE codes when <B>SQLError</B>, <B>SQLGetDiagField</B>, or <B>SQLGetDiagRec</B> is called.</li>
#	
#		<LI>The <I>CatalogName</I> argument in a call to <B>SQLTables</B> does not accept a search pattern.</li>
#	</UL>
#	
#	<P>An application must set this environment attribute before calling any function that has an SQLHENV argument, or the call will return SQLSTATE HY010 (Function sequence error). It is driver-specific whether or not additional behaviors exist for these environmental flags.
#	
#	<UL type=disc>
#		<LI>For more information, see "<A HREF="odbcdeclaring_the_application_s_odbc_version.htm">Declaring the Application's ODBC Version</A>" in Chapter 6: Connecting to a Data Source or Driver and "<A HREF="odbcbehavioral_changes.htm">Behavioral Changes</A>" in Chapter 17: Programming Considerations.</li>
#	</UL>
#	</TD>
#	</TR>
    SQL_ATTR_ODBC_VERSION => {
	type => q(SQLINTEGER),
	ptr => 0,
	value => [ qw(SQL_OV_ODBC3 SQL_OV_ODBC2) ],
	default => undef,
	mode => 'rw',
	order => ++$order,
    },
#	<TR VALIGN="top">
#	<TD width=35%>SQL_ATTR_OUTPUT_NTS<BR>
#	(ODBC 3.0)</TD>
#	<TD width=65%>A 32-bit integer that determines how the driver returns string data. If SQL_TRUE, the driver returns string data null-terminated. If SQL_FALSE, the driver does not return string data null-terminated.
#	<P>This attribute defaults to SQL_TRUE. A call to <B>SQLSetEnvAttr</B> to set it to SQL_TRUE returns SQL_SUCCESS. A call to <B>SQLSetEnvAttr</B> to set it to SQL_FALSE returns SQL_ERROR and SQLSTATE HYC00 (Optional feature not implemented).</P>
#	</TD>
#	</TR>
    SQL_ATTR_OUTPUT_NTS => {
	type => q(SQLINTEGER),
	ptr => 0,
	value => [ qw(SQL_FALSE SQL_TRUE) ],
	default => q(SQL_TRUE),
	mode => 'rw',
	order => ++$order,
    }
#	</table></div>
#	<!--TS:-->
#	<P class="label"><B>Related Functions</B></P>
#	<!--TS:--><div class="tablediv"><table>
#	
#	<TR VALIGN="top">
#	<TH width=48%>For information about</TH>
#	<TH width=52%>See</TH>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=48%>Allocating a handle</TD>
#	<TD width=52%><A HREF="odbcsqlallochandle.htm">SQLAllocHandle</A></TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=48%>Returning the setting of an environment attribute</TD>
#	<TD width=52%><A HREF="odbcsqlgetenvattr.htm">SQLGetEnvAttr</A></TD>
#	</TR>
#	</table></div>
#	<!--TS:--><H4><A NAME="feedback"></A></H4>
#	<SPAN id="SDKFeedB"></SPAN>
#	</div>
#	
#	</BODY>
#	</HTML>
};

#
# odbcsqlsetconnectattr.htm
#
my $attrDbc = {
#	<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 3.2 Final//EN">
#	<HTML DIR="LTR"><HEAD>
#	<META HTTP-EQUIV="Content-Type" Content="text/html; charset=Windows-1252">
#	<TITLE>SQLSetConnectAttr</TITLE>
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
#	<H1><A NAME="odbcsqlsetconnectattr"></A>SQLSetConnectAttr</H1>
#	
#	<P class="label"><B>Conformance</B></P>
#	
#	<P>Version Introduced: ODBC 3.0<BR>
#	Standards Compliance: ISO 92</P>
#	
#	<P class="label"><B>Summary</B></P>
#	
#	<P><B>SQLSetConnectAttr</B> sets attributes that govern aspects of connections.</P>
#	
#	<P class="indent"><b class="le">Note</b>&nbsp;&nbsp;&nbsp;For more information about what the Driver Manager maps this function to when an ODBC 3<I>.x</I> application is working with an ODBC 2<I>.x</I> driver, see "<A HREF="odbcmapping_replacement_functions_for_backward_compatibility_of_applications.htm">Mapping Replacement Functions for Backward Compatibility of Applications</A>" in Chapter 17: Programming Considerations.</P>
#	
#	<P class="label"><B>Syntax</B></P>
#	
#	<PRE class="syntax">SQLRETURN <B>SQLSetConnectAttr</B>(
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLHDBC&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>ConnectionHandle</I>,
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLINTEGER&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>Attribute</I>,
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLPOINTER&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>ValuePtr</I>,
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLINTEGER&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>StringLength</I>);</PRE>
#	
#	<P class="label"><B>Arguments</B>
#	
#	<DL>
#	<DT><I>ConnectionHandle</I></DT>
#	
#	<DD>[Input]<BR>
#	Connection handle.</dd>
#	
#	<DT><I>Attribute</I></DT>
#	
#	<DD>[Input]<BR>
#	Attribute to set, listed in "Comments."</dd>
#	
#	<DT><I>ValuePtr</I></DT>
#	
#	<DD>[Input]<BR>
#	Pointer to the value to be associated with <I>Attribute</I>. Depending on the value of <I>Attribute</I>, <I>ValuePtr</I> will be a 32-bit unsigned integer value or will point to a null-terminated character string. Note that if the <I>Attribute</I> argument is a driver-specific value, the value in <I>ValuePtr</I> may be a signed integer.</dd>
#	
#	<DT><I>StringLength</I></DT>
#	
#	<DD>[Input]<BR>
#	If <I>Attribute</I> is an ODBC-defined attribute and <I>ValuePtr</I> points to a character string or a binary buffer, this argument should be the length of *<I>ValuePtr</I>. If <I>Attribute</I> is an ODBC-defined attribute and <I>ValuePtr</I> is an integer, <I>StringLength</I> is ignored.
#	
#	<P>If <I>Attribute</I> is a driver-defined attribute, the application indicates the nature of the attribute to the Driver Manager by setting the <I>StringLength</I> argument. <I>StringLength</I> can have the following values:
#	
#	
#	<UL type=disc>
#		<LI>If <I>ValuePtr</I> is a pointer to a character string, then <I>StringLength</I> is the length of the string or SQL_NTS.</li>
#	
#		<LI>If <I>ValuePtr</I> is a pointer to a binary buffer, then the application places the result of the SQL_LEN_BINARY_ATTR(<I>length</I>) macro in <I>StringLength</I>. This places a negative value in <I>StringLength</I>.</li>
#	
#		<LI>If <I>ValuePtr</I> is a pointer to a value other than a character string or a binary string, then <I>StringLength</I> should have the value SQL_IS_POINTER.</li>
#	
#		<LI>If <I>ValuePtr</I> contains a fixed-length value, then <I>StringLength</I> is either SQL_IS_INTEGER or SQL_IS_UINTEGER, as appropriate.</li>
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
#	<P>When <B>SQLSetConnectAttr</B> returns SQL_ERROR or SQL_SUCCESS_WITH_INFO, an associated SQLSTATE value can be obtained by calling <B>SQLGetDiagRec</B> with a <I>HandleType</I> of SQL_HANDLE_DBC and a <I>Handle</I> of <I>ConnectionHandle</I>. The following table lists the SQLSTATE values commonly returned by <B>SQLSetConnectAttr</B> and explains each one in the context of this function; the notation "(DM)" precedes the descriptions of SQLSTATEs returned by the Driver Manager. The return code associated with each SQLSTATE value is SQL_ERROR, unless noted otherwise.</P>
#	
#	<P>The driver can return SQL_SUCCESS_WITH_INFO to provide information about the result of setting an option.</P>
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
#	<TD width=52%>The driver did not support the value specified in <I>ValuePtr</I> and substituted a similar value. (Function returns SQL_SUCCESS_WITH_INFO.)</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>08002</TD>
#	<TD width=26%>Connection name in use</TD>
#	<TD width=52%>The <I>Attribute</I> argument was SQL_ATTR_ODBC_CURSORS, and the driver was already connected to the data source.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>08003</TD>
#	<TD width=26%>Connection does not exist</TD>
#	<TD width=52%>(DM) An <I>Attribute</I> value was specified that required an open connection, but the <I>ConnectionHandle</I> was not in a connected state.</TD>
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
#	<TD width=52%>The <I>Attribute</I> argument was SQL_ATTR_CURRENT_CATALOG, and a result set was pending.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>3D000</TD>
#	<TD width=26%>Invalid catalog name</TD>
#	<TD width=52%>The <I>Attribute</I> argument was SQL_CURRENT_CATALOG, and the specified catalog name was invalid.</TD>
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
#	<TD width=22%>HY009</TD>
#	<TD width=26%>Invalid use of null pointer</TD>
#	<TD width=52%>The <I>Attribute</I> argument identified a connection attribute that required a string value, and the <I>ValuePtr </I>argument was a null pointer.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HY010</TD>
#	<TD width=26%>Function sequence error</TD>
#	<TD width=52%>(DM) An asynchronously executing function was called for a <I>StatementHandle</I> associated with the <I>ConnectionHandle</I> and was still executing when <B>SQLSetConnectAttr</B> was called.
#	<P>(DM) <B>SQLExecute</B>, <B>SQLExecDirect</B>, <B>SQLBulkOperations</B>, or <B>SQLSetPos</B> was called for a <I>StatementHandle</I> associated with the <I>ConnectionHandle</I> and returned SQL_NEED_DATA. This function was called before data was sent for all data-at-execution parameters or columns.</P>
#	
#	<P>(DM) <B>SQLBrowseConnect</B> was called for the <I>ConnectionHandle</I> and returned SQL_NEED_DATA. This function was called before <B>SQLBrowseConnect</B> returned SQL_SUCCESS_WITH_INFO or SQL_SUCCESS.</P>
#	</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HY011</TD>
#	<TD width=26%>Attribute cannot be set now</TD>
#	<TD width=52%>The <I>Attribute</I> argument was SQL_ATTR_TXN_ISOLATION, and a transaction was open.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HY013</TD>
#	<TD width=26%>Memory management error</TD>
#	<TD width=52%>The function call could not be processed because the underlying memory objects could not be accessed, possibly because of low memory conditions.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HY024</TD>
#	<TD width=26%>Invalid attribute value</TD>
#	<TD width=52%>Given the specified <I>Attribute</I> value, an invalid value was specified in <I>ValuePtr</I>. (The Driver Manager returns this SQLSTATE only for connection and statement attributes that accept a discrete set of values, such as SQL_ATTR_ACCESS_MODE or SQL_ATTR_ASYNC_ENABLE. For all other connection and statement attributes, the driver must verify the value specified in <I>ValuePtr</I>.)
#	<P>The <I>Attribute</I> argument was SQL_ATTR_TRACEFILE or SQL_ATTR_TRANSLATE_LIB, and <I>ValuePtr</I> was an empty string.</P>
#	</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HY090</TD>
#	<TD width=26%>Invalid string or buffer length</TD>
#	<TD width=52%><I>(DM) *ValuePtr </I>is a character string, and the <I>StringLength</I> argument was less than 0 but was not SQL_NTS.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HY092</TD>
#	<TD width=26%>Invalid attribute/option identifier</TD>
#	<TD width=52%>(DM) The value specified for the argument <I>Attribute</I> was not valid for the version of ODBC supported by the driver.
#	<P>(DM) The value specified for the argument <I>Attribute</I> was a read-only attribute.</P>
#	</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HYC00</TD>
#	<TD width=26%>Optional feature not implemented</TD>
#	<TD width=52%>The value specified for the argument <I>Attribute</I> was a valid ODBC connection or statement attribute for the version of ODBC supported by the driver but was not supported by the driver.</TD>
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
#	<TD width=52%>(DM) The driver associated with the <I>ConnectionHandle</I> does not support the function.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>IM009</TD>
#	<TD width=26%>Unable to load translation DLL</TD>
#	<TD width=52%>The driver was unable to load the translation DLL that was specified for the connection. This error can be returned only when <I>Attribute</I> is SQL_ATTR_TRANSLATE_LIB.</TD>
#	</TR>
#	</table></div>
#	<!--TS:-->
#	<P>When <I>Attribute</I> is a statement attribute, <B>SQLSetConnectAttr</B> can return any SQLSTATEs returned by <B>SQLSetStmtAttr</B>.</P>
#	
#	<P class="label"><B>Comments</B></P>
#	
#	<P>For general information about connection attributes, see "<A HREF="odbcconnection_attributes.htm">Connection Attributes</A>" in Chapter 6: Connecting to a Data Source or Driver.</P>
#	
#	<P>The currently defined attributes and the version of ODBC in which they were introduced are shown in the table later in this section; it is expected that more attributes will be defined to take advantage of different data sources. A range of attributes is reserved by ODBC; driver developers must reserve values for their own driver-specific use from X/Open.</P>
#	
#	<P class="indent"><b class="le">Note</b>&nbsp;&nbsp;&nbsp;The ability to set statement attributes at the connection level by calling <B>SQLSetConnectAttr</B> has been deprecated in ODBC 3<I>.x</I>. ODBC 3<I>.x</I> applications should never set statement attributes at the connection level. ODBC 3<I>.x</I> statement attributes cannot be set at the connection level, with the exception of the SQL_ATTR_METADATA_ID and SQL_ATTR_ASYNC_ENABLE attributes, which are both connection attributes and statement attributes and can be set at either the connection level or the statement level.</P>
#	
#	<P class="indent">ODBC 3<I>.x</I> drivers need only support this functionality if they should work with ODBC 2<I>.x</I> applications that set ODBC 2<I>.x</I> statement options at the connection level. For more information, see "<A HREF="odbcsqlsetconnectoption_mapping.htm">SQLSetConnectOption Mapping</A>" in Appendix G: Driver Guidelines for Backward Compatibility.</P>
#	
#	<P>An application can call <B>SQLSetConnectAttr</B> at any time between the time the connection is allocated and freed. All connection and statement attributes successfully set by the application for the connection persist until <B>SQLFreeHandle</B> is called on the connection. For example, if an application calls <B>SQLSetConnectAttr</B> before connecting to a data source, the attribute persists even if <B>SQLSetConnectAttr</B> fails in the driver when the application connects to the data source; if an application sets a driver-specific attribute, the attribute persists even if the application connects to a different driver on the connection.</P>
#	
#	<P>Some connection attributes can be set only before a connection has been made; others can be set only after a connection has been made. The following table indicates those connection attributes that must be set either before or after a connection has been made. <I>Either</I> indicates that the attribute can be set either before or after connection.</P>
#	<!--TS:--><div class="tablediv"><table>
#	
#	<TR VALIGN="top">
#	<TH width=50%>Attribute</TH>
#	<TH width=50%>Set before or after connection?</TH>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_ATTR_ACCESS_MODE</TD>
#	<TD width=50%>Either<SUP>[1]</SUP></TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_ATTR_ASYNC_ENABLE</TD>
#	<TD width=50%>Either<SUP>[2]</SUP></TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_ATTR_AUTOCOMMIT</TD>
#	<TD width=50%>Either</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_ATTR_CONNECTION_TIMEOUT</TD>
#	<TD width=50%>Either</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_ATTR_CURRENT_CATALOG</TD>
#	<TD width=50%>Either<SUP>[1]</SUP></TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_ATTR_LOGIN_TIMEOUT</TD>
#	<TD width=50%>Before</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_ATTR_METADATA_ID</TD>
#	<TD width=50%>Either</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_ATTR_ODBC_CURSORS</TD>
#	<TD width=50%>Before</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_ATTR_PACKET_SIZE</TD>
#	<TD width=50%>Before</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_ATTR_QUIET_MODE</TD>
#	<TD width=50%>Either</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_ATTR_TRACE</TD>
#	<TD width=50%>Either</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_ATTR_TRACEFILE</TD>
#	<TD width=50%>Either</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_ATTR_TRANSLATE_LIB</TD>
#	<TD width=50%>After</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_ATTR_TRANSLATE_OPTION</TD>
#	<TD width=50%>After</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=50%>SQL_ATTR_TXN_ISOLATION</TD>
#	<TD width=50%>Either<SUP>[3]</SUP></TD>
#	</TR>
#	</table></div>
#	
#	<P class="fineprint">[1]&nbsp;&nbsp;&nbsp;SQL_ATTR_ACCESS_MODE and SQL_ATTR_CURRENT_CATALOG can be set before or after connecting, depending on the driver. However, interoperable applications set them before connecting because some drivers do not support changing these after connecting.</p>
#	<P class="fineprint">[2]&nbsp;&nbsp;&nbsp;SQL_ATTR_ASYNC_ENABLE must be set before there is an active statement.</p>
#	<P class="fineprint">[3]&nbsp;&nbsp;&nbsp;SQL_ATTR_TXN_ISOLATION can be set only if there are no open transactions on the connection. Some connection attributes support substitution of a similar value if the data source does not support the value specified in *<I>ValuePtr</I>. In such cases, the driver returns SQL_SUCCESS_WITH_INFO and SQLSTATE 01S02 (Option value changed). For example, if <I>Attribute</I> is SQL_ATTR_PACKET_SIZE and *<I>ValuePtr</I> exceeds the maximum packet size, the driver substitutes the maximum size. To determine the substituted value, an application calls <B>SQLGetConnectAttr</B>.</p>
#	<P>The format of information set in the *<I>ValuePtr</I> buffer depends on the specified <I>Attribute</I>. <B>SQLSetConnectAttr</B> will accept attribute information in one of two different formats: a null-terminated character string or a 32-bit integer value. The format of each is noted in the attribute's description. Character strings pointed to by the <I>ValuePtr</I> argument of <B>SQLSetConnectAttr</B> have a length of <I>StringLength</I> bytes.</P>
#	
#	<P>The <I>StringLength</I> argument is ignored if the length is defined by the attribute, as is the case for all attributes introduced in ODBC 2<I>.x</I> or earlier.</P>
#	<!--TS:--><div class="tablediv"><table>
#	
#	<TR VALIGN="top">
#	<TH width=38%><I>Attribute</I></TH>
#	<TH width=62%><I>ValuePtr</I> contents</TH>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_ACCESS_MODE<BR>
#	(ODBC 1.0)</TD>
#	<TD width=62%>An SQLUINTEGER value. SQL_MODE_READ_ONLY is used by the driver or data source as an indicator that the connection is not required to support SQL statements that cause updates to occur. This mode can be used to optimize locking strategies, transaction management, or other areas as appropriate to the driver or data source. The driver is not required to prevent such statements from being submitted to the data source. The behavior of the driver and data source when asked to process SQL statements that are not read-only during a read-only connection is implementation-defined. SQL_MODE_READ_WRITE is the default.</TD>
#	</TR>
    SQL_ATTR_ACCESS_MODE => {
	type => q(SQLUINTEGER),
	ptr => 0,
	value => [ qw(SQL_MODE_READ_ONLY SQL_MODE_READ_WRITE) ],
	default => q(SQL_MODE_READ_WRITE),
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_ASYNC_ENABLE<BR>
#	(ODBC 3.0)</TD>
#	<TD width=62%>An SQLUINTEGER value that specifies whether a function called with a statement on the specified connection is executed asynchronously:
#	<P>SQL_ASYNC_ENABLE_OFF = Off (the default)<BR>
#	SQL_ASYNC_ENABLE_ON = On</P>
#	
#	<P>Setting SQL_ASYNC_ENABLE_ON enables asynchronous execution for all future statement handles allocated on this connection. It is driver-defined whether this enables asynchronous execution for existing statement handles associated with this connection. An error is returned if asynchronous execution is enabled while there is an active statement on the connection.</P>
#	
#	<P>This attribute can be set whether <B>SQLGetInfo</B> with the SQL_ASYNC_MODE information type returns SQL_AM_CONNECTION or SQL_AM_STATEMENT.</P>
#	
#	<P>After a function has been called asynchronously, only the original function, <B>SQLAllocHandle</B>, <B>SQLCancel</B>, <B>SQLGetDiagField</B>, or <B>SQLGetDiagRec </B>can be called on the statement or the connection associated with <I>StatementHandle</I>, until the original function returns a code other than SQL_STILL_EXECUTING. Any other function called on <I>StatementHandle</I> or the connection associated with <I>StatementHandle</I> returns SQL_ERROR with an SQLSTATE of HY010 (Function sequence error). Functions can be called on other statements. For more information, see "<A HREF="odbcasynchronous_execution.htm">Asynchronous Execution</A>" in Chapter 9: Executing Statements.</P>
#	
#	<P>In general, applications should execute functions asynchronously only on single-thread operating systems. On multithread operating systems, applications should execute functions on separate threads rather than executing them asynchronously on the same thread. Drivers that operate only on multithread operating systems do not need to support asynchronous execution.</P>
#	
#	<P>The following functions can be executed asynchronously:</P>
#	
#	<P><B>SQLBulkOperations<BR>
#	SQLColAttribute</B><BR>
#	<B>SQLColumnPrivileges</B><BR>
#	<B>SQLColumns</B><BR>
#	<B>SQLCopyDesc</B><BR>
#	<B>SQLDescribeCol</B><BR>
#	<B>SQLDescribeParam</B><BR>
#	<B>SQLExecDirect</B><BR>
#	<B>SQLExecute</B><BR>
#	<B>SQLFetch</B><BR>
#	<B>SQLFetchScroll</B><BR>
#	<B>SQLForeignKeys</B><BR>
#	<B>SQLGetData</B><BR>
#	<B>SQLGetDescField</B><SUP>[1]<BR>
#	</SUP><B>SQLGetDescRec</B><SUP>[1]</SUP><B><BR>
#	SQLGetDiagField</B><BR>
#	<B>SQLGetDiagRec<BR>
#	SQLGetTypeInfo</B><BR>
#	<B>SQLMoreResults</B><BR>
#	<B>SQLNumParams</B><BR>
#	<B>SQLNumResultCols</B><BR>
#	<B>SQLParamData</B><BR>
#	<B>SQLPrepare</B><BR>
#	<B>SQLPrimaryKeys</B><BR>
#	<B>SQLProcedureColumns</B><BR>
#	<B>SQLProcedures</B><BR>
#	<B>SQLPutData</B><BR>
#	<B>SQLSetPos</B><BR>
#	<B>SQLSpecialColumns</B><BR>
#	<B>SQLStatistics</B><BR>
#	<B>SQLTablePrivileges</B><BR>
#	<B>SQLTables</B></P>
#	</TD>
#	</TR>
    SQL_ASYNC_ENABLE_ON => {
	type => q(SQLUINTEGER),
	ptr => 0,
	value => [ qw(SQL_ASYNC_ENABLE_OFF SQL_ASYNC_ENABLE_ON) ],
	default => q(SQL_ASYNC_ENABLE_OFF),
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_AUTO_IPD<BR>
#	(ODBC 3.0)</TD>
#	<TD width=62%>A read-only SQLUINTEGER value that specifies whether automatic population of the IPD after a call to <B>SQLPrepare </B>is supported:
#	<P>SQL_TRUE = Automatic population of the IPD after a call to <B>SQLPrepare </B>is supported by the driver.</P>
#	
#	<P>SQL_FALSE = Automatic population of the IPD after a call to <B>SQLPrepare </B>is not supported by the driver. Servers that do not support prepared statements will not be able to populate the IPD automatically. </P>
#	
#	<P>If SQL_TRUE is returned for the SQL_ATTR_AUTO_IPD connection attribute, the statement attribute SQL_ATTR_ENABLE_AUTO_IPD can be set to turn automatic population of the IPD on or off. If SQL_ATTR_AUTO_IPD is SQL_FALSE, SQL_ATTR_ENABLE_AUTO_IPD cannot be set to SQL_TRUE. The default value of SQL_ATTR_ENABLE_AUTO_IPD is equal to the value of SQL_ATTR_AUTO_IPD.</P>
#	
#	<P>This connection attribute can be returned by <B>SQLGetConnectAttr</B> but cannot be set by <B>SQLSetConnectAttr</B>.</P>
#	</TD>
#	</TR>
    SQL_ATTR_AUTO_IPD => {
	type => q(SQLUINTEGER),
	ptr => 0,
	value => [ qw(SQL_FALSE SQL_TRUE) ],
	default => undef,
	mode => 'ro',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_AUTOCOMMIT<BR>
#	(ODBC 1.0)</TD>
#	<TD width=62%>An SQLUINTEGER value that specifies whether to use autocommit or manual-commit mode:
#	<P>SQL_AUTOCOMMIT_OFF = The driver uses manual-commit mode, and the application must explicitly commit or roll back transactions with <B>SQLEndTran</B>.</P>
#	
#	<P>SQL_AUTOCOMMIT_ON = The driver uses autocommit mode. Each statement is committed immediately after it is executed. This is the default. Any open transactions on the connection are committed when SQL_ATTR_AUTOCOMMIT is set to SQL_AUTOCOMMIT_ON to change from manual-commit mode to autocommit mode.</P>
#	
#	<P>For more information, see "<A HREF="odbccommit_mode.htm">Commit Mode</A>" in Chapter 14: Transactions.</P>
#	
#	<P class="indent"><b class="le">Important&nbsp;&nbsp;&nbsp;</b>Some data sources delete the access plans and close the cursors for all statements on a connection each time a statement is committed; autocommit mode can cause this to happen after each nonquery statement is executed or when the cursor is closed for a query. For more information, see the SQL_CURSOR_COMMIT_BEHAVIOR and SQL_CURSOR_ROLLBACK_BEHAVIOR information types in <A HREF="odbcsqlgetinfo.htm">SQLGetInfo</A> and "<A HREF="odbceffect_of_transactions_on_cursors_and_prepared_statements.htm">Effect of Transactions on Cursors and Prepared Statements</A>" in Chapter 14: Transactions.</P>
#	
#	<P>When a batch is executed in autocommit mode, two things are possible. The entire batch can be treated as an autocommitable unit, or each statement in a batch is treated as an autocommitable unit. Certain data sources can support both these behaviors and may provide a way of choosing one or the other. It is driver-defined whether a batch is treated as an autocommitable unit or whether each individual statement within the batch is autocommitable.</P>
#	</TD>
#	</TR>
    SQL_ATTR_AUTOCOMMIT => {
	type => q(SQLUINTEGER),
	ptr => 0,
	value => [ qw(SQL_AUTOCOMMIT_OFF SQL_AUTOCOMMIT_ON) ],
	default => q(SQL_AUTOCOMMIT_ON),
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_CONNECTION_DEAD
#	<P>(ODBC 3.5)</P>
#	</TD>
#	<TD width=62%>An SQLUINTERGER value that indicates the state of the connection. If SQL_CD_TRUE, the connection has been lost. If SQL_CD_FALSE, the connection is still active.</TD>
#	</TR>
    SQL_ATTR_CONNECTION_DEAD => {
	type => q(SQLUINTEGER),
	ptr => 0,
	value => [ qw(SQL_CD_FALSE SQL_CD_TRUE) ],
	default => undef,
	mode => 'ro',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_CONNECTION_TIMEOUT<BR>
#	(ODBC 3.0)</TD>
#	<TD width=62%>An SQLUINTEGER value corresponding to the number of seconds to wait for any request on the connection to complete before returning to the application. The driver should return SQLSTATE HYT00 (Timeout expired) anytime that it is possible to time out in a situation not associated with query execution or login.
#	<P>If <I>ValuePtr</I> is equal to 0 (the default), there is no timeout.</P>
#	</TD>
#	</TR>
    SQL_ATTR_CONNECTION_TIMEOUT => {
	type => q(SQLUINTEGER),
	ptr => 0,
	value => undef,
	default => 0,
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_CURRENT_CATALOG<BR>
#	(ODBC 2.0)</TD>
#	<TD width=62%>A character string containing the name of the catalog to be used by the data source. For example, in SQL Server, the catalog is a database, so the driver sends a <B>USE</B> <I>database</I> statement to the data source, where <I>database</I> is the database specified in *<I>ValuePtr</I>. For a single-tier driver, the catalog might be a directory, so the driver changes its current directory to the directory specified in *<I>ValuePtr</I>.</TD>
#	</TR>
    SQL_ATTR_CURRENT_CATALOG => {
	type => q(SQLCHAR),
	ptr => undef,
	value => undef,
	default => undef,
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_LOGIN_TIMEOUT<BR>
#	(ODBC 1.0)</TD>
#	<TD width=62%>An SQLUINTEGER value corresponding to the number of seconds to wait for a login request to complete before returning to the application. The default is driver-dependent. If <I>ValuePtr</I> is 0, the timeout is disabled and a connection attempt will wait indefinitely.
#	<P>If the specified timeout exceeds the maximum login timeout in the data source, the driver substitutes that value and returns SQLSTATE 01S02 (Option value changed).</P>
#	</TD>
#	</TR>
    SQL_ATTR_LOGIN_TIMEOUT => {
	type => q(SQLUINTEGER),
	ptr => 0,
	value => undef,
	default => 0,
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_METADATA_ID<BR>
#	(ODBC 3.0)</TD>
#	<TD width=62%>An SQLUINTEGER value that determines how the string arguments of catalog functions are treated.
#	<P>If SQL_TRUE, the string argument of catalog functions are treated as identifiers. The case is not significant. For nondelimited strings, the driver removes any trailing spaces and the string is folded to uppercase. For delimited strings, the driver removes any leading or trailing spaces and takes literally whatever is between the delimiters. If one of these arguments is set to a null pointer, the function returns SQL_ERROR and SQLSTATE HY009 (Invalid use of null pointer). </P>
#	
#	<P>If SQL_FALSE, the string arguments of catalog functions are not treated as identifiers. The case is significant. They can either contain a string search pattern or not, depending on the argument.</P>
#	
#	<P>The default value is SQL_FALSE.</P>
#	
#	<P>The <I>TableType</I> argument of <B>SQLTables</B>, which takes a list of values, is not affected by this attribute.</P>
#	
#	<P>SQL_ATTR_METADATA_ID can also be set on the statement level. (It is the only connection attribute that is also a statement attribute.)</P>
#	
#	<P>For more information, see "<A HREF="odbcarguments_in_catalog_functions.htm">Arguments in Catalog Functions</A>" in Chapter 7: Catalog Functions.</P>
#	</TD>
#	</TR>
    SQL_ATTR_METADATA_ID => {
	type => q(SQLUINTEGER),
	ptr => 0,
	value => [ qw(SQL_FALSE SQL_TRUE) ],
	default => q(SQL_FALSE),
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_ODBC_CURSORS<BR>
#	(ODBC 2.0)</TD>
#	<TD width=62%>An SQLUINTEGER value specifying how the Driver Manager uses the ODBC cursor library:
#	<P>SQL_CUR_USE_IF_NEEDED = The Driver Manager uses the ODBC cursor library only if it is needed. If the driver supports the SQL_FETCH_PRIOR option in <B>SQLFetchScroll</B>, the Driver Manager uses the scrolling capabilities of the driver. Otherwise, it uses the ODBC cursor library.</P>
#	
#	<P>SQL_CUR_USE_ODBC = The Driver Manager uses the ODBC cursor library.</P>
#	
#	<P>SQL_CUR_USE_DRIVER = The Driver Manager uses the scrolling capabilities of the driver. This is the default setting.</P>
#	
#	<P>For more information about the ODBC cursor library, see <A HREF="odbcodbc_cursor_library.htm">Appendix F: ODBC Cursor Library</A>.</P>
#	</TD>
#	</TR>
    SQL_ATTR_ODBC_CURSORS => {
	type => q(SQLUINTEGER),
	ptr => 0,
	value => [ qw(SQL_CUR_USE_IF_NEEDED SQL_CUR_USE_ODBC SQL_CUR_USE_DRIVER) ],
	default => q(SQL_CUR_USE_DRIVER),
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_PACKET_SIZE<BR>
#	(ODBC 2.0)</TD>
#	<TD width=62%>An SQLUINTEGER value specifying the network packet size in bytes.
#	<P class="indent"><B>Note</B>&nbsp;&nbsp;&nbsp;Many data sources either do not support this option or only can return but not set the network packet size.</P>
#	
#	<P>If the specified size exceeds the maximum packet size or is smaller than the minimum packet size, the driver substitutes that value and returns SQLSTATE 01S02 (Option value changed).</P>
#	
#	<P>If the application sets packet size after a connection has already been made, the driver will return SQLSTATE HY011 (Attribute cannot be set now).</P>
#	</TD>
#	</TR>
    SQL_ATTR_PACKET_SIZE => {
	type => q(SQLUINTEGER),
	ptr => 0,
	value => undef,
	default => undef,
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_QUIET_MODE<BR>
#	(ODBC 2.0)</TD>
#	<TD width=62%>A 32-bit window handle (<I>hwnd</I>).
#	<P>If the window handle is a null pointer, the driver does not display any dialog boxes.</P>
#	
#	<P>If the window handle is not a null pointer, it should be the parent window handle of the application. This is the default. The driver uses this handle to display dialog boxes.</P>
#	
#	<P class="indent"><b class="le">Note&nbsp;&nbsp;&nbsp;</b>The SQL_ATTR_QUIET_MODE connection attribute does not apply to dialog boxes displayed by <B>SQLDriverConnect</B>.</P>
#	</TD>
#	</TR>
    SQL_ATTR_QUIET_MODE => {
	type => q(SQLPOINTER),
	ptr => undef,
	value => undef,
	default => undef,
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_TRACE<BR>
#	(ODBC 1.0)</TD>
#	<TD width=62%>An SQLUINTEGER value telling the Driver Manager whether to perform tracing:
#	<P>SQL_OPT_TRACE_OFF = Tracing off (the default)</P>
#	
#	<P>SQL_OPT_TRACE_ON = Tracing on</P>
#	
#	<P>When tracing is on, the Driver Manager writes each ODBC function call to the trace file.</P>
#	
#	<P class="indent"><b class="le">Note&nbsp;&nbsp;&nbsp;</b>When tracing is on, the Driver Manager can return SQLSTATE IM013 (Trace file error) from any function.</P>
#	
#	<P>An application specifies a trace file with the SQL_ATTR_TRACEFILE option. If the file already exists, the Driver Manager appends to the file. Otherwise, it creates the file. If tracing is on and no trace file has been specified, the Driver Manager writes to the file SQL.LOG in the root directory. </P>
#	
#	<P>An application can set the variable <B>ODBCSharedTraceFlag</B> to enable tracing dynamically. Tracing is then enabled for all ODBC applications currently running. If an application turns tracing off, it is turned off only for that application.</P>
#	
#	<P>If the <B>Trace</B> keyword in the system information is set to 1 when an application calls <B>SQLAllocHandle</B> with a <I>HandleType</I> of SQL_HANDLE_ENV, tracing is enabled for all handles. It is enabled only for the application that called <B>SQLAllocHandle</B>.</P>
#	
#	<P>Calling <B>SQLSetConnectAttr</B> with an <I>Attribute</I> of SQL_ATTR_TRACE does not require that the <I>ConnectionHandle</I> argument be valid and will not return SQL_ERROR if <I>ConnectionHandle</I> is NULL. This attribute applies to all connections.</P>
#	</TD>
#	</TR>
    SQL_ATTR_TRACE => {
	type => q(SQLUINTEGER),
	ptr => 0,
	value => [ qw(SQL_OPT_TRACE_OFF SQL_OPT_TRACE_ON) ],
	default => q(SQL_OPT_TRACE_OFF),
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_TRACEFILE<BR>
#	(ODBC 1.0)</TD>
#	<TD width=62%>A null-terminated character string containing the name of the trace file.
#	<P>The default value of the SQL_ATTR_TRACEFILE attribute is specified with the <B>TraceFile</B> keyword in the system information. For more information, see "<A HREF="odbcodbc_subkey.htm">ODBC Subkey</A>" in Chapter 19: Configuring Data Sources.</P>
#	
#	<P>Calling <B>SQLSetConnectAttr</B> with an <I>Attribute</I> of SQL_ATTR_ TRACEFILE does not require the <I>ConnectionHandle</I> argument to be valid and will not return SQL_ERROR if <I>ConnectionHandle</I> is invalid. This attribute applies to all connections.</P>
#	</TD>
#	</TR>
    SQL_ATTR_TRACEFILE => {
	type => q(SQLCHAR),
	ptr => undef,
	value => undef,
	default => undef,
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_TRANSLATE_LIB<BR>
#	(ODBC 1.0)</TD>
#	<TD width=62%>A null-terminated character string containing the name of a library containing the functions <B>SQLDriverToDataSource</B> and <B>SQLDataSourceToDriver</B> that the driver accesses to perform tasks such as character set translation. This option may be specified only if the driver has connected to the data source. The setting of this attribute will persist across connections. For more information about translating data, see "<A HREF="odbctranslation_dlls.htm">Translation DLLs</A>" in Chapter 17: Programming Considerations, and <A HREF="odbctranslation_dll_function_reference.htm">Chapter 24: Translation DLL Function Reference</A>.</TD>
#	</TR>
    SQL_ATTR_TRANSLATE_LIB => {
	type => q(SQLCHAR),
	ptr => undef,
	value => undef,
	default => undef,
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_TRANSLATE_OPTION<BR>
#	(ODBC 1.0)</TD>
#	<TD width=62%>A 32-bit flag value that is passed to the translation DLL. This attribute can be specified only if the driver has connected to the data source. For information about translating data, see "<A HREF="odbctranslation_dlls.htm">Translation DLLs</A>" in Chapter 17: Programming Considerations.</TD>
#	</TR>
    SQL_ATTR_TRANSLATE_OPTION => {
	type => q(SQLUINTEGER),
	ptr => 0,
	value => undef,
	default => undef,
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_TXN_ISOLATION<BR>
#	(ODBC 1.0)</TD>
#	<TD width=62%>A 32-bit bitmask that sets the transaction isolation level for the current connection. An application must call <B>SQLEndTran</B> to commit or roll back all open transactions on a connection, before calling <B>SQLSetConnectAttr</B> with this option.
#	<P>The valid values for <I>ValuePtr</I> can be determined by calling <B>SQLGetInfo</B> with <I>InfoType</I> equal to SQL_TXN_ISOLATION_OPTIONS.</P>
#	
#	<P>For a description of transaction isolation levels, see the description of the SQL_DEFAULT_TXN_ISOLATION information type in <B>SQLGetInfo</B> and "<A HREF="odbctransaction_isolation_levels.htm">Transaction Isolation Levels</A>" in Chapter 14: Transactions.</P>
#	</TD>
#	</TR>
    SQL_ATTR_TXN_ISOLATION => {
	type => q(SQLUINTEGER),
	ptr => 0,
	value => undef,
	default => undef,
	mode => 'rw',
	order => ++$order,
    },
#	</table></div>
#	
#	<P class="fineprint">[1]&nbsp;&nbsp;&nbsp;These functions can be called asynchronously only if the descriptor is an implementation descriptor, not an application descriptor.</p>
#	<P class="label"><B>Code Example</B></P>
#	
#	<P>See <A HREF="odbcsqlconnect.htm">SQLConnect</A>.</P>
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
#	<TD width=48%>Allocating a handle</TD>
#	<TD width=52%><A HREF="odbcsqlallochandle.htm">SQLAllocHandle</A></TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=48%>Returning the setting of a connection <BR>
#	attribute</TD>
#	<TD width=52%><A HREF="odbcsqlgetconnectattr.htm">SQLGetConnectAttr</A></TD>
#	</TR>
#	</table></div>
#	<!--TS:--><H4><A NAME="feedback"></A></H4>
#	<SPAN id="SDKFeedB"></SPAN>
#	</div>
#	
#	</BODY>
#	</HTML>
};

#
# odbcsqlsetstmtattr.htm
#
my $attrStmt = {
#	<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 3.2 Final//EN">
#	<HTML DIR="LTR"><HEAD>
#	<META HTTP-EQUIV="Content-Type" Content="text/html; charset=Windows-1252">
#	<TITLE>SQLSetStmtAttr</TITLE>
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
#	<H1><A NAME="odbcsqlsetstmtattr"></A>SQLSetStmtAttr</H1>
#	
#	<P class="label"><B>Conformance</B></P>
#	
#	<P>Version Introduced: ODBC 3.0<BR>
#	Standards Compliance: ISO 92</P>
#	
#	<P class="label"><B>Summary</B></P>
#	
#	<P><B>SQLSetStmtAttr</B> sets attributes related to a statement.</P>
#	
#	<P class="indent"><b class="le">Note</b>&nbsp;&nbsp;&nbsp;For more information about what the Driver Manager maps this function to when an ODBC 3<I>.x</I> application is working with an ODBC 2<I>.x</I> driver, see "<A HREF="odbcmapping_replacement_functions_for_backward_compatibility_of_applications.htm">Mapping Replacement Functions for Backward Compatibility of Applications</A>" in Chapter 17: Programming Considerations.</P>
#	
#	<P class="label"><B>Syntax</B></P>
#	
#	<PRE class="syntax">SQLRETURN <B>SQLSetStmtAttr</B>(
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLHSTMT&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>StatementHandle</I>,
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLINTEGER&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>Attribute</I>,
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLPOINTER&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>ValuePtr</I>,
#	&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SQLINTEGER&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<I>StringLength</I>);</PRE>
#	
#	<P class="label"><B>Arguments</B>
#	
#	<DL>
#	<DT><I>StatementHandle</I></DT>
#	
#	<DD>[Input]<BR>
#	Statement handle.</dd>
#	
#	<DT><I>Attribute</I></DT>
#	
#	<DD>[Input]<BR>
#	Option to set, listed in "Comments."</dd>
#	
#	<DT><I>ValuePtr</I></DT>
#	
#	<DD>[Input]<BR>
#	Pointer to the value to be associated with <I>Attribute</I>. Depending on the value of <I>Attribute</I>, <I>ValuePtr</I> will be a 32-bit unsigned integer value or a pointer to a null-terminated character string, a binary buffer, or a driver-defined value. If the <I>Attribute</I> argument is a driver-specific value, <I>ValuePtr</I> may be a signed integer.</dd>
#	
#	<DT><I>StringLength</I></DT>
#	
#	<DD>[Input]<BR>
#	If <I>Attribute</I> is an ODBC-defined attribute and <I>ValuePtr</I> points to a character string or a binary buffer, this argument should be the length of *<I>ValuePtr</I>. If <I>Attribute</I> is an ODBC-defined attribute and <I>ValuePtr</I> is an integer, <I>StringLength</I> is ignored.
#	
#	<P>If <I>Attribute</I> is a driver-defined attribute, the application indicates the nature of the attribute to the Driver Manager by setting the <I>StringLength</I> argument. <I>StringLength</I> can have the following values:
#	</dd>
#	</DL>
#	
#	<UL type=disc>
#		<LI>If <I>ValuePtr</I> is a pointer to a character string, then <I>StringLength</I> is the length of the string or SQL_NTS.</li>
#	
#		<LI>If <I>ValuePtr</I> is a pointer to a binary buffer, then the application places the result of the SQL_LEN_BINARY_ATTR(<I>length</I>) macro in <I>StringLength</I>. This places a negative value in <I>StringLength</I>.</li>
#	
#		<LI>If <I>ValuePtr</I> is a pointer to a value other than a character string or a binary string, then <I>StringLength</I> should have the value SQL_IS_POINTER. </li>
#	
#		<LI>If <I>ValuePtr</I> contains a fixed-length value, then <I>StringLength</I> is either SQL_IS_INTEGER or SQL_IS_UINTEGER, as appropriate.</li>
#	</UL>
#	
#	<P class="label"><B>Returns</B></P>
#	
#	<P>SQL_SUCCESS, SQL_SUCCESS_WITH_INFO, SQL_ERROR, or SQL_INVALID_HANDLE.</P>
#	
#	<P class="label"><B>Diagnostics</B></P>
#	
#	<P>When <B>SQLSetStmtAttr</B> returns SQL_ERROR or SQL_SUCCESS_WITH_INFO, an associated SQLSTATE value may be obtained by calling <B>SQLGetDiagRec</B> with a <I>HandleType</I> of SQL_HANDLE_STMT and a <I>Handle</I> of <I>StatementHandle</I>. The following table lists the SQLSTATE values commonly returned by <B>SQLSetStmtAttr</B> and explains each one in the context of this function; the notation "(DM)" precedes the descriptions of SQLSTATEs returned by the Driver Manager. The return code associated with each SQLSTATE value is SQL_ERROR, unless noted otherwise.</P>
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
#	<TD width=52%>The driver did not support the value specified in <I>ValuePtr</I>, or the value specified in <I>ValuePtr</I> was invalid because of implementation working conditions, so the driver substituted a similar value. (<B>SQLGetStmtAttr</B> can be called to determine the temporarily substituted value.) The substitute value is valid for the <I>StatementHandle</I> until the cursor is closed, at which point the statement attribute reverts to its previous value. The statement attributes that can be changed are: 
#	<P>SQL_ ATTR_CONCURRENCY<BR>
#	SQL_ ATTR_CURSOR_TYPE<BR>
#	SQL_ ATTR_KEYSET_SIZE<BR>
#	SQL_ ATTR_MAX_LENGTH<BR>
#	SQL_ ATTR_MAX_ROWS<BR>
#	SQL_ ATTR_QUERY_TIMEOUT <BR>
#	SQL_ATTR_ROW_ARRAY_SIZE<BR>
#	SQL_ ATTR_SIMULATE_CURSOR </P>
#	
#	<P>(Function returns SQL_SUCCESS_WITH_INFO.)</P>
#	</TD>
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
#	<TD width=52%>The <I>Attribute</I> was SQL_ATTR_CONCURRENCY, SQL_ATTR_CURSOR_TYPE, SQL_ATTR_SIMULATE_CURSOR, or SQL_ATTR_USE_BOOKMARKS, and the cursor was open.</TD>
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
#	<TD width=22%>HY009</TD>
#	<TD width=26%>Invalid use of null pointer</TD>
#	<TD width=52%>The <I>Attribute</I> argument identified a statement attribute that required a string attribute, and the <I>ValuePtr </I>argument was a null pointer.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HY010</TD>
#	<TD width=26%>Function sequence error</TD>
#	<TD width=52%>(DM) An asynchronously executing function was called for the <I>StatementHandle</I> and was still executing when this function was called.
#	<P>(DM) <B>SQLExecute</B>, <B>SQLExecDirect</B>, <B>SQLBulkOperations</B>, or <B>SQLSetPos</B> was called for the <I>StatementHandle</I> and returned SQL_NEED_DATA. This function was called before data was sent for all data-at-execution parameters or columns.</P>
#	</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HY011</TD>
#	<TD width=26%>Attribute cannot be set now</TD>
#	<TD width=52%>The <I>Attribute</I> was SQL_ATTR_CONCURRENCY, SQL_ ATTR_CURSOR_TYPE, SQL_ ATTR_SIMULATE_CURSOR, or SQL_ ATTR_USE_BOOKMARKS, and the statement was prepared.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HY013</TD>
#	<TD width=26%>Memory management error</TD>
#	<TD width=52%>The function call could not be processed because the underlying memory objects could not be accessed, possibly because of low memory conditions.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HY017</TD>
#	<TD width=26%>Invalid use of an automatically allocated descriptor handle</TD>
#	<TD width=52%>(DM) The <I>Attribute</I> argument was SQL_ATTR_IMP_ROW_DESC or SQL_ATTR_IMP_PARAM_DESC.
#	<P>(DM) The <I>Attribute</I> argument was SQL_ATTR_APP_ROW_DESC or SQL_ATTR_APP_PARAM_DESC, and the value in <I>ValuePtr</I> was an implicitly allocated descriptor handle other than the handle originally allocated for the ARD or APD.</P>
#	</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HY024</TD>
#	<TD width=26%>Invalid attribute value</TD>
#	<TD width=52%>Given the specified <I>Attribute</I> value, an invalid value was specified in <I>ValuePtr</I>. (The Driver Manager returns this SQLSTATE only for connection and statement attributes that accept a discrete set of values, such as SQL_ATTR_ACCESS_MODE or SQL_ ATTR_ASYNC_ENABLE. For all other connection and statement attributes, the driver must verify the value specified in <I>ValuePtr</I>.)
#	<P>The <I>Attribute</I> argument was SQL_ATTR_APP_ROW_DESC or SQL_ATTR_APP_PARAM_DESC, and <I>ValuePtr</I> was an explicitly allocated descriptor handle that is not on the same connection as the <I>StatementHandle</I> argument.</P>
#	</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HY090</TD>
#	<TD width=26%>Invalid string or buffer length</TD>
#	<TD width=52%>(DM) <I>*ValuePtr</I> is a character string, and the <I>StringLength</I> argument was less than 0 but was not SQL_NTS.</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HY092</TD>
#	<TD width=26%>Invalid attribute/option identifier</TD>
#	<TD width=52%>(DM) The value specified for the argument <I>Attribute</I> was not valid for the version of ODBC supported by the driver.
#	<P>(DM) The value specified for the argument <I>Attribute</I> was a read-only attribute.</P>
#	</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=22%>HYC00</TD>
#	<TD width=26%>Optional feature not implemented</TD>
#	<TD width=52%>The value specified for the argument <I>Attribute</I> was a valid ODBC statement attribute for the version of ODBC supported by the driver but was not supported by the driver.
#	<P>The <I>Attribute</I> argument was SQL_ATTR_ASYNC_ENABLE, and a call to <B>SQLGetInfo</B> with an <I>InfoType</I> of SQL_ASYNC_MODE returns SQL_AM_CONNECTION.</P>
#	
#	<P>The <I>Attribute</I> argument was SQL_ATTR_ENABLE_AUTO_IPD, and the value of the connection attribute SQL_ATTR_AUTO_IPD was SQL_FALSE.</P>
#	</TD>
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
#	<P>Statement attributes for a statement remain in effect until they are changed by another call to <B>SQLSetStmtAttr</B> or until the statement is dropped by calling <B>SQLFreeHandle</B>. Calling <B>SQLFreeStmt</B> with the SQL_CLOSE, SQL_UNBIND, or SQL_RESET_PARAMS option does not reset statement attributes.</P>
#	
#	<P>Some statement attributes support substitution of a similar value if the data source does not support the value specified in <I>ValuePtr</I>. In such cases, the driver returns SQL_SUCCESS_WITH_INFO and SQLSTATE 01S02 (Option value changed). For example, if <I>Attribute</I> is SQL_ATTR_CONCURRENCY and <I>ValuePtr</I> is SQL_CONCUR_ROWVER, and if the data source does not support this, the driver substitutes SQL_CONCUR_VALUES and returns SQL_SUCCESS_WITH_INFO. To determine the substituted value, an application calls <B>SQLGetStmtAttr</B>.</P>
#	
#	<P>The format of information set with <I>ValuePtr</I> depends on the specified <I>Attribute</I>. <B>SQLSetStmtAttr</B> accepts attribute information in one of two different formats: a character string or a 32-bit integer value. The format of each is noted in the attribute's description. This format applies to the information returned for each attribute in <B>SQLGetStmtAttr</B>. Character strings pointed to by the <I>ValuePtr</I> argument of <B>SQLSetStmtAttr</B> have a length of <I>StringLength</I>.</P>
#	
#	<P class="indent"><b class="le">Note</b>&nbsp;&nbsp;&nbsp;The ability to set statement attributes at the connection level by calling <B>SQLSetConnectAttr</B> has been deprecated in ODBC 3<I>.x</I>. ODBC 3<I>.x</I> applications should never set statement attributes at the connection level. ODBC 3<I>.x</I> statement attributes cannot be set at the connection level, with the exception of the SQL_ATTR_METADATA_ID and SQL_ATTR_ASYNC_ENABLE attributes, which are both connection attributes and statement attributes, and can be set at either the connection level or the statement level.</P>
#	
#	<P class="indent">ODBC 3<I>.x</I> drivers need only support this functionality if they should work with ODBC 2<I>.x</I> applications that set ODBC 2<I>.x</I> statement options at the connection level. For more information, see "Setting Statement Options on the Connection Level" under "<A HREF="odbcsqlsetconnectoption_mapping.htm">SQLSetConnectOption Mapping</A>" in Appendix G: Driver Guidelines for Backward Compatibility.</P>
#	
#	<H1>Statement Attributes That Set Descriptor Fields</H1>
#	
#	<P>Many statement attributes correspond to a header field of a descriptor. Setting these attributes actually results in the setting of the descriptor fields. Setting fields by a call to <B>SQLSetStmtAttr</B> rather than to <B>SQLSetDescField</B> has the advantage that a descriptor handle does not have to be obtained for the function call.</P>
#	
#	<P class="indent"><b class="le">Caution</b>&nbsp;&nbsp;&nbsp;Calling <B>SQLSetStmtAttr</B> for one statement can affect other statements. This occurs when the APD or ARD associated with the statement is explicitly allocated and is also associated with other statements. Because <B>SQLSetStmtAttr</B> modifies the APD or ARD, the modifications apply to all statements with which this descriptor is associated. If this is not the required behavior, the application should dissociate this descriptor from the other statements (by calling <B>SQLSetStmtAttr</B> to set the SQL_ATTR_APP_ROW_DESC or SQL_ATTR_APP_PARAM_DESC field to a different descriptor handle) before calling <B>SQLSetStmtAttr</B> again.</P>
#	
#	<P>When a descriptor field is set as a result of the corresponding statement attribute being set, the field is set only for the applicable descriptors that are currently associated with the statement identified by the <I>StatementHandle</I> argument, and the attribute setting does not affect any descriptors that may be associated with that statement in the future. When a descriptor field that is also a statement attribute is set by a call to <B>SQLSetDescField</B>, the corresponding statement attribute is set. If an explicitly allocated descriptor is dissociated from a statement, a statement attribute that corresponds to a header field will revert to the value of the field in the implicitly allocated descriptor.</P>
#	
#	<P>When a statement is allocated (see <A HREF="odbcsqlallochandle.htm">SQLAllocHandle</A>), four descriptor handles are automatically allocated and associated with the statement. Explicitly allocated descriptor handles can be associated with the statement by calling <B>SQLAllocHandle</B> with an <I>fHandleType</I> of SQL_HANDLE_DESC to allocate a descriptor handle and then calling <B>SQLSetStmtAttr</B> to associate the descriptor handle with the statement. </P>
#	
#	<P>The statement attributes in the following table correspond to descriptor header fields.</P>
#	<!--TS:--><div class="tablediv"><table>
#	
#	<TR VALIGN="top">
#	<TH width=42%>Statement attribute</TH>
#	<TH width=45%>Header field</TH>
#	<TH width=13%>Desc.</TH>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=42%>SQL_ATTR_PARAM_BIND_OFFSET_PTR</TD>
#	<TD width=45%>SQL_DESC_BIND_OFFSET_PTR</TD>
#	<TD width=13%>APD</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=42%>SQL_ATTR_PARAM_BIND_TYPE</TD>
#	<TD width=45%>SQL_DESC_BIND_TYPE</TD>
#	<TD width=13%>APD</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=42%>SQL_ATTR_PARAM_OPERATION_PTR</TD>
#	<TD width=45%>SQL_DESC_ARRAY_STATUS_PTR</TD>
#	<TD width=13%>APD</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=42%>SQL_ATTR_PARAM_STATUS_PTR</TD>
#	<TD width=45%>SQL_DESC_ARRAY_STATUS_PTR</TD>
#	<TD width=13%>IPD</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=42%>SQL_ATTR_PARAMS_PROCESSED_PTR</TD>
#	<TD width=45%>SQL_DESC_ROWS_PROCESSED_PTR</TD>
#	<TD width=13%>IPD</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=42%>SQL_ATTR_PARAMSET_SIZE</TD>
#	<TD width=45%>SQL_DESC_ARRAY_SIZE</TD>
#	<TD width=13%>APD</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=42%>SQL_ATTR_ROW_ARRAY_SIZE</TD>
#	<TD width=45%>SQL_DESC_ARRAY_SIZE</TD>
#	<TD width=13%>ARD</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=42%>SQL_ATTR_ROW_BIND_OFFSET_PTR</TD>
#	<TD width=45%>SQL_DESC_BIND_OFFSET_PTR</TD>
#	<TD width=13%>ARD</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=42%>SQL_ATTR_ROW_BIND_TYPE</TD>
#	<TD width=45%>SQL_DESC_BIND_TYPE</TD>
#	<TD width=13%>ARD</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=42%>SQL_ATTR_ROW_OPERATION_PTR</TD>
#	<TD width=45%>SQL_DESC_ARRAY_STATUS_PTR</TD>
#	<TD width=13%>ARD</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=42%>SQL_ATTR_ROW_STATUS_PTR</TD>
#	<TD width=45%>SQL_DESC_ARRAY_STATUS_PTR</TD>
#	<TD width=13%>IRD</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=42%>SQL_ATTR_ROWS_FETCHED_PTR</TD>
#	<TD width=45%>SQL_DESC_ROWS_PROCESSED_PTR</TD>
#	<TD width=13%>IRD</TD>
#	</TR>
#	</table></div>
#	<!--TS:-->
#	<H1>Statement Attributes</H1>
#	
#	<P>The currently defined attributes and the version of ODBC in which they were introduced are shown in the following table; it is expected that more attributes will be defined by drivers to take advantage of different data sources. A range of attributes is reserved by ODBC; driver developers must reserve values for their own driver-specific use from X/Open. For more information, see "<A HREF="odbcdriver_specific_data_types__descriptor_types__information_types.htm">Driver-Specific Data Types, Descriptor Types, Information Types, Diagnostic Types, and Attributes</A>" in Chapter 17: Programming Considerations.</P>
#	<!--TS:--><div class="tablediv"><table>
#	
#	<TR VALIGN="top">
#	<TH width=38%>Attribute</TH>
#	<TH width=62%><I>ValuePtr</I> contents</TH>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_APP_PARAM_DESC<BR>
#	(ODBC 3.0)</TD>
#	<TD width=62%>The handle to the APD for subsequent calls to <B>SQLExecute</B> and <B>SQLExecDirect</B> on the statement handle. The initial value of this attribute is the descriptor implicitly allocated when the statement was initially allocated. If the value of this attribute is set to SQL_NULL_DESC or the handle originally allocated for the descriptor, an explicitly allocated APD handle that was previously associated with the statement handle is dissociated from it and the statement handle reverts to the implicitly allocated APD handle. 
#	<P>This attribute cannot be set to a descriptor handle that was implicitly allocated for another statement or to another descriptor handle that was implicitly set on the same statement; implicitly allocated descriptor handles cannot be associated with more than one statement or descriptor handle.</P>
#	</TD>
#	</TR>
    SQL_ATTR_APP_PARAM_DESC => {
	type => q(SQLPOINTER),
	ptr => undef,
	value => undef,
	default => undef,
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_APP_ROW_DESC<BR>
#	(ODBC 3.0)</TD>
#	<TD width=62%>The handle to the ARD for subsequent fetches on the statement handle. The initial value of this attribute is the descriptor implicitly allocated when the statement was initially allocated. If the value of this attribute is set to SQL_NULL_DESC or the handle originally allocated for the descriptor, an explicitly allocated ARD handle that was previously associated with the statement handle is dissociated from it and the statement handle reverts to the implicitly allocated ARD handle. 
#	<P>This attribute cannot be set to a descriptor handle that was implicitly allocated for another statement or to another descriptor handle that was implicitly set on the same statement; implicitly allocated descriptor handles cannot be associated with more than one statement or descriptor handle.</P>
#	</TD>
#	</TR>
    SQL_ATTR_APP_ROW_DESC => {
	type => q(SQLPOINTER),
	ptr => undef,
	value => undef,
	default => undef,
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_ASYNC_ENABLE<BR>
#	(ODBC 1.0)</TD>
#	<TD width=62%>An SQLUINTEGER value that specifies whether a function called with the specified statement is executed asynchronously:
#	<P>SQL_ASYNC_ENABLE_OFF = Off (the default)<BR>
#	SQL_ASYNC_ENABLE_ON = On</P>
#	
#	<P>Once a function has been called asynchronously, only the original function, <B>SQLCancel</B>, <B>SQLGetDiagField</B>, or <B>SQLGetDiagRec</B> can be called on the statement, and only the original function, <B>SQLAllocHandle </B>(with a <I>HandleType</I> of SQL_HANDLE_STMT), <B>SQLGetDiagField</B>, <B>SQLGetDiagRec</B>, or <B>SQLGetFunctions</B> can be called on the connection associated with the statement, until the original function returns a code other than SQL_STILL_EXECUTING. Any other function called on the statement or the connection associated with the statement returns SQL_ERROR with an SQLSTATE of HY010 (Function sequence error). Functions can be called on other statements. For more information, see "<A HREF="odbcasynchronous_execution.htm">Asynchronous Execution</A>" in Chapter 9: Executing Statements.</P>
#	
#	<P>For drivers with statement level asynchronous execution support, the statement attribute SQL_ATTR_ASYNC_ENABLE may be set. Its initial value is the same as the value of the connection level attribute with the same name at the time the statement handle was allocated. </P>
#	
#	<P>For drivers with connection-level, asynchronous-execution support, the statement attribute SQL_ATTR_ASYNC_ENABLE is read-only. Its value is the same as the value of the connection level attribute with the same name at the time the statement handle was allocated. Calling <B>SQLSetStmtAttr</B> to set SQL_ATTR_ASYNC_ENABLE when the SQL_ASYNC_MODE <I>InfoType</I> returns SQL_AM_CONNECTION returns SQLSTATE HYC00 (Optional feature not implemented). (See <B>SQLSetConnectAttr</B> for more information.)</P>
#	
#	<P>As a standard practice, applications should execute functions asynchronously only on single-thread operating systems. On multithread operating systems, applications should execute functions on separate threads rather than executing them asynchronously on the same thread. No functionality is lost if drivers that operate only on multithread operating systems do not need to support asynchronous execution. </P>
#	
#	<P>The following functions can be executed asynchronously:</P>
#	
#	<P><B>SQLBulkOperations<BR>
#	SQLColAttribute</B><BR>
#	<B>SQLColumnPrivileges</B><BR>
#	<B>SQLColumns</B><BR>
#	<B>SQLCopyDesc</B><BR>
#	<B>SQLDescribeCol</B><BR>
#	<B>SQLDescribeParam</B><BR>
#	<B>SQLExecDirect</B><BR>
#	<B>SQLExecute</B><BR>
#	<B>SQLFetch</B><BR>
#	<B>SQLFetchScroll</B><BR>
#	<B>SQLForeignKeys</B><BR>
#	<B>SQLGetData</B><BR>
#	<B>SQLGetDescField</B><SUP>[1]</SUP><BR>
#	<B>SQLGetDescRec</B><SUP>[1]</SUP><B><BR>
#	SQLGetDiagField</B><BR>
#	<B>SQLGetDiagRec<BR>
#	SQLGetTypeInfo</B><BR>
#	<B>SQLMoreResults</B><BR>
#	<B>SQLNumParams</B><BR>
#	<B>SQLNumResultCols</B><BR>
#	<B>SQLParamData</B><BR>
#	<B>SQLPrepare</B><BR>
#	<B>SQLPrimaryKeys</B><BR>
#	<B>SQLProcedureColumns</B><BR>
#	<B>SQLProcedures</B><BR>
#	<B>SQLPutData</B><BR>
#	<B>SQLSetPos</B><BR>
#	<B>SQLSpecialColumns</B><BR>
#	<B>SQLStatistics</B><BR>
#	<B>SQLTablePrivileges</B><BR>
#	<B>SQLTables</B></P>
#	</TD>
#	</TR>
    SQL_ATTR_ASYNC_ENABLE => {
	type => q(SQLUINTEGER),
	ptr => 0,
	value => undef,
	default => undef,
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_CONCURRENCY<BR>
#	(ODBC 2.0)</TD>
#	<TD width=62%>An SQLUINTEGER value that specifies the cursor concurrency:
#	<P>SQL_CONCUR_READ_ONLY = Cursor is read-only. No updates are allowed.</P>
#	
#	<P>SQL_CONCUR_LOCK = Cursor uses the lowest level of locking sufficient to ensure that the row can be updated.</P>
#	
#	<P>SQL_CONCUR_ROWVER = Cursor uses optimistic concurrency control, comparing row versions such as SQLBase ROWID or Sybase TIMESTAMP.</P>
#	
#	<P>SQL_CONCUR_VALUES = Cursor uses optimistic concurrency control, comparing values.</P>
#	
#	<P>The default value for SQL_ATTR_CONCURRENCY is SQL_CONCUR_READ_ONLY.</P>
#	
#	<P>This attribute cannot be specified for an open cursor. For more information, see "<A HREF="odbcconcurrency_types.htm">Concurrency Types</A>" in Chapter 14: Transactions.</P>
#	
#	<P>If the SQL_ATTR_CURSOR_TYPE <I>Attribute</I> is changed to a type that does not support the current value of SQL_ATTR_CONCURRENCY, the value of SQL_ATTR_CONCURRENCY will be changed at execution time, and a warning issued when <B>SQLExecDirect</B> or <B>SQLPrepare</B> is called.</P>
#	
#	<P>If the driver supports the <B>SELECT FOR UPDATE</B> statement and such a statement is executed while the value of SQL_ATTR_CONCURRENCY is set to SQL_CONCUR_READ_ONLY, an error will be returned. If the value of SQL_ATTR_CONCURRENCY is changed to a value that the driver supports for some value of SQL_ATTR_CURSOR_TYPE but not for the current value of SQL_ATTR_CURSOR_TYPE, the value of SQL_ATTR_CURSOR_TYPE will be changed at execution time and SQLSTATE 01S02 (Option value changed) is issued when <B>SQLExecDirect</B> or <B>SQLPrepare</B> is called.</P>
#	
#	<P>If the specified concurrency is not supported by the data source, the driver substitutes a different concurrency and returns SQLSTATE 01S02 (Option value changed). For SQL_CONCUR_VALUES, the driver substitutes SQL_CONCUR_ROWVER, and vice versa. For SQL_CONCUR_LOCK, the driver substitutes, in order, SQL_CONCUR_ROWVER or SQL_CONCUR_VALUES. The validity of the substituted value is not checked until execution time.</P>
#	
#	<P>For more information about the relationship between SQL_ATTR_CONCURRENCY and the other cursor attributes, see "<A HREF="odbccursor_characteristics_and_cursor_type.htm">Cursor Characteristics and Cursor Type</A>" in Chapter 11: Retrieving Results (Advanced).</P>
#	</TD>
#	</TR>
    SQL_ATTR_CONCURRENCY => {
	type => q(SQLUINTEGER),
	ptr => undef,
	value => [ qw(SQL_CONCUR_READ_ONLY SQL_CONCUR_LOCK SQL_CONCUR_ROWVER SQL_CONCUR_ROWVER) ],
	default => q(SQL_CONCUR_READ_ONLY),
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_CURSOR_SCROLLABLE<BR>
#	(ODBC 3.0)</TD>
#	<TD width=62%>An SQLUINTEGER value that specifies the level of support that the application requires. Setting this attribute affects subsequent calls to <B>SQLExecDirect</B> and <B>SQLExecute</B>.
#	<P>SQL_NONSCROLLABLE = Scrollable cursors are not required on the statement handle. If the application calls <B>SQLFetchScroll</B> on this handle, the only valid value of <I>FetchOrientation</I> is SQL_FETCH_NEXT. This is the default.</P>
#	
#	<P>SQL_SCROLLABLE = Scrollable cursors are required on the statement handle. When calling <B>SQLFetchScroll</B>, the application may specify any valid value of <I>FetchOrientation</I>, achieving cursor positioning in modes other than the sequential mode. </P>
#	
#	<P>For more information about scrollable cursors, see "<A HREF="odbcscrollable_cursors.htm">Scrollable Cursors</A>" in Chapter 11: Retrieving Results (Advanced). For more information about the relationship between SQL_ATTR_CURSOR_SCROLLABLE and the other cursor attributes, see "<A HREF="odbccursor_characteristics_and_cursor_type.htm">Cursor Characteristics and Cursor Type</A>" in Chapter 11: Retrieving Results (Advanced).</P>
#	</TD>
#	</TR>
    SQL_ATTR_CURSOR_SCROLLABLE => {
	type => q(SQLUINTEGER),
	ptr => 0,
	value => [ qw(SQL_NONSCROLLABLE SQL_SCROLLABLE) ],
	default => q(SQL_NONSCROLLABLE),
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_CURSOR_SENSITIVITY<BR>
#	(ODBC 3.0)</TD>
#	<TD width=62%>An SQLUINTEGER value that specifies whether cursors on the statement handle make visible the changes made to a result set by another cursor. Setting this attribute affects subsequent calls to <B>SQLExecDirect</B> and <B>SQLExecute</B>. An application can read back the value of this attribute to obtain its initial state or its state as most recently set by the application.
#	<P>SQL_UNSPECIFIED = It is unspecified what the cursor type is and whether cursors on the statement handle make visible the changes made to a result set by another cursor. Cursors on the statement handle may make visible none, some, or all such changes. This is the default.</P>
#	
#	<P>SQL_INSENSITIVE = All cursors on the statement handle show the result set without reflecting any changes made to it by any other cursor. Insensitive cursors are read-only. This corresponds to a static cursor, which has a concurrency that is read-only.</P>
#	
#	<P>SQL_SENSITIVE = All cursors on the statement handle make visible all changes made to a result set by another cursor. </P>
#	
#	<P>For more information about the relationship between SQL_ATTR_CURSOR_SENSITIVITY and the other cursor attributes, see "<A HREF="odbccursor_characteristics_and_cursor_type.htm">Cursor Characteristics and Cursor Type</A>" in Chapter 11: Retrieving Results (Advanced).</P>
#	</TD>
#	</TR>
    SQL_ATTR_CURSOR_SENSITIVITY => {
	type => q(SQLUINTEGER),
	ptr => 0,
	value => [ qw(SQL_UNSPECIFIED SQL_INSENSITIVE SQL_SENSITIVE) ],
	default => q(SQL_UNSPECIFIED),
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_CURSOR_TYPE<BR>
#	(ODBC 2.0)</TD>
#	<TD width=62%>An SQLUINTEGER value that specifies the cursor type:
#	<P>SQL_CURSOR_FORWARD_ONLY = The cursor only scrolls forward.</P>
#	
#	<P>SQL_CURSOR_STATIC = The data in the result set is static.</P>
#	
#	<P>SQL_CURSOR_KEYSET_DRIVEN = The driver saves and uses the keys for the number of rows specified in the SQL_ATTR_KEYSET_SIZE statement attribute.</P>
#	
#	<P>SQL_CURSOR_DYNAMIC = The driver saves and uses only the keys for the rows in the rowset.</P>
#	
#	<P>The default value is SQL_CURSOR_FORWARD_ONLY. This attribute cannot be specified after the SQL statement has been prepared.</P>
#	
#	<P>If the specified cursor type is not supported by the data source, the driver substitutes a different cursor type and returns SQLSTATE 01S02 (Option value changed). For a mixed or dynamic cursor, the driver substitutes, in order, a keyset-driven or static cursor. For a keyset-driven cursor, the driver substitutes a static cursor. </P>
#	
#	<P>For more information about scrollable cursor types, see "<A HREF="odbcscrollable_cursor_types.htm">Scrollable Cursor Types</A>" in Chapter 11: Retrieving Results (Advanced). For more information about the relationship between SQL_ATTR_CURSOR_TYPE and the other cursor attributes, see "<A HREF="odbccursor_characteristics_and_cursor_type.htm">Cursor Characteristics and Cursor Type</A>" in Chapter 11: Retrieving Results (Advanced).</P>
#	</TD>
#	</TR>
    SQL_ATTR_CURSOR_TYPE => {
	type => q(SQLUINTEGER),
	ptr => 0,
	value => [ qw(SQL_CURSOR_FORWARD_ONLY SQL_CURSOR_STATIC SQL_CURSOR_KEYSET_DRIVEN SQL_CURSOR_DYNAMIC) ],
	default => q(SQL_CURSOR_FORWARD_ONLY),
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_ENABLE_AUTO_IPD<BR>
#	(ODBC 3.0)</TD>
#	<TD width=62%>An SQLUINTEGER value that specifies whether automatic population of the IPD is performed:
#	<P>SQL_TRUE = Turns on automatic population of the IPD after a call to <B>SQLPrepare</B>. SQL_FALSE = Turns off automatic population of the IPD after a call to <B>SQLPrepare</B>. (An application can still obtain IPD field information by calling <B>SQLDescribeParam</B>, if supported.) The default value of the statement attribute SQL_ATTR_ENABLE_AUTO_IPD is SQL_FALSE. For more information, see "<A HREF="odbcautomatic_population_of_the_ipd.htm">Automatic Population of the IPD</A>" in Chapter 13: Descriptors.</P>
#	</TD>
#	</TR>
    SQL_ATTR_ENABLE_AUTO_IPD => {
	type => q(SQLUINTEGER),
	ptr => 0,
	value => [ qw(SQL_FALSE SQL_TRUE) ],
	default => q(SQL_FALSE),
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_FETCH_BOOKMARK_PTR<BR>
#	(ODBC 3.0)</TD>
#	<TD width=62%>A pointer that points to a binary bookmark value. When <B>SQLFetchScroll</B> is called with <I>fFetchOrientation</I> equal to SQL_FETCH_BOOKMARK, the driver picks up the bookmark value from this field. This field defaults to a null pointer. For more information, see "<A HREF="odbcscrolling_by_bookmark.htm">Scrolling by Bookmark</A>" in Chapter 11: Retrieving Results (Advanced).
#	<P>The value pointed to by this field is not used for delete by bookmark, update by bookmark, or fetch by bookmark operations in <B>SQLBulkOperations</B>, which use bookmarks cached in rowset buffers.</P>
#	</TD>
#	</TR>
    SQL_ATTR_FETCH_BOOKMARK_PTR => {
	type => q(SQLPOINTER),
	ptr => undef,
	value => undef,
	default => undef,
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_IMP_PARAM_DESC<BR>
#	(ODBC 3.0)</TD>
#	<TD width=62%>The handle to the IPD. The value of this attribute is the descriptor allocated when the statement was initially allocated. The application cannot set this attribute. 
#	<P>This attribute can be retrieved by a call to <B>SQLGetStmtAttr</B> but not set by a call to <B>SQLSetStmtAttr</B>.</P>
#	</TD>
#	</TR>
    SQL_ATTR_IMP_PARAM_DESC => {
	type => q(SQLPOINTER),
	ptr => undef,
	value => undef,
	default => undef,
	mode => 'ro',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_IMP_ROW_DESC<BR>
#	(ODBC 3.0)</TD>
#	<TD width=62%>The handle to the IRD. The value of this attribute is the descriptor allocated when the statement was initially allocated. The application cannot set this attribute. 
#	<P>This attribute can be retrieved by a call to <B>SQLGetStmtAttr</B> but not set by a call to <B>SQLSetStmtAttr</B>.</P>
#	</TD>
#	</TR>
    SQL_ATTR_IMP_ROW_DESC => {
	type => q(SQLPOINTER),
	ptr => undef,
	value => undef,
	default => undef,
	mode => 'ro',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_KEYSET_SIZE<BR>
#	(ODBC 2.0)</TD>
#	<TD width=62%>An SQLUINTEGER that specifies the number of rows in the keyset for a keyset-driven cursor. If the keyset size is 0 (the default), the cursor is fully keyset-driven. If the keyset size is greater than 0, the cursor is mixed (keyset-driven within the keyset and dynamic outside of the keyset). The default keyset size is 0. For more information about keyset-driven cursors, see "<A HREF="odbckeyset_driven_cursors.htm">Keyset-Driven Cursors</A>" in Chapter 11: Retrieving Results (Advanced).
#	<P>If the specified size exceeds the maximum keyset size, the driver substitutes that size and returns SQLSTATE 01S02 (Option value changed).</P>
#	
#	<P><B>SQLFetch</B> or <B>SQLFetchScroll</B> returns an error if the keyset size is greater than 0 and less than the rowset size.</P>
#	</TD>
#	</TR>
    SQL_ATTR_KEYSET_SIZE => {
	type => q(SQLUINTEGER),
	ptr => undef,
	value => undef,
	default => undef,
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_MAX_LENGTH<BR>
#	(ODBC 1.0)</TD>
#	<TD width=62%>An SQLUINTEGER value that specifies the maximum amount of data that the driver returns from a character or binary column. If <I>ValuePtr</I> is less than the length of the available data, <B>SQLFetch</B> or <B>SQLGetData</B> truncates the data and returns SQL_SUCCESS. If <I>ValuePtr</I> is 0 (the default), the driver attempts to return all available data.
#	<P>If the specified length is less than the minimum amount of data that the data source can return or greater than the maximum amount of data that the data source can return, the driver substitutes that value and returns SQLSTATE 01S02 (Option value changed).</P>
#	
#	<P>The value of this attribute can be set on an open cursor; however, the setting might not take effect immediately, in which case the driver will return SQLSTATE 01S02 (Option value changed) and reset the attribute to its original value.</P>
#	
#	<P>This attribute is intended to reduce network traffic and should be supported only when the data source (as opposed to the driver) in a multiple-tier driver can implement it. This mechanism should not be used by applications to truncate data; to truncate data received, an application should specify the maximum buffer length in the <I>BufferLength </I>argument in <B>SQLBindCol</B> or <B>SQLGetData</B>.</P>
#	</TD>
#	</TR>
    SQL_ATTR_MAX_LENGTH => {
	type => q(SQLUINTEGER),
	ptr => undef,
	value => undef,
	default => 0,
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_MAX_ROWS<BR>
#	(ODBC 1.0)</TD>
#	<TD width=62%>An SQLUINTEGER value corresponding to the maximum number of rows to return to the application for a <B>SELECT</B> statement. If *<I>ValuePtr</I> equals 0 (the default), the driver returns all rows.
#	<P>This attribute is intended to reduce network traffic. Conceptually, it is applied when the result set is created and limits the result set to the first <I>ValuePtr</I> rows. If the number of rows in the result set is greater than <I>ValuePtr</I>, the result set is truncated. </P>
#	
#	<P>SQL_ATTR_MAX_ROWS applies to all result sets on the <I>Statement</I>, including those returned by catalog functions. SQL_ATTR_MAX_ROWS establishes a maximum for the value of the cursor row count.</P>
#	
#	<P>A driver should not emulate SQL_ATTR_MAX_ROWS behavior for <B>SQLFetch</B> or <B>SQLFetchScroll</B> (if result set size limitations cannot be implemented at the data source) if it cannot guarantee that SQL_ATTR_MAX_ROWS will be implemented properly.</P>
#	
#	<P>It is driver-defined whether SQL_ATTR_MAX_ROWS applies to statements other than SELECT statements (such as catalog functions). </P>
#	
#	<P>The value of this attribute can be set on an open cursor; however, the setting might not take effect immediately, in which case the driver will return SQLSTATE 01S02 (Option value changed) and reset the attribute to its original value.</P>
#	</TD>
#	</TR>
    SQL_ATTR_MAX_ROWS => {
	type => q(SQLUINTEGER),
	ptr => undef,
	value => undef,
	default => undef,
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_METADATA_ID<BR>
#	(ODBC 3.0)</TD>
#	<TD width=62%>An SQLUINTEGER value that determines how the string arguments of catalog functions are treated.
#	<P>If SQL_TRUE, the string argument of catalog functions are treated as identifiers. The case is not significant. For nondelimited strings, the driver removes any trailing spaces and the string is folded to uppercase. For delimited strings, the driver removes any leading or trailing spaces and takes whatever is between the delimiters literally. If one of these arguments is set to a null pointer, the function returns SQL_ERROR and SQLSTATE HY009 (Invalid use of null pointer). </P>
#	
#	<P>If SQL_FALSE, the string arguments of catalog functions are not treated as identifiers. The case is significant. They can either contain a string search pattern or not, depending on the argument.</P>
#	
#	<P>The default value is SQL_FALSE.</P>
#	
#	<P>The <I>TableType</I> argument of <B>SQLTables</B>, which takes a list of values, is not affected by this attribute.</P>
#	
#	<P>SQL_ATTR_METADATA_ID can also be set on the connection level. (It and SQL_ATTR_ASYNC_ENABLE are the only statement attributes that are also connection attributes.)</P>
#	
#	<P>For more information, see "<A HREF="odbcarguments_in_catalog_functions.htm">Arguments in Catalog Functions</A>" in Chapter 7: Catalog Functions.</P>
#	</TD>
#	</TR>
    SQL_ATTR_METADATA_ID => {
	type => q(SQLUINTEGER),
	ptr => 0,
	value => [ qw(SQL_FALSE SQL_TRUE) ],
	default => q(SQL_FALSE),
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_NOSCAN<BR>
#	(ODBC 1.0)</TD>
#	<TD width=62%>An SQLUINTEGER value that indicates whether the driver should scan SQL strings for escape sequences:
#	<P>SQL_NOSCAN_OFF = The driver scans SQL strings for escape sequences (the default).</P>
#	
#	<P>SQL_NOSCAN_ON = The driver does not scan SQL strings for escape sequences. Instead, the driver sends the statement directly to the data source.</P>
#	
#	<P>For more information, see "<A HREF="odbcescape_sequences_in_odbc.htm">Escape Sequences in ODBC</A>" in Chapter 8: SQL Statements.</P>
#	</TD>
#	</TR>
    SQL_ATTR_NOSCAN => {
	type => q(SQLUINTEGER),
	ptr => 0,
	value => [ qw(SQL_NOSCAN_OFF SQL_NOSCAN_ON) ],
	default => undef,
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_PARAM_BIND_OFFSET_PTR<BR>
#	(ODBC 3.0)</TD>
#	<TD width=62%>An SQLUINTEGER * value that points to an offset added to pointers to change binding of dynamic parameters. If this field is non-null, the driver dereferences the pointer, adds the dereferenced value to each of the deferred fields in the descriptor record (SQL_DESC_DATA_PTR, SQL_DESC_INDICATOR_PTR, and SQL_DESC_OCTET_LENGTH_PTR), and uses the new pointer values when binding. It is set to null by default.
#	<P>The bind offset is always added directly to the SQL_DESC_DATA_PTR, SQL_DESC_INDICATOR_PTR, and SQL_DESC_OCTET_LENGTH_PTR fields. If the offset is changed to a different value, the new value is still added directly to the value in the descriptor field. The new offset is not added to the field value plus any earlier offsets.</P>
#	</TD>
#	</TR>
    SQL_ATTR_PARAM_BIND_OFFSET_PTR => {
	type => q(SQLUINTEGER),
	ptr => 1,
	value => undef,
	default => undef,
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_PARAM_BIND_OFFSET_PTR<BR>
#	(ODBC 3.0) (<I>continued</I>)</TD>
#	<TD width=62%>For more information, see "<A HREF="odbcparameter_binding_offsets.htm">Parameter Binding Offsets</A>" in Chapter 9: Executing Statements.
#	<P>Setting this statement attribute sets the SQL_DESC_BIND_OFFSET_PTR field in the APD header.</P>
#	</TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_PARAM_BIND_TYPE<BR>
#	(ODBC 3.0)</TD>
#	<TD width=62%>An SQLUINTEGER value that indicates the binding orientation to be used for dynamic parameters. 
#	<P>This field is set to SQL_PARAM_BIND_BY_COLUMN (the default) to select column-wise binding. </P>
#	
#	<P>To select row-wise binding, this field is set to the length of the structure or an instance of a buffer that will be bound to a set of dynamic parameters. This length must include space for all of the bound parameters and any padding of the structure or buffer to ensure that when the address of a bound parameter is incremented with the specified length, the result will point to the beginning of the same parameter in the next set of parameters. When using the <I>sizeof</I> operator in ANSI C, this behavior is guaranteed.</P>
#	
#	<P>For more information, see "<A HREF="odbcbinding_arrays_of_parameters.htm">Binding Arrays of Parameters</A>" in Chapter 9: Executing Statements.</P>
#	
#	<P>Setting this statement attribute sets the SQL_DESC_ BIND_TYPE field in the APD header.</P>
#	</TD>
#	</TR>
    SQL_ATTR_PARAM_BIND_TYPE => {
	type => q(SQLUINTEGER),
	ptr => 0,
	value => undef,
	default => undef,
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_PARAM_OPERATION_PTR<BR>
#	(ODBC 3.0)</TD>
#	<TD width=62%>An SQLUSMALLINT * value that points to an array of SQLUSMALLINT values used to ignore a parameter during execution of an SQL statement. Each value is set to either SQL_PARAM_PROCEED (for the parameter to be executed) or SQL_PARAM_IGNORE (for the parameter to be ignored). 
#	<P>A set of parameters can be ignored during processing by setting the status value in the array pointed to by SQL_DESC_ARRAY_STATUS_PTR in the APD to SQL_PARAM_IGNORE. A set of parameters is processed if its status value is set to SQL_PARAM_PROCEED or if no elements in the array are set.</P>
#	
#	<P>This statement attribute can be set to a null pointer, in which case the driver does not return parameter status values. This attribute can be set at any time, but the new value is not used until the next time <B>SQLExecDirect</B> or <B>SQLExecute</B> is called.</P>
#	
#	<P>For more information, see "<A HREF="odbcusing_arrays_of_parameters.htm">Using Arrays of Parameters</A>" in Chapter 9: Executing Statements.</P>
#	
#	<P>Setting this statement attribute sets the SQL_DESC_ARRAY_STATUS_PTR field in the APD header.</P>
#	</TD>
#	</TR>
    SQL_ATTR_PARAM_OPERATION_PTR => {
	type => q(SQLUSMALLINT),
	ptr => 1,
	value => undef,
	default => undef,
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_PARAM_STATUS_PTR<BR>
#	(ODBC 3.0)</TD>
#	<TD width=62%>An SQLUSMALLINT * value that points to an array of SQLUSMALLINT values containing status information for each row of parameter values after a call to <B>SQLExecute</B> or <B>SQLExecDirect</B>. This field is required only if PARAMSET_SIZE is greater than 1. 
#	<P>The status values can contain the following values:</P>
#	
#	<P>SQL_PARAM_SUCCESS: The SQL statement was successfully executed for this set of parameters.</P>
#	
#	<P>SQL_PARAM_SUCCESS_WITH_INFO: The SQL statement was successfully executed for this set of parameters; however, warning information is available in the diagnostics data structure.</P>
#	
#	<P>SQL_PARAM_ERROR: There was an error in processing this set of parameters. Additional error information is available in the diagnostics data structure.</P>
#	
#	<P>SQL_PARAM_UNUSED: This parameter set was unused, possibly due to the fact that some previous parameter set caused an error that aborted further processing, or because SQL_PARAM_IGNORE was set for that set of parameters in the array specified by the SQL_ATTR_PARAM_OPERATION_PTR.</P>
#	
#	<P>SQL_PARAM_DIAG_UNAVAILABLE: The driver treats arrays of parameters as a monolithic unit and so does not generate this level of error information. </P>
#	
#	<P>This statement attribute can be set to a null pointer, in which case the driver does not return parameter status values. This attribute can be set at any time, but the new value is not used until the next time <B>SQLExecute</B> or <B>SQLExecDirect</B> is called. Note that setting this attribute can affect the output parameter behavior implemented by the driver.</P>
#	
#	<P>For more information, see "<A HREF="odbcusing_arrays_of_parameters.htm">Using Arrays of Parameters</A>" in Chapter 9: Executing Statements.</P>
#	
#	<P>Setting this statement attribute sets the SQL_DESC_ARRAY_STATUS_PTR field in the IPD header.</P>
#	</TD>
#	</TR>
    SQL_ATTR_PARAM_STATUS_PTR => {
	type => q(SQLUSMALLINT),
	ptr => 1,
	value => undef,
	default => undef,
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_PARAMS_PROCESSED_PTR<BR>
#	(ODBC 3.0)</TD>
#	<TD width=62%>An SQLUINTEGER * record field that points to a buffer in which to return the number of sets of parameters that have been processed, including error sets. No number will be returned if this is a null pointer.
#	<P>Setting this statement attribute sets the SQL_DESC_ROWS_PROCESSED_PTR field in the IPD header.</P>
#	
#	<P>If the call to <B>SQLExecDirect</B> or <B>SQLExecute</B> that fills in the buffer pointed to by this attribute does not return SQL_SUCCESS or SQL_SUCCESS_WITH_INFO, the contents of the buffer are undefined.</P>
#	
#	<P>For more information, see "<A HREF="odbcusing_arrays_of_parameters.htm">Using Arrays of Parameters</A>" in Chapter 9: Executing Statements.</P>
#	</TD>
#	</TR>
    SQL_ATTR_PARAMS_PROCESSED_PTR => {
	type => q(SQLUINTEGER),
	ptr => 1,
	value => undef,
	default => undef,
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_PARAMSET_SIZE<BR>
#	(ODBC 3.0)</TD>
#	<TD width=62%>An SQLUINTEGER value that specifies the number of values for each parameter. If SQL_ATTR_PARAMSET_SIZE is greater than 1, SQL_DESC_DATA_PTR, SQL_DESC_INDICATOR_PTR, and SQL_DESC_OCTET_LENGTH_PTR of the APD point to arrays. The cardinality of each array is equal to the value of this field. 
#	<P>For more information, see "<A HREF="odbcusing_arrays_of_parameters.htm">Using Arrays of Parameters</A>" in Chapter 9: Executing Statements.</P>
#	
#	<P>Setting this statement attribute sets the SQL_DESC_ARRAY_SIZE field in the APD header.</P>
#	</TD>
#	</TR>
    SQL_ATTR_PARAMSET_SIZE => {
	type => q(SQLUINTEGER),
	ptr => 0,
	value => undef,
	default => undef,
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_QUERY_TIMEOUT<BR>
#	(ODBC 1.0)</TD>
#	<TD width=62%>An SQLUINTEGER value corresponding to the number of seconds to wait for an SQL statement to execute before returning to the application. If <I>ValuePtr</I> is equal to 0 (default), there is no timeout.
#	<P>If the specified timeout exceeds the maximum timeout in the data source or is smaller than the minimum timeout, <B>SQLSetStmtAttr</B> substitutes that value and returns SQLSTATE 01S02 (Option value changed).</P>
#	
#	<P>Note that the application need not call <B>SQLCloseCursor</B> to reuse the statement if a <B>SELECT</B> statement timed out.</P>
#	
#	<P>The query timeout set in this statement attribute is valid in both synchronous and asynchronous modes.</P>
#	</TD>
#	</TR>
    SQL_ATTR_QUERY_TIMEOUT => {
	type => q(SQLUINTEGER),
	ptr => 0,
	value => undef,
	default => undef,
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_RETRIEVE_DATA<BR>
#	(ODBC 2.0)</TD>
#	<TD width=62%>An SQLUINTEGER value:
#	<P>SQL_RD_ON = <B>SQLFetchScroll</B> and, in ODBC 3<I>.x</I>, <B>SQLFetch</B> retrieve data after it positions the cursor to the specified location. This is the default.</P>
#	
#	<P>SQL_RD_OFF = <B>SQLFetchScroll</B> and, in ODBC 3<I>.x</I>, <B>SQLFetch</B> do not retrieve data after it positions the cursor.</P>
#	
#	<P>By setting SQL_RETRIEVE_DATA to SQL_RD_OFF, an application can verify that a row exists or retrieve a bookmark for the row without incurring the overhead of retrieving rows. For more information, see "<A HREF="odbcscrolling_and_fetching_rows.htm">Scrolling and Fetching Rows</A>" in Chapter 11: Retrieving Results (Advanced).</P>
#	
#	<P>The value of this attribute can be set on an open cursor; however, the setting might not take effect immediately, in which case the driver will return SQLSTATE 01S02 (Option value changed) and reset the attribute to its original value.</P>
#	</TD>
#	</TR>
    SQL_ATTR_RETRIEVE_DATA => {
	type => q(SQLUINTEGER),
	ptr => 0,
	value => [ qw(SQL_RD_ON SQL_RD_OFF) ],
	default => q(SQL_RD_ON),
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_ROW_ARRAY_SIZE<BR>
#	(ODBC 3.0)</TD>
#	<TD width=62%>An SQLUINTEGER value that specifies the number of rows returned by each call to <B>SQLFetch</B> or <B>SQLFetchScroll</B>. It is also the number of rows in a bookmark array used in a bulk bookmark operation in <B>SQLBulkOperations</B>. The default value is 1.
#	<P>If the specified rowset size exceeds the maximum rowset size supported by the data source, the driver substitutes that value and returns SQLSTATE 01S02 (Option value changed).</P>
#	
#	<P>For more information, see "<A HREF="odbcrowset_size.htm">Rowset Size</A>" in Chapter 11: Retrieving Results (Advanced).</P>
#	
#	<P>Setting this statement attribute sets the SQL_DESC_ARRAY_SIZE field in the ARD header.</P>
#	</TD>
#	</TR>
    SQL_ATTR_ROW_ARRAY_SIZE => {
	type => q(SQLUINTEGER),
	ptr => 0,
	value => undef,
	default => undef,
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_ROW_BIND_OFFSET_PTR<BR>
#	(ODBC 3.0)</TD>
#	<TD width=62%>An SQLUINTEGER * value that points to an offset added to pointers to change binding of column data. If this field is non-null, the driver dereferences the pointer, adds the dereferenced value to each of the deferred fields in the descriptor record (SQL_DESC_DATA_PTR, SQL_DESC_INDICATOR_PTR, and SQL_DESC_OCTET_LENGTH_PTR), and uses the new pointer values when binding. It is set to null by default.
#	<P>Setting this statement attribute sets the SQL_DESC_BIND_OFFSET_PTR field in the ARD header.</P>
#	</TD>
#	</TR>
    SQL_ATTR_ROW_BIND_OFFSET_PTR => {
	type => q(SQLUINTEGER),
	ptr => undef,
	value => undef,
	default => undef,
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_ROW_BIND_TYPE<BR>
#	(ODBC 1.0)</TD>
#	<TD width=62%>An SQLUINTEGER value that sets the binding orientation to be used when <B>SQLFetch</B> or <B>SQLFetchScroll</B> is called on the associated statement. Column-wise binding is selected by setting the value to SQL_BIND_BY_COLUMN. Row-wise binding is selected by setting the value to the length of a structure or an instance of a buffer into which result columns will be bound.
#	<P>If a length is specified, it must include space for all of the bound columns and any padding of the structure or buffer to ensure that when the address of a bound column is incremented with the specified length, the result will point to the beginning of the same column in the next row. When using the <B>sizeof</B> operator with structures or unions in ANSI C, this behavior is guaranteed.</P>
#	
#	<P>Column-wise binding is the default binding orientation for <B>SQLFetch</B> and <B>SQLFetchScroll</B>.</P>
#	
#	<P>For more information, see "<A HREF="odbcbinding_columns_for_use_with_block_cursors.htm">Binding Columns for Use with Block Cursors</A>" in Chapter 11: Retrieving Results (Advanced).</P>
#	
#	<P>Setting this statement attribute sets the SQL_DESC_BIND_TYPE field in the ARD header.</P>
#	</TD>
#	</TR>
    SQL_ATTR_ROW_BIND_TYPE => {
	type => q(SQLUINTEGER),
	ptr => 0,
	value => [ qw(SQL_BIND_BY_COLUMN etc) ],
	default => q(SQL_BIND_BY_COLUMN),
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_ROW_NUMBER<BR>
#	(ODBC 2.0)</TD>
#	<TD width=62%>An SQLUINTEGER value that is the number of the current row in the entire result set. If the number of the current row cannot be determined or there is no current row, the driver returns 0.
#	<P>This attribute can be retrieved by a call to <B>SQLGetStmtAttr</B> but not set by a call to <B>SQLSetStmtAttr</B>.</P>
#	</TD>
#	</TR>
    SQL_ATTR_ROW_NUMBER => {
	type => q(SQLUINTEGER),
	ptr => 0,
	value => undef,
	default => undef,
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_ROW_OPERATION_PTR<BR>
#	(ODBC 3.0)</TD>
#	<TD width=62%>An SQLUSMALLINT * value that points to an array of SQLUSMALLINT values used to ignore a row during a bulk operation using <B>SQLSetPos</B>. Each value is set to either SQL_ROW_PROCEED (for the row to be included in the bulk operation) or SQL_ROW_IGNORE (for the row to be excluded from the bulk operation). (Rows cannot be ignored by using this array during calls to <B>SQLBulkOperations</B>.)
#	<P>This statement attribute can be set to a null pointer, in which case the driver does not return row status values. This attribute can be set at any time, but the new value is not used until the next time <B>SQLSetPos</B> is called.</P>
#	
#	<P>For more information, see "<A HREF="odbcupdating_rows_in_the_rowset_with_sqlsetpos.htm">Updating Rows in the Rowset with SQLSetPos</A>" and "<A HREF="odbcdeleting_rows_in_the_rowset_with_sqlsetpos.htm">Deleting Rows in the Rowset with SQLSetPos</A>" in Chapter 12: Updating Data.</P>
#	
#	<P>Setting this statement attribute sets the SQL_DESC_ARRAY_STATUS_PTR field in the ARD.</P>
#	</TD>
#	</TR>
    SQL_ATTR_ROW_OPERATION_PTR => {
	type => q(SQLUSMALLINT),
	ptr => 1,
	value => undef,
	default => undef,
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_ROW_STATUS_PTR<BR>
#	(ODBC 3.0)</TD>
#	<TD width=62%>An SQLUSMALLINT * value that points to an array of SQLUSMALLINT values containing row status values after a call to <B>SQLFetch</B> or <B>SQLFetchScroll</B>. The array has as many elements as there are rows in the rowset. 
#	<P>This statement attribute can be set to a null pointer, in which case the driver does not return row status values. This attribute can be set at any time, but the new value is not used until the next time <B>SQLBulkOperations</B>, <B>SQLFetch</B>, <B>SQLFetchScroll</B>, or <B>SQLSetPos</B> is called.</P>
#	
#	<P>For more information, see "<A HREF="odbcnumber_of_rows_fetched_and_status.htm">Number of Rows Fetched and Status</A>" in Chapter 11: Retrieving Results (Advanced).</P>
#	
#	<P>Setting this statement attribute sets the SQL_DESC_ARRAY_STATUS_PTR field in the IRD header.</P>
#	
#	<P>This attribute is mapped by an ODBC 2<I>.x</I> driver to the <I>rgbRowStatus</I> array in a call to <B>SQLExtendedFetch</B>.</P>
#	</TD>
#	</TR>
    SQL_ATTR_ROW_STATUS_PTR => {
	type => q(SQLUSMALLINT),
	ptr => 1,
	value => undef,
	default => undef,
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_ROWS_FETCHED_PTR<BR>
#	(ODBC 3.0)</TD>
#	<TD width=62%>An SQLUINTEGER * value that points to a buffer in which to return the number of rows fetched after a call to <B>SQLFetch</B> or <B>SQLFetchScroll</B>; the number of rows affected by a bulk operation performed by a call to <B>SQLSetPos</B> with an <I>Operation</I> argument of SQL_REFRESH; or the number of rows affected by a bulk operation performed by <B>SQLBulkOperations</B>. This number includes error rows.
#	<P>For more information, see "<A HREF="odbcnumber_of_rows_fetched_and_status.htm">Number of Rows Fetched and Status</A>" in Chapter 11: Retrieving Results (Advanced).</P>
#	
#	<P>Setting this statement attribute sets the SQL_DESC_ROWS_PROCESSED_PTR field in the IRD header. </P>
#	
#	<P>If the call to <B>SQLFetch</B> or <B>SQLFetchScroll</B> that fills in the buffer pointed to by this attribute does not return SQL_SUCCESS or SQL_SUCCESS_WITH_INFO, the contents of the buffer are undefined.</P>
#	</TD>
#	</TR>
    SQL_ATTR_ROWS_FETCHED_PTR => {
	type => q(SQLUINTEGER),
	ptr => 1,
	value => undef,
	default => undef,
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_SIMULATE_CURSOR<BR>
#	(ODBC 2.0)</TD>
#	<TD width=62%>An SQLUINTEGER value that specifies whether drivers that simulate positioned update and delete statements guarantee that such statements affect only one single row.
#	<P>To simulate positioned update and delete statements, most drivers construct a searched <B>UPDATE</B> or <B>DELETE</B> statement containing a <B>WHERE</B> clause that specifies the value of each column in the current row. Unless these columns make up a unique key, such a statement can affect more than one row.</P>
#	
#	<P>To guarantee that such statements affect only one row, the driver determines the columns in a unique key and adds these columns to the result set. If an application guarantees that the columns in the result set make up a unique key, the driver is not required to do so. This may reduce execution time.</P>
#	
#	<P>SQL_SC_NON_UNIQUE = The driver does not guarantee that simulated positioned update or delete statements will affect only one row; it is the application's responsibility to do so. If a statement affects more than one row, <B>SQLExecute</B>, <B>SQLExecDirect</B>, or <B>SQLSetPos</B> returns SQLSTATE 01001 (Cursor operation conflict).</P>
#	
#	<P>SQL_SC_TRY_UNIQUE = The driver attempts to guarantee that simulated positioned update or delete statements affect only one row. The driver always executes such statements, even if they might affect more than one row, such as when there is no unique key. If a statement affects more than one row, <B>SQLExecute</B>, <B>SQLExecDirect</B>, or <B>SQLSetPos</B> returns SQLSTATE 01001 (Cursor operation conflict).</P>
#	
#	<P>SQL_SC_UNIQUE = The driver guarantees that simulated positioned update or delete statements affect only one row. If the driver cannot guarantee this for a given statement, <B>SQLExecDirect</B> or <B>SQLPrepare</B> returns an error.</P>
#	
#	<P>If the data source provides native SQL support for positioned update and delete statements and the driver does not simulate cursors, SQL_SUCCESS is returned when SQL_SC_UNIQUE is requested for SQL_SIMULATE_CURSOR. SQL_SUCCESS_WITH_INFO is returned if SQL_SC_TRY_UNIQUE or SQL_SC_NON_UNIQUE is requested. If the data source provides the SQL_SC_TRY_UNIQUE level of support and the driver does not, SQL_SUCCESS is returned for SQL_SC_TRY_UNIQUE and SQL_SUCCESS_WITH_INFO is returned for SQL_SC_NON_UNIQUE.</P>
#	
#	<P>If the specified cursor simulation type is not supported by the data source, the driver substitutes a different simulation type and returns SQLSTATE 01S02 (Option value changed). For SQL_SC_UNIQUE, the driver substitutes, in order, SQL_SC_TRY_UNIQUE or SQL_SC_NON_UNIQUE. For SQL_SC_TRY_UNIQUE, the driver substitutes SQL_SC_NON_UNIQUE.</P>
#	
#	<P>For more information, see "<A HREF="odbcsimulating_positioned_update_and_delete_statements.htm">Simulating Positioned Update and Delete Statements</A>" in Chapter 12: Updating Data.</P>
#	</TD>
#	</TR>
    SQL_ATTR_SIMULATE_CURSOR => {
	type => q(SQLUINTEGER),
	ptr => 0,
	value => [ qw(SQL_SC_NON_UNIQUE SQL_SC_TRY_UNIQUE SQL_SC_UNIQUE) ],
	default => undef,
	mode => 'rw',
	order => ++$order,
    },
#	
#	<TR VALIGN="top">
#	<TD width=38%>SQL_ATTR_USE_BOOKMARKS<BR>
#	(ODBC 2.0)</TD>
#	<TD width=62%>An SQLUINTEGER value that specifies whether an application will use bookmarks with a cursor:
#	<P>SQL_UB_OFF = Off (the default)</P>
#	
#	<P>SQL_UB_VARIABLE = An application will use bookmarks with a cursor, and the driver will provide variable-length bookmarks if they are supported. SQL_UB_FIXED is deprecated in ODBC 3<I>.x</I>. ODBC 3<I>.x</I> applications should always use variable-length bookmarks, even when working with ODBC 2<I>.x</I> drivers (which supported only 4-byte, fixed-length bookmarks). This is because a fixed-length bookmark is just a special case of a variable-length bookmark. When working with an ODBC 2<I>.x</I> driver, the Driver Manager maps SQL_UB_VARIABLE to SQL_UB_FIXED.</P>
#	
#	<P>To use bookmarks with a cursor, the application must specify this attribute with the SQL_UB_VARIABLE value before opening the cursor.</P>
#	
#	<P>For more information, see "<A HREF="odbcretrieving_bookmarks.htm">Retrieving Bookmarks</A>" in Chapter 11: Retrieving Results (Advanced).</P>
#	</TD>
#	</TR>
    SQL_ATTR_USE_BOOKMARKS => {
	type => q(SQLUINTEGER),
	ptr => 0,
	value => [ qw(SQL_UB_OFF SQL_UB_VARIABLE SQL_UB_FIXED) ],
	default => undef,
	mode => 'rw',
	order => ++$order,
    },
#	</table></div>
#	
#	<P class="fineprint">[1]&nbsp;&nbsp;&nbsp;These functions can be called asynchronously only if the descriptor is an implementation descriptor, not an application descriptor.</p>
#	<P>See "<A HREF="odbccolumn_wise_binding.htm">Column-Wise Binding</A>" and "<A HREF="odbcrow_wise_binding.htm">Row-Wise Binding</A>" in Chapter 11: Retrieving Results (Advanced).</P>
#	
#	<P class="label"><B>Related Functions</B></P>
#	<!--TS:--><div class="tablediv"><table>
#	
#	<TR VALIGN="top">
#	<TH width=47%>For information about</TH>
#	<TH width=53%>See</TH>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=47%>Canceling statement processing</TD>
#	<TD width=53%><A HREF="odbcsqlcancel.htm">SQLCancel</A></TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=47%>Returning the setting of a connection attribute</TD>
#	<TD width=53%><A HREF="odbcsqlgetconnectattr.htm">SQLGetConnectAttr</A></TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=47%>Returning the setting of a statement attribute</TD>
#	<TD width=53%><A HREF="odbcsqlgetstmtattr.htm">SQLGetStmtAttr</A></TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=47%>Setting a connection attribute</TD>
#	<TD width=53%><A HREF="odbcsqlsetconnectattr.htm">SQLSetConnectAttr</A></TD>
#	</TR>
#	
#	<TR VALIGN="top">
#	<TD width=47%>Setting a single field of the descriptor</TD>
#	<TD width=53%><A HREF="odbcsqlsetdescfield.htm">SQLSetDescField</A></TD>
#	</TR>
#	</table></div>
#	<!--TS:--><H4><A NAME="feedback"></A></H4>
#	<SPAN id="SDKFeedB"></SPAN>
#	</div>
#	
#	</BODY>
#	</HTML>
};

my $i3 = " " x 3;
my $i4 = " " x 4;
my $i8 = " " x 8;

my $type2type = {
    SQLSMALLINT => 'Smallint',
    SQLUSMALLINT => 'Usmallint',
    SQLINTEGER => 'Integer',
    SQLUINTEGER => 'Uinteger',
    SQLPOINTER => 'Pointer',
    SQLCHAR => 'Sqlchar',
};

my $attr =
    $type eq 'Env' ? $attrEnv :
    $type eq 'Dbc' ? $attrDbc :
    $type eq 'Stmt' ? $attrStmt : die "bad type $type";

my @name = sort {
    $attr->{$a}{order} <=> $attr->{$b}{order}
} keys %$attr;

print "#include \"Handle$type.hpp\"\n";
my $class = "OdbcData";

for my $name (@name) {
    my $p = $attr->{$name};
    my $odbctype = $type2type->{$p->{type}} or die $name;
    $odbctype .= "Ptr" if $p->{ptr};
    print "\nstatic void\n";
    print "callback_${name}_set(Ctx& ctx, HandleBase* self, const $class& data)\n";
    print "{\n";
    print "${i4}Handle$type* p$type = dynamic_cast<Handle$type*>(self);\n";
    print "${i4}assert(p$type != 0 && data.type() == ${class}::$odbctype);\n";
    print "}\n";
    print "\nstatic void\n";
    print "callback_${name}_default(Ctx& ctx, HandleBase* self, $class& data)\n";
    print "{\n";
    print "${i4}Handle$type* p$type = dynamic_cast<Handle$type*>(self);\n";
    print "${i4}assert(p$type != 0);\n";
    print "${i4}data.set();\n";
    print "}\n";
}

print "\nAttrSpec Handle${type}::m_attrSpec\[\] = {\n";
for my $name (@name) {
    my $p = $attr->{$name};
    my $odbctype = $type2type->{$p->{type}} or die $name;
    $odbctype .= "Ptr" if $p->{ptr};
    print "${i4}\{${i3}$name,\n";
    print "${i8}${class}::$odbctype,\n";
    my $attrmode =
	$p->{mode} eq 'rw' ? 'Attr_mode_readwrite' :
	$p->{mode} eq 'ro' ? 'Attr_mode_readonly' : die "bad mode $p->{mode}";
    print "${i8}$attrmode,\n";
    print "${i8}callback_${name}_set,\n";
    print "${i8}callback_${name}_default,\n";
    print "${i4}\},\n";
}
print "${i4}\{${i3}0,\n";
print "${i8}${class}::Undef,\n";
print "${i8}Attr_mode_undef,\n";
print "${i8}0,\n";
print "${i8}0,\n";
print "${i4}\},\n";

print "};\n";
