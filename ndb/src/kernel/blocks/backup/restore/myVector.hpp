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

#ifndef MY_VECTOR_HPP
#define MY_VECTOR_HPP

// Template class for std::vector-like class (hopefully works in OSE)
template <class T>
class myVector
{
  
  // Note that last element in array is used for end() and is always empty
  int sizeIncrement;
  int thisSize;
  int used;
  T *storage;
  
public:
  
  // Assignment of whole vector
  myVector<T> & operator=(myVector<T> & org) {
    
    // Don't copy if they point to the same address
    if (!(this == &org)) {
      // Check memory space
      if (thisSize < org.thisSize) {
	// We have to increase memory for destination
	T* tmpStorage = new T[org.thisSize];
	delete[] storage;
	storage = tmpStorage;
      } // if
      thisSize = org.thisSize;
      sizeIncrement = org.sizeIncrement;
      used = org.used;
      for (int i = 0; i < thisSize; i++) {
	storage[i] = org.storage[i];
      } // for
    } // if
    return *this;
  } // operator=
  
  // Construct with size s+1
  myVector(int s = 1) : sizeIncrement(5), // sizeIncrement(s),
    thisSize(s + 1), 
    used(0), 
    storage(new T[s + 1]) { } 
  
  ~myVector() { delete[] storage; } // Destructor: deallocate memory
  
  T& operator[](int i) { // Return by index
    if ((i < 0) || (i >= used)) {
      // Index error
      ndbout << "vector index out of range" << endl;
      abort();
      return storage[used - 1];
    } // if
    else {
      return storage[i];
    } // else
  } // operator[]

  const T& operator[](int i) const { // Return by index
    if ((i < 0) || (i >= used)) {
      // Index error
      ndbout << "vector index out of range" << endl;
      abort();
      return storage[used - 1];
    } // if
    else {
      return storage[i];
    } // else
  } // operator[]
  
  int getSize() const { return used; }
  
  void push_back (T& item) {
    if (used >= thisSize - 1) {
      // We have to allocate new storage
      int newSize = thisSize + sizeIncrement;
      T* tmpStorage = new T[newSize];
      if (tmpStorage == NULL) {
	// Memory allocation error! break
	ndbout << "PANIC: Memory allocation error in vector" << endl;
	return;
      } // if
      thisSize = newSize;
      for (int i = 0; i < used; i++) {
	tmpStorage[i] = storage[i];
      } // for
      delete[] storage;
      storage = tmpStorage;
    } // if
    
    // Now push
    storage[used] = item; 
    used++;
  }; // myVector<> push_back()
  
  // Remove item at back
  void pop_back() {
    if (used > 0) {
      used--; 
    } // if
  } // pop_back()
  
  int size() const { return used; };
  
  bool empty() const { return(used == 0); } 

  void clear() {
    used = 0;
  }
};

#endif
