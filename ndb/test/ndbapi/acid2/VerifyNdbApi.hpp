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


#ifndef VerifyNdbApi_hpp
#define VerifyNdbApi_hpp


class CVerifyNdbSchemaOp : public NdbSchemaOp
{
public:
  int createTable(const char* aTableName)
  {
    int i = NdbSchemaOp::createTable(aTableName);
    VerifyInt(i, "createTable");
    return i;
  };

  int createAttribute(const char* aAttrName, KeyType aTupleyKey)
  {
    int i = NdbSchemaOp::createAttribute(aAttrName, aTupleyKey);
    VerifyInt(i, "createAttribute");
    return i;
  };

private:
  void VerifyInt(const int i, const char* szMethod)
  {
    if(i)
    {
      VerifyIntError(i, szMethod);
    }
  }

  void VerifyIntError(const int i, const char* szMethod);
};



class CVerifyNdbSchemaCon : public NdbSchemaCon
{
public:
  CVerifyNdbSchemaOp* getNdbSchemaOp()
  {
    NdbSchemaOp* p = NdbSchemaCon::getNdbSchemaOp();
    VerifyPtr(p, "getNdbSchemaOp");
    return (CVerifyNdbSchemaOp*)p;
  };

  int execute()
  {
    int i = NdbSchemaCon::execute();
    VerifyInt(i, "execute");
    return i;
  };

private:
  void VerifyInt(const int i, const char* szMethod)
  {
    if(i)
    {
      VerifyIntError(i, szMethod);
    }
  }

  void VerifyPtr(void* p, const char* szMethod)
  {
    if(!p)
    {
      VerifyPtrError(p, szMethod);
    }
  }

  void VerifyIntError(const int i, const char* szMethod);
  void VerifyPtrError(void* p, const char* szMethod);
};



class CVerifyNdbRecAttr : public NdbRecAttr
{
public:
  Uint32 u_32_value()
  {
    Uint32 n = NdbRecAttr::u_32_value();
    VerifyValue("u_32_value");
    return n;
  };
	
private:
  void VerifyValue(const char* szMethod)
  {
    int iNull = NdbRecAttr::isNULL();
    if(iNull)
    {
      VerifyValueError(iNull, szMethod);
    }
  };

  void VerifyValueError(const int iNull, const char* szMethod);
};


class CVerifyNdbOperation : public NdbOperation
{
public:
  int insertTuple()
  {
    int i = NdbOperation::insertTuple();
    VerifyInt(i, "insertTuple");
    return i;
  };

  int updateTuple()
  {
    int i = NdbOperation::updateTuple();
    VerifyInt(i, "updateTuple");
    return i;
  };

  int interpretedUpdateTuple()
  {
    int i = NdbOperation::interpretedUpdateTuple();
    VerifyInt(i, "interpretedUpdateTuple");
    return i;
  }

  int readTuple()
  {
    int i = NdbOperation::readTuple();				
    VerifyInt(i, "readTuple");
    return i;
  }

  int readTupleExclusive()
  {
    int i = NdbOperation::readTupleExclusive();				
    VerifyInt(i, "readTupleExclusive");
    return i;
  }

  int deleteTuple()
  {
    int i = NdbOperation::deleteTuple();
    VerifyInt(i, "deleteTuple");
    return i;
  }

  int equal(const char* anAttrName, Uint32 aValue)
  {
    int i = NdbOperation::equal(anAttrName, aValue);
    VerifyInt(i, "equal");
    return i;
  }

  int setValue(const char* anAttrName, Uint32 aValue)
  {
    int i = NdbOperation::setValue(anAttrName, aValue);
    VerifyInt(i, "setValue");
    return i;
  }

  int incValue(const char* anAttrName, Uint32 aValue)
  {
    int i = NdbOperation::incValue(anAttrName, aValue);
    VerifyInt(i, "incValue");
    return i;
  }

  CVerifyNdbRecAttr* getValue(const char* anAttrName)
  {
    NdbRecAttr* p = NdbOperation::getValue(anAttrName);
    VerifyPtr(p, "getValue");
    return (CVerifyNdbRecAttr*)p;
  }


private:
  void VerifyInt(const int i, const char* szMethod)
  {
    if(i)
    {
      VerifyIntError(i, szMethod);
    }
  }

  void VerifyPtr(void* p, const char* szMethod)
  {
    if(!p)
    {
      VerifyPtrError(p, szMethod);
    }
  }

  void VerifyIntError(const int i, const char* szMethod);
  void VerifyPtrError(void* p, const char* szMethod);
};


class CVerifyNdbIndexOperation : public NdbIndexOperation
{
public:
  int insertTuple()
  {
    int i = NdbIndexOperation::insertTuple();
    VerifyInt(i, "insertTuple");
    return i;
  };

  int updateTuple()
  {
    int i = NdbIndexOperation::updateTuple();
    VerifyInt(i, "updateTuple");
    return i;
  };

  int interpretedUpdateTuple()
  {
    int i = NdbIndexOperation::interpretedUpdateTuple();
    VerifyInt(i, "interpretedUpdateTuple");
    return i;
  }

  int readTuple()
  {
    int i = NdbIndexOperation::readTuple();				
    VerifyInt(i, "readTuple");
    return i;
  }

  int readTupleExclusive()
  {
    int i = NdbIndexOperation::readTupleExclusive();				
    VerifyInt(i, "readTupleExclusive");
    return i;
  }

  int deleteTuple()
  {
    int i = NdbIndexOperation::deleteTuple();
    VerifyInt(i, "deleteTuple");
    return i;
  }

  int equal(const char* anAttrName, Uint32 aValue)
  {
    int i = NdbIndexOperation::equal(anAttrName, aValue);
    VerifyInt(i, "equal");
    return i;
  }

  int setValue(const char* anAttrName, Uint32 aValue)
  {
    int i = NdbIndexOperation::setValue(anAttrName, aValue);
    VerifyInt(i, "setValue");
    return i;
  }

  int incValue(const char* anAttrName, Uint32 aValue)
  {
    int i = NdbIndexOperation::incValue(anAttrName, aValue);
    VerifyInt(i, "incValue");
    return i;
  }

  CVerifyNdbRecAttr* getValue(const char* anAttrName)
  {
    NdbRecAttr* p = NdbIndexOperation::getValue(anAttrName);
    VerifyPtr(p, "getValue");
    return (CVerifyNdbRecAttr*)p;
  }


private:
  void VerifyInt(const int i, const char* szMethod)
  {
    if(i)
    {
      VerifyIntError(i, szMethod);
    }
  }

  void VerifyPtr(void* p, const char* szMethod)
  {
    if(!p)
    {
      VerifyPtrError(p, szMethod);
    }
  }

  void VerifyIntError(const int i, const char* szMethod);
  void VerifyPtrError(void* p, const char* szMethod);
};


class CVerifyNdbConnection : public NdbConnection
{
public:
  CVerifyNdbOperation* getNdbOperation(const char* aTableName)
  {
    NdbOperation* p = NdbConnection::getNdbOperation(aTableName);
    VerifyPtr(p, "getNdbOperation");
    return (CVerifyNdbOperation*)p;
  }

  CVerifyNdbIndexOperation* getNdbIndexOperation(const char* anIndexName, const char* aTableName)
  {
    NdbIndexOperation* p = NdbConnection::getNdbIndexOperation(anIndexName, aTableName);
    VerifyPtr(p, "getNdbIndexOperation");
    return (CVerifyNdbIndexOperation*)p;
  }

  int execute(ExecType aTypeOfExec)
  {
    int i = NdbConnection::execute(aTypeOfExec);
    VerifyInt(i, "execute");
    return i;
  }

  int execute_ok(ExecType aTypeOfExec)
  {
    int iExec = NdbConnection::execute(aTypeOfExec);
    NdbError err = NdbConnection::getNdbError();
    int iCode = err.code;
    if(iExec 
       && ((aTypeOfExec==NoCommit && iCode!=0) 
        || (aTypeOfExec==Commit && iCode!=626 && iCode!=630)))
    {
      VerifyInt(iExec, "execute");
    }
    return iExec;
  }


private:
  void VerifyInt(const int i, const char* szMethod)
  {
    if(i)
    {
      VerifyIntError(i, szMethod);
    }
  }

  void VerifyPtr(void* p, const char* szMethod)
  {
    if(!p)
    {
      VerifyPtrError(p, szMethod);
    }
  }

  void VerifyIntError(const int i, const char* szMethod);
  void VerifyPtrError(void* p, const char* szMethod);
};


//class CVerifyTable : public NdbDictionary::Table
//{
//public:
//};


class CVerifyNdbDictionary : public NdbDictionary
{
public:
	class CVerifyTable : public Table
	{
	public:
	private:
	};

	class CVerifyIndex : public Index
	{
	public:
	private:
	};

	class CVerifyColumn : public Column
	{
	public:
	private:
	};

    int createTable(const CVerifyTable &);
    int createIndex(const CVerifyIndex &);


private:
};


class CVerifyNdb : public Ndb
{
public:
  CVerifyNdb(const char* aDataBase)
  : Ndb(aDataBase)
  {
    VerifyVoid("Ndb");
  };

  CVerifyNdbSchemaCon* startSchemaTransaction()
  {
    NdbSchemaCon* p = Ndb::startSchemaTransaction();
    VerifyPtr(p, "startSchemaTransaction");
    return (CVerifyNdbSchemaCon*)p;
  };

  void closeSchemaTransaction(CVerifyNdbSchemaCon* aSchemaCon)
  {
    Ndb::closeSchemaTransaction(aSchemaCon);
    VerifyVoid("closeSchemaTransaction");
  };

  CVerifyNdbConnection* startTransaction()
  {
    NdbConnection* p = Ndb::startTransaction();
    VerifyPtr(p, "startTransaction");
    return (CVerifyNdbConnection*)p;
  };

  void closeTransaction(CVerifyNdbConnection* aConnection)
  {
    Ndb::closeTransaction(aConnection);
    VerifyVoid("closeTransaction");
  };


private:
  void VerifyPtr(void* p, const char* szMethod)
  {
    if(!p)
    {
      VerifyPtrError(p, szMethod);
    }
  }

  void VerifyVoid(const char* szMethod)
  {
    NdbError err = Ndb::getNdbError();
    int iCode = err.code;
    if(iCode)
    {
      VerifyVoidError(iCode, szMethod);
    }
  }

  void VerifyPtrError(void* p, const char* szMethod);
  void VerifyVoidError(const int iCode, const char* szMethod);
};



#endif // VerifyNdbApi_hpp
