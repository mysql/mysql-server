/*
   Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_VECTOR_HPP
#define NDB_VECTOR_HPP

#include <ndb_global.h>
#include <portlib/NdbMutex.h>

template<class T>
class Vector {
public:
  Vector(unsigned sz = 10, unsigned inc_sz = 0);
  int expand(unsigned sz);
  ~Vector();

  T& operator[](unsigned i);
  const T& operator[](unsigned i) const;
  unsigned size() const { return m_size; };
  
  int push_back(const T &);
  int push(const T&, unsigned pos);
  T& set(T&, unsigned pos, T& fill_obj);
  T& back();
  const T& back() const;
  
  void erase(unsigned index);
  
  void clear();
  
  int fill(unsigned new_size, T & obj);

  Vector<T>& operator=(const Vector<T>&);

  /** Does deep copy.*/
  Vector(const Vector&); 
  /**
   * Shallow equal (i.e does memcmp)
   */
  bool equal(const Vector<T>& obj) const;

  int assign(const T*, unsigned cnt);
  int assign(const Vector<T>& obj) { return assign(obj.getBase(), obj.size());}

  T* getBase() { return m_items;}
  const T* getBase() const { return m_items;}
private:
  T * m_items;
  unsigned m_size;
  unsigned m_incSize;
  unsigned m_arraySize;
};

/**
 * BEWARE: Constructing Vector with initial size > 0 is
 * unsafe wrt. catching 'out of memory' errors.
 * (C'tor doesn't return error code)
 * Instead construct Vector with size==0, and then
 * expand() it to the wanted initial size.
 */
template<class T>
Vector<T>::Vector(unsigned sz, unsigned inc_sz):
  m_items(NULL),
  m_size(0),
  m_incSize((inc_sz > 0) ? inc_sz : 50),
  m_arraySize(0)
{
  if (sz == 0)
    return;

  m_items = new T[sz];
  if (m_items == NULL)
  {
    errno = ENOMEM;
    return;
  }
  m_arraySize = sz;
}

template<class T>
int
Vector<T>::expand(unsigned sz){
  if (sz <= m_size)
    return 0;

  T * tmp = new T[sz];
  if(tmp == NULL)
  {
    errno = ENOMEM;
    return -1;
  }
  for (unsigned i = 0; i < m_size; i++)
    tmp[i] = m_items[i];
  delete[] m_items;
  m_items = tmp;
  m_arraySize = sz;
  return 0;
}

/**
 * BEWARE: Copy-constructing a Vector is
 * unsafe wrt. catching 'out of memory' errors.
 * (C'tor doesn't return error code)
 * Instead construct empty Vector (size==0),
 * and then assign() it the initial contents.
 */
template<class T>
Vector<T>::Vector(const Vector& src):
  m_items(NULL),
  m_size(0),
  m_incSize(src.m_incSize),
  m_arraySize(0)
{
  const unsigned sz = src.m_size;
  if (sz == 0)
    return;

  m_items = new T[sz];
  if (unlikely(m_items == NULL)){
    errno = ENOMEM;
    return;
  }
  for(unsigned i = 0; i < sz; i++){
    m_items[i] = src.m_items[i];
  }
  m_arraySize = sz;
  m_size = sz;
}

template<class T>
Vector<T>::~Vector(){
  delete[] m_items;
  // safety for placement new usage
  m_items = 0;
  m_size = 0;
  m_arraySize = 0;
}

template<class T>
T &
Vector<T>::operator[](unsigned i){
  if(i >= m_size)
    abort();
  return m_items[i];
}

template<class T>
const T &
Vector<T>::operator[](unsigned i) const {
  if(i >= m_size)
    abort();
  return m_items[i];
}

template<class T>
T &
Vector<T>::back(){
  if(m_size==0)
    abort();
  return (* this)[m_size - 1];
}

template<class T>
const T &
Vector<T>::back() const {
  if(m_size==0)
    abort();
  return (* this)[m_size - 1];
}

template<class T>
int
Vector<T>::push_back(const T & t){
  if(m_size == m_arraySize){
    const int err = expand(m_arraySize + m_incSize);
    if (unlikely(err))
      return err;
  }
  m_items[m_size] = t;
  m_size++;
  return 0;
}

template<class T>
int
Vector<T>::push(const T & t, unsigned pos)
{
  const int err = push_back(t);
  if (unlikely(err))
    return err;
  if (pos < m_size - 1)
  {
    for(unsigned i = m_size - 1; i > pos; i--)
    {
      m_items[i] = m_items[i-1];
    }
    m_items[pos] = t;
  }
  return 0;
}

template<class T>
T&
Vector<T>::set(T & t, unsigned pos, T& fill_obj)
{
  if (fill(pos, fill_obj))
    abort();
  T& ret = m_items[pos];
  m_items[pos] = t;
  return ret;
}

template<class T>
void
Vector<T>::erase(unsigned i){
  if(i >= m_size)
    abort();
  
  for (unsigned k = i; k + 1 < m_size; k++)
    m_items[k] = m_items[k + 1];
  m_size--;
}

template<class T>
void
Vector<T>::clear(){
  m_size = 0;
}

template<class T>
int
Vector<T>::fill(unsigned new_size, T & obj){
  const int err = expand(new_size);
  if (unlikely(err))
    return err;
  while(m_size <= new_size)
    if (push_back(obj))
      return -1;
  return 0;
}

/**
 * 'operator=' will 'abort()' on 'out of memory' errors.
 *  You may prefer using ::assign()' which returns
 *  an error code instead of aborting.
 */
template<class T>
Vector<T>& 
Vector<T>::operator=(const Vector<T>& obj){
  if(this != &obj){
    clear();
    const int err = expand(obj.size());
    if (unlikely(err))
      abort();
    for(unsigned i = 0; i<obj.size(); i++){
      if (push_back(obj[i]))
        abort();
    }
  }
  return * this;
}

template<class T>
int
Vector<T>::assign(const T* src, unsigned cnt)
{
  if (getBase() == src)
    return 0;  // Self-assign is a NOOP

  clear();
  const int err = expand(cnt);
  if (unlikely(err))
    return err;

  for (unsigned i = 0; i<cnt; i++)
  {
    const int err = push_back(src[i]);
    if (unlikely(err))
      return err;
  }
  return 0;
}

template<class T>
bool
Vector<T>::equal(const Vector<T>& obj) const
{
  if (size() != obj.size())
    return false;

  return memcmp(getBase(), obj.getBase(), size() * sizeof(T)) == 0;
}

template<class T>
class MutexVector : public NdbLockable {
public:
  MutexVector(unsigned sz = 10, unsigned inc_sz = 0);
  int expand(unsigned sz);
  ~MutexVector();

  T& operator[](unsigned i);
  const T& operator[](unsigned i) const;
  unsigned size() const { return m_size; };
  
  int push_back(const T &);
  int push_back(const T &, bool lockMutex);
  T& back();
  const T& back() const;
  
  void erase(unsigned index);
  void erase(unsigned index, bool lockMutex);

  void clear();
  void clear(bool lockMutex);

  int fill(unsigned new_size, T & obj);
private:
  // Don't allow copy and assignment of MutexVector
  MutexVector(const MutexVector&); 
  MutexVector<T>& operator=(const MutexVector<T>&);

  T * m_items;
  unsigned m_size;
  unsigned m_incSize;
  unsigned m_arraySize;
};

/**
 * BEWARE: Constructing MutexVector with initial size > 0 is
 * unsafe wrt. catching 'out of memory' errors.
 * (C'tor doesn't return error code)
 * Instead construct MutexVector with size==0, and then
 * expand() it to the wanted initial size.
 */
template<class T>
MutexVector<T>::MutexVector(unsigned sz, unsigned inc_sz):
  m_items(NULL),
  m_size(0),
  m_incSize((inc_sz > 0) ? inc_sz : 50),
  m_arraySize(0)
{
  if (sz == 0)
    return;

  m_items = new T[sz];
  if (m_items == NULL)
  {
    errno = ENOMEM;
    return;
  }
  m_arraySize = sz;
}

template<class T>
int
MutexVector<T>::expand(unsigned sz){
  if (sz <= m_size)
    return 0;

  T * tmp = new T[sz];
  if(tmp == NULL)
  {
    errno = ENOMEM;
    return -1;
  }
  for (unsigned i = 0; i < m_size; i++)
    tmp[i] = m_items[i];
  delete[] m_items;
  m_items = tmp;
  m_arraySize = sz;
  return 0;
}

template<class T>
MutexVector<T>::~MutexVector(){
  delete[] m_items;
  // safety for placement new usage
  m_items = 0;
  m_size = 0;
  m_arraySize = 0;
}

template<class T>
T &
MutexVector<T>::operator[](unsigned i){
  if(i >= m_size)
    abort();
  return m_items[i];
}

template<class T>
const T &
MutexVector<T>::operator[](unsigned i) const {
  if(i >= m_size)
    abort();
  return m_items[i];
}

template<class T>
T &
MutexVector<T>::back(){
  if(m_size==0)
    abort();
  return (* this)[m_size - 1];
}

template<class T>
const T &
MutexVector<T>::back() const {
  if(m_size==0)
    abort();
  return (* this)[m_size - 1];
}

template<class T>
int
MutexVector<T>::push_back(const T & t){
  lock();
  if(m_size == m_arraySize){
    const int err = expand(m_arraySize + m_incSize);
    if (unlikely(err))
    {
      unlock();
      return err;
    }
  }
  m_items[m_size] = t;
  m_size++;
  unlock();
  return 0;
}

template<class T>
int
MutexVector<T>::push_back(const T & t, bool lockMutex){
  if(lockMutex) 
    lock();
  if(m_size == m_arraySize){
    const int err = expand(m_arraySize + m_incSize);
    if (unlikely(err))
    {
      if(lockMutex) 
        unlock();
      return err;
    }
  }
  m_items[m_size] = t;
  m_size++;
  if(lockMutex)
    unlock();
  return 0;
}

template<class T>
void
MutexVector<T>::erase(unsigned i){
  if(i >= m_size)
    abort();
  
  lock();
  for (unsigned k = i; k + 1 < m_size; k++)
    m_items[k] = m_items[k + 1];
  m_size--;
  unlock();
}

template<class T>
void
MutexVector<T>::erase(unsigned i, bool _lock){
  if(i >= m_size)
    abort();
  
  if(_lock) 
    lock();
  for (unsigned k = i; k + 1 < m_size; k++)
    m_items[k] = m_items[k + 1];
  m_size--;
  if(_lock) 
    unlock();
}

template<class T>
void
MutexVector<T>::clear(){
  lock();
  m_size = 0;
  unlock();
}

template<class T>
void
MutexVector<T>::clear(bool l){
  if(l) lock();
  m_size = 0;
  if(l) unlock();
}

template<class T>
int
MutexVector<T>::fill(unsigned new_size, T & obj){
  while(m_size <= new_size)
    if (push_back(obj))
      return -1;
  return 0;
}

#endif
