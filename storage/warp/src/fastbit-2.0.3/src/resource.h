// $Id$
// Author: John Wu <John.Wu at ACM.org>
// Lawrence Berkeley National Laboratory
// Copyright (c) 2000-2016 the Regents of the University of California
#ifndef IBIS_RESOURCE_H
#define IBIS_RESOURCE_H
///@file
/// Defines a class to hold name-value pairs.
#include "util.h"	// ibis::util::strnewdup(), std::less<const char*>
#include <fstream>	// std::ofstream
#include <map>		// std::map

/// A container for name-value pairs.
///
/// It is mostly used for storing the configuration parameters.  The
/// parameters are in a format as follows:
/// @code
/// group:group:...:name=value
/// @endcode
/// where the delimiter can be either '*', ':' or '.' and anything
/// following the first '=' sign is assumed to be part of the value string
/// until the end of line.  The leading and trailing spaces are removed from
/// both the name and the value.  The specification that appears later in
/// the same configuration file or read later (through a call to read())
/// will overwrite the parameter with the same name.  This include the
/// groups and individual parameters.  For example if a parameter named
/// 'abcd' is specified first, but later, the same name is used as a group
/// name, then the previously specified parameter will be removed from the
/// list and the new group will be inserted.  If the parameter with name
/// 'abcd' appeared again, then the group 'abcd' will be removed and the
/// named parameter will be inserted.
///
/// The line length must of no more than MAX_LINE defined in const.h.
///
/// The top level group name can be any one of the following: all, common,
/// and '*'.  When writing out the parameters, the top level name is not
/// written.
class FASTBIT_CXX_DLLSPEC ibis::resource {
public:
    /// The name-value pairs are categorized into two types, names that map
    /// to simple values (vList) and names that map to groups of name-value
    /// pairs (gList).
    typedef std::map< const char*, resource*, ibis::lessi > gList;
    typedef std::map< const char*, char*, ibis::lessi > vList;

    ~resource() {clear();};
    /// Default constructor.  Creates an empty object.
    resource() : prefix(0), context(0) {};
    /// Read the content of a parameter file.
    explicit resource(const char *fn) : prefix(0), context(0) {(void)read(fn);}
    /// Create an empty object with the specified prefix and context.
    resource(const resource* ctx, const char* pfx) :
	prefix(ibis::util::strnewdup(pfx)), context(ctx) {}
    /// Copy constructor.  Deep copy.
    resource(const resource& rhs) :
	groups(rhs.groups), values(rhs.values),
	prefix(ibis::util::strnewdup(rhs.prefix)), context(rhs.context) {}
    resource& operator=(const resource& rhs);

    /// Locate the named parameter and return its value in raw string form.
    const char* operator[](const char *name) const;
    /// Locate the named parameter and return its value as a number.
    double getNumber(const char* name) const;
    /// Locate the named parameter and return its value as true or false.
    bool isTrue(const char *name) const;

    int  read(const char* fn=0);
    void write(const char* fn=0) const;
    void add(const char *name, const char *value);

    /// Returns true if there is no name-value pair on record.
    bool empty() const {return (values.empty() && groups.empty());}
    gList::const_iterator gBegin() const {return groups.begin();}
    gList::const_iterator gEnd() const {return groups.end();}
    vList::const_iterator vBegin() const {return values.begin();}
    vList::const_iterator vEnd() const {return values.end();}

    /// Find a group with the given name.  The name is expected to be a
    /// simple name without any separators.  Any separator in the name will
    /// cause it to return a nil pointer.
    inline const resource* getGroup(const char* name) const;
    /// Find a named parameter.  The name is expected to be a simple name
    /// without any separators.  Any separator in it will cause a nil
    /// pointer to be returned.
    inline const char* getValue(const char *name) const;
    /// Return the name of the full prefix of the resource
    inline std::string getPrefix() const;

    /// Clear a vList
    static void clear(vList &vl);
    /// Parse a string into a name-value list
    static void parseNameValuePairs(const char* in, vList& lst);
    static bool isStringTrue(const char *val);

private:
    static const char* delimiters;
    gList groups;	///!< Names for Groups of values.
    vList values;	///!< Name-value pairs.
    const char *prefix;
    const resource* context;

    void clear(); // clear the memory occupied by the strings
    void write(std::ostream& out, const char* ctx=0) const;
};

// only search the top level level
inline const ibis::resource* ibis::resource::getGroup(const char* name) const {
    const ibis::resource* group = 0;
    if (name==0) return group;
    if (*name==static_cast<char>(0)) return group;

    gList::const_iterator it = groups.find(name);
    if (it != groups.end())
	group = (*it).second;
    return group;
} // ibis::resource::getGroup

// only search the top level
inline const char* ibis::resource::getValue(const char* name) const {
    const char* value = 0;
    if (name==0) return value;
    if (*name==static_cast<char>(0)) return value;

    vList::const_iterator it = values.find(name);
    if (it != values.end())
	value = (*it).second;
    return value;
} // ibis::resource::getValue

// get the full prefix of the resource
inline std::string ibis::resource::getPrefix() const {
    std::string ret;
    if (context != 0)
	ret = context->getPrefix();
    if (prefix != 0) {
	if (ret.empty()) {
	    ret = prefix;
	}
	else {
	    ret += '.';
	    ret += prefix;
	}
    }
    return ret;
} // ibis::resource::getPrefix

/// Returns @c true is the string value should be interpreted as logical
/// truth.  The string values string with 'y', 't', and '1' (the number
/// one), and the string "on" are interpreted as true.  All other strings
/// are interpreted as false.
inline bool ibis::resource::isStringTrue(const char *val) {
    return(val != 0 && *val != 0 &&
	   ((*val == '1') || (*val == 't') || (*val == 'y') ||
	    (*val == 'T') || (*val == 'Y') || (stricmp(val, "on") == 0)));
} // ibis::resource::isStringTrue
#endif // IBIS_RESOURCE_H
