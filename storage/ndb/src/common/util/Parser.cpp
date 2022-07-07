/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <ndb_global.h>

#include <Properties.hpp>
#include "Parser.hpp"

void ParserImpl::check_parser_rows(const DummyRow* rows) const {
  // Simple validation of rows definitions
  while (rows->name) {
    assert(rows->type < rows->End);
    rows++;
  }

  // Check that last row has type End
  assert(rows->type == rows->End);
}

ParserImpl::ParserImpl(const DummyRow* rows, InputStream& in)
    : m_rows(rows), input(in) {
#ifndef NDEBUG
  check_parser_rows(rows);
#endif
}

ParserImpl::~ParserImpl() {}

static bool Empty(const char* str) {
  if (str == nullptr) return true;
  const int len = (int)strlen(str);
  if (len == 0) return false;
  for (int i = 0; i < len; i++)
    if (str[i] != ' ' && str[i] != '\t' && str[i] != '\n') return false;
  return true;
}

static bool Eof(const char* str) { return str == nullptr; }

static void trim(char* str) {
  if (str == nullptr) return;
  int len = (int)strlen(str);
  for (len--; str[len] == '\n' || str[len] == ' ' || str[len] == '\t'; len--)
    str[len] = 0;

  int pos = 0;
  while (str[pos] == ' ' || str[pos] == '\t') pos++;

  if (str[pos] == '\"' && str[len] == '\"') {
    pos++;
    str[len] = 0;
    len--;
  }

  memmove(str, &str[pos], len - pos + 2);
}

static bool split(char* buf, char** name, char** value) {
  for (*value = buf; **value; (*value)++) {
    if (**value == ':' || **value == '=') {
      break;
    }
  }

  if (*value == nullptr) {
    return false;
  }
  (*value)[0] = 0;
  *value = (*value + 1);
  *name = buf;

  trim(*name);
  trim(*value);

  return true;
}

bool ParserImpl::run(Context* ctx, const class Properties** pDst,
                     volatile bool* stop) const {
  input.set_mutex(ctx->m_mutex);

  *pDst = nullptr;
  bool ownStop = false;
  if (stop == nullptr) stop = &ownStop;

  ctx->m_aliasUsed.clear();

  const unsigned sz = sizeof(ctx->m_tokenBuffer);
  ctx->m_currentToken = input.gets(ctx->m_tokenBuffer, sz);
  if (Eof(ctx->m_currentToken)) {
    ctx->m_status = Parser<Dummy>::Eof;
    return false;
  }

  int last = (int)strlen(ctx->m_currentToken);
  if (last > 0) last--;

  if (ctx->m_currentToken[last] != '\n') {
    ctx->m_status = Parser<Dummy>::NoLine;
    ctx->m_tokenBuffer[0] = '\0';
    return false;
  }

  if (Empty(ctx->m_currentToken)) {
    ctx->m_status = Parser<Dummy>::EmptyLine;
    return false;
  }

  trim(ctx->m_currentToken);
  ctx->m_currentCmd = matchCommand(ctx, ctx->m_currentToken, m_rows);
  if (ctx->m_currentCmd == nullptr) {
    ctx->m_status = Parser<Dummy>::UnknownCommand;
    return false;
  }

  Properties* p = new Properties();

  bool invalidArgument = false;
  ctx->m_currentToken = input.gets(ctx->m_tokenBuffer, sz);

  while ((!*stop) && !Eof(ctx->m_currentToken) && !Empty(ctx->m_currentToken)) {
    if (ctx->m_currentToken[0] != 0) {
      trim(ctx->m_currentToken);
      if (!parseArg(ctx, ctx->m_currentToken, ctx->m_currentCmd + 1, p)) {
        delete p;
        invalidArgument = true;
        break;
      }
    }
    ctx->m_currentToken = input.gets(ctx->m_tokenBuffer, sz);
  }

  if (invalidArgument) {
    // Invalid argument found, return error
    return false;
  }

  if (*stop) {
    delete p;
    ctx->m_status = Parser<Dummy>::ExternalStop;
    return false;
  }

  if (!checkMandatory(ctx, p)) {
    ctx->m_status = Parser<Dummy>::MissingMandatoryArgument;
    delete p;
    return false;
  }

  /**
   * Add alias to properties
   */
  for (unsigned i = 0; i < ctx->m_aliasUsed.size(); i++) {
    const ParserRow<Dummy>* alias = ctx->m_aliasUsed[i];
    Properties tmp;
    tmp.put("name", alias->name);
    tmp.put("realName", alias->realName);
    p->put("$ALIAS", i, &tmp);
  }
  p->put("$ALIAS", ctx->m_aliasUsed.size());

  ctx->m_status = Parser<Dummy>::Ok;
  *pDst = p;
  return true;
}

const ParserImpl::DummyRow* ParserImpl::matchCommand(Context* ctx,
                                                     const char* buf,
                                                     const DummyRow rows[]) {
  const char* name = buf;
  const DummyRow* tmp = &rows[0];
  while (tmp->name != nullptr && name != nullptr) {
    if (strcmp(tmp->name, name) == 0) {
      if (tmp->type == DummyRow::Cmd) return tmp;
      if (tmp->type == DummyRow::CmdAlias) {
        if (ctx != nullptr) ctx->m_aliasUsed.push_back(tmp);
        name = tmp->realName;
        tmp = &rows[0];
        continue;
      }
    }
    tmp++;
  }
  return nullptr;
}

const ParserImpl::DummyRow* ParserImpl::matchArg(Context* ctx, const char* buf,
                                                 const DummyRow rows[]) {
  const char* name = buf;
  const DummyRow* tmp = &rows[0];
  while (tmp->name != nullptr) {
    const DummyRow::Type t = tmp->type;
    if (t != DummyRow::Arg && t != DummyRow::ArgAlias &&
        t != DummyRow::CmdAlias)
      break;
    if (t != DummyRow::CmdAlias && strcmp(tmp->name, name) == 0) {
      if (tmp->type == DummyRow::Arg) {
        return tmp;
      }
      if (tmp->type == DummyRow::ArgAlias) {
        if (ctx != nullptr) ctx->m_aliasUsed.push_back(tmp);
        name = tmp->realName;
        tmp = &rows[0];
        continue;
      }
    } else if (strcmp(tmp->name, "") == 0) {
      return tmp;
    }
    tmp++;
  }
  return nullptr;
}

bool ParserImpl::parseArg(Context* ctx, char* buf, const DummyRow* rows,
                          Properties* p) {
  char* name;
  char* value;
  if (!split(buf, &name, &value)) {
    ctx->m_status = Parser<Dummy>::InvalidArgumentFormat;
    return false;
  }
  const bool append = (name[0] == '+');
  if (append) {
    // skip passed the plus sign
    name++;
  }
  const DummyRow* arg = matchArg(ctx, name, rows);
  if (arg == nullptr) {
    ctx->m_status = Parser<Dummy>::UnknownArgument;
    return false;
  }
  if (arg->argRequired == ParserRow<Dummy>::Ignore) {
    return true;
  }

  if (append && arg->argType != DummyRow::LongString) {
    // only LongString supports append
    ctx->m_status = Parser<Dummy>::TypeMismatch;
    return false;
  }
  switch (arg->argType) {
    case DummyRow::LongString:
      if (append)
        return p->append(arg->name, value);
      else
        return p->put(arg->name, value);
      break;
    case DummyRow::String:
      return p->put(arg->name, value);
      break;
    case DummyRow::Int: {
      Uint32 i;
      int c = sscanf(value, "%u", &i);
      if (c != 1) {
        ctx->m_status = Parser<Dummy>::TypeMismatch;
        return false;
      }
      if (p->put(arg->name, i)) return true;
      break;
    }

    case DummyRow::Properties: {
      abort();
      break;
    }
    default:
      ctx->m_status = Parser<Dummy>::UnknownArgumentType;
      return false;
  }
  if (p->getPropertiesErrno() == E_PROPERTIES_ELEMENT_ALREADY_EXISTS) {
    ctx->m_status = Parser<Dummy>::ArgumentGivenTwice;
    return false;
  }

  abort();
  return false;
}

bool ParserImpl::checkMandatory(Context* ctx, const Properties* props) {
  const DummyRow* tmp = &ctx->m_currentCmd[1];
  while (tmp->name != nullptr && tmp->type == DummyRow::Arg) {
    if (tmp->argRequired == ParserRow<Dummy>::Mandatory &&
        !props->contains(tmp->name)) {
      ctx->m_status = Parser<Dummy>::MissingMandatoryArgument;
      ctx->m_currentArg = tmp;
      return false;
    }
    tmp++;
  }
  return true;
}

template class Vector<const ParserRow<ParserImpl::Dummy>*>;

#ifdef TEST_PARSER
#include <NdbTap.hpp>

TAPTEST(Parser) {
  char *str, *name, *value;

  // split modifies arg so dup
  str = strdup("x=c:\\windows");
  OK(split(str, &name, &value));
  OK(!strcmp(name, "x"));
  OK(!strcmp(value, "c:\\windows"));
  free(str);

  return 1;
}
#endif
