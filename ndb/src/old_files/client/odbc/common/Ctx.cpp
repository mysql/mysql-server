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

#include <NdbApi.hpp>
#include <common/common.hpp>
#include "DiagArea.hpp"

// ctor

Ctx::Ctx() :
    m_diagArea(0)	// create on demand
{
    const char* p;
    if ((p = getenv("NDB_ODBC_TRACE")) != 0)
	m_logLevel = atoi(p);
    if ((p = getenv("NDB_ODBC_TRACE_FILE")) != 0 && *p != 0)
	strcpy(m_szTraceFile, p);
}

Ctx::~Ctx()
{
    delete m_diagArea;
    m_diagArea = 0;
}

// handle exceptions

CtxAssert::CtxAssert(const char* file, int line) :
    m_file(file),
    m_line(line)
{
    const char* p;
    if ((p = getenv("NDB_ODBC_DEBUG")) != 0 && atoi(p) != 0) {
	char buf[200];
	snprintf(buf, sizeof(buf), "%s, line %d: assert failed\n", m_file, m_line);
	if ((p = getenv("NDB_ODBC_TRACE_FILE")) != 0 && *p != 0) {
	    FILE* pFile = fopen(p, "a");
	    fprintf(pFile, buf);
	    fflush(pFile);
	    fclose(pFile);
	} else {
	    fprintf(stderr, buf);
	    fflush(stderr);
	}
	abort();
	exit(1);
    }
}

void
Ctx::handleEx(CtxAssert& ctxAssert)
{
    pushStatus(Sqlstate::_IM001, Error::Gen, "exception at %s line %d", ctxAssert.m_file, ctxAssert.m_line);
}

// logging methods

int Ctx::m_logLevel = 0;
char Ctx::m_szTraceFile[MAX_PATH];

void
Ctx::log(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    if (m_szTraceFile[0]) {
	FILE* pFile = fopen(m_szTraceFile, "a");
	fprintf(pFile, "[NdbOdbc] ");
	vfprintf(pFile, fmt, ap);
	fprintf(pFile, "\n");
	fflush(pFile);
	fclose(pFile);
    } else {
	printf("[NdbOdbc] ");
	vprintf(fmt, ap);
	printf("\n");
	fflush(stdout);
    }
    va_end(ap);
}

void
Ctx::logSqlEnter(const char* sqlFunction)
{
    Ctx& ctx = *this;
    snprintf(m_sqlFunction, sizeof(m_sqlFunction), "%s", sqlFunction);
    ctx_log3(("%s", m_sqlFunction));
}

void
Ctx::logSqlExit()
{
    Ctx& ctx = *this;
    if (m_diagArea == 0) {
	ctx_log3(("%s ret=%d", m_sqlFunction, getCode()));
	return;
    }
    int logLevel = diagArea().numStatus() != 0 ? 2 : 3;
    ctx_logN(logLevel, ("%s ret=%d diag=%d", m_sqlFunction, diagArea().getCode(), diagArea().numStatus()));
    for (unsigned i = 1; i <= diagArea().numStatus(); i++) {
	OdbcData state;
	OdbcData message;
	diagArea().getRecord(ctx, i, SQL_DIAG_SQLSTATE, state);
	diagArea().getRecord(ctx, i, SQL_DIAG_MESSAGE_TEXT, message);
	ctx_logN(logLevel, ("diag %u: %s - %s", i, state.sqlstate().state(), message.sqlchar()));
    }
}

void
Ctx::print(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    if (m_szTraceFile[0]) {
	FILE* pFile = fopen(m_szTraceFile, "a");
	vfprintf(pFile, fmt, ap);
	unsigned n = strlen(fmt);
	if (n > 0 && fmt[n-1] == '\n')
	    fflush(pFile);
	fclose(pFile);
    } else {
	vprintf(fmt, ap);
	unsigned n = strlen(fmt);
	if (n > 0 && fmt[n-1] == '\n')
	    fflush(stdout);
    }
    va_end(ap);
}

void
Ctx::print(int level, const char* fmt, ...)
{
    if (level > m_logLevel)
	return;
    va_list ap;
    va_start(ap, fmt);
    if (m_szTraceFile[0]) {
	FILE* pFile = fopen(m_szTraceFile, "a");
	vfprintf(pFile, fmt, ap);
	unsigned n = strlen(fmt);
	if (n > 0 && fmt[n-1] == '\n')
	    fflush(pFile);
	fclose(pFile);
    } else {
	vprintf(fmt, ap);
	unsigned n = strlen(fmt);
	if (n > 0 && fmt[n-1] == '\n')
	    fflush(stdout);
    }
    va_end(ap);
}

// diagnostics

static const unsigned MessageSize = 512;

DiagArea&
Ctx::diagArea() const
{
    ctx_assert(m_diagArea != 0);
    return *m_diagArea;
}

DiagArea&
Ctx::diagArea()
{
    if (m_diagArea == 0)
	m_diagArea = new DiagArea;
    return *m_diagArea;
}

SQLRETURN
Ctx::getCode() const
{
    if (m_diagArea == 0)
	return SQL_SUCCESS;
    return diagArea().getCode();
}

void
Ctx::setCode(SQLRETURN ret)
{
    diagArea().setCode(ret);
}

void
Ctx::pushStatus(const Sqlstate& state, SQLINTEGER code, const char* fmt, ...)
{
    char message[MessageSize];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(message, sizeof(message), fmt, ap);
    va_end(ap);
    Error error(state);
    error.m_status = NdbError::PermanentError;
    error.m_classification = NdbError::ApplicationError;
    error.m_code = code;
    error.m_message = message;
    error.m_sqlFunction = m_sqlFunction;
    diagArea().pushStatus(error);
}

void
Ctx::pushStatus(SQLINTEGER code, const char* fmt, ...)
{
    char message[MessageSize];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(message, sizeof(message), fmt, ap);
    va_end(ap);
    Error error(Sqlstate::_IM000);
    error.m_status = NdbError::PermanentError;
    error.m_classification = NdbError::ApplicationError;
    error.m_code = code;
    error.m_message = message;
    error.m_sqlFunction = m_sqlFunction;
    diagArea().pushStatus(error);
}

void
Ctx::pushStatus(const NdbError& ndbError, const char* fmt, ...)
{
    char message[MessageSize];
    va_list ap;
    va_start(ap, fmt);
    snprintf(message, sizeof(message), "%s", ndbError.message);
    snprintf(message + strlen(message), sizeof(message) - strlen(message), "%s", " - at ");
    vsnprintf(message + strlen(message), sizeof(message) - strlen(message), fmt, ap);
    va_end(ap);
    Error error(Sqlstate::_IM000);
    error.m_status = ndbError.status;
    error.m_classification = ndbError.classification;
    error.m_code = ndbError.code;
    error.m_message = message;
    error.m_sqlFunction = m_sqlFunction;
    diagArea().pushStatus(error);
}

void
Ctx::pushStatus(const Ndb* ndb, const char* fmt, ...)
{
    char message[MessageSize];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(message, sizeof(message), fmt, ap);
    va_end(ap);
    bool found = false;
    if (ndb != 0) {
	const NdbError& ndbError = ndb->getNdbError();
	if (ndbError.code != 0) {
	    pushStatus(ndbError, "%s", message);
	    found = true;
	}
    }
    if (! found) {
	pushStatus(Error::Gen, "unknown NDB error");
    }
}

void
Ctx::pushStatus(const Ndb* ndb, const NdbConnection* tcon, const NdbOperation* op, const char* fmt, ...)
{
    char message[MessageSize];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(message, sizeof(message), fmt, ap);
    va_end(ap);
    bool found = false;
    if (op != 0) {
	const NdbError& ndbError = op->getNdbError();
	if (ndbError.code != 0) {
	    pushStatus(ndbError, "%s", message);
	    found = true;
	}
    }
    if (tcon != 0) {
	const NdbError& ndbError = tcon->getNdbError();
	if (ndbError.code != 0) {
	    pushStatus(ndbError, "%s", message);
	    found = true;
	}
    }
    if (ndb != 0) {
	const NdbError& ndbError = ndb->getNdbError();
	if (ndbError.code != 0) {
	    pushStatus(ndbError, "%s", message);
	    found = true;
	}
    }
    if (! found) {
	pushStatus(Error::Gen, "unknown NDB error");
    }
}

void
Ctx::pushStatus(const Ndb* ndb, const NdbSchemaCon* scon, const NdbSchemaOp* op, const char* fmt, ...)
{
    char message[MessageSize];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(message, sizeof(message), fmt, ap);
    va_end(ap);
    bool found = false;
    if (op != 0) {
	const NdbError& ndbError = op->getNdbError();
	if (ndbError.code != 0) {
	    pushStatus(ndbError, "%s", message);
	    found = true;
	}
    }
    if (scon != 0) {
	const NdbError& ndbError = scon->getNdbError();
	if (ndbError.code != 0) {
	    pushStatus(ndbError, "%s", message);
	    found = true;
	}
    }
    if (ndb != 0) {
	const NdbError& ndbError = ndb->getNdbError();
	if (ndbError.code != 0) {
	    pushStatus(ndbError, "%s", message);
	    found = true;
	}
    }
    if (! found) {
	pushStatus(Error::Gen, "unknown NDB error");
    }
}

// check for error

bool
Ctx::ok()
{
    if (m_diagArea == 0)
	return true;
    if (diagArea().getCode() == SQL_SUCCESS)
	return true;
    if (diagArea().getCode() == SQL_SUCCESS_WITH_INFO)
	return true;
    return false;
}
