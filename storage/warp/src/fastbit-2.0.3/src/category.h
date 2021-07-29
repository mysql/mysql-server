//File: $Id$
// Author: John Wu <John.Wu at ACM.org>
// Copyright (c) 2000-2016 the Regents of the University of California
#ifndef IBIS_CATEGORY_H
#define IBIS_CATEGORY_H
///@file
/// Define three specialization of the column class.
///
/// IBIS represents incoming data table with vertical partitioning.  Each
/// column object represents one column of the relational table.  The terms
/// used to describe each column of the table are strongly influenced by
/// the first project using this software, a high-energy physics experiment
/// named STAR.
///
#include "column.h"	// ibis::column
#include "dictionary.h" // ibis::dictionary
#include "idirekte.h"	// ibis::direkte

/// A data structure for storing null-terminated text.  This is meant for
/// string values that are relatively long and may have an internal
/// structure.  The most useful search operation supported on this type of
/// data is the keyword search, also known as full-text search.  The
/// keyword search operation is implemented through a boolean term-document
/// matrix (implemented as ibis::keywords).
///
/// @sa ibis::keywords
class ibis::text : public ibis::column {
public:
    virtual ~text() {unloadIndex();};
    text(const part* tbl, FILE* file);
    text(const part* tbl, const char* name, ibis::TYPE_T t=ibis::TEXT);
    text(const ibis::column& col); // copy from column

    virtual long keywordSearch(const char* str, ibis::bitvector& hits) const;
    virtual long keywordSearch(const std::vector<std::string>& strs,
			       ibis::bitvector& hits) const;
    virtual long keywordSearch(const char*) const;
    virtual long keywordSearch(const std::vector<std::string>&) const;

    virtual long stringSearch(const char* str, ibis::bitvector& hits) const;
    virtual long stringSearch(const std::vector<std::string>& strs,
			      ibis::bitvector& hits) const;
    virtual long stringSearch(const char* str) const;
    virtual long stringSearch(const std::vector<std::string>& strs) const;

    virtual long patternSearch(const char*, ibis::bitvector&) const;
    virtual long patternSearch(const char*) const;

    using ibis::column::estimateCost;
    virtual double estimateCost(const ibis::qString& cmp) const;
    virtual double estimateCost(const ibis::qAnyString& cmp) const;

    virtual void loadIndex(const char* iopt=0, int ropt=0) const throw ();
    virtual long append(const char* dt, const char* df, const uint32_t nold,
			const uint32_t nnew, uint32_t nbuf, char* buf);
    virtual long append(const void*, const ibis::bitvector&) {return -1;}
    virtual long saveSelected(const ibis::bitvector& sel, const char *dest,
			      char *buf, uint32_t nbuf);
    /// Return the positions of records marked 1 in the mask.
    virtual array_t<uint32_t>* selectUInts(const bitvector& mask) const;
    /// Return the starting positions of strings marked 1 in the mask.
    virtual array_t<int64_t>* selectLongs(const bitvector& mask) const;
    virtual std::vector<std::string>*
    selectStrings(const bitvector& mask) const;
    virtual const char* findString(const char* str) const;
    virtual int getString(uint32_t i, std::string &val) const {
	return readString(i, val);}
    virtual int getOpaque(uint32_t, ibis::opaque&) const;
    // virtual std::vector<ibis::opaque>*
    // selectOpaques(const bitvector& mask) const;

    virtual void write(FILE* file) const; ///!< Write the metadata entry.
    virtual void print(std::ostream& out) const; ///!< Print header info.

    const column* IDColumnForKeywordIndex() const;
    void TDListForKeywordIndex(std::string&) const;
    void delimitersForKeywordIndex(std::string&) const;

    /// A tokenizer class to turn a string buffer into tokens.  Used by
    /// ibis::keywords to build a term-document index.
    struct tokenizer {
	/// A tokenizer must implement a two-argument operator().  It takes
	/// an input string in buf to produce a list of tokens in tkns.
	/// The input buffer may be modified in this function.  The return
	/// value shall be zero (0) to indicate success, a positive value
	/// to carray a warning message, and a negative value to indicate
	/// fatal error.
	///
	/// @note This function is not declared as const because a derived
	/// class might want to keep some statistics or otherwise alter its
	/// state while processing an incoming text buffer.
	virtual int operator()(std::vector<const char*>& tkns, char *buf) = 0;
	/// Destructor.
	virtual ~tokenizer() {}
    }; // struct tokenizer

protected:

    void startPositions(const char *dir, char *buf, uint32_t nbuf) const;
    int  readString(uint32_t i, std::string &val) const;
    int  readString(std::string&, int, long, long, char*, uint32_t,
		    uint32_t&, off_t&) const;
    int  readStrings1(const ibis::bitvector&, std::vector<std::string>&) const;
    int  readStrings2(const ibis::bitvector&, std::vector<std::string>&) const;
    int  writeStrings(const char *to, const char *from,
		      const char *spto, const char *spfrom,
		      ibis::bitvector &msk, const ibis::bitvector &sel,
		      char *buf, uint32_t nbuf) const;

private:
    text& operator=(const text&);
}; // ibis::text

/// A specialized low-cardinality text field.  It is also known as control
/// values or categorical values.  This implementation directly converts
/// string values into bitvectors (as ibis::direkte), and does not store
/// integer version of the string.
///
/// @note The integer zero (0) is reserved for NULL values.
class ibis::category : public ibis::text {
public:
    virtual ~category();
    category(const part* tbl, FILE* file);
    category(const part* tbl, const char* name);
    category(const ibis::column& col); // copy from column
    // a special construct for meta-tag attributes
    category(const part* tbl, const char* name, const char* value,
	     const char* dir=0, uint32_t nevt=0);

    virtual long keywordSearch(const char* str, ibis::bitvector& hits) const {
	return stringSearch(str, hits);}
    virtual long keywordSearch(const std::vector<std::string>& vals,
			       ibis::bitvector& hits) const {
	return stringSearch(vals, hits);}
    virtual long keywordSearch(const char* str) const {
	return stringSearch(str);}
    virtual long keywordSearch(const std::vector<std::string>& vals) const {
	return stringSearch(vals);}

    virtual long stringSearch(const char* str, ibis::bitvector& hits) const;
    virtual long stringSearch(const std::vector<std::string>& vals,
			      ibis::bitvector& hits) const;
    virtual long stringSearch(const char* str) const;
    virtual long stringSearch(const std::vector<std::string>& vals) const;

    virtual long patternSearch(const char* pat) const;
    virtual long patternSearch(const char* pat, ibis::bitvector &hits) const;

    using ibis::text::estimateCost;
    virtual double estimateCost(const ibis::qLike& cmp) const;
    virtual double estimateCost(const ibis::qString& cmp) const;
    virtual double estimateCost(const ibis::qAnyString& cmp) const;

    virtual void loadIndex(const char* =0, int =0) const throw ();
    /// Append the content in @a df to the directory @a dt.
    virtual long append(const char* dt, const char* df, const uint32_t nold,
			const uint32_t nnew, uint32_t nbuf, char* buf);
    virtual long append(const void*, const ibis::bitvector&) {return -1;}
    /// Return the integers corresponding to the select strings.
    virtual array_t<uint32_t>* selectUInts(const bitvector& mask) const;
    virtual std::vector<std::string>*
    selectStrings(const bitvector& mask) const;
    virtual int getString(uint32_t i, std::string &val) const;
    // virtual std::vector<ibis::opaque>*
    // selectOpaques(const bitvector& mask) const;

    virtual uint32_t getNumKeys() const;
    virtual const char* getKey(uint32_t i) const;
    virtual const char* isKey(const char* str) const;

    virtual void write(FILE* file) const;
    virtual void print(std::ostream& out) const;

    ibis::direkte* fillIndex(const char *dir=0) const;
    virtual const ibis::dictionary* getDictionary() const;
    int setDictionary(const dictionary&);

private:
    // private member variables

    // dictionary is mutable in order to delay the reading of dictionary
    // from disk as late as possible
    mutable ibis::dictionary dic;

    // private member functions
    void prepareMembers() const;
    void readDictionary(const char *dir=0) const;

    category& operator=(const category&);
}; // ibis::category
#endif // IBIS_CATEGORY_H
