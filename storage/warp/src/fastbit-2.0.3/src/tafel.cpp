// File $Id$
// Author: John Wu <John.Wu at ACM.org> Lawrence Berkeley National Laboratory
// Copyright (c) 2007-2016 the Regents of the University of California
//
#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(disable:4786)   // some identifier longer than 256 characters
#endif

#include "tafel.h"      // ibis::tafel
#include "bord.h"       // ibis::part, ibis::bord
#include "blob.h"       // ibis::opaque

#include <fstream>      // std::ofstream
#include <limits>       // std::numeric_limits
#include <typeinfo>     // typeid
#include <memory>       // std::unique_ptr
#include <iomanip>      // std::setfill

#include <stdlib.h>     // strtol strtoul [strtoll strtoull]
// This file definte does not use the min and max macro.  Their presence
// could cause the calls to numeric_limits::min and numeric_limits::max to
// be misunderstood!
#undef max
#undef min

/// Add metadata about a new column.
/// Return value
/// -  0 == success,
/// - -2 == invalid name or type,
/// -  1 == name already in the list of columns, same type,
/// - -1 == existing column with different type.
int ibis::tafel::addColumn(const char* cn, ibis::TYPE_T ct,
                           const char* cd, const char* idx) {
    if (cn == 0 || *cn == 0 || ct == ibis::UNKNOWN_TYPE) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- tafel::addColumn(" << (void*)cn << ", "
            << (void*)ct << ", " << (void*)cd << ", " << (void*)idx
            << ") expects a valid name (1st arguement) and type (2nd argument)";
        return -2;
    }
    columnList::iterator it = cols.find(cn);
    if (it != cols.end()) {
        LOGGER(ibis::gVerbose > 1)
            << "tafel::addColumn(" << cn << ", " << ct
            << ") -- name already in the data partition";
        if (cd != 0 && *cd != 0)
            it->second->desc = cd;
        if (idx != 0 && *idx != 0)
            it->second->indexSpec = idx;
        return (ct == it->second->type ? 1 : -1);
    }

    column* col = new column();
    col->name = cn;
    col->type = ct;
    col->desc = (cd && *cd ? cd : cn);
    if (idx != 0 && *idx != 0)
        col->indexSpec = idx;
    switch (ct) {
    case ibis::BYTE:
        col->values = new array_t<signed char>();
        break;
    case ibis::UBYTE:
        col->values = new array_t<unsigned char>();
        break;
    case ibis::SHORT:
        col->values = new array_t<int16_t>();
        break;
    case ibis::USHORT:
        col->values = new array_t<uint16_t>();
        break;
    case ibis::INT:
        col->values = new array_t<int32_t>();
        break;
    case ibis::UINT:
        col->values = new array_t<uint32_t>();
        break;
    case ibis::LONG:
        col->values = new array_t<int64_t>();
        break;
    case ibis::OID:
    case ibis::ULONG:
        col->values = new array_t<uint64_t>();
        break;
    case ibis::FLOAT:
        col->values = new array_t<float>();
        break;
    case ibis::DOUBLE:
        col->values = new array_t<double>();
        break;
    case ibis::TEXT:
    case ibis::CATEGORY:
        col->values = new std::vector<std::string>();
        break;
    case ibis::BLOB:
        col->values = new std::vector<ibis::opaque>();
    default:
        break;
    }
    cols[col->name.c_str()] = col;
    colorder.push_back(col);
    return 0;
} // ibis::tafel::addColumn

/// Ingest a complete SQL CREATE TABLE statement.  Creates all metadata
/// specified.  It extracts the table name (into tname) to be used later by
/// functions such as write and writeMetaData.
///
/// The statement is expected to be in the form of "create table tname
/// (column1, column2, ...)".  It can not contain embedded comments.
///
/// Because the SQL standard supports many more data types than FastBit
/// does, many SQL column types are mapped in a crude manner.  Here is the
/// current list.
/// - enum = ibis::CATEGORY; the values specified are not recorded.
/// - set = ibis::CATEGORY; this treatment does not fully reflect the
///         flexibility with which a set value can be handled in SQL.
/// - blob = ibis::BLOB; however, since SQL dump file contains only
///          printable characters, this function simply pickup printable
///          characters as with strings.
int ibis::tafel::SQLCreateTable(const char *stmt, std::string &tname) {
    if (stmt == 0 && *stmt == 0) return -1;
    if (strnicmp(stmt, "create table ", 13) != 0)
        return -1;

    const char *buf = stmt + 13;
    int ierr = ibis::util::readString(tname, buf, 0); // extract table name
    LOGGER(ierr < 0 && ibis::gVerbose > 0)
        << "Warning -- tafel::SQLCreateTable cannot extract a name from \""
        << stmt << "\"";

    while (*buf != 0 && *buf != '(') ++ buf;
    buf += (*buf == '('); // skip opening (
    if (buf == 0 || *buf == 0) { // incomplete SQL statement
        tname.erase();
        return -1;
    }

    clear(); // clear all existing content
    std::string colname, tmp;
    ibis::tafel::column *col;
    const char *delim = " ,;\t\n\v";
    while (*buf != 0 && *buf != ')') { // loop till closing )
        ierr = ibis::util::readString(colname, buf, 0);
        if (colname.empty()) {
            LOGGER(ibis::gVerbose >= 0)
                << "tafel::SQLCreateTable failed to extract a column";
            return -2;
        }
        else if (stricmp(colname.c_str(), "key") == 0) { // reserved word
            // KEY name (name, name)
            ierr = ibis::util::readString(colname, buf, 0);
            while (*buf != 0 && *buf != '(' && *buf != ',') ++ buf;
            if (*buf == '(') { // skip till ')'
                while (*buf != 0 && *buf != ')') ++ buf;
                buf += (*buf == ')');
            }
            while (*buf != 0 && *buf != ',' && *buf != ')') ++ buf;
            buf += (*buf == ',');
            continue;
        }

        while (*buf !=0 && isspace(*buf)) ++ buf;
        switch (*buf) { // for data types
        default: { // unknown type
                col = 0;
                ierr = ibis::util::readString(tmp, buf, delim);
                LOGGER(ibis::gVerbose > 0)
                    << "tafel::SQLCreateTable column " << colname
                    << " has a unexpected type (" << tmp
                    << "), skip column specification";
                while (*buf != 0 && *buf != ',') ++ buf;
                break;}
        case 'b':
        case 'B': { // blob/bigint
            if (strnicmp(buf, "bigint", 6) == 0) {
                col = new ibis::tafel::column;
                col->name.swap(colname);
                buf += 6;
                if (*buf == '(') {
                    for (++ buf; *buf != ')'; ++ buf);
                    buf += (*buf == ')');
                }
                while (*buf != 0 && isspace(*buf)) ++ buf;
                if (*buf != 0 && strnicmp(buf, "unsigned", 9) == 0) {
                    buf += 8;
                    col->type = ibis::ULONG;
                    col->values = new ibis::array_t<uint64_t>();
                }
                else {
                    col->type = ibis::LONG;
                    col->values = new ibis::array_t<int64_t>();
                }
            }
            else { // assume blob
                buf += 4;
                col = new ibis::tafel::column;
                col->name.swap(colname);
                col->type = ibis::BLOB;
                col->values = new std::vector<ibis::opaque>();
            }
            break;}
        case 'e':
        case 'E': { // enum
            buf += 4;
            while (*buf != ',' && isspace(*buf)) ++ buf;
            if (*buf == '(') {
                for (++ buf; *buf != 0 && *buf != ')'; ++ buf);
                buf += (*buf == ')');
            }
            col = new ibis::tafel::column;
            col->name.swap(colname);
            col->type = ibis::CATEGORY;
            col->values = new std::vector<std::string>();
            break;}
        case 'd':
        case 'D': { // double
            buf += 6;
            if (*buf == '(') {
                for (++ buf; *buf != ')'; ++ buf);
                buf += (*buf == ')');
            }
            col = new ibis::tafel::column;
            col->name.swap(colname);
            col->type = ibis::DOUBLE;
            col->values = new ibis::array_t<double>();
            break;}
        case 'f':
        case 'F': { // float
            buf += 5;
            if (*buf == '(') {
                for (++ buf; *buf != ')'; ++ buf);
                buf += (*buf == ')');
            }
            col = new ibis::tafel::column;
            col->name.swap(colname);
            col->type = ibis::FLOAT;
            col->values = new ibis::array_t<float>();
            break;}
        case 'i':
        case 'I': { // int/integer          
            col = new ibis::tafel::column;
            col->name.swap(colname);
            buf += (strnicmp(buf, "integer", 7) == 0 ? 7 : 3);
            if (*buf == '(') {
                for (++ buf; *buf != ')'; ++ buf);
                buf += (*buf == ')');
            }
            while (*buf != 0 && isspace(*buf)) ++ buf;
            if (*buf != 0 && strnicmp(buf, "unsigned", 8) == 0) {
                buf += 8;
                col->type = ibis::UINT;
                col->values = new ibis::array_t<uint32_t>();
            }
            else {
                col->type = ibis::INT;
                col->values = new ibis::array_t<int32_t>();
            }
            break;}
        case 's':
        case 'S': { // smallint/short/set
            col = new ibis::tafel::column;
            col->name.swap(colname);
            if (strnicmp(buf, "set", 3) == 0) {
                buf += 3;
                while (*buf != ',' && isspace(*buf)) ++ buf;
                if (*buf == '(') {
                    for (++ buf; *buf != 0 && *buf != ')'; ++ buf);
                    buf += (*buf == ')');
                }
                col->type = ibis::CATEGORY;
                col->values = new std::vector<std::string>();
            }
            else { // assume smallint
                buf += (strnicmp(buf, "short", 5) == 0 ? 5 : 8);
                if (*buf == '(') {
                    for (++ buf; *buf != ')'; ++ buf);
                    buf += (*buf == ')');
                }
                while (*buf != 0 && isspace(*buf)) ++ buf;
                if (*buf != 0 && strnicmp(buf, "unsigned", 8) == 0) {
                    buf += 8;
                    col->type = ibis::USHORT;
                    col->values = new ibis::array_t<uint16_t>();
                }
                else {
                    col->type = ibis::SHORT;
                    col->values = new ibis::array_t<int16_t>();
                }
            }
            break;}
        case 't':
        case 'T': { // tinyint
            col = new ibis::tafel::column;
            col->name.swap(colname);
            buf += 7;
            if (*buf == '(') {
                for (++ buf; *buf != ')'; ++ buf);
                buf += (*buf == ')');
            }
            while (*buf != 0 && isspace(*buf)) ++ buf;
            if (*buf != 0 && strnicmp(buf, "unsigned", 8) == 0) {
                buf += 8;
                col->type = ibis::UBYTE;
                col->values = new ibis::array_t<unsigned char>();
            }
            else {
                col->type = ibis::BYTE;
                col->values = new ibis::array_t<signed char>();
            }
            break;}
        case 'v':
        case 'V': { // varchar
            col = new ibis::tafel::column;
            col->name.swap(colname);
            buf += 7;
            int precision = 0;
            if (*buf == '(') {
                for (++ buf; isdigit(*buf); ++ buf) {
                    precision = 10 * precision + (*buf - '0');
                }
                while (*buf != 0 && *buf != ')') ++ buf;
                buf += (*buf == ')');
            }
            if (precision < 6) {
                col->type = ibis::CATEGORY;
            }
            else {
                col->type = ibis::TEXT;
            }
            col->values = new std::vector<std::string>();
            break;}
        }

        if (col != 0) { // add col to cols
            cols[col->name.c_str()] = col;
            colorder.push_back(col);

            while (*buf != 0 && *buf != ',') {
                ierr = ibis::util::readString(tmp, buf, delim);
                if ((!tmp.empty()) && stricmp(tmp.c_str(), "default") == 0) {
                    int ierr = assignDefaultValue(*col, buf);
                    LOGGER(ierr < 0 && ibis::gVerbose > 1)
                        << "tafel::SQLCreateTable failed to assign a default "
                        "value to column " << col->name;
                    break;
                }
            }

            if (ibis::gVerbose > 4) {
                ibis::util::logger lg;
                lg() << "tafel::SQLCreateTable created column "
                            << col->name << " with type "
                            << ibis::TYPESTRING[(int)col->type];
                if (col->defval != 0) {
                    lg() << " and defaul value ";
                    switch (col->type) {
                    case ibis::BYTE:
                        lg() << static_cast<short>
                            (*static_cast<signed char*>(col->defval));
                        break;
                    case ibis::UBYTE:
                        lg() << static_cast<short>
                            (*static_cast<unsigned char*>(col->defval));
                        break;
                    case ibis::SHORT:
                        lg() << *static_cast<int16_t*>(col->defval);
                        break;
                    case ibis::USHORT:
                        lg() << *static_cast<uint16_t*>(col->defval);
                        break;
                    case ibis::INT:
                        lg() << *static_cast<int32_t*>(col->defval);
                        break;
                    case ibis::UINT:
                        lg() << *static_cast<uint32_t*>(col->defval);
                        break;
                    case ibis::LONG:
                        lg() << *static_cast<int64_t*>(col->defval);
                        break;
                    case ibis::ULONG:
                        lg() << *static_cast<uint64_t*>(col->defval);
                        break;
                    case ibis::FLOAT:
                        lg() << *static_cast<float*>(col->defval);
                        break;
                    case ibis::DOUBLE:
                        lg() << *static_cast<double*>(col->defval);
                        break;
                    case ibis::TEXT:
                    case ibis::CATEGORY:
                        lg() << *static_cast<std::string*>(col->defval);
                        break;
                    case ibis::BLOB:
                        lg() << *static_cast<ibis::opaque*>(col->defval);
                        break;
                    default:
                        break;
                    }
                }
            }
        }

        // skip the remaining part of this column specification
        while (*buf != 0 && *buf != ',') ++ buf;
        buf += (*buf == ',');
    }

    LOGGER(ibis::gVerbose > 2)
        << "tafel::SQLCreateTable extract meta data for " << cols.size()
        << " column" << (cols.size()>1 ? "s" : "") << " from " << stmt;
    return cols.size();
} // ibis::tafel::SQLCreateTable

/// Assign the default value for the given column.  Returns 0 on success
/// and a negative number for error.
int ibis::tafel::assignDefaultValue(ibis::tafel::column& col,
                                    const char *val) const {
    char *ptr;
    switch (col.type) {
    case ibis::BYTE: {
        long tmp = strtol(val, &ptr, 0);
        if (tmp >= -128 && tmp <= 127) {
            char *actual = new char;
            *actual = static_cast<char>(tmp);
            col.defval = actual;
        }
        else {
            LOGGER(ibis::gVerbose > 1)
                << "tafel::assignDefaultValue(" << col.name << ", " << val
                << ") can not continue because the value (" << tmp
                << ") is out of range for column type BYTE";
            return -14;
        }
        break;}
    case ibis::UBYTE: {
        long tmp = strtol(val, &ptr, 0);
        if (tmp >= 0 && tmp <= 255) {
            unsigned char *actual = new unsigned char;
            *actual = static_cast<unsigned char>(tmp);
            col.defval = actual;
        }
        else {
            LOGGER(ibis::gVerbose > 1)
                << "tafel::assignDefaultValue(" << col.name << ", " << val
                << ") can not continue because the value (" << tmp
                << ") is out of range for column type UBYTE";
            return -13;
        }
        break;}
    case ibis::SHORT: {
        long tmp = strtol(val, &ptr, 0);
        if (tmp >= std::numeric_limits<int16_t>::min() &&
            tmp <= std::numeric_limits<int16_t>::max()) {
            int16_t *actual = new int16_t;
            *actual = static_cast<int16_t>(tmp);
            col.defval = actual;
        }
        else {
            LOGGER(ibis::gVerbose > 1)
                << "tafel::assignDefaultValue(" << col.name << ", " << val
                << ") can not continue because the value (" << tmp
                << ") is out of range for column type SHORT";
            return -12;
        }
        break;}
    case ibis::USHORT: {
        long tmp = strtol(val, &ptr, 0);
        if (tmp >= 0 && tmp <= std::numeric_limits<uint16_t>::max()) {
            uint16_t *actual = new uint16_t;
            *actual = static_cast<uint16_t>(tmp);
            col.defval = actual;
        }
        else {
            LOGGER(ibis::gVerbose > 1)
                << "tafel::assignDefaultValue(" << col.name << ", " << val
                << ") can not continue because the value (" << tmp
                << ") is out of range for column type USHORT";
            return -11;
        }
        break;}
    case ibis::INT: {
        long tmp = strtol(val, &ptr, 0);
        if (tmp >= std::numeric_limits<int32_t>::min() &&
            tmp <= std::numeric_limits<int32_t>::max()) {
            int32_t *actual = new int32_t;
            *actual = static_cast<int32_t>(tmp);
            col.defval = actual;
        }
        else {
            LOGGER(ibis::gVerbose > 1)
                << "tafel::assignDefaultValue(" << col.name << ", " << val
                << ") can not continue because the value (" << tmp
                << ") is out of range for column type INT";
            return -10;
        }
        break;}
    case ibis::UINT: {
        unsigned long tmp = strtoul(val, &ptr, 0);
        if (tmp <= std::numeric_limits<uint32_t>::max()) {
            uint32_t *actual = new uint32_t;
            *actual = static_cast<uint32_t>(tmp);
            col.defval = actual;
        }
        else {
            LOGGER(ibis::gVerbose > 1)
                << "tafel::assignDefaultValue(" << col.name << ", " << val
                << ") can not continue because the value (" << tmp
                << ") is out of range for column type UINT";
            return -9;
        }
        break;}
    case ibis::LONG: {
        errno = 0;
#if defined(_WIN32) && defined(_MSC_VER)
        long long tmp = strtol(val, &ptr, 0);
#elif defined(HAVE_STRTOLL) || defined(__USE_ISOC99) || (defined(__GLIBC_HAVE_LONG_LONG) && defined(__USE_MISC))
        long long tmp = strtoll(val, &ptr, 0);
#else
        long long tmp = strtol(val, &ptr, 0);
#endif
        if (errno == 0) {
            int64_t *actual = new int64_t;
            *actual = tmp;
            col.defval = actual;
        }
        else {
            LOGGER(ibis::gVerbose > 1)
                << "tafel::assignDefaultValue(" << col.name << ", " << val
                << ") can not continue because the value (" << tmp
                << ") is invalid or out of range for column type LONG";
            return -8;
        }
        break;}
    case ibis::ULONG: {
        errno = 0;
#if defined(_WIN32) && defined(_MSC_VER)
        unsigned long long tmp = strtoul(val, &ptr, 0);
#elif defined(HAVE_STRTOLL) || defined(__USE_ISOC99) || (defined(__GLIBC_HAVE_LONG_LONG) && defined(__USE_MISC))
        unsigned long long tmp = strtoull(val, &ptr, 0);
#else
        unsigned long long tmp = strtoul(val, &ptr, 0);
#endif
        if (errno == 0) {
            uint64_t *actual = new uint64_t;
            *actual = static_cast<uint64_t>(tmp);
            col.defval = actual;
        }
        else {
            LOGGER(ibis::gVerbose > 1)
                << "tafel::assignDefaultValue(" << col.name << ", " << val
                << ") can not continue because the value (" << tmp
                << ") is invalid or out of range for column type ULONG";
            return -7;
        }
        break;}
    case ibis::FLOAT: {
        errno = 0;
#if defined(_ISOC99_SOURCE) || defined(_ISOC9X_SOURCE) \
    || (__STDC_VERSION__+0 >= 199901L)
        float tmp = strtof(val, &ptr);
#else
        float tmp = strtod(val, &ptr);
#endif
        if (errno == 0) { // no conversion error
            float *actual = new float;
            *actual = tmp;
            col.defval = actual;
        }
        else {
            LOGGER(ibis::gVerbose > 1)
                << "tafel::assignDefaultValue(" << col.name << ", " << val
                << ") can not continue because the value (" << tmp
                << ") is invalid or out of range for column type FLOAT";
            return -6;
        }
        break;}
    case ibis::DOUBLE: {
        errno = 0;
        double tmp = strtod(val, &ptr);
        if (errno == 0) { // no conversion error
            double *actual = new double;
            *actual = tmp;
            col.defval = actual;
        }
        else {
            LOGGER(ibis::gVerbose > 1)
                << "tafel::assignDefaultValue(" << col.name << ", " << val
                << ") can not continue because the value (" << tmp
                << ") is invalid or out of range for column type DOUBLE";
            return -5;
        }
        break;}
    case ibis::TEXT:
    case ibis::CATEGORY: {
        if (col.defval == 0)
            col.defval = new std::string;
        std::string &str = *(static_cast<std::string*>(col.defval));
        str.clear();
        if (val != 0 && *val != 0)
            (void) ibis::util::readString(str, val, 0);
        break;}
    case ibis::BLOB: {
        if (col.defval == 0) // set default value to an empty object
            col.defval = new ibis::opaque;
        std::string str;
        if (val != 0 && *val != 0)
            (void) ibis::util::readString(str, val, 0);
        if (! str.empty())
            static_cast<ibis::opaque*>(col.defval)->
                copy(str.data(), str.size());
        break;}
    default: {
        LOGGER(ibis::gVerbose > 1)
            << "tafel::assignDefaultValue(" << col.name << ", " << val
            << ") can not handle column type "
            << ibis::TYPESTRING[(int)col.type];
        return -3;}
    } // switch (col.type)
    return 0;
} // ibis::tafel::assignDefault

/// Add values to an array of type T.  The input values (in) are copied to
/// out[be:en-1].  If the array out has less then be elements to start
/// with, it will be filled with value fill.  The output mask indicates
/// whether the values in array out are valid.  This version works with one
/// column as at a time.
/// @note It is a const function because it only makes changes to its
/// arguments.
template <typename T>
void ibis::tafel::append(const T* in, ibis::bitvector::word_t be,
                         ibis::bitvector::word_t en, array_t<T>& out,
                         const T& fill, ibis::bitvector& mask) const {
    ibis::bitvector inmsk;
    inmsk.appendFill(0, be);
    inmsk.appendFill(1, en-be);
    if (out.size() > en)
        inmsk.appendFill(0, out.size()-en);
    if (out.size() < be)
        out.insert(out.end(), be-out.size(), fill);
    if (out.size() < en) {
        out.resize(en);
        mask.adjustSize(0, en);
    }
    std::copy(in, in+(en-be), out.begin()+be);
    mask |= inmsk;

    LOGGER(ibis::gVerbose > 7)
        << "tafel::append(" << typeid(T).name()
        << ", " << be << ", " << en << ")\ninmask: "
        << inmsk << "totmask: " << mask;
} // ibis::tafel::append

/// Copy the incoming strings to out[be:en-1].  Work with one column at a
/// time.
/// @note It is a const function because it only makes changes to its
/// arguments.
void ibis::tafel::appendString(const std::vector<std::string>* in,
                               ibis::bitvector::word_t be,
                               ibis::bitvector::word_t en,
                               std::vector<std::string>& out,
                               ibis::bitvector& mask) const {
    ibis::bitvector inmsk;
    inmsk.appendFill(0, be);
    inmsk.appendFill(1, en-be);
    if (out.size() < be) {
        const std::string tmp;
        out.insert(out.end(), be-out.size(), tmp);
    }
    if (out.size() > en)
        inmsk.appendFill(0, out.size()-en);
    if (out.size() < en) {
        out.resize(en);
        mask.adjustSize(0, en);
    }
    std::copy(in->begin(), in->begin()+(en-be), out.begin()+be);
    mask |= inmsk;

    LOGGER(ibis::gVerbose > 7)
        << "tafel::appendString(" << be << ", " << en << ")\ninmask: "
        << inmsk << "totmask: " << mask;
} // ibis::tafel::appendString

/// Copy the incoming values to rows [begin:end) of column cn.
int ibis::tafel::append(const char* cn, uint64_t begin, uint64_t end,
                        void* values) {
    ibis::bitvector::word_t be = static_cast<ibis::bitvector::word_t>(begin);
    ibis::bitvector::word_t en = static_cast<ibis::bitvector::word_t>(end);
    if (be != begin || en != end || be >= en || cn == 0 || *cn == 0 ||
        values == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "tafel::append(" << cn << ", " << begin << ", " << end
            << ", " << values << ") can not proceed because of invalid "
            "parameters";
        return -1;
    }

    columnList::iterator it = cols.find(cn);
    if (it == cols.end()) {
        LOGGER(ibis::gVerbose > 0)
            << "tafel::append(" << cn << ", " << begin << ", " << end
            << ", " << values << ") can not proceed because " << cn
            << " is not a column of this data partition";
        return -2;
    }

    if (en > mrows) mrows = en;
    column& col = *((*it).second);
    switch (col.type) {
    case ibis::BYTE:
        append(static_cast<const signed char*>(values), be, en,
               *static_cast<array_t<signed char>*>(col.values),
               (signed char)0x7F, col.mask);
        break;
    case ibis::UBYTE:
        append(static_cast<const unsigned char*>(values), be, en,
               *static_cast<array_t<unsigned char>*>(col.values),
               (unsigned char)0xFFU, col.mask);
        break;
    case ibis::SHORT:
        append(static_cast<const int16_t*>(values), be, en,
               *static_cast<array_t<int16_t>*>(col.values),
               (int16_t)0x7FFF, col.mask);
        break;
    case ibis::USHORT:
        append(static_cast<const uint16_t*>(values), be, en,
               *static_cast<array_t<uint16_t>*>(col.values),
               (uint16_t)0xFFFFU, col.mask);
        break;
    case ibis::INT:
        append(static_cast<const int32_t*>(values), be, en,
               *static_cast<array_t<int32_t>*>(col.values),
               (int32_t)0x7FFFFFFF, col.mask);
        break;
    case ibis::UINT:
        append(static_cast<const uint32_t*>(values), be, en,
               *static_cast<array_t<uint32_t>*>(col.values),
               (uint32_t)0xFFFFFFFFU, col.mask);
        break;
    case ibis::LONG:
        append<int64_t>(static_cast<const int64_t*>(values), be, en,
                        *static_cast<array_t<int64_t>*>(col.values),
                        0x7FFFFFFFFFFFFFFFLL, col.mask);
        break;
    case ibis::ULONG:
        append<uint64_t>(static_cast<const uint64_t*>(values), be, en,
                         *static_cast<array_t<uint64_t>*>(col.values),
                         0xFFFFFFFFFFFFFFFFULL, col.mask);
        break;
    case ibis::FLOAT:
        append(static_cast<const float*>(values), be, en,
               *static_cast<array_t<float>*>(col.values),
               FASTBIT_FLOAT_NULL, col.mask);
        break;
    case ibis::DOUBLE:
        append(static_cast<const double*>(values), be, en,
               *static_cast<array_t<double>*>(col.values),
               FASTBIT_DOUBLE_NULL, col.mask);
        break;
    case ibis::TEXT:
    case ibis::CATEGORY:
        appendString(static_cast<const std::vector<std::string>*>(values),
                     be, en,
                     *static_cast<std::vector<std::string>*>(col.values),
                     col.mask);
        break;
    default:
        break;
    }
#if _DEBUG+0 > 0 || DEBUG+0 > 0
    LOGGER(ibis::gVerbose > 6)
        << "tafel::append(" << cn  << ", " << begin << ", " << end
        << ", " << values << ") worked with column "
        << static_cast<void*>((*it).second) << " with mask("
        << it->second->mask.cnt() << " out of " << it->second->mask.size()
        << ")";
#endif
    return 0;
} // ibis::tafel::append

void ibis::tafel::normalize() {
    if (cols.empty()) return;
    // loop one - determine the maximum values is all the columns
    bool need2nd = false;
    for (columnList::iterator it = cols.begin(); it != cols.end(); ++ it) {
        column& col = *((*it).second);
        switch (col.type) {
        case ibis::BYTE: {
            array_t<signed char>& vals =
                *static_cast<array_t<signed char>*>(col.values);
            if (mrows < vals.size()) {
                mrows = vals.size();
                need2nd = true;
            }
            else if (mrows > vals.size()) {
                need2nd = true;
            }
            break;}
        case ibis::UBYTE: {
            array_t<unsigned char>& vals =
                *static_cast<array_t<unsigned char>*>(col.values);
            if (vals.size() > mrows) {
                mrows = vals.size();
                need2nd = true;
            }
            else if (mrows > vals.size()) {
                need2nd = true;
            }
            break;}
        case ibis::SHORT: {
            array_t<int16_t>& vals =
                *static_cast<array_t<int16_t>*>(col.values);
            if (vals.size() > mrows) {
                mrows = vals.size();
                need2nd = true;
            }
            else if (mrows > vals.size()) {
                need2nd = true;
            }
            break;}
        case ibis::USHORT: {
            array_t<uint16_t>& vals =
                *static_cast<array_t<uint16_t>*>(col.values);
            if (vals.size() > mrows) {
                mrows = vals.size();
                need2nd = true;
            }
            else if (mrows > vals.size()) {
                need2nd = true;
            }
            break;}
        case ibis::INT: {
            array_t<int32_t>& vals =
                *static_cast<array_t<int32_t>*>(col.values);
            if (vals.size() > mrows) {
                mrows = vals.size();
                need2nd = true;
            }
            else if (mrows > vals.size()) {
                need2nd = true;
            }
            break;}
        case ibis::UINT: {
            array_t<uint32_t>& vals =
                *static_cast<array_t<uint32_t>*>(col.values);
            if (vals.size() > mrows) {
                mrows = vals.size();
                need2nd = true;
            }
            else if (mrows > vals.size()) {
                need2nd = true;
            }
            break;}
        case ibis::LONG: {
            array_t<int64_t>& vals =
                *static_cast<array_t<int64_t>*>(col.values);
            if (vals.size() > mrows) {
                mrows = vals.size();
                need2nd = true;
            }
            else if (mrows > vals.size()) {
                need2nd = true;
            }
            break;}
        case ibis::OID:
        case ibis::ULONG: {
            array_t<uint64_t>& vals =
                *static_cast<array_t<uint64_t>*>(col.values);
            if (vals.size() > mrows) {
                mrows = vals.size();
                need2nd = true;
            }
            else if (mrows > vals.size()) {
                need2nd = true;
            }
            break;}
        case ibis::FLOAT: {
            array_t<float>& vals =
                *static_cast<array_t<float>*>(col.values);
            if (vals.size() > mrows) {
                mrows = vals.size();
                need2nd = true;
            }
            else if (mrows > vals.size()) {
                need2nd = true;
            }
            break;}
        case ibis::DOUBLE: {
            array_t<double>& vals =
                *static_cast<array_t<double>*>(col.values);
            if (vals.size() > mrows) {
                mrows = vals.size();
                need2nd = true;
            }
            else if (mrows > vals.size()) {
                need2nd = true;
            }
            break;}
        case ibis::TEXT:
        case ibis::CATEGORY: {
            std::vector<std::string>& vals =
                *static_cast<std::vector<std::string>*>(col.values);
            if (vals.size() > mrows) {
                mrows = vals.size();
                need2nd = true;
            }
            else if (mrows > vals.size()) {
                need2nd = true;
            }
            break;}
        case ibis::BLOB: {
            std::vector<ibis::opaque>& vals =
                *static_cast<std::vector<ibis::opaque>*>(col.values);
            if (vals.size() > mrows) {
                mrows = vals.size();
                need2nd = true;
            }
            else if (mrows > vals.size()) {
                need2nd = true;
            }
            break;}
        default: {
            break;}
        }
        if (col.mask.size() > mrows) {
            LOGGER(ibis::gVerbose >= 0)
                << "tafel::normalize - col[" << col.name << "].mask("
                << col.mask.cnt() << ", " << col.mask.size() << ") -- mrows = "
                << mrows;
            mrows = col.mask.size();
            need2nd = true;
        }
    }
    if (! need2nd) return;

    LOGGER(ibis::gVerbose > 5)
        << "tafel::normalize - setting number of rows to " << mrows
        << ", adjusting all in-memory data to reflect this change";
    // second loop - adjust the arrays and null masks
    for (columnList::iterator it = cols.begin(); it != cols.end(); ++ it) {
        column& col = *((*it).second);
        switch (col.type) {
        case ibis::BYTE: {
            array_t<signed char>& vals =
                *static_cast<array_t<signed char>*>(col.values);
            if (vals.size() < mrows) {
                if (col.defval != 0) {
                    col.mask.adjustSize(mrows, mrows);
                    vals.insert(vals.end(), mrows-vals.size(),
                                *static_cast<signed char*>(col.defval));
                }
                else {
                    col.mask.adjustSize(vals.size(), mrows);
                    vals.insert(vals.end(), mrows-vals.size(), 0x7F);
                }
            }
            else if (vals.size() > mrows) {
                col.mask.adjustSize(mrows, mrows);
                vals.resize(mrows);
            }
            break;}
        case ibis::UBYTE: {
            array_t<unsigned char>& vals =
                *static_cast<array_t<unsigned char>*>(col.values);
            if (vals.size() < mrows) {
                if (col.defval != 0) {
                    col.mask.adjustSize(mrows, mrows);
                    vals.insert(vals.end(), mrows-vals.size(),
                                *static_cast<unsigned char*>(col.defval));
                }
                else {
                    col.mask.adjustSize(vals.size(), mrows);
                    vals.insert(vals.end(), mrows-vals.size(), 0xFFU);
                }
            }
            else if (vals.size() > mrows) {
                col.mask.adjustSize(mrows, mrows);
                vals.resize(mrows);
            }
            break;}
        case ibis::SHORT: {
            array_t<int16_t>& vals =
                *static_cast<array_t<int16_t>*>(col.values);
            if (vals.size() < mrows) {
                if (col.defval != 0) {
                    col.mask.adjustSize(mrows, mrows);
                    vals.insert(vals.end(), mrows-vals.size(),
                                *static_cast<int16_t*>(col.defval));
                }
                else {
                    col.mask.adjustSize(vals.size(), mrows);
                    vals.insert(vals.end(), mrows-vals.size(), 0x7FFF);
                }
            }
            else if (vals.size() > mrows) {
                col.mask.adjustSize(mrows, mrows);
                vals.resize(mrows);
            }
            break;}
        case ibis::USHORT: {
            array_t<uint16_t>& vals =
                *static_cast<array_t<uint16_t>*>(col.values);
            if (vals.size() < mrows) {
                if (col.defval != 0) {
                    col.mask.adjustSize(mrows, mrows);
                    vals.insert(vals.end(), mrows-vals.size(),
                                *static_cast<uint16_t*>(col.defval));
                }
                else {
                    col.mask.adjustSize(vals.size(), mrows);
                    vals.insert(vals.end(), mrows-vals.size(), 0xFFFFU);
                }
            }
            else if (vals.size() > mrows) {
                col.mask.adjustSize(mrows, mrows);
                vals.resize(mrows);
            }
            break;}
        case ibis::INT: {
            array_t<int32_t>& vals =
                *static_cast<array_t<int32_t>*>(col.values);
            if (vals.size() < mrows) {
                if (col.defval != 0) {
                    col.mask.adjustSize(mrows, mrows);
                    vals.insert(vals.end(), mrows-vals.size(),
                                *static_cast<int32_t*>(col.defval));
                }
                else {
                    col.mask.adjustSize(vals.size(), mrows);
                    vals.insert(vals.end(), mrows-vals.size(), 0x7FFFFFFF);
                }
            }
            else if (vals.size() > mrows) {
                col.mask.adjustSize(mrows, mrows);
                vals.resize(mrows);
            }
            break;}
        case ibis::UINT: {
            array_t<uint32_t>& vals =
                *static_cast<array_t<uint32_t>*>(col.values);
            if (vals.size() < mrows) {
                if (col.defval != 0) {
                    col.mask.adjustSize(mrows, mrows);
                    vals.insert(vals.end(), mrows-vals.size(),
                                *static_cast<uint32_t*>(col.defval));
                }
                else {
                    col.mask.adjustSize(vals.size(), mrows);
                    vals.insert(vals.end(), mrows-vals.size(), 0xFFFFFFFFU);
                }
            }
            else if (vals.size() > mrows) {
                col.mask.adjustSize(mrows, mrows);
                vals.resize(mrows);
            }
            break;}
        case ibis::LONG: {
            array_t<int64_t>& vals =
                *static_cast<array_t<int64_t>*>(col.values);
            if (vals.size() < mrows) {
                if (col.defval != 0) {
                    col.mask.set(0, mrows);
                    vals.insert(vals.end(), mrows-vals.size(),
                                *static_cast<int64_t*>(col.defval));
                }
                else {
                    col.mask.adjustSize(vals.size(), mrows);
                    vals.insert(vals.end(), mrows-vals.size(),
                                0x7FFFFFFFFFFFFFFFLL);
                }
            }
            else if (vals.size() > mrows) {
                col.mask.adjustSize(mrows, mrows);
                vals.resize(mrows);
            }
            break;}
        case ibis::OID:
        case ibis::ULONG: {
            array_t<uint64_t>& vals =
                *static_cast<array_t<uint64_t>*>(col.values);
            if (vals.size() < mrows) {
                if (col.defval != 0) {
                    col.mask.adjustSize(mrows, mrows);
                    vals.insert(vals.end(), mrows-vals.size(),
                                *static_cast<uint64_t*>(col.defval));
                }
                else {
                    col.mask.adjustSize(vals.size(), mrows);
                    vals.insert(vals.end(), mrows-vals.size(),
                                0xFFFFFFFFFFFFFFFFULL);
                }
            }
            else if (vals.size() > mrows) {
                col.mask.adjustSize(mrows, mrows);
                vals.resize(mrows);
            }
            break;}
        case ibis::FLOAT: {
            array_t<float>& vals =
                *static_cast<array_t<float>*>(col.values);
            if (vals.size() < mrows) {
                if (col.defval != 0) {
                    col.mask.adjustSize(mrows, mrows);
                    vals.insert(vals.end(), mrows-vals.size(),
                                *static_cast<float*>(col.defval));
                }
                else {
                    col.mask.adjustSize(vals.size(), mrows);
                    vals.insert(vals.end(), mrows-vals.size(),
                                FASTBIT_FLOAT_NULL);
                }
            }
            else if (vals.size() > mrows) {
                col.mask.adjustSize(mrows, mrows);
                vals.resize(mrows);
            }
            break;}
        case ibis::DOUBLE: {
            array_t<double>& vals =
                *static_cast<array_t<double>*>(col.values);
            if (vals.size() < mrows) {
                if (col.defval != 0) {
                    col.mask.adjustSize(mrows, mrows);
                    vals.insert(vals.end(), mrows-vals.size(),
                                *static_cast<double*>(col.defval));
                }
                else {
                    col.mask.adjustSize(vals.size(), mrows);
                    vals.insert(vals.end(), mrows-vals.size(),
                                FASTBIT_DOUBLE_NULL);
                }
            }
            else if (vals.size() > mrows) {
                col.mask.adjustSize(mrows, mrows);
                vals.resize(mrows);
            }
            break;}
        case ibis::TEXT:
        case ibis::CATEGORY: {
            std::vector<std::string>& vals =
                *static_cast<std::vector<std::string>*>(col.values);
            if (vals.size() < mrows) {
                if (col.defval != 0) {
                    col.mask.adjustSize(mrows, mrows);
                    vals.insert(vals.end(), mrows-vals.size(),
                                *static_cast<std::string*>(col.defval));
                }
                else {
                    col.mask.adjustSize(vals.size(), mrows);
                    vals.insert(vals.end(), mrows-vals.size(), "");
                }
            }
            else if (vals.size() > mrows) {
                col.mask.adjustSize(mrows, mrows);
                vals.resize(mrows);
            }
            break;}
        case ibis::BLOB: {
            std::vector<ibis::opaque>& vals =
                *static_cast<std::vector<ibis::opaque>*>(col.values);
            if (vals.size() < mrows) {
                vals.reserve(mrows);
                col.mask.adjustSize(vals.size(), mrows);
                if (col.defval != 0) {
                    const ibis::opaque &def =
                        *static_cast<ibis::opaque*>(col.defval);
                    while (vals.size() < mrows) {
                        vals.push_back(def);
                    }
                }
                else {
                    vals.resize(mrows);
                }
            }
            else if (vals.size() > mrows) {
                vals.resize(mrows);
                col.mask.adjustSize(mrows, mrows);
            }
            break;}
        default: {
            break;}
        }
    }
} // ibis::tafel::normalize

/// Locate the buffers and masks associated with a data type.
template <typename T>
void ibis::tafel::locate(ibis::TYPE_T t, std::vector<array_t<T>*>& buf,
                         std::vector<ibis::bitvector*>& msk) const {
    buf.clear();
    msk.clear();
    for (uint32_t i = 0; i < colorder.size(); ++ i) {
        if (colorder[i]->type == t) {
            buf.push_back(static_cast<array_t<T>*>(colorder[i]->values));
            msk.push_back(&(colorder[i]->mask));
        }
    }
} // ibis::tafel::locate

/// Append one row to columns of a particular type.  This version with
/// multiple columns but only one row.
///
/// @note It assumes that the existing data have been normalized, i.e., all
/// columns have the same number of rows.
template <typename T>
void ibis::tafel::append(const std::vector<std::string>& nm,
                         const std::vector<T>& va,
                         std::vector<array_t<T>*>& buf,
                         std::vector<ibis::bitvector*>& msk) {
    const uint32_t n1 = (nm.size() <= va.size() ? nm.size() : va.size());
    for (uint32_t i = 0; i < n1; ++ i) {
        if (nm[i].empty()) {
            if (buf.size() > i && buf[i] != 0)
                buf[i]->push_back(va[i]);
            if (msk.size() > i && msk[i] != 0)
                msk[i]->operator+=(1);
        }
        else {
            columnList::iterator it = cols.find(nm[i].c_str());
            if (it != cols.end()) {
                if (buf.size() < i) buf.resize(i+1);
                buf[i] = static_cast<array_t<T>*>((*it).second->values);
                buf[i]->push_back(va[i]);
                if (msk.size() < i) msk.resize(i+1);
                msk[i] = &(it->second->mask);
                *(msk[i]) += 1;
            }
        }
    }

    const uint32_t n2 = (va.size() <= buf.size() ? va.size() : buf.size());    
    for (uint32_t i = n1; i < n2; ++ i) {
        if (buf[i] != 0)
            buf[i]->push_back(va[i]);
        if (msk.size() > i && msk[i] != 0)
            *(msk[i]) += 1;
    }
} // ibis::tafel::append

/// Locate the buffers and masks associated with a string-valued data type.
void ibis::tafel::locateString(ibis::TYPE_T t,
                               std::vector<std::vector<std::string>*>& buf,
                               std::vector<ibis::bitvector*>& msk) const {
    buf.clear();
    msk.clear();
    for (uint32_t i = 0; i < colorder.size(); ++ i) {
        if (colorder[i]->type == t) {
            buf.push_back(static_cast<std::vector<std::string>*>
                          (colorder[i]->values));
            msk.push_back(&(colorder[i]->mask));
        }
    }
} // ibis::tafel::locateString

/// Append one row to string-valued columns.  This version with multiple
/// columns but only one row.
///
/// @note It assumes that the existing data have been normalized, i.e., all
/// columns have the same number of rows.
void ibis::tafel::appendString(const std::vector<std::string>& nm,
                               const std::vector<std::string>& va,
                               std::vector<std::vector<std::string>*>& buf,
                               std::vector<ibis::bitvector*>& msk) {
    const uint32_t n1 = (nm.size() <= va.size() ? nm.size() : va.size());
    for (uint32_t i = 0; i < n1; ++ i) {
        if (nm[i].empty()) {
            if (buf.size() > i && buf[i] != 0)
                buf[i]->push_back(va[i]);
            if (msk.size() > i && msk[i] != 0)
                *(msk[i]) += 1;
        }
        else {
            columnList::iterator it = cols.find(nm[i].c_str());
            if (it != cols.end()) {
                if (buf.size() < i) buf.resize(i+1);
                buf[i] = static_cast<std::vector<std::string>*>
                    ((*it).second->values);
                buf[i]->push_back(va[i]);
                msk[i] = &(it->second->mask);
                *(msk[i]) += 1;
            }
        }
    }

    const uint32_t n2 = (va.size() <= buf.size() ? va.size() : buf.size());    
    for (uint32_t i = n1; i < n2; ++ i) {
        if (buf[i] != 0)
            buf[i]->push_back(va[i]);
        if (msk.size() > i && msk[i] != 0)
            *(msk[i]) += 1;
    }
} // ibis::tafel::appendString

/// Locate the buffers and masks associated with a string-valued data type.
void ibis::tafel::locateBlob(std::vector<std::vector<ibis::opaque>*>& buf,
                             std::vector<ibis::bitvector*>& msk) const {
    buf.clear();
    msk.clear();
    for (uint32_t i = 0; i < colorder.size(); ++ i) {
        if (colorder[i]->type == ibis::BLOB) {
            buf.push_back(static_cast<std::vector<ibis::opaque>*>
                          (colorder[i]->values));
            msk.push_back(&(colorder[i]->mask));
        }
    }
} // ibis::tafel::locateBlob

/// Append one row to blob columns.  This version with multiple
/// columns but only one row.
///
/// @note It assumes that the existing data have been normalized, i.e., all
/// columns have the same number of rows.
void ibis::tafel::appendBlob(const std::vector<std::string>& nm,
                             const std::vector<ibis::opaque>& va,
                             std::vector<std::vector<ibis::opaque>*>& buf,
                             std::vector<ibis::bitvector*>& msk) {
    const uint32_t n1 = (nm.size() <= va.size() ? nm.size() : va.size());
    for (uint32_t i = 0; i < n1; ++ i) {
        if (nm[i].empty()) {
            if (buf.size() > i && buf[i] != 0)
                buf[i]->push_back(va[i]);
            if (msk.size() > i && msk[i] != 0)
                *(msk[i]) += 1;
        }
        else {
            columnList::iterator it = cols.find(nm[i].c_str());
            if (it != cols.end()) {
                if (buf.size() < i) buf.resize(i+1);
                buf[i] = static_cast<std::vector<ibis::opaque>*>
                    ((*it).second->values);
                buf[i]->push_back(va[i]);
                msk[i] = &(it->second->mask);
                *(msk[i]) += 1;
            }
        }
    }

    const uint32_t n2 = (va.size() <= buf.size() ? va.size() : buf.size());    
    for (uint32_t i = n1; i < n2; ++ i) {
        if (buf[i] != 0)
            buf[i]->push_back(va[i]);
        if (msk.size() > i && msk[i] != 0)
            *(msk[i]) += 1;
    }
} // ibis::tafel::appendBlob

int ibis::tafel::appendRow(const ibis::table::row& r) {
    int cnt = 0;
    if (r.nColumns() >= cols.size())
        normalize();

    std::vector<ibis::bitvector*> msk;
    if (r.bytesvalues.size() > 0) {
        std::vector<array_t<signed char>*> bytesptr;
        locate(ibis::BYTE, bytesptr, msk);
        cnt += r.bytesvalues.size();
        append(r.bytesnames, r.bytesvalues, bytesptr, msk);
    }
    if (r.ubytesvalues.size() > 0) {
        std::vector<array_t<unsigned char>*> ubytesptr;
        locate(ibis::UBYTE, ubytesptr, msk);
        cnt += r.ubytesvalues.size();
        append(r.ubytesnames, r.ubytesvalues, ubytesptr, msk);
    }
    if (r.shortsvalues.size() > 0) {
        std::vector<array_t<int16_t>*> shortsptr;
        locate(ibis::SHORT, shortsptr, msk);
        cnt += r.shortsvalues.size();
        append(r.shortsnames, r.shortsvalues, shortsptr, msk);
    }
    if (r.ushortsvalues.size() > 0) {
        std::vector<array_t<uint16_t>*> ushortsptr;
        locate(ibis::USHORT, ushortsptr, msk);
        cnt += r.ushortsvalues.size();
        append(r.ushortsnames, r.ushortsvalues, ushortsptr, msk);
    }
    if (r.intsvalues.size() > 0) {
        std::vector<array_t<int32_t>*> intsptr;
        locate(ibis::INT, intsptr, msk);
        cnt += r.intsvalues.size();
        append(r.intsnames, r.intsvalues, intsptr, msk);
    }
    if (r.uintsvalues.size() > 0) {
        std::vector<array_t<uint32_t>*> uintsptr;
        locate(ibis::UINT, uintsptr, msk);
        cnt += r.uintsvalues.size();
        append(r.uintsnames, r.uintsvalues, uintsptr, msk);
    }
    if (r.longsvalues.size() > 0) {
        std::vector<array_t<int64_t>*> longsptr;
        locate(ibis::LONG, longsptr, msk);
        cnt += r.longsvalues.size();
        append(r.longsnames, r.longsvalues, longsptr, msk);
    }
    if (r.ulongsvalues.size() > 0) {
        std::vector<array_t<uint64_t>*> ulongsptr;
        locate(ibis::ULONG, ulongsptr, msk);
        cnt += r.ulongsvalues.size();
        append(r.ulongsnames, r.ulongsvalues, ulongsptr, msk);
    }
    if (r.floatsvalues.size() > 0) {
        std::vector<array_t<float>*> floatsptr;
        locate(ibis::FLOAT, floatsptr, msk);
        cnt += r.floatsvalues.size();
        append(r.floatsnames, r.floatsvalues, floatsptr, msk);
    }
    if (r.doublesvalues.size() > 0) {
        std::vector<array_t<double>*> doublesptr;
        locate(ibis::DOUBLE, doublesptr, msk);
        cnt += r.doublesvalues.size();
        append(r.doublesnames, r.doublesvalues, doublesptr, msk);
    }
    if (r.catsvalues.size() > 0) {
        std::vector<std::vector<std::string>*> catsptr;
        locateString(ibis::CATEGORY, catsptr, msk);
        cnt += r.catsvalues.size();
        appendString(r.catsnames, r.catsvalues, catsptr, msk);
    }
    if (r.textsvalues.size() > 0) {
        std::vector<std::vector<std::string>*> textsptr;
        locateString(ibis::TEXT, textsptr, msk);
        cnt += r.textsvalues.size();
        appendString(r.textsnames, r.textsvalues, textsptr, msk);
    }
    if (r.blobsvalues.size() > 0) {
        std::vector<std::vector<ibis::opaque>*> blobsptr;
        locateBlob(blobsptr, msk);
        cnt += r.blobsvalues.size();
        appendBlob(r.blobsnames, r.blobsvalues, blobsptr, msk);
    }
    mrows += ((size_t)cnt >= cols.size());
    return cnt;
} // ibis::tafel::appendRow

int ibis::tafel::appendRows(const std::vector<ibis::table::row>& rs) {
    if (rs.empty()) return 0;
    std::vector<ibis::bitvector*> bytesmsk;
    std::vector<array_t<signed char>*> bytesptr;
    locate(ibis::BYTE, bytesptr, bytesmsk);
    std::vector<ibis::bitvector*> ubytesmsk;
    std::vector<array_t<unsigned char>*> ubytesptr;
    locate(ibis::UBYTE, ubytesptr, ubytesmsk);
    std::vector<ibis::bitvector*> shortsmsk;
    std::vector<array_t<int16_t>*> shortsptr;
    locate(ibis::SHORT, shortsptr, shortsmsk);
    std::vector<ibis::bitvector*> ushortsmsk;
    std::vector<array_t<uint16_t>*> ushortsptr;
    locate(ibis::USHORT, ushortsptr, ushortsmsk);
    std::vector<ibis::bitvector*> intsmsk;
    std::vector<array_t<int32_t>*> intsptr;
    locate(ibis::INT, intsptr, intsmsk);
    std::vector<ibis::bitvector*> uintsmsk;
    std::vector<array_t<uint32_t>*> uintsptr;
    locate(ibis::UINT, uintsptr, uintsmsk);
    std::vector<ibis::bitvector*> longsmsk;
    std::vector<array_t<int64_t>*> longsptr;
    locate(ibis::LONG, longsptr, longsmsk);
    std::vector<ibis::bitvector*> ulongsmsk;
    std::vector<array_t<uint64_t>*> ulongsptr;
    locate(ibis::ULONG, ulongsptr, ulongsmsk);
    std::vector<ibis::bitvector*> floatsmsk;
    std::vector<array_t<float>*> floatsptr;
    locate(ibis::FLOAT, floatsptr, floatsmsk);
    std::vector<ibis::bitvector*> doublesmsk;
    std::vector<array_t<double>*> doublesptr;
    locate(ibis::DOUBLE, doublesptr, doublesmsk);
    std::vector<ibis::bitvector*> catsmsk;
    std::vector<std::vector<std::string>*> catsptr;
    locateString(ibis::CATEGORY, catsptr, catsmsk);
    std::vector<ibis::bitvector*> textsmsk;
    std::vector<std::vector<std::string>*> textsptr;
    locateString(ibis::TEXT, textsptr, textsmsk);
    std::vector<ibis::bitvector*> blobsmsk;
    std::vector<std::vector<ibis::opaque>*> blobsptr;
    locateBlob(blobsptr, blobsmsk);

    const uint32_t ncols = cols.size();
    uint32_t cnt = 0;
    int jnew = 0;
    for (uint32_t i = 0; i < rs.size(); ++ i) {
        if (cnt < ncols)
            normalize();

        cnt = 0;
        if (rs[i].bytesvalues.size() > 0) {
            cnt += rs[i].bytesvalues.size();
            append(rs[i].bytesnames, rs[i].bytesvalues, bytesptr, bytesmsk);
        }
        if (rs[i].ubytesvalues.size() > 0) {
            cnt += rs[i].ubytesvalues.size();
            append(rs[i].ubytesnames, rs[i].ubytesvalues, ubytesptr, ubytesmsk);
        }
        if (rs[i].shortsvalues.size() > 0) {
            cnt += rs[i].shortsvalues.size();
            append(rs[i].shortsnames, rs[i].shortsvalues, shortsptr, shortsmsk);
        }
        if (rs[i].ushortsvalues.size() > 0) {
            cnt += rs[i].ushortsvalues.size();
            append(rs[i].ushortsnames, rs[i].ushortsvalues, ushortsptr,
                   ushortsmsk);
        }
        if (rs[i].intsvalues.size() > 0) {
            cnt += rs[i].intsvalues.size();
            append(rs[i].intsnames, rs[i].intsvalues, intsptr, intsmsk);
        }
        if (rs[i].uintsvalues.size() > 0) {
            cnt += rs[i].uintsvalues.size();
            append(rs[i].uintsnames, rs[i].uintsvalues, uintsptr, uintsmsk);
        }
        if (rs[i].longsvalues.size() > 0) {
            cnt += rs[i].longsvalues.size();
            append(rs[i].longsnames, rs[i].longsvalues, longsptr, longsmsk);
        }
        if (rs[i].ulongsvalues.size() > 0) {
            cnt += rs[i].ulongsvalues.size();
            append(rs[i].ulongsnames, rs[i].ulongsvalues, ulongsptr, ulongsmsk);
        }
        if (rs[i].floatsvalues.size() > 0) {
            cnt += rs[i].floatsvalues.size();
            append(rs[i].floatsnames, rs[i].floatsvalues, floatsptr, floatsmsk);
        }
        if (rs[i].doublesvalues.size() > 0) {
            cnt += rs[i].doublesvalues.size();
            append(rs[i].doublesnames, rs[i].doublesvalues, doublesptr,
                   doublesmsk);
        }
        if (rs[i].catsvalues.size() > 0) {
            cnt += rs[i].catsvalues.size();
            appendString(rs[i].catsnames, rs[i].catsvalues, catsptr, catsmsk);
        }
        if (rs[i].textsvalues.size() > 0) {
            cnt += rs[i].textsvalues.size();
            appendString(rs[i].textsnames, rs[i].textsvalues, textsptr,
                         textsmsk);
        }
        if (rs[i].blobsvalues.size() > 0) {
            cnt += rs[i].blobsvalues.size();
            appendBlob(rs[i].blobsnames, rs[i].blobsvalues, blobsptr,
                       blobsmsk);
        }
        if (cnt > 0) {
            ++ mrows;
            ++ jnew;
        }
        
    }
    return jnew;
} // ibis::tafel::appendRows

/// Write the metadata file if no metadata file already exists in the given
/// directory.
/// Return error code:
/// - number of columns: successful completion.  The return value of this
///   function should match the return of mColumns.
/// -  0: a metadata file already exists.  The content of the existing
///    metadata file is not checked.
/// - -1: no directory specified.
/// - -3: unable to open the metadata file.
int ibis::tafel::writeMetaData(const char* dir, const char* tname,
                               const char* tdesc, const char* idx,
                               const char* nvpairs) const {
    if (cols.empty()) return 0; // nothing new to write
    if (dir == 0 || *dir == 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- tafel::writeMetaData needs a valid output directory";
        return -1; // dir must be specified
    }
    std::string mdfile = dir;
    mdfile += FASTBIT_DIRSEP;
    mdfile += "-part.txt";
    if (ibis::util::getFileSize(mdfile.c_str()) > 0) {
        LOGGER(ibis::gVerbose > 1)
            << "tafel::writeMetaData detects an existing -part.txt in " << dir
            << ", return now";
        return 0;
    }
    int ierr;
    ibis::horometer timer;
    if (ibis::gVerbose > 0)
        timer.start();

    uint64_t nr = 0, nb;
    std::string nmlocal, desclocal;
    for (columnList::const_iterator it = cols.begin(); it != cols.end();
         ++ it) {
        // examine the data files to determine the number of rows
        const column &col = *(it->second);
        nmlocal = dir;
        nmlocal += '/';
        nmlocal += col.name;
        nb = ibis::util::getFileSize(nmlocal.c_str());
        switch (col.type) {
        default:
            break;
        case ibis::BYTE:
        case ibis::UBYTE:
            if (nb > nr)
                nr = nb;
            break;
        case ibis::SHORT:
        case ibis::USHORT:
            nb /= 2;
            if (nb > nr)
                nr = nb;
            break;
        case ibis::INT:
        case ibis::UINT:
        case ibis::FLOAT:
            nb /= 4;
            if (nb > nr)
                nr = nb;
            break;
        case ibis::LONG:
        case ibis::ULONG:
        case ibis::DOUBLE:
            nb /= 8;
            if (nb > nr)
                nr = nb;
            break;
        }
    }

    time_t currtime = time(0); // current time
    char stamp[28];
    ibis::util::secondsToString(currtime, stamp);
    if (tdesc == 0 || *tdesc == 0) { // generate a description
        std::ostringstream oss;
        oss << "Metadata written with ibis::tafel::writeMetaData on "
            << stamp << " with " << cols.size() << " column"
            << (cols.size() > 1 ? "s" : "");
        desclocal = oss.str();
        tdesc = desclocal.c_str();
    }
    if (tname == 0 || *tname == 0) { // use the directory name as table name
        tname = strrchr(dir, FASTBIT_DIRSEP);
        if (tname == 0)
            tname = strrchr(dir, '/');
        if (tname != 0) {
            if (tname[1] != 0) {
                ++ tname;
            }
            else { // dir ends with FASTBIT_DIRSEP
                nmlocal = dir;
                // remove the last FASTBIT_DIRSEP
                nmlocal.erase(nmlocal.size()-1);
                uint32_t j = 1 + nmlocal.rfind(FASTBIT_DIRSEP);
                if (j > nmlocal.size())
                    j = 1 + nmlocal.rfind('/');
                if (j < nmlocal.size())
                    nmlocal.erase(0, j);
                if (! nmlocal.empty())
                    tname = nmlocal.c_str();
                else
                    tname = 0;
            }
        }
        else if (tname == 0 && *dir != '.') { // no directory separator
            tname = dir;
        }
        if (tname == 0) {
            uint32_t sum = ibis::util::checksum(tdesc, std::strlen(tdesc));
            ibis::util::int2string(nmlocal, sum);
            if (! isalpha(nmlocal[0]))
                nmlocal[0] = 'A' + (nmlocal[0] % 26);
        }
    }
    LOGGER(ibis::gVerbose > 1)
        << "tafel::writeMetaData starting to write " << cols.size()
        << " column" << (cols.size()>1?"s":"") << " to " << dir << " as "
        << " data partition " << tname;

    ibis::fileManager::instance().flushDir(dir);
    std::ofstream md(mdfile.c_str());
    if (! md) {
        LOGGER(ibis::gVerbose > 0)
            << "tafel::writeMetaData(" << dir
            << ") failed to open metadata file \"-part.txt\"";
        return -3; // metadata file not ready
    }

    md << "# meta data for data partition " << tname
       << " written by ibis::tafel::writeMetaData on " << stamp << "\n\n"
       << "BEGIN HEADER\nName = " << tname
       << "\nDescription = " << tdesc
       << "\nNumber_of_rows = " << nr
       << "\nNumber_of_columns = " << cols.size()
       << "\nTimestamp = " << currtime;
    if (idx != 0 && *idx != 0) {
        md << "\nindex = " << idx;
    }
    else { // try to find the default index specification
        std::string idxkey = "ibis.";
        idxkey += tname;
        idxkey += ".index";
        const char* str = ibis::gParameters()[idxkey.c_str()];
        if (str != 0 && *str != 0)
            md << "\nindex = " << str;
    }
    if (nvpairs != 0 && *nvpairs != 0) {
        md << "\nmetaTags = " << nvpairs;
    }
    md << "\nEND HEADER\n";

    if (colorder.size() == cols.size()) { // in input order
        for (size_t j = 0; j < cols.size(); ++ j) {
            const column& col = *(colorder[j]);
            md << "\nBegin Column\nname = " << col.name << "\ndata_type = "
               << ibis::TYPESTRING[(int) col.type];
            if (!col.desc.empty())
                md << "\ndescription = " << col.desc;
            if (! col.indexSpec.empty()) {
                md << "\nindex = " << col.indexSpec;
            }
            else {
                std::string idxkey = "ibis.";
                idxkey += tname;
                idxkey += ".";
                idxkey += col.name;
                idxkey += ".index";
                const char* str = ibis::gParameters()[idxkey.c_str()];
                if (str != 0)
                    md << "\nindex = " << str;
            }
            md << "\nEnd Column\n";
            if (! col.dictfile.empty()) {
                // read the ASCII dictionary, then write it out in binary
                ibis::dictionary tmp;
                std::ifstream dfile(col.dictfile.c_str());
                if (! dfile) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- tafel::writeMetaData failed to open \""
                        << col.dictfile << '"';
                    continue;
                }
                ierr = tmp.fromASCII(dfile);
                dfile.close();
                if (ierr < 0) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- tafel::writeMetaData failed to read the "
                        "content of user supplied ASCII dictionary file \""
                        << col.dictfile << '"';
                    continue;
                }
                else {
                    LOGGER(ibis::gVerbose > 2)
                        << "tafel::writeMetaData read " << tmp.size()
                        << " dictionary entries from " << col.dictfile
                        << " for column " << col.name;
                }

                // successfully read the ASCII dictionary
                std::string dictname = dir;
                dictname += FASTBIT_DIRSEP;
                dictname += col.name;
                dictname += ".dic";
                ierr = tmp.write(dictname.c_str());
                LOGGER(ierr < 0 && ibis::gVerbose > 0)
                    << "Warning -- tafel::writeMetaData failed to write the "
                    "content of \"" << col.dictfile
                    << "\" in the binary format to \"" << dictname << '"';
            }
        }
    }
    else { // write columns in alphabetic order
        for (columnList::const_iterator it = cols.begin();
             it != cols.end(); ++ it) {
            const column& col = *((*it).second);
            md << "\nBegin Column\nname = " << (*it).first << "\ndata_type = "
               << ibis::TYPESTRING[(int) col.type];
            if (!col.desc.empty())
                md << "\ndescription = " << col.desc;
            if (! col.indexSpec.empty()) {
                md << "\nindex = " << col.indexSpec;
            }
            else {
                std::string idxkey = "ibis.";
                idxkey += tname;
                idxkey += ".";
                idxkey += (*it).first;
                idxkey += ".index";
                const char* str = ibis::gParameters()[idxkey.c_str()];
                if (str != 0)
                    md << "\nindex = " << str;
            }
            md << "\nEnd Column\n";
            if (! col.dictfile.empty()) {
                // read the ASCII dictionary, then write it out in binary
                ibis::dictionary tmp;
                std::ifstream dfile(col.dictfile.c_str());
                if (! dfile) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- tafel::writeMetaData failed to open \""
                        << col.dictfile << '"';
                    continue;
                }
                ierr = tmp.fromASCII(dfile);
                dfile.close();
                if (ierr < 0) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- tafel::writeMetaData failed to read the "
                        "content of user supplied ASCII dictionary file \""
                        << col.dictfile << '"';
                    continue;
                }
                else {
                    LOGGER(ibis::gVerbose > 2)
                        << "tafel::writeMetaData read " << tmp.size()
                        << " dictionary entries from " << col.dictfile
                        << " for column " << col.name;
                }

                // successfully read the ASCII dictionary
                std::string dictname = dir;
                dictname += FASTBIT_DIRSEP;
                dictname += col.name;
                dictname += ".dic";
                ierr = tmp.write(dictname.c_str());
                LOGGER(ierr < 0 && ibis::gVerbose > 0)
                    << "Warning -- tafel::writeMetaData failed to write the "
                    "content of \"" << col.dictfile
                    << "\" in the binary format to \"" << dictname << '"';
            }
        }
    }
    md.close(); // close the file
    if (ibis::gVerbose > 0) {
        timer.stop();
        ibis::util::logger()()
            << "tafel::writeMetaData completed writing partition " 
            << tname << " (" << tdesc << ") with " << cols.size()
            << " column" << (cols.size()>1 ? "s" : "") << " to " << dir
            << " using " << timer.CPUTime() << " sec(CPU), "
            << timer.realTime() << " sec(elapsed)";
    }

    return cols.size();
} // ibis::tafel::writeMetaData

/// Write the data values and update the metadata file.
/// Return error code:
/// -  0: successful completion.
/// - -1: no directory specified.
/// - -2: column type conflicts.
/// - -3: unable to open the metadata file.
/// - -4: unable to open a data file.
/// - -5: failed to write the expected number of records.
int ibis::tafel::write(const char* dir, const char* tname,
                       const char* tdesc, const char* idx,
                       const char* nvpairs) const {
    if (cols.empty() || mrows == 0) return 0; // nothing new to write
    if (dir == 0 || *dir == 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- tafel::write needs a valid output directory name";
        return -1; // dir must be specified
    }
    int ierr = 0;
    ibis::horometer timer;
    if (ibis::gVerbose > 0)
        timer.start();
    do {
        int jerr = writeData(dir, tname, tdesc, idx, nvpairs, ierr);
        if (jerr < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- tafel::write failed to write data after "
                "completing " << ierr << " row" << (ierr>1?"s":"");
            ierr = jerr;
            break;
        }
        else {
            ierr += jerr;
            LOGGER(ibis::gVerbose > 1)
                << "tafel::write complete writing " << jerr << " row"
                << (jerr>1?"s":"") << " as partition " << ipart << " in "
                << dir;
            if ((unsigned)ierr < mrows)
                ++ ipart;
        }
    } while ((unsigned)ierr < mrows);
    if (ierr >= (long)mrows && ibis::gVerbose > 0) {
        timer.stop();
        ibis::util::logger()()
            << "tafel::write completed writing partition '" 
            << (tname?tname:"") << "' (" << (tdesc?tdesc:"") << ") with "
            << cols.size() << " column" << (cols.size()>1 ? "s" : "")
            << " and " << mrows << " row" << (mrows>1 ? "s" : "")
            << " to " << (dir?dir:"tmp")
            << " using " << timer.CPUTime() << " sec(CPU), "
            << timer.realTime() << " sec(elapsed)";
    }
    else if (ierr < (long)mrows) {
        LOGGER(ibis::gVerbose > 0)
            << "tafel::write expected to write " << mrows << " row"
            << (mrows>1?"s":"") << ", but only wrote " << ierr;
    }
    return ierr;
} // ibis::tafel::write

int ibis::tafel::writeData(const char* dir, const char* tname,
                           const char* tdesc, const char* idx,
                           const char* nvpairs, uint32_t voffset) const {
    const uint32_t prows =
        (maxpart>0 ? (mrows-voffset>=maxpart ? maxpart : mrows-voffset) :
         mrows);
    if (cols.empty() || prows == 0) return 0; // nothing new to write
    if (dir == 0 || *dir == 0) {
        dir = "tmp";
        LOGGER(ibis::gVerbose >= 0)
            << "tafel::writeData sets the output directory name to be tmp";
    }
    ibis::horometer timer;
    if (ibis::gVerbose > 2)
        timer.start();

    std::string oldnm, olddesc, oldidx, oldtags, dirstr;
    ibis::bitvector::word_t nold = 0;
    const char *mydir = dir;
    bool again = false;
    do { // read the existing meta data
        if (ipart > 0) {
            const bool needdirsep = (FASTBIT_DIRSEP != dir[std::strlen(dir)-1]);
            do {
                std::ostringstream oss;
                oss << dir;
                if (needdirsep)
                    oss << FASTBIT_DIRSEP;
                if (tname != 0 && *tname != 0)
                    oss << tname;
                else
                    oss << '_';
                oss << std::setw(2) << std::setfill('0') << std::hex << ipart
                    << std::dec;
                dirstr = oss.str();
                Stat_T st1;
                if (UnixStat(dirstr.c_str(), &st1) == 0) {
                    if ((st1.st_mode & S_IFDIR) != S_IFDIR) {
                        again = true;
                        ++ ipart;
                        if (ipart == 0) {
                            LOGGER(ibis::gVerbose >= 0)
                                << "Warning -- tafel::writeData failed to "
                                "generate an output directory name in "
                                << dir;
                            return -1;
                        }
                    }
                    else {
                        again = false;
                    }
                }
                else {
                    again = false;
                }
            } while (again);
            mydir = dirstr.c_str();
        }

        ibis::part tmp(mydir, static_cast<const char*>(0));
        nold = static_cast<ibis::bitvector::word_t>(tmp.nRows());
        if (nold > 0 && tmp.nColumns() > 0) {
            if (tname == 0 || *tname == 0) {
                oldnm = tmp.name();
                tname = oldnm.c_str();
            }
            if (tdesc == 0 || *tdesc == 0) {
                olddesc = tmp.description();
                tdesc = olddesc.c_str();
            }

            if (nvpairs == 0 || *nvpairs == 0)
                oldtags = tmp.metaTags();
            if (tmp.indexSpec() != 0 && *(tmp.indexSpec()) != 0)
                oldidx = tmp.indexSpec();
            unsigned nconflicts = 0;
            for (columnList::const_iterator it = cols.begin();
                 it != cols.end(); ++ it) {
                const column& col = *((*it).second);
                const ibis::column* old = tmp.getColumn((*it).first);
                if (old != 0) { // check for conflicting types
                    bool conflict = false;
                    switch (col.type) {
                    default:
                        conflict = (old->type() != col.type); break;
                    case ibis::BYTE:
                    case ibis::UBYTE:
                        conflict = (old->type() != ibis::BYTE &&
                                    old->type() != ibis::UBYTE);
                        break;
                    case ibis::SHORT:
                    case ibis::USHORT:
                        conflict = (old->type() != ibis::SHORT &&
                                    old->type() != ibis::USHORT);
                        break;
                    case ibis::INT:
                    case ibis::UINT:
                        conflict = (old->type() != ibis::INT &&
                                    old->type() != ibis::UINT);
                        break;
                    case ibis::LONG:
                    case ibis::ULONG:
                        conflict = (old->type() != ibis::LONG &&
                                    old->type() != ibis::ULONG);
                        break;
                    }
                    if (conflict) {
                        ++ nconflicts;
                        LOGGER(ibis::gVerbose >= 0)
                            << "Warning -- tafel::writeData(" << mydir
                            << ") column " << (*it).first
                            << " has conflicting types specified, previously "
                            << ibis::TYPESTRING[(int)old->type()]
                            << ", currently "
                            << ibis::TYPESTRING[(int)col.type];
                    }
                }
            }
            if (nconflicts > 0) {
                LOGGER(ibis::gVerbose >= 0)
                    << "tafel::writeData(" << mydir
                    << ") can not proceed because " << nconflicts
                    << " column" << (nconflicts>1 ? "s" : "")
                    << " contains conflicting type specifications, "
                    "will try another name";
                again = true;
                ++ ipart;
            }
            else if (maxpart > 0 && nold >= maxpart) {
                LOGGER(ibis::gVerbose > 1)
                    << "tafel::writeData(" << mydir << ") found " << mydir
                    << " to have reached the specified max, "
                    "will try another name";
                again = true;
                ++ ipart;
            }
            else {
                again = false;
                LOGGER(ibis::gVerbose > 2) 
                    << "tafel::writeData(" << mydir
                    << ") found existing data partition named "
                    << tmp.name() << " with " << tmp.nRows()
                    << " row" << (tmp.nRows()>1 ? "s" : "")
                    << " and " << tmp.nColumns() << " column"
                    << (tmp.nColumns()>1?"s":"")
                    << ", will append " << prows << " new row"
                    << (prows>1 ? "s" : "");
            }
            tmp.emptyCache(); // empty cached content from mydir
        }
    } while (again);
    if (maxpart > 0 && nold >= maxpart) {
        // can not write more entries into this directory
        return 0;
    }

    time_t currtime = time(0); // current time
    char stamp[28];
    ibis::util::secondsToString(currtime, stamp);
    if (tdesc == 0 || *tdesc == 0) { // generate a description
        std::ostringstream oss;
        oss << "Data initially wrote with ibis::tablex interface on "
            << stamp << " with " << cols.size() << " column"
            << (cols.size() > 1 ? "s" : "") << " and " << nold + prows
            << " row" << (nold+prows>1 ? "s" : "");
        olddesc = oss.str();
        tdesc = olddesc.c_str();
    }
    if (tname == 0 || *tname == 0) { // use the directory name as table name
        tname = strrchr(mydir, FASTBIT_DIRSEP);
        if (tname == 0)
            tname = strrchr(mydir, '/');
        if (tname != 0) {
            if (tname[1] != 0) {
                ++ tname;
            }
            else { // mydir ends with FASTBIT_DIRSEP
                oldnm = mydir;
                oldnm.erase(oldnm.size()-1); // remove the last FASTBIT_DIRSEP
                uint32_t j = 1 + oldnm.rfind(FASTBIT_DIRSEP);
                if (j > oldnm.size())
                    j = 1 + oldnm.rfind('/');
                if (j < oldnm.size())
                    oldnm.erase(0, j);
                if (! oldnm.empty())
                    tname = oldnm.c_str();
                else
                    tname = 0;
            }
        }
        else if (tname == 0 && *mydir != '.') { // no directory separator
            tname = mydir;
        }
        if (tname == 0) {
            uint32_t sum = ibis::util::checksum(tdesc, std::strlen(tdesc));
            ibis::util::int2string(oldnm, sum);
            if (! isalpha(oldnm[0]))
                oldnm[0] = 'A' + (oldnm[0] % 26);
        }
    }
    LOGGER(ibis::gVerbose > 1)
        << "tafel::writeData starting to write " << prows << " row"
        << (prows>1?"s":"") << " and " << cols.size() << " column"
        << (cols.size()>1?"s":"") << " to " << mydir << " as data partition "
        << tname;

    std::string mdfile = mydir;
    mdfile += FASTBIT_DIRSEP;
    mdfile += "-part.txt";
    std::ofstream md(mdfile.c_str());
    if (! md) {
        LOGGER(ibis::gVerbose > 0)
            << "tafel::writeData(" << mydir << ") failed to open metadata file "
            "\"-part.txt\"";
        return -3; // metadata file not ready
    }

    md << "# meta data for data partition " << tname
       << " written by ibis::tafel::writeData on " << stamp << "\n\n"
       << "BEGIN HEADER\nName = " << tname << "\nDescription = "
       << tdesc << "\nNumber_of_rows = " << nold+prows
       << "\nNumber_of_columns = " << cols.size()
       << "\nTimestamp = " << currtime;
    if (idx != 0 && *idx != 0) {
        md << "\nindex = " << idx;
    }
    else if (! oldidx.empty()) {
        md << "\nindex = " << oldidx;
    }
    else { // try to find the default index specification
        std::string idxkey = "ibis.";
        idxkey += tname;
        idxkey += ".index";
        const char* str = ibis::gParameters()[idxkey.c_str()];
        if (str != 0 && *str != 0)
            md << "\nindex = " << str;
    }
    if (nvpairs != 0 && *nvpairs != 0) {
        md << "\nmetaTags = " << nvpairs;
    }
    else if (! oldtags.empty()) {
        md << "\nmetaTags = " << oldtags;
    }
    md << "\nEND HEADER\n";

    int ierr = 0;
    int nnew = (maxpart==0 ? mrows :
                nold+mrows <= maxpart ? mrows : maxpart-nold);
    const_cast<tafel*>(this)->normalize();
    for (columnList::const_iterator it = cols.begin();
         it != cols.end(); ++ it) {
        const column& col = *((*it).second);
        std::string cnm = mydir;
        cnm += FASTBIT_DIRSEP;
        cnm += (*it).first;
        int fdes = UnixOpen(cnm.c_str(), OPEN_WRITEADD, OPEN_FILEMODE);
        if (fdes < 0) {
            LOGGER(ibis::gVerbose >= 0)
                << "tafel::writeData(" << mydir << ") failed to open file "
                << cnm << " for writing";
            return -4;
        }
        IBIS_BLOCK_GUARD(UnixClose, fdes);
#if defined(_WIN32) && defined(_MSC_VER)
        (void)_setmode(fdes, _O_BINARY);
#endif
        LOGGER(ibis::gVerbose > 2)
            << "tafel::writeData opened file " << cnm
            << " to write data for column " << (*it).first;
        std::string mskfile = cnm; // mask file name
        mskfile += ".msk";
        ibis::bitvector msk(mskfile.c_str());

        switch (col.type) {
        case ibis::BYTE:
            if (col.defval != 0) {
                ierr = ibis::part::writeColumn
                    (fdes, nold, nnew, voffset,
                     *static_cast<const array_t<signed char>*>(col.values),
                     *static_cast<const signed char*>(col.defval),
                     msk, col.mask);
            }
            else {
                ierr = ibis::part::writeColumn
                    (fdes, nold, nnew, voffset,
                     *static_cast<const array_t<signed char>*>(col.values),
                     (signed char)0x7F, msk, col.mask);
            }
            break;
        case ibis::UBYTE:
            if (col.defval != 0) {
                ierr = ibis::part::writeColumn
                    (fdes, nold, nnew, voffset,
                     *static_cast<const array_t<unsigned char>*>(col.values),
                     *static_cast<const unsigned char*>(col.defval),
                     msk, col.mask);
            }
            else {
                ierr = ibis::part::writeColumn
                    (fdes, nold, nnew, voffset,
                     *static_cast<const array_t<unsigned char>*>(col.values),
                     (unsigned char)0xFF, msk, col.mask);
            }
            break;
        case ibis::SHORT:
            if (col.defval != 0) {
                ierr = ibis::part::writeColumn
                    (fdes, nold, nnew, voffset,
                     *static_cast<const array_t<int16_t>*>(col.values),
                     *static_cast<const int16_t*>(col.defval), msk, col.mask);
            }
            else {
                ierr = ibis::part::writeColumn
                    (fdes, nold, nnew, voffset,
                     *static_cast<const array_t<int16_t>*>(col.values),
                     (int16_t)0x7FFF, msk, col.mask);
            }
            break;
        case ibis::USHORT:
            if (col.defval != 0) {
                ierr = ibis::part::writeColumn
                    (fdes, nold, nnew, voffset,
                     *static_cast<const array_t<uint16_t>*>(col.values),
                     *static_cast<const uint16_t*>(col.defval), msk, col.mask);
            }
            else {
                ierr = ibis::part::writeColumn
                    (fdes, nold, nnew, voffset,
                     *static_cast<const array_t<uint16_t>*>(col.values),
                     (uint16_t)0xFFFF, msk, col.mask);
            }
            break;
        case ibis::INT:
            if (col.defval != 0) {
                ierr = ibis::part::writeColumn
                    (fdes, nold, nnew, voffset,
                     *static_cast<const array_t<int32_t>*>(col.values),
                     *static_cast<const int32_t*>(col.defval), msk, col.mask);
            }
            else {
                ierr = ibis::part::writeColumn
                    (fdes, nold, nnew, voffset,
                     *static_cast<const array_t<int32_t>*>(col.values),
                     (int32_t)0x7FFFFFFF, msk, col.mask);
            }
            break;
        case ibis::UINT:
            if (col.defval != 0) {
                ierr = ibis::part::writeColumn
                    (fdes, nold, nnew, voffset,
                     *static_cast<const array_t<uint32_t>*>(col.values),
                     *static_cast<const uint32_t*>(col.defval), msk, col.mask);
            }
            else {
                ierr = ibis::part::writeColumn
                    (fdes, nold, nnew, voffset,
                     *static_cast<const array_t<uint32_t>*>(col.values),
                     (uint32_t)0xFFFFFFFF, msk, col.mask);
            }
            break;
        case ibis::LONG:
            if (col.defval != 0) {
                ierr = ibis::part::writeColumn<int64_t>
                    (fdes, nold, nnew, voffset,
                     *static_cast<const array_t<int64_t>*>(col.values),
                     *static_cast<const int64_t*>(col.defval), msk, col.mask);
            }
            else {
                ierr = ibis::part::writeColumn<int64_t>
                    (fdes, nold, nnew, voffset,
                     *static_cast<const array_t<int64_t>*>(col.values),
                     0x7FFFFFFFFFFFFFFFLL, msk, col.mask);
            }
            break;
        case ibis::OID:
        case ibis::ULONG:
            if (col.defval != 0) {
                ierr = ibis::part::writeColumn<uint64_t>
                    (fdes, nold, nnew, voffset,
                     *static_cast<const array_t<uint64_t>*>(col.values),
                     *static_cast<const uint64_t*>(col.defval), msk, col.mask);
            }
            else {
                ierr = ibis::part::writeColumn<uint64_t>
                    (fdes, nold, nnew, voffset,
                     *static_cast<const array_t<uint64_t>*>(col.values),
                     0xFFFFFFFFFFFFFFFFULL, msk, col.mask);
            }
            break;
        case ibis::FLOAT:
            if (col.defval != 0) {
                ierr = ibis::part::writeColumn
                    (fdes, nold, nnew, voffset,
                     *static_cast<const array_t<float>*>(col.values),
                     *static_cast<const float*>(col.defval), msk, col.mask);
            }
            else {
                ierr = ibis::part::writeColumn
                    (fdes, nold, nnew, voffset,
                     *static_cast<const array_t<float>*>(col.values),
                     FASTBIT_FLOAT_NULL, msk, col.mask);
            }
            break;
        case ibis::DOUBLE:
            if (col.defval != 0) {
                ierr = ibis::part::writeColumn
                    (fdes, nold, nnew, voffset,
                     *static_cast<const array_t<double>*>(col.values), 
                     *static_cast<const double*>(col.defval), msk, col.mask);
            }
            else {
                ierr = ibis::part::writeColumn
                    (fdes, nold, nnew, voffset,
                     *static_cast<const array_t<double>*>(col.values), 
                     FASTBIT_DOUBLE_NULL, msk, col.mask);
            }
            break;
        case ibis::TEXT:
        case ibis::CATEGORY:
            if (col.defval != 0) {
                ierr = ibis::part::writeStrings
                    (cnm.c_str(), nold, nnew, voffset,
                     *static_cast<const std::vector<std::string>*>
                     (col.values), msk, col.mask);
            }
            else {
                ierr = ibis::part::writeStrings
                    (cnm.c_str(), nold, nnew, voffset,
                     *static_cast<const std::vector<std::string>*>
                     (col.values), msk, col.mask);
            }
            break;
        case ibis::BLOB: {
            std::string spname = cnm;
            spname += ".sp";
            int sdes = UnixOpen(spname.c_str(), OPEN_READWRITE, OPEN_FILEMODE);
            if (sdes < 0) {
                LOGGER(ibis::gVerbose >= 0)
                    << "tafel::writeData(" << mydir << ") failed to open file "
                    << spname << " for writing the starting positions";
                return -4;
            }
            IBIS_BLOCK_GUARD(UnixClose, sdes);
#if defined(_WIN32) && defined(_MSC_VER)
            (void)_setmode(sdes, _O_BINARY);
#endif

            ierr = ibis::part::writeOpaques
                (fdes, sdes, nold, nnew, voffset,
                 *static_cast<const std::vector<ibis::opaque>*>(col.values),
                 msk, col.mask);
            break;}
        default:
            break;
        }
#if defined(FASTBIT_SYNC_WRITE)
#if _POSIX_FSYNC+0 > 0
        (void) UnixFlush(fdes); // write to disk
#elif defined(_WIN32) && defined(_MSC_VER)
        (void) _commit(fdes);
#endif
#endif
        if (ierr < 0) {
            LOGGER(ibis::gVerbose > 0)
                << "tafel::writeData(" << mydir << ") failed to write column "
                << (*it).first << " (type " << ibis::TYPESTRING[(int)col.type]
                << ") to " << cnm;
            return ierr;
        }

        if (msk.size() != nold+nnew) {
            if (col.defval != 0) { // fill default value, not NULL
                msk.adjustSize(nold+nnew, nold+nnew);
            }
            else { // fill with NULL
                msk.adjustSize(0, nold+nnew);
            }
        }
        if (msk.cnt() != msk.size()) {
            msk.write(mskfile.c_str());
        }
        else { // remove the mask file
            remove(mskfile.c_str());
        }

        md << "\nBegin Column\nname = " << (*it).first << "\ndata_type = "
           << ibis::TYPESTRING[(int) col.type];
        if (! col.indexSpec.empty()) {
            md << "\nindex = " << col.indexSpec;
        }
        else if (col.type == ibis::BLOB) {
            md << "\nindex=none";
        }
        else {
            std::string idxkey = "ibis.";
            idxkey += tname;
            idxkey += ".";
            idxkey += (*it).first;
            idxkey += ".index";
            const char* str = ibis::gParameters()[idxkey.c_str()];
            if (str != 0)
                md << "\nindex = " << str;
        }
        md << "\nEnd Column\n";
        if (! col.dictfile.empty()) {
            // read the ASCII dictionary, then write it out in binary
            ibis::dictionary tmp;
            std::ifstream dfile(col.dictfile.c_str());
            if (! dfile) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- tafel::writeData failed to open \""
                    << col.dictfile << '"';
                continue;
            }
            ierr = tmp.fromASCII(dfile);
            dfile.close();
            if (ierr < 0) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- tafel::writeData failed to read the "
                    "content of user supplied ASCII dictionary file \""
                    << col.dictfile << '"';
                continue;
            }
            else {
                LOGGER(ibis::gVerbose > 2)
                    << "tafel::writeData read " << tmp.size()
                    << " dictionary entries from " << col.dictfile
                    << " for column " << col.name;
            }

            // successfully read the ASCII dictionary
            std::string dictname = dir;
            dictname += FASTBIT_DIRSEP;
            dictname += col.name;
            dictname += ".dic";
            ierr = tmp.write(dictname.c_str());
            LOGGER(ierr < 0 && ibis::gVerbose > 0)
                << "Warning -- tafel::writeData failed to write the "
                "content of \"" << col.dictfile
                << "\" in the binary format to \"" << dictname << '"';
        }
    }
    md.close(); // close the metadata file
    ibis::fileManager::instance().flushDir(mydir);
    if (ibis::gVerbose > 2) {
        timer.stop();
        ibis::util::logger()()
            << "tafel::writeData outputted " 
            << cols.size() << " column" << (cols.size()>1 ? "s" : "")
            << " and " << nnew << " row" << (nnew>1 ? "s" : "")
            << " (total " << nold+nnew << ") to " << mydir
            << " using " << timer.CPUTime() << " sec(CPU), "
            << timer.realTime() << " sec(elapsed)";
    }

    return nnew;
} // ibis::tafel::writeData

void ibis::tafel::clearData() {
    mrows = 0;
    for (columnList::iterator it = cols.begin(); it != cols.end(); ++ it) {
        column& col = *((*it).second);
        col.mask.clear();
        switch (col.type) {
        case ibis::BLOB:
            static_cast<std::vector<ibis::opaque>*>(col.values)->clear();
            break;
        case ibis::BYTE:
            static_cast<array_t<signed char>*>(col.values)->clear();
            break;
        case ibis::UBYTE:
            static_cast<array_t<unsigned char>*>(col.values)->clear();
            break;
        case ibis::SHORT:
            static_cast<array_t<int16_t>*>(col.values)->clear();
            break;
        case ibis::USHORT:
            static_cast<array_t<uint16_t>*>(col.values)->clear();
            break;
        case ibis::INT:
            static_cast<array_t<int32_t>*>(col.values)->clear();
            break;
        case ibis::UINT:
            static_cast<array_t<uint32_t>*>(col.values)->clear();
            break;
        case ibis::LONG:
            static_cast<array_t<int64_t>*>(col.values)->clear();
            break;
        case ibis::ULONG:
            static_cast<array_t<uint64_t>*>(col.values)->clear();
            break;
        case ibis::FLOAT:
            static_cast<array_t<float>*>(col.values)->clear();
            break;
        case ibis::DOUBLE:
            static_cast<array_t<double>*>(col.values)->clear();
            break;
        case ibis::OID:
            static_cast<array_t<ibis::rid_t>*>(col.values)->clear();
            break;
        case ibis::TEXT:
        case ibis::CATEGORY:
            static_cast<std::vector<std::string>*>(col.values)->clear();
            break;
        default:
            break;
        } // switch
    } // for
} // ibis::tafel::clearData

/// Attempt to reserve enough memory for maxr rows to be stored in memory.
/// This function will not reserve space for more than 1 billion rows.  If
/// maxr is less than mrows, it will simply return mrows.  It calls
/// doReserve to performs the actual reservations.  If doReserve throws an
/// exception, it will reduce the value of maxr and try again.  It will
/// give up after 5 tries and return -1, otherwise, it returns the actual
/// capacity allocated.
///
/// @note
/// If the caller does not store more rows than can be held in memory, the
/// underlying data structure should automatically expand to accomodate the
/// new rows.  However, it is definitely advantageous to reserve space
/// ahead of time.  It will reduce the need to expand the underlying
/// storage objects, which can reduce the execution time.  In addition,
/// reserving a good fraction of the physical memory, say 10 - 40%, for
/// storing rows in memory can reduce the number of times the write
/// operation is invoked when loading a large number of rows from external
/// sources.  Since the string values are stored as std::vector objects,
/// additional memory is allocated for each new string added to memory,
/// therefore, after importing many long strings, it is still possible to
/// run out of memory even after one successfully reserved space with this
/// function.
///
/// @note It is possible for the existing content to be lost if doReserve
/// throws an exception, therefore, one should call this function when
/// this object does not hold any user data in memory.
int32_t ibis::tafel::reserveBuffer(uint32_t maxr) {
    if (cols.empty()) return maxr;
    if (mrows >= maxr) return mrows;
    if (maxr > 0x40000000) maxr = 0x40000000;

    int32_t ret = 0;
    try {
        size_t rowsize = 0;
        for (columnList::iterator it = cols.begin(); it != cols.end(); ++ it) {
            switch (it->second->type) {
            default:
                rowsize += 16; break;
            case ibis::BYTE:
            case ibis::UBYTE:
                rowsize += 1; break;
            case ibis::SHORT:
            case ibis::USHORT:
                rowsize += 2; break;
            case ibis::INT:
            case ibis::UINT:
            case ibis::FLOAT:
                rowsize += 4; break;
            case ibis::OID:
            case ibis::LONG:
            case ibis::ULONG:
            case ibis::DOUBLE:
                rowsize += 8; break;
            }
        }
        if (rowsize > 0) {
            rowsize += rowsize;
            size_t tmp = ibis::fileManager::bytesFree();
            if (tmp < 10000000) tmp = 10000000;
            tmp = tmp / rowsize;
            if (tmp < maxr) {
                LOGGER(ibis::gVerbose > 0)
                    << "tafel::reserveBuffer will reduce maxr from " << maxr
                    << " to " << tmp;
                maxr = tmp;
            }
        }
        ret = doReserve(maxr);
    }
    catch (...) {
        if (mrows > 0) {
            LOGGER(ibis::gVerbose >= 0)
                << "tafel::reserveBuffer(" << maxr << ") failed while mrows="
                << mrows << ", existing content has been lost";
            mrows = 0;
            return -2;
        }

        maxr >>= 1;
        try {
            ret = doReserve(maxr);
        }
        catch (...) {
            maxr >>= 2;
            try {
                ret = doReserve(maxr);
            }
            catch (...) {
                maxr >>= 2;
                try {
                    ret = doReserve(maxr);
                }
                catch (...) {
                    maxr >>= 2;
                    try {
                        ret = doReserve(maxr);
                    }
                    catch (...) {
                        LOGGER(ibis::gVerbose >= 0)
                            << "tafel::reserveBuffer(" << maxr
                            << ") failed after 5 tries";
                        ret = -1;
                    }
                }
            }
        }
    }
    return ret;
} // ibis::tafel::reserveBuffer

/// Reserve space for maxr records in memory.  This function does not
/// perform error checking.  The public version of it reserveBuffer does.
int32_t ibis::tafel::doReserve(uint32_t maxr) {
    if (mrows >= maxr)
        return mrows;
    LOGGER(ibis::gVerbose > 3)
        << "tafel::doReserve is to reserve space for " << maxr
        << " row" << (maxr>1?"s":"");

    int32_t ret = 0x7FFFFFFF;
    for (columnList::iterator it = cols.begin(); it != cols.end(); ++ it) {
        column& col = *((*it).second);
        col.mask.clear();
        switch (col.type) {
        case ibis::BYTE: {
            array_t<signed char>* tmp = 
                static_cast<array_t<signed char>*>(col.values);
            const uint32_t curr = tmp->capacity();
            if (mrows == 0 && curr > (maxr >> 1)*3) {
                delete tmp;
                tmp = new array_t<signed char>(maxr);
                col.values = tmp;
                tmp->resize(0);
                ret = maxr;
            }
            else if (curr < maxr) {
                tmp->reserve(maxr);
                ret = maxr;
            }
            else if (ret > (int32_t)curr) {
                ret = curr;
            }
            break;}
        case ibis::UBYTE: {
            array_t<unsigned char>* tmp = 
                static_cast<array_t<unsigned char>*>(col.values);
            const uint32_t curr = tmp->capacity();
            if (mrows == 0 && curr > (maxr >> 1)*3) {
                delete tmp;
                tmp = new array_t<unsigned char>(maxr);
                col.values = tmp;
                tmp->resize(0);
                ret = maxr;
            }
            else if (curr < maxr) {
                tmp->reserve(maxr);
                ret = maxr;
            }
            else if (ret > (int32_t) curr) {
                ret = curr;
            }
            break;}
        case ibis::SHORT: {
            array_t<int16_t>* tmp = 
                static_cast<array_t<int16_t>*>(col.values);
            const uint32_t curr = tmp->capacity();
            if (mrows == 0 && curr > (maxr >> 1)*3) {
                delete tmp;
                tmp = new array_t<int16_t>(maxr);
                col.values = tmp;
                tmp->resize(0);
                ret = maxr;
            }
            else if (curr < maxr) {
                tmp->reserve(maxr);
                ret = maxr;
            }
            else if (ret > (int32_t) curr) {
                ret = curr;
            }
            break;}
        case ibis::USHORT: {
            array_t<uint16_t>* tmp = 
                static_cast<array_t<uint16_t>*>(col.values);
            const uint32_t curr = tmp->capacity();
            if (mrows == 0 && curr > (maxr >> 1)*3) {
                delete tmp;
                tmp = new array_t<uint16_t>(maxr);
                col.values = tmp;
                tmp->resize(0);
                ret = maxr;
            }
            else if (curr < maxr) {
                tmp->reserve(maxr);
                ret = maxr;
            }
            else if (ret > (int32_t) curr) {
                ret = curr;
            }
            break;}
        case ibis::INT: {
            array_t<int32_t>* tmp = 
                static_cast<array_t<int32_t>*>(col.values);
            const uint32_t curr = tmp->capacity();
            if (mrows == 0 && curr > (maxr >> 1)*3) {
                delete tmp;
                tmp = new array_t<int32_t>(maxr);
                col.values = tmp;
                tmp->resize(0);
                ret = maxr;
            }
            else if (curr < maxr) {
                tmp->reserve(maxr);
                ret = maxr;
            }
            else if (ret > (int32_t) curr) {
                ret = curr;
            }
            break;}
        case ibis::UINT: {
            array_t<uint32_t>* tmp = 
                static_cast<array_t<uint32_t>*>(col.values);
            const uint32_t curr = tmp->capacity();
            if (mrows == 0 && curr > (maxr >> 1)*3) {
                delete tmp;
                tmp = new array_t<uint32_t>(maxr);
                col.values = tmp;
                tmp->resize(0);
                ret = maxr;
            }
            else if (curr < maxr) {
                tmp->reserve(maxr);
                ret = maxr;
            }
            else if (ret > (int32_t) curr) {
                ret = curr;
            }
            break;}
        case ibis::LONG: {
            array_t<int64_t>* tmp = 
                static_cast<array_t<int64_t>*>(col.values);
            const uint32_t curr = tmp->capacity();
            if (mrows == 0 && curr > (maxr >> 1)*3) {
                delete tmp;
                tmp = new array_t<int64_t>(maxr);
                col.values = tmp;
                tmp->resize(0);
                ret = maxr;
            }
            else if (curr < maxr) {
                tmp->reserve(maxr);
                ret = maxr;
            }
            else if (ret > (int32_t) curr) {
                ret = curr;
            }
            break;}
        case ibis::OID:
        case ibis::ULONG: {
            array_t<uint64_t>* tmp = 
                static_cast<array_t<uint64_t>*>(col.values);
            const uint32_t curr = tmp->capacity();
            if (mrows == 0 && curr > (maxr >> 1)*3) {
                delete tmp;
                tmp = new array_t<uint64_t>(maxr);
                col.values = tmp;
                tmp->resize(0);
                ret = maxr;
            }
            else if (curr < maxr) {
                tmp->reserve(maxr);
                ret = maxr;
            }
            else if (ret > (int32_t) curr) {
                ret = curr;
            }
            break;}
        case ibis::FLOAT: {
            array_t<float>* tmp = 
                static_cast<array_t<float>*>(col.values);
            const uint32_t curr = tmp->capacity();
            if (mrows == 0 && curr > (maxr >> 1)*3) {
                delete tmp;
                tmp = new array_t<float>(maxr);
                col.values = tmp;
                tmp->resize(0);
                ret = maxr;
            }
            else if (curr < maxr) {
                tmp->reserve(maxr);
                ret = maxr;
            }
            else if (ret > (int32_t) curr) {
                ret = curr;
            }
            break;}
        case ibis::DOUBLE: {
            array_t<double>* tmp = 
                static_cast<array_t<double>*>(col.values);
            const uint32_t curr = tmp->capacity();
            if (mrows == 0 && curr > (maxr >> 1)*3) {
                delete tmp;
                tmp = new array_t<double>(maxr);
                col.values = tmp;
                tmp->resize(0);
                ret = maxr;
            }
            else if (curr < maxr) {
                tmp->reserve(maxr);
                ret = maxr;
            }
            else if (ret > (int32_t) curr) {
                ret = curr;
            }
            break;}
        case ibis::TEXT:
        case ibis::CATEGORY: {
            std::vector<std::string>* tmp = 
                static_cast<std::vector<std::string>*>(col.values);
            const uint32_t curr = tmp->capacity();
            if (mrows == 0 && curr > (maxr >> 1)*3) {
                delete tmp;
                tmp = new std::vector<std::string>(maxr);
                col.values = tmp;
                tmp->resize(0);
                ret = maxr;
            }
            else if (curr < maxr) {
                tmp->reserve(maxr);
                ret = maxr;
            }
            else if (ret > (int32_t) curr) {
                ret = curr;
            }
            break;}
        case ibis::BLOB: {
            std::vector<ibis::opaque>* tmp =
                static_cast<std::vector<ibis::opaque>*>(col.values);
            tmp->reserve(maxr);
            ret = maxr;
            break;}
        default:
            break;
        } // switch
    } // for
    LOGGER(ibis::gVerbose > 1)
        << "tafel::doReserve(" << maxr << ") completed with actual capacity "
        << ret;
    return ret;
} // ibis::tafel::doReserve

uint32_t ibis::tafel::bufferCapacity() const {
    if (cols.empty()) return 0U;
    uint32_t cap = 0xFFFFFFFF;
    for (columnList::const_iterator it = cols.begin();
         it != cols.end(); ++ it) {
        column& col = *((*it).second);
        if (col.values == 0) {
            col.mask.clear();
            return 0U;
        }

        uint32_t tmp = 0;
        switch (col.type) {
        case ibis::BYTE:
            tmp = static_cast<array_t<signed char>*>(col.values)->capacity();
            break;
        case ibis::UBYTE:
            tmp = static_cast<array_t<unsigned char>*>(col.values)->capacity();
            break;
        case ibis::SHORT:
            tmp = static_cast<array_t<int16_t>*>(col.values)->capacity();
            break;
        case ibis::USHORT:
            tmp = static_cast<array_t<uint16_t>*>(col.values)->capacity();
            break;
        case ibis::INT:
            tmp = static_cast<array_t<int32_t>*>(col.values)->capacity();
            break;
        case ibis::UINT:
            tmp = static_cast<array_t<uint32_t>*>(col.values)->capacity();
            break;
        case ibis::LONG:
            tmp = static_cast<array_t<int64_t>*>(col.values)->capacity();
            break;
        case ibis::ULONG:
            tmp = static_cast<array_t<uint64_t>*>(col.values)->capacity();
            break;
        case ibis::FLOAT:
            tmp = static_cast<array_t<float>*>(col.values)->capacity();
            break;
        case ibis::DOUBLE:
            tmp = static_cast<array_t<double>*>(col.values)->capacity();
            break;
        case ibis::TEXT:
        case ibis::CATEGORY:
            tmp =
                static_cast<std::vector<std::string>*>(col.values)->capacity();
            break;
        case ibis::BLOB:
            tmp =
                static_cast<std::vector<ibis::opaque>*>(col.values)->capacity();
            break;
        case ibis::OID:
            tmp = static_cast<array_t<ibis::rid_t>*>(col.values)->capacity();
            break;
        default:
            break;
        } // switch

        if (tmp < cap)
            cap = tmp;
        if (tmp == 0U)
            return tmp;
    } // for
    return cap;
} // ibis::tafel::capacity

/// Compute the number of rows that are likely to fit in available memory.
/// It only count string valued column to cost 16 bytes for each row.  This
/// can be a significant underestimate of the actual cost.  Memory
/// fragmentation may also significantly reduce the available space.
uint32_t ibis::tafel::preferredSize() const {
    long unsigned width = 0;
    for (columnList::const_iterator it = cols.begin(); it != cols.end();
         ++ it) {
        const column& col = *((*it).second);
        switch (col.type) {
        case ibis::BYTE:
        case ibis::UBYTE:
            ++ width;
            break;
        case ibis::SHORT:
        case ibis::USHORT:
            width += 2;
            break;
        case ibis::INT:
        case ibis::UINT:
        case ibis::FLOAT:
            width += 4;
            break;
        case ibis::OID:
        case ibis::LONG:
        case ibis::ULONG:
        case ibis::DOUBLE:
            width += 8;
            break;
        default:
            width += 64;
            break;
        } // switch
    } // for
    if (width == 0) width = 1024;
    width = ibis::fileManager::bytesFree() / width;
    width = static_cast<long unsigned>(ibis::util::coarsen(0.45*width, 1));
    if (width > 100000000)
        width = 100000000;
    return width;
} // ibis::tafel::preferredSize

void ibis::tafel::clear() {
    const uint32_t ncol = colorder.size();
    LOGGER(ibis::gVerbose > 2)
        << "clearing content of ibis::tafel " << (void*)this;
    for (uint32_t i = 0; i < ncol; ++ i)
        delete colorder[i];
    colorder.clear();
    cols.clear();
    mrows = 0;
} // ibis::tafel::clear

/// Digest a line of text and place the values identified into the
/// corresponding columns.  The actual values are extracted by
/// ibis::util::readInt, ibis::util::readUInt, ibis::util::readDouble and
/// ibis::util::readString.  When any of these functions returns an error
/// condition, this function assumes the value to be recorded is a NULL.
/// The presence of a NULL value is marked by a 0-bit in the mask
/// associated with the column.  The actual value in the associated buffer
/// is the largest integer value for an integer column and a quiet NaN for
/// floating-point valued column.
int ibis::tafel::parseLine(const char* str, const char* del, const char* id) {
    int cnt = 0;
    int ierr;
    int64_t itmp;
    double dtmp;
    std::string stmp;
    if (del == 0 || *del == 0)
        del = " ,;\t\n\v";
    const uint32_t ncol = colorder.size();
    for (uint32_t i = 0; i < ncol; ++ i) {
        column& col = *(colorder[i]);
        if (col.values == 0) {
            reserveBuffer(100000);
            if (col.values == 0) {
                LOGGER(ibis::gVerbose >= 0)
                    << "Warning -- tafel::parseLine failed to acquire memory "
                    "for column " << i << " ("<< col.name << ")";
                return -1;
            }
        }
        switch (col.type) {
        case ibis::BYTE: {
            ierr = ibis::util::readInt(itmp, str, del);
            if (ierr == 0) {
                signed char tmp = static_cast<signed char>(itmp);
                static_cast<array_t<signed char>*>(col.values)
                    ->push_back(tmp);
                ++ cnt;
                if ((int64_t)tmp == itmp) {
                    col.mask += 1;
                }
                else {
                    col.mask += 0;
                    LOGGER(ibis::gVerbose > 2)
                        << "Warning -- tafel::parseLine column " << i+1
                        << " in " << id << " (" << itmp << ") "
                        << "can not fit into a one-byte integer";
                }
            }
            else {
                static_cast<array_t<signed char>*>(col.values)
                    ->push_back((signed char)0x7F);
                col.mask += 0;
                ++ cnt;
                LOGGER(ibis::gVerbose > 3)
                    << "tafel::parseLine treating column " << i+1
                    << " in " << id << " as a null value";
            }
            break;}
        case ibis::UBYTE: {
            ierr = ibis::util::readInt(itmp, str, del);
            if (ierr == 0) {
                unsigned char tmp = static_cast<unsigned char>(itmp);
                static_cast<array_t<unsigned char>*>(col.values)
                    ->push_back(tmp);
                ++ cnt;
                if ((int64_t)tmp == itmp) {
                    col.mask += 1;
                }
                else {
                    col.mask += 0;
                    LOGGER(ibis::gVerbose > 2)
                        << "Warning -- tafel::parseLine column " << i+1
                        << " in " << id << " (" << itmp << ") "
                        << "can not fit into a one-byte integer";
                }
            }
            else {
                static_cast<array_t<unsigned char>*>(col.values)
                    ->push_back((unsigned char)0xFFU);
                col.mask += 0;
                ++ cnt;
                LOGGER(ibis::gVerbose > 3)
                    << "tafel::parseLine treating column " << i+1
                    << " in " << id << " as a null value";
            }
            break;}
        case ibis::SHORT: {
            ierr = ibis::util::readInt(itmp, str, del);
            if (ierr == 0) {
                int16_t tmp = static_cast<int16_t>(itmp);
                static_cast<array_t<int16_t>*>(col.values)
                    ->push_back(tmp);
                ++ cnt;
                if ((int64_t)tmp == itmp) {
                    col.mask += 1;
                }
                else {
                    col.mask += 0;
                    LOGGER(ibis::gVerbose > 2)
                        << "Warning -- tafel::parseLine column " << i+1
                        << " in " << id << " (" << itmp << ") "
                        << "can not fit into a two-byte integer";
                }
            }
            else {
                static_cast<array_t<int16_t>*>(col.values)
                    ->push_back((int16_t)0x7FFF);
                col.mask += 0;
                ++ cnt;
                LOGGER(ibis::gVerbose > 3)
                    << "tafel::parseLine treating column " << i+1
                    << " in " << id << " as a null value";
            }
            break;}
        case ibis::USHORT: {
            ierr = ibis::util::readInt(itmp, str, del);
            if (ierr == 0) {
                uint16_t tmp = static_cast<uint16_t>(itmp);
                static_cast<array_t<uint16_t>*>(col.values)
                    ->push_back(tmp);
                ++ cnt;
                if ((int64_t)tmp == itmp) {
                    col.mask += 1;
                }
                else {
                    col.mask += 0;
                    LOGGER(ibis::gVerbose > 2)
                        << "Warning -- tafel::parseLine column " << i+1
                        << " in " << id << " (" << itmp << ") "
                        << "can not fit into a two-byte integer";
                }
            }
            else {
                static_cast<array_t<uint16_t>*>(col.values)
                    ->push_back((uint16_t)0xFFFFU);
                col.mask += 0;
                ++ cnt;
                LOGGER(ibis::gVerbose > 3)
                    << "tafel::parseLine treating column " << i+1
                    << " in " << id << " as a null value";
            }
            break;}
        case ibis::INT: {
            ierr = ibis::util::readInt(itmp, str, del);
            if (ierr == 0) {
                int32_t tmp = static_cast<int32_t>(itmp);
                static_cast<array_t<int32_t>*>(col.values)
                    ->push_back(tmp);
                ++ cnt;
                if ((int64_t)tmp == itmp) {
                    col.mask += 1;
                }
                else {
                    col.mask += 0;
                    LOGGER(ibis::gVerbose > 2)
                        << "Warning -- tafel::parseLine column " << i+1
                        << " in " << id << " (" << itmp << ") "
                        << "can not fit into a four-byte integer";
                }
            }
            else {
                static_cast<array_t<int32_t>*>(col.values)
                    ->push_back((int32_t)0x7FFFFFFF);
                col.mask += 0;
                ++ cnt;
                LOGGER(ibis::gVerbose > 3)
                    << "tafel::parseLine treating column " << i+1
                    << " in " << id << " as a null value";
            }
            break;}
        case ibis::UINT: {
            ierr = ibis::util::readInt(itmp, str, del);
            if (ierr == 0) {
                uint32_t tmp = static_cast<uint32_t>(itmp);
                static_cast<array_t<uint32_t>*>(col.values)
                    ->push_back(tmp);
                ++ cnt;
                if ((int64_t)tmp == itmp) {
                    col.mask += 1;
                }
                else {
                    col.mask += 0;
                    LOGGER(ibis::gVerbose > 2)
                        << "Warning -- tafel::parseLine column " << i+1
                        << " in " << id << " (" << itmp << ") "
                        << "can not fit into a four-byte integer";
                }
            }
            else {
                static_cast<array_t<uint32_t>*>(col.values)
                    ->push_back((uint32_t)0xFFFFFFFFU);
                col.mask += 0;
                ++ cnt;
                LOGGER(ibis::gVerbose > 3)
                    << "tafel::parseLine treating column " << i+1
                    << " in " << id << " as a null value";
            }
            break;}
        case ibis::LONG: {
            ierr = ibis::util::readInt(itmp, str, del);
            ++ cnt;
            if (ierr == 0) {
                static_cast<array_t<int64_t>*>(col.values)->push_back(itmp);
                col.mask += 1;
            }
            else {
                static_cast<array_t<int64_t>*>(col.values)
                    ->push_back(0x7FFFFFFFFFFFFFFFLL);
                col.mask += 0;
                LOGGER(ibis::gVerbose > 3)
                    << "tafel::parseLine treating column " << i+1
                    << " in " << id << " as a null value";
            }
            break;}
        case ibis::OID:
        case ibis::ULONG: {
            uint64_t jtmp;
            ierr = ibis::util::readUInt(jtmp, str, del);
            ++ cnt;
            if (ierr == 0) {
                static_cast<array_t<uint64_t>*>(col.values)->push_back(jtmp);
                col.mask += 1;
            }
            else {
                static_cast<array_t<uint64_t>*>(col.values)
                    ->push_back(0xFFFFFFFFFFFFFFFFULL);
                col.mask += 0;
                LOGGER(ibis::gVerbose > 3)
                    << "tafel::parseLine treating column " << i+1
                    << " in " << id << " as a null value";
            }
            break;}
        case ibis::FLOAT: {
            ierr = ibis::util::readDouble(dtmp, str, del);
            ++ cnt;
            if (ierr == 0) {
                static_cast<array_t<float>*>(col.values)
                    ->push_back((float)dtmp);
                col.mask += 1;
            }
            else {
                static_cast<array_t<float>*>(col.values)
                    ->push_back(FASTBIT_FLOAT_NULL);
                col.mask += 0;
                LOGGER(ibis::gVerbose > 3)
                    << "tafel::parseLine treating column " << i+1
                    << " in " << id << " as a null value";
            }
            break;}
        case ibis::DOUBLE: {
            ierr = ibis::util::readDouble(dtmp, str, del);
            ++ cnt;
            if (ierr == 0) {
                static_cast<array_t<double>*>(col.values)->push_back(dtmp);
                col.mask += 1;
            }
            else {
                static_cast<array_t<double>*>(col.values)
                    ->push_back(FASTBIT_DOUBLE_NULL);
                col.mask += 0;
                LOGGER(ibis::gVerbose > 3)
                    << "tafel::parseLine treating column " << i+1
                    << " in " << id << " as a null value";
            }
            break;}
        case ibis::CATEGORY:
        case ibis::TEXT: {
            ierr = ibis::util::readString(stmp, str, del);
            static_cast<std::vector<std::string>*>(col.values)->push_back(stmp);
            col.mask += (ierr >= 0);
            ++ cnt;
            break;}
        case ibis::BLOB: {
            ierr = ibis::util::readString(stmp, str, del);
            std::vector<ibis::opaque> *raw =
                static_cast<std::vector<ibis::opaque>*>(col.values);
            raw->resize(raw->size()+1);
            raw->back().copy(stmp.data(), stmp.size());
            col.mask += 1;
            ++ cnt;
            break;}
        default: {
            LOGGER(ibis::gVerbose > 2)
                << "Warning -- tafel::parseLine column " << i+1
                << " in " << id << " has an unsupported type "
                << ibis::TYPESTRING[(int) col.type];
            break;}
        }

        if (*str != 0) { // skip trailing space and one delimeter
            // while (*str != 0 && isspace(*str)) ++ str; // trailing space
            // if (*str != 0 && del != 0 && *del != 0 && strchr(del, *str) != 0)
            //  ++ str;
        }
        else {
            break;
        }
    }
    return cnt;
} // ibis::tafel::parseLine

int ibis::tafel::appendRow(const char* line, const char* del) {
    if (line == 0 || *line == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "tafel::appendRow can not proceed because the incoming line "
            "is nil or empty";
        return -1;
    }
    while (*line != 0 && isspace(*line)) ++ line;
    if (*line == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "tafel::appendRow can not proceed because the incoming line "
            "is a blank string";
        return -1;
    }
    if (*line == '#' || (*line == '-' && line[1] == '-')) return 0;

    std::string id = "string ";
    id.append(line, 10);
    id += " ...";

    normalize();
    int ierr = parseLine(line, del, id.c_str());
    LOGGER(ierr < static_cast<int>(cols.size()) && ibis::gVerbose > 1)
        << "tafel::appendRow expects to extract " << cols.size() << " value"
        << (cols.size()>1?"s":"") << ", but got " << ierr;
    mrows += (ierr > 0);
    return ierr;
} // ibis::tafel::appendRow

int ibis::tafel::readCSV(const char* filename, int maxrows,
                         const char* outdir, const char* del) {
    if (filename == 0 || *filename == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "tafel::readCSV needs a filename to proceed";
        return -1;
    }
    if (colorder.empty()) {
        LOGGER(ibis::gVerbose > 0)
            << "tafel::readCSV(" << filename << ") can not proceed because of "
            "improper initialization (colorder is empty)";
        return -2;
    }
    ibis::horometer timer;
    timer.start();

    ibis::fileManager::buffer<char> linebuf(MAX_LINE);
    std::ifstream csv(filename);
    if (! csv) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- tafel::readCSV(" << filename << ") failed to open "
            "the named file for reading";
        return -3; // failed to open the specified data file
    }
    if (maxrows <= 0)
        maxrows = preferredSize();
    if (maxrows > 1) {
        try { // try to reserve request amount of space
            reserveBuffer(maxrows);
        }
        catch (...) {
            LOGGER(ibis::gVerbose > 0)
                << "tafel::readCSV(" << filename << ", " << maxrows
                << ") -- failed to reserve space for "
                << maxrows << " rows for reading, continue anyway";
        }
    }

    int ierr;
    int ret = 0;
    uint32_t cnt = 0;
    uint32_t iline = 0;
    bool more = true;
    const uint32_t pline = (ibis::gVerbose < 3 ? 1000000 :
                            ibis::gVerbose < 5 ? 100000 :
                            ibis::gVerbose < 7 ? 10000 : 1000);
    char* str; // pointer to next character to be processed
    const uint32_t ncol = colorder.size();
    while (more) {
        ++ iline;
        std::streampos linestart = csv.tellg();
        while (! csv.getline(linebuf.address(), linebuf.size())) {
            if (csv.eof()) {
                *(linebuf.address()) = 0;
                more = false;
                -- iline;
                break;
            }

            // failed to read the line
            const uint32_t nold =
                (linebuf.size() > 0 ? linebuf.size() : MAX_LINE);
            // double the size of linebuf
            if (nold+nold != linebuf.resize(nold+nold)) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- tafel::readCSV(" << filename
                    << ") failed to allocate linebuf of " << nold+nold
                    << " bytes";
                more = false;
                break;
            }
            csv.clear(); // clear the error bit
            // go back to the beginning of the line so we can try to read again
            if (! csv.seekg(linestart, std::ios::beg)) {
                LOGGER(ibis::gVerbose >= 0)
                    << "Warning -- tafel::readCSV(" << filename
                    << ") failed to seek to the start of line # " << iline
                    << ", no way to continue";
                *(linebuf.address()) = 0;
                more = false;
                break;
            }
        }

        str = linebuf.address();
        if (str == 0) break;
        while (*str != 0 && isspace(*str)) ++ str; // skip leading space
        if (*str == 0 || *str == '#' || (*str == '-' && str[1] == '-')) {
            // skip comment line (shell style comment and SQL style comments)
            continue;
        }

        if (0 < cnt && cnt < ncol)
            normalize();
        try {
            cnt = parseLine(str, del, filename);
        }
        catch (...) {
            if (outdir != 0 && *outdir != 0 && mrows > 0) {
                LOGGER(ibis::gVerbose > 3)
                    << "tafel::readCSV(" << filename << ") encountered an "
                    "exception while processing line " << iline
                    << ", writing in-memory data and then continue";
                ierr = write(outdir, 0, 0, 0);
                if (ierr < 0)
                    return ierr - 10;
                ret += mrows;
                // update maxrows to avoid out of memory problem
                if (mrows > 1024) {
                    maxrows = (int) ibis::util::coarsen((double)mrows, 1U);
                    if ((unsigned)maxrows >= mrows)
                        maxrows >>= 1;
                }
                else {
                    maxrows = mrows;
                }
            }
            else {
                return -4;
            }

            cnt = 0;
            -- iline;
            clearData();
            csv.seekg(linestart, std::ios::beg);
        }

        mrows += (cnt > 0);
        LOGGER(ibis::gVerbose > 0 && (iline % pline) == 0)
            << "tafel::readCSV(" << filename << ") processed line "
            << iline << " ...";
        if (maxrows > 1 && mrows >= static_cast<unsigned>(maxrows) &&
            outdir != 0 && *outdir != 0) {
            ierr = write(outdir, 0, 0, 0);
            ret += mrows;
            if (ierr < 0)
                return ierr - 20;
            else
                clearData();
        }
    }

    ret += mrows;
    timer.stop();
    LOGGER(ibis::gVerbose > 0)
        << "tafel::readCSV(" << filename << ") processed " << iline
        << (iline>1 ? " lines":" line") << " of text and extracted " << ret
        << (ret>1?" records":" record") << " using " << timer.CPUTime()
        << " sec(CPU), " << timer.realTime() << " sec(elapsed)";
    return ret;
} // ibis::tafel::readCSV

int ibis::tafel::readSQLDump(const char* filename, std::string& tname,
                             int maxrows, const char* outdir) {
    if (filename == 0 || *filename == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "tafel::readSQLDump needs a filename to proceed";
        return -1;
    }
    const char* delimiters = " ,;\t\n\v";
    ibis::horometer timer;
    timer.start();

    const unsigned defaultBufferSize = 1048576; // 1 MB
    ibis::fileManager::buffer<char> linebuf(defaultBufferSize);
    ibis::fileManager::buffer<char> stmtbuf(defaultBufferSize);
    std::ifstream sqlfile(filename);
    if (! sqlfile) {
        LOGGER(ibis::gVerbose >= 0)
            << "tafel::readSQLDump(" << filename << ") failed to open the "
            "named file for reading";
        return -3; // failed to open the specified data file
    }
    if (maxrows <= 0)
        maxrows = preferredSize();
    if (maxrows > 1) {
        try { // try to reserve request amount of space
            reserveBuffer(maxrows);
        }
        catch (...) {
            LOGGER(ibis::gVerbose > 0)
                << "tafel::readSQLDump(" << filename << ", " << maxrows
                << ") -- failed to reserve space for "
                << maxrows << " rows for reading, continue anyway";
        }
    }
    LOGGER(ibis::gVerbose > 2)
        << "tafel::readSQLDump(" << filename
        << ") successfully opened the named file for reading";
    int ierr=-1;
    char *str=0;
    char *ptr=0;
    std::string tmp;
    int ret = 0;
    uint32_t iline = 0;
    const uint32_t pline = (ibis::gVerbose < 3 ? 1000000 :
                            ibis::gVerbose < 5 ? 100000 :
                            ibis::gVerbose < 7 ? 10000 : 1000);
    while ((ierr = readSQLStatement(sqlfile, stmtbuf, linebuf)) > 0) {
        ++ iline;
        if (strnicmp(stmtbuf.address(), "create table ", 13) == 0) {
            ierr = SQLCreateTable(stmtbuf.address(), tname);
            if (ierr < 0) {
                LOGGER(ibis::gVerbose >= 0)
                    << "Warning -- tafel::readSQLDump(" << filename
                    << ") failed to digest the creat table statement:\n\t"
                    << stmtbuf.address();
                return ierr - 10;
            }
            else {
                LOGGER(ibis::gVerbose > 2)
                    << "tafel::readSQLDump(" << filename
                    << ") ingest the create table statement, starting "
                    "a brand new in-memory data table with " << cols.size()
                    << " column" << (cols.size()>1?"s":"");
            }
        }
        else if (strnicmp(stmtbuf.address(), "insert into ", 12) == 0) {
            str = stmtbuf.address() + 12;
            ierr = ibis::util::readString(tmp, const_cast<const char*&>(str),
                                          static_cast<const char*>(0));
            if (ierr < 0) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- tafel::readSQLDump(" << filename
                    << ") failed to extract table name from SQL statment # "
                    << iline;
                continue;
            }
            else if (!tname.empty() && tmp.compare(tname) != 0) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- tafel::readSQLDump(" << filename
                    << ") SQL statment # " << iline << " refers to table "
                    << tmp << ", but the current active table is " << tname
                    << ", skipping this statement";
                continue;
            }

            do { // loop through the values
                // skip to the open '('
                while (*str != 0 && *str != '(') ++ str;
                if (*str == '(') {
                    ++ str;
                    if (*str != 0) {
                        // string values can contain paired parentheses
                        int nesting = 0;
                        for (ptr = str;
                             *ptr != 0 && (nesting > 0 || *ptr != ')');
                             ++ ptr) {
                            nesting += (int)(*ptr == '(') - (int)(*ptr == ')');
                        }
                    }
                    if (ptr > str) {
                        if (*ptr == ')') *ptr = 0;
                        try {
                            ierr = parseLine(str, delimiters, filename);
                        }
                        catch (...) {
                            if (outdir != 0 && *outdir != 0 && mrows > 0) {
                                LOGGER(ibis::gVerbose > 3)
                                    << "tafel::readSQLDump(" << filename
                                    << ") encountered an exception while "
                                    "processing statement " << iline
                                    << ", writing out in-memory data";
                                ierr = write(outdir, tname.c_str(), 0, 0);
                                if (ierr < 0)
                                    return ierr - 20;

                                // to avoid future out-of-memory problem
                                if (mrows > 1024) {
                                    maxrows = (int)
                                        ibis::util::coarsen((double)mrows, 1);
                                    if ((unsigned)maxrows >= mrows)
                                        maxrows >>= 1;
                                }
                                else {
                                    maxrows = mrows;
                                }
                                ret += mrows;
                                clearData();

                                // try to parse the same line
                                ierr = parseLine(str, delimiters, filename);
                            }
                            else {
                                return -4;
                            }
                        }
                        mrows += (ierr > 0);

                        LOGGER(ibis::gVerbose > 1 &&
                               ierr < static_cast<long>(colorder.size()))
                            << "tafel::readSQLDump(" << filename
                            << ") expects to extract " << colorder.size()
                            << " value" << (colorder.size()>1?"s":"")
                            << ", but actually got " << ierr
                            << " while processing SQL statement # " << iline
                            << " and row " << mrows;
                        LOGGER(ibis::gVerbose > 0 && (mrows % pline) == 0)
                            << "tafel::readSQLDump(" << filename
                            << ") processed row " << mrows << " ...";

                        if (maxrows > 1 &&
                            mrows >= static_cast<unsigned>(maxrows) &&
                            outdir != 0 && *outdir != 0) {
                            ierr = write(outdir, tname.c_str(), 0, 0);
                            ret += mrows;
                            if (ierr < 0)
                                return ierr - 20;

                            clearData();
                        }
                    }
                    str = ptr + 1;
                }
            } while (*str != 0);
        }
        else { // do nothing with this statement
            LOGGER(ibis::gVerbose > 4)
                << "tafel::readSQLDump(" << filename << ") skipping: "
                << stmtbuf.address();
        }
    }

    ret += mrows;
    timer.stop();
    LOGGER(ibis::gVerbose > 0)
        << "tafel::readSQLDump(" << filename << ") processed " << iline
        << (iline>1 ? " lines":" line") << " of text and extracted " << ret
        << (ret>1?" records":" record") << " using " << timer.CPUTime()
        << " sec(CPU), " << timer.realTime() << " sec(elapsed)";
    return ret;
} // ibis::tafel::readSQLDump

/// Read one complete SQL statment from an SQL dump file.  It will read one
/// line at a time until a semicolon ';' is found.  It will expand the
/// buffers as needed.  The return value is either the number of bytes in
/// the SQL statement or an eror code (less than 0).
int ibis::tafel::readSQLStatement(std::istream& sqlfile,
                                  ibis::fileManager::buffer<char>& stmt,
                                  ibis::fileManager::buffer<char>& line) const {
    if (! sqlfile) return -1;

    bool more = true, retry = false;
    int nstmt = 0; // # of bytes used in stmt
    char* ptr;
    char* qtr;
    while (more) {
        std::streampos linestart = sqlfile.tellg();

        do {
            // attempt to read a line
            sqlfile.getline(line.address(), line.size());
            int bytes = sqlfile.gcount();
            retry = ! sqlfile.good();

            // failed to read the line
            if (sqlfile.eof() && bytes <= 0) {
                return 0;
            }

            if (static_cast<size_t>(bytes+1) == line.size()) { // line too small
                const uint32_t nold = (line.size() > 0 ? line.size() : 1048576);
                // double the size of line
                if (nold+nold != line.resize(nold+nold)) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- tafel::readSQLStatement failed to "
                        "allocate a line buffer with " << nold+nold << " bytes";
                    return -2;
                }

                sqlfile.clear(); // clear the error bit
                // go back to the beginning of the line so we can try to
                // read again
                if (! sqlfile.seekg(linestart, std::ios::beg)) {
                    LOGGER(ibis::gVerbose >= 0)
                        << "Warning -- tafel::readSQLStatement failed to seek "
                        "to the start of line, no way to continue";
                    *(stmt.address()) = 0;
                    return -3;
                }
                else {
                    retry = true;
                }
            }
        } while (retry);

        // got a line of input text
        ptr = line.address();
        while (*ptr != 0 && isspace(*ptr)) ++ ptr; // skip leading space
        if (*ptr == 0) continue;

        qtr = strstr(ptr, "--");
        if (qtr != 0) // change 1st - to nil character to terminate the string
            *qtr = 0;

        while (ptr != 0 && *ptr != 0) {
            // copy till / *
            qtr = strstr(ptr, "/*");
            if (qtr == 0)
                for (qtr = ptr+1; *qtr != 0; ++ qtr);

            int newchars = (qtr - ptr);
            uint32_t newsize = nstmt + newchars;
            if (newsize > stmt.size()) { // allocate new space
                newsize = (stmt.size()+stmt.size() >= newsize ?
                           stmt.size()+stmt.size() : newsize);
                ibis::fileManager::buffer<char> newbuf(line.size() << 1);
                if (newbuf.size() < line.size()) {
                    LOGGER(ibis::gVerbose >= 0)
                        << "Warning -- tafel::readSQLStatement failed to "
                        "allocate  a new buffer of " << newsize << " bytes";
                    return -4;
                }
                if (nstmt > 0) {
                    char *dest = newbuf.address();
                    const char *src = stmt.address();
                    for (int j = 0; j < nstmt; ++ j)
                        dest[j] = src[j];
                }
                newbuf.swap(stmt);
            }

            if (nstmt > 0 && isspace(stmt[nstmt-1]) == 0) {
                stmt[nstmt] = ' '; // add space between lines
                ++ nstmt;
            }
            else if (nstmt == 0) { // skip ; at the beginning of a statement
                while (ptr < qtr && (isspace(*ptr) || *ptr == ';')) ++ ptr;
            }
            for (char *curr = stmt.address()+nstmt; ptr < qtr;
                 ++ ptr, ++ curr, ++ nstmt)
                *curr = *ptr;

            // skip past * /, if not found, assume everything is comment
            qtr = strstr(ptr, "*/");
            if (qtr == 0) {
                ptr = 0; // skip the remaining of the line
            }
            else {
                ptr = qtr + 2; // skip over * /
            }
        }

        // remove trailing space
        while (nstmt > 0 && isspace(stmt[nstmt-1])) -- nstmt;
        if (nstmt == 1 && stmt[0] == ';') {
            nstmt = 0;
        }
        else if (nstmt > 1 && stmt[nstmt-1] == ';') {
            more = false;
        }
    } // while (more)

    if (nstmt > 1 && stmt[nstmt-1] == ';') {
        // turn semicolon into nil character
        -- nstmt;
        stmt[nstmt] = 0;
    }
    return nstmt;
} // ibis::tafel::readSQLStatement

void ibis::tafel::describe(std::ostream &out) const {
    out << "An extensible (in-memory) table with " << mrows << " row"
        << (mrows>1 ? "s" : "") << " and " << cols.size() << " column"
        << (cols.size()>1 ? "s" : "");
    for (columnList::const_iterator it = cols.begin();
         it != cols.end(); ++ it) {
        const ibis::tafel::column& col = *((*it).second);
        out << "\n  " << (*it).first
#if _DEBUG+0 > 0 || DEBUG+0 > 0
            << "(" << static_cast<void*>((*it).second) << ")"
#endif
            << ", " << ibis::TYPESTRING[col.type]
            << ", mask(" << col.mask.cnt() << " out of " << col.mask.size()
            << ")";
    }
    out << std::endl;
} // ibis::tafel::describe

ibis::table* ibis::tafel::toTable(const char *nm, const char *de) {
    ibis::table::bufferArray databuf;
    ibis::table::stringArray cname;
    ibis::table::typeArray ctype;
    if (mrows == 0 || cols.empty())
        return new ibis::bord(nm, de, 0, databuf, ctype, cname);

    normalize();
    const uint32_t ncol = colorder.size();
    LOGGER(ibis::gVerbose > 2)
        << "tafel::toTable -- preparing " << mrows << " row" << (mrows>1?"s":"")
        << " and " << ncol << " column" << (ncol>1?"s":"")
        << " for transferring";
    databuf.resize(ncol);
    cname.resize(ncol);
    ctype.resize(ncol);
    for (unsigned j = 0; j < ncol; ++ j) {
        const column* col = colorder[j];
        if (col == 0 || col->name.empty() ||
            col->type == ibis::UNKNOWN_TYPE) {
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- tafel::toTable can not process column " << j
                << " because it has no name or an invalid type";
            return 0;
        }
        cname[j] = col->name.c_str();
        ctype[j] = col->type;
        databuf[j] = col->values;
    }
    std::unique_ptr<ibis::bord>
        brd(new ibis::bord(nm, de, mrows, databuf, ctype, cname));
    if (brd.get() == 0) return 0;

    mrows = 0;
    // completed the transfer of content, reset the pointers
    for (unsigned j = 0; j < ncol; ++ j) {
        colorder[j]->values = 0;

        ibis::column *col = brd->getColumn(colorder[j]->name.c_str());
        if (col == 0) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- tafel::toTable failed to locate column "
                << colorder[j]->name << " in the new table object";
        }
        else if (0 > col->setNullMask(colorder[j]->mask)) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- tafel::toTable failed to set the null mask for "
                << colorder[j]->name;
        }
        colorder[j]->mask.clear();
    }
    return brd.release();
} // ibis::tafel::toTable

void ibis::tafel::setASCIIDictionary
(const char *colname, const char *dictfile) {
    if (colname == 0 || *colname == 0) return;
    ibis::tafel::columnList::iterator it = cols.find(colname);
    if (it == cols.end() || it->second == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- tafel::setASCIIDictionary can not find "
            "a columne named " << colname;
        return;
    }
    ibis::tafel::column &col = *(it->second);
    if (col.type != ibis::CATEGORY && col.type != ibis::UINT) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- tafel::setASCIIDictionary can only set a dictionary "
            "on a column of categorical values, but column " << colname
            << " has a type of " << ibis::TYPESTRING[(int)col.type];
        return;
    }
    col.dictfile = dictfile;
    LOGGER(ibis::gVerbose > 2)
        << "tafel::setASCIIDictionary -- " << col.name << " : " << col.dictfile;
} // ibis::tafel::setASCIIDictionary

const char* ibis::tafel::getASCIIDictionary(const char *colname) const {
    if (colname == 0 || *colname == 0) return 0;
    ibis::tafel::columnList::const_iterator it = cols.find(colname);
    if (it == cols.end() || it->second == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- tafel::getASCIIDictionary can not find "
            "a columne named " << colname;
        return 0;
    }
    const ibis::tafel::column &col = *(it->second);
    return col.dictfile.c_str();
} // ibis::tafel::getASCIIDictionary

/// Default constructor.  The name and type are assigned later.
ibis::tafel::column::column() : type(ibis::UNKNOWN_TYPE), values(0), defval(0) {
}

/// Destructor.
ibis::tafel::column::~column() {
    LOGGER(ibis::gVerbose > 5 && !name.empty())
        << "clearing tafel::column " << name;

    switch (type) {
    case ibis::BYTE:
        delete static_cast<array_t<signed char>*>(values);
        delete static_cast<signed char*>(defval);
        break;
    case ibis::UBYTE:
        delete static_cast<array_t<unsigned char>*>(values);
        delete static_cast<unsigned char*>(defval);
        break;
    case ibis::SHORT:
        delete static_cast<array_t<int16_t>*>(values);
        delete static_cast<int16_t*>(defval);
        break;
    case ibis::USHORT:
        delete static_cast<array_t<uint16_t>*>(values);
        delete static_cast<uint16_t*>(defval);
        break;
    case ibis::INT:
        delete static_cast<array_t<int32_t>*>(values);
        delete static_cast<int32_t*>(defval);
        break;
    case ibis::UINT:
        delete static_cast<array_t<uint32_t>*>(values);
        delete static_cast<uint32_t*>(defval);
        break;
    case ibis::LONG:
        delete static_cast<array_t<int64_t>*>(values);
        delete static_cast<int64_t*>(defval);
        break;
    case ibis::ULONG:
        delete static_cast<array_t<uint64_t>*>(values);
        delete static_cast<uint64_t*>(defval);
        break;
    case ibis::FLOAT:
        delete static_cast<array_t<float>*>(values);
        delete static_cast<float*>(defval);
        break;
    case ibis::DOUBLE:
        delete static_cast<array_t<double>*>(values);
        delete static_cast<double*>(defval);
        break;
    case ibis::TEXT:
    case ibis::CATEGORY:
        delete static_cast<std::vector<std::string>*>(values);
        delete static_cast<std::string*>(defval);
        break;
    case ibis::BLOB:
        delete static_cast<std::vector<ibis::opaque>*>(values);
        delete static_cast<ibis::opaque*>(defval);
        break;
    default:
        break;
    }
} // ibis::tafel::column::~column

/// Create a tablex for entering new data.
ibis::tablex* ibis::tablex::create() {
    return new ibis::tafel;
} // ibis::tablex::create

/// Read a file containing the names and types of columns.
/// The content of the file is either the simple list of "name:type" pairs
/// or the more verbose version used in '-part.txt' files.  If it is the
/// plain 'name:type' pair form, the pairs can be either specified one at a
/// time or a group at a time.  This function attempts to read one line at
/// a time and will automatically grow the internal buffer used if the
/// existing buffer is too small to read a long line.  However, it is
/// typically a good idea to keep the lines relatively short so it can be
/// examined manually if necessary.
int ibis::tablex::readNamesAndTypes(const char* filename) {
    if (filename == 0 || *filename == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "tablex::readNamesAndTypes needs a filename to proceed";
        return -1;
    }

    ibis::fileManager::buffer<char> linebuf(MAX_LINE);
    std::ifstream ntfile(filename);
    if (! ntfile) {
        LOGGER(ibis::gVerbose >= 0)
            << "tablex::readNamesAndTypes(" << filename
            << ") failed to open the named file for reading";
        return -2;
    }

    int ret = 0;
    bool more = true;
    bool withHeader = false; // true is encounter begin column
    bool skipHeader = false;
    const char *str, *s1;
    std::string buf2, b2;
    while (more) {
        std::streampos linestart = ntfile.tellg();
        // read a line from the input file, retry if encounter an error
        while (! ntfile.getline(linebuf.address(), linebuf.size())) {
            if (ntfile.eof()) {
                *(linebuf.address()) = 0;
                more = false;
                break;
            }

            const uint32_t nold =
                (linebuf.size() > 0  ? linebuf.size() : MAX_LINE);
            if (nold+nold != linebuf.resize(nold+nold)) {
                LOGGER(ibis::gVerbose >= 0)
                    << "Warning -- tablex::readNamesAndTypes(" << filename
                    << ") failed to allocate linebuf of " << nold+nold
                    << " bytes";
                more = false;
                return -3;
            }
            ntfile.clear(); // clear the error bit
            *(linebuf.address()) = 0;
            if (! ntfile.seekg(linestart, std::ios::beg)) {
                LOGGER(ibis::gVerbose >= 0)
                    << "Warning -- tablex::readNamesAndTypes(" << filename
                    << ") failed to seek to the beginning of a line";
                more = false;
                return -4;
            }
        }

        str = linebuf.address();
        while (*str != 0 && isspace(*str) != 0) ++ str; // skip space
        if (*str == 0 || *str == '#' || (*str == '-' && str[1] == '-')) {
            // do nothing
        }
        else if (skipHeader) { // reading the header
            skipHeader = (stricmp(str, "end header") != 0);
        }
        else if (stricmp(str, "begin header") == 0) {
            withHeader = true;
            skipHeader = true;
        }
        else if ((s1 = strstr(str, "name = ")) != 0) {
            s1 += 7;
            ret = ibis::util::readString(buf2, s1);
        }
        else if ((s1 = strstr(str, "type = ")) != 0) {
            s1 += 7;
            ret = ibis::util::readString(b2, s1);
            if (ret >= 0 && ! b2.empty()) {
                buf2 += ':';
                buf2 += b2;
            }
        }
        else if (stricmp(str, "end column") == 0) {
            if (! buf2.empty()) {
                int ierr = parseNamesAndTypes(buf2.c_str());
                if (ierr > 0)
                    ret += ierr;
            }
        }
        else if (withHeader == false) {
            if (stricmp(str, "begin column") == 0) {
                withHeader  = true;
            }
            else {
                // plain version of name:type pairs
                int ierr = parseNamesAndTypes(str);
                if (ierr >  0)
                    ret += ierr;
            }
        }
    } // while (more)

    LOGGER(ibis::gVerbose > 2)
        << "tablex::readNamesAndTypes(" << filename << ") successfully parsed "
        << ret << " name-type pair" << (ret > 1 ? "s" : "");
    return ret;
} // ibis::tablex::readNamesAndTypes

/// Parse names and data types in string form.
/// A column name must start with an alphabet or a underscore (_); it can be
/// followed by any number of alphanumeric characters (including
/// underscores).  For each built-in data types, the type names recognized
/// are as follows:
/// - ibis::BYTE: byte,
/// - ibis::UBYTE: ubyte, unsigned byte,
/// - ibis::SHORT: short, halfword
/// - ibis::USHORT: ushort, unsigned short,
/// - ibis::INT: int,
/// - ibis::UINT: uint, unsigned int,
/// - ibis::LONG: long,
/// - ibis::ULONG: ulong, unsigned long,
/// - ibis::FLOAT: float, real,
/// - ibis::DOUBLE: double,
/// - ibis::CATEGORY: category, key
/// - ibis::TEXT: text, string
///
/// If it can not find a type, but a valid name is found, then the type is
/// assumed to be int.
///
/// @note Column names are not case-sensitive and all types should be
/// specified in lower case letters.
///
/// Characters following '#' or '--' on a line will be treated as comments
/// and discarded.
int ibis::tablex::parseNamesAndTypes(const char* txt) {
    if (txt == 0 || *txt == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "tablex::parseNamesAndTypes received an empty string";
        return -1;
    }

    int ret = 0;
    const char *str = txt;
    std::string nm, type;
    while (*str != 0) {
        // skip leading space
        while (*str != 0 && isspace(*str)) ++ str;
        // find first alphabet or _
        while (*str != 0) {
            if (*str == '#' || (*str == '-' && str[1] == '-')) return ret;
            else if (*str != '_' && isalpha(*str) == 0) ++ str;
            else break;
        }
        nm.clear();
        while (*str != 0 && (isalnum(*str) != 0 || *str == '_')) {
            nm += *str;
            ++ str;
        }

        if (nm.empty()) return ret;

        // skip comment line and empty line
        while (*str != 0) {
            if (*str == '#' || (*str == '-' && str[1] == '-')) {
                for (++ str; *str != 0; ++ str);
            }
            else if (isalpha(*str) == 0) ++ str;
            else break;
        }
        // fold type to lower case to simplify comparisons
        type.clear();
        while (*str != 0 && isalpha(*str) != 0) {
            type += tolower(*str);
            ++ str;
        }
        if (type.compare("unsigned") == 0 || type.compare("signed") == 0) {
            // read the second word, drop signed, drop spaces
            if (type.compare("signed") == 0)
                type.clear();
            while (*str != 0 && isspace(*str) != 0) ++ str;
            while (*str != 0 && isalnum(*str) != 0) {
                type += tolower(*str);
                ++ str;
            }
        }
        if (type.empty())
            type = 'i';

        LOGGER(ibis::gVerbose > 2)
            << "tablex::parseNamesAndTypes processing name:type pair \"" << nm
            << ':' << type << "\"";

        if (type.compare(0, 8, "unsigned") == 0) {
            // unsigned<no-space>type
            const char *next = type.c_str()+8;
            while (*next != 0 && isspace(*next)) ++ next;
            switch (*(next)) {
            case 'b':
            case 'B':
                addColumn(nm.c_str(), ibis::UBYTE); break;
            case 's':
            case 'S':
                addColumn(nm.c_str(), ibis::USHORT); break;
            case 'i':
            case 'I':
                addColumn(nm.c_str(), ibis::UINT); break;
            case 'l':
            case 'L':
                addColumn(nm.c_str(), ibis::ULONG); break;
            default:
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- tablex::parseNamesAndTypes assumes type \""
                    << type << "\" to mean uint32_t";
                addColumn(nm.c_str(), ibis::UINT); break;
            }
        }
        else if (type[0] == 'u' || type[0] == 'U') {
            // uType
            switch (*(type.c_str()+1)) {
            case 'b':
            case 'B':
                addColumn(nm.c_str(), ibis::UBYTE); break;
            case 's':
            case 'S':
                addColumn(nm.c_str(), ibis::USHORT); break;
            case 'i':
            case 'I':
                addColumn(nm.c_str(), ibis::UINT); break;
            case 'l':
            case 'L':
                addColumn(nm.c_str(), ibis::ULONG); break;
            default:
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- tablex::parseNamesAndTypes assumes type \""
                    << type << "\" to mean uint32_t";
                addColumn(nm.c_str(), ibis::UINT); break;
            }
        }
        else {
            // single-letter types: old unadvertised convention
            switch (type[0]) {
            case 'a':
            case 'A':
                addColumn(nm.c_str(), ibis::UBYTE); break;
            case 'b':
            case 'B':
                if (type[1] == 'l' || type[1] == 'L')
                    addColumn(nm.c_str(), ibis::BLOB);
                else
                    addColumn(nm.c_str(), ibis::BYTE);
                break;
            case 'h':
            case 'H':
                addColumn(nm.c_str(), ibis::SHORT); break;
            case 'g':
            case 'G':
                addColumn(nm.c_str(), ibis::USHORT); break;
            case 'i':
            case 'I':
                addColumn(nm.c_str(), ibis::INT); break;
            case 'l':
            case 'L':
                addColumn(nm.c_str(), ibis::LONG); break;
            case 'v':
            case 'V':
                addColumn(nm.c_str(), ibis::ULONG); break;
            case 'r':
            case 'R':
            case 'f':
            case 'F':
                addColumn(nm.c_str(), ibis::FLOAT); break;
            case 'd':
            case 'D':
                addColumn(nm.c_str(), ibis::DOUBLE); break;
            case 'c':
            case 'C':
            case 'k':
            case 'K':
                addColumn(nm.c_str(), ibis::CATEGORY); break;
            case 't':
            case 'T':
                addColumn(nm.c_str(), ibis::TEXT); break;
            case 'q':
            case 'Q':
                addColumn(nm.c_str(), ibis::BLOB); break;
            case 's':
            case 'S':
                if (type[1] == 't' && type[1] == 'T')
                    addColumn(nm.c_str(), ibis::TEXT);
                else
                    addColumn(nm.c_str(), ibis::SHORT);
                break;
            default:
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- tablex::parseNamesAndTypes assumes type \""
                    << type << "\" to mean int32_t";
                addColumn(nm.c_str(), ibis::INT); break;
            }
        }
        ++ ret;
    } // while (*str != 0)

    LOGGER(ibis::gVerbose > 4)
        << "tablex::parseNamesAndType extracted " << ret
        << " name-type pair" << (ret > 1 ? "s" : "");
    return ret;
} // ibis::tablex::parseNamesAndTypes
