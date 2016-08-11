/*
 * Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef _MYSQLX_CRUD_H_
#define _MYSQLX_CRUD_H_

#include "mysqlx.h"

namespace Mysqlx
{
  namespace Crud
  {
    class Find;
    class Update;
    class Insert;
    class Delete;
  }

  namespace Datatypes
  {
    class Any;
    class Scalar;
  }
}

namespace mysqlx
{
  class Table;
  class Collection;

  typedef boost::shared_ptr<Table> TableRef;
  typedef boost::shared_ptr<Collection> CollectionRef;

  class Schema : public boost::enable_shared_from_this<Schema>
  {
  public:
    Schema(boost::shared_ptr<Session> conn, const std::string &name);

    TableRef getTable(const std::string &name);
    CollectionRef getCollection(const std::string &name);

    boost::shared_ptr<Session> session() const { return m_sess.lock(); }
    const std::string &name() const { return m_name; }
  private:
    std::map<std::string, boost::shared_ptr<Table> > m_tables;
    std::map<std::string, boost::shared_ptr<Collection> > m_collections;
    boost::weak_ptr<Session> m_sess;
    std::string m_name;
  };

  // -------------------------------------------------------

  class Statement
  {
  public:
    Statement(){};
    Statement(const Statement& other);
    virtual ~Statement();
    virtual boost::shared_ptr<Result> execute() = 0;

  protected:
    std::vector<std::string> m_placeholders;
    std::vector<Mysqlx::Datatypes::Scalar*> m_bound_values;
    void insert_bound_values(::google::protobuf::RepeatedPtrField< ::Mysqlx::Datatypes::Scalar >* target);
    void init_bound_values();
    void validate_bind_placeholder(const std::string& name);
  };

  // -------------------------------------------------------

  class TableValue
  {
  public:
    enum Type
    {
      TInteger,
      TUInteger,
      TNull,
      TDouble,
      TFloat,
      TBool,
      TString,
      TOctets,
      TExpression,
    };

    TableValue(const TableValue &other)
    {
      m_type = other.m_type;
      m_value = other.m_value;
      if (m_type == TString || m_type == TOctets || m_type == TExpression)
        m_value.s = new std::string(*other.m_value.s);
    }

    explicit TableValue(const std::string &s, Type type = TString)
    {
      m_type = type;
      m_value.s = new std::string(s);
    }

    explicit TableValue(int64_t n)
    {
      m_type = TInteger;
      m_value.i = n;
    }

    explicit TableValue(uint64_t n)
    {
      m_type = TUInteger;
      m_value.ui = n;
    }

    explicit TableValue(double n)
    {
      m_type = TDouble;
      m_value.d = n;
    }

    explicit TableValue(float n)
    {
      m_type = TFloat;
      m_value.f = n;
    }

    explicit TableValue(bool n)
    {
      m_type = TBool;
      m_value.b = n;
    }

    explicit TableValue()
    {
      m_type = TNull;
    }

    ~TableValue()
    {
      if (m_type == TString || m_type == TOctets || m_type == TExpression)
        delete m_value.s;
    }

    inline Type type() const { return m_type; }

    inline operator uint64_t () const
    {
      if (m_type != TUInteger)
        throw std::logic_error("type error");
      return m_value.ui;
    }

    inline operator int64_t () const
    {
      if (m_type != TInteger)
        throw std::logic_error("type error");
      return m_value.i;
    }

    inline operator double() const
    {
      if (m_type != TDouble)
        throw std::logic_error("type error");
      return m_value.d;
    }

    inline operator float() const
    {
      if (m_type != TFloat)
        throw std::logic_error("type error");
      return m_value.f;
    }

    inline operator bool() const
    {
      if (m_type != TBool)
        throw std::logic_error("type error");
      return m_value.b;
    }

    inline operator const std::string & () const
    {
      if (m_type != TString && m_type != TOctets && m_type != TExpression)
        throw std::logic_error("type error");
      return *m_value.s;
    }

  private:
    Type m_type;
    union
    {
      std::string *s;
      int64_t i;
      uint64_t ui;
      double d;
      float f;
      bool b;
    } m_value;
  };

  class DocumentValue
  {
  public:
    enum Type
    {
      TNull,
      TString,
      TInteger,
      TFloat,
      TArray,
      TDocument,
      TExpression
    };

    DocumentValue()
    {
      m_type = TNull;
    }

    DocumentValue(const DocumentValue &other)
    {
      m_type = other.m_type;
      m_value = other.m_value;

      if (m_type == TString || m_type == TExpression || m_type == TArray)
        m_value.s = new std::string(*other.m_value.s);
      else if (m_type == TDocument)
        m_value.d = new Document(*other.m_value.d);
    }

    explicit DocumentValue(const std::string &s, Type type = TString)
    {
      m_type = type;
      m_value.s = new std::string(s);
    }

    explicit DocumentValue(int64_t n)
    {
      m_type = TInteger;
      m_value.i = n;
    }

    explicit DocumentValue(uint64_t n)
    {
      m_type = TInteger;
      m_value.i = n;
    }

    explicit DocumentValue(double n)
    {
      m_type = TFloat;
      m_value.f = n;
    }

    explicit DocumentValue(const Document &doc)
    {
      m_type = TDocument;
      m_value.d = new Document(doc);
    }

    ~DocumentValue()
    {
      if (m_type == TDocument)
        delete m_value.d;
      else if (m_type == TString || m_type == TExpression || m_type == TArray)
        delete m_value.s;
    }

    inline Type type() const { return m_type; }

    inline operator int64_t () const
    {
      if (m_type != TInteger)
        throw std::logic_error("type error");
      return m_value.i;
    }

    inline operator double() const
    {
      if (m_type != TFloat)
        throw std::logic_error("type error");
      return m_value.f;
    }

    inline operator const std::string & () const
    {
      if (m_type != TString && m_type != TExpression && m_type != TDocument && m_type != TArray)
        throw std::logic_error("type error");

      if (m_type == TDocument)
        return m_value.d->str();
      else
      return *m_value.s;
    }

    inline operator const Document & () const
    {
      if (m_type != TDocument)
        throw std::logic_error("type error");
      return *m_value.d;
    }

  private:
    Type m_type;
    union
    {
      std::string *s;
      int64_t i;
      double f;
      Document *d;
    } m_value;
  };

  // -------------------------------------------------------

  class Table_Statement : public Statement
  {
  public:
    Table_Statement(boost::shared_ptr<Table> table);
    Table_Statement(const Table_Statement& other);

    Table_Statement &bind(const std::string &name, const TableValue &value);

    boost::shared_ptr<Table> table() const { return m_table; }

  protected:
    Mysqlx::Datatypes::Scalar* convert_table_value(const TableValue& value);

    boost::shared_ptr<Table> m_table;
  };

  // -------------------------------------------------------

  class Select_Base : public Table_Statement
  {
  public:
    Select_Base(boost::shared_ptr<Table> table);
    Select_Base(const Select_Base &other);
    Select_Base &operator = (const Select_Base &other);

    virtual boost::shared_ptr<Result> execute();
  protected:
    boost::shared_ptr<Mysqlx::Crud::Find> m_find;
  };

  class Select_Offset : public Select_Base
  {
  public:
    Select_Offset(boost::shared_ptr<Table> table) : Select_Base(table) {}
    Select_Offset(const Select_Offset &other) : Select_Base(other) {}
    Select_Offset &operator = (const Select_Offset &other) { Select_Base::operator=(other); return *this; }

    Select_Base &offset(uint64_t skip);
  };

  class Select_Limit : public Select_Offset
  {
  public:
    Select_Limit(boost::shared_ptr<Table> table) : Select_Offset(table) {}
    Select_Limit(const Select_Limit &other) : Select_Offset(other) {}
    Select_Limit &operator = (const Select_Limit &other) { Select_Offset::operator=(other); return *this; }

    Select_Offset &limit(uint64_t limit);
  };

  class Select_OrderBy : public Select_Limit
  {
  public:
    Select_OrderBy(boost::shared_ptr<Table> table) : Select_Limit(table) {}
    Select_OrderBy(const Select_OrderBy &other) : Select_Limit(other) {}
    Select_OrderBy &operator = (const Select_OrderBy &other) { Select_Limit::operator=(other); return *this; }

    Select_Limit &orderBy(const std::vector<std::string> &sortFields);
  };

  class Select_Having : public Select_OrderBy
  {
  public:
    Select_Having(boost::shared_ptr<Table> table) : Select_OrderBy(table) {}
    Select_Having(const Select_Having &other) : Select_OrderBy(other) {}
    Select_Having &operator = (const Select_Having &other) { Select_OrderBy::operator=(other); return *this; }

    Select_OrderBy &having(const std::string &searchCondition);
  };

  class Select_GroupBy : public Select_Having
  {
  public:
    Select_GroupBy(boost::shared_ptr<Table> table) : Select_Having(table) {}
    Select_GroupBy(const Select_GroupBy &other) : Select_Having(other) {}
    Select_GroupBy &operator = (const Select_Having &other) { Select_Having::operator=(other); return *this; }

    Select_Having &groupBy(const std::vector<std::string> &searchFields);
  };

  class SelectStatement : public Select_GroupBy
  {
  public:
    SelectStatement(boost::shared_ptr<Table> table, const std::vector<std::string> &fieldList);
    SelectStatement(const SelectStatement &other) : Select_GroupBy(other) {}
    SelectStatement &operator = (const SelectStatement &other) { Select_GroupBy::operator=(other); return *this; }

    Select_GroupBy &where(const std::string &searchCondition);
  };

  // -------------------------------------------------------

  class Insert_Base : public Table_Statement
  {
  public:
    Insert_Base(boost::shared_ptr<Table> table);
    Insert_Base(const Insert_Base &other);
    Insert_Base &operator = (const Insert_Base &other);

    virtual boost::shared_ptr<Result> execute();
  protected:
    boost::shared_ptr<Mysqlx::Crud::Insert> m_insert;
  };

  class Insert_Values : public Insert_Base
  {
  public:
    Insert_Values(boost::shared_ptr<Table> table);
    Insert_Values(const Insert_Base &other);
    Insert_Values &operator = (const Insert_Values &other);

    Insert_Values &values(const std::vector<TableValue> &row);
  };

  class InsertStatement : public Insert_Values
  {
  public:
    InsertStatement(boost::shared_ptr<Table> coll);
    InsertStatement(const InsertStatement &other) : Insert_Values(other) {}
    InsertStatement &operator = (const InsertStatement &other) { Insert_Values::operator=(other); return *this; }

    Insert_Values &insert(const std::vector<std::string> &columns);
  };

  class Delete_Base : public Table_Statement
  {
  public:
    Delete_Base(boost::shared_ptr<Table> table);
    Delete_Base(const Delete_Base &other);
    Delete_Base &operator = (const Delete_Base &other);

    virtual boost::shared_ptr<Result> execute();
  protected:
    boost::shared_ptr<Mysqlx::Crud::Delete> m_delete;
  };

  class Delete_Limit : public Delete_Base
  {
  public:
    Delete_Limit(boost::shared_ptr<Table> table) : Delete_Base(table) {}
    Delete_Limit(const Delete_Limit &other) : Delete_Base(other) {}
    Delete_Limit &operator = (const Delete_Limit &other) { Delete_Base::operator=(other); return *this; }

    Delete_Base &limit(uint64_t limit);
  };

  class Delete_OrderBy : public Delete_Limit
  {
  public:
    Delete_OrderBy(boost::shared_ptr<Table> table) : Delete_Limit(table) {}
    Delete_OrderBy(const Delete_Limit &other) : Delete_Limit(other) {}
    Delete_OrderBy &operator = (const Delete_OrderBy &other) { Delete_Limit::operator=(other); return *this; }

    Delete_Limit &orderBy(const std::vector<std::string> &sortFields);
  };

  class DeleteStatement : public Delete_OrderBy
  {
  public:
    DeleteStatement(boost::shared_ptr<Table> table);
    DeleteStatement(const DeleteStatement &other) : Delete_OrderBy(other) {}
    DeleteStatement &operator = (const DeleteStatement &other) { Delete_OrderBy::operator=(other); return *this; }

    Delete_OrderBy &where(const std::string &searchCondition);
  };

  class Update_Base : public Table_Statement
  {
  public:
    Update_Base(boost::shared_ptr<Table> table);
    Update_Base(const Update_Base &other);
    Update_Base &operator = (const Update_Base &other);

    virtual boost::shared_ptr<Result> execute();
  protected:
    boost::shared_ptr<Mysqlx::Crud::Update> m_update;
  };

  class Update_Limit : public Update_Base
  {
  public:
    Update_Limit(boost::shared_ptr<Table> table) : Update_Base(table) {}
    Update_Limit(const Update_Limit &other) : Update_Base(other) {}
    Update_Limit &operator = (const Update_Limit &other) { Update_Base::operator=(other); return *this; }

    Update_Base &limit(uint64_t limit);
  };

  class Update_OrderBy : public Update_Limit
  {
  public:
    Update_OrderBy(boost::shared_ptr<Table> table) : Update_Limit(table) {}
    Update_OrderBy(const Update_Limit &other) : Update_Limit(other) {}
    Update_OrderBy &operator = (const Update_OrderBy &other) { Update_Limit::operator=(other); return *this; }

    Update_Limit &orderBy(const std::vector<std::string> &sortFields);
  };

  class Update_Where : public Update_OrderBy
  {
  public:
    Update_Where(boost::shared_ptr<Table> table) : Update_OrderBy(table) {}
    Update_Where(const Update_OrderBy &other) : Update_OrderBy(other) {}
    Update_Where &operator = (const Update_Where &other) { Update_OrderBy::operator=(other); return *this; }

    Update_OrderBy &where(const std::string &searchCondition);
  };

  class Update_Set : public Update_Where
  {
  public:
    Update_Set(boost::shared_ptr<Table> table) : Update_Where(table) {}
    Update_Set(const Update_Where &other) : Update_Where(other) {}
    Update_Set &operator = (const Update_Set &other) { Update_Where::operator=(other); return *this; }

    Update_Set &set(const std::string &field, const TableValue& value);
    Update_Set &set(const std::string &field, const std::string& expression);
  };

  class UpdateStatement : public Update_Set
  {
  public:
    UpdateStatement(boost::shared_ptr<Table> table);
    UpdateStatement(const UpdateStatement &other) : Update_Set(other) {}
    UpdateStatement &operator = (const UpdateStatement &other) { Update_Set::operator=(other); return *this; }
  };

  class Table : public boost::enable_shared_from_this<Table>
  {
  public:
    Table(boost::shared_ptr<Schema> schema, const std::string &name);

    boost::shared_ptr<Schema> schema() const { return m_schema.lock(); }
    const std::string& name() const { return m_name; }

    UpdateStatement update();
    DeleteStatement remove();
    InsertStatement insert();
    SelectStatement select(const std::vector<std::string> &fieldList);

  private:
    boost::weak_ptr<Schema> m_schema;
    std::string m_name;
  };

  // -------------------------------------------------------

  class Collection_Statement : public Statement
  {
  public:
    Collection_Statement(boost::shared_ptr<Collection> coll);
    Collection_Statement(const Collection_Statement& other);
    Collection_Statement &bind(const std::string &name, const DocumentValue &value);

    boost::shared_ptr<Collection> collection() const { return m_coll; }

  protected:
    Mysqlx::Datatypes::Scalar* convert_document_value(const DocumentValue& value);

    boost::shared_ptr<Collection> m_coll;
  };

  class Find_Base : public Collection_Statement
  {
  public:
    Find_Base(boost::shared_ptr<Collection> coll);
    Find_Base(const Find_Base &other);
    Find_Base &operator = (const Find_Base &other);

    virtual boost::shared_ptr<Result> execute();
  protected:
    boost::shared_ptr<Mysqlx::Crud::Find> m_find;
  };

  class Find_Skip : public Find_Base
  {
  public:
    Find_Skip(boost::shared_ptr<Collection> coll) : Find_Base(coll) {}
    Find_Skip(const Find_Skip &other) : Find_Base(other) {}
    Find_Skip &operator = (const Find_Skip &other) { Find_Base::operator=(other); return *this; }

    Find_Base &skip(uint64_t skip);
  };

  class Find_Limit : public Find_Skip
  {
  public:
    Find_Limit(boost::shared_ptr<Collection> coll) : Find_Skip(coll) {}
    Find_Limit(const Find_Limit &other) : Find_Skip(other) {}
    Find_Limit &operator = (const Find_Limit &other) { Find_Skip::operator=(other); return *this; }

    Find_Skip &limit(uint64_t limit);
  };

  class Find_Sort : public Find_Limit
  {
  public:
    Find_Sort(boost::shared_ptr<Collection> coll) : Find_Limit(coll) {}
    Find_Sort(const Find_Sort &other) : Find_Limit(other) {}
    Find_Sort &operator = (const Find_Sort &other) { Find_Limit::operator=(other); return *this; }

    Find_Limit &sort(const std::vector<std::string> &sortFields);
  };

  class Find_Having : public Find_Sort
  {
  public:
    Find_Having(boost::shared_ptr<Collection> coll) : Find_Sort(coll) {}
    Find_Having(const Find_Having &other) : Find_Sort(other) {}
    Find_Having &operator = (const Find_Having &other) { Find_Sort::operator=(other); return *this; }

    Find_Sort &having(const std::string &searchCondition);
  };

  class Find_GroupBy : public Find_Having
  {
  public:
    Find_GroupBy(boost::shared_ptr<Collection> coll) : Find_Having(coll) {}
    Find_GroupBy(const Find_GroupBy &other) : Find_Having(other) {}
    Find_GroupBy &operator = (const Find_Having &other) { Find_Having::operator=(other); return *this; }

    Find_Having &groupBy(const std::vector<std::string> &searchFields);
  };

  class FindStatement : public Find_GroupBy
  {
  public:
    FindStatement(boost::shared_ptr<Collection> coll, const std::string &searchCondition);
    FindStatement(const FindStatement &other) : Find_GroupBy(other) {}
    FindStatement &operator = (const FindStatement &other) { Find_GroupBy::operator=(other); return *this; }

    Find_GroupBy &fields(const std::string& projection);
    Find_GroupBy &fields(const std::vector<std::string> &searchFields);
  };

  // -------------------------------------------------------

  class Add_Base : public Collection_Statement
  {
  protected:
    std::vector<std::string> m_last_document_ids;

  public:
    Add_Base(boost::shared_ptr<Collection> coll);
    Add_Base(const Add_Base &other);
    Add_Base &operator = (const Add_Base &other);

    virtual boost::shared_ptr<Result> execute();
  protected:
    boost::shared_ptr<Mysqlx::Crud::Insert> m_insert;
  };

  class AddStatement : public Add_Base
  {
  public:
    AddStatement(boost::shared_ptr<Collection> coll);
    AddStatement(boost::shared_ptr<Collection> coll, const Document &doc);
    AddStatement(const AddStatement &other) : Add_Base(other) {}
    AddStatement &operator = (const AddStatement &other) { Add_Base::operator=(other); return *this; }

    AddStatement &add(const Document &doc);
  };

  // -------------------------------------------------------

  class Modify_Base : public Collection_Statement
  {
  public:
    Modify_Base(boost::shared_ptr<Collection> coll);
    Modify_Base(const Modify_Base &other);
    Modify_Base &operator = (const Modify_Base &other);

    virtual boost::shared_ptr<Result> execute();
  protected:
    boost::shared_ptr<Mysqlx::Crud::Update> m_update;
  };

  class Modify_Limit : public Modify_Base
  {
  public:
    Modify_Limit(boost::shared_ptr<Collection> coll) : Modify_Base(coll) {}
    Modify_Limit(const Modify_Limit &other) : Modify_Base(other) {}
    Modify_Limit &operator = (const Modify_Limit &other) { Modify_Base::operator=(other); return *this; }

    Modify_Base &limit(uint64_t limit);
  };

  class Modify_Sort : public Modify_Limit
  {
  public:
    Modify_Sort(boost::shared_ptr<Collection> coll) : Modify_Limit(coll) {}
    Modify_Sort(const Modify_Sort &other) : Modify_Limit(other) {}
    Modify_Sort &operator = (const Modify_Sort &other) { Modify_Limit::operator=(other); return *this; }

    Modify_Limit &sort(const std::vector<std::string> &sortFields);
  };

  class Modify_Operation : public Modify_Sort
  {
  public:
    Modify_Operation(boost::shared_ptr<Collection> coll) : Modify_Sort(coll) {}
    Modify_Operation(const Modify_Operation &other) : Modify_Sort(other) {}
    Modify_Operation &operator = (const Modify_Operation &other) { Modify_Sort::operator=(other); return *this; }

    Modify_Operation &remove(const std::string &path);
    Modify_Operation &set(const std::string &path, const DocumentValue &value); // For raw values
    Modify_Operation &change(const std::string &path, const DocumentValue &value);
    Modify_Operation &merge(const DocumentValue &value);
    Modify_Operation &arrayInsert(const std::string &path, const DocumentValue &value);
    Modify_Operation &arrayAppend(const std::string &path, const DocumentValue &value);
    Modify_Operation &arrayDelete(const std::string &path);

  private:
    Modify_Operation &set_operation(int type, const std::string &path, const DocumentValue *value = NULL, bool validateArray = false);
  };

  class ModifyStatement : public Modify_Operation
  {
  public:
    ModifyStatement(boost::shared_ptr<Collection> coll, const std::string &searchCondition);
    ModifyStatement(const ModifyStatement &other) : Modify_Operation(other) {}
    ModifyStatement &operator = (const ModifyStatement &other) { Modify_Operation::operator=(other); return *this; }
  };

  // -------------------------------------------------------

  class Remove_Base : public Collection_Statement
  {
  public:
    Remove_Base(boost::shared_ptr<Collection> coll);
    Remove_Base(const Remove_Base &other);
    Remove_Base &operator = (const Remove_Base &other);

    virtual boost::shared_ptr<Result> execute();
  protected:
    boost::shared_ptr<Mysqlx::Crud::Delete> m_delete;
  };

  class Remove_Limit : public Remove_Base
  {
  public:
    Remove_Limit(boost::shared_ptr<Collection> coll) : Remove_Base(coll) {}
    Remove_Limit(const Remove_Limit &other) : Remove_Base(other) {}
    Remove_Limit &operator = (const Remove_Limit &other) { Remove_Base::operator=(other); return *this; }

    Remove_Base &limit(uint64_t limit);
  };

  class RemoveStatement : public Remove_Limit
  {
  public:
    RemoveStatement(boost::shared_ptr<Collection> coll, const std::string &searchCondition);
    RemoveStatement(const RemoveStatement &other) : Remove_Limit(other) {}
    RemoveStatement &operator = (const RemoveStatement &other) { Remove_Limit::operator=(other); return *this; }

    Remove_Limit &sort(const std::vector<std::string> &sortFields);
  };

  // -------------------------------------------------------

  class Collection : public boost::enable_shared_from_this<Collection>
  {
  public:
    Collection(boost::shared_ptr<Schema> schema, const std::string &name);

    boost::shared_ptr<Schema> schema() const { return m_schema.lock(); }
    const std::string& name() const { return m_name; }

    FindStatement find(const std::string &searchCondition);
    AddStatement add(const Document &doc);
    ModifyStatement modify(const std::string &searchCondition);
    RemoveStatement remove(const std::string &searchCondition);

  private:
    boost::weak_ptr<Schema> m_schema;
    std::string m_name;
  };
};

#endif
