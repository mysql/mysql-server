//File: $Id$
// Author: John Wu <John.Wu at ACM.org>
// Copyright (c) 2006-2016 the Regents of the University of California
#ifndef IBIS_KEYWORDS_H
#define IBIS_KEYWORDS_H
///@file
/// This index is a keyword index for a string-valued column.  It
/// contains a boolean version of the term-document matrix and supports
/// exact matches of keywords/terms.

#include "index.h"	// base index class
#include "category.h"	// definitions of string-valued columns

/// Class ibis::keywords defines a boolean term-document matrix.  The terms
/// are stored in an ibis::dictionary and the columns of the matrix are
/// stored in a series of bitvectors.  The name term-document matrix is
/// borrowed from literature about indexing documents.  In this context, a
/// document is a row of the text column and each document ID is either
/// stored in another column of unsigned integers or simply the ordinal
/// number of the row.
///
/// The current implementation can either read a term-document list or
/// parse the binary string values with a list of delimiters to extract the
/// key words.  It first checks for the presence of a term-document list
/// which can be explicitly or implicitly specified.  Here are the options.
///
/// @li Specifying tdlist in the indexing option, e.g.,
/// @code
/// index=keywords tdlist=filename
/// @endcode
///
/// @li Specifying tdlist in a configuration file, e.g.,
/// @code
/// <table-name>.<column-name>.tdlist=filename
/// @endcode
///
/// @li Placing a file named <column-name>.tdlist among the data files.
/// This is the implicit option mentioned above.
///
/// Note that the filename given above can be either a fully qualified name
/// or a name in the same directory as the data file.
///
/// If a term-document list is provided, the document id used in the list
/// may be specified explicitly through docIdName either in the index
/// specification or in a configuration file.  An example of index
/// specification is as follows
/// @code
/// index=keywords tdlist=filename docidname=mid
/// @endcode
///
/// In a configuration file, the syntax for specifying a docIdName is as
/// follows.
/// @code
/// <table-name>.<column-name>.docIDName=<id-column-name>
/// @endcode
/// For example,
/// @code
/// enrondata.subject.docIDName=mid
/// enrondata.body.docIDName=mid
/// @endcode
/// If an ID column is not specified, the integer IDs in the @c .tdlist
/// file is assumed to the row numbers.
///
/// If the term-document list is not specified, one may specify a list of
/// delimiters for the tokenizer to parse the text values.  The list of
/// delimiters can be specified in either the index option or through a
/// configuration file.  Here is an example indexing option
///
/// @code
/// index=keywords delimiters=" \t,;"
/// @endcode
///
/// The following is an example line in a configuration file (say, ibis.rc)
/// @code
/// <table-name>.<column-name>.delimiters=","
/// @endcode
/// This particular choice is suitable for indexing set-valued columns,
/// where the values are stored as coma-separated ASCII text strings.  For
/// example, in a data set about automobiles from different manufactures,
/// the column about color choices might have the following values:
/// @verbatim
/// "model A", "red, white, tan", ...
/// "model B', "black, maroon, silver", ...
/// "model C', "red, light blue, sky blue, white, pink", ...
/// @endverbatim
/// In this case, the second column about color choices could be read in as
/// TEXT and indexed with keywords index using coma as delimiter for the
/// built-in tokenizer.
///
/// There are two different ways of building a keyword index and they can
/// each be specified explicitly or implicitly.  The precedence is as
/// follows: an explicitly specified option takes precedence over an
/// implicitly option, a term-document list has the precedence over
/// the built-in parser.
///
class ibis::keywords : public ibis::index {
public:
    virtual ~keywords() {clear();}
    explicit keywords(const ibis::column* c, const char* f=0);
    keywords(const ibis::column* c, ibis::text::tokenizer& tkn,
	     const char* f=0);
    keywords(const ibis::column* c, ibis::fileManager::storage* st);

    virtual INDEX_TYPE type() const {return KEYWORDS;}
    virtual const char* name() const {return "keywords";}
    virtual void binBoundaries(std::vector<double>& b) const {b.clear();}
    virtual void binWeights(std::vector<uint32_t>& b) const;
    virtual double getMin() const {return DBL_MAX;}
    virtual double getMax() const {return -DBL_MAX;}
    virtual double getSum() const {return -DBL_MAX;}
    long search(const char*, ibis::bitvector&) const;
    long search(const char*) const;
    long search(const std::vector<std::string>&, ibis::bitvector&) const;
    long search(const std::vector<std::string>&) const;

    virtual index* dup() const;
    virtual void print(std::ostream& out) const;
    virtual void serialSizes(uint64_t&, uint64_t&, uint64_t&) const;
    virtual int write(ibis::array_t<double> &,
                      ibis::array_t<int64_t> &,
                      ibis::array_t<uint32_t> &) const;
    virtual int write(const char* dt) const;
    virtual int read(const char* idxfile);
    virtual int read(ibis::fileManager::storage* st);
    virtual long append(const char* dt, const char* df, uint32_t nnew);

    using ibis::index::evaluate;
    using ibis::index::estimate;
    using ibis::index::undecidable;
    virtual long evaluate(const ibis::qContinuousRange& expr,
			  ibis::bitvector& hits) const;
    virtual void estimate(const ibis::qContinuousRange& expr,
			  ibis::bitvector& lower,
			  ibis::bitvector& upper) const;
    virtual uint32_t estimate(const ibis::qContinuousRange& expr) const;
    /// This class and its derived classes should produce exact answers,
    /// therefore no undecidable rows.
    virtual float undecidable(const ibis::qContinuousRange &,
			      ibis::bitvector &iffy) const {
	iffy.clear();
	return 0.0;
    }
    virtual double estimateCost(const ibis::qContinuousRange& expr) const;
    virtual double estimateCost(const ibis::qDiscreteRange& expr) const;

    virtual long select(const ibis::qContinuousRange&, void*) const {
	return -1;}
    virtual long select(const ibis::qContinuousRange&, void*,
			ibis::bitvector&) const {
	return -1;}

    class tokenizer;

protected:
    /// A dictionary for the terms.
    ibis::dictionary terms;

    virtual size_t getSerialSize() const throw();
    int readTermDocFile(const ibis::column* idcol, const char* f);
    inline char readTerm(const char*& buf, std::string &key) const;
    inline uint32_t readUInt(const char*& buf) const;
    int readTDLine(std::istream& in, std::string& key,
		   std::vector<uint32_t>& idlist,
		   char* buf, uint32_t nbuf) const;
    void setBits(std::vector<uint32_t>& pos, ibis::bitvector& bvec) const;
    int parseTextFile(ibis::text::tokenizer &tkn, const char *f);

    void clear();
    void reorderTerms();
}; // class ibis::keywords

/// Extract the term from a line of input term-document file.
/// A keyword is any number of printable characters.  Returns the first
/// non-space character following the keyword, which should be the
/// delimiter ':'.  Consecutive spaces in the keyword are replaced with a
/// single plain space character.
inline char ibis::keywords::readTerm(const char*& buf,
				     std::string &keyword) const {
    while (isspace(*buf)) // skip leading space
	++ buf;
    while (isprint(*buf)) { // loop through all printable till the delimiter
	if (*buf == ':') {
	    return *buf;
	}
	else if (isspace(*buf)) {
	    for (++ buf; isspace(*buf); ++ buf);
	    if (*buf == ':') {
		return *buf;
	    }
	    else {
		keyword += ' ';
		keyword += *buf;
		++ buf;
	    }
	}
	else {
	    keyword += *buf;
	    ++ buf;
	}
    }
    return *buf;
} // ibis::keywords::readTerm

/// Extract the next integer in an inputline.
inline uint32_t ibis::keywords::readUInt(const char*& buf) const {
    uint32_t res = 0;
    while (*buf && ! isdigit(*buf)) // skip leading non-digit
	++ buf;

    while (isdigit(*buf)) {
	res = res * 10 + (*buf - '0');
	++ buf;
    }
    return res;
} // ibis::keywords::readUInt

/// A simple tokenizer used to extract keywords.  A text field (i.e., a row
/// of a text column) is split into a list of null-terminated tokens and
/// each of these token is a keyword that could be searched.
class ibis::keywords::tokenizer : public ibis::text::tokenizer {
public:
    tokenizer(const char *d=ibis::util::delimiters);
    /// Destructor.
    virtual ~tokenizer() {}

    virtual int operator()(std::vector<const char*>& tkns, char *buf);

protected:
    /// The list of delimiters.  May be empty.
    std::string delim_;
}; // class ibis::keywords::tokenizer
#endif
