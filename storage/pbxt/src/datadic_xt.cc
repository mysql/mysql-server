/* Copyright (c) 2005 PrimeBase Technologies GmbH
 *
 * PrimeBase XT
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * 2006-05-16	Paul McCullagh
 *
 * H&G2JCtL
 *
 * Implementation of the PBXT internal data dictionary.
 */


#include "xt_config.h"

#ifdef DRIZZLED
#include <bitset>
#endif

#include <ctype.h>
#include <errno.h>

#ifdef DEBUG
#ifdef DRIZZLED
#include <drizzled/common_includes.h>
#else
#include "mysql_priv.h"
#endif
#endif

#include "pthread_xt.h"
#include "datadic_xt.h"
#include "util_xt.h"
#include "database_xt.h"
#include "table_xt.h"
#include "heap_xt.h"
#include "strutil_xt.h"
#include "myxt_xt.h"
#include "hashtab_xt.h"

/*
 * -----------------------------------------------------------------------
 * Lexical analyser
 */

#define XT_TK_EOF				0
#define XT_TK_IDENTIFIER		1
#define XT_TK_NUMBER			2
#define XT_TK_STRING			3
#define XT_TK_PUNCTUATION		4

#define XT_TK_RESERVER_WORDS	5
#define XT_TK_PRIMARY			5
#define XT_TK_UNIQUE			6
#define XT_TK_FULLTEXT			7
#define XT_TK_SPATIAL			8
#define XT_TK_INDEX				9
#define XT_TK_KEY				10
#define XT_TK_CHECK				11
#define XT_TK_FOREIGN			12
#define XT_TK_COLUMN			13
#define XT_TK_REFERENCES		14
#define XT_TK_NOT				15
#define XT_TK_NULL				16
#define XT_TK_AUTO_INCREMENT	17
#define XT_TK_COMMENT			18
#define XT_TK_DEFAULT			19
#define XT_TK_COLLATE			20

class XTToken {
	public:	
	u_int	tk_type;
	char	*tk_text;
	size_t	tk_length;

	void initCString(u_int type, char *start, char *end);
	inline char charAt(u_int i) {
		if (i >= tk_length)
			return 0;
		return toupper(tk_text[i]);
	}
	void expectKeyWord(XTThreadPtr self, c_char *keyword);
	void expectIdentifier(XTThreadPtr self);
	void expectNumber(XTThreadPtr self);
	bool isKeyWord(c_char *keyword);
	bool isReservedWord();
	bool isReservedWord(u_int word);
	void identifyReservedWord();
	bool isEOF();
	bool isIdentifier();
	bool isNumber();
	size_t getString(char *string, size_t len);
	void getTokenText(char *string, size_t len);
	XTToken *clone(XTThreadPtr self);
};

void XTToken::initCString(u_int type, char *start, char *end)
{
	tk_type = type;
	tk_text = start;
	tk_length = (size_t) end - (size_t) start;
}

bool XTToken::isKeyWord(c_char *keyword)
{
	char	*str = tk_text;
	size_t	len = tk_length;
	
	while (len && *keyword) {
		if (toupper(*keyword) != toupper(*str))
			return false;
		keyword++;
		str++;
		len--;
	}
	return !len && !*keyword;
}

bool XTToken::isReservedWord()
{
	return tk_type >= XT_TK_RESERVER_WORDS;
}

bool XTToken::isReservedWord(u_int word)
{
	return tk_type == word;
}

void XTToken::identifyReservedWord()
{
	if (tk_type == XT_TK_IDENTIFIER) {
		switch (charAt(0)) {
			case 'A':
				if (isKeyWord("AUTO_INCREMENT"))
					tk_type = XT_TK_AUTO_INCREMENT;
				break;
			case 'C':
				switch (charAt(2)) {
					case 'E':
						if (isKeyWord("CHECK"))
							tk_type = XT_TK_CHECK;
						break;
					case 'L':
						if (isKeyWord("COLUMN"))
							tk_type = XT_TK_COLUMN;
						else if (isKeyWord("COLLATE"))
							tk_type = XT_TK_COLLATE;
						break;
					case 'M':
						if (isKeyWord("COMMENT"))
							tk_type = XT_TK_COMMENT;
						break;
				}
				break;
			case 'D':
				if (isKeyWord("DEFAULT"))
					tk_type = XT_TK_DEFAULT;
				break;
			case 'F':
				switch (charAt(1)) {
					case 'O':
						if (isKeyWord("FOREIGN"))
							tk_type = XT_TK_FOREIGN;
						break;
					case 'U':
						if (isKeyWord("FULLTEXT"))
							tk_type = XT_TK_FULLTEXT;
						break;
				}
				break;
			case 'I':
				if (isKeyWord("INDEX"))
					tk_type = XT_TK_INDEX;
				break;
			case 'K':
				if (isKeyWord("KEY"))
					tk_type = XT_TK_KEY;
				break;
			case 'N':
				switch (charAt(1)) {
					case 'O':
						if (isKeyWord("NOT"))
							tk_type = XT_TK_NOT;
						break;
					case 'U':
						if (isKeyWord("NULL"))
							tk_type = XT_TK_NULL;
						break;
				}
				break;
			case 'P':
				if (isKeyWord("PRIMARY"))
					tk_type = XT_TK_PRIMARY;
				break;
			case 'R':
				if (isKeyWord("REFERENCES"))
					tk_type = XT_TK_REFERENCES;
				break;
			case 'S':
				if (isKeyWord("SPATIAL"))
					tk_type = XT_TK_SPATIAL;
				break;
			case 'U':
				if (isKeyWord("UNIQUE"))
					tk_type = XT_TK_UNIQUE;
				break;			
		}
	}
}

bool XTToken::isEOF()
{
	return tk_type == XT_TK_EOF;
}

bool XTToken::isIdentifier()
{
	return tk_type == XT_TK_IDENTIFIER;
}

bool XTToken::isNumber()
{
	return tk_type == XT_TK_NUMBER;
}

/* Return actual, or required string length. */
size_t XTToken::getString(char *dtext, size_t dsize)
{
	char	*buffer = dtext;
	int		slen;
	size_t	dlen;
	char	*stext;
	char	quote;

	if ((slen = (int) tk_length) == 0) {
		*dtext = 0;
		return 0;
	}
	switch (*tk_text) {
		case '\'':
		case '"':
		case '`':
			quote = *tk_text;
			stext = tk_text+1;
			slen -= 2;
			dlen = 0;
			while (slen > 0) {
				if (*stext == '\\') {
					stext++;
					slen--;
					if (slen > 0) {
						switch (*stext) {
							case '\0':
								*dtext = 0;
								break;
							case '\'':
								*dtext = '\'';
								break;
							case '"':
								*dtext = '"';
								break;
							case 'b':
								*dtext = '\b';
								break;
							case 'n':
								*dtext = '\n';
								break;
							case 'r':
								*dtext = '\r';
								break;
							case 't':
								*dtext = '\t';
								break;
							case 'z':
								*dtext = (char) 26;
								break;
							case '\\':
								*dtext = '\\';
								break;
							default:
								*dtext = *stext;
								break;
						}
					}
				}
				else if (*stext == quote) {
					if (dlen < dsize)
						*dtext = quote;
					stext++;
					slen--;
				}
				else {
					if (dlen < dsize)
						*dtext = *stext;
				}
				dtext++;
				dlen++;
				stext++;
				slen--;
			}
			if (dlen < dsize)
				buffer[dlen] = 0;
			else if (dsize > 0)
				buffer[dsize-1] = 0;
			break;
		default:
			if (dsize > 0) {
				dlen = dsize-1;
				if ((int) dlen > slen)
					dlen = slen;
				memcpy(dtext, tk_text, dlen);
				dtext[dlen] = 0;
			}
			dlen = tk_length;
			break;
	}
	return dlen;
}

/* Return the token as a string with ... in it if it is too long
 */
void XTToken::getTokenText(char *string, size_t size)
{
	if (tk_length == 0 || !tk_text) {
		xt_strcpy(size, string, "EOF");
		return;
	}

	size--;
	if (tk_length <= size) {
		memcpy(string, tk_text, tk_length);
		string[tk_length] = 0;
		return;
	}
	
	size = (size - 3) / 2;
	memcpy(string, tk_text, size);
	memcpy(string+size, "...", 3);
	memcpy(string+size+3, tk_text + tk_length - size, size);
	string[size+3+size] = 0;
}

XTToken *XTToken::clone(XTThreadPtr self)
{
	XTToken *tk;

	if (!(tk = new XTToken()))
		xt_throw_errno(XT_CONTEXT, XT_ENOMEM);
	tk->initCString(tk_type, tk_text, tk_text + tk_length);
	return tk;
}

void XTToken::expectKeyWord(XTThreadPtr self, c_char *keyword)
{
	char	buffer[100];

	if (isKeyWord(keyword))
		return;
	getTokenText(buffer, 100);
	xt_throw_i2xterr(XT_CONTEXT, XT_ERR_A_EXPECTED_NOT_B, keyword, buffer);
}

void XTToken::expectIdentifier(XTThreadPtr self)
{
	char buffer[100];

	if (isIdentifier())
		return;
	getTokenText(buffer, 100);
	xt_throw_i2xterr(XT_CONTEXT, XT_ERR_A_EXPECTED_NOT_B, "Identifier", buffer);
}

void XTToken::expectNumber(XTThreadPtr self)
{
	char buffer[100];

	if (isNumber())
		return;
	getTokenText(buffer, 100);
	xt_throw_i2xterr(XT_CONTEXT, XT_ERR_A_EXPECTED_NOT_B, "Value", buffer);
}

struct charset_info_st;

class XTTokenizer {
	struct charset_info_st	*tkn_charset;
	char					*tkn_cstring;
	char					*tkn_curr_pos;
	XTToken					*tkn_current;
	bool					tkn_in_comment;

	public:

	XTTokenizer(bool convert, char *cstring) {
		tkn_charset = myxt_getcharset(convert);
		tkn_cstring = cstring;
		tkn_curr_pos = cstring;
		tkn_current = NULL;
		tkn_in_comment = FALSE;
	}

	virtual ~XTTokenizer(void) {
		if (tkn_current)
			delete tkn_current;
	}

	inline bool isSingleChar(int ch)
	{
		return  ch != '$' && ch != '_' && myxt_ispunct(tkn_charset, ch);
	}

	inline bool isIdentifierChar(int ch)
	{
		return  ch && !isSingleChar(ch) && !myxt_isspace(tkn_charset, ch);
	}

	inline bool isNumberChar(int ch, int next_ch)
	{
		return myxt_isdigit(tkn_charset, ch) || ((ch == '-' || ch == '+') && myxt_isdigit(tkn_charset, next_ch));
	}

	XTToken *newToken(XTThreadPtr self, u_int type, char *start, char *end);
	XTToken *nextToken(XTThreadPtr self);
	XTToken *nextToken(XTThreadPtr self, c_char *keyword, XTToken *tk);
};

void ri_free_token(XTThreadPtr XT_UNUSED(self), XTToken *tk)
{
	delete tk;
}

XTToken *XTTokenizer::newToken(XTThreadPtr self, u_int type, char *start, char *end)
{
	if (!tkn_current) {
		if (!(tkn_current = new XTToken()))
			xt_throw_errno(XT_CONTEXT, XT_ENOMEM);
	}
	tkn_current->initCString(type, start, end);
	if (type == XT_TK_IDENTIFIER)
		tkn_current->identifyReservedWord();
	return tkn_current;
}

XTToken *XTTokenizer::nextToken(XTThreadPtr self)
{
	char	*token_start;
	u_int	token_type = XT_TK_PUNCTUATION;
	char	quote;
	bool	must_be_num;

	restart:

	/* Ignore space: */
	while (*tkn_curr_pos && myxt_isspace(tkn_charset, *tkn_curr_pos)) tkn_curr_pos++;

	token_start = tkn_curr_pos;
	switch (*tkn_curr_pos) {
		case '\0':
			return newToken(self, XT_TK_EOF, NULL, NULL);
		// Comment: # ... EOL
		case '#':
			tkn_curr_pos++;
			while (*tkn_curr_pos && *tkn_curr_pos != '\n' && *tkn_curr_pos != '\r') tkn_curr_pos++;
			goto restart;
		case '-':
			if (tkn_curr_pos[1] == '-') {
				// Comment: -- ... EOL
				while (*tkn_curr_pos && *tkn_curr_pos != '\n' && *tkn_curr_pos != '\r') tkn_curr_pos++;
				goto restart;
			}
			if (myxt_isdigit(tkn_charset, tkn_curr_pos[1]))
				goto is_number;
			tkn_curr_pos++;
			break;
		case '+':
			if (myxt_isdigit(tkn_charset, tkn_curr_pos[1]))
				goto is_number;
			tkn_curr_pos++;
			break;
		case '/':
			tkn_curr_pos++;
			if (*tkn_curr_pos == '*') {
				// Comment: /* ... */
				// Look for: /*!99999 ... */  version conditional statements
				tkn_curr_pos++;
				if (*tkn_curr_pos == '!') {
					tkn_curr_pos++;
					if (isdigit(*tkn_curr_pos)) {
						while (isdigit(*tkn_curr_pos))
							tkn_curr_pos++;
						tkn_in_comment = true;
						goto restart;
					}
				}

				while (*tkn_curr_pos && !(*tkn_curr_pos == '*' && *(tkn_curr_pos+1) == '/')) tkn_curr_pos++;
				if (*tkn_curr_pos == '*' && *(tkn_curr_pos+1) == '/')
					tkn_curr_pos += 2;
				goto restart;
			}
			break;
		case '\'':
			token_type = XT_TK_STRING;
			goto is_string;
		case '"':
		case '`':
			token_type = XT_TK_IDENTIFIER;
			is_string:
			quote = *tkn_curr_pos;
			tkn_curr_pos++;
			while (*tkn_curr_pos) {
				if (*tkn_curr_pos == quote) {
					// Doubling the quote means stay in string...
					if (*(tkn_curr_pos + 1) != quote)
						break;
					tkn_curr_pos++;
				}
				/* TODO: Unless sql_mode == 'NO_BACKSLASH_ESCAPES'!!! */
				if (*tkn_curr_pos == '\\') {
					if (*(tkn_curr_pos+1) == quote) {
						if (quote == '"' || quote == '\'')
							tkn_curr_pos++;
					}
				}
				tkn_curr_pos++;
			}
			
			if (*tkn_curr_pos == quote)
				tkn_curr_pos++;
			break;
		case '$':
			goto is_identifier;
		case '*':
			if (tkn_in_comment) {
				if (tkn_curr_pos[1] == '/') {
					tkn_in_comment = false;
					tkn_curr_pos += 2;
					goto restart;
				}
			}
			/* No break required! */
		default:
			if (isNumberChar(tkn_curr_pos[0], tkn_curr_pos[1]))
				goto is_number;

			if (isSingleChar(*tkn_curr_pos)) {
				token_type = XT_TK_PUNCTUATION;
				// The rest are singles...
				tkn_curr_pos++;
				break;
			}
			
			is_identifier:
			// Identifier (any string of characters that is not punctuation or a space:
			token_type = XT_TK_IDENTIFIER;
			while (isIdentifierChar(*tkn_curr_pos))
				tkn_curr_pos++;
			break;

			is_number:
			must_be_num = false;
			token_type = XT_TK_NUMBER;

			if (*tkn_curr_pos == '-' || *tkn_curr_pos == '+') {
				must_be_num = true;
				tkn_curr_pos++;
			}

			// Number: 9999 [ . 9999 ] [ e/E [+/-] 9999 ]
			// However, 9999e or 9999E is an identifier!
			while (*tkn_curr_pos && myxt_isdigit(tkn_charset, *tkn_curr_pos)) tkn_curr_pos++;
			
			if (*tkn_curr_pos == '.') {
				must_be_num = true;
				tkn_curr_pos++;
				while (*tkn_curr_pos && myxt_isdigit(tkn_charset, *tkn_curr_pos)) tkn_curr_pos++;
			}

			if (*tkn_curr_pos == 'e' || *tkn_curr_pos == 'E') {
				tkn_curr_pos++;

				if (isNumberChar(tkn_curr_pos[0], tkn_curr_pos[1])) {
					must_be_num = true;

					if (*tkn_curr_pos == '-' || *tkn_curr_pos == '+')
						tkn_curr_pos++;
					while (*tkn_curr_pos && myxt_isdigit(tkn_charset, *tkn_curr_pos))
						tkn_curr_pos++;
				}
				else if (!must_be_num)
					token_type = XT_TK_IDENTIFIER;
			}

			if (must_be_num || !isIdentifierChar(*tkn_curr_pos))
				break;

			/* Crazy, but true. An identifier can start by looking like a number! */
			goto is_identifier;
	}

	return newToken(self, token_type, token_start, tkn_curr_pos);
}

XTToken *XTTokenizer::nextToken(XTThreadPtr self, c_char *keyword, XTToken *tk)
{
	tk->expectKeyWord(self, keyword);
	return nextToken(self);
}

/*
 * -----------------------------------------------------------------------
 * Parser
 */

/*
	We must parse the following syntax. Note that the constraints
	may be embedded in a CREATE TABLE/ALTER TABLE statement.

	[CONSTRAINT symbol] FOREIGN KEY [id] (index_col_name, ...)
    REFERENCES tbl_name (index_col_name, ...)
    [ON DELETE {RESTRICT | CASCADE | SET NULL | SET DEFAULT | NO ACTION}]
    [ON UPDATE {RESTRICT | CASCADE | SET NULL | SET DEFAULT | NO ACTION}]
*/

class XTParseTable : public XTObject {
	public:	
	void raiseError(XTThreadPtr self, XTToken *tk, int err);

	private:
	XTTokenizer			*pt_tokenizer;
	XTToken				*pt_current;
	XTStringBufferRec	pt_sbuffer;

	void syntaxError(XTThreadPtr self, XTToken *tk);	

	void parseIdentifier(XTThreadPtr self, char *name);
	int parseKeyAction(XTThreadPtr self);	
	void parseCreateTable(XTThreadPtr self);
	void parseAddTableItem(XTThreadPtr self);
	void parseQualifiedName(XTThreadPtr self, char *parent_name, char *name);
	void parseTableName(XTThreadPtr self, bool alterTable);
	void parseExpression(XTThreadPtr self, bool allow_reserved);
	void parseBrackets(XTThreadPtr self);
	void parseMoveColumn(XTThreadPtr self);
	
	/* If old_col_name is NULL, then this column is to be added,
	 * if old_col_name is empty (strlen() = 0) then the column
	 * exists, and should be modified, otherwize the column
	 * given is to be modified.
	 */
	void parseColumnDefinition(XTThreadPtr self, char *old_col_name);
	void parseDataType(XTThreadPtr self);
	void parseReferenceDefinition(XTThreadPtr self, u_int req_cols);
	void optionalIndexName(XTThreadPtr self);
	void optionalIndexType(XTThreadPtr self);
	u_int columnList(XTThreadPtr self, bool index_cols);
	void parseAlterTable(XTThreadPtr self);	
	void parseCreateIndex(XTThreadPtr self);
	void parseDropIndex(XTThreadPtr self);

	public:	
	XTParseTable() {
		pt_tokenizer = NULL;
		pt_current = NULL;
		memset(&pt_sbuffer, 0, sizeof(XTStringBufferRec));
	}

	virtual void finalize(XTThreadPtr XT_UNUSED(self)) {
		if (pt_tokenizer)
			delete pt_tokenizer;
		xt_sb_set_size(NULL, &pt_sbuffer, 0);
	}

	// Hooks to receive output from the parser:
	virtual void setTableName(XTThreadPtr XT_UNUSED(self), char *XT_UNUSED(name), bool XT_UNUSED(alterTable)) {
	}
	virtual void addColumn(XTThreadPtr XT_UNUSED(self), char *XT_UNUSED(col_name), char *XT_UNUSED(old_col_name)) {
	}
	virtual void setDataType(XTThreadPtr self, char *cstring) {
		if (cstring) 
			xt_free(self, cstring);
	}
	virtual void setNull(XTThreadPtr XT_UNUSED(self), bool XT_UNUSED(nullOK)) {
	}
	virtual void setAutoInc(XTThreadPtr XT_UNUSED(self), bool XT_UNUSED(autoInc)) {
	}
	
	/* Add a contraint. If lastColumn is TRUE then add the contraint 
	 * to the last column. If not, expect addListedColumn() to be called.
	 */
	virtual void addConstraint(XTThreadPtr XT_UNUSED(self), char *XT_UNUSED(name), u_int XT_UNUSED(type), bool XT_UNUSED(lastColumn)) {
	}
	
	/* Move the last column created. If symbol is NULL then move the column to the
	 * first position, else move it to the position just after the given column.
	 */
	virtual void moveColumn(XTThreadPtr XT_UNUSED(self), char *XT_UNUSED(col_name)) {
	}

	virtual void dropColumn(XTThreadPtr XT_UNUSED(self), char *XT_UNUSED(col_name)) {
	}

	virtual void dropConstraint(XTThreadPtr XT_UNUSED(self), char *XT_UNUSED(name), u_int XT_UNUSED(type)) {
	}

	virtual void setIndexName(XTThreadPtr XT_UNUSED(self), char *XT_UNUSED(name)) {
	}
	virtual void addListedColumn(XTThreadPtr XT_UNUSED(self), char *XT_UNUSED(index_col_name)) {
	}
	virtual void setReferencedTable(XTThreadPtr XT_UNUSED(self), char *XT_UNUSED(ref_schema), char *XT_UNUSED(ref_table)) {
	}
	virtual void addReferencedColumn(XTThreadPtr XT_UNUSED(self), char *XT_UNUSED(index_col_name)) {
	}
	virtual void setActions(XTThreadPtr XT_UNUSED(self), int XT_UNUSED(on_delete), int XT_UNUSED(on_update)) {
	}

	virtual void parseTable(XTThreadPtr self, bool convert, char *sql);	
};

void XTParseTable::raiseError(XTThreadPtr self, XTToken *tk, int err)
{
	char buffer[100];

	tk->getTokenText(buffer, 100);
	xt_throw_ixterr(XT_CONTEXT, err, buffer);
}

void XTParseTable::syntaxError(XTThreadPtr self, XTToken *tk)
{
	raiseError(self, tk, XT_ERR_SYNTAX);
}

void XTParseTable::parseIdentifier(XTThreadPtr self, char *name)
{
	pt_current->expectIdentifier(self);
	if (name) {
		if (pt_current->getString(name, XT_IDENTIFIER_NAME_SIZE) >= XT_IDENTIFIER_NAME_SIZE)
			raiseError(self, pt_current, XT_ERR_ID_TOO_LONG);
	}
	pt_current = pt_tokenizer->nextToken(self);
}

int XTParseTable::parseKeyAction(XTThreadPtr self)
{
	XTToken *tk;

	tk = pt_tokenizer->nextToken(self);

	if (tk->isKeyWord("RESTRICT"))
		return XT_KEY_ACTION_RESTRICT;

	if (tk->isKeyWord("CASCADE"))
		return XT_KEY_ACTION_CASCADE;

	if (tk->isKeyWord("SET")) {
		tk = pt_tokenizer->nextToken(self);
		if (tk->isKeyWord("DEFAULT"))
			return XT_KEY_ACTION_SET_DEFAULT;
		tk->expectKeyWord(self, "NULL");
		return XT_KEY_ACTION_SET_NULL;
	}

	if (tk->isKeyWord("NO")) {
		tk = pt_tokenizer->nextToken(self);
		tk->expectKeyWord(self, "ACTION");
		return XT_KEY_ACTION_NO_ACTION;
	}

	syntaxError(self, tk);
	return 0;
}

void XTParseTable::parseTable(XTThreadPtr self, bool convert, char *sql)
{
	if (pt_tokenizer)
		delete pt_tokenizer;
	pt_tokenizer = new XTTokenizer(convert, sql);
	if (!pt_tokenizer)
		xt_throw_errno(XT_CONTEXT, XT_ENOMEM);
	pt_current = pt_tokenizer->nextToken(self);

	if (pt_current->isKeyWord("CREATE")) {
		pt_current = pt_tokenizer->nextToken(self);
		if (pt_current->isKeyWord("TEMPORARY") || pt_current->isKeyWord("TABLE"))
			parseCreateTable(self);
		else
			parseCreateIndex(self);
	}
	else if (pt_current->isKeyWord("ALTER"))
		parseAlterTable(self);
	else if (pt_current->isKeyWord("DROP"))
		parseDropIndex(self);
	else if (pt_current->isKeyWord("TRUNCATE")) {
		pt_current = pt_tokenizer->nextToken(self);
		if (pt_current->isKeyWord("TABLE"))
			pt_current = pt_tokenizer->nextToken(self);
		parseTableName(self, true);
	}
	else if (pt_current->isKeyWord("OPTIMIZE") || pt_current->isKeyWord("REPAIR")) {
		/* OPTIMIZE [LOCAL | NO_WRITE_TO_BINLOG] TABLE tbl_name [, tbl_name] ...
		 *
		 * GOTCHA: This cannot work if more than one table is specified,
		 * because then I cannot find the source table?!
		 */
		pt_current = pt_tokenizer->nextToken(self);
		while (!pt_current->isEOF() && !pt_current->isKeyWord("TABLE"))
			pt_current = pt_tokenizer->nextToken(self);
		pt_current = pt_tokenizer->nextToken(self);
		parseTableName(self, true);
	}
	else
		syntaxError(self, pt_current);
}

void XTParseTable::parseCreateTable(XTThreadPtr self)
{
	if (pt_current->isKeyWord("TEMPORARY"))
		pt_current = pt_tokenizer->nextToken(self);
	pt_current = pt_tokenizer->nextToken(self, "TABLE", pt_current);
	if (pt_current->isKeyWord("IF")) {
		pt_current = pt_tokenizer->nextToken(self);
		pt_current = pt_tokenizer->nextToken(self, "NOT", pt_current);
		pt_current = pt_tokenizer->nextToken(self, "EXISTS", pt_current);
	}

	/* Table name is optional (when loading from dictionary)! */
	if (!pt_current->isKeyWord("("))
		parseTableName(self, false);
	else
		setTableName(self, NULL, false);

	/* We do not support CREATE ... SELECT! */
	if (pt_current->isKeyWord("(")) {
		pt_current = pt_tokenizer->nextToken(self);
		// Avoid this:
		// create table t3 (select group_concat(a) as a from t1 where a = 'a') union
		// (select group_concat(b) as a from t1 where a = 'b');
		if (pt_current->isKeyWord("SELECT"))
			return;
		
		/* Allow empty table definition for temporary table. */
		while (!pt_current->isEOF() && !pt_current->isKeyWord(")")) {
			parseAddTableItem(self);
			if (!pt_current->isKeyWord(","))
				break;
			pt_current = pt_tokenizer->nextToken(self);
		}
		pt_current = pt_tokenizer->nextToken(self, ")", pt_current);
	}
}

void XTParseTable::parseAddTableItem(XTThreadPtr self)
{
	char name[XT_IDENTIFIER_NAME_SIZE];

	*name = 0;
	if (pt_current->isKeyWord("CONSTRAINT")) {
		pt_current = pt_tokenizer->nextToken(self);
		if (pt_current->isIdentifier())
			parseQualifiedName(self, NULL, name);
	}

	if (pt_current->isReservedWord(XT_TK_PRIMARY)) {
		pt_current = pt_tokenizer->nextToken(self);
		pt_current = pt_tokenizer->nextToken(self, "KEY", pt_current);

		addConstraint(self, name, XT_DD_KEY_PRIMARY, false);
		optionalIndexType(self);

		/* GATCHA: Wierd?! This syntax is used in a test:
		 * alter table t1 add primary key aaa(tt);
		 */
		if (!pt_current->isKeyWord("("))
			pt_current = pt_tokenizer->nextToken(self);
		columnList(self, true);
	}
	else if (pt_current->isReservedWord(XT_TK_UNIQUE) ||
		pt_current->isReservedWord(XT_TK_FULLTEXT) ||
		pt_current->isReservedWord(XT_TK_SPATIAL) ||
		pt_current->isReservedWord(XT_TK_INDEX) ||
		pt_current->isReservedWord(XT_TK_KEY)) {
		bool is_unique = false;

		if (pt_current->isReservedWord(XT_TK_FULLTEXT) || pt_current->isReservedWord(XT_TK_SPATIAL))
			pt_current = pt_tokenizer->nextToken(self);
		else if (pt_current->isReservedWord(XT_TK_UNIQUE)) {
			pt_current = pt_tokenizer->nextToken(self);
			is_unique = true;
		}
		if (pt_current->isReservedWord(XT_TK_INDEX) || pt_current->isReservedWord(XT_TK_KEY))
			pt_current = pt_tokenizer->nextToken(self);

		addConstraint(self, name, is_unique ? XT_DD_INDEX_UNIQUE : XT_DD_INDEX, false);
		optionalIndexName(self);
		optionalIndexType(self);
		columnList(self, true);
	}
	else if (pt_current->isReservedWord(XT_TK_CHECK)) {
		pt_current = pt_tokenizer->nextToken(self);
		parseExpression(self, false);
	}
	else if (pt_current->isReservedWord(XT_TK_FOREIGN)) {
		u_int req_cols;

		pt_current = pt_tokenizer->nextToken(self);
		pt_current = pt_tokenizer->nextToken(self, "KEY", pt_current);

		addConstraint(self, name, XT_DD_KEY_FOREIGN, false);
		optionalIndexName(self);
		req_cols = columnList(self, false);
		/* GOTCHA: According the MySQL manual this is optional, but without domains,
		 * it is required!
		 */
		parseReferenceDefinition(self, req_cols);
	}
	else if (pt_current->isKeyWord("(")) {
		pt_current = pt_tokenizer->nextToken(self);
		for (;;) {
			parseColumnDefinition(self, NULL);
			if (!pt_current->isKeyWord(","))
				break;
			pt_current = pt_tokenizer->nextToken(self);
		}
		pt_current = pt_tokenizer->nextToken(self, ")", pt_current);
	}
	else {
		if (pt_current->isReservedWord(XT_TK_COLUMN))
			pt_current = pt_tokenizer->nextToken(self);
		parseColumnDefinition(self, NULL);
		parseMoveColumn(self);
	}
	/* GOTCHA: Support: create table t1 (a int not null, key `a` (a) key_block_size=1024)
	 * and any other undocumented syntax?!
	 */
	parseExpression(self, true);
}

void XTParseTable::parseExpression(XTThreadPtr self, bool allow_reserved)
{
	while (!pt_current->isEOF() && !pt_current->isKeyWord(",") &&
		!pt_current->isKeyWord(")") && (allow_reserved || !pt_current->isReservedWord())) {
		if (pt_current->isKeyWord("("))
			parseBrackets(self);
		else
			pt_current = pt_tokenizer->nextToken(self);
	}
}

void XTParseTable::parseBrackets(XTThreadPtr self)
{
	u_int cnt = 1;
	pt_current = pt_tokenizer->nextToken(self, "(", pt_current);
	while (cnt) {
		if (pt_current->isEOF())
			break;
		if (pt_current->isKeyWord("("))
			cnt++;
		if (pt_current->isKeyWord(")"))
			cnt--;
		pt_current = pt_tokenizer->nextToken(self);
	}
}

void XTParseTable::parseMoveColumn(XTThreadPtr self)
{
	if (pt_current->isKeyWord("FIRST")) {
		pt_current = pt_tokenizer->nextToken(self);
		/* If name is NULL it means move to the front. */
		moveColumn(self, NULL);
	}
	else if (pt_current->isKeyWord("AFTER")) {
		char	name[XT_IDENTIFIER_NAME_SIZE];

		pt_current = pt_tokenizer->nextToken(self);
		parseQualifiedName(self, NULL, name);
		moveColumn(self, name);
	}
}

void XTParseTable::parseQualifiedName(XTThreadPtr self, char *parent_name, char *name)
{
	if (parent_name)
		parent_name[0] = '\0';
	/* Should be an identifier by I have this example:
	 * CREATE TABLE t1 ( comment CHAR(32) ASCII NOT NULL, koi8_ru_f CHAR(32) CHARACTER SET koi8r NOT NULL default '' ) CHARSET=latin5;
	 *
	 * COMMENT is elsewhere used as reserved word?!
	 */
	if (pt_current->getString(name, XT_IDENTIFIER_NAME_SIZE) >= XT_IDENTIFIER_NAME_SIZE)
		raiseError(self, pt_current, XT_ERR_ID_TOO_LONG);
	pt_current = pt_tokenizer->nextToken(self);
	while (pt_current->isKeyWord(".")) {
		if (parent_name)
			xt_strcpy(XT_IDENTIFIER_NAME_SIZE,parent_name, name);
		pt_current = pt_tokenizer->nextToken(self);
		/* Accept anything after the DOT! */
		if (pt_current->getString(name, XT_IDENTIFIER_NAME_SIZE) >= XT_IDENTIFIER_NAME_SIZE)
			raiseError(self, pt_current, XT_ERR_ID_TOO_LONG);
		pt_current = pt_tokenizer->nextToken(self);
	}
}

void XTParseTable::parseTableName(XTThreadPtr self, bool alterTable)
{
	char name[XT_IDENTIFIER_NAME_SIZE];

	parseQualifiedName(self, NULL, name);
	setTableName(self, name, alterTable);
}

void XTParseTable::parseColumnDefinition(XTThreadPtr self, char *old_col_name)
{
	char col_name[XT_IDENTIFIER_NAME_SIZE];

	// column_definition
	parseQualifiedName(self, NULL, col_name);
	addColumn(self, col_name, old_col_name);
	parseDataType(self);

	for (;;) {
		if (pt_current->isReservedWord(XT_TK_NOT)) {
			pt_current = pt_tokenizer->nextToken(self);
			pt_current = pt_tokenizer->nextToken(self, "NULL", pt_current);
			setNull(self, false);
		}
		else if (pt_current->isReservedWord(XT_TK_NULL)) {
			pt_current = pt_tokenizer->nextToken(self);
			setNull(self, true);
		}
		else if (pt_current->isReservedWord(XT_TK_DEFAULT)) {
			pt_current = pt_tokenizer->nextToken(self);
			/* Possible here [ + | - ] <value> or [ <charset> ] <string> */
			parseExpression(self, false);
		}
		else if (pt_current->isReservedWord(XT_TK_AUTO_INCREMENT)) {
			pt_current = pt_tokenizer->nextToken(self);
			setAutoInc(self, true);
		}
		else if (pt_current->isReservedWord(XT_TK_UNIQUE)) {
			pt_current = pt_tokenizer->nextToken(self);
			if (pt_current->isReservedWord(XT_TK_KEY))
				pt_current = pt_tokenizer->nextToken(self);
			addConstraint(self, NULL, XT_DD_INDEX_UNIQUE, true);
		}
		else if (pt_current->isReservedWord(XT_TK_KEY)) {
			pt_current = pt_tokenizer->nextToken(self);
			addConstraint(self, NULL, XT_DD_INDEX, true);
		}
		else if (pt_current->isReservedWord(XT_TK_PRIMARY)) {
			pt_current = pt_tokenizer->nextToken(self);
			pt_current = pt_tokenizer->nextToken(self, "KEY", pt_current);
			addConstraint(self, NULL, XT_DD_KEY_PRIMARY, true);
		}
		else if (pt_current->isReservedWord(XT_TK_COMMENT)) {
			pt_current = pt_tokenizer->nextToken(self);
			pt_current = pt_tokenizer->nextToken(self);
		}
		else if (pt_current->isReservedWord(XT_TK_REFERENCES)) {
			addConstraint(self, NULL, XT_DD_KEY_FOREIGN, true);
			parseReferenceDefinition(self, 1);
		}
		else if (pt_current->isReservedWord(XT_TK_CHECK)) {
			pt_current = pt_tokenizer->nextToken(self);
			parseExpression(self, false);
		}
		/* GOTCHA: Not in the documentation:
		 * CREATE TABLE t1 (c varchar(255) NOT NULL COLLATE utf8_general_ci, INDEX (c))
		 */
		else if (pt_current->isReservedWord(XT_TK_COLLATE)) {
			pt_current = pt_tokenizer->nextToken(self);
			pt_current = pt_tokenizer->nextToken(self);
		}
		else
			break;
	}
}

void XTParseTable::parseDataType(XTThreadPtr self)
{
	/* Not actually implemented because MySQL allows undocumented
	 * syntax like this:
	 * create table t1 (c national character varying(10))
	 */
	parseExpression(self, false);
	setDataType(self, NULL);
}

void XTParseTable::optionalIndexName(XTThreadPtr self)
{
	// [index_name]
	if (!pt_current->isKeyWord("USING") && !pt_current->isKeyWord("(")) {
		char name[XT_IDENTIFIER_NAME_SIZE];

		parseIdentifier(self, name);
		setIndexName(self, name);
	}
}

void XTParseTable::optionalIndexType(XTThreadPtr self)
{
	// USING {BTREE | HASH}
	if (pt_current->isKeyWord("USING")) {
		pt_current = pt_tokenizer->nextToken(self);
		pt_current = pt_tokenizer->nextToken(self);
	}
}

u_int XTParseTable::columnList(XTThreadPtr self, bool index_cols)
{
	char	name[XT_IDENTIFIER_NAME_SIZE];
	u_int	cols = 0;
	
	pt_current->expectKeyWord(self, "(");
	do {
		pt_current = pt_tokenizer->nextToken(self);
		parseQualifiedName(self, NULL, name);
		addListedColumn(self, name);
		cols++;
		if (index_cols) {
			if (pt_current->isKeyWord("(")) {
				pt_current = pt_tokenizer->nextToken(self);
				pt_current = pt_tokenizer->nextToken(self);
				pt_current = pt_tokenizer->nextToken(self, ")", pt_current);
			}
			if (pt_current->isKeyWord("ASC"))
				pt_current = pt_tokenizer->nextToken(self);
			else if (pt_current->isKeyWord("DESC"))
				pt_current = pt_tokenizer->nextToken(self);
		}
	} while (pt_current->isKeyWord(","));
	pt_current = pt_tokenizer->nextToken(self, ")", pt_current);
	return cols;
}

void XTParseTable::parseReferenceDefinition(XTThreadPtr self, u_int req_cols)
{
	int		on_delete = XT_KEY_ACTION_DEFAULT;
	int		on_update = XT_KEY_ACTION_DEFAULT;
	char	name[XT_IDENTIFIER_NAME_SIZE];
	char	parent_name[XT_IDENTIFIER_NAME_SIZE];
	u_int	cols = 0;

	// REFERENCES tbl_name
	pt_current = pt_tokenizer->nextToken(self, "REFERENCES", pt_current);
	parseQualifiedName(self, parent_name, name);
	setReferencedTable(self, parent_name[0] ? parent_name : NULL, name);

	// [ (index_col_name,...) ]
	if (pt_current->isKeyWord("(")) {
		pt_current->expectKeyWord(self, "(");
		do {
			pt_current = pt_tokenizer->nextToken(self);
			parseQualifiedName(self, NULL, name);
			addReferencedColumn(self, name);
			cols++;
			if (cols > req_cols)
				raiseError(self, pt_current, XT_ERR_INCORRECT_NO_OF_COLS);
		} while (pt_current->isKeyWord(","));
		if (cols != req_cols)
			raiseError(self, pt_current, XT_ERR_INCORRECT_NO_OF_COLS);
		pt_current = pt_tokenizer->nextToken(self, ")", pt_current);			
	}
	else
		addReferencedColumn(self, NULL);

	// [MATCH FULL | MATCH PARTIAL | MATCH SIMPLE]
	if (pt_current->isKeyWord("MATCH")) {
		pt_current = pt_tokenizer->nextToken(self);
		pt_current = pt_tokenizer->nextToken(self);
	}

	// [ON DELETE {RESTRICT | CASCADE | SET NULL | SET DEFAULT | NO ACTION}]
	// [ON UPDATE {RESTRICT | CASCADE | SET NULL | SET DEFAULT | NO ACTION}]
	while (pt_current->isKeyWord("ON")) {
		pt_current = pt_tokenizer->nextToken(self);
		if (pt_current->isKeyWord("DELETE"))
			on_delete = parseKeyAction(self);
		else if (pt_current->isKeyWord("UPDATE"))
			on_update = parseKeyAction(self);
		else
			syntaxError(self, pt_current);
		pt_current = pt_tokenizer->nextToken(self);
	}

	setActions(self, on_delete, on_update);
}

void XTParseTable::parseAlterTable(XTThreadPtr self)
{
	char name[XT_IDENTIFIER_NAME_SIZE];

	pt_current = pt_tokenizer->nextToken(self, "ALTER", pt_current);
	if (pt_current->isKeyWord("IGNORE"))
		pt_current = pt_tokenizer->nextToken(self);
	pt_current = pt_tokenizer->nextToken(self, "TABLE", pt_current);
	parseTableName(self, true);
	for (;;) {
		if (pt_current->isKeyWord("ADD")) {
			pt_current = pt_tokenizer->nextToken(self);
			parseAddTableItem(self);
		}
		else if (pt_current->isKeyWord("ALTER")) {
			pt_current = pt_tokenizer->nextToken(self);
			if (pt_current->isReservedWord(XT_TK_COLUMN))
				pt_current = pt_tokenizer->nextToken(self);
			pt_current->expectIdentifier(self);
			pt_current = pt_tokenizer->nextToken(self);
			if (pt_current->isKeyWord("SET")) {
				pt_current = pt_tokenizer->nextToken(self);
				pt_current = pt_tokenizer->nextToken(self, "DEFAULT", pt_current);
				pt_current = pt_tokenizer->nextToken(self);
			}
			else if (pt_current->isKeyWord("DROP")) {
				pt_current = pt_tokenizer->nextToken(self);
				pt_current = pt_tokenizer->nextToken(self, "DEFAULT", pt_current);
			}
		}
		else if (pt_current->isKeyWord("CHANGE")) {
			char old_col_name[XT_IDENTIFIER_NAME_SIZE];

			pt_current = pt_tokenizer->nextToken(self);
			if (pt_current->isReservedWord(XT_TK_COLUMN))
				pt_current = pt_tokenizer->nextToken(self);

			parseQualifiedName(self, NULL, old_col_name);
			parseColumnDefinition(self, old_col_name);
			parseMoveColumn(self);
		}
		else if (pt_current->isKeyWord("MODIFY")) {
			pt_current = pt_tokenizer->nextToken(self);
			if (pt_current->isReservedWord(XT_TK_COLUMN))
				pt_current = pt_tokenizer->nextToken(self);
			parseColumnDefinition(self, NULL);
			parseMoveColumn(self);
		}
		else if (pt_current->isKeyWord("DROP")) {
			pt_current = pt_tokenizer->nextToken(self);
			if (pt_current->isReservedWord(XT_TK_PRIMARY)) {
				pt_current = pt_tokenizer->nextToken(self);
				pt_current = pt_tokenizer->nextToken(self, "KEY", pt_current);
				dropConstraint(self, NULL, XT_DD_KEY_PRIMARY);
			}
			else if (pt_current->isReservedWord(XT_TK_INDEX) || pt_current->isReservedWord(XT_TK_KEY)) {
				pt_current = pt_tokenizer->nextToken(self);
				parseIdentifier(self, name);
				dropConstraint(self, name, XT_DD_INDEX);
			}
			else if (pt_current->isReservedWord(XT_TK_FOREIGN)) {
				pt_current = pt_tokenizer->nextToken(self);
				pt_current = pt_tokenizer->nextToken(self, "KEY", pt_current);
				parseIdentifier(self, name);
				dropConstraint(self, name, XT_DD_KEY_FOREIGN);
			}
			else {
				if (pt_current->isReservedWord(XT_TK_COLUMN))
					pt_current = pt_tokenizer->nextToken(self);
				parseQualifiedName(self, NULL, name);
				dropColumn(self, name);
			}
		}
		else if (pt_current->isKeyWord("RENAME")) {
			pt_current = pt_tokenizer->nextToken(self);
			if (pt_current->isKeyWord("TO"))
				pt_current = pt_tokenizer->nextToken(self);
			parseQualifiedName(self, NULL, name);
		}
		else
			/* Just ignore the syntax until the next , */
			parseExpression(self, true);
		if (!pt_current->isKeyWord(","))
			break;
		pt_current = pt_tokenizer->nextToken(self);
	}
}

void XTParseTable::parseCreateIndex(XTThreadPtr self)
{
	char name[XT_IDENTIFIER_NAME_SIZE];
	bool is_unique = false;

	if (pt_current->isReservedWord(XT_TK_UNIQUE)) {
		pt_current = pt_tokenizer->nextToken(self);
		is_unique = true;
	}
	else if (pt_current->isReservedWord(XT_TK_FULLTEXT))
		pt_current = pt_tokenizer->nextToken(self);
	else if (pt_current->isKeyWord("SPACIAL"))
		pt_current = pt_tokenizer->nextToken(self);
	pt_current = pt_tokenizer->nextToken(self, "INDEX", pt_current);
	parseQualifiedName(self, NULL, name);
	optionalIndexType(self);
	pt_current = pt_tokenizer->nextToken(self, "ON", pt_current);
	parseTableName(self, true);
	addConstraint(self, NULL, is_unique ? XT_DD_INDEX_UNIQUE : XT_DD_INDEX, false);
	setIndexName(self, name);
	columnList(self, true);
}

void XTParseTable::parseDropIndex(XTThreadPtr self)
{
	char name[XT_IDENTIFIER_NAME_SIZE];

	pt_current = pt_tokenizer->nextToken(self, "DROP", pt_current);
	pt_current = pt_tokenizer->nextToken(self, "INDEX", pt_current);
	parseQualifiedName(self, NULL, name);
	pt_current = pt_tokenizer->nextToken(self, "ON", pt_current);
	parseTableName(self, true);
	dropConstraint(self, name, XT_DD_INDEX);
}

/*
 * -----------------------------------------------------------------------
 * Create/Alter table table
 */

class XTCreateTable : public XTParseTable {
	public:
	bool					ct_convert;
	struct charset_info_st	*ct_charset;
	XTPathStrPtr			ct_tab_path;
	u_int					ct_contraint_no;
	XTDDTable				*ct_curr_table;
	XTDDColumn				*ct_curr_column;
	XTDDConstraint			*ct_curr_constraint;

	XTCreateTable(bool convert, XTPathStrPtr tab_path) : XTParseTable() {
		ct_convert = convert;
		ct_charset = myxt_getcharset(convert);
		ct_tab_path = tab_path;
		ct_curr_table = NULL;
		ct_curr_column = NULL;
		ct_curr_constraint = NULL;
	}

	virtual void finalize(XTThreadPtr self) {
		if (ct_curr_table)
			ct_curr_table->release(self);
		XTParseTable::finalize(self);
	}

	virtual void setTableName(XTThreadPtr self, char *name, bool alterTable);
	virtual void addColumn(XTThreadPtr self, char *col_name, char *old_col_name);
	virtual void addConstraint(XTThreadPtr self, char *name, u_int type, bool lastColumn);
	virtual void dropConstraint(XTThreadPtr self, char *name, u_int type);
	virtual void addListedColumn(XTThreadPtr self, char *index_col_name);
	virtual void setReferencedTable(XTThreadPtr self, char *ref_schema, char *ref_table);
	virtual void addReferencedColumn(XTThreadPtr self, char *index_col_name);
	virtual void setActions(XTThreadPtr self, int on_delete, int on_update);

	virtual void parseTable(XTThreadPtr self, bool convert, char *sql);	
};

static void ri_free_create_table(XTThreadPtr self, XTCreateTable *ct)
{
	if (ct)
		ct->release(self);
}

XTDDTable *xt_ri_create_table(XTThreadPtr self, bool convert, XTPathStrPtr tab_path, char *sql, XTDDTable *start_tab)
{
	XTCreateTable	*ct;
	XTDDTable		*dd_tab;

	if (!(ct = new XTCreateTable(convert, tab_path))) {
		if (start_tab)
			start_tab->release(self);
		xt_throw_errno(XT_CONTEXT, XT_ENOMEM);
	}

	ct->ct_curr_table = start_tab;

	pushr_(ri_free_create_table, ct);

	ct->parseTable(self, convert, sql);
	
	/* Return the table ... */
	dd_tab = ct->ct_curr_table;
	ct->ct_curr_table = NULL;

	freer_();
	return dd_tab;
}

void XTCreateTable::parseTable(XTThreadPtr self, bool convert, char *sql)
{
	u_int i;

	ct_contraint_no = 0;
	XTParseTable::parseTable(self, convert, sql);

	/* Remove contraints that do not have matching columns. */
	for (i=0; i<ct_curr_table->dt_indexes.size();) {
		if (!ct_curr_table->dt_indexes.itemAt(i)->attachColumns())
			ct_curr_table->dt_indexes.remove(self, i);
		else
			i++;
	}

	for (i=0; i<ct_curr_table->dt_fkeys.size(); ) {
		if (!ct_curr_table->dt_fkeys.itemAt(i)->attachColumns())
			ct_curr_table->dt_fkeys.remove(self, i);
		else
			i++;
	}
}

void XTCreateTable::setTableName(XTThreadPtr self, char *name, bool alterTable)
{
	char path[PATH_MAX];

	if (!name)
		return;

	xt_strcpy(PATH_MAX, path, ct_tab_path->ps_path);
	xt_remove_last_name_of_path(path);

	if (ct_convert) {
		char	buffer[XT_IDENTIFIER_NAME_SIZE];
		size_t	len;

		myxt_static_convert_identifier(self, ct_charset, name, buffer, XT_IDENTIFIER_NAME_SIZE);
		len = strlen(path);
		myxt_static_convert_table_name(self, buffer, &path[len], PATH_MAX - len);
	}
	else
		xt_strcat(PATH_MAX, path, name);

	if (alterTable) {
		XTTableHPtr	tab;

		/* Find the table... */
		pushsr_(tab, xt_heap_release, xt_use_table(self, (XTPathStrPtr) path, FALSE, TRUE, NULL));

		/* Clone the foreign key definitions: */
		if (tab && tab->tab_dic.dic_table) {
			ct_curr_table->dt_fkeys.deleteAll(self);
			ct_curr_table->dt_fkeys.clone(self, &tab->tab_dic.dic_table->dt_fkeys);	
			for (u_int i=0; i<ct_curr_table->dt_fkeys.size(); i++)
				ct_curr_table->dt_fkeys.itemAt(i)->co_table = ct_curr_table;
		}

		freer_(); // xt_heap_release(tab)
	}
}

/*
 * old_name is given if the column name was changed.
 * NOTE that we built the table desciption from the current MySQL table
 * description. This means that all changes to columns and 
 * indexes have already been applied.
 *
 * Our job is to now add the foreign key changes.
 * This means we have to note the current column here. It is
 * possible to add a FOREIGN KEY contraint directly to a column!
 */
void XTCreateTable::addColumn(XTThreadPtr self, char *new_name, char *old_name)
{
	char new_col_name[XT_IDENTIFIER_NAME_SIZE];

	myxt_static_convert_identifier(self, ct_charset, new_name, new_col_name, XT_IDENTIFIER_NAME_SIZE);
	ct_curr_column = ct_curr_table->findColumn(new_col_name);
	if (old_name) {
		char old_col_name[XT_IDENTIFIER_NAME_SIZE];

		myxt_static_convert_identifier(self, ct_charset, old_name, old_col_name, XT_IDENTIFIER_NAME_SIZE);
		ct_curr_table->alterColumnName(self, old_col_name, new_col_name);
	}
}

void XTCreateTable::addConstraint(XTThreadPtr self, char *name, u_int type, bool lastColumn)
{
	/* We are only interested in foreign keys! */
	if (type == XT_DD_KEY_FOREIGN) {
		char buffer[50];

		if (!(ct_curr_constraint = new XTDDForeignKey()))
			xt_throw_errno(XT_CONTEXT, XT_ENOMEM);
		ct_curr_table->dt_fkeys.append(self, (XTDDForeignKey *) ct_curr_constraint);
		ct_curr_constraint->co_table = ct_curr_table;

		if (name && *name)
			ct_curr_constraint->co_name = myxt_convert_identifier(self, ct_charset, name);
		else {
			// Generate a default constraint name:
			ct_contraint_no++;
			sprintf(buffer, "FOREIGN_%d", ct_contraint_no);
			ct_curr_constraint->co_name = xt_dup_string(self, buffer);
		}

		if (lastColumn && ct_curr_column) {
			/* This constraint has one column, the current column. */
			XTDDColumnRef	*cref;
			char			*col_name = xt_dup_string(self, ct_curr_column->dc_name);

			if (!(cref = new XTDDColumnRef())) {
				xt_free(self, col_name);
				xt_throw_errno(XT_CONTEXT, XT_ENOMEM);
			}
			cref->cr_col_name = col_name;
			ct_curr_constraint->co_cols.append(self, cref);
		}
	}
	else
		/* Other constraints/indexes do not interest us: */
		ct_curr_constraint = NULL;
}

void XTCreateTable::dropConstraint(XTThreadPtr self, char *name, u_int type)
{
	if (type == XT_DD_KEY_FOREIGN && name) {
		u_int			i;
		XTDDForeignKey	*fkey;
		char			con_name[XT_IDENTIFIER_NAME_SIZE];

		myxt_static_convert_identifier(self, ct_charset, name, con_name, XT_IDENTIFIER_NAME_SIZE);
		for (i=0; i<ct_curr_table->dt_fkeys.size(); i++) {
			fkey = ct_curr_table->dt_fkeys.itemAt(i);
			if (fkey->co_name && myxt_strcasecmp(con_name, fkey->co_name) == 0) {
				ct_curr_table->dt_fkeys.remove(fkey);
				fkey->release(self);
			}
		}
	}
}

void XTCreateTable::addListedColumn(XTThreadPtr self, char *index_col_name)
{
	if (ct_curr_constraint && ct_curr_constraint->co_type == XT_DD_KEY_FOREIGN) {
		XTDDColumnRef	*cref;
		char			*name = myxt_convert_identifier(self, ct_charset, index_col_name);

		if (!(cref = new XTDDColumnRef())) {
			xt_free(self, name);
			xt_throw_errno(XT_CONTEXT, XT_ENOMEM);
		}
		cref->cr_col_name = name;
		ct_curr_constraint->co_cols.append(self, cref);
	}
}

void XTCreateTable::setReferencedTable(XTThreadPtr self, char *ref_schema, char *ref_table)
{
	XTDDForeignKey	*fk = (XTDDForeignKey *) ct_curr_constraint;
	char			path[PATH_MAX];

	if (ref_schema) {
		xt_strcpy(PATH_MAX,path, ".");
		xt_add_dir_char(PATH_MAX, path);
		xt_strcat(PATH_MAX, path, ref_schema);
		xt_add_dir_char(PATH_MAX, path);
		xt_strcat(PATH_MAX, path, ref_table);
	} else {
		xt_strcpy(PATH_MAX, path, ct_tab_path->ps_path);
		xt_remove_last_name_of_path(path);
		if (ct_convert) {
			char	buffer[XT_IDENTIFIER_NAME_SIZE];
			size_t	len;

			myxt_static_convert_identifier(self, ct_charset, ref_table, buffer, XT_IDENTIFIER_NAME_SIZE);
			len = strlen(path);
			myxt_static_convert_table_name(self, buffer, &path[len], PATH_MAX - len);
		}
		else
			xt_strcat(PATH_MAX, path, ref_table);
	}

	fk->fk_ref_tab_name = (XTPathStrPtr) xt_dup_string(self, path);
}

/* If the referenced column is NULL, this means 
 * duplicate the local column list!
 */
void XTCreateTable::addReferencedColumn(XTThreadPtr self, char *index_col_name)
{
	XTDDForeignKey	*fk = (XTDDForeignKey *) ct_curr_constraint;
	XTDDColumnRef	*cref;
	char			*name;

	if (index_col_name) {
		name = myxt_convert_identifier(self, ct_charset, index_col_name);
		if (!(cref = new XTDDColumnRef())) {
			xt_free(self, name);
			xt_throw_errno(XT_CONTEXT, XT_ENOMEM);
		}
		cref->cr_col_name = name;
		fk->fk_ref_cols.append(self, cref);
	}
	else
		fk->fk_ref_cols.clone(self, &fk->co_cols);
}

void XTCreateTable::setActions(XTThreadPtr XT_UNUSED(self), int on_delete, int on_update)
{
	XTDDForeignKey	*fk = (XTDDForeignKey *) ct_curr_constraint;

	fk->fk_on_delete = on_delete;
	fk->fk_on_update = on_update;
}

/*
 * -----------------------------------------------------------------------
 * Dictionary methods
 */

void XTDDColumn::init(XTThreadPtr self, XTObject *obj) {
	XTDDColumn *col = (XTDDColumn *) obj;

	XTObject::init(self, obj);
	if (col->dc_name)
		dc_name = xt_dup_string(self, col->dc_name);
	if (col->dc_data_type)
		dc_data_type = xt_dup_string(self, col->dc_data_type);
	dc_null_ok = col->dc_null_ok;
	dc_auto_inc = col->dc_auto_inc;
}

void XTDDColumn::finalize(XTThreadPtr self)
{
	if (dc_name)
		xt_free(self, dc_name);
	if (dc_data_type)
		xt_free(self, dc_data_type);
}

void XTDDColumn::loadString(XTThreadPtr self, XTStringBufferPtr sb)
{
	xt_sb_concat(self, sb, "`");
	xt_sb_concat(self, sb, dc_name);
	xt_sb_concat(self, sb, "` ");
	if (dc_data_type) {
		xt_sb_concat(self, sb, dc_data_type);
		if (dc_null_ok)
			xt_sb_concat(self, sb, " NULL");
		else
			xt_sb_concat(self, sb, " NOT NULL");
		if (dc_auto_inc)
			xt_sb_concat(self, sb, " AUTO_INCREMENT");
	}
}

void  XTDDColumnRef::init(XTThreadPtr self, XTObject *obj)
{
	XTDDColumnRef *cr = (XTDDColumnRef *) obj;

	XTObject::init(self, obj);
	cr_col_name = xt_dup_string(self, cr->cr_col_name);
}

void XTDDColumnRef::finalize(XTThreadPtr self)
{
	XTObject::finalize(self);
	if (cr_col_name) {
		xt_free(self, cr_col_name);
		cr_col_name = NULL;
	}
}

void  XTDDConstraint::init(XTThreadPtr self, XTObject *obj)
{
	XTDDConstraint *co = (XTDDConstraint *) obj;

	XTObject::init(self, obj);
	co_type = co->co_type;
	if (co->co_name)
		co_name = xt_dup_string(self, co->co_name);
	if (co->co_ind_name)
		co_ind_name = xt_dup_string(self, co->co_ind_name);
	co_cols.clone(self, &co->co_cols);
}

void XTDDConstraint::loadString(XTThreadPtr self, XTStringBufferPtr sb)
{
	if (co_name) {
		xt_sb_concat(self, sb, "CONSTRAINT `");
		xt_sb_concat(self, sb, co_name);
		xt_sb_concat(self, sb, "` ");
	}
	switch (co_type) {
		case XT_DD_INDEX:
			xt_sb_concat(self, sb, "INDEX ");
			break;
		case XT_DD_INDEX_UNIQUE:
			xt_sb_concat(self, sb, "UNIQUE INDEX ");
			break;
		case XT_DD_KEY_PRIMARY:
			xt_sb_concat(self, sb, "PRIMARY KEY ");
			break;
		case XT_DD_KEY_FOREIGN:
			xt_sb_concat(self, sb, "FOREIGN KEY ");
			break;		
	}
	if (co_ind_name) {
		xt_sb_concat(self, sb, "`");
		xt_sb_concat(self, sb, co_ind_name);
		xt_sb_concat(self, sb, "` ");
	}
	xt_sb_concat(self, sb, "(`");
	xt_sb_concat(self, sb, co_cols.itemAt(0)->cr_col_name);
	for (u_int i=1; i<co_cols.size(); i++) {
		xt_sb_concat(self, sb, "`, `");
		xt_sb_concat(self, sb, co_cols.itemAt(i)->cr_col_name);
	}
	xt_sb_concat(self, sb, "`)");
}

void XTDDConstraint::alterColumnName(XTThreadPtr self, char *from_name, char *to_name)
{
	XTDDColumnRef *col;

	for (u_int i=0; i<co_cols.size(); i++) {
		col = co_cols.itemAt(i);
		if (myxt_strcasecmp(col->cr_col_name, from_name) == 0) {
			char *name = xt_dup_string(self, to_name);

			xt_free(self, col->cr_col_name);
			col->cr_col_name = name;
			break;
		}
	}
}

void XTDDConstraint::getColumnList(char *buffer, size_t size)
{
	if (co_table->dt_table) {
		xt_strcat(size, buffer, "`");
		xt_strcpy(size, buffer, co_table->dt_table->tab_name->ps_path);
		xt_strcat(size, buffer, "` (`");
	}
	else
		xt_strcpy(size, buffer, "(`");
	xt_strcat(size, buffer, co_cols.itemAt(0)->cr_col_name);
	for (u_int i=1; i<co_cols.size(); i++) {
		xt_strcat(size, buffer, "`, `");
		xt_strcat(size, buffer, co_cols.itemAt(i)->cr_col_name);
	}
	xt_strcat(size, buffer, "`)");
}

bool XTDDConstraint::sameColumns(XTDDConstraint *co)
{
	u_int i = 0;

	if (co_cols.size() != co->co_cols.size())
		return false;
	while (i<co_cols.size()) {
		if (myxt_strcasecmp(co_cols.itemAt(i)->cr_col_name, co->co_cols.itemAt(i)->cr_col_name) != 0)
			return false;
		i++;
	}
	return OK;
}

bool XTDDConstraint::attachColumns()
{
	XTDDColumn		*col;

	for (u_int i=0; i<co_cols.size(); i++) {
		if (!(col = co_table->findColumn(co_cols.itemAt(i)->cr_col_name)))
			return false;
		/* If this is a primary key, then the column becomes not-null! */
		if (co_type == XT_DD_KEY_PRIMARY)
			col->dc_null_ok = false;
	}
	return true;
}

void XTDDTableRef::finalize(XTThreadPtr self)
{
	XTDDForeignKey	*fk;

	if ((fk = tr_fkey)) {
		tr_fkey = NULL;
		fk->removeReference(self);
		xt_heap_release(self, fk->co_table->dt_table); /* We referenced the database table, not the foreign key */
	}
	XTObject::finalize(self);
}

bool XTDDTableRef::checkReference(xtWord1 *before_buf, XTThreadPtr thread)
{
	XTIndexPtr			loc_ind, ind;
	xtBool				no_null = TRUE;
	XTOpenTablePtr		ot;
	XTIdxSearchKeyRec	search_key;
	xtXactID			xn_id;
	XTXactWaitRec		xw;
	bool				ok = false;

	if (!(loc_ind = tr_fkey->getReferenceIndexPtr()))
		return false;

	if (!(ind = tr_fkey->getIndexPtr()))
		return false;

	search_key.sk_key_value.sv_flags = 0;
	search_key.sk_key_value.sv_rec_id = 0;
	search_key.sk_key_value.sv_row_id = 0;
	search_key.sk_key_value.sv_key = search_key.sk_key_buf;
	search_key.sk_key_value.sv_length = myxt_create_foreign_key_from_row(loc_ind, search_key.sk_key_buf, before_buf, ind, &no_null);
	search_key.sk_on_key = FALSE;

	if (!no_null)
		return true;

	/* Search for the key in the child (referencing) table: */
	if (!(ot = xt_db_open_table_using_tab(tr_fkey->co_table->dt_table, thread)))
		return false;

	retry:
	if (!xt_idx_search(ot, ind, &search_key))
		goto done;
		
	while (ot->ot_curr_rec_id && search_key.sk_on_key) {
		switch (xt_tab_maybe_committed(ot, ot->ot_curr_rec_id, &xn_id, &ot->ot_curr_row_id, &ot->ot_curr_updated)) {
			case XT_MAYBE:				
				xw.xw_xn_id = xn_id;
				if (!xt_xn_wait_for_xact(thread, &xw, NULL))
					goto done;
				goto retry;
			case XT_ERR:
				goto done;
			case TRUE:
				/* We found a matching child: */
				xt_register_ixterr(XT_REG_CONTEXT, XT_ERR_ROW_IS_REFERENCED, tr_fkey->co_name);
				goto done;
			case FALSE:
				if (!xt_idx_next(ot, ind, &search_key))
					goto done;
				break;
		}
	}

	/* No matching children, all OK: */
	ok = true;

	done:
	if (ot->ot_ind_rhandle) {
		xt_ind_release_handle(ot->ot_ind_rhandle, FALSE, thread);
		ot->ot_ind_rhandle = NULL;
	}
	xt_db_return_table_to_pool_ns(ot);
	return ok;
}

/*
 * A row has been deleted or updated (after_buf non-NULL), check if it is referenced by the foreign key table.
 * If it is referenced, then we need to follow the specified action.
 */
bool XTDDTableRef::modifyRow(XTOpenTablePtr XT_UNUSED(ref_ot), xtWord1 *before_buf, xtWord1 *after_buf, XTThreadPtr thread)
{
	XTIndexPtr			loc_ind, ind;
	xtBool				no_null = TRUE;
	XTOpenTablePtr		ot;
	XTIdxSearchKeyRec	search_key;
	xtXactID			xn_id;
	int					action = after_buf ? tr_fkey->fk_on_update : tr_fkey->fk_on_delete;
	u_int				after_key_len = 0;
	xtWord1				*after_key = NULL;
	XTInfoBufferRec		after_info;
	XTXactWaitRec		xw;

	after_info.ib_free = FALSE;

	if (!(loc_ind = tr_fkey->getReferenceIndexPtr()))
		return false;

	if (!(ind = tr_fkey->getIndexPtr()))
		return false;

	search_key.sk_key_value.sv_flags = 0;
	search_key.sk_key_value.sv_rec_id = 0;
	search_key.sk_key_value.sv_row_id = 0;
	search_key.sk_key_value.sv_key = search_key.sk_key_buf;
	search_key.sk_key_value.sv_length = myxt_create_foreign_key_from_row(loc_ind, search_key.sk_key_buf, before_buf, ind, &no_null);
	search_key.sk_on_key = FALSE;

	if (!no_null)
		return true;

	if (after_buf) {
		if (!(after_key = (xtWord1 *) xt_malloc_ns(XT_INDEX_MAX_KEY_SIZE)))
			return false;
		after_key_len = myxt_create_foreign_key_from_row(loc_ind, after_key, after_buf, ind, NULL);
		
		/* Check whether the key value has changed, if not, we have nothing
		 * to do here!
		 */
		if (myxt_compare_key(ind, 0, search_key.sk_key_value.sv_length,
			search_key.sk_key_value.sv_key, after_key) == 0)
			goto success;

	}

	/* Search for the key in the child (referencing) table: */
	if (!(ot = xt_db_open_table_using_tab(tr_fkey->co_table->dt_table, thread)))
		goto failed;

	retry:
	if (!xt_idx_search(ot, ind, &search_key))
		goto failed_2;
		
	while (ot->ot_curr_rec_id && search_key.sk_on_key) {
		switch (xt_tab_maybe_committed(ot, ot->ot_curr_rec_id, &xn_id, &ot->ot_curr_row_id, &ot->ot_curr_updated)) {
			case XT_MAYBE:
				xw.xw_xn_id = xn_id;
				if (!xt_xn_wait_for_xact(thread, &xw, NULL))
					goto failed_2;
				goto retry;
			case XT_ERR:
				goto failed_2;
			case TRUE:
				/* We found a matching child: */
				switch (action) {
					case XT_KEY_ACTION_CASCADE:
						if (after_buf) {
							/* Do a cascaded update: */
							if (!xt_tab_load_record(ot, ot->ot_curr_rec_id, &after_info))
								goto failed_2;

							if (!myxt_create_row_from_key(ot, ind, after_key, after_key_len, after_info.ib_db.db_data))
								goto failed_2;

							if (!xt_tab_update_record(ot, NULL, after_info.ib_db.db_data)) {
								// Change to duplicate foreign key
								if (ot->ot_thread->t_exception.e_xt_err == XT_ERR_DUPLICATE_KEY)
									xt_register_ixterr(XT_REG_CONTEXT, XT_ERR_DUPLICATE_FKEY, tr_fkey->co_name);
								goto failed_2;
							}
						}
						else {
							/* Do a cascaded delete: */
							if (!xt_tab_delete_record(ot, NULL))
								goto failed_2;
						}
						break;
					case XT_KEY_ACTION_SET_NULL:
						if (!xt_tab_load_record(ot, ot->ot_curr_rec_id, &after_info))
							goto failed_2;

						myxt_set_null_row_from_key(ot, ind, after_info.ib_db.db_data);

						if (!xt_tab_update_record(ot, NULL, after_info.ib_db.db_data))
							goto failed_2;
						break;
					case XT_KEY_ACTION_SET_DEFAULT:

						if (!xt_tab_load_record(ot, ot->ot_curr_rec_id, &after_info))
							goto failed_2;

						myxt_set_default_row_from_key(ot, ind, after_info.ib_db.db_data);

						if (!xt_tab_update_record(ot, NULL, after_info.ib_db.db_data))
							goto failed_2;

						break;
					case XT_KEY_ACTION_NO_ACTION:
#ifdef XT_IMPLEMENT_NO_ACTION
						XTRestrictItemRec	r;
						
						r.ri_tab_id = ref_ot->ot_table->tab_id;
						r.ri_rec_id = ref_ot->ot_curr_rec_id;
						if (!xt_bl_append(NULL, &thread->st_restrict_list, (void *) &r))
							goto failed_2;
						break;
#endif
					default:
						xt_register_ixterr(XT_REG_CONTEXT, XT_ERR_ROW_IS_REFERENCED, tr_fkey->co_name);
						goto failed_2;
				}
				/* Fall throught to next: */
			case FALSE:
				if (!xt_idx_next(ot, ind, &search_key))
					goto failed_2;
				break;
		}
	}

	/* No matching children, all OK: */
	if (ot->ot_ind_rhandle) {
		xt_ind_release_handle(ot->ot_ind_rhandle, FALSE, thread);
		ot->ot_ind_rhandle = NULL;
	}
	xt_db_return_table_to_pool_ns(ot);

	success:
	xt_ib_free(NULL, &after_info);
	if (after_key)
		xt_free_ns(after_key);
	return true;

	failed_2:
	if (ot->ot_ind_rhandle) {
		xt_ind_release_handle(ot->ot_ind_rhandle, FALSE, thread);
		ot->ot_ind_rhandle = NULL;
	}
	xt_db_return_table_to_pool_ns(ot);

	failed:
	xt_ib_free(NULL, &after_info);
	if (after_key)
		xt_free_ns(after_key);
	return false;
}

void XTDDTableRef::deleteAllRows(XTThreadPtr self)
{
	XTOpenTablePtr	ot;
	xtInt8			row_count;

	if (!tr_fkey->getReferenceIndexPtr())
		throw_();

	if (!tr_fkey->getIndexPtr())
		throw_();

	if (!(ot = xt_db_open_table_using_tab(tr_fkey->co_table->dt_table, self)))
		throw_();

	row_count = ((xtInt8) ot->ot_table->tab_row_eof_id) - 1;
	row_count -= (xtInt8) ot->ot_table->tab_row_fnum;

	xt_db_return_table_to_pool_ns(ot);

	if (row_count > 0)
		xt_throw_ixterr(XT_CONTEXT, XT_ERR_ROW_IS_REFERENCED, tr_fkey->co_name);
}

void  XTDDIndex::init(XTThreadPtr self, XTObject *obj)
{
	XTDDConstraint::init(self, obj);
}

XTIndexPtr XTDDIndex::getIndexPtr()
{
	if (in_index >= co_table->dt_table->tab_dic.dic_key_count) {
		XTDDIndex		*in;

		if (!(in = co_table->findIndex(this)))
			return NULL;
		in_index = in->in_index;
	}
	return co_table->dt_table->tab_dic.dic_keys[in_index];
}

void XTDDForeignKey::init(XTThreadPtr self, XTObject *obj)
{
	XTDDForeignKey *fk = (XTDDForeignKey *) obj;

	XTDDIndex::init(self, obj);
	if (fk->fk_ref_tab_name)
		fk_ref_tab_name = (XTPathStrPtr) xt_dup_string(self, fk->fk_ref_tab_name->ps_path);
	fk_ref_cols.clone(self, &fk->fk_ref_cols);
	fk_on_delete = fk->fk_on_delete;
	fk_on_update = fk->fk_on_update;
}

void XTDDForeignKey::finalize(XTThreadPtr self)
{
	XTDDTable *ref_tab;

	if (fk_ref_tab_name) {
		xt_free(self, fk_ref_tab_name);
		fk_ref_tab_name = NULL;
	}

	if ((ref_tab = fk_ref_table)) {
		fk_ref_table = NULL;
		ref_tab->removeReference(self, this);
		xt_heap_release(self, ref_tab->dt_table); /* We referenced the table, not the index! */
	}

	fk_ref_index = UINT_MAX;

	fk_ref_cols.deleteAll(self);
	XTDDConstraint::finalize(self);
}

void XTDDForeignKey::loadString(XTThreadPtr self, XTStringBufferPtr sb)
{
	char schema_name[XT_IDENTIFIER_NAME_SIZE];
	
	XTDDConstraint::loadString(self, sb);
	xt_sb_concat(self, sb, " REFERENCES `");
	xt_2nd_last_name_of_path(XT_IDENTIFIER_NAME_SIZE, schema_name, fk_ref_tab_name->ps_path);
	xt_sb_concat(self, sb, schema_name);
	xt_sb_concat(self, sb, "`.`");
	xt_sb_concat(self, sb, xt_last_name_of_path(fk_ref_tab_name->ps_path));
	xt_sb_concat(self, sb, "` ");

	xt_sb_concat(self, sb, "(`");
	xt_sb_concat(self, sb, fk_ref_cols.itemAt(0)->cr_col_name);
	for (u_int i=1; i<fk_ref_cols.size(); i++) {
		xt_sb_concat(self, sb, "`, `");
		xt_sb_concat(self, sb, fk_ref_cols.itemAt(i)->cr_col_name);
	}
	xt_sb_concat(self, sb, "`)");
	
	if (fk_on_delete != XT_KEY_ACTION_DEFAULT && fk_on_delete != XT_KEY_ACTION_RESTRICT) {
		xt_sb_concat(self, sb, " ON DELETE ");
		switch (fk_on_delete) {
			case XT_KEY_ACTION_CASCADE:		xt_sb_concat(self, sb, "CASCADE"); break;
			case XT_KEY_ACTION_SET_NULL:	xt_sb_concat(self, sb, "SET NULL"); break;
			case XT_KEY_ACTION_SET_DEFAULT:	xt_sb_concat(self, sb, "SET DEFAULT"); break;
			case XT_KEY_ACTION_NO_ACTION:	xt_sb_concat(self, sb, "NO ACTION"); break;
		}
	}
	if (fk_on_update != XT_KEY_ACTION_DEFAULT && fk_on_update != XT_KEY_ACTION_RESTRICT) {
		xt_sb_concat(self, sb, " ON UPDATE ");
		switch (fk_on_update) {
			case XT_KEY_ACTION_DEFAULT:		xt_sb_concat(self, sb, "RESTRICT"); break;
			case XT_KEY_ACTION_RESTRICT:	xt_sb_concat(self, sb, "RESTRICT"); break;
			case XT_KEY_ACTION_CASCADE:		xt_sb_concat(self, sb, "CASCADE"); break;
			case XT_KEY_ACTION_SET_NULL:	xt_sb_concat(self, sb, "SET NULL"); break;
			case XT_KEY_ACTION_SET_DEFAULT:	xt_sb_concat(self, sb, "SET DEFAULT"); break;
			case XT_KEY_ACTION_NO_ACTION:	xt_sb_concat(self, sb, "NO ACTION"); break;
		}
	}
}

void XTDDForeignKey::getReferenceList(char *buffer, size_t size)
{
	buffer[0] = '`';
	xt_strcpy(size, buffer + 1, xt_last_name_of_path(fk_ref_tab_name->ps_path));
	xt_strcat(size, buffer, "` (");
	xt_strcat(size, buffer, fk_ref_cols.itemAt(0)->cr_col_name);
	for (u_int i=1; i<fk_ref_cols.size(); i++) {
		xt_strcat(size, buffer, ", ");
		xt_strcat(size, buffer, fk_ref_cols.itemAt(i)->cr_col_name);
	}
	xt_strcat(size, buffer, ")");
}

struct XTIndex *XTDDForeignKey::getReferenceIndexPtr()
{
	if (!fk_ref_table) {
		xt_register_taberr(XT_REG_CONTEXT, XT_ERR_REF_TABLE_NOT_FOUND, fk_ref_tab_name);
		return NULL;
	}
	if (fk_ref_index >= fk_ref_table->dt_table->tab_dic.dic_key_count) {
		XTDDIndex *in;

		if (!(in = fk_ref_table->findReferenceIndex(this)))
			return NULL;
		if (!checkReferencedTypes(fk_ref_table))
			return NULL;
		fk_ref_index = in->in_index;
	}

	return fk_ref_table->dt_table->tab_dic.dic_keys[fk_ref_index];
}

bool XTDDForeignKey::sameReferenceColumns(XTDDConstraint *co)
{
	u_int i = 0;

	if (fk_ref_cols.size() != co->co_cols.size())
		return false;
	while (i<fk_ref_cols.size()) {
		if (myxt_strcasecmp(fk_ref_cols.itemAt(i)->cr_col_name, co->co_cols.itemAt(i)->cr_col_name) != 0)
			return false;
		i++;
	}
	return OK;
}

bool XTDDForeignKey::checkReferencedTypes(XTDDTable *dt)
{
	XTDDColumn *col, *ref_col;
	XTDDEnumerableColumn *enum_col, *enum_ref_col;

	if (dt->dt_table->tab_dic.dic_tab_flags & XT_TAB_FLAGS_TEMP_TAB) {
		xt_register_xterr(XT_REG_CONTEXT, XT_ERR_FK_REF_TEMP_TABLE);
		return false;
	}

	for (u_int i=0; i<co_cols.size() && i<fk_ref_cols.size(); i++) {
		col = co_table->findColumn(co_cols.itemAt(i)->cr_col_name);
		ref_col = dt->findColumn(fk_ref_cols.itemAt(i)->cr_col_name);
		if (!col || !ref_col)
			continue;

		enum_col = col->castToEnumerable();
		enum_ref_col = ref_col->castToEnumerable();

		if (!enum_col && !enum_ref_col && (strcmp(col->dc_data_type, ref_col->dc_data_type) == 0))
			continue;

		/* Allow match varchar(30) == varchar(40): */
		if (strncmp(col->dc_data_type, "varchar", 7) == 0 && strncmp(ref_col->dc_data_type, "varchar", 7) == 0) {
			char *t1, *t2;
			
			t1 = col->dc_data_type + 7;
			while (*t1 && (isdigit(*t1) || *t1 == '(' || *t1 == ')')) t1++;
			t2 = col->dc_data_type + 7;
			while (*t2 && (isdigit(*t2) || *t2 == '(' || *t2 == ')')) t2++;
			
			if (strcmp(t1, t2) == 0)
				continue;
		}

		/*
		 * MySQL stores ENUMs an integer indexes for string values. That's why
		 * it is ok to have refrences between columns that are different ENUMs as long
		 * as they contain equal number of members, so that for example a cascase update
		 * will not cause an invaid value to be stored in the child table. 
		 *
		 * The above is also true for SETs.
		 *
		 */

		if (enum_col && enum_ref_col && 
			(enum_col->enum_size == enum_ref_col->enum_size) && 
			(enum_col->is_enum == enum_ref_col->is_enum))
			continue;

		xt_register_tabcolerr(XT_REG_CONTEXT, XT_ERR_REF_TYPE_WRONG, fk_ref_tab_name, ref_col->dc_name);
		return false;
	}
	return true;
}

void XTDDForeignKey::removeReference(XTThreadPtr self)
{
	XTDDTable *ref_tab;

	xt_xlock_rwlock(self, &co_table->dt_ref_lock);
	pushr_(xt_unlock_rwlock, &co_table->dt_ref_lock);

	if ((ref_tab = fk_ref_table)) {			
		fk_ref_table = NULL;
		ref_tab->removeReference(self, this);
		xt_heap_release(self, ref_tab->dt_table); /* We referenced the table, not the index! */
	}

	fk_ref_index = UINT_MAX;

	freer_(); // xt_unlock_rwlock(&co_table->dt_ref_lock);
}

/*
 * A row was inserted, check that a key exists in the referenced
 * table.
 */
bool XTDDForeignKey::insertRow(xtWord1 *before_buf, xtWord1 *rec_buf, XTThreadPtr thread)
{
	XTIndexPtr			loc_ind, ind;
	xtBool				no_null = TRUE;
	XTOpenTablePtr		ot;
	XTIdxSearchKeyRec	search_key;
	xtXactID			xn_id;
	XTXactWaitRec		xw;

	/* This lock ensures that the foreign key references are not
	 * changed.
	 */
	xt_slock_rwlock_ns(&co_table->dt_ref_lock);

	if (!(loc_ind = getIndexPtr()))
		goto failed;

	if (!(ind = getReferenceIndexPtr()))
		goto failed;

	search_key.sk_key_value.sv_flags = 0;
	search_key.sk_key_value.sv_rec_id = 0;
	search_key.sk_key_value.sv_row_id = 0;
	search_key.sk_key_value.sv_key = search_key.sk_key_buf;
	search_key.sk_key_value.sv_length = myxt_create_foreign_key_from_row(loc_ind, search_key.sk_key_buf, rec_buf, ind, &no_null);
	search_key.sk_on_key = FALSE;

	if (!no_null)
		goto success;

	if (before_buf) {
		u_int	before_key_len;
		xtWord1	before_key[XT_INDEX_MAX_KEY_SIZE];

		/* If there is a before buffer, this insert was an update, so check
		 * if the key value has changed. If not, we need not do anything.
		 */
		before_key_len = myxt_create_foreign_key_from_row(loc_ind, before_key, before_buf, ind, NULL);
		
		/* Check whether the key value has changed, if not, we have nothing
		 * to do here!
		 */
		if (search_key.sk_key_value.sv_length == before_key_len &&
			memcmp(search_key.sk_key_buf, before_key, before_key_len) == 0)
			goto success;
	}

	/* Search for the key in the parent (referenced) table: */
	if (!(ot = xt_db_open_table_using_tab(fk_ref_table->dt_table, thread)))
		goto failed;

	retry:
	if (!xt_idx_search(ot, ind, &search_key))
		goto failed_2;
		
	while (ot->ot_curr_rec_id) {
		if (!search_key.sk_on_key)
			break;

		switch (xt_tab_maybe_committed(ot, ot->ot_curr_rec_id, &xn_id, &ot->ot_curr_row_id, &ot->ot_curr_updated)) {
			case XT_MAYBE:
				/* We should not get a deadlock here because the thread
				 * that we are waiting for, should not doing
				 * data definition (i.e. should not be trying to
				 * get an exclusive lock on dt_ref_lock.
				 */
				xw.xw_xn_id = xn_id;
				if (!xt_xn_wait_for_xact(thread, &xw, NULL))
					goto failed_2;
				goto retry;			
			case XT_ERR:
				goto failed_2;
			case TRUE:
				/* We found a matching parent: */
				if (ot->ot_ind_rhandle) {
					xt_ind_release_handle(ot->ot_ind_rhandle, FALSE, thread);
					ot->ot_ind_rhandle = NULL;
				}
				xt_db_return_table_to_pool_ns(ot);
				goto success;
			case FALSE:
				if (!xt_idx_next(ot, ind, &search_key))
					goto failed_2;
				break;
		}
	}

	xt_register_ixterr(XT_REG_CONTEXT, XT_ERR_NO_REFERENCED_ROW, co_name);

	failed_2:
	if (ot->ot_ind_rhandle) {
		xt_ind_release_handle(ot->ot_ind_rhandle, FALSE, thread);
		ot->ot_ind_rhandle = NULL;
	}
	xt_db_return_table_to_pool_ns(ot);

	failed:
	xt_unlock_rwlock_ns(&co_table->dt_ref_lock);
	return false;

	success:
	xt_unlock_rwlock_ns(&co_table->dt_ref_lock);
	return true;
}

/*
 * Convert XT_KEY_ACTION_* constants to strings
 */
const char *XTDDForeignKey::actionTypeToString(int action)
{
	switch (action)
	{
	case XT_KEY_ACTION_DEFAULT:
	case XT_KEY_ACTION_RESTRICT:
		return "RESTRICT";
	case XT_KEY_ACTION_CASCADE:
		return "CASCADE";
	case XT_KEY_ACTION_SET_NULL:
		return "SET NULL";
	case XT_KEY_ACTION_SET_DEFAULT:
		return "";
	case XT_KEY_ACTION_NO_ACTION:
		return "NO ACTION";
	}

	return "";
}

void XTDDTable::init(XTThreadPtr self)
{
	xt_init_rwlock_with_autoname(self, &dt_ref_lock);
	dt_trefs = NULL;
}

void XTDDTable::init(XTThreadPtr self, XTObject *obj)
{
	XTDDTable *tab = (XTDDTable *) obj;
	u_int		i;

	init(self);
	XTObject::init(self, obj);
	dt_cols.clone(self, &tab->dt_cols);	
	dt_indexes.clone(self, &tab->dt_indexes);	
	dt_fkeys.clone(self, &tab->dt_fkeys);	

	for (i=0; i<dt_indexes.size(); i++)
		dt_indexes.itemAt(i)->co_table = this;
	for (i=0; i<dt_fkeys.size(); i++)
		dt_fkeys.itemAt(i)->co_table = this;
}

void XTDDTable::finalize(XTThreadPtr self)
{
	XTDDTableRef *ptr;

	removeReferences(self);

	dt_cols.deleteAll(self);
	dt_indexes.deleteAll(self);
	dt_fkeys.deleteAll(self);

	while (dt_trefs) {
		ptr = dt_trefs;
		dt_trefs = dt_trefs->tr_next;
		ptr->release(self);
	}

	xt_free_rwlock(&dt_ref_lock);
}

XTDDColumn *XTDDTable::findColumn(char *name)
{
	XTDDColumn *col;

	for (u_int i=0; i<dt_cols.size(); i++) {
		col = dt_cols.itemAt(i);
		if (myxt_strcasecmp(name, col->dc_name) == 0)
			return col;
	}
	return NULL;
}

void XTDDTable::loadString(XTThreadPtr self, XTStringBufferPtr sb)
{
	u_int i;

	/* I do not specify a table name because that is known */
	xt_sb_concat(self, sb, "CREATE TABLE (\n  ");

	/* We only need to save the foreign key definitions!!
	for (i=0; i<dt_cols.size(); i++) {
		if (i != 0)
			xt_sb_concat(self, sb, ",\n  ");
		dt_cols.itemAt(i)->loadString(self, sb);
	}

	for (i=0; i<dt_indexes.size(); i++) {
		xt_sb_concat(self, sb, ",\n  ");
		dt_indexes.itemAt(i)->loadString(self, sb);
	}
	*/

	for (i=0; i<dt_fkeys.size(); i++) {
		if (i != 0)
			xt_sb_concat(self, sb, ",\n  ");
		dt_fkeys.itemAt(i)->loadString(self, sb);
	}

	xt_sb_concat(self, sb, "\n)\n");
}

void XTDDTable::loadForeignKeyString(XTThreadPtr self, XTStringBufferPtr sb)
{
	for (u_int i=0; i<dt_fkeys.size(); i++) {
		xt_sb_concat(self, sb, ",\n  ");
		dt_fkeys.itemAt(i)->loadString(self, sb);
	}
}

/* Change all references to the given column name to new name. */
void XTDDTable::alterColumnName(XTThreadPtr self, char *from_name, char *to_name)
{
	u_int i;

	/* We only alter references in the foreign keys (we copied the
	 * other changes from MySQL).
	 */
	for (i=0; i<dt_fkeys.size(); i++)
		dt_fkeys.itemAt(i)->alterColumnName(self, from_name, to_name);
}

void XTDDTable::attachReference(XTThreadPtr self, XTDDForeignKey *fk)
{
	XTDDTableRef	*tr;

	/* Remove the reference to this FK if one exists: */
	removeReference(self, fk);

	if (!fk->checkReferencedTypes(this)) {
		if (!self->st_ignore_fkeys)
			throw_();
	}

	xt_xlock_rwlock(self, &dt_ref_lock);
	pushr_(xt_unlock_rwlock, &dt_ref_lock);

	if (!(tr = new XTDDTableRef()))
		xt_throw_errno(XT_CONTEXT, XT_ENOMEM);
	tr->tr_fkey = fk;
	tr->tr_next = dt_trefs;
	dt_trefs = tr;

	/* Reference the database table of the foreign key, not the FK itself.
	 * Just referencing the key will not guarantee that the
	 * table remains valid because the FK does not reference the
	 * table.
	 */
	xt_heap_reference(self, fk->co_table->dt_table);

	freer_(); // xt_unlock_rwlock(&dt_ref_lock);
}

/*
 * Remove the reference to the given foreign key.
 */
void XTDDTable::removeReference(XTThreadPtr self, XTDDForeignKey *fk)
{
	XTDDTableRef	*tr, *prev_tr = NULL;

	xt_xlock_rwlock(self, &dt_ref_lock);
	pushr_(xt_unlock_rwlock, &dt_ref_lock);

	tr = dt_trefs;
	while (tr) {
		if (tr->tr_fkey == fk) {
			if (prev_tr)
				prev_tr->tr_next = tr->tr_next;
			else
				dt_trefs = tr->tr_next;
			break;
		}
		prev_tr = tr;
		tr = tr->tr_next;
	}
	freer_(); // xt_unlock_rwlock(&dt_ref_lock);
	if (tr)
		tr->release(self);
}

void XTDDTable::checkForeignKeyReference(XTThreadPtr self, XTDDForeignKey *fk)
{
	XTDDColumnRef	*cr;

	for (u_int i=0; i<fk->fk_ref_cols.size(); i++) {
		cr = fk->fk_ref_cols.itemAt(i);
		if (!findColumn(cr->cr_col_name))
			xt_throw_tabcolerr(XT_CONTEXT, XT_ERR_COLUMN_NOT_FOUND, fk->fk_ref_tab_name, cr->cr_col_name);
	}
}

void XTDDTable::attachReference(XTThreadPtr self, XTDDTable *dt)
{
	XTDDForeignKey	*fk;

	for (u_int i=0; i<dt_fkeys.size(); i++) {
		fk = dt_fkeys.itemAt(i);
		if (xt_tab_compare_names(fk->fk_ref_tab_name->ps_path, dt->dt_table->tab_name->ps_path) == 0) {
			fk->removeReference(self);

			dt->attachReference(self, fk);

			xt_xlock_rwlock(self, &dt_ref_lock);
			pushr_(xt_unlock_rwlock, &dt_ref_lock);
			/* Referenced the table, not the index!
			 * We do this because we know that if the table is referenced, the
			 * index will remain valid!
			 * This is because the table references the index, and only
			 * releases it when the table is released. The index does not
			 * reference the table though!
			 */
			xt_heap_reference(self, dt->dt_table);
			fk->fk_ref_table = dt;
			freer_(); // xt_unlock_rwlock(&dt_ref_lock);
		}
	}
}

/*
 * This function assumes the database table list is locked!
 */
void XTDDTable::attachReferences(XTThreadPtr self, XTDatabaseHPtr db)
{
	XTDDForeignKey	*fk;
	XTTableHPtr		tab;
	XTDDTable		*dt;
	XTHashEnumRec	tables;

	/* Search for table referenced by this table. */
	for (u_int i=0; i<dt_fkeys.size(); i++) {
		fk = dt_fkeys.itemAt(i);
		fk->removeReference(self);

		// if self-reference
		if (xt_tab_compare_names(fk->fk_ref_tab_name->ps_path, this->dt_table->tab_name->ps_path) == 0)
			fk->fk_ref_table = this;
		else {
			/* get pointer to the referenced table, load it if needed
			 * cyclic references are being handled, absent table is ignored
			 */
			tab = xt_use_table_no_lock(self, db, fk->fk_ref_tab_name, /*TRUE*/FALSE, /*FALSE*/TRUE, NULL, NULL);

			if (tab) {
				pushr_(xt_heap_release, tab);
				if ((dt = tab->tab_dic.dic_table)) {
					// Add a reverse reference:
					dt->attachReference(self, fk);
					xt_heap_reference(self, dt->dt_table); /* Referenced the table, not the index! */
					fk->fk_ref_table = dt;
				}
				freer_(); // xt_heap_release(tab)
			}
			else if (!self->st_ignore_fkeys) {
				xt_throw_taberr(XT_CONTEXT, XT_ERR_REF_TABLE_NOT_FOUND, fk->fk_ref_tab_name);
			}
		}
	}

	/* Search for tables that reference this table. */
	xt_ht_enum(self, dt_table->tab_db->db_tables, &tables);
	while ((tab = (XTTableHPtr) xt_ht_next(self, &tables))) {
		if (tab == this->dt_table) /* no need to re-reference itself, also this fails with "native" pthreads */
			continue;
		xt_heap_reference(self, tab);
		pushr_(xt_heap_release, tab);
		if ((dt = tab->tab_dic.dic_table))
			dt->attachReference(self, this);
		freer_(); // xt_heap_release(tab)
	}
}

void XTDDTable::removeReferences(XTThreadPtr self)
{
	XTDDForeignKey	*fk;
	XTDDTableRef	*tr;
	XTDDTable		*tab;

	xt_xlock_rwlock(self, &dt_ref_lock);
	pushr_(xt_unlock_rwlock, &dt_ref_lock);

	for (u_int i=0; i<dt_fkeys.size(); i++) {
		fk = dt_fkeys.itemAt(i);
		if ((tab = fk->fk_ref_table)) {			
			fk->fk_ref_table = NULL;
			fk->fk_ref_index = UINT_MAX;
			if (tab != this) {
				/* To avoid deadlock we do not hold more than
				 * one lock at a time!
				 */
				freer_(); // xt_unlock_rwlock(&dt_ref_lock);
	
				tab->removeReference(self, fk);
				xt_heap_release(self, tab->dt_table); /* We referenced the table, not the index! */
	
				xt_xlock_rwlock(self, &dt_ref_lock);
				pushr_(xt_unlock_rwlock, &dt_ref_lock);
			}
		}
	}

	while (dt_trefs) {
		tr = dt_trefs;
		dt_trefs = tr->tr_next;
		freer_(); // xt_unlock_rwlock(&dt_ref_lock);
		tr->release(self);
		xt_xlock_rwlock(self, &dt_ref_lock);
		pushr_(xt_unlock_rwlock, &dt_ref_lock);
	}

	freer_(); // xt_unlock_rwlock(&dt_ref_lock);
}

void XTDDTable::checkForeignKeys(XTThreadPtr self, bool temp_table)
{
	XTDDForeignKey	*fk;

	if (temp_table && dt_fkeys.size()) {
		/* Temporary tables cannot have foreign keys: */
		xt_throw_xterr(XT_CONTEXT, XT_ERR_FK_ON_TEMP_TABLE);
		
	}

	/* Search for table referenced by this table. */
	for (u_int i=0; i<dt_fkeys.size(); i++) {
		fk = dt_fkeys.itemAt(i);

		if (fk->fk_on_delete == XT_KEY_ACTION_SET_NULL || fk->fk_on_update == XT_KEY_ACTION_SET_NULL) {
			/* Check that all the columns can be set to NULL! */
			XTDDColumn *col;

			for (u_int j=0; j<fk->co_cols.size(); j++) {
				if ((col = findColumn(fk->co_cols.itemAt(j)->cr_col_name))) {
					if (!col->dc_null_ok)
						xt_throw_tabcolerr(XT_CONTEXT, XT_ERR_COLUMN_IS_NOT_NULL, fk->fk_ref_tab_name, col->dc_name);
				}
			}
		}

		// TODO: dont close table immediately so it can be possibly reused in this loop
		XTTable *ref_tab;

		pushsr_(ref_tab, xt_heap_release, xt_use_table(self, fk->fk_ref_tab_name, FALSE, TRUE, NULL));
		if (ref_tab && !fk->checkReferencedTypes(ref_tab->tab_dic.dic_table))
			throw_();
		freer_();

		/* Currently I allow foreign keys to be created on tables that do not yet exist!
		pushsr_(tab, xt_heap_release, xt_use_table(self, fk->fk_ref_tab_name, FALSE FALSE));
		if ((dt = tab->tab_dic.dic_table))
			dt->checkForeignKeyReference(self, fk);
		freer_(); // xt_heap_release(tab)
		*/
	}
}

XTDDIndex *XTDDTable::findIndex(XTDDConstraint *co)
{
	XTDDIndex *ind;

	for (u_int i=0; i<dt_indexes.size(); i++) {
		ind = dt_indexes.itemAt(i);
		if (co->sameColumns(ind))
			return ind;
	}
	{
		char buffer[XT_ERR_MSG_SIZE - 200];

		co->getColumnList(buffer, XT_ERR_MSG_SIZE - 200);
		xt_register_ixterr(XT_REG_CONTEXT, XT_ERR_NO_MATCHING_INDEX, buffer);
	}
	return NULL;
}

XTDDIndex *XTDDTable::findReferenceIndex(XTDDForeignKey *fk)
{
	XTDDIndex		*ind;
	XTDDColumnRef	*cr;
	u_int			i;

	for (i=0; i<dt_indexes.size(); i++) {
		ind = dt_indexes.itemAt(i);
		if (fk->sameReferenceColumns(ind))
			return ind;
	}

	/* If the index does not exist, maybe the columns do not exist?! */
	for (i=0; i<fk->fk_ref_cols.size(); i++) {
		cr = fk->fk_ref_cols.itemAt(i);
		if (!findColumn(cr->cr_col_name)) {
			xt_register_tabcolerr(XT_REG_CONTEXT, XT_ERR_COLUMN_NOT_FOUND, fk->fk_ref_tab_name, cr->cr_col_name);
			return NULL;
		}
	}
	
	{
		char buffer[XT_ERR_MSG_SIZE - 200];

		fk->getReferenceList(buffer, XT_ERR_MSG_SIZE - 200);
		xt_register_ixterr(XT_REG_CONTEXT, XT_ERR_NO_MATCHING_INDEX, buffer);
	}
	return NULL;
}

bool XTDDTable::insertRow(XTOpenTablePtr ot, xtWord1 *rec_ptr)
{
	bool			ok = true;
	XTInfoBufferRec	rec_buf;

	if (ot->ot_thread->st_ignore_fkeys)
		return true;

	rec_buf.ib_free = FALSE;
	if (!rec_ptr) {
		if (!xt_tab_load_record(ot, ot->ot_curr_rec_id, &rec_buf))
			return false;
		rec_ptr = rec_buf.ib_db.db_data;
		
	}
	for (u_int i=0; i<dt_fkeys.size(); i++) {
		if (!dt_fkeys.itemAt(i)->insertRow(NULL, rec_ptr, ot->ot_thread)) {
			ok = false;
			break;
		}
	}
	xt_ib_free(NULL, &rec_buf);
	return ok;
}

bool XTDDTable::checkNoAction(XTOpenTablePtr ot, xtRecordID rec_id)
{
	XTDDTableRef	*tr;
	bool			ok = true;
	XTInfoBufferRec	rec_buf;
	xtWord1			*rec_ptr;

	if (ot->ot_thread->st_ignore_fkeys)
		return true;

	rec_buf.ib_free = FALSE;
	if (!xt_tab_load_record(ot, rec_id, &rec_buf))
		return false;
	rec_ptr = rec_buf.ib_db.db_data;

	xt_slock_rwlock_ns(&dt_ref_lock);
	tr = dt_trefs;
	while (tr) {
		if (!tr->checkReference(rec_ptr, ot->ot_thread)) {
			ok = false;
			break;
		}
		tr = tr->tr_next;
	}
	xt_unlock_rwlock_ns(&dt_ref_lock);
	xt_ib_free(NULL, &rec_buf);
	return ok;
}

bool XTDDTable::deleteRow(XTOpenTablePtr ot, xtWord1 *rec_ptr)
{
	XTDDTableRef	*tr;
	bool			ok = true;
	XTInfoBufferRec	rec_buf;

	if (ot->ot_thread->st_ignore_fkeys)
		return true;

	rec_buf.ib_free = FALSE;
	if (!rec_ptr) {
		if (!xt_tab_load_record(ot, ot->ot_curr_rec_id, &rec_buf))
			return false;
		rec_ptr = rec_buf.ib_db.db_data;
		
	}
	xt_slock_rwlock_ns(&dt_ref_lock);
	tr = dt_trefs;
	while (tr) {
		if (!tr->modifyRow(ot, rec_ptr, NULL, ot->ot_thread)) {
			ok = false;
			break;
		}
		tr = tr->tr_next;
	}
	xt_unlock_rwlock_ns(&dt_ref_lock);
	xt_ib_free(NULL, &rec_buf);
	return ok;
}

void XTDDTable::deleteAllRows(XTThreadPtr self)
{
	XTDDTableRef	*tr;

	xt_slock_rwlock(self, &dt_ref_lock);
	pushr_(xt_unlock_rwlock, &dt_ref_lock);

	tr = dt_trefs;
	while (tr) {
		tr->deleteAllRows(self);
		tr = tr->tr_next;
	}

	freer_(); // xt_unlock_rwlock(&dt_ref_lock);
}

bool XTDDTable::updateRow(XTOpenTablePtr ot, xtWord1 *before, xtWord1 *after)
{
	XTDDTableRef	*tr;
	bool			ok;
	XTInfoBufferRec	before_buf;

	ASSERT_NS(after);

	if (ot->ot_thread->st_ignore_fkeys)
		return true;

	/* If before is NULL then this is a cascaded
	 * update. In this case there is no need to check
	 * if the column has a parent!!
	 */
	if (before) {
		if (dt_fkeys.size() > 0) {
			for (u_int i=0; i<dt_fkeys.size(); i++) {
				if (!dt_fkeys.itemAt(i)->insertRow(before, after, ot->ot_thread))
					return false;
			}
		}
	}

	ok = true;
	before_buf.ib_free = FALSE;

	xt_slock_rwlock_ns(&dt_ref_lock);
	if ((tr = dt_trefs)) {
		if (!before) {
			if (!xt_tab_load_record(ot, ot->ot_curr_rec_id, &before_buf))
				return false;
			before = before_buf.ib_db.db_data;
		}

		while (tr) {
			if (!tr->modifyRow(ot, before, after, ot->ot_thread)) {
				ok = false;
				break;
			}
			tr = tr->tr_next;
		}
	}
	xt_unlock_rwlock_ns(&dt_ref_lock);
	
	xt_ib_free(NULL, &before_buf);
	return ok;
}

/*
 * drop_db parameter is TRUE if we are dropping the schema of this table. In this case
 * we return TRUE if the table has only refs to the tables from its own schema
 */
xtBool XTDDTable::checkCanDrop(xtBool drop_db)
{
	/* no refs or references only itself */
	if ((dt_trefs == NULL) || ((dt_trefs->tr_next == NULL) && (dt_trefs->tr_fkey->co_table == this)))
		return TRUE;

	if (!drop_db) 
		return FALSE;
	
	const char *this_schema = xt_last_2_names_of_path(dt_table->tab_name->ps_path);
	size_t this_schema_sz = xt_last_name_of_path(dt_table->tab_name->ps_path) - this_schema;
	XTDDTableRef *tr = dt_trefs;

	while (tr) {
		const char *tab_path = tr->tr_fkey->co_table->dt_table->tab_name->ps_path;
		const char *tab_schema = xt_last_2_names_of_path(tab_path);
		size_t tab_schema_sz = xt_last_name_of_path(tab_path) - tab_schema;

		if (this_schema_sz != tab_schema_sz || strncmp(this_schema, tab_schema, tab_schema_sz))
			return FALSE;
		
		tr = tr->tr_next;
	}

	return TRUE;
}
