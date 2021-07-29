//File: $Id$
// Author: John Wu <John.Wu at ACM.org>
// Copyright (c) 2009-2016 the Regents of the University of California
///@file
/// Define the class ibis::blob.
#ifndef IBIS_BLOB_H
#define IBIS_BLOB_H

#include "table.h"	// ibis::TYPE_T
#include "bitvector.h"
#include "column.h"	// ibis::column

/// A class to provide a minimal support for byte arrays.  Since a byte
/// array may contain any arbitrary byte values, we can not rely on the
/// null terminator any more, nor use std::string as the container for each
/// array.  It is intended to store opaque data, and can not be searched.
class ibis::blob : public ibis::column {
public:
    virtual ~blob() {};
    blob(const part*, FILE*);
    blob(const part*, const char*);
    blob(const ibis::column&);

    virtual long stringSearch(const char*, ibis::bitvector&) const {return -1;}
    virtual long stringSearch(const std::vector<std::string>&,
			      ibis::bitvector&) const {return -1;}
    virtual long stringSearch(const char*) const {return -1;}
    virtual long stringSearch(const std::vector<std::string>&) const {
	return -1;}

    virtual void computeMinMax() {}
    virtual void computeMinMax(const char*) {}
    virtual void computeMinMax(const char*, double&, double&, bool&) const {}
    virtual void loadIndex(const char*, int) const throw () {}
    virtual long indexSize() const {return -1;}
    virtual int  getValuesArray(void*) const {return -1;}

    virtual array_t<signed char>*
    selectBytes(const bitvector&) const {return 0;}
    virtual array_t<unsigned char>*
    selectUBytes(const bitvector&) const {return 0;}
    virtual array_t<int16_t>*
    selectShorts(const bitvector&) const {return 0;}
    virtual array_t<uint16_t>*
    selectUShorts(const bitvector&) const {return 0;}
    virtual array_t<int32_t>*
    selectInts(const bitvector&) const {return 0;}
    virtual array_t<uint32_t>*
    selectUInts(const bitvector&) const {return 0;}
    virtual array_t<int64_t>*
    selectLongs(const bitvector&) const {return 0;}
    virtual array_t<uint64_t>*
    selectULongs(const bitvector&) const {return 0;}
    virtual array_t<float>*
    selectFloats(const bitvector&) const {return 0;}
    virtual array_t<double>*
    selectDoubles(const bitvector&) const {return 0;}
    virtual std::vector<std::string>*
    selectStrings(const bitvector&) const {return 0;}
    virtual std::vector<ibis::opaque>*
	selectOpaques(const bitvector& mask) const;
    virtual int getOpaque(uint32_t, ibis::opaque&) const;

    virtual double getActualMin() const {return DBL_MAX;}
    virtual double getActualMax() const {return -DBL_MAX;}
    virtual double getSum() const {return 0;}

    virtual long append(const void*, const ibis::bitvector&) {return -1;}
    virtual long append(const char* dt, const char* df, const uint32_t nold,
			const uint32_t nnew, uint32_t nbuf, char* buf);
    virtual long writeData(const char* dir, uint32_t nold, uint32_t nnew,
			   ibis::bitvector& mask, const void *va1,
			   void *va2);

    virtual void write(FILE*) const;
    virtual void print(std::ostream&) const;

    long countRawBytes(const bitvector&) const;
    int selectRawBytes(const bitvector&,
		       array_t<char>&, array_t<uint64_t>&) const;
    int getBlob(uint32_t ind, char *&buf, uint64_t &size) const;

protected:
    int extractAll(const bitvector&,
		   array_t<char>&, array_t<uint64_t>&,
		   const array_t<char>&,
		   const array_t<int64_t>&) const;
    int extractSome(const bitvector&,
		    array_t<char>&, array_t<uint64_t>&,
		    const array_t<char>&, const array_t<int64_t>&,
		    const uint32_t) const;
    int extractAll(const bitvector&,
		   array_t<char>&, array_t<uint64_t>&,
		   const char*, const array_t<int64_t>&) const;
    int extractSome(const bitvector&,
		    array_t<char>&, array_t<uint64_t>&,
		    const char*, const array_t<int64_t>&, const uint32_t) const;
    int extractSome(const bitvector&,
		    array_t<char>&, array_t<uint64_t>&,
		    const char*, const char*, const uint32_t) const;
    int readBlob(uint32_t ind, char *&buf, uint64_t &size,
		 const array_t<int64_t> &starts, const char *datafile) const;
    int readBlob(uint32_t ind, char *&buf, uint64_t &size,
		 const char *spfile, const char *datafile) const;
}; // ibis::blob

std::ostream& operator<<(std::ostream& out, const ibis::opaque& opq);
#endif
