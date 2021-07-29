// File: $Id$
// Author: John Wu <John.Wu at nersc.gov> Lawrence Berkeley National Laboratory
// Copyright (c) 2000-2016 the Regents of the University of California
#ifndef IBIS_COLVALUES_H
#define IBIS_COLVALUES_H
#include "column.h"
#include "utilidor.h"	// ibis::util::reorder

///@file
/// A set of utility classes for storing the selected values.

/// @ingroup FastBitIBIS
/// A pure virtual base class for storing selected values in memory.
class FASTBIT_CXX_DLLSPEC ibis::colValues {
public:
    virtual ~colValues() {}

    static colValues* create(const ibis::column* c);
    static colValues* create(const ibis::column* c,
			     const ibis::bitvector& hits);
    static colValues* create(const ibis::column* c,
			     ibis::fileManager::storage* store,
			     const uint32_t start, const uint32_t end);

    /// Provide a pointer to the column containing the selected values.
    const ibis::column* operator->() const {return col;}
    const ibis::column* columnPointer() const {return col;}
    /// Name.
    const char* name() const {return(col!=0?col->name():0);}

    virtual bool empty() const = 0;
    virtual void reduce(const array_t<uint32_t>& starts) = 0;
    virtual void reduce(const array_t<uint32_t>& starts,
			ibis::selectClause::AGREGADO func) = 0;
    virtual void erase(uint32_t i, uint32_t j) = 0;
    virtual void swap(uint32_t i, uint32_t j) = 0;
    virtual uint32_t size() const = 0;
    virtual uint32_t elementSize() const = 0;
    /// Return the type of the data stored.
    virtual ibis::TYPE_T getType() const =0;
    /// Return the pointer to the pointer to underlying array_t<T> object.
    virtual void* getArray() const =0;
    /// Make sure the content of the underlying storage is not shared.
    virtual void nosharing() =0;

    bool canSort() const
    {return (col ? col->type() != ibis::TEXT : false);}

    void swap(colValues& rhs) { // swap two colValues
	const ibis::column* c = rhs.col;
	rhs.col = col;
	col = c;
    }

    /// Write out whole array as binary.
    virtual long write(FILE* fptr) const  = 0;
    /// Write ith element as text.
    virtual void write(std::ostream& out, uint32_t i) const = 0;

    /// Sort rows in the range <code>[i, j)</code>.
    virtual void sort(uint32_t i, uint32_t j, bundle* bdl) = 0;
    /// Sort rows in the range <code>[i, j)</code>.  Also sort the columns
    /// between <code>[head, tail)</code>.
    virtual void sort(uint32_t i, uint32_t j, bundle* bdl,
		      colList::iterator head, colList::iterator tail) = 0;
    /// Sort rows in the range <code>[i, j)</code>.  Output the new order
    /// in array @c neworder.
    virtual void sort(uint32_t i, uint32_t j,
		      array_t<uint32_t>& neworder) const = 0;
    /// Reorder the values according to the specified indices.  <code>
    /// New[i] = Old[ind[i]]</code>.
    virtual void reorder(const array_t<uint32_t>& ind) = 0;
    /// Produce an array of the starting positions of values that are the
    /// same.
    virtual array_t<uint32_t>*
    segment(const array_t<uint32_t>* old=0) const = 0;
    /// Truncate the number element to no more than @c keep.
    virtual long truncate(uint32_t keep) = 0;
    /// Truncate the number element to no more than @c keep.
    virtual long truncate(uint32_t keep, uint32_t start) = 0;
    /// Return the positions of the @c k largest elements.
    virtual void topk(uint32_t k, array_t<uint32_t> &ind) const = 0;
    /// Return the positions of the @c k smallest elements.
    virtual void bottomk(uint32_t k, array_t<uint32_t> &ind) const = 0;

    virtual double getMin() const = 0;
    virtual double getMax() const = 0;
    virtual double getSum() const = 0;
    virtual int32_t getInt(uint32_t) const = 0;
    virtual uint32_t getUInt(uint32_t) const = 0;
    virtual int64_t getLong(uint32_t) const = 0;
    virtual uint64_t getULong(uint32_t) const = 0;
    virtual float getFloat(uint32_t) const = 0;
    virtual double getDouble(uint32_t) const = 0;

    void setTimeFormat(const char*, const char* =0);

protected:
    const ibis::column* col; ///!< The column where the value is from.
    ibis::column::unixTimeScribe *utform;

    colValues() : col(0), utform(0) {}
    colValues(const ibis::column* c) : col(c), utform(0) {};

private:
    colValues& operator=(const colValues&);
}; // ibis::colValues

/// A class to store integer values.
class FASTBIT_CXX_DLLSPEC ibis::colInts : public ibis::colValues {
public:
    colInts() : colValues(), array(0) {};
    colInts(const ibis::column* c, const ibis::bitvector& hits)
	: colValues(c), array(c->selectInts(hits)) {}
    colInts(const ibis::column* c, ibis::fileManager::storage* store,
	    const uint32_t start, const uint32_t nelm)
	: colValues(c), array(new array_t<int32_t>(store, start, nelm)) {}
    colInts(const ibis::column* c);
    virtual ~colInts() {delete array;}

    virtual bool   empty() const {return (col==0 || array==0);}
    virtual uint32_t size() const {return (array ? array->size() : 0);}
    virtual uint32_t elementSize() const {return sizeof(int32_t);}
    virtual ibis::TYPE_T getType() const {return ibis::INT;}
    virtual void* getArray() const {return array;}
    virtual void nosharing() {array->nosharing();}

    virtual void   reduce(const array_t<uint32_t>& starts);
    virtual void   reduce(const array_t<uint32_t>& starts,
			  ibis::selectClause::AGREGADO func);
    virtual void   erase(uint32_t i, uint32_t j) {
	array->erase(array->begin()+i, array->begin()+j);}
    virtual void   swap(uint32_t i, uint32_t j) {
	int32_t tmp = (*array)[i];
	(*array)[i] = (*array)[j];
	(*array)[j] = tmp;}

    void swap(colInts& rhs) { // swap two colInts
	const ibis::column* c = rhs.col; rhs.col = col; col = c;
	array_t<int32_t>* a = rhs.array; rhs.array = array; array = a;}

    virtual long write(FILE* fptr) const;
    virtual void write(std::ostream& out, uint32_t i) const;

    virtual void sort(uint32_t i, uint32_t j, bundle* bdl);
    virtual void sort(uint32_t i, uint32_t j, bundle* bdl,
		      colList::iterator head, colList::iterator tail);
    virtual void sort(uint32_t i, uint32_t j,
		      array_t<uint32_t>& neworder) const;
    virtual array_t<uint32_t>* segment(const array_t<uint32_t>* old=0) const;
    virtual void reorder(const array_t<uint32_t>& ind)
    {ibis::util::reorder(*array, ind);}
    virtual void topk(uint32_t k, array_t<uint32_t> &ind) const
    {array->topk(k, ind);}
    virtual void bottomk(uint32_t k, array_t<uint32_t> &ind) const
    {array->bottomk(k, ind);}
    virtual long truncate(uint32_t keep);
    virtual long truncate(uint32_t keep, uint32_t start);

    virtual double getMin() const;
    virtual double getMax() const;
    virtual double getSum() const;
    virtual int32_t getInt(uint32_t i) const {return (*array)[i];}
    virtual uint32_t getUInt(uint32_t i) const {return (uint32_t)(*array)[i];}
    virtual int64_t getLong(uint32_t i) const {return (int64_t)(*array)[i];}
    virtual uint64_t getULong(uint32_t i) const {return (uint64_t)(*array)[i];}
    virtual float getFloat(uint32_t i) const {return (float)(*array)[i];};
    virtual double getDouble(uint32_t i) const {return (double)(*array)[i];};

private:
    array_t<int32_t>* array;

    colInts(const colInts&);
    colInts& operator=(const colInts&);
}; // ibis::colInts

/// A class to store unsigned integer values.
class FASTBIT_CXX_DLLSPEC ibis::colUInts : public ibis::colValues {
public:
    colUInts() : colValues(), array(0), dic(0) {};
    colUInts(const ibis::column* c, const ibis::bitvector& hits);
    colUInts(const ibis::column* c, ibis::fileManager::storage* store,
	     const uint32_t start, const uint32_t nelm);
    colUInts(const ibis::column* c);
    virtual ~colUInts() {delete array;}

    virtual bool   empty() const {return (col==0 || array==0);}
    virtual uint32_t size() const {return (array ? array->size() : 0);}
    virtual uint32_t elementSize() const {return sizeof(uint32_t);}
    virtual ibis::TYPE_T getType() const {return ibis::UINT;}
    virtual void* getArray() const {return array;}
    virtual void nosharing() {array->nosharing();}

    virtual void   erase(uint32_t i, uint32_t j) {
	array->erase(array->begin()+i, array->begin()+j);}
    virtual void   swap(uint32_t i, uint32_t j) {
	uint32_t tmp = (*array)[i];
	(*array)[i] = (*array)[j];
	(*array)[j] = tmp;}

    virtual void   reduce(const array_t<uint32_t>& starts);
    virtual void   reduce(const array_t<uint32_t>& starts,
			  ibis::selectClause::AGREGADO func);
    void swap(colUInts& rhs) { // swap two colUInts
	const ibis::column* c = rhs.col; rhs.col = col; col = c;
	array_t<uint32_t>* a = rhs.array; rhs.array = array; array = a;}

    virtual long write(FILE* fptr) const;
    virtual void write(std::ostream& out, uint32_t i) const;

    virtual void sort(uint32_t i, uint32_t j, bundle* bdl);
    virtual void sort(uint32_t i, uint32_t j, bundle* bdl,
		      colList::iterator head, colList::iterator tail);
    virtual void sort(uint32_t i, uint32_t j,
		      array_t<uint32_t>& neworder) const;
    virtual array_t<uint32_t>* segment(const array_t<uint32_t>* old=0) const;
    virtual void reorder(const array_t<uint32_t>& ind)
    {ibis::util::reorder(*array, ind);}
    virtual void topk(uint32_t k, array_t<uint32_t> &ind) const
    {array->topk(k, ind);}
    virtual void bottomk(uint32_t k, array_t<uint32_t> &ind) const
    {array->bottomk(k, ind);}
    virtual long truncate(uint32_t keep);
    virtual long truncate(uint32_t keep, uint32_t start);

    virtual double getMin() const;
    virtual double getMax() const;
    virtual double getSum() const;
    virtual int32_t getInt(uint32_t i) const {return (int32_t)(*array)[i];}
    virtual uint32_t getUInt(uint32_t i) const {return (*array)[i];}
    virtual int64_t getLong(uint32_t i) const {return (int64_t)(*array)[i];}
    virtual uint64_t getULong(uint32_t i) const {return (uint64_t)(*array)[i];}
    virtual float getFloat(uint32_t i) const {return (float)(*array)[i];};
    virtual double getDouble(uint32_t i) const {return (double)(*array)[i];};

private:
    array_t<uint32_t>* array;
    const dictionary* dic;

    colUInts(const colUInts&);
    colUInts& operator=(const colUInts&);
}; // ibis::colUInts

/// A class to store signed 64-bit integer values.
class FASTBIT_CXX_DLLSPEC ibis::colLongs : public ibis::colValues {
public:
    colLongs() : colValues(), array(0) {};
    colLongs(const ibis::column* c, const ibis::bitvector& hits)
	: colValues(c), array(c->selectLongs(hits)) {}
    colLongs(const ibis::column* c, ibis::fileManager::storage* store,
	    const uint32_t start, const uint32_t nelm)
	: colValues(c), array(new array_t<int64_t>(store, start, nelm)) {}
    colLongs(const ibis::column* c);
    virtual ~colLongs() {delete array;}

    virtual bool   empty() const {return (col==0 || array==0);}
    virtual uint32_t size() const {return (array ? array->size() : 0);}
    virtual uint32_t elementSize() const {return sizeof(int64_t);}
    virtual ibis::TYPE_T getType() const {return ibis::LONG;}
    virtual void* getArray() const {return array;}
    virtual void nosharing() {array->nosharing();}

    virtual void   reduce(const array_t<uint32_t>& starts);
    virtual void   reduce(const array_t<uint32_t>& starts,
			  ibis::selectClause::AGREGADO func);
    virtual void   erase(uint32_t i, uint32_t j) {
	array->erase(array->begin()+i, array->begin()+j);}
    virtual void   swap(uint32_t i, uint32_t j) {
	int64_t tmp = (*array)[i];
	(*array)[i] = (*array)[j];
	(*array)[j] = tmp;}

    void swap(colLongs& rhs) { // swap two colLongs
	const ibis::column* c = rhs.col; rhs.col = col; col = c;
	array_t<int64_t>* a = rhs.array; rhs.array = array; array = a;}

    virtual long write(FILE* fptr) const;
    virtual void write(std::ostream& out, uint32_t i) const;

    virtual void sort(uint32_t i, uint32_t j, bundle* bdl);
    virtual void sort(uint32_t i, uint32_t j, bundle* bdl,
		      colList::iterator head, colList::iterator tail);
    virtual void sort(uint32_t i, uint32_t j,
		      array_t<uint32_t>& neworder) const;
    virtual array_t<uint32_t>* segment(const array_t<uint32_t>* old=0) const;
    virtual void reorder(const array_t<uint32_t>& ind)
    {ibis::util::reorder(*array, ind);}
    virtual void topk(uint32_t k, array_t<uint32_t> &ind) const
    {array->topk(k, ind);}
    virtual void bottomk(uint32_t k, array_t<uint32_t> &ind) const
    {array->bottomk(k, ind);}
    virtual long truncate(uint32_t keep);
    virtual long truncate(uint32_t keep, uint32_t start);

    virtual double getMin() const;
    virtual double getMax() const;
    virtual double getSum() const;
    virtual int32_t getInt(uint32_t i) const {return (int32_t)(*array)[i];}
    virtual uint32_t getUInt(uint32_t i) const {return (uint32_t)(*array)[i];}
    virtual int64_t getLong(uint32_t i) const {return (*array)[i];}
    virtual uint64_t getULong(uint32_t i) const {return (uint64_t)(*array)[i];}
    virtual float getFloat(uint32_t i) const {return (float)(*array)[i];};
    virtual double getDouble(uint32_t i) const {return (double)(*array)[i];};

private:
    array_t<int64_t>* array;

    colLongs(const colLongs&);
    colLongs& operator=(const colLongs&);
}; // ibis::colLongs

/// A class to store unsigned 64-bit integer values.
class FASTBIT_CXX_DLLSPEC ibis::colULongs : public ibis::colValues {
public:
    colULongs() : colValues(), array(0) {};
    colULongs(const ibis::column* c, const ibis::bitvector& hits)
	: colValues(c), array(c->selectULongs(hits)) {}
    colULongs(const ibis::column* c, ibis::fileManager::storage* store,
	    const uint32_t start, const uint32_t nelm)
	: colValues(c), array(new array_t<uint64_t>(store, start, nelm)) {}
    colULongs(const ibis::column* c);
    virtual ~colULongs() {delete array;}

    virtual bool   empty() const {return (col==0 || array==0);}
    virtual uint32_t size() const {return (array ? array->size() : 0);}
    virtual uint32_t elementSize() const {return sizeof(uint64_t);}
    virtual ibis::TYPE_T getType() const {return ibis::ULONG;}
    virtual void* getArray() const {return array;}
    virtual void nosharing() {array->nosharing();}

    virtual void   erase(uint32_t i, uint32_t j) {
	array->erase(array->begin()+i, array->begin()+j);}
    virtual void   swap(uint32_t i, uint32_t j) {
	uint64_t tmp = (*array)[i];
	(*array)[i] = (*array)[j];
	(*array)[j] = tmp;}

    virtual void   reduce(const array_t<uint32_t>& starts);
    virtual void   reduce(const array_t<uint32_t>& starts,
			  ibis::selectClause::AGREGADO func);
    void swap(colULongs& rhs) { // swap two colULongs
	const ibis::column* c = rhs.col; rhs.col = col; col = c;
	array_t<uint64_t>* a = rhs.array; rhs.array = array; array = a;}

    virtual long write(FILE* fptr) const;
    virtual void write(std::ostream& out, uint32_t i) const;

    virtual void sort(uint32_t i, uint32_t j, bundle* bdl);
    virtual void sort(uint32_t i, uint32_t j, bundle* bdl,
		      colList::iterator head, colList::iterator tail);
    virtual void sort(uint32_t i, uint32_t j,
		      array_t<uint32_t>& neworder) const;
    virtual array_t<uint32_t>* segment(const array_t<uint32_t>* old=0) const;
    virtual void reorder(const array_t<uint32_t>& ind)
    {ibis::util::reorder(*array, ind);}
    virtual void topk(uint32_t k, array_t<uint32_t> &ind) const
    {array->topk(k, ind);}
    virtual void bottomk(uint32_t k, array_t<uint32_t> &ind) const
    {array->bottomk(k, ind);}
    virtual long truncate(uint32_t keep);
    virtual long truncate(uint32_t keep, uint32_t start);

    virtual double getMin() const;
    virtual double getMax() const;
    virtual double getSum() const;
    virtual int32_t getInt(uint32_t i) const {return (int32_t)(*array)[i];}
    virtual uint32_t getUInt(uint32_t i) const {return (uint32_t)(*array)[i];}
    virtual int64_t getLong(uint32_t i) const {return (int64_t)(*array)[i];}
    virtual uint64_t getULong(uint32_t i) const {return (*array)[i];}
    virtual float getFloat(uint32_t i) const {return (float)(*array)[i];};
    virtual double getDouble(uint32_t i) const {return (double)(*array)[i];};

private:
    array_t<uint64_t>* array;

    colULongs(const colULongs&);
    colULongs& operator=(const colULongs&);
}; // ibis::colULongs

/// A class to store short integer values.
class FASTBIT_CXX_DLLSPEC ibis::colShorts : public ibis::colValues {
public:
    colShorts() : colValues(), array(0) {};
    colShorts(const ibis::column* c, const ibis::bitvector& hits)
	: colValues(c), array(c->selectShorts(hits)) {}
    colShorts(const ibis::column* c, ibis::fileManager::storage* store,
	      const uint32_t start, const uint32_t nelm)
	: colValues(c), array(new array_t<int16_t>(store, start, nelm)) {}
    colShorts(const ibis::column* c);
    virtual ~colShorts() {delete array;}

    virtual bool   empty() const {return (col==0 || array==0);}
    virtual uint32_t size() const {return (array ? array->size() : 0);}
    virtual uint32_t elementSize() const {return sizeof(int16_t);}
    virtual ibis::TYPE_T getType() const {return ibis::SHORT;}
    virtual void* getArray() const {return array;}
    virtual void nosharing() {array->nosharing();}

    virtual void   reduce(const array_t<uint32_t>& starts);
    virtual void   reduce(const array_t<uint32_t>& starts,
			  ibis::selectClause::AGREGADO func);
    virtual void   erase(uint32_t i, uint32_t j) {
	array->erase(array->begin()+i, array->begin()+j);}
    virtual void   swap(uint32_t i, uint32_t j) {
	int16_t tmp = (*array)[i];
	(*array)[i] = (*array)[j];
	(*array)[j] = tmp;}

    void swap(colShorts& rhs) { // swap two colShorts
	const ibis::column* c = rhs.col; rhs.col = col; col = c;
	array_t<int16_t>* a = rhs.array; rhs.array = array; array = a;}

    virtual long write(FILE* fptr) const;
    virtual void write(std::ostream& out, uint32_t i) const;

    virtual void sort(uint32_t i, uint32_t j, bundle* bdl);
    virtual void sort(uint32_t i, uint32_t j, bundle* bdl,
		      colList::iterator head, colList::iterator tail);
    virtual void sort(uint32_t i, uint32_t j,
		      array_t<uint32_t>& neworder) const;
    virtual array_t<uint32_t>* segment(const array_t<uint32_t>* old=0) const;
    virtual void reorder(const array_t<uint32_t>& ind)
    {ibis::util::reorder(*array, ind);}
    virtual void topk(uint32_t k, array_t<uint32_t> &ind) const
    {array->topk(k, ind);}
    virtual void bottomk(uint32_t k, array_t<uint32_t> &ind) const
    {array->bottomk(k, ind);}
    virtual long truncate(uint32_t keep);
    virtual long truncate(uint32_t keep, uint32_t start);

    virtual double getMin() const;
    virtual double getMax() const;
    virtual double getSum() const;
    virtual int32_t getInt(uint32_t i) const {return (int32_t)(*array)[i];}
    virtual uint32_t getUInt(uint32_t i) const {return (uint32_t)(*array)[i];}
    virtual int64_t getLong(uint32_t i) const {return (*array)[i];}
    virtual uint64_t getULong(uint32_t i) const {return (uint64_t)(*array)[i];}
    virtual float getFloat(uint32_t i) const {return (float)(*array)[i];};
    virtual double getDouble(uint32_t i) const {return (double)(*array)[i];};

private:
    array_t<int16_t>* array;

    colShorts(const colShorts&);
    colShorts& operator=(const colShorts&);
}; // ibis::colShorts

/// A class to store unsigned short integer values.
class FASTBIT_CXX_DLLSPEC ibis::colUShorts : public ibis::colValues {
public:
    colUShorts() : colValues(), array(0) {};
    colUShorts(const ibis::column* c, const ibis::bitvector& hits)
	: colValues(c), array(c->selectUShorts(hits)) {}
    colUShorts(const ibis::column* c, ibis::fileManager::storage* store,
	    const uint32_t start, const uint32_t nelm)
	: colValues(c), array(new array_t<uint16_t>(store, start, nelm)) {}
    colUShorts(const ibis::column* c);
    virtual ~colUShorts() {delete array;}

    virtual bool   empty() const {return (col==0 || array==0);}
    virtual uint32_t size() const {return (array ? array->size() : 0);}
    virtual uint32_t elementSize() const {return sizeof(uint16_t);}
    virtual ibis::TYPE_T getType() const {return ibis::USHORT;}
    virtual void* getArray() const {return array;}
    virtual void nosharing() {array->nosharing();}

    virtual void   erase(uint32_t i, uint32_t j) {
	array->erase(array->begin()+i, array->begin()+j);}
    virtual void   swap(uint32_t i, uint32_t j) {
	uint16_t tmp = (*array)[i];
	(*array)[i] = (*array)[j];
	(*array)[j] = tmp;}

    virtual void   reduce(const array_t<uint32_t>& starts);
    virtual void   reduce(const array_t<uint32_t>& starts,
			  ibis::selectClause::AGREGADO func);
    void swap(colUShorts& rhs) { // swap two colUShorts
	const ibis::column* c = rhs.col; rhs.col = col; col = c;
	array_t<uint16_t>* a = rhs.array; rhs.array = array; array = a;}

    virtual long write(FILE* fptr) const;
    virtual void write(std::ostream& out, uint32_t i) const;

    virtual void sort(uint32_t i, uint32_t j, bundle* bdl);
    virtual void sort(uint32_t i, uint32_t j, bundle* bdl,
		      colList::iterator head, colList::iterator tail);
    virtual void sort(uint32_t i, uint32_t j,
		      array_t<uint32_t>& neworder) const;
    virtual array_t<uint32_t>* segment(const array_t<uint32_t>* old=0) const;
    virtual void reorder(const array_t<uint32_t>& ind)
    {ibis::util::reorder(*array, ind);}
    virtual void topk(uint32_t k, array_t<uint32_t> &ind) const
    {array->topk(k, ind);}
    virtual void bottomk(uint32_t k, array_t<uint32_t> &ind) const
    {array->bottomk(k, ind);}
    virtual long truncate(uint32_t keep);
    virtual long truncate(uint32_t keep, uint32_t start);

    virtual double getMin() const;
    virtual double getMax() const;
    virtual double getSum() const;
    virtual int32_t getInt(uint32_t i) const {return (int32_t)(*array)[i];}
    virtual uint32_t getUInt(uint32_t i) const {return (uint32_t)(*array)[i];}
    virtual int64_t getLong(uint32_t i) const {return (int64_t)(*array)[i];}
    virtual uint64_t getULong(uint32_t i) const {return (*array)[i];}
    virtual float getFloat(uint32_t i) const {return (float)(*array)[i];};
    virtual double getDouble(uint32_t i) const {return (double)(*array)[i];};

private:
    array_t<uint16_t>* array;

    colUShorts(const colUShorts&);
    colUShorts& operator=(const colUShorts&);
}; // ibis::colUShorts

/// A class to store signed 8-bit integer values.
class FASTBIT_CXX_DLLSPEC ibis::colBytes : public ibis::colValues {
public:
    colBytes() : colValues(), array(0) {};
    colBytes(const ibis::column* c, const ibis::bitvector& hits)
	: colValues(c), array(c->selectBytes(hits)) {}
    colBytes(const ibis::column* c, ibis::fileManager::storage* store,
	    const uint32_t start, const uint32_t nelm)
	: colValues(c), array(new array_t<signed char>(store, start, nelm)) {}
    colBytes(const ibis::column* c);
    virtual ~colBytes() {delete array;}

    virtual bool   empty() const {return (col==0 || array==0);}
    virtual uint32_t size() const {return (array ? array->size() : 0);}
    virtual uint32_t elementSize() const {return sizeof(char);}
    virtual ibis::TYPE_T getType() const {return ibis::BYTE;}
    virtual void* getArray() const {return array;}
    virtual void nosharing() {array->nosharing();}

    virtual void   reduce(const array_t<uint32_t>& starts);
    virtual void   reduce(const array_t<uint32_t>& starts,
			  ibis::selectClause::AGREGADO func);
    virtual void   erase(uint32_t i, uint32_t j) {
	array->erase(array->begin()+i, array->begin()+j);}
    virtual void   swap(uint32_t i, uint32_t j) {
	signed char tmp = (*array)[i];
	(*array)[i] = (*array)[j];
	(*array)[j] = tmp;}

    void swap(colBytes& rhs) { // swap two colBytes
	const ibis::column* c = rhs.col; rhs.col = col; col = c;
	array_t<signed char>* a = rhs.array; rhs.array = array; array = a;}

    virtual long write(FILE* fptr) const;
    virtual void write(std::ostream& out, uint32_t i) const;

    virtual void sort(uint32_t i, uint32_t j, bundle* bdl);
    virtual void sort(uint32_t i, uint32_t j, bundle* bdl,
		      colList::iterator head, colList::iterator tail);
    virtual void sort(uint32_t i, uint32_t j,
		      array_t<uint32_t>& neworder) const;
    virtual array_t<uint32_t>* segment(const array_t<uint32_t>* old=0) const;
    virtual void reorder(const array_t<uint32_t>& ind)
    {ibis::util::reorder(*array, ind);}
    virtual void topk(uint32_t k, array_t<uint32_t> &ind) const
    {array->topk(k, ind);}
    virtual void bottomk(uint32_t k, array_t<uint32_t> &ind) const
    {array->bottomk(k, ind);}
    virtual long truncate(uint32_t keep);
    virtual long truncate(uint32_t keep, uint32_t start);

    virtual double getMin() const;
    virtual double getMax() const;
    virtual double getSum() const;
    virtual int32_t getInt(uint32_t i) const {return (int32_t)(*array)[i];}
    virtual uint32_t getUInt(uint32_t i) const {return (uint32_t)(*array)[i];}
    virtual int64_t getLong(uint32_t i) const {return (*array)[i];}
    virtual uint64_t getULong(uint32_t i) const {return (uint64_t)(*array)[i];}
    virtual float getFloat(uint32_t i) const {return (float)(*array)[i];};
    virtual double getDouble(uint32_t i) const {return (double)(*array)[i];};

private:
    array_t<signed char>* array;

    colBytes(const colBytes&);
    colBytes& operator=(const colBytes&);
}; // ibis::colBytes

/// A class to store unsigned 64-bit integer values.
class FASTBIT_CXX_DLLSPEC ibis::colUBytes : public ibis::colValues {
public:
    colUBytes() : colValues(), array(0) {};
    colUBytes(const ibis::column* c, const ibis::bitvector& hits)
	: colValues(c), array(c->selectUBytes(hits)) {}
    colUBytes(const ibis::column* c, ibis::fileManager::storage* store,
	    const uint32_t start, const uint32_t nelm)
	: colValues(c), array(new array_t<unsigned char>(store, start, nelm)) {}
    colUBytes(const ibis::column* c);
    virtual ~colUBytes() {delete array;}

    virtual bool   empty() const {return (col==0 || array==0);}
    virtual uint32_t size() const {return (array ? array->size() : 0);}
    virtual uint32_t elementSize() const {return sizeof(char);}
    virtual ibis::TYPE_T getType() const {return ibis::UBYTE;}
    virtual void* getArray() const {return array;}
    virtual void nosharing() {array->nosharing();}

    virtual void   erase(uint32_t i, uint32_t j) {
	array->erase(array->begin()+i, array->begin()+j);}
    virtual void   swap(uint32_t i, uint32_t j) {
	unsigned char tmp = (*array)[i];
	(*array)[i] = (*array)[j];
	(*array)[j] = tmp;}

    virtual void   reduce(const array_t<uint32_t>& starts);
    virtual void   reduce(const array_t<uint32_t>& starts,
			  ibis::selectClause::AGREGADO func);
    void swap(colUBytes& rhs) { // swap two colUBytes
	const ibis::column* c = rhs.col; rhs.col = col; col = c;
	array_t<unsigned char>* a = rhs.array; rhs.array = array; array = a;}

    virtual long write(FILE* fptr) const;
    virtual void write(std::ostream& out, uint32_t i) const;

    virtual void sort(uint32_t i, uint32_t j, bundle* bdl);
    virtual void sort(uint32_t i, uint32_t j, bundle* bdl,
		      colList::iterator head, colList::iterator tail);
    virtual void sort(uint32_t i, uint32_t j,
		      array_t<uint32_t>& neworder) const;
    virtual array_t<uint32_t>* segment(const array_t<uint32_t>* old=0) const;
    virtual void reorder(const array_t<uint32_t>& ind)
    {ibis::util::reorder(*array, ind);}
    virtual void topk(uint32_t k, array_t<uint32_t> &ind) const
    {array->topk(k, ind);}
    virtual void bottomk(uint32_t k, array_t<uint32_t> &ind) const
    {array->bottomk(k, ind);}
    virtual long truncate(uint32_t keep);
    virtual long truncate(uint32_t keep, uint32_t start);

    virtual double getMin() const;
    virtual double getMax() const;
    virtual double getSum() const;
    virtual int32_t getInt(uint32_t i) const {return (int32_t)(*array)[i];}
    virtual uint32_t getUInt(uint32_t i) const {return (uint32_t)(*array)[i];}
    virtual int64_t getLong(uint32_t i) const {return (int64_t)(*array)[i];}
    virtual uint64_t getULong(uint32_t i) const {return (*array)[i];}
    virtual float getFloat(uint32_t i) const {return (float)(*array)[i];};
    virtual double getDouble(uint32_t i) const {return (double)(*array)[i];};

private:
    array_t<unsigned char>* array;

    colUBytes(const colUBytes&);
    colUBytes& operator=(const colUBytes&);
}; // ibis::colUBytes

/// A class to store single precision float-point values.
class FASTBIT_CXX_DLLSPEC ibis::colFloats : public ibis::colValues {
public:
    colFloats() : colValues(), array(0) {};
    colFloats(const ibis::column* c, const ibis::bitvector& hits)
	: colValues(c), array(c->selectFloats(hits)) {}
    colFloats(const ibis::column* c, ibis::fileManager::storage* store,
	      const uint32_t start, const uint32_t nelm)
	: colValues(c), array(new array_t<float>(store, start, nelm)) {}
    colFloats(const ibis::column* c);
    virtual ~colFloats() {delete array;}

    virtual bool   empty() const {return (col==0 || array==0);}
    virtual uint32_t size() const {return (array != (void*)NULL ? array->size() : 0);}
    virtual uint32_t elementSize() const {return sizeof(float);}
    virtual ibis::TYPE_T getType() const {return ibis::FLOAT;}
    virtual void* getArray() const {return array;}
    virtual void nosharing() {array->nosharing();}

    virtual void   erase(uint32_t i, uint32_t j) {
	array->erase(array->begin()+i, array->begin()+j);}
    virtual void   swap(uint32_t i, uint32_t j) {
	float tmp = (*array)[i];
	(*array)[i] = (*array)[j];
	(*array)[j] = tmp;}

    void swap(colFloats& rhs) { // swap two colFloats
	const ibis::column* c = rhs.col; rhs.col = col; col = c;
	array_t<float>* a = rhs.array; rhs.array = array; array = a;}
    virtual void   reduce(const array_t<uint32_t>& starts);
    virtual void   reduce(const array_t<uint32_t>& starts,
			  ibis::selectClause::AGREGADO func);

    virtual long write(FILE* fptr) const;
    virtual void write(std::ostream& out, uint32_t i) const;

    virtual void sort(uint32_t i, uint32_t j, bundle* bdl);
    virtual void sort(uint32_t i, uint32_t j, bundle* bdl,
		      colList::iterator head, colList::iterator tail);
    virtual void sort(uint32_t i, uint32_t j,
		      array_t<uint32_t>& neworder) const;
    virtual array_t<uint32_t>* segment(const array_t<uint32_t>* old=0) const;
    virtual void reorder(const array_t<uint32_t>& ind)
    {ibis::util::reorder(*array, ind);}
    virtual void topk(uint32_t k, array_t<uint32_t> &ind) const
    {array->topk(k, ind);}
    virtual void bottomk(uint32_t k, array_t<uint32_t> &ind) const
    {array->bottomk(k, ind);}
    virtual long truncate(uint32_t keep);
    virtual long truncate(uint32_t keep, uint32_t start);

    virtual double getMin() const;
    virtual double getMax() const;
    virtual double getSum() const;
    virtual int32_t getInt(uint32_t i) const {return (int32_t)(*array)[i];}
    virtual uint32_t getUInt(uint32_t i) const {return (uint32_t)(*array)[i];}
    virtual int64_t getLong(uint32_t i) const {return (int64_t)(*array)[i];}
    virtual uint64_t getULong(uint32_t i) const {return (uint64_t)(*array)[i];}
    virtual float getFloat(uint32_t i) const {return (float)(*array)[i];};
    virtual double getDouble(uint32_t i) const {return (double)(*array)[i];};

private:
    array_t<float>* array;

    colFloats(const colFloats&);
    colFloats& operator=(const colFloats&);
}; // ibis::colFloats

/// A class to store double precision floating-point values.
class FASTBIT_CXX_DLLSPEC ibis::colDoubles : public ibis::colValues {
public:
    colDoubles() : colValues(), array(0) {};
    colDoubles(const ibis::column* c, const ibis::bitvector& hits)
	: colValues(c), array(c->selectDoubles(hits)) {}
    colDoubles(const ibis::column* c, ibis::fileManager::storage* store,
	       const uint32_t start, const uint32_t end)
	: colValues(c), array(new array_t<double>(store, start, end)) {}
    colDoubles(const ibis::column* c);
    colDoubles(size_t n, double v) : array(new array_t<double>(n, v)) {}
    virtual ~colDoubles() {delete array;}

    virtual bool   empty() const {return (col==0 || array==0);}
    virtual uint32_t size() const {return (array ? array->size() : 0);}
    virtual uint32_t elementSize() const {return sizeof(double);}
    virtual ibis::TYPE_T getType() const {return ibis::DOUBLE;}
    virtual void* getArray() const {return array;}
    virtual void nosharing() {array->nosharing();}

    virtual void   erase(uint32_t i, uint32_t j) {
	array->erase(array->begin()+i, array->begin()+j);}
    virtual void   swap(uint32_t i, uint32_t j) {
	double tmp = (*array)[i];
	(*array)[i] = (*array)[j];
	(*array)[j] = tmp;}

    void swap(colDoubles& rhs) { // swap two colDoubles
	const ibis::column* c = rhs.col; rhs.col = col; col = c;
	array_t<double>* a = rhs.array; rhs.array = array; array = a;}
    virtual void   reduce(const array_t<uint32_t>& starts);
    virtual void   reduce(const array_t<uint32_t>& starts,
			  ibis::selectClause::AGREGADO func);

    virtual long write(FILE* fptr) const;
    virtual void write(std::ostream& out, uint32_t i) const;

    virtual void sort(uint32_t i, uint32_t j, bundle* bdl);
    virtual void sort(uint32_t i, uint32_t j, bundle* bdl,
		      colList::iterator head, colList::iterator tail);
    virtual void sort(uint32_t i, uint32_t j,
		      array_t<uint32_t>& neworder) const;
    virtual array_t<uint32_t>* segment(const array_t<uint32_t>* old=0) const;
    virtual void reorder(const array_t<uint32_t>& ind)
    {ibis::util::reorder(*array, ind);}
    virtual void topk(uint32_t k, array_t<uint32_t> &ind) const
    {array->topk(k, ind);}
    virtual void bottomk(uint32_t k, array_t<uint32_t> &ind) const
    {array->bottomk(k, ind);}
    virtual long truncate(uint32_t keep);
    virtual long truncate(uint32_t keep, uint32_t start);

    virtual double getMin() const;
    virtual double getMax() const;
    virtual double getSum() const;
    virtual int32_t getInt(uint32_t i) const {return (int32_t)(*array)[i];}
    virtual uint32_t getUInt(uint32_t i) const {return (uint32_t)(*array)[i];}
    virtual int64_t getLong(uint32_t i) const {return (int64_t)(*array)[i];}
    virtual uint64_t getULong(uint32_t i) const {return (uint64_t)(*array)[i];}
    virtual float getFloat(uint32_t i) const {return (float)(*array)[i];};
    virtual double getDouble(uint32_t i) const {return (double)(*array)[i];};

private:
    array_t<double>* array;

    colDoubles(const colDoubles&);
    colDoubles& operator=(const colDoubles&);
}; // ibis::colDoubles

/// A class to store string values.
class FASTBIT_CXX_DLLSPEC ibis::colStrings : public ibis::colValues {
public:
    colStrings() : colValues(), array(0) {};
    colStrings(const ibis::column* c, const ibis::bitvector& hits)
	: colValues(c), array(c->selectStrings(hits)) {}
    colStrings(const ibis::column* c);
    colStrings(size_t n, const std::string& v)
	: array(new std::vector<std::string>(n, v)) {}
    virtual ~colStrings() {delete array;}

    virtual bool         empty() const {return (col==0 || array==0);}
    virtual uint32_t     size() const {return (array ? array->size() : 0);}
    virtual uint32_t     elementSize() const {return 0;}
    virtual ibis::TYPE_T getType() const {
	return (col->type()==ibis::CATEGORY?ibis::CATEGORY:ibis::TEXT);}
    virtual void*        getArray() const {return array;}
    virtual void         nosharing() {/* neve shared */}

    virtual void erase(uint32_t i, uint32_t j) {
	array->erase(array->begin()+i, array->begin()+j);}
    virtual void swap(uint32_t i, uint32_t j) {(*array)[i].swap((*array)[j]);}

    void swap(colStrings& rhs) { // swap two colStrings
	const ibis::column* c = rhs.col; rhs.col = col; col = c;
	std::vector<std::string>* a = rhs.array; rhs.array = array; array = a;}
    virtual void reduce(const array_t<uint32_t>& starts);
    virtual void reduce(const array_t<uint32_t>& starts,
			ibis::selectClause::AGREGADO func);

    virtual long write(FILE* fptr) const;
    virtual void write(std::ostream& out, uint32_t i) const;

    virtual void sort(uint32_t i, uint32_t j, bundle* bdl);
    virtual void sort(uint32_t i, uint32_t j, bundle* bdl,
		      colList::iterator head, colList::iterator tail);
    virtual void sort(uint32_t i, uint32_t j,
		      array_t<uint32_t>& neworder) const;
    virtual array_t<uint32_t>* segment(const array_t<uint32_t>* old=0) const;
    virtual void reorder(const array_t<uint32_t>& ind);
    virtual void topk(uint32_t k, array_t<uint32_t> &ind) const;
    virtual void bottomk(uint32_t k, array_t<uint32_t> &ind) const;
    virtual long truncate(uint32_t keep);
    virtual long truncate(uint32_t keep, uint32_t start);

    /// Compute the minimum.  NOT implemented.
    virtual double getMin() const {return FASTBIT_DOUBLE_NULL;}
    /// Compute the maximum.  NOT implemented.
    virtual double getMax() const {return FASTBIT_DOUBLE_NULL;}
    /// Compute the sum.  NOT implemented.
    virtual double getSum() const {return FASTBIT_DOUBLE_NULL;}
    /// Return the ith value as int.  NOT implemented.
    virtual int32_t getInt(uint32_t) const {return 0;}
    /// Return the ith value as unsigned int.  NOT implemented.
    virtual uint32_t getUInt(uint32_t) const {return 0;}
    /// Return the ith value as long.  NOT implemented.
    virtual int64_t getLong(uint32_t) const {return 0;}
    /// Return the ith value as unsigned long.  NOT implemented.
    virtual uint64_t getULong(uint32_t) const {return 0;}
    /// Return the ith value as float.  NOT implemented.
    virtual float getFloat(uint32_t) const {return  FASTBIT_FLOAT_NULL;}
    /// Return the ith value as double.  NOT implemented.
    virtual double getDouble(uint32_t) const {return FASTBIT_DOUBLE_NULL;}

private:
    /// String values are internally stored as a vector of std::string.
    std::vector<std::string>* array;

    colStrings(const colStrings&);
    colStrings& operator=(const colStrings&);

    void sortsub(uint32_t i, uint32_t j, array_t<uint32_t>& ind) const;
    uint32_t partitionsub(uint32_t, uint32_t, array_t<uint32_t>&) const;
}; // ibis::colStrings

/// A class to store raw binary values.  It does not support sorting or any
/// arithematic operations.
class FASTBIT_CXX_DLLSPEC ibis::colBlobs : public ibis::colValues {
public:
    colBlobs() : colValues(), array(0) {};
    colBlobs(const ibis::column* c, const ibis::bitvector& hits)
	: colValues(c), array(c->selectOpaques(hits)) {}
    colBlobs(const ibis::column* c);
    colBlobs(size_t n, const ibis::opaque& v)
	: array(new std::vector<ibis::opaque>(n, v)) {}
    virtual ~colBlobs() {delete array;}

    virtual bool         empty() const {return (col==0 || array==0);}
    virtual uint32_t     size() const {return (array ? array->size() : 0);}
    virtual uint32_t     elementSize() const {return 0;}
    virtual ibis::TYPE_T getType() const {return col->type();}
    virtual void*        getArray() const {return array;}
    virtual void         nosharing() {/* neve shared */}

    virtual void erase(uint32_t i, uint32_t j) {
	array->erase(array->begin()+i, array->begin()+j);}
    virtual void swap(uint32_t i, uint32_t j) {(*array)[i].swap((*array)[j]);}

    void swap(colBlobs& rhs) { // swap two colBlobs
	const ibis::column* c = rhs.col; rhs.col = col; col = c;
	std::vector<ibis::opaque>* a = rhs.array; rhs.array = array; array = a;}
    virtual void reduce(const array_t<uint32_t>&);
    virtual void reduce(const array_t<uint32_t>&,
			ibis::selectClause::AGREGADO);

    virtual long write(FILE* fptr) const;
    virtual void write(std::ostream& out, uint32_t i) const;

    virtual void sort(uint32_t, uint32_t, bundle*);
    virtual void sort(uint32_t, uint32_t, bundle*,
		      colList::iterator, colList::iterator);
    virtual void sort(uint32_t, uint32_t,
		      array_t<uint32_t>&) const;
    virtual array_t<uint32_t>* segment(const array_t<uint32_t>*) const;
    virtual void reorder(const array_t<uint32_t>& ind);
    virtual void topk(uint32_t k, array_t<uint32_t> &ind) const;
    virtual void bottomk(uint32_t k, array_t<uint32_t> &ind) const;
    virtual long truncate(uint32_t keep);
    virtual long truncate(uint32_t keep, uint32_t start);

    /// Compute the minimum.  NOT implemented.
    virtual double getMin() const {return FASTBIT_DOUBLE_NULL;}
    /// Compute the maximum.  NOT implemented.
    virtual double getMax() const {return FASTBIT_DOUBLE_NULL;}
    /// Compute the sum.  NOT implemented.
    virtual double getSum() const {return FASTBIT_DOUBLE_NULL;}
    /// Return the ith value as int.  NOT implemented.
    virtual int32_t getInt(uint32_t) const {return 0;}
    /// Return the ith value as unsigned int.  NOT implemented.
    virtual uint32_t getUInt(uint32_t) const {return 0;}
    /// Return the ith value as long.  NOT implemented.
    virtual int64_t getLong(uint32_t) const {return 0;}
    /// Return the ith value as unsigned long.  NOT implemented.
    virtual uint64_t getULong(uint32_t) const {return 0;}
    /// Return the ith value as float.  NOT implemented.
    virtual float getFloat(uint32_t) const {return  FASTBIT_FLOAT_NULL;}
    /// Return the ith value as double.  NOT implemented.
    virtual double getDouble(uint32_t) const {return FASTBIT_DOUBLE_NULL;}

private:
    /// Blob values are internally stored as a vector of ibis::opaque.
    std::vector<ibis::opaque>* array;

    colBlobs(const colBlobs&);
    colBlobs& operator=(const colBlobs&);
}; // ibis::colBlobs
#endif
