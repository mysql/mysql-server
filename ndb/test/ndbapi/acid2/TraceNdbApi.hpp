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


#ifndef TraceNdbApi_hpp
#define TraceNdbApi_hpp


class CTraceNdbSchemaOp : public NdbSchemaOp
{
public:
  int createTable(const char* aTableName);
  int createAttribute(const char* aAttrName, KeyType aTupleyKey);
};



class CTraceNdbSchemaCon : public NdbSchemaCon
{
public:
  CTraceNdbSchemaOp* getNdbSchemaOp(); 
  int execute();
};



class CTraceNdbRecAttr : public NdbRecAttr
{
public:
  Uint32 u_32_value();		
};


class CTraceNdbOperation : public NdbOperation
{
public:
  int insertTuple();
  int updateTuple();
  int interpretedUpdateTuple();
  int readTuple();				
  int readTupleExclusive();
  int deleteTuple();
  int equal(const char* anAttrName, Uint32 aValue);	
  int setValue(const char* anAttrName, Uint32 aValue);
  int incValue(const char* anAttrName, Uint32 aValue);
  CTraceNdbRecAttr* getValue(const char* anAttrName);

};


class CTraceNdbIndexOperation : public NdbIndexOperation
{
public:
  int insertTuple();
  int updateTuple();
  int interpretedUpdateTuple();
  int readTuple();
  int readTupleExclusive();
  int deleteTuple();
  int equal(const char* anAttrName, Uint32 aValue);
  int setValue(const char* anAttrName, Uint32 aValue);
  int incValue(const char* anAttrName, Uint32 aValue);
  CTraceNdbRecAttr* getValue(const char* anAttrName);
};



class CTraceNdbConnection : public NdbConnection
{
public:
  CTraceNdbOperation* getNdbOperation(const char* aTableName);
  CTraceNdbIndexOperation* getNdbIndexOperation(const char* anIndexName, const char* aTableName);

  int execute(ExecType aTypeOfExec);

  int execute_ok(ExecType aTypeOfExec)
  {
    return execute(aTypeOfExec);
  };

	const NdbError & getNdbError(void) const;
};



class CTraceNdbDictionary : public NdbDictionary
{
public:
	class CTraceTable : public Table
	{
	};

	class CTraceIndex : public Index
	{
	};

	class CTraceColumn : public Column
	{
	};

    int createTable(const CTraceTable &);
    int createIndex(const CTraceIndex &);
};



class CTraceNdb : public Ndb
{
public:
  CTraceNdb(const char* aDataBase);
  CTraceNdbSchemaCon* startSchemaTransaction();
  void closeSchemaTransaction(CTraceNdbSchemaCon* aSchemaCon);
  CTraceNdbConnection* startTransaction();
  void closeTransaction(CTraceNdbConnection* aConnection);
};



#endif // TraceNdbApi_hpp
