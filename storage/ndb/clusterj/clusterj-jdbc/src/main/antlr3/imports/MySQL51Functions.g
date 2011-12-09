/*
 *  Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.
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

parser grammar MySQL51Functions;

options {
	output=AST;
}

functionCall
	:	/* builtin functions  */
	  AVG LPAREN expr RPAREN     -> ^(FUNC ^(AVG expr))
	|	BIT_AND LPAREN expr RPAREN		-> ^(FUNC ^(BIT_AND expr))
	|	BIT_OR LPAREN expr RPAREN		-> ^(FUNC ^(BIT_OR expr))
	|	BIT_XOR LPAREN expr RPAREN		-> ^(FUNC ^(BIT_XOR expr))
	|	CAST LPAREN expr AS cast_data_type RPAREN				-> ^(FUNC ^(CAST expr cast_data_type))
	| CONCAT LPAREN exprList RPAREN  -> ^(FUNC ^(CONCAT exprList))
	|	COUNT LPAREN MULT RPAREN		-> ^(FUNC ^(COUNT_STAR[$MULT] ))
	|	COUNT LPAREN expr RPAREN		-> ^(FUNC ^(COUNT expr ))
	|	COUNT LPAREN DISTINCT exprList RPAREN		-> ^(FUNC ^(COUNT exprList DISTINCT))
	|	DATE_ADD LPAREN date=expr COMMA INTERVAL interval=expr timeUnit RPAREN				-> ^(FUNC ^(DATE_ADD $date $interval timeUnit))
	|	DATE_SUB LPAREN date=expr COMMA INTERVAL interval=expr timeUnit RPAREN		-> ^(FUNC ^(DATE_SUB $date $interval timeUnit))
	|	GROUP_CONCAT LPAREN 
			DISTINCT? exprList
			order_by?
			(SEPARATOR text_string)?
		RPAREN									-> ^(FUNC ^(GROUP_CONCAT exprList DISTINCT? order_by? text_string?))
	|	MAX LPAREN DISTINCT? expr RPAREN		-> ^(FUNC ^(MAX expr DISTINCT? ))
	// MID is an alias for SUBSTR(str, pos, len)
	|	MID LPAREN expr COMMA expr COMMA expr RPAREN		-> ^(FUNC ^(MID expr+))
	|	MIN LPAREN DISTINCT? expr RPAREN		-> ^(FUNC ^(MIN expr DISTINCT? ))
	// SESSION_USER is an alias for USER
	|	SESSION_USER LPAREN RPAREN		-> ^(FUNC ^(SESSION_USER LPAREN))
	|	STD LPAREN expr RPAREN		-> ^(FUNC ^(STD expr))
	|	STDDEV LPAREN expr RPAREN		-> ^(FUNC ^(STDDEV expr))
	|	STDDEV_POP LPAREN expr RPAREN		-> ^(FUNC ^(STDDEV_POP expr))
	|	STDDEV_SAMP LPAREN expr RPAREN		-> ^(FUNC ^(STDDEV_SAMP expr))
	|	SUM LPAREN DISTINCT? expr RPAREN		-> ^(FUNC ^(SUM expr DISTINCT? ))
	// SYSTEM_USER is an alias for USER
	|	SYSTEM_USER LPAREN RPAREN		-> ^(FUNC ^(SYSTEM_USER LPAREN))
	|	TRIM LPAREN
			(	(pos=BOTH | pos=LEADING | pos=TRAILING)
				remstr=expr? FROM str=expr		-> ^(FUNC ^(TRIM $str $pos? $remstr?))
			|	str1=expr
				(	FROM str2=expr				-> ^(FUNC ^(TRIM $str2 $str1))
				|	/* empty */					-> ^(FUNC ^(TRIM $str1))
				)
			)
			RPAREN
	|	VARIANCE LPAREN expr RPAREN		-> ^(FUNC ^(VARIANCE expr))
	|	VAR_POP LPAREN expr RPAREN		-> ^(FUNC ^(VAR_POP expr))
	|	VAR_SAMP LPAREN expr RPAREN		-> ^(FUNC ^(VAR_SAMP expr))

/* non-keywords */
	|	ADDDATE LPAREN date=expr COMMA 
		(	(INTERVAL)=> INTERVAL interval=expr timeUnit RPAREN		-> ^(FUNC ^(ADDDATE $date $interval timeUnit))
		|	days=expr RPAREN		-> ^(FUNC ^(ADDDATE $date $days))
		)
	|	CURDATE LPAREN RPAREN		-> ^(FUNC ^(CURDATE LPAREN))
	|	CURRENT_DATE (LPAREN RPAREN)? -> ^(FUNC ^(CURRENT_DATE LPAREN?))
	|	CURTIME LPAREN RPAREN			-> ^(FUNC ^(CURTIME LPAREN))
	|	CURRENT_TIME (LPAREN RPAREN)?	-> ^(FUNC ^(CURRENT_TIME LPAREN?))
	|	EXTRACT LPAREN timeUnit FROM expr RPAREN		-> ^(FUNC ^(EXTRACT timeUnit expr))
	|	GET_FORMAT LPAREN 
			(type=DATE | type=TIME | type=DATETIME)
			locale=expr
		RPAREN								-> ^(FUNC ^(GET_FORMAT $type $locale))
	|	NOW LPAREN RPAREN					-> ^(FUNC ^(NOW LPAREN))
	|	CURRENT_TIMESTAMP (LPAREN RPAREN)?	-> ^(FUNC ^(CURRENT_TIMESTAMP LPAREN?))
	|	POSITION LPAREN substr=expr IN str=expr RPAREN		-> ^(FUNC ^(POSITION $substr $str))		// todo: LOCATE as well?
	|	SUBDATE LPAREN date=expr COMMA
			(	days=expr			-> ^(FUNC ^(SUBDATE $date $days))
			|	(INTERVAL)=>INTERVAL interval=expr timeUnit	-> ^(FUNC ^(SUBDATE $date $interval timeUnit))
			)
		RPAREN
	// SUBSTR is an alias for SUBSTRING
	|	(funcName=SUBSTR | funcName=SUBSTRING) LPAREN 
			str=expr 
			(	COMMA position=expr COMMA len=expr? 				-> ^(FUNC ^($funcName $str $position $len?))
			|	FROM position=expr (FOR len=expr)? 		-> ^(FUNC ^($funcName $str $position $len? FROM?))
			)
			RPAREN
	|	SYSDATE LPAREN RPAREN				-> ^(FUNC ^(SYSDATE LPAREN))
	|	TIMESTAMP_ADD LPAREN
			timestampUnit
			interval=expr
			date=expr
			RPAREN								-> ^(FUNC ^(TIMESTAMP_ADD $date $interval timestampUnit))
	|	TIMESTAMP_DIFF LPAREN
			timestampUnit
			date1=expr
			date2=expr
			RPAREN								-> ^(FUNC ^(TIMESTAMP_DIFF $date1 $date2 timestampUnit))
	|	UTC_DATE (LPAREN RPAREN)?				-> ^(FUNC ^(UTC_DATE LPAREN?))
	|	UTC_TIMESTAMP (LPAREN RPAREN)?			-> ^(FUNC ^(UTC_TIMESTAMP LPAREN?))
	|	UTC_TIME (LPAREN RPAREN)?				-> ^(FUNC ^(UTC_TIME LPAREN?))

	|	functionCall_conflicts
	|	functionCall_reserved

	/* generic functions we don't know */
	|	name=ID args=parenOptExprList						-> ^(FUNC ^($name $args))
	;

functionCall_conflicts
	:
	/* conflict with keywords, or geometry functions */
		ASCII LPAREN expr RPAREN		-> ^(FUNC ^(ASCII expr))
	|	CHARSET LPAREN expr RPAREN		-> ^(FUNC ^(CHARSET expr))
	|	COALESCE LPAREN exprList RPAREN		-> ^(FUNC ^(COALESCE exprList))
	|	COLLATION LPAREN expr RPAREN		-> ^(FUNC ^(COLLATION expr))
	|	CONTAINS LPAREN e1=expr COMMA e2=expr RPAREN		-> ^(FUNC ^(CONTAINS $e1 $e2))	// geom
	|	DATABASE LPAREN RPAREN		-> ^(FUNC ^(DATABASE LPAREN))
	|	GEOMETRYCOLLECTION LPAREN exprList RPAREN		-> ^(FUNC ^(GEOMETRYCOLLECTION exprList))	// geom
	|	IF LPAREN e1=expr COMMA e2=expr COMMA e3=expr RPAREN	-> ^(FUNC ^(IF $e1 $e2 $e3))
	|	LINESTRING LPAREN exprList RPAREN		-> ^(FUNC ^(LINESTRING exprList))	// geom
	|	MICROSECOND LPAREN expr RPAREN		-> ^(FUNC ^(MICROSECOND expr))
	|	MOD LPAREN e1=expr COMMA e2=expr RPAREN		-> ^(FUNC ^(MOD $e1 $e2))
	|	MULTILINESTRING LPAREN exprList RPAREN		-> ^(FUNC ^(MULTILINESTRING exprList))	// geom
	|	MULTIPOINT LPAREN exprList RPAREN		-> ^(FUNC ^(MULTIPOINT exprList))	// geom
	|	MULTIPOLYGON LPAREN exprList RPAREN		-> ^(FUNC ^(MULTIPOLYGON exprList))	// geom
	|	OLD_PASSWORD LPAREN expr RPAREN		-> ^(FUNC ^(OLD_PASSWORD expr))
	|	PASSWORD LPAREN expr RPAREN		-> ^(FUNC ^(PASSWORD expr))
	|	POINT LPAREN x=expr COMMA y=expr RPAREN		-> ^(FUNC ^(POINT $x $y))	// geom
	|	POLYGON LPAREN exprList RPAREN		-> ^(FUNC ^(POLYGON exprList))	// geom
	|	QUARTER LPAREN expr RPAREN		-> ^(FUNC ^(QUARTER expr))
	|	REPEAT LPAREN str=expr COMMA count=expr RPAREN		-> ^(FUNC ^(REPEAT $str $count))
	|	REPLACE LPAREN str=expr COMMA from=expr COMMA to=expr RPAREN		-> ^(FUNC ^(REPLACE $str $from $to))
	|	TRUNCATE LPAREN num=expr COMMA decimals=expr RPAREN		-> ^(FUNC ^(TRUNCATE $num $decimals))
	|	WEEK LPAREN date=expr (COMMA mode=expr)? RPAREN		-> ^(FUNC ^(WEEK $date $mode?))
	;

functionCall_reserved
	:	/* keywords that can also be function names */
		CHAR LPAREN exprList (USING (charsetname=ID|charsetname=STRING))? RPAREN		-> ^(FUNC ^(CHAR exprList $charsetname?))	// todo: implement proper charset name handling
	|	CURRENT_USER (LPAREN RPAREN)?							-> ^(FUNC ^(CURRENT_USER LPAREN?))
	|	DATE LPAREN expr RPAREN		-> ^(FUNC ^(DATE expr))
	|	DAY LPAREN expr RPAREN		-> ^(FUNC ^(DAY expr))
	|	HOUR LPAREN expr RPAREN		-> ^(FUNC ^(HOUR expr))
	|	INSERT LPAREN 
			str=expr COMMA pos=expr COMMA len=expr COMMA newstr=expr
		RPAREN						-> ^(FUNC ^(INSERT $str $pos $len $newstr))
	// this is not the time INTERVAL operation!
	|	INTERVAL LPAREN exprList RPAREN		-> ^(FUNC ^(INTERVAL exprList))
	|	LEFT LPAREN expr RPAREN		-> ^(FUNC ^(LEFT expr))
	|	MINUTE LPAREN expr RPAREN		-> ^(FUNC ^(MINUTE expr))
	|	MONTH LPAREN expr RPAREN		-> ^(FUNC ^(MONTH expr))
	|	RIGHT LPAREN expr RPAREN		-> ^(FUNC ^(RIGHT expr))
	|	SECOND LPAREN expr RPAREN		-> ^(FUNC ^(SECOND expr))
	|	TIME LPAREN expr RPAREN		-> ^(FUNC ^(TIME expr))
	|	TIMESTAMP LPAREN expr RPAREN		-> ^(FUNC ^(TIMESTAMP expr))
	|	USER LPAREN RPAREN		-> ^(FUNC ^(USER LPAREN))
	|	YEAR LPAREN expr RPAREN		-> ^(FUNC ^(YEAR expr))
	;
