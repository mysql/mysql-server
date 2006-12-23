/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


#include <ndb_global.h>

#include "Parser.hpp"
#include <NdbOut.hpp>
#include <Properties.hpp>

#undef DEBUG
#define DEBUG(x) ndbout << x << endl;

static void trim(char * str);

class ParseInputStream : public InputStream {
public:
  ParseInputStream(InputStream & in, bool trim = true, char eofComment = '#');
  
  char* gets(char * buf, int bufLen); 
  void push_back(const char *);
  void set_mutex(NdbMutex *m) { in.set_mutex(m); };
private:
  InputStream & in;
  char * buffer;
};

ParseInputStream::ParseInputStream(InputStream & _in, 
				   bool /* unused */, 
				   char /* unused */)
  : in(_in){
  buffer = 0;
}

char*
ParseInputStream::gets(char * buf, int bufLen){
  if(buffer != 0){
    strncpy(buf, buffer, bufLen);
    free(buffer);
    buffer = 0;
    return buf;
  }
  char *t = in.gets(buf, bufLen);
  return t;
}

void
ParseInputStream::push_back(const char * str){
  if(buffer != 0)
    abort();
  buffer = strdup(str);
}

ParserImpl::ParserImpl(const DummyRow * rows, InputStream & in,
		       bool b_cmd, bool b_empty, bool b_iarg) 
  : m_rows(rows), input(* new ParseInputStream(in))
{
  m_breakOnCmd = b_cmd;
  m_breakOnEmpty = b_empty;
  m_breakOnInvalidArg = b_iarg;
}

ParserImpl::~ParserImpl(){
  delete & input;
}

static
bool
Empty(const char * str){
  if(str == 0)
    return true;
  const int len = strlen(str);
  if(len == 0)
    return false;
  for(int i = 0; i<len; i++)
    if(str[i] != ' ' && str[i] != '\t' && str[i] != '\n')
      return false;
  return true;
}

static
bool
Eof(const char * str) { return str == 0;}

static
void
trim(char * str){
  if(str == NULL)
    return;
  int len = strlen(str);
  for(len--; str[len] == '\n' || str[len] == ' ' || str[len] == '\t'; len--)
    str[len] = 0;
  
  int pos = 0;
  while(str[pos] == ' ' || str[pos] == '\t')
    pos++;
  
  if(str[pos] == '\"' && str[len] == '\"') {
    pos++;
    str[len] = 0;
    len--;
  }
  
  memmove(str, &str[pos], len - pos + 2);
}

static
bool
split(char * buf, char ** name, char ** value){
  
  * value = strchr(buf, ':');
  if(* value == 0)
    * value = strchr(buf, '=');


  if(* value == 0){
    return false;
  }
  (* value)[0] = 0;
  * value = (* value + 1);
  * name = buf;
  
  trim(* name);
  trim(* value);

  return true;
}

bool
ParserImpl::run(Context * ctx, const class Properties ** pDst,
		volatile bool * stop) const
{
  DBUG_ENTER("ParserImpl::run");

  input.set_mutex(ctx->m_mutex);

  * pDst = 0;
  bool ownStop = false;
  if(stop == 0)
    stop = &ownStop;

  ctx->m_aliasUsed.clear();

  const unsigned sz = sizeof(ctx->m_tokenBuffer);
  ctx->m_currentToken = input.gets(ctx->m_tokenBuffer, sz);
  if(Eof(ctx->m_currentToken)){
    ctx->m_status = Parser<Dummy>::Eof;
    DBUG_RETURN(false);
  }

  int last= strlen(ctx->m_currentToken);
  if(last>0)
    last--;

  if(ctx->m_currentToken[last] !='\n'){
    ctx->m_status = Parser<Dummy>::NoLine;
    ctx->m_tokenBuffer[0]= '\0';
    DBUG_RETURN(false);
  }

  if(Empty(ctx->m_currentToken)){
    ctx->m_status = Parser<Dummy>::EmptyLine;
    DBUG_RETURN(false);
  }

  trim(ctx->m_currentToken);
  ctx->m_currentCmd = matchCommand(ctx, ctx->m_currentToken, m_rows);
  if(ctx->m_currentCmd == 0){
    ctx->m_status = Parser<Dummy>::UnknownCommand;
    DBUG_RETURN(false);
  }

  Properties * p = new Properties();

  bool invalidArgument = false;
  ctx->m_currentToken = input.gets(ctx->m_tokenBuffer, sz);

  while((! * stop) &&
	!Eof(ctx->m_currentToken) &&
	!Empty(ctx->m_currentToken)){
    if(ctx->m_currentToken[0] != 0){
      trim(ctx->m_currentToken);
      if(!parseArg(ctx, ctx->m_currentToken, ctx->m_currentCmd + 1, p)){
	delete p;
	invalidArgument = true;
	break;
      }
    }
    ctx->m_currentToken = input.gets(ctx->m_tokenBuffer, sz);
  }

  if(invalidArgument){
    char buf[sz];
    char * tmp;
    if(!m_breakOnInvalidArg){
      do {
	tmp = input.gets(buf, sz);
      } while((! * stop) && !Eof(tmp) && !Empty(tmp));
    }
    DBUG_RETURN(false);
  }

  if(* stop){
    delete p;
    ctx->m_status = Parser<Dummy>::ExternalStop;
    DBUG_RETURN(false);
  }

  if(!checkMandatory(ctx, p)){
    ctx->m_status = Parser<Dummy>::MissingMandatoryArgument;
    delete p;
    DBUG_RETURN(false);
  }

  /**
   * Add alias to properties
   */
  for(unsigned i = 0; i<ctx->m_aliasUsed.size(); i++){
    const ParserRow<Dummy> * alias = ctx->m_aliasUsed[i];
    Properties tmp;
    tmp.put("name", alias->name);
    tmp.put("realName", alias->realName);
    p->put("$ALIAS", i, &tmp);
  }
  p->put("$ALIAS", ctx->m_aliasUsed.size());

  ctx->m_status = Parser<Dummy>::Ok;
  * pDst = p;
  DBUG_RETURN(true);
}

const ParserImpl::DummyRow* 
ParserImpl::matchCommand(Context* ctx, const char* buf, const DummyRow rows[]){
  const char * name = buf;
  const DummyRow * tmp = &rows[0];
  while(tmp->name != 0 && name != 0){
    if(strcmp(tmp->name, name) == 0){
      if(tmp->type == DummyRow::Cmd)
	return tmp;
      if(tmp->type == DummyRow::CmdAlias){
	if(ctx != 0)
	  ctx->m_aliasUsed.push_back(tmp);
	name = tmp->realName;
	tmp = &rows[0];
	continue;
      }
    }
    tmp++;
  }
  return 0;
}

const ParserImpl::DummyRow* 
ParserImpl::matchArg(Context* ctx, const char * buf, const DummyRow rows[]){
  const char * name = buf;
  const DummyRow * tmp = &rows[0];
  while(tmp->name != 0){
    const DummyRow::Type t = tmp->type;
    if(t != DummyRow::Arg && t != DummyRow::ArgAlias && t !=DummyRow::CmdAlias)
      break;
    if(t !=DummyRow::CmdAlias && strcmp(tmp->name, name) == 0){
      if(tmp->type == DummyRow::Arg){
	return tmp;
      }
      if(tmp->type == DummyRow::ArgAlias){
	if(ctx != 0)
	  ctx->m_aliasUsed.push_back(tmp);
	name = tmp->realName;
	tmp = &rows[0];
	continue;
      }
    }
    tmp++;
  }
  return 0;
}

bool
ParserImpl::parseArg(Context * ctx,
		     char * buf, 
		     const DummyRow * rows,
		     Properties * p){
  char * name;
  char * value;
  if(!split(buf, &name, &value)){
    ctx->m_status = Parser<Dummy>::InvalidArgumentFormat;
    return false;
  }
  const DummyRow * arg = matchArg(ctx, name, rows);
  if(arg == 0){
    ctx->m_status = Parser<Dummy>::UnknownArgument;
    return false;
  }
  
  switch(arg->argType){
  case DummyRow::String:
    if(p->put(arg->name, value))
      return true;
    break;
  case DummyRow::Int:{
    Uint32 i;
    int c = sscanf(value, "%u", &i);
    if(c != 1){
      ctx->m_status = Parser<Dummy>::TypeMismatch;
      return false;
    }
    if(p->put(arg->name, i))
      return true;
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
  if(p->getPropertiesErrno() == E_PROPERTIES_ELEMENT_ALREADY_EXISTS){
    ctx->m_status = Parser<Dummy>::ArgumentGivenTwice;
    return false;
  }

  abort();
}

bool
ParserImpl::checkMandatory(Context* ctx, const Properties* props){
  const DummyRow * tmp = &ctx->m_currentCmd[1];
  while(tmp->name != 0 && tmp->type == DummyRow::Arg){
    if(tmp->argRequired == ParserRow<Dummy>::Mandatory &&
       !props->contains(tmp->name)){
      ctx->m_status = Parser<Dummy>::MissingMandatoryArgument;
      ctx->m_currentArg = tmp;
      return false;
    }
    tmp++;
  }
  return true;
}

template class Vector<const ParserRow<ParserImpl::Dummy>*>;
