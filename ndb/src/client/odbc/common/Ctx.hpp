/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef ODBC_COMMON_Ctx_hpp
#define ODBC_COMMON_Ctx_hpp

#include <NdbDictionary.hpp>

class Ndb;
class NdbConnection;
class NdbOperation;
class NdbSchemaCon;
class NdbSchemaOp;
class NdbError;

class Sqlstate;
class DiagArea;
class CtxOwner;

#ifndef MAX_PATH
#define MAX_PATH	1024
#endif

/**
 * @class Error
 * @brief Sql state, error codes, and message
 */
struct Error {
    enum {
	Gen = NDB_ODBC_ERROR_MIN + 1		// unclassified
    };
    explicit Error(const Sqlstate& sqlstate);
    const Sqlstate& m_sqlstate;
    int m_status;
    int m_classification;
    int m_code;
    const char* m_message;
    const char* m_sqlFunction;
    bool driverError() const;
};

inline
Error::Error(const Sqlstate& sqlstate) :
    m_sqlstate(sqlstate),
    m_status(0),
    m_classification(0),
    m_code(0),
    m_sqlFunction(0)
{
}

inline bool
Error::driverError() const
{
    return NDB_ODBC_ERROR_MIN <= m_code && m_code < NDB_ODBC_ERROR_MAX;
}

#define ctx_assert(x)	\
    do { if (x) break; throw CtxAssert(__FILE__, __LINE__); } while (0)

/**
 * @class Assert
 * @brief Assert thrown
 */
class CtxAssert {
public:
    CtxAssert(const char* file, int line);
    const char* const m_file;
    const int m_line;
};

/**
 * @class Ctx
 * @brief Context for one ODBC SQL function
 * 
 * Local to the function (not member of the handle) because methods on
 * a handle can call methods on other handles.  Created in driver/
 * before method calls and saved under the handle on return.  Contains
 * diag area for the function.  Also used as logger.
 */
class Ctx {
public:
    Ctx();
    ~Ctx();
    // handle exceptions
    void handleEx(CtxAssert& ctxAssert);
    // logging methods
    int logLevel() const;
    void log(const char* fmt, ...) PRINTFLIKE(2,3);
    void logSqlEnter(const char* sqlFunction);
    void logSqlExit();
    const char* sqlFunction() const;
    void print(const char* fmt, ...) PRINTFLIKE(2,3);
    void print(int level, const char* fmt, ...) PRINTFLIKE(3,4);
    // diagnostics area.
    DiagArea& diagArea() const;
    DiagArea& diagArea();
    SQLRETURN getCode() const;
    void setCode(SQLRETURN ret);
    // push diagnostic record
    void pushStatus(const Sqlstate& state, SQLINTEGER code, const char* fmt, ...) PRINTFLIKE(4,5);
    void pushStatus(SQLINTEGER code, const char* fmt, ...) PRINTFLIKE(3,4);
    void pushStatus(const NdbError& ndbError, const char* fmt, ...) PRINTFLIKE(3,4);
    void pushStatus(const Ndb* ndb, const char* fmt, ...) PRINTFLIKE(3,4);
    void pushStatus(const Ndb* ndb, const NdbConnection* tcon, const NdbOperation* op, const char* fmt, ...) PRINTFLIKE(5,6);
    void pushStatus(const Ndb* ndb, const NdbSchemaCon* scon, const NdbSchemaOp* op, const char* fmt, ...) PRINTFLIKE(5,6);
    // check if we should continue executing.
    bool ok();
private:
    static int m_logLevel;
    static char m_szTraceFile[MAX_PATH];
    char m_sqlFunction[32];	// max needed is 20
    DiagArea* m_diagArea;
};

inline int
Ctx::logLevel() const
{
    return m_logLevel;
}

inline const char*
Ctx::sqlFunction() const
{
    return m_sqlFunction;
}

// logging macros can be used only when ctx is in scope

#define ctx_logN(n, x)	\
    do { if (ctx.logLevel() < (n)) break; ctx.log x; } while (0)

#if NDB_ODBC_MAX_LOG_LEVEL >= 0
#define ctx_log0(x)	ctx_logN(0, x)
#else
#define ctx_log0(x)
#endif

#if NDB_ODBC_MAX_LOG_LEVEL >= 1
#define ctx_log1(x)	ctx_logN(1, x)
#else
#define ctx_log1(x)
#endif

#if NDB_ODBC_MAX_LOG_LEVEL >= 2
#define ctx_log2(x)	ctx_logN(2, x)
#else
#define ctx_log2(x)
#endif

#if NDB_ODBC_MAX_LOG_LEVEL >= 3
#define ctx_log3(x)	ctx_logN(3, x)
#else
#define ctx_log3(x)
#endif

#if NDB_ODBC_MAX_LOG_LEVEL >= 4
#define ctx_log4(x)	ctx_logN(4, x)
#else
#define ctx_log4(x)
#endif

#if NDB_ODBC_MAX_LOG_LEVEL >= 5
#define ctx_log5(x)	ctx_logN(5, x)
#else
#define ctx_log5(x)
#endif

#endif
