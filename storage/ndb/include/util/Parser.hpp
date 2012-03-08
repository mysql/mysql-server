/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef CPCD_PARSER_HPP
#define CPCD_PARSER_HPP

#include "Vector.hpp"
#include "Properties.hpp"
#include "InputStream.hpp"

class ParserImpl;
template<class T> struct ParserRow;

//#define PARSER_DEBUG
#ifdef PARSER_DEBUG
#include "NdbOut.hpp"
#define DEBUG(x) \
  ndbout_c("%s:%d:%s", __FILE__, __LINE__, x);
#else 
#define DEBUG(x)
#endif

/**
 * A generic parser
 */
template<class T>
class Parser {
public:
  /**
   * Status for parser
   */
  enum ParserStatus {
    Ok                     = 0,
    Eof                    = 1,
    NoLine                 = 2,
    EmptyLine              = 3,
    UnknownCommand         = 4,
    UnknownArgument        = 5,
    TypeMismatch           = 6,
    InvalidArgumentFormat  = 7,
    UnknownArgumentType    = 8,
    CommandWithoutFunction = 9,
    ArgumentGivenTwice     = 10,
    ExternalStop           = 11,
    MissingMandatoryArgument = 12
  };

  /**
   * Context for parse
   */
  class Context {
  public:
    Context() { m_mutex= NULL; };
    ParserStatus m_status;
    const ParserRow<T> * m_currentCmd;
    const ParserRow<T> * m_currentArg;
    char * m_currentToken;
    STATIC_CONST(MaxParseBytes = 512);
    char m_tokenBuffer[ MaxParseBytes ];
    NdbMutex *m_mutex;

    Vector<const ParserRow<T> *> m_aliasUsed;
  };
  
  /**
   * Initialize parser
   */
  Parser(const ParserRow<T> rows[], class InputStream & in = Stdin,
	 bool breakOnCommand = false, 
	 bool breakOnEmptyLine = true, 
	 bool breakOnInvalidArg = false);
  ~Parser();
  
  /**
   * Run parser
   */
  bool run(Context &, T &, volatile bool * stop = 0) const;

  /**
   * Parse only one entry and return Properties object representing
   * the message
   */
  const Properties *parse(Context &, T &);

  bool getBreakOnCommand() const;
  void setBreakOnCommand(bool v);
  
  bool getBreakOnEmptyLine() const;
  void setBreakOnEmptyLine(bool v);

  bool getBreakOnInvalidArg() const;
  void setBreakOnInvalidArg(bool v);
  
private:
  ParserImpl * impl;
};

template<class T>
struct ParserRow {
public:
  enum Type { Cmd, Arg, CmdAlias, ArgAlias };
  enum ArgType { String, Int, Properties };
  enum ArgRequired { Mandatory, Optional };
  enum ArgMinMax { CheckMinMax, IgnoreMinMax };
  
  const char * name;
  const char * realName; 
  Type type;
  ArgType argType;
  ArgRequired argRequired;
  ArgMinMax argMinMax;
  int minVal;
  int maxVal;
  void (T::* function)(typename Parser<T>::Context & ctx, 
		       const class Properties& args);
  const char * description;
  void *user_value;
};

/**
 * The void* equivalent implementation
 */
class ParserImpl {
public:
  class Dummy {};
  typedef ParserRow<Dummy> DummyRow;
  typedef Parser<Dummy>::Context Context;

  
  ParserImpl(const DummyRow rows[], class InputStream & in,
	     bool b_cmd, bool b_empty, bool b_iarg);
  ~ParserImpl();
  
  bool run(Context *ctx, const class Properties **, volatile bool *) const ;
  
  static const DummyRow* matchCommand(Context*, const char*, const DummyRow*);
  static const DummyRow* matchArg(Context*, const char *, const DummyRow *);
  static bool parseArg(Context*, char*, const DummyRow*, Properties*);
  static bool checkMandatory(Context*, const Properties*);
private:
  const DummyRow * const m_rows;
  class ParseInputStream & input;
  bool m_breakOnEmpty;
  bool m_breakOnCmd;
  bool m_breakOnInvalidArg;
};

template<class T>
inline
Parser<T>::Parser(const ParserRow<T> rows[], class InputStream & in,
		  bool b_cmd, bool b_empty, bool b_iarg){
  impl = new ParserImpl((ParserImpl::DummyRow *)rows, in,
			b_cmd, b_empty, b_iarg);
}

template<class T>
inline
Parser<T>::~Parser(){
  delete impl;
}

template<class T>
inline
bool
Parser<T>::run(Context & ctx, T & t, volatile bool * stop) const {
  const Properties * p;
  DEBUG("Executing Parser<T>::run");
  if(impl->run((ParserImpl::Context*)&ctx, &p, stop)){
    const ParserRow<T> * cmd = ctx.m_currentCmd; // Cast to correct type
    if(cmd == 0){
      /**
       * Should happen if run returns true
       */
      abort();
    }

    for(unsigned i = 0; i<ctx.m_aliasUsed.size(); i++){
      const ParserRow<T> * alias = ctx.m_aliasUsed[i];
      if(alias->function != 0){
	/**
	 * Report alias usage with callback (if specified by user)
	 */
	DEBUG("Alias usage with callback");
	(t.* alias->function)(ctx, * p);
      }
    }

    if(cmd->function == 0){
      ctx.m_status = CommandWithoutFunction;
      DEBUG("CommandWithoutFunction");
      delete p;
      return false;
    }
    (t.* cmd->function)(ctx, * p); // Call the function
    delete p;
    return true;
  }
  DEBUG("");
  return false;
}

template<class T>
inline
const Properties *
Parser<T>::parse(Context &ctx, T &t) {
  const Properties * p;
  volatile bool stop = false;
  DEBUG("Executing Parser<T>::parse");

  if(impl->run((ParserImpl::Context*)&ctx, &p, &stop)){
    const ParserRow<T> * cmd = ctx.m_currentCmd; // Cast to correct type
    if(cmd == 0){
      /**
       * Should happen if run returns true
       */
      abort();
    }

    for(unsigned i = 0; i<ctx.m_aliasUsed.size(); i++){
      const ParserRow<T> * alias = ctx.m_aliasUsed[i];
      if(alias->function != 0){
	/**
	 * Report alias usage with callback (if specified by user)
	 */
	DEBUG("Alias usage with callback");
	(t.* alias->function)(ctx, * p);
      }
    }

    if(cmd->function == 0){
      DEBUG("CommandWithoutFunction");
      ctx.m_status = CommandWithoutFunction;
      return p;
    }
    return p;
  }
  DEBUG("");
  return NULL;
}

template<class T>
inline
bool 
Parser<T>::getBreakOnCommand() const{
  return impl->m_breakOnCmd;
}

template<class T>
inline
void
Parser<T>::setBreakOnCommand(bool v){
  impl->m_breakOnCmd = v;
}

template<class T>
inline
bool
Parser<T>::getBreakOnEmptyLine() const{
  return impl->m_breakOnEmpty;
}
template<class T>
inline
void
Parser<T>::setBreakOnEmptyLine(bool v){
  impl->m_breakOnEmpty = v;
}

template<class T>
inline
bool
Parser<T>::getBreakOnInvalidArg() const{
  return impl->m_breakOnInvalidArg;
}

template<class T>
inline
void
Parser<T>::setBreakOnInvalidArg(bool v){
  impl->m_breakOnInvalidArg = v;
}

#endif
