/*
   Copyright (C) 2009 Sun Microsystems Inc
    All rights reserved. Use is subject to license terms.

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


#ifndef NdbQueryBuilderImpl_H
#define NdbQueryBuilderImpl_H


#include <Vector.hpp>
#include "NdbQueryBuilder.hpp"

class NdbQueryBuilderImpl;

class NdbQueryDefImpl : public NdbQueryDef
{
  friend class NdbQueryDef;

public:
  NdbQueryDefImpl(const NdbQueryBuilderImpl& builder);
  ~NdbQueryDefImpl();

private:
  Vector<const NdbQueryOperationDef*> m_operations;
//Vector<NdbParamOperand*> m_paramOperand;
//Vector<NdbConstOperand*> m_constOperand;
//Vector<NdbLinkedOperand*> m_linkedOperand;
}; // class NdbQueryDefImpl


class NdbQueryBuilderImpl
{
  friend class NdbQueryBuilder;
  friend NdbQueryDefImpl::NdbQueryDefImpl(const NdbQueryBuilderImpl& builder);

public:
  ~NdbQueryBuilderImpl();
  NdbQueryBuilderImpl(Ndb& ndb);

  class NdbQueryDef* prepare();

  const NdbError& getNdbError() const;

  void setErrorCode(int aErrorCode)
  { if (!m_error.code)
      m_error.code = aErrorCode;
  }

private:
  bool hasError() const
  { return (m_error.code!=0); }

  bool contains(const NdbQueryOperationDef*);

  Ndb& m_ndb;
  NdbError m_error;

  Vector<const NdbQueryOperationDef*> m_operations;
  Vector<const NdbParamOperand*> m_paramOperands;
  Vector<const NdbConstOperand*> m_constOperands;
  Vector<const NdbLinkedOperand*> m_linkedOperands;

}; // class NdbQueryBuilderImpl



#endif
