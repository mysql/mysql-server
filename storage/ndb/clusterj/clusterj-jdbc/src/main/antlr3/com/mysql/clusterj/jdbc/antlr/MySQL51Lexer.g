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

lexer grammar MySQL51Lexer; 

options {
	superClass=MySQLLexer;
}

@header{
package com.mysql.clusterj.jdbc.antlr;
}

// $<Keywords 

/* These are actual reserved words that cannot be used as an identifier without quotes.
   eventually we need to gate all these with a predicate (see nextTokenIsID) to allow the "word following a '.' character is always an identifier" rule
 */
ACCESSIBLE	:	'ACCESSIBLE';
ADD	:	'ADD';
ALL	:	'ALL';
ALTER	:	'ALTER';
ANALYZE	:	'ANALYZE';
AND	:	'AND';
AS	:	'AS';
ASC	:	'ASC';
ASENSITIVE	:	'ASENSITIVE';
BEFORE	:	'BEFORE';
BETWEEN	:	'BETWEEN';
// BIGINT	:	'BIGINT';		// datatype defined below 
BINARY	:	'BINARY';
//BLOB	:	'BLOB';		// datatype defined below 
BOTH	:	'BOTH';
BY	:	'BY';
CALL	:	'CALL';
CASCADE	:	'CASCADE';
CASE	:	'CASE';
CHANGE	:	'CHANGE';
//CHAR	:	'CHAR';		// datatype defined below 
CHARACTER	:	'CHARACTER';
CHECK	:	'CHECK';
COLLATE	:	'COLLATE';
COLUMN	:	'COLUMN';
CONDITION	:	'CONDITION';
CONSTRAINT	:	'CONSTRAINT';
CONTINUE	:	'CONTINUE';
CONVERT	:	'CONVERT';
CREATE	:	'CREATE';
CROSS	:	'CROSS';
CURRENT_DATE	:	'CURRENT_DATE';
CURRENT_TIME	:	'CURRENT_TIME';
CURRENT_TIMESTAMP	:	'CURRENT_TIMESTAMP';
//CURRENT_USER	:	'CURRENT_USER';	//reserved, function below
CURSOR	:	'CURSOR';
DATABASE	:	'DATABASE';
DATABASES	:	'DATABASES';
DAY_HOUR	:	'DAY_HOUR';
DAY_MICROSECOND	:	'DAY_MICROSECOND';
DAY_MINUTE	:	'DAY_MINUTE';
DAY_SECOND	:	'DAY_SECOND';
DEC	:	'DEC';
//DECIMAL	:	'DECIMAL';		// datatype defined below 
DECLARE	:	'DECLARE';
DEFAULT	:	'DEFAULT';
DELAYED	:	'DELAYED';
DELETE	:	'DELETE';
DESC	:	'DESC';
DESCRIBE	:	'DESCRIBE';
DETERMINISTIC	:	'DETERMINISTIC';
DISTINCT	:	'DISTINCT';
DISTINCTROW	:	'DISTINCTROW';
DIV	:	'DIV';
//DOUBLE	:	'DOUBLE';		// datatype defined below 
DROP	:	'DROP';
DUAL	:	'DUAL';
EACH	:	'EACH';
ELSE	:	'ELSE';
ELSEIF	:	'ELSEIF';
ENCLOSED	:	'ENCLOSED';
ESCAPED	:	'ESCAPED';
EXISTS	:	'EXISTS';
EXIT	:	'EXIT';
EXPLAIN	:	'EXPLAIN';
FALSE	:	'FALSE';
FETCH	:	'FETCH';
//FLOAT	:	'FLOAT';		// datatype defined below 
FLOAT4	:	'FLOAT4';
FLOAT8	:	'FLOAT8';
FOR	:	'FOR';
FORCE	:	'FORCE';
FOREIGN	:	'FOREIGN';
FROM	:	'FROM';
FULLTEXT	:	'FULLTEXT';
GOTO	:	'GOTO';
GRANT	:	'GRANT';
GROUP	:	'GROUP';
HAVING	:	'HAVING';
HIGH_PRIORITY	:	'HIGH_PRIORITY';
HOUR_MICROSECOND	:	'HOUR_MICROSECOND';
HOUR_MINUTE	:	'HOUR_MINUTE';
HOUR_SECOND	:	'HOUR_SECOND';
IF	:	'IF';
IGNORE	:	'IGNORE';
IN	:	'IN';
INDEX	:	'INDEX';
INDEX_SYM  :    'INDEX_SYM';
INFILE	:	'INFILE';
INNER	:	'INNER';
INNODB  : 'INNODB';
INOUT	:	'INOUT';
INSENSITIVE	:	'INSENSITIVE';
//INSERT	:	'INSERT';	// reserved keyword and function below
//INT	:	'INT';		// datatype defined below 
INT1	:	'INT1';
INT2	:	'INT2';
INT3	:	'INT3';
INT4	:	'INT4';
INT8	:	'INT8';
//INTEGER	:	'INTEGER';		// datatype defined below 
//INTERVAL	:	'INTERVAL';		// reserved keyword and function below
INTO	:	'INTO';
IS	:	'IS';
ITERATE	:	'ITERATE';
JOIN	:	'JOIN';
KEY	:	'KEY';
KEYS	:	'KEYS';
KILL	:	'KILL';
LABEL	:	'LABEL';
LEADING	:	'LEADING';
LEAVE	:	'LEAVE';
//LEFT	:	'LEFT';	// reserved keyword and function below
LIKE	:	'LIKE';
LIMIT	:	'LIMIT';
LINEAR	:	'LINEAR';
LINES	:	'LINES';
LOAD	:	'LOAD';
LOCALTIME	:	'LOCALTIME';
LOCALTIMESTAMP	:	'LOCALTIMESTAMP';
LOCK	:	'LOCK';
LONG	:	'LONG';
//LONGBLOB	:	'LONGBLOB';		// datatype defined below 
//LONGTEXT	:	'LONGTEXT';		// datatype defined below 
LOOP	:	'LOOP';
LOW_PRIORITY	:	'LOW_PRIORITY';
MASTER_SSL_VERIFY_SERVER_CERT	:	'MASTER_SSL_VERIFY_SERVER_CERT';
MATCH	:	'MATCH';
//MEDIUMBLOB	:	'MEDIUMBLOB';		// datatype defined below 
//MEDIUMINT	:	'MEDIUMINT';		// datatype defined below 
//MEDIUMTEXT	:	'MEDIUMTEXT';		// datatype defined below 
MIDDLEINT	:	'MIDDLEINT';		// datatype defined below 
MINUTE_MICROSECOND	:	'MINUTE_MICROSECOND';
MINUTE_SECOND	:	'MINUTE_SECOND';
MOD	:	'MOD';
MODIFIES	:	'MODIFIES';
NATURAL	:	'NATURAL';
NOT	:	'NOT';
NO_WRITE_TO_BINLOG	:	'NO_WRITE_TO_BINLOG';
NULL	:	'NULL';
//NUMERIC	:	'NUMERIC';		// datatype defined below 
ON	:	'ON';
OPTIMIZE	:	'OPTIMIZE';
OPTION	:	'OPTION';
OPTIONALLY	:	'OPTIONALLY';
OR	:	'OR';
ORDER	:	'ORDER';
OUT	:	'OUT';
OUTER	:	'OUTER';
OUTFILE	:	'OUTFILE';
PRECISION	:	'PRECISION';
PRIMARY	:	'PRIMARY';
PROCEDURE	:	'PROCEDURE';
PURGE	:	'PURGE';
RANGE	:	'RANGE';
READ	:	'READ';
READS	:	'READS';
READ_ONLY	:	'READ_ONLY';
READ_WRITE	:	'READ_WRITE';
//REAL	:	'REAL';		// datatype defined below 
REFERENCES	:	'REFERENCES';
REGEXP	:	'REGEXP';
RELEASE	:	'RELEASE';
RENAME	:	'RENAME';
REPEAT	:	'REPEAT';
REPLACE	:	'REPLACE';
REQUIRE	:	'REQUIRE';
RESTRICT	:	'RESTRICT';
RETURN	:	'RETURN';
REVOKE	:	'REVOKE';
//RIGHT	:	'RIGHT';	// reserved keyword and function below
RLIKE	:	'RLIKE';
SCHEDULER : 'SCHEDULER';
SCHEMA	:	'SCHEMA';
SCHEMAS	:	'SCHEMAS';
SECOND_MICROSECOND	:	'SECOND_MICROSECOND';
SELECT	:	'SELECT';
SENSITIVE	:	'SENSITIVE';
SEPARATOR	:	'SEPARATOR';
SET	:	'SET';
SHOW	:	'SHOW';
//SMALLINT	:	'SMALLINT';		// datatype defined below 
SPATIAL	:	'SPATIAL';
SPECIFIC	:	'SPECIFIC';
SQL	:	'SQL';
SQLEXCEPTION	:	'SQLEXCEPTION';
SQLSTATE	:	'SQLSTATE';
SQLWARNING	:	'SQLWARNING';
SQL_BIG_RESULT	:	'SQL_BIG_RESULT';
SQL_CALC_FOUND_ROWS	:	'SQL_CALC_FOUND_ROWS';
SQL_SMALL_RESULT	:	'SQL_SMALL_RESULT';
SSL	:	'SSL';
STARTING	:	'STARTING';
STRAIGHT_JOIN	:	'STRAIGHT_JOIN';
TABLE	:	'TABLE';
TERMINATED	:	'TERMINATED';
THEN	:	'THEN';
//TINYBLOB	:	'TINYBLOB';		// datatype defined below 
//TINYINT	:	'TINYINT';		// datatype defined below 
//TINYTEXT	:	'TINYTEXT';		// datatype defined below 
TO	:	'TO';
TRAILING	:	'TRAILING';
TRIGGER	:	'TRIGGER';
TRUE	:	'TRUE';
UNDO	:	'UNDO';
UNION	:	'UNION';
UNIQUE	:	'UNIQUE';
UNLOCK	:	'UNLOCK';
UNSIGNED	:	'UNSIGNED';
UPDATE	:	'UPDATE';
USAGE	:	'USAGE';
USE	:	'USE';
USING	:	'USING';
//UTC_DATE	:	'UTC_DATE';		// next three are functions defined below
//UTC_TIME	:	'UTC_TIME';
//UTC_TIMESTAMP	:	'UTC_TIMESTAMP';
VALUES	:	'VALUES';
//VARBINARY	:	'VARBINARY';		// datatype defined below 
//VARCHAR	:	'VARCHAR';		// datatype defined below 
VARCHARACTER	:	'VARCHARACTER';
VARYING	:	'VARYING';
WHEN	:	'WHEN';
WHERE	:	'WHERE';
WHILE	:	'WHILE';
WITH	:	'WITH';
WRITE	:	'WRITE';
XOR	:	'XOR';
YEAR_MONTH	:	'YEAR_MONTH';
ZEROFILL	:	'ZEROFILL';

// $> Keywords


// $< Keywords allowed as identifiers
/* the following two lists are taken from the official YACC grammar
 * is contains all keywords that are allowed to be used as identifiers
 * without quoting them.
 */
/* Keyword that we allow for identifiers (except SP labels) */

// $< Keywords not in SP labels

ASCII	:	'ASCII';             
BACKUP	:	'BACKUP';            
BEGIN	:	'BEGIN';             
BYTE	:	'BYTE';              
CACHE	:	'CACHE';             
CHARSET	:	'CHARSET';               
CHECKSUM	:	'CHECKSUM';          
CLOSE	:	'CLOSE';             
COMMENT	:	'COMMENT';           
COMMIT	:	'COMMIT';            
CONTAINS	:	'CONTAINS';          
DEALLOCATE	:	'DEALLOCATE';        
DO	:	'DO';                
END	:	'END';                   
EXECUTE	:	'EXECUTE';           
FLUSH	:	'FLUSH';             
HANDLER	:	'HANDLER';           
HELP	:	'HELP';              
HOST	:	'HOST';              
INSTALL	:	'INSTALL';           
LANGUAGE	:	'LANGUAGE';          
NO	:	'NO';                
OPEN	:	'OPEN';              
OPTIONS	:	'OPTIONS';           
OWNER	:	'OWNER';             
PARSER	:	'PARSER';            
PARTITION	:	'PARTITION';         
PORT	:	'PORT';              
PREPARE	:	'PREPARE';           
REMOVE	:	'REMOVE';            
REPAIR	:	'REPAIR';                
RESET	:	'RESET';             
RESTORE	:	'RESTORE';           
ROLLBACK	:	'ROLLBACK';          
SAVEPOINT	:	'SAVEPOINT';         
SECURITY	:	'SECURITY';          
SERVER	:	'SERVER';            
SIGNED	:	'SIGNED';            
SOCKET	:	'SOCKET';            
SLAVE	:	'SLAVE';                 
SONAME	:	'SONAME';            
START	:	'START';             
STOP	:	'STOP';              
TRUNCATE	:	'TRUNCATE';
UNICODE	:	'UNICODE';           
UNINSTALL	:	'UNINSTALL';         
WRAPPER	:	'WRAPPER';           
XA	:	'XA';                
UPGRADE	:	'UPGRADE';           

// $> Keywords not in SP labels

// $< Keywords in SP labels
/* Keywords that we allow for labels in SPs. */
ACTION	:	'ACTION';                   
//ADDDATE	:	'ADDDATE';              // function defined below 
AFTER	:	'AFTER';                
AGAINST	:	'AGAINST';                  
AGGREGATE	:	'AGGREGATE';            
ALGORITHM	:	'ALGORITHM';            
ANY	:	'ANY';                  
AT	:	'AT';                   
AUTHORS	:	'AUTHORS';              
AUTO_INCREMENT	:	'AUTO_INCREMENT';                 
AUTOEXTEND_SIZE	:	'AUTOEXTEND_SIZE';      
AVG : 'AVG';                  
AVG_ROW_LENGTH	:	'AVG_ROW_LENGTH';           
BINLOG	:	'BINLOG';               
// BIT	:	'BIT';                  // datatype defined below
BLOCK	:	'BLOCK';                
BOOL	:	'BOOL';                 
BOOLEAN	:	'BOOLEAN';              
BTREE	:	'BTREE';                
CASCADED	:	'CASCADED';                 
CHAIN	:	'CHAIN';                
CHANGED	:	'CHANGED';                  
CIPHER	:	'CIPHER';               
CLIENT	:	'CLIENT';               
COALESCE	:	'COALESCE';                 
CODE	:	'CODE';                 
COLLATION	:	'COLLATION';            
COLUMNS	:	'COLUMNS';
// FIELDS is a synonym for COLUMNS
FIELDS	:	'FIELDS';
COMMITTED	:	'COMMITTED';            
COMPACT	:	'COMPACT';              
COMPLETION	:	'COMPLETION';           
COMPRESSED	:	'COMPRESSED';           
CONCURRENT	:	'CONCURRENT';               
CONNECTION	:	'CONNECTION';           
CONSISTENT	:	'CONSISTENT';           
CONTEXT	:	'CONTEXT';              
CONTRIBUTORS	:	'CONTRIBUTORS';         
CPU	:	'CPU';                  
CUBE	:	'CUBE';                 
DATA	:	'DATA';                 
DATAFILE	:	'DATAFILE';             
// DATETIME	:	'DATETIME';                 // datatype defined below
// DATE	:	'DATE';                 		// datatype defined below
//DAY	:	'DAY';                  // reserved, is also function below
DEFINER	:	'DEFINER';              
DELAY_KEY_WRITE	:	'DELAY_KEY_WRITE';      
DES_KEY_FILE	:	'DES_KEY_FILE';             
DIRECTORY	:	'DIRECTORY';            
DISABLE	:	'DISABLE';              
DISCARD	:	'DISCARD';                  
DISK	:	'DISK';                 
DUMPFILE	:	'DUMPFILE';                 
DUPLICATE	:	'DUPLICATE';            
DYNAMIC	:	'DYNAMIC';              
ENDS	:	'ENDS';                 
//ENUM	:	'ENUM';          		// datatype defined below           
ENGINE	:	'ENGINE';               
ENGINES	:	'ENGINES';              
ERRORS	:	'ERRORS';                   
ESCAPE	:	'ESCAPE';               
EVENT	:	'EVENT';                
EVENTS	:	'EVENTS';               
EVERY	:	'EVERY';                
EXPANSION	:	'EXPANSION';            
EXTENDED	:	'EXTENDED';             
EXTENT_SIZE	:	'EXTENT_SIZE';          
FAULTS	:	'FAULTS';               
FAST	:	'FAST';                 
FOUND	:	'FOUND';                
ENABLE	:	'ENABLE';               
FULL	:	'FULL';                     
FILE	:	'FILE';                 
FIRST	:	'FIRST';                
FIXED	:	'FIXED';                
FRAC_SECOND	:	'FRAC_SECOND';          
GEOMETRY	:	'GEOMETRY';             
GEOMETRYCOLLECTION	:	'GEOMETRYCOLLECTION';
//GET_FORMAT	:	'GET_FORMAT';               //function defined below
GRANTS	:	'GRANTS';                   
GLOBAL	:	'GLOBAL';               
HASH	:	'HASH';                 
HOSTS	:	'HOSTS';                
//HOUR	:	'HOUR';                 //reserved, also function below
IDENTIFIED	:	'IDENTIFIED';           
INVOKER	:	'INVOKER';              
IMPORT	:	'IMPORT';                   
INDEXES	:	'INDEXES';                  
INITIAL_SIZE	:	'INITIAL_SIZE';         
IO	:	'IO';                   
IPC	:	'IPC';                  
ISOLATION	:	'ISOLATION';                
ISSUER	:	'ISSUER';               
INNOBASE	:	'INNOBASE';             
INSERT_METHOD	:	'INSERT_METHOD';            
KEY_BLOCK_SIZE	:	'KEY_BLOCK_SIZE';           
LAST	:	'LAST';                 
LEAVES	:	'LEAVES';                   
LESS	:	'LESS';                 
LEVEL	:	'LEVEL';                
LINESTRING	:	'LINESTRING';               // geometry function
LIST	:	'LIST';                 
LOCAL	:	'LOCAL';                
LOCKS	:	'LOCKS';                
LOGFILE	:	'LOGFILE';              
LOGS	:	'LOGS';                 
MAX_ROWS	:	'MAX_ROWS';                 
MASTER	:	'MASTER';               
MASTER_HOST	:	'MASTER_HOST';          
MASTER_PORT	:	'MASTER_PORT';          
MASTER_LOG_FILE	:	'MASTER_LOG_FILE';      
MASTER_LOG_POS	:	'MASTER_LOG_POS';       
MASTER_USER	:	'MASTER_USER';          
MASTER_PASSWORD	:	'MASTER_PASSWORD';      
MASTER_SERVER_ID	:	'MASTER_SERVER_ID';     
MASTER_CONNECT_RETRY	:	'MASTER_CONNECT_RETRY'; 
MASTER_SSL	:	'MASTER_SSL';           
MASTER_SSL_CA	:	'MASTER_SSL_CA';        
MASTER_SSL_CAPATH	:	'MASTER_SSL_CAPATH';    
MASTER_SSL_CERT	:	'MASTER_SSL_CERT';      
MASTER_SSL_CIPHER	:	'MASTER_SSL_CIPHER';    
MASTER_SSL_KEY	:	'MASTER_SSL_KEY';       
MAX_CONNECTIONS_PER_HOUR	:	'MAX_CONNECTIONS_PER_HOUR'; 
MAX_QUERIES_PER_HOUR	:	'MAX_QUERIES_PER_HOUR';     
MAX_SIZE	:	'MAX_SIZE';             
MAX_UPDATES_PER_HOUR	:	'MAX_UPDATES_PER_HOUR';     
MAX_USER_CONNECTIONS	:	'MAX_USER_CONNECTIONS'; 
MAX_VALUE	:	'MAX_VALUE';            
MEDIUM	:	'MEDIUM';               
MEMORY	:	'MEMORY';               
MERGE	:	'MERGE';                
MICROSECOND	:	'MICROSECOND';          
MIGRATE	:	'MIGRATE';              
//MINUTE	:	'MINUTE';               // also function
MIN_ROWS	:	'MIN_ROWS';                 
MODIFY	:	'MODIFY';               
MODE	:	'MODE';                 
//MONTH	:	'MONTH';         		// also function       
MULTILINESTRING	:	'MULTILINESTRING';
MULTIPOINT	:	'MULTIPOINT';               
MULTIPOLYGON	:	'MULTIPOLYGON';             
MUTEX	:	'MUTEX';                
NAME	:	'NAME';                 
NAMES	:	'NAMES';                
NATIONAL	:	'NATIONAL';             
NCHAR	:	'NCHAR';                
NDBCLUSTER	:	'NDBCLUSTER';           
NEXT	:	'NEXT';                 
NEW	:	'NEW';                  
NO_WAIT	:	'NO_WAIT';              
NODEGROUP	:	'NODEGROUP';            
NONE	:	'NONE';                 
NVARCHAR	:	'NVARCHAR';             
OFFSET	:	'OFFSET';               
OLD_PASSWORD	:	'OLD_PASSWORD';             
ONE_SHOT	:	'ONE_SHOT';             
ONE	:	'ONE';                  
PACK_KEYS	:	'PACK_KEYS';            
PAGE	:	'PAGE';                 
PARTIAL	:	'PARTIAL';                  
PARTITIONING	:	'PARTITIONING';         
PARTITIONS	:	'PARTITIONS';           
PASSWORD	:	'PASSWORD';                 
PHASE	:	'PHASE';                
PLUGIN	:	'PLUGIN';               
PLUGINS	:	'PLUGINS';              
POINT	:	'POINT';                
POLYGON	:	'POLYGON';                  
PRESERVE	:	'PRESERVE';             
PREV	:	'PREV';                 
PRIVILEGES	:	'PRIVILEGES';               
PROCESS	:	'PROCESS';                  
PROCESSLIST	:	'PROCESSLIST';          
PROFILE	:	'PROFILE';              
PROFILES	:	'PROFILES';             
QUARTER	:	'QUARTER';              
QUERY	:	'QUERY';                
QUICK	:	'QUICK';                    
REBUILD	:	'REBUILD';              
RECOVER	:	'RECOVER';              
REDO_BUFFER_SIZE	:	'REDO_BUFFER_SIZE';     
REDOFILE	:	'REDOFILE';             
REDUNDANT	:	'REDUNDANT';            
RELAY_LOG_FILE	:	'RELAY_LOG_FILE';       
RELAY_LOG_POS	:	'RELAY_LOG_POS';        
RELAY_THREAD	:	'RELAY_THREAD';             
RELOAD	:	'RELOAD';                   
REORGANIZE	:	'REORGANIZE';           
REPEATABLE	:	'REPEATABLE';           
REPLICATION	:	'REPLICATION';              
RESOURCES	:	'RESOURCES';                
RESUME	:	'RESUME';               
RETURNS	:	'RETURNS';              
ROLLUP	:	'ROLLUP';               
ROUTINE	:	'ROUTINE';              
ROWS	:	'ROWS';                 
ROW_FORMAT	:	'ROW_FORMAT';           
ROW	:	'ROW';                  
RTREE	:	'RTREE';                
SCHEDULE	:	'SCHEDULE';             
//SECOND	:	'SECOND';               		// also function
SERIAL	:	'SERIAL';               
SERIALIZABLE	:	'SERIALIZABLE';         
SESSION	:	'SESSION';              
SIMPLE	:	'SIMPLE';               
SHARE	:	'SHARE';                
SHUTDOWN	:	'SHUTDOWN';                 
SNAPSHOT	:	'SNAPSHOT';
SOME:	'SOME';		// alias for ANY             
SOUNDS	:	'SOUNDS';               
SOURCE	:	'SOURCE';               
SQL_CACHE	:	'SQL_CACHE';            
SQL_BUFFER_RESULT	:	'SQL_BUFFER_RESULT';        
SQL_NO_CACHE	:	'SQL_NO_CACHE';         
SQL_THREAD	:	'SQL_THREAD';               
STARTS	:	'STARTS';               
STATUS	:	'STATUS';               
STORAGE	:	'STORAGE';              
STRING_KEYWORD	:	'STRING'; //  this is not a string but the keyword STRING used as a return value for UDF
//SUBDATE	:	'SUBDATE';              // function defined below
SUBJECT	:	'SUBJECT';              
SUBPARTITION	:	'SUBPARTITION';         
SUBPARTITIONS	:	'SUBPARTITIONS';        
SUPER	:	'SUPER';                
SUSPEND	:	'SUSPEND';              
SWAPS	:	'SWAPS';                
SWITCHES	:	'SWITCHES';             
TABLES	:	'TABLES';                   
TABLESPACE	:	'TABLESPACE';               
TEMPORARY	:	'TEMPORARY';                
TEMPTABLE	:	'TEMPTABLE';            
//TEXT	:	'TEXT';                 // datatype defined below
THAN	:	'THAN';                 
TRANSACTION	:	'TRANSACTION';          
TRANSACTIONAL	:	'TRANSACTIONAL';        
TRIGGERS	:	'TRIGGERS';             
//TIMESTAMP	:	'TIMESTAMP';                // datatype defined below
//TIMESTAMP_ADD	:	'TIMESTAMP_ADD';            // function defined below
//TIMESTAMP_DIFF	:	'TIMESTAMP_DIFF';           // function defined below
//TIME	:	'TIME';                 // datatype defined below
TYPES	:	'TYPES';                
TYPE	:	('TYPE' (WS|EOF))=> 'TYPE';
UDF_RETURNS	:	'UDF_RETURNS';          
FUNCTION	:	'FUNCTION';             
UNCOMMITTED	:	'UNCOMMITTED';          
UNDEFINED	:	'UNDEFINED';            
UNDO_BUFFER_SIZE	:	'UNDO_BUFFER_SIZE';     
UNDOFILE	:	'UNDOFILE';             
UNKNOWN	:	'UNKNOWN';              
UNTIL	:	'UNTIL';                
//USER	:	'USER';                     		// also function
USE_FRM	:	'USE_FRM';                  
VARIABLES	:	'VARIABLES';                
VIEW	:	'VIEW';                 
VALUE	:	'VALUE';                
WARNINGS	:	'WARNINGS';                 
WAIT	:	'WAIT';                 
WEEK	:	'WEEK';                 // also function
WORK	:	'WORK';                 
X509	:	'X509';                 
//YEAR	:	'YEAR';                 // datatype defined below

// $> Keywords in SP labels

// $> Keywords allowed as identifiers

// $< Punctuation
COMMA	:	',';
DOT		:	'.';
SEMI	:	';';
LPAREN	:	'(';
RPAREN	:	')';
LCURLY	:	'{';
RCURLY	:	'}';
// $> Punctuation

// $< Builtin SQL Functions

/** functions must be directly followed by '(' to be considered a keyword (and thus a function name)
 * TODO: this is the place to support the SQL mode IGNORE_SPACE
 */
//ADDDATE	:	'ADDDATE';	// duplicate from the keywords list
BIT_AND	:	'BIT_AND' {$type = checkFunctionAsID($type);};
BIT_OR	:	'BIT_OR' {$type = checkFunctionAsID($type);};
BIT_XOR	:	'BIT_XOR' {$type = checkFunctionAsID($type);};
CAST	:	'CAST' {$type = checkFunctionAsID($type);};
COUNT	:	'COUNT' {$type = checkFunctionAsID($type);};
//CURDATE	:	'CURDATE';	//below
//CURTIME	:	'CURTIME';	//below
DATE_ADD	:	'DATE_ADD' {$type = checkFunctionAsID($type);};
DATE_SUB	:	'DATE_SUB' {$type = checkFunctionAsID($type);};
//EXTRACT	:	'EXTRACT';	//below
GROUP_CONCAT	:	'GROUP_CONCAT' {$type = checkFunctionAsID($type);};
MAX	:	'MAX' {$type = checkFunctionAsID($type);};
MID	:	'MID' {$type = checkFunctionAsID($type);};
MIN	:	'MIN' {$type = checkFunctionAsID($type);};
//NOW	:	'NOW';	//below
//POSITION	:	'POSITION';	//below
SESSION_USER	:	'SESSION_USER' {$type = checkFunctionAsID($type);};
STD	:	'STD' {$type = checkFunctionAsID($type);};
STDDEV	:	'STDDEV' {$type = checkFunctionAsID($type);};
STDDEV_POP	:	'STDDEV_POP' {$type = checkFunctionAsID($type);};
STDDEV_SAMP	:	'STDDEV_SAMP' {$type = checkFunctionAsID($type);};
//SUBDATE	:	'SUBDATE';			// duplicate from the keywords list
SUBSTR	:	'SUBSTR' {$type = checkFunctionAsID($type);};
//SUBSTRING	:	'SUBSTRING';	//below
SUM	:	'SUM' {$type = checkFunctionAsID($type);};
// SYSDATE	:	'SYSDATE';	//below
SYSTEM_USER	:	'SYSTEM_USER' {$type = checkFunctionAsID($type);};
//TRIM	:	'TRIM';	//below
VARIANCE	:	'VARIANCE' {$type = checkFunctionAsID($type);};
VAR_POP	:	'VAR_POP' {$type = checkFunctionAsID($type);};
VAR_SAMP	:	'VAR_SAMP' {$type = checkFunctionAsID($type);};

/* non-keywords */
ADDDATE	:	'ADDDATE' {$type = checkFunctionAsID($type);};
CURDATE	:	'CURDATE' {$type = checkFunctionAsID($type);};
CURTIME	:	'CURTIME' {$type = checkFunctionAsID($type);};
DATE_ADD_INTERVAL	:	'DATE_ADD_INTERVAL' {$type = checkFunctionAsID($type);};
DATE_SUB_INTERVAL	:	'DATE_SUB_INTERVAL' {$type = checkFunctionAsID($type);};
EXTRACT	:	'EXTRACT' {$type = checkFunctionAsID($type);};
GET_FORMAT	:	'GET_FORMAT' {$type = checkFunctionAsID($type);};
NOW	:	'NOW' {$type = checkFunctionAsID($type);};
POSITION	:	'POSITION' {$type = checkFunctionAsID($type);};
SUBDATE	:	'SUBDATE' {$type = checkFunctionAsID($type);};
SUBSTRING	:	'SUBSTRING' {$type = checkFunctionAsID($type);};
SYSDATE	:	'SYSDATE' {$type = checkFunctionAsID($type);};
TIMESTAMP_ADD	:	'TIMESTAMP_ADD' {$type = checkFunctionAsID($type);};
TIMESTAMP_DIFF	:	'TIMESTAMP_DIFF' {$type = checkFunctionAsID($type);};
UTC_DATE	:	'UTC_DATE' {$type = checkFunctionAsID($type);};
UTC_TIMESTAMP	:	'UTC_TIMESTAMP' {$type = checkFunctionAsID($type);};
UTC_TIME	:	'UTC_TIME' {$type = checkFunctionAsID($type);};

/* conflict with keywords, or geometry functions */
// the following keywords are handled by the ident parser rule where they are allowed as identifiers 
// and are special cased in function_call
//ASCII	:	'ASCII' {$type = checkFunctionAsID($type);};	// ascii is special in other places, too. 
//CHARSET	:	'CHARSET' {$type = checkFunctionAsID($type);};
//COALESCE	:	'COALESCE' {$type = checkFunctionAsID($type);};
//COLLATION	:	'COLLATION' {$type = checkFunctionAsID($type);};
//CONTAINS	:	'CONTAINS' {$type = checkFunctionAsID($type);};
//DATABASE	:	'DATABASE' {$type = checkFunctionAsID($type);};
//GEOMETRYCOLLECTION	:	'GEOMETRYCOLLECTION' {$type = checkFunctionAsID($type);};
// IF is a function and reserved, thus it cannot appear unquoted in any other context
//LINESTRING	:	'LINESTRING' {$type = checkFunctionAsID($type);};
//MICROSECOND	:	'MICROSECOND' {$type = checkFunctionAsID($type);};
// MOD can both be a function or an operator and is reserved, thus it cannot appear unquoted in any other context
//MULTILINESTRING	:	'MULTILINESTRING' {$type = checkFunctionAsID($type);};
//MULTIPOINT	:	'MULTIPOINT' {$type = checkFunctionAsID($type);};
//MULTIPOLYGON	:	'MULTIPOLYGON' {$type = checkFunctionAsID($type);};
//OLD_PASSWORD	:	'OLD_PASSWORD' {$type = checkFunctionAsID($type);};
//PASSWORD	:	'PASSWORD' {$type = checkFunctionAsID($type);};
//POINT	:	'POINT' {$type = checkFunctionAsID($type);};
//POLYGON	:	'POLYGON' {$type = checkFunctionAsID($type);};
//QUARTER	:	'QUARTER' {$type = checkFunctionAsID($type);};
//REPEAT	:	'REPEAT' {$type = checkFunctionAsID($type);};
//REPLACE	:	'REPLACE' {$type = checkFunctionAsID($type);};
//TRUNCATE	:	'TRUNCATE' {$type = checkFunctionAsID($type);};
//WEEK	:	'WEEK' {$type = checkFunctionAsID($type);};

/* keywords that can also be function names */
CHAR	:	'CHAR'; // reserved
CURRENT_USER	:	'CURRENT_USER';// reserved
DATE	:	'DATE' {$type = checkFunctionAsID($type);};
DAY	:	'DAY'; // {$type = checkFunctionAsID($type);}; // not affected by IGNORE_SPACE since 5.1.13
HOUR	:	'HOUR' {$type = checkFunctionAsID($type);};
INSERT	:	'INSERT'; // reserved
INTERVAL	:	'INTERVAL'; // reserved
LEFT	:	'LEFT'; // reserved
MINUTE	:	'MINUTE' {$type = checkFunctionAsID($type);};
MONTH	:	'MONTH' {$type = checkFunctionAsID($type);};
RIGHT	:	'RIGHT'; // reserved
SECOND	:	'SECOND' {$type = checkFunctionAsID($type);};
TIME	:	'TIME' {$type = checkFunctionAsID($type);};
TIMESTAMP	:	'TIMESTAMP' {$type = checkFunctionAsID($type);};
TRIM	:	'TRIM' {$type = checkFunctionAsID($type);};
USER	:	'USER' {$type = checkFunctionAsID($type);};
YEAR	:	'YEAR' {$type = checkFunctionAsID($type);};


// $> Builtin SQL Functions

// $< Operators

/**
Operator Precedence Table from the 5.1 docs:

BINARY, COLLATE
!
- (unary minus), ~ (unary bit inversion)
^
*, /, DIV, %, MOD
-, +
<<, >>
&
|
=, <=>, >=, >, <=, <, <>, !=, IS, LIKE, REGEXP, IN
BETWEEN, CASE, WHEN, THEN, ELSE
NOT
&&, AND
XOR
||, OR
:=
*/

ASSIGN		:	':=';
PLUS		:	'+';
MINUS 		:	'-';
MULT		:	'*';
DIVISION	:	'/';
MODULO		:	'%';
BITWISE_XOR	:	'^';
BITWISE_INVERSION	:	'~';
BITWISE_AND	:	'&';
LOGICAL_AND	:	'&&';
BITWISE_OR	:	'|';
LOGICAL_OR	:	'||';
LESS_THAN	:	'<';
LEFT_SHIFT	:	'<<';
LESS_THAN_EQUAL	:	'<=';
NULL_SAFE_NOT_EQUAL	:	'<=>';
EQUALS		:	'=';
NOT_OP		:	'!';
NOT_EQUAL	:	'<>' | '!=';
GREATER_THAN:	'>';
RIGHT_SHIFT	:	'>>';
GREATER_THAN_EQUAL	:	'>=';
// $> Operators

// $< Data types
BIGINT	:	'BIGINT';
BIT	:	'BIT';
BLOB	:	'BLOB';
//CHAR	:	'CHAR';		// also function
//DATE	:	'DATE';		// also function
DATETIME	:	'DATETIME';
DECIMAL	:	'DECIMAL';
DOUBLE	:	'DOUBLE';
ENUM	:	'ENUM';
FLOAT	:	'FLOAT';
INT	:	'INT';
INTEGER	:	'INTEGER';
LONGBLOB	:	'LONGBLOB';
LONGTEXT	:	'LONGTEXT';
MEDIUMBLOB	:	'MEDIUMBLOB';
MEDIUMINT	:	'MEDIUMINT';
MEDIUMTEXT	:	'MEDIUMTEXT';
NUMERIC	:	'NUMERIC';
REAL	:	'REAL';
SMALLINT	:	'SMALLINT';
TEXT	:	'TEXT';
//TIME	:	'TIME';		// also function
//TIMESTAMP	:	'TIMESTAMP';		// also function
TINYBLOB	:	'TINYBLOB';
TINYINT	:	'TINYINT';
TINYTEXT	:	'TINYTEXT';
VARBINARY	:	'VARBINARY';
VARCHAR	:	'VARCHAR';
//YEAR	:	'YEAR';		// also function
// $> Data types



// $< Generic Tokens

/**
 * Values like b'1011' or B'1011'.
 * Even though binary values look like strings, they are of a special data type. The initial 'b' can both upper and lower case.
 * The quote character must be the single quote ('\''), whitespace is not allowed between 'b'|'B' and the quoted binary value.
 */
BINARY_VALUE
	:	('B' '\'')=> 'B\'' ('0'|'1')* '\''
	;

HEXA_VALUE
	:	('X' '\'')=> 'X\'' (DIGIT|'A'|'B'|'C'|'D'|'E'|'F')* '\''
	;	

/*
 * Character sets and collations are handled in the parser, as both allow whitespace between the string and the modifiers.
 * The n and N 'national character set' (UTF8 for MySQL 4.1 and up) modifiers do _not_ allow whitespace and thus must be handled here.
 *
 * Quoting quotes: By doubling the quote character used to start the string, you can quote that character itself:
 *        Input such as  "He said:""Foo!""" results in the single token STRING with the text: < He said:"Foo!" >
 * Alternatively the quote character can be escaped using the standard escape character ('\').
 *
 * Binary and hexadecimal are syntactically equivalent to strings but have a different meaning and should not be tokenized as STRING. See below.
 */
fragment
STRING
	:	'N'?			// "introducer" for the national character set (UTF8 for MySQL 4.1 and up). must immediately precede the first quote character.
		(	'"' 
			(	('""')=> '""'
//			|	(ESCAPE_SEQUENCE)=> ESCAPE_SEQUENCE
			|	~('"'|'\\')
			)*
			'"'	// TODO: collapse two consecutive internal double quotes into one
		|	'\''
			(	('\'\'')=> '\'\''
//			|	(ESCAPE_SEQUENCE)=> ESCAPE_SEQUENCE
			|	~('\''|'\\')
			)*
			'\''	// TODO: same as above with single quotes
		)
	;


/**
 * UNDERSCORE_ID is a bad hack because for character sets we need an identifier which starts with '_'.
 * The bad thing is, if the actual identifier is not a valid character set name it does not start a string, even though it looks it does.
 * In that case, if it is a select_expr, it is specifying an alias for a column. Note that this ambiguity only arises with unquoted identifiers,
 * as character set modifiers for strings can never be quoted.
 */
/*UNDERSCORE_ID
	:	{input.LA(1) == '_'}? => REAL_ID
	;
*/
/* user@host, ID and SESSION_VARIABLES are ambiguous */
USER_HOST_or_ID_or_STRING
  : ID  {$type=ID;}
    ( USER_HOST {$type=USER_HOST;} )?
  | STRING {$type=STRING;} (USER_HOST {$type=USER_HOST;})?
  ;

fragment
USER_HOST
  : '@' (ID | STRING)
  ;

fragment
ID  : '`' (options{greedy=false;}: (~('`'))+) '`'
  | REAL_ID
  ;

fragment
REAL_ID
	:	('A'..'Z'|'_') ('0'..'9'|'A'..'Z'|'_')*		// TODO: what are the valid characters?
	;



// TODO: these are case sensitive -> specifying them as lowercase in the grammar causes them to never be matched (because ANTLR doesn't know
// we are only serving uppercase letters. Add trueCaseLA predicates here (but beware of hoisting)
// TODO: this rule is broken; it is to parse Java source files not compiled strings.
// The entire rule should be removed...
//fragment
//ESCAPE_SEQUENCE
//	:	'\\'
//		(	'0'
//		|	'\''
//		|	'"'
//		|	'b'
//		|	'n'		// TODO currently this clashes with \N == NULL. add predicate!
//		|	'r'
//		|	't'
//		|	'Z'		// this is UPPERCASE! -> use ANTLRNoCaseStringStream.trueCaseLA() in predicate to resolve
//		|	'\\'
//		|	'%'
//		|	'_'
//              |    character=.     // TODO: collapse into just $char; this might be an error
//		)
//	;
		
fragment
DIGIT
	:	'0'..'9'
	;


/**
 * alternatives are delineated because otherwise NUMBER would be an epsilon transition, matching endlessly on a syntax error
 * unary minus|plus must be handled in the parser
 */
NUMBER
	:	
		(	DIGIT+			// 1434
		|	DOT DIGIT+		// .02343
		|	DIGIT+ DOT DIGIT*	// 13212.	or	12334.234234
		)
		('E' DIGIT+)?
	;

// $< Comments
// DASHDASH_COMMENTS are special in the sense that they require the '--' to be followed by whitespace. If there's no whitespace '--' should be lexed as MINUS MINUS
COMMENT_RULE
	:	(	C_COMMENT 
		|	POUND_COMMENT
		|	{input.LA(3)==' ' || input.LA(3) == '\t' || input.LA(3) == '\n' || input.LA(3) == '\r'}?=> DASHDASH_COMMENT
		)
		{$channel=98;}
	;

fragment
C_COMMENT
	:	'/*' ( options {greedy=false;} : . )* '*/'
	;
	
fragment
POUND_COMMENT
	:	'#' ~('\n'|'\r')* '\r'? '\n'
	;

fragment
DASHDASH_COMMENT
	:	'--' (' ' | '\t' | '\n' | '\r') ~('\n'|'\r')* '\r'? '\n'
	;
// $> Comments
GLOBAL_VARIABLE
	:	'@@' ID
	;
/* todo: user variables can be quoted, thus ID is wrong here */
SESSION_VARIABLE
	:	'@' ID
	;

WS	:	(' ' | '\t' | '\n' | '\r')+ { $channel=HIDDEN; }
	;

/**
 * for normalized queries all values should have been replaced with a '?' character.
 * Tokenize that as special.
 */
VALUE_PLACEHOLDER 
	:	'?'
	;
// $> Generic Tokens
