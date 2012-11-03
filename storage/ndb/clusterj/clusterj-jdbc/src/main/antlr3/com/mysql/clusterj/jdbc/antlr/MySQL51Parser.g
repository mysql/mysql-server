/*
 *  Copyright (c) 2008, 2011, Oracle and/or its affiliates. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

parser grammar MySQL51Parser; 

options {
	output=AST;
	tokenVocab=MySQL51Lexer;
	superClass=MySQLParser;
}

import MySQL51Functions;

tokens {
SELECT;
SELECT_EXPR;
UNARY_MINUS;
UNARY_PLUS;
OPTIONS;
FUNC;
DIRECTION;
ALIAS;
FIELD;
SUBSELECT;
COMMA_JOIN;
COLUMNS;
INSERT_VALUES;
INDEX_HINTS;
ROLLBACK_POINT;	/* rollback to savepoint */
/* token types for the various CREATE statements */
CREATE_TABLE;
/* helper tokens for column definitions  */
NOT_NULL;
DEFINITION;
DEFINITIONS;
COUNT_STAR;
}

@header{
package com.mysql.clusterj.jdbc.antlr;
}

statement_list
	:	stmts+=statement (SEMI stmts+=statement)* SEMI*
		-> $stmts+
	;

statement
	:	(	select
		|	do_stmt
		|	show_stmt
		|	explain_stmt
		|	insert
		|	update
		|	delete
		|	transaction
		|	create_table
		|	COMMENT_RULE
		)
	;

// $< DML

do_stmt	:	DO exprList		-> ^(DO exprList)
	;

show_stmt
  : (SHOW -> ^(SHOW))
    ( AUTHORS     -> ^($show_stmt ^(AUTHORS))
    | BINLOG EVENTS (IN logName=STRING)? (FROM NUMBER)? limit?  -> ^($show_stmt ^(BINLOG $logName? NUMBER? limit?))
    | CHARACTER SET like_or_where?  -> ^($show_stmt ^(CHARACTER like_or_where?))
    | COLLATION like_or_where?    -> ^($show_stmt ^(COLLATION like_or_where?))
    | FULL? COLUMNS (FROM|IN) simple_table_ref_no_alias ((FROM|IN) ident)? like_or_where? -> ^($show_stmt ^(COLUMNS FULL? simple_table_ref_no_alias ident? like_or_where?))
    | CONTRIBUTORS  -> ^($show_stmt ^(CONTRIBUTORS))
    | CREATE DATABASE ident -> ^($show_stmt ^(CREATE DATABASE ident))
    | CREATE EVENT ident    -> ^($show_stmt ^(CREATE EVENT ident))
    | CREATE FUNCTION ident -> ^($show_stmt ^(CREATE FUNCTION ident))
    | CREATE PROCEDURE ident  -> ^($show_stmt ^(CREATE PROCEDURE ident))
    | CREATE TABLE simple_table_ref_no_alias  -> ^($show_stmt ^(CREATE TABLE simple_table_ref_no_alias))
    | CREATE TRIGGER ident  -> ^($show_stmt ^(CREATE TRIGGER ident))
    | CREATE VIEW ident   -> ^($show_stmt ^(CREATE VIEW ident))
    | DATABASES like_or_where?  -> ^($show_stmt ^(DATABASES like_or_where?))
    | ENGINE (INNODB | ident) (what=STATUS | what=MUTEX) // have to add INNODB token, because of SHOW INNODB STATUS :(
                    -> ^($show_stmt ^(ENGINE INNODB? ident? $what))
    | STORAGE? ENGINES    -> ^($show_stmt ^(ENGINES))
    | ERRORS limit?     -> ^($show_stmt ^(ERRORS limit?))
    | FULL? EVENTS      -> ^($show_stmt ^(EVENTS FULL?))
    | FUNCTION CODE ident   -> ^($show_stmt ^(CODE FUNCTION ident))
    | FUNCTION STATUS like_or_where?  -> ^($show_stmt ^(STATUS FUNCTION like_or_where?))
    | GRANTS FOR
        ( whom=USER_HOST
        | whom=CURRENT_USER
        | whom=CURRENT_USER LPAREN RPAREN
        )         -> ^($show_stmt ^(GRANTS $whom))
    | INDEX_SYM FROM simple_table_ref_no_alias ((FROM|IN) ident)? -> ^($show_stmt ^(INDEX_SYM simple_table_ref_no_alias ident?))
    | INNODB STATUS   -> ^($show_stmt ^(ENGINE INNODB STATUS))
    | OPEN TABLES ((FROM|IN) ident)? like_or_where? -> ^($show_stmt ^(OPEN ident? like_or_where?))
    | PLUGINS     -> ^($show_stmt ^(PLUGINS))
    | PROCEDURE CODE ident      -> ^($show_stmt ^(CODE PROCEDURE ident))
    | PROCEDURE STATUS like_or_where? -> ^($show_stmt ^(STATUS PROCEDURE ident))
    | PRIVILEGES      -> ^($show_stmt ^(PRIVILEGES))
    | FULL? PROCESSLIST -> ^($show_stmt ^(PROCESSLIST FULL?))
    | PROFILE show_profile_types (FOR QUERY NUMBER)? limit? -> ^($show_stmt ^(PROFILE show_profile_types NUMBER? limit?))
    | PROFILES    -> ^($show_stmt ^(PROFILES))
    | SCHEDULER STATUS  -> ^($show_stmt ^(STATUS SCHEDULER))
    | optScopeModifier STATUS like_or_where?  -> ^($show_stmt ^(STATUS optScopeModifier? like_or_where?))
    | TABLE STATUS ((FROM|IN) ident)? like_or_where?      -> ^($show_stmt  ^(STATUS TABLE ident? like_or_where?))
    | TABLES ((FROM|IN) ident)? like_or_where?          -> ^($show_stmt ^(TABLES ident? like_or_where?))
    | TRIGGERS ((FROM|IN) ident)? like_or_where?        -> ^($show_stmt ^(TRIGGERS ident? like_or_where?))
    | optScopeModifier VARIABLES like_or_where?   -> ^($show_stmt ^(VARIABLES optScopeModifier? like_or_where?))
    | WARNINGS limit?   -> ^($show_stmt ^(WARNINGS limit?))
    )
  ;
  
optScopeModifier
  : GLOBAL    -> GLOBAL
  | SESSION   -> SESSION
  | l=LOCAL   -> SESSION[$l]
  | /* empty, defaults to SESSION */ -> SESSION
  ;

show_profile_types
  : ALL
  | BLOCK IO
  | CONTEXT SWITCHES
  | CPU
  | IPC
  | MEMORY
  | PAGE FAULTS
  | SOURCE
  | SWAPS
  ;

like_or_where
  : // behold, this is a special LIKE...does not allow expressions on the RHS
    LIKE string_or_placeholder   -> ^(LIKE string_or_placeholder)
  | WHERE expr    -> ^(WHERE expr)
  ;
  
explain_stmt
  : EXPLAIN select -> ^(EXPLAIN select)
  ;

select
@init {
boolean seenUnion = false;
}
	:	select_paren
		(UNION (mod=ALL | mod=DISTINCT)? union_selects+=select_paren {seenUnion=true;})*
		 	-> {seenUnion}? ^(UNION $mod? select_paren $union_selects+)
			-> select_paren
	;

select_paren
	:	LPAREN select_paren RPAREN	-> select_paren
	|	select_inner				-> select_inner
	;
	
select_inner
	:	SELECT (options{k=1;}:select_options)*
		exprs+=select_expr (COMMA exprs+=select_expr)*
		(
            (FROM table_references)
            (WHERE where=where_condition)?
            group_by?
            (HAVING having=where_condition)?
            order_by?
            limit?
            // these procedures are not "SQL-SPs" but C++ ones. very unlikely that we'll see them.
            (PROCEDURE procId=ident procArgs=parenOptExprList)?
            /* TODO: what is allowed in the "STRING" below? probably not N'foobar' etc. */
            ((	INTO OUTFILE file=STRING opts=infile_options_helper
                |	INTO DUMPFILE file=STRING
                |	INTO vars+=variable (COMMA vars+=variable)*
                )	
            )?
            (forUpdate=FOR UPDATE | lock=LOCK IN SHARE MODE)?
        |   order_by
            limit?
        |   limit
		)?
		-> ^(SELECT<com.mysql.clusterj.jdbc.antlr.node.SelectNode>
				^(OPTIONS select_options*)?
				^(COLUMNS $exprs+)
				^(FROM table_references)?
				^(WHERE<com.mysql.clusterj.jdbc.antlr.node.WhereNode> $where)?
				group_by?
				^(HAVING $having)?
				order_by?
				limit?
				FOR?
				LOCK?
			)
	;

infile_options_helper
	:	(	(COLUMNS|FIELDS)
			(TERMINATED BY fieldTerm=STRING)?
			(optEnclosed=OPTIONALLY? ENCLOSED BY fieldEncl=STRING)?			/* TODO: STRING here is one character, really */
			(ESCAPED BY fieldEsc=STRING)?									/* TODO: STRING here is one character, really */
		)?
		(	LINES
			(STARTING BY linesStart=STRING)?
			(TERMINATED BY linesTerm=STRING)?
		)?
	;
	
variable
	:	SESSION_VARIABLE
	|	GLOBAL_VARIABLE
	;

limit
	:	LIMIT
			(	((offset=number_or_placeholder COMMA)? lim=number_or_placeholder)	
			| 	(lim=number_or_placeholder offsetForm=OFFSET offset=number_or_placeholder)
			)
		-> ^(LIMIT $lim $offset? $offsetForm?)
	;

string_or_placeholder
	:	STRING
	|	VALUE_PLACEHOLDER
	;

number_or_placeholder
	:	NUMBER
	|	VALUE_PLACEHOLDER
	;

text_string
	:	STRING
	|	BINARY_VALUE
	|	HEXA_VALUE
	;

group_by
	:	GROUP BY 
			elements+=order_group_by_elements (COMMA elements+=order_group_by_elements )*
		(rollup=WITH ROLLUP)?
		-> ^(GROUP $elements+ $rollup?)
	;

order_by
	:	ORDER BY
			elements+=order_group_by_elements (COMMA elements+=order_group_by_elements )*
		-> ^(ORDER $elements+)
	;
	
order_group_by_elements
	:	expr 
		(	asc=ASC						-> ^(DIRECTION[$asc] expr)
		|	desc=DESC					-> ^(DIRECTION[$desc] expr)
		|	/* implicit ASC */			-> ^(DIRECTION["ASC"] expr)
		)
	;

select_options
	:	ALL
	|	DISTINCT
	|	DISTINCTROW
	|	HIGH_PRIORITY
	|	STRAIGHT_JOIN
	|	SQL_SMALL_RESULT
	|	SQL_BIG_RESULT
// the following cause parser warnings
//	|	SQL_BUFFER_RESULT
//	|	SQL_CACHE
//	|	SQL_NO_CACHE
	|	SQL_CALC_FOUND_ROWS
	;
	
select_expr
	:	expr (AS? ident)?		-> ^(SELECT_EXPR expr ^(ALIAS ident)?)
	|	star=MULT				-> ^(SELECT_EXPR $star)
	;
/*	catch[RecognitionException re] {
	   Object errorNode = (Object)adaptor.errorNode(input, retval.start, input.LT(-1), re);
	   Object root_1 = (Object)adaptor.nil();
     root_1 = (Object)adaptor.becomeRoot((Object)adaptor.create(SELECT_EXPR, "SELECT_EXPR"), root_1);
     adaptor.addChild(root_1, errorNode);
     adaptor.addChild(root_0, root_1);
     retval.tree = (Object)adaptor.rulePostProcessing(root_0);
	}
*/
table_references returns [int table_count]
scope {
int count;
}
@init {
$table_references::count = 0;
}
@after {
$table_references.table_count = $table_references::count;
}
	:	(	t1=table_ref 			-> $t1)
		(	COMMA t2=table_ref		-> ^(COMMA_JOIN[$COMMA] $table_references $t2)
		)*
	;

/* left factored to get rid of the recursion */
table_ref
	:	(t1=table_factor -> $t1 )
		(
			(LEFT|RIGHT)=>(ltype=LEFT|ltype=RIGHT) outer=OUTER? JOIN t3=table_ref lrjoinCond=join_condition_either 
				-> ^($ltype {$tree} $t3 $lrjoinCond $outer?)
// join condition is not optional here
		|	(type=INNER|type=CROSS)? JOIN t2=table_factor cond1=join_condition_either 
				-> ^(JOIN {$tree} $t2 $cond1? $type?)
		|	(	type=STRAIGHT_JOIN t2=table_factor 
				(	(join_condition_on)=> cond2=join_condition_on	-> ^($type {$tree} $t2 $cond2)
				|							-> ^($type {$tree} $t2)
				)
			)
		|	(NATURAL)=> NATURAL ((type=LEFT|type=RIGHT) outer=OUTER?)? JOIN t2=table_factor -> ^(NATURAL $type? {$tree} $t2 $outer?)
		)*
	;
	
table_factor
	:	simple_table_ref_alias index_hint_list? {$table_references::count++;} -> ^(TABLE simple_table_ref_alias index_hint_list?)
	|	LPAREN select_inner RPAREN AS? ident		{$table_references::count++;} -> ^(SUBSELECT select_inner ^(ALIAS ident))
    |   LPAREN table_ref {$table_references::count++;} (COMMA table_ref {$table_references::count++;} )* RPAREN   -> ^(TABLE table_ref+)
	/* ident in the following should really by 'OJ', but the parser accepts any identifier in its place */
	|	LCURLY ident t1=table_ref LEFT OUTER JOIN t2=table_ref join_condition_on RCURLY 
			-> ^(ident $t1 $t2 join_condition_on)
	|	DUAL								-> ^(DUAL)
	;

join_condition_on
	:	ON where_condition		-> ^(ON where_condition)
	;

join_condition_either
	:	join_condition_on
	|	USING LPAREN fields+=ident (COMMA fields+=ident)* RPAREN		-> ^(USING $fields+)
	;

simple_table_ref_no_alias
	:	first=ident (DOT second=ident)?		-> $first $second?
	;

simple_table_ref_alias
	:	first=ident (DOT second=ident)? table_alias?		-> $first $second? table_alias?
	;

table_alias
	:	AS? alias=ident	-> ^(ALIAS $alias)
	;

field_name
@init {
int i = 0;
boolean seenStar = false;
}
	:	ident
		({seenStar == false}?=> 
		  (DOT 
		    ({seenStar == false}? (ident | star=MULT {seenStar = true;}) {++i <= 2}?)
		  )*
		)
		-> ^(FIELD ident+ $star?)
//	|	(DOT)=>DOT column=field_name_column	-> ^(FIELD $column)
	;

/* list all keywords that can also be used as an identifier
   This list is taken from the 5.1 YACC grammar 
*/

ident
	:	
	(	tok=ASCII
	|	tok=BACKUP
	|	tok=BEGIN
	|	tok=BYTE
	|	tok=CACHE
	|	tok=CHARSET
	|	tok=CHECKSUM
	|	tok=CLOSE
	|	tok=COMMENT
	|	tok=COMMIT
	|	tok=CONTAINS
	|	tok=DEALLOCATE
	|	tok=DO
	|	tok=END
	|	tok=EXECUTE
	|	tok=FLUSH
	|	tok=GROUP
	|	tok=HANDLER
	|	tok=HELP
	|	tok=HOST
	|	tok=INSTALL
	|	tok=LABEL
	|	tok=LANGUAGE
	|	tok=NO
	|	tok=OPEN
	|	tok=OPTIONS
	|	tok=OWNER
	|	tok=PARSER
	|	tok=PARTITION
	|	tok=PORT
	|	tok=PREPARE
	|	tok=REMOVE
	|	tok=REPAIR
	|	tok=RESET
	|	tok=RESTORE
	|	tok=ROLLBACK
	|	tok=SAVEPOINT
	|	tok=SECURITY
	|	tok=SERVER
	|	tok=SIGNED
	|	tok=SOCKET
	|	tok=SLAVE
	|	tok=SONAME
	|	tok=START
	|	tok=STOP
	|	tok=TRUNCATE
	|	tok=UNICODE
	|	tok=UNINSTALL
	|	tok=WRAPPER
	|	tok=XA
	|	tok=UPGRADE
	)	-> ID[$tok]
	|	ident_sp_label	-> {$ident_sp_label.tree}
	;

ident_sp_label
	:		
	(	tok=ID!
	|	tok=ACTION!
	|	tok=ADDDATE!
	|	tok=AFTER!
	|	tok=AGAINST!
	|	tok=AGGREGATE!
	|	tok=ALGORITHM!
	|	tok=ANY!
	|	tok=AT!
	|	tok=AUTHORS!
	|	tok=AUTO_INCREMENT!
	|	tok=AUTOEXTEND_SIZE!
	|	tok=AVG_ROW_LENGTH!
	|	tok=AVG!
	|	tok=BINLOG!
	|	tok=BIT!
	|	tok=BLOCK!
	|	tok=BOOL!
	|	tok=BOOLEAN!
	|	tok=BTREE!
	|	tok=CASCADED!
	|	tok=CHAIN!
	|	tok=CHANGED!
	|	tok=CIPHER!
	|	tok=CLIENT!
	|	tok=COALESCE!
	|	tok=CODE!
	|	tok=COLLATION!
	|	tok=COLUMNS!
	|	tok=FIELDS!
	|	tok=COMMITTED!
	|	tok=COMPACT!
	|	tok=COMPLETION!
	|	tok=COMPRESSED!
	|	tok=CONCURRENT!
	|	tok=CONNECTION!
	|	tok=CONSISTENT!
	|	tok=CONTEXT!
	|	tok=CONTRIBUTORS!
	|	tok=CPU!
	|	tok=CUBE!
	|	tok=DATA!
	|	tok=DATAFILE!
	|	tok=DATETIME!
	|	tok=DATE!
	|	tok=DAY!
	|	tok=DEFINER!
	|	tok=DELAY_KEY_WRITE!
	|	tok=DES_KEY_FILE!
	|	tok=DIRECTORY!
	|	tok=DISABLE!
	|	tok=DISCARD!
	|	tok=DISK!
	|	tok=DUMPFILE!
	|	tok=DUPLICATE!
	|	tok=DYNAMIC!
	|	tok=ENDS!
	|	tok=ENUM!
	|	tok=ENGINE!
	|	tok=ENGINES!
	|	tok=ERRORS!
	|	tok=ESCAPE!
	|	tok=EVENT!
	|	tok=EVENTS!
	|	tok=EVERY!
	|	tok=EXPANSION!
	|	tok=EXTENDED!
	|	tok=EXTENT_SIZE!
	|	tok=FAULTS!
	|	tok=FAST!
	|	tok=FOUND!
	|	tok=ENABLE!
	|	tok=FULL!
	|	tok=FILE!
	|	tok=FIRST!
	|	tok=FIXED!
	|	tok=FRAC_SECOND!
	|	tok=GEOMETRY!
	|	tok=GEOMETRYCOLLECTION!
	|	tok=GET_FORMAT!
	|	tok=GRANTS!
	|	tok=GLOBAL!
	|	tok=HASH!
	|	tok=HOSTS!
	|	tok=HOUR!
	|	tok=IDENTIFIED!
	|	tok=INVOKER!
	|	tok=IMPORT!
	|	tok=INDEXES!
	|	tok=INITIAL_SIZE!
	|	tok=IO!
	|	tok=IPC!
	|	tok=ISOLATION!
	|	tok=ISSUER!
	|	tok=INNOBASE!
	|	tok=INSERT_METHOD!
	|	tok=KEY_BLOCK_SIZE!
	|	tok=LAST!
	|	tok=LEAVES!
	|	tok=LESS!
	|	tok=LEVEL!
	|	tok=LINESTRING!
	|	tok=LIST!
	|	tok=LOCAL!
	|	tok=LOCKS!
	|	tok=LOGFILE!
	|	tok=LOGS!
	|	tok=MAX_ROWS!
	|	tok=MASTER!
	|	tok=MASTER_HOST!
	|	tok=MASTER_PORT!
	|	tok=MASTER_LOG_FILE!
	|	tok=MASTER_LOG_POS!
	|	tok=MASTER_USER!
	|	tok=MASTER_PASSWORD!
	|	tok=MASTER_SERVER_ID!
	|	tok=MASTER_CONNECT_RETRY!
	|	tok=MASTER_SSL!
	|	tok=MASTER_SSL_CA!
	|	tok=MASTER_SSL_CAPATH!
	|	tok=MASTER_SSL_CERT!
	|	tok=MASTER_SSL_CIPHER!
	|	tok=MASTER_SSL_KEY!
	|	tok=MAX_CONNECTIONS_PER_HOUR!
	|	tok=MAX_QUERIES_PER_HOUR!
	|	tok=MAX_SIZE!
	|	tok=MAX_UPDATES_PER_HOUR!
	|	tok=MAX_USER_CONNECTIONS!
	|	tok=MAX_VALUE!
	|	tok=MEDIUM!
	|	tok=MEMORY!
	|	tok=MERGE!
	|	tok=MICROSECOND!
	|	tok=MIGRATE!
	|	tok=MINUTE!
	|	tok=MIN_ROWS!
	|	tok=MODIFY!
	|	tok=MODE!
	|	tok=MONTH!
	|	tok=MULTILINESTRING!
	|	tok=MULTIPOINT!
	|	tok=MULTIPOLYGON!
	|	tok=MUTEX!
	|	tok=NAME!
	|	tok=NAMES!
	|	tok=NATIONAL!
	|	tok=NCHAR!
	|	tok=NDBCLUSTER!
	|	tok=NEXT!
	|	tok=NEW!
	|	tok=NO_WAIT!
	|	tok=NODEGROUP!
	|	tok=NONE!
	|	tok=NVARCHAR!
	|	tok=OFFSET!
	|	tok=OLD_PASSWORD!
	|	tok=ONE_SHOT!
	|	tok=ONE!
	|	tok=PACK_KEYS!
	|	tok=PAGE!
	|	tok=PARTIAL!
	|	tok=PARTITIONING!
	|	tok=PARTITIONS!
	|	tok=PASSWORD!
	|	tok=PHASE!
	|	tok=PLUGIN!
	|	tok=PLUGINS!
	|	tok=POINT!
	|	tok=POLYGON!
	|	tok=PRESERVE!
	|	tok=PREV!
	|	tok=PRIVILEGES!
	|	tok=PROCESS!
	|	tok=PROCESSLIST!
	|	tok=PROFILE!
	|	tok=PROFILES!
	|	tok=QUARTER!
	|	tok=QUERY!
	|	tok=QUICK!
	|	tok=REBUILD!
	|	tok=RECOVER!
	|	tok=REDO_BUFFER_SIZE!
	|	tok=REDOFILE!
	|	tok=REDUNDANT!
	|	tok=RELAY_LOG_FILE!
	|	tok=RELAY_LOG_POS!
	|	tok=RELAY_THREAD!
	|	tok=RELOAD!
	|	tok=REORGANIZE!
	|	tok=REPEATABLE!
	|	tok=REPLICATION!
	|	tok=RESOURCES!
	|	tok=RESUME!
	|	tok=RETURNS!
	|	tok=ROLLUP!
	|	tok=ROUTINE!
	|	tok=ROWS!
	|	tok=ROW_FORMAT!
	|	tok=ROW!
	|	tok=RTREE!
	|	tok=SCHEDULE!
	|	tok=SECOND!
	|	tok=SERIAL!
	|	tok=SERIALIZABLE!
	|	tok=SESSION!
	|	tok=SIMPLE!
	|	tok=SHARE!
	|	tok=SHUTDOWN!
	|	tok=SNAPSHOT!
	|	tok=SOUNDS!
	|	tok=SOURCE!
	|	tok=SQL_CACHE!
	|	tok=SQL_BUFFER_RESULT!
	|	tok=SQL_NO_CACHE!
	|	tok=SQL_THREAD!
	|	tok=STARTS!
	|	tok=STATUS!
	|	tok=STORAGE!
	|	tok=STRING_KEYWORD!
	|	tok=SUBDATE!
	|	tok=SUBJECT!
	|	tok=SUBPARTITION!
	|	tok=SUBPARTITIONS!
	|	tok=SUPER!
	|	tok=SUSPEND!
	|	tok=SWAPS!
	|	tok=SWITCHES!
	|	tok=TABLES!
	|	tok=TABLESPACE!
	|	tok=TEMPORARY!
	|	tok=TEMPTABLE!
	|	tok=TEXT!
	|	tok=THAN!
	|	tok=TRANSACTION!
	|	tok=TRANSACTIONAL!
	|	tok=TRIGGERS!
	|	tok=TIMESTAMP!
	|	tok=TIMESTAMP_ADD!
	|	tok=TIMESTAMP_DIFF!
	|	tok=TIME!
	|	tok=TYPES!
	|	tok=TYPE!
	|	tok=UDF_RETURNS!
	|	tok=FUNCTION!
	|	tok=UNCOMMITTED!
	|	tok=UNDEFINED!
	|	tok=UNDO_BUFFER_SIZE!
	|	tok=UNDOFILE!
	|	tok=UNKNOWN!
	|	tok=UNTIL!
	|	tok=USER!
	|	tok=USE_FRM!
	|	tok=VARIABLES!
	|	tok=VIEW!
	|	tok=VALUE!
	|	tok=WARNINGS!
	|	tok=WAIT!
	|	tok=WEEK!
	|	tok=WORK!
	|	tok=X509!
	|	tok=YEAR!
	)	
	{
		adaptor.addChild(root_0, (Object)adaptor.create(ID, $tok));
	}
	;

index_hint_list
	:	index_hint (COMMA index_hint)*		-> ^(INDEX_HINTS index_hint+)
	;

index_hint
scope {
boolean namesOptional;
}
@init {
$index_hint::namesOptional = false;
}
	:	USE {$index_hint::namesOptional = true;} index_hint_rest		-> ^(USE index_hint_rest)
	|	IGNORE {$index_hint::namesOptional = false;} index_hint_rest	-> ^(IGNORE index_hint_rest)
	|	FORCE {$index_hint::namesOptional = false;} index_hint_rest	-> ^(FORCE index_hint_rest)
	;

index_hint_rest
	:	(name=INDEX|name=KEY) (FOR (usage=JOIN | usage=ORDER BY | usage=GROUP BY))?
		LPAREN
		( {$index_hint::namesOptional == true}?=> (names+=ident (COMMA names+=ident)*)?
		| names+=ident (COMMA names+=ident)* 
		)
		RPAREN
		-> $name ^(LPAREN $names?) $usage?
	;
// $<Expressions

exprList
	:	e+=expr (COMMA e+=expr)*	-> $e+
	;

parenExpr
	:	LPAREN expr RPAREN	-> ^(LPAREN<com.mysql.clusterj.jdbc.antlr.node.ParensNode> expr)
	;
	
parenExprList
	:	LPAREN exprList RPAREN	-> ^(LPAREN exprList)
	;
	
parenOptExprList
	:	LPAREN e+=exprList? RPAREN	-> ^(LPAREN $e*)
	;

expr
	:	lhs=assignOrExpr (op=ASSIGN^ rhs=expr)?
	;

assignOrExpr
	:	lhs=assignXORExpr ((op+=LOGICAL_OR^ | op+=OR<com.mysql.clusterj.jdbc.antlr.node.OrNode>^) rhs+=assignXORExpr)*
	;

assignXORExpr
	:	lhs=assignAndExpr (op+=XOR^ rhs+=assignAndExpr)*
	;

assignAndExpr
	:	lhs=assignNotExpr (( op+=LOGICAL_AND^ | op+=AND<com.mysql.clusterj.jdbc.antlr.node.AndNode>^ ) rhs+=assignNotExpr)*
	;

assignNotExpr
	:	lhs=equalityExpr
	|	op+=NOT<com.mysql.clusterj.jdbc.antlr.node.NotNode>^ rhs+=equalityExpr
	;

equalityExpr
	:	bitwiseOrExpr
		(op+=equalityOperator^ ((subselect_in_expr_rhs)=> subselect_in_expr_rhs | bitwiseOrExpr))*										
	;

subselect_in_expr_rhs
	:	(mod=ANY | mod=SOME | mod=ALL) LPAREN select RPAREN	-> ^(SUBSELECT $mod select)
	;
	
subselect
	:	LPAREN select_inner RPAREN	-> ^(SUBSELECT select_inner)
	;

isOperator
	:	IS NOT? (value2=NULL | value2=FALSE | value2=TRUE | value2=UNKNOWN)	-> ^(IS NOT? $value2)
	;

equalityOperator
	:	(	value=EQUALS<com.mysql.clusterj.jdbc.antlr.node.EqualsNode>
		|	value=NOT_EQUAL
		|	value=LESS_THAN<com.mysql.clusterj.jdbc.antlr.node.LessThanNode>
		|	value=LESS_THAN_EQUAL<com.mysql.clusterj.jdbc.antlr.node.LessEqualsNode>
		|	value=GREATER_THAN<com.mysql.clusterj.jdbc.antlr.node.GreaterThanNode>		
		| 	value=GREATER_THAN_EQUAL<com.mysql.clusterj.jdbc.antlr.node.GreaterEqualsNode>
		|	value=NULL_SAFE_NOT_EQUAL
		| 	value=REGEXP
		|	value=CASE
		|	value=WHEN
		|	value=THEN
		|	value=ELSE
		)
	;

bitwiseOrExpr
    : lhs=bitwiseAndExpr 
    ( (op+=BITWISE_OR^ rhs+=bitwiseAndExpr)+
// force compiler to always recognize NOT IN regardless of whatever follows
    | (((NOT^)? IN^)=>(NOT^)? IN^ (parenExprList | subselect))
    | LIKE^ unaryExpr (ESCAPE STRING)?  // STRING must be empty or one character long (or be "\\" if not in sql_mode NO_BACKSLASH_ESCAPES)
    | isOperator^
    | ((NOT^)? BETWEEN^)=> (NOT<com.mysql.clusterj.jdbc.antlr.node.NotNode>^)? (BETWEEN<com.mysql.clusterj.jdbc.antlr.node.BetweenNode>^ unaryExpr AND! unaryExpr )
    )?
    ;

bitwiseAndExpr
	:	lhs=shiftExpr (op+=BITWISE_AND^ rhs+=shiftExpr)*
	;

shiftExpr
	:	lhs=additiveExpr ((op+=LEFT_SHIFT^ | op+=RIGHT_SHIFT^) rhs+=additiveExpr)*
	;

/* this is ugly because of INTERVAL:
   As rightmost in an expression, it has the highest precendence.
   Otherwise it must be followed by PLUS|MINUS.
   TODO: It cannot be on the left of a MINUS, because that expression makes no sense.
*/
additiveExpr
// force any PLUS or MINUS to be binary not unary for this rule
    :   lhs=multiplicativeExpr ((PLUS|MINUS)=>(op+=PLUS^|op+=MINUS^) rhs+=multiplicativeExpr)*
	;

multOperator
	:	value=MULT
	|	value=DIVISION
	|	value=DIV
	|	value=MODULO
	;

multiplicativeExpr
	:	lhs=bitwiseXORExpr (op+=multOperator^ rhs+=bitwiseXORExpr)*
	;

bitwiseXORExpr
	:	lhs=unaryExpr (op+=BITWISE_XOR^ rhs+=unaryExpr)*
	;

unaryExpr
	:	op=MINUS lhs=unaryExpr	-> ^(UNARY_MINUS[$op] $lhs)
	|	op=PLUS lhs=unaryExpr	-> ^(UNARY_PLUS[$op] $lhs)
	|	op=BITWISE_INVERSION lhs=unaryExpr -> ^(BITWISE_INVERSION $lhs)
	|	lhsUnaryNot=unaryNotExpr	-> unaryNotExpr
	;
	
unaryNotExpr
	:	op=NOT_OP lhs=unaryNotExpr	-> ^(NOT_OP $lhs)
	|	lhsBin=binaryCollateExpr	-> binaryCollateExpr
	;

binaryCollateExpr
	:	op=BINARY lhs=binaryCollateExpr		-> ^(BINARY $lhs)
	|	op=COLLATE lhs=binaryCollateExpr	-> ^(COLLATE $lhs)
	|	intervalExpr
	;

/* INTERVAL can bind extremely closely, if used as the rightmost subexpr of an expression, otherwise it is in additiveExpr 
   the validating predicate disallows its usage all by itself (can't select just an interval, it must be used in an additive expr)
   defer checking that to a semantic tree phase.
*/
intervalExpr
	:	(INTERVAL ~(LPAREN))=> INTERVAL expr timeUnit {input.LA(1) == PLUS || input.LA(1) == MINUS}? -> ^(INTERVAL expr timeUnit)
	|	lhsPrim=primary	-> primary
	;
	
primary
	:	lhsParen=parenExpr -> parenExpr
	|	lhsLit=literal	-> literal
	|	subselect	-> subselect
	|	EXISTS subselect -> ^(EXISTS subselect)
	// TODO: add missing primary expressions, like ROW, DEFAULT etc.
	;

literal
	:	value=STRING
	|	value=NUMBER
	|	value=GLOBAL_VARIABLE
	|	value=SESSION_VARIABLE
	|	value=VALUE_PLACEHOLDER<com.mysql.clusterj.jdbc.antlr.node.PlaceholderNode>
	|	value=BINARY_VALUE
	|	value=HEXA_VALUE
	|	value=NULL
	|	value=TRUE
	|	value=FALSE
	|	(functionCall)=>functionCall
	|	field_name
	;
// $>

cast_data_type
	:	BINARY (LPAREN NUMBER RPAREN)?
	|	CHAR (LPAREN NUMBER RPAREN)?
	|	DATE
	|	DATETIME
	|	TIME
	|	DECIMAL	(LPAREN num1=NUMBER COMMA num2=NUMBER RPAREN)?
	|	SIGNED INTEGER?
	|	UNSIGNED INTEGER?
	;
	
timeUnit
	:	MICROSECOND
	|	SECOND
	|	MINUTE
	|	HOUR
	|	DAY
	|	WEEK
	|	MONTH
	|	QUARTER
	|	YEAR
	|	SECOND_MICROSECOND
	|	MINUTE_MICROSECOND
	|	MINUTE_SECOND
	|	HOUR_MICROSECOND
	|	HOUR_SECOND
	|	HOUR_MINUTE
	|	DAY_MICROSECOND
	|	DAY_SECOND
	|	DAY_MINUTE
	|	DAY_HOUR
	|	YEAR_MONTH
	;

/* TODO: add the SQL_TSI_ prefix versions */
timestampUnit
	:	FRAC_SECOND | MICROSECOND
	|	SECOND
	|	MINUTE
	|	HOUR
	|	DAY
	|	WEEK
	|	MONTH
	|	QUARTER
	|	YEAR
	;
	
where_condition
	:	expr
	;

// $< Transactions

/* generates bogus warning about RELEASE */
transaction
	// general trx statements
	:	(	BEGIN  WORK?										-> ^(BEGIN WORK?)
		|	START TRANSACTION (WITH CONSISTENT SNAPSHOT)?		-> ^(START SNAPSHOT?)
		)
	|	COMMIT WORK?
		(AND NO? CHAIN)?
		(NO? RELEASE)?											-> ^(COMMIT ^(CHAIN NO?)? ^(RELEASE NO?)?)
	|	ROLLBACK WORK?
		(AND NO? CHAIN)?
		(NO? RELEASE)?											-> ^(ROLLBACK ^(CHAIN NO?)? ^(RELEASE NO?)?)
	// NUMBER must be (0 | 1), no grammar checks done at this point, TODO check AUTOCOMMIT vs keywords/identifiers
//	|	SET AUTOCOMMIT EQUALS NUMBER 			
	|	SET 
		(	txnScope=GLOBAL 
		|	txnScope=SESSION 
		)?
		TRANSACTION ISOLATION LEVEL
		(	READ UNCOMMITTED			-> ^(ISOLATION UNCOMMITTED $txnScope)
		|	READ COMMITTED				-> ^(ISOLATION COMMITTED $txnScope)	
		|	REPEATABLE READ				-> ^(ISOLATION REPEATABLE $txnScope)
		|	SERIALIZABLE				-> ^(ISOLATION SERIALIZABLE $txnScope)	
		)
	|	savepoint
	|	lockTables
	//	TODO support for XA transactions is missing
	;

// savepoint handling
savepoint
	:	RELEASE? SAVEPOINT ident				-> ^(SAVEPOINT ident RELEASE?)
	|	ROLLBACK WORK? TO SAVEPOINT? ident		-> ^(ROLLBACK_POINT ident)
	;

// $> Transactions

// $< Insert

insert
	:	INSERT (opt=LOW_PRIORITY | opt=DELAYED | opt=HIGH_PRIORITY)?
		IGNORE? INTO?
		table=simple_table_ref_no_alias
		(	insert_columns
		|	set_columns
		|	select
		)
		on_dup_key?
		-> ^(INSERT<com.mysql.clusterj.jdbc.antlr.node.InsertNode> IGNORE? INTO? $opt? ^(TABLE $table)
			/* the following three lines are really alts as they cannot appear at the same time */
			insert_columns?
			set_columns?
			select?
			on_dup_key?)
	;

insert_columns
	:	(LPAREN column_name_list? RPAREN)?
		(VALUE|VALUES) LPAREN val+=insert_default_or_expression (COMMA val+=insert_default_or_expression)* RPAREN
			-> ^(INSERT_VALUES column_name_list? ^(VALUES[] $val+))
	;

insert_default_or_expression
	:	DEFAULT
	|	expr
	;
	
set_columns
	:	SET column_assignment (COMMA column_assignment)*	-> ^(SET column_assignment+)
	;

on_dup_key
	:	ON DUPLICATE KEY UPDATE
		column_assignment (COMMA column_assignment)*	-> ^(DUPLICATE column_assignment+)
	;

column_assignment
	:	field_name EQUALS 
		(	DEFAULT			-> ^(EQUALS field_name DEFAULT)
		|	expr			-> ^(EQUALS field_name expr)
		)
	;
	
column_name_list
	:	field_name (COMMA field_name)*	-> ^(COLUMNS field_name+)
	;
// $> Insert

// $< Update

update
	:	UPDATE LOW_PRIORITY? IGNORE?
		table=table_references			// this must be table_references because the mysql parser allows an alias here, even for single table updates (unlike DELETE)
		set=set_columns
		(WHERE where_condition)?
		/* these options are only valid if we update one table */
		({$table.table_count==1}?=> 
			order_by?
			(LIMIT NUMBER)?	
		)?
		-> ^(UPDATE LOW_PRIORITY? IGNORE? $table $set ^(WHERE<com.mysql.clusterj.jdbc.antlr.node.WhereNode> where_condition)? order_by? ^(LIMIT NUMBER)?)
	;


// $> Update

// $< Delete

/* both multi table delete trees are basically identical. The FROM and USING nodes are just in there to differentiate between the syntax used, in order to format it correctly
   the AST drops potential .* suffixes for the table names, as they are simply syntactic sugar.
*/
delete
@init {
boolean multiTableDelete = false;
}
	:	DELETE
// opts+=QUICK causes parser warnings
		(options{k=1;}: opts+=LOW_PRIORITY | opts+=IGNORE)*		// the yacc parser accepts any combination and any number of these modifiers, so we do, too.
		(	FROM 
			t+=simple_table_ref_no_alias (DOT MULT {multiTableDelete = true;} )? (COMMA t+=simple_table_ref_no_alias (DOT MULT)? {multiTableDelete = true;} )*
			(USING tr=table_references {multiTableDelete = true;})?
			(WHERE where_condition)?
			({multiTableDelete == false}?=>
				order_by?
				(LIMIT NUMBER)?
			)?
				-> {multiTableDelete}? ^(DELETE<com.mysql.clusterj.jdbc.antlr.node.DeleteNode> ^(OPTIONS $opts+)? ^(TABLE $t)+ ^(USING $tr) ^(WHERE<com.mysql.clusterj.jdbc.antlr.node.WhereNode> where_condition)?)
				-> ^(DELETE<com.mysql.clusterj.jdbc.antlr.node.DeleteNode> ^(OPTIONS $opts+)? ^(TABLE $t) ^(WHERE<com.mysql.clusterj.jdbc.antlr.node.WhereNode> where_condition)? order_by? ^(LIMIT NUMBER)?)
								 
		|	t+=simple_table_ref_no_alias (DOT MULT)? (COMMA t+=simple_table_ref_no_alias (DOT MULT)?)*
			FROM tr=table_references
			(WHERE where_condition)?			-> ^(DELETE ^(OPTIONS $opts+)? ^(TABLE $t)+ ^(FROM $tr) ^(WHERE<com.mysql.clusterj.jdbc.antlr.node.WhereNode> where_condition)?)
		)
	;

// $> Delete

// $< Lock tables

lockTables
	:	LOCK TABLES tables+=lock_table_ref (COMMA tables+=lock_table_ref)*	-> ^(LOCK $tables)
	|	UNLOCK TABLES														-> ^(UNLOCK TABLES)
	;

lock_table_ref
	:	simple_table_ref_alias
		(	READ  (LOCAL )?					-> ^(READ simple_table_ref_alias LOCAL?)
		|	(LOW_PRIORITY )? WRITE 			-> ^(WRITE simple_table_ref_alias LOW_PRIORITY?)
		)
	;

// $> Lock tables

// $> DML

// $< DDL

// $< Create Table

create_table
	:	CREATE (TEMPORARY )? TABLE
		(IF NOT EXISTS )?
		tableName=simple_table_ref_no_alias
		LPAREN create+=create_definition (COMMA create+=create_definition)* RPAREN
		-> ^(CREATE_TABLE
				TEMPORARY?
				EXISTS?
				simple_table_ref_no_alias
				^(DEFINITIONS $create+)
			)
	;

create_definition
	:	colName=ident column_definition	-> ^(DEFINITION $colName column_definition)
	;
	
// $> Create Table

column_definition

	:	data_type
		(notSym=NOT NULL | nullSym=NULL)?
		(DEFAULT literal)?		// TODO check whether literal covers all the legal values
		autoInc=AUTO_INCREMENT?
		(UNIQUE uniqueKey=KEY? | PRIMARY? generalKey=KEY)?
		(COMMENT STRING)?
		(reference_definition )?
		// TODO the following two are NDB specific, skipping for now.
//		(COLUMN_FORMAT (FIXED|DYNAMIC|DEFAULT))?
//		(STORAGE (DISK|MEMORY))?
		-> ^(TYPE data_type
				$notSym?
				($nullSym)?
				^(DEFAULT literal)?
				($autoInc)?
				UNIQUE? PRIMARY? KEY?
			)
	;

data_type
	:	BIT 
		( LPAREN NUMBER  RPAREN )?
	|	(	TINYINT		
		|	SMALLINT	
		|	MEDIUMINT	
		|	INT			
		|	INTEGER		
		|	BIGINT		
		)
		(LPAREN NUMBER  RPAREN)?
		(SIGNED | UNSIGNED )?
		(ZEROFILL )?
	|	(	REAL 		
		|	DOUBLE		
		|	FLOAT		
		|	DECIMAL		
		|	NUMERIC		
		)
		(LPAREN num1=NUMBER COMMA num2=NUMBER RPAREN )?
		(SIGNED | UNSIGNED )?
		(ZEROFILL )?
	|	DATE		
	|	TIME		
	|	TIMESTAMP	
	|	DATETIME	
	|	YEAR		
	|	TINYBLOB	
	|	BLOB		
	|	MEDIUMBLOB	
	|	LONGBLOB	
	|	(	CHAR	
		|	VARCHAR	
		)
		LPAREN NUMBER  RPAREN
		(charset )?
		(collate )?
	|	(	BINARY		
		|	VARBINARY	
		)
		LPAREN NUMBER  RPAREN
	|	(	TINYTEXT	
		|	TEXT		
		|	MEDIUMTEXT	
		|	LONGTEXT	
		)
		(BINARY )?
		(charset )?
		(collate )?
	|	(	ENUM	
		|	SET		
		)
		LPAREN values+=STRING (COMMA values+=STRING)* RPAREN 
		(charset )?
		(collate )?
	;

charset
	:	CHARACTER SET
		(	ID 
		|	STRING 
		)
	;

collate
	:	COLLATE 
		(	ID 
		|	STRING 
		)
	;
	
reference_definition
	:	RESTRICT	
	|	CASCADE		
	|	SET NULL	
	|	NO ACTION	
	;
// $> DDL
