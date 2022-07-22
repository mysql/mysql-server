// File: $id$
// Author: John Wu <John.Wu at ACM.org> Lawrence Berkeley National Laboratory
// Copyright (c) 2007-2016 the Regents of the University of California
//
#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(disable:4786)   // some identifier longer than 256 characters
#endif

#include "tab.h"        // ibis::tabula and ibis::tabele
#include "bord.h"       // ibis::bord
#include "query.h"      // ibis::query
#include "countQuery.h" // ibis::countQuery
#include "bundle.h"     // ibis::bundle
#include "ikeywords.h"  // ibis::keyword::tokenizer
#include "blob.h"       // printing ibis::opaque

#include <iomanip>      // std::setprecision
#include <limits>       // std::numeric_limits
#include <sstream>      // std::ostringstream
#include <typeinfo>     // std::typeid
#include <memory>       // std::unique_ptr
#include <algorithm>    // std::reverse, std::copy

#define FASTBIT_SYNC_WRITE 1

/// Constructor.  The responsibility of freeing the memory pointed by the
/// elements of buf is transferred to this object.
ibis::bord::bord(const char *tn, const char *td, uint64_t nr,
                 ibis::table::bufferArray       &buf,
                 const ibis::table::typeArray   &ct,
                 const ibis::table::stringArray &cn,
                 const ibis::table::stringArray *cdesc,
                 const std::vector<const ibis::dictionary*> *dct)
    : ibis::table(), ibis::part("in-core") {
    nEvents = static_cast<uint32_t>(nr);
    if (nEvents != nr) {
        LOGGER(ibis::gVerbose >= 0)
            << "Error -- bord::ctor can not handle " << nr
            << " rows in an in-memory table";
        throw "Too many rows for an in-memory table" IBIS_FILE_LINE;
    }

    switchTime = time(0);
    if (td != 0 && *td != 0) {
        m_desc = td;
    }
    else if (tn != 0 && *tn != 0) {
        m_desc = tn;
    }
    else {
        char abuf[32];
        ibis::util::secondsToString(switchTime, abuf);
        m_desc = "unnamed in-memory data partition constructed at ";
        m_desc += abuf;
    }
    m_name = ibis::util::strnewdup(tn?tn:ibis::util::shortName(m_desc).c_str());
    name_ = m_name; // make sure the name of part and table are the same
    desc_ = m_desc;

    const uint32_t nc = (cn.size()<=ct.size() ? cn.size() : ct.size());
    for (uint32_t i = 0; i < nc; ++ i) {
        std::string cnm0;
        if (cn[i] == 0 || *(cn[i]) == 0) {
            if (cdesc != 0 && i < cdesc->size()) {
                cnm0 = (*cdesc)[i];
                cnm0 = ibis::util::randName(cnm0);
            }
            else {
                std::ostringstream oss;
                oss << "_" << i;
                cnm0 = oss.str();
            }
        }
        else {
            if (isalpha(*(cn[i])) != 0 || *(cn[i]) == '_')
                cnm0 = *(cn[i]);
            else
                cnm0 = 'A' + (*(cn[i]) % 26);
            for (const char* ptr = cn[i]+1; *ptr != 0; ++ ptr)
                cnm0 += (isalnum(*ptr) ? *ptr : '_');
        }
        if (columns.find(cnm0.c_str()) == columns.end()) { // a new column
            ibis::bord::column *tmp;
            if (cdesc != 0 && cdesc->size() > i)
                tmp = new ibis::bord::column
                    (this, ct[i], cnm0.c_str(), buf[i], (*cdesc)[i]);
            else
                tmp = new ibis::bord::column(this, ct[i], cnm0.c_str(), buf[i]);
            if (dct != 0 && i < dct->size())
                tmp->setDictionary((*dct)[i]);
            columns[tmp->name()] = tmp;
            colorder.push_back(tmp);
        }
        else { // duplicate name
            LOGGER(ibis::gVerbose > 2)
                << "Warning -- bord::ctor found column " << i << " ("
                << cnm0 << ") to be a duplicate, discarding it...";
            // free the buffer because it will not be freed anywhere else
            freeBuffer(buf[i], ct[i]);
        }
        buf[i] = 0;
    }

    amask.set(1, nEvents);
    state = ibis::part::STABLE_STATE;
    if (ibis::gVerbose > 1) {
        ibis::util::logger lg;
        lg() << "Constructed in-memory data partition "
             << (m_name != 0 ? m_name : "<unnamed>");
        if (! m_desc.empty())
            lg() << " -- " << m_desc;
        lg() << " -- with " << nr << " row" << (nr > 1U ? "s" : "") << " and "
             << columns.size() << " column" << (columns.size() > 1U ? "s" : "");
        if (ibis::gVerbose > 4) {
            lg() << "\n";
            dumpNames(lg(), ",\t");
            if (ibis::gVerbose > 6) {
                uint64_t npr = (1ULL << (ibis::gVerbose - 4));
                lg() << "\n";
                dump(lg(), npr, ",\t");
            }
        }
    }
} // ibis::bord::bord

/// Constructor.  Produce a partition from the list of columns.  The number
/// of rows is takens to be the number of elements in the first column.
/// The shape of the arrays is also assumed to be that of the first column.
///
//// @note This function copies the pointers to the columns, it does not
//// copy the columns themselves, therefore the column objects pointed by
//// cols can not be deleted while this object is in use.
ibis::bord::bord(const std::vector<ibis::bord::column*> &cols,
                 uint32_t nr)
    : ibis::part("in-core") {
    if (cols.empty() || cols[0] == 0) return;
    std::ostringstream oss;
    oss << "in-memory data partition from " << cols.size() << " column"
        << (cols.size()>1?"s":"") << ": ";
    oss << cols[0]->name();
    for (unsigned j = 1; j < cols.size(); ++ j)
        oss << ", " << cols[j]->name();
    m_desc = oss.str();
    desc_ = m_desc;
    nEvents = nr;
    name_ = ibis::util::randName(m_desc);
    m_name = ibis::util::strnewdup(name_.c_str());

    switch(cols[0]->type()) {
    default: {
        if (nEvents == 0 && cols[0]->nRows() != 0)
            nEvents = cols[0]->nRows();
        break;}
    case ibis::BYTE: {
        array_t<signed char> *buf =
            static_cast<array_t<signed char>*>(cols[0]->getArray());
        if (nEvents == 0 && buf != 0)
            nEvents = buf->size();
        break;}
    case ibis::UBYTE: {
        array_t<unsigned char> *buf =
            static_cast<array_t<unsigned char>*>(cols[0]->getArray());
        if (nEvents == 0 && buf != 0)
            nEvents = buf->size();
        break;}
    case ibis::SHORT: {
        array_t<int16_t> *buf =
            static_cast<array_t<int16_t>*>(cols[0]->getArray());
        if (nEvents == 0 && buf != 0)
            nEvents = buf->size();
        break;}
    case ibis::USHORT: {
        array_t<uint16_t> *buf =
            static_cast<array_t<uint16_t>*>(cols[0]->getArray());
        if (nEvents == 0 && buf != 0)
            nEvents = buf->size();
        break;}
    case ibis::INT: {
        array_t<int32_t> *buf =
            static_cast<array_t<int32_t>*>(cols[0]->getArray());
        if (nEvents == 0 && buf != 0)
            nEvents = buf->size();
        break;}
    case ibis::UINT: {
        array_t<uint32_t> *buf =
            static_cast<array_t<uint32_t>*>(cols[0]->getArray());
        if (nEvents == 0 && buf != 0)
            nEvents = buf->size();
        break;}
    case ibis::LONG: {
        array_t<int64_t> *buf =
            static_cast<array_t<int64_t>*>(cols[0]->getArray());
        if (nEvents == 0 && buf != 0)
            nEvents = buf->size();
        break;}
    case ibis::ULONG: {
        array_t<uint64_t> *buf =
            static_cast<array_t<uint64_t>*>(cols[0]->getArray());
        if (nEvents == 0 && buf != 0)
            nEvents = buf->size();
        break;}
    case ibis::FLOAT: {
        array_t<float> *buf =
            static_cast<array_t<float>*>(cols[0]->getArray());
        if (nEvents == 0 && buf != 0)
            nEvents = buf->size();
        break;}
    case ibis::DOUBLE: {
        array_t<double> *buf =
            static_cast<array_t<double>*>(cols[0]->getArray());
        if (nEvents == 0 && buf != 0)
            nEvents = buf->size();
        break;}
    }
    LOGGER(nr == 0 && ibis::gVerbose > 6)
        << "Warning -- bord::ctor determines the number of rows ("
        << nEvents << ") in the data partition based on column[0] "
        << cols[0]->name() << " (" << cols[0]->description()
        << ") with type " << ibis::TYPESTRING[(int)cols[0]->type()];

    if (! cols[0]->getMeshShape().empty()) {
        setMeshShape(cols[0]->getMeshShape());
        nEvents = shapeSize[0];
        for (unsigned j = 1; j < shapeSize.size(); ++ j)
            nEvents *= shapeSize[j];
    }
    colorder.reserve(cols.size());

    for (unsigned j = 0; j < cols.size(); ++ j) {
        columnList::const_iterator it = columns.find(cols[j]->name());
        if (it == columns.end()) { // a new column
            ibis::bord::column *tmp = new ibis::bord::column(*cols[j]);
            tmp->partition() = this;
            colorder.push_back(tmp);
            columns[tmp->name()] = tmp;
            if (nEvents == 0)
                nEvents = tmp->nRows();
            LOGGER(ibis::gVerbose > 6)
                << "bord::ctor adding column[" << j << "] " << tmp->name()
                << " (" << tmp->description() << ") to " << m_name;
        }
        else {
            LOGGER(ibis::gVerbose > 6)
                << "Warning -- bord::ctor encountered column[" << j << "] "
                << cols[j]->name() << " (" << cols[j]->description()
                << ") already appeared in " << m_name;
        }
    }

    amask.set(1, nEvents);
    state = ibis::part::STABLE_STATE;
    LOGGER(ibis::gVerbose > 1)
        << "Constructed in-memory data partition "
        << (m_name != 0 ? m_name : "<unnamed>") << " -- " << m_desc
        << " -- with " << columns.size() << " column"
        << (columns.size() > 1U ? "s" : "") << " and " << nEvents
        << " row" << (nEvents>1U ? "s" : "");
} // ibis::bord::bord

/// Constructor.  It produces an empty data partition for storing values to
/// be selected by the select clause.  The reference data partition ref is
/// used to determine the data types.  Use the append function to add data
/// for the actual selected values.
ibis::bord::bord(const char *tn, const char *td,
                 const ibis::selectClause &sc, const ibis::part &ref)
    : ibis::part("in-core") {
    if (td != 0 && *td != 0) {
        m_desc = td;
    }
    else {
        std::ostringstream oss;
        oss << "in-memory data partition for select clause " << sc;
        m_desc = oss.str();
    }
    if (tn != 0 && *tn != 0) {
        m_name = ibis::util::strnewdup(tn);
    }
    else {
        m_name = ibis::util::strnewdup(ibis::util::shortName(m_desc).c_str());
    }
    desc_ = m_desc;
    name_ = m_name;
    const size_t nagg = sc.aggSize();
    for (size_t j = 0; j < nagg; ++ j) {
        const char* cname = sc.aggName(j);
        const ibis::math::term* ctrm = sc.aggExpr(j);
        if (cname == 0 || *cname == 0 || ctrm == 0) continue;
        cname = ibis::part::skipPrefix(cname);

        switch (ctrm->termType()) {
        case ibis::math::UNDEF_TERM:
        case ibis::math::NUMBER:
        case ibis::math::STRING:
            break;
        case ibis::math::VARIABLE: {
            const ibis::math::variable &var =
                *static_cast<const ibis::math::variable*>(ctrm);
            const ibis::column* refcol = ref.getColumn(var.variableName());
            if (refcol == 0) {
                size_t nch = std::strlen(ref.name());
                const char* vname = var.variableName();
                if (0 == strnicmp(ref.name(), vname, nch) &&
                    vname[nch] == '_') {
                    vname += (nch + 1);
                    refcol = ref.getColumn(vname);
                }
            }
            if (*(var.variableName()) == '*') { // special name
                ibis::bord::column *col = new ibis::bord::column
                    (this, ibis::UINT, "*", 0, "count(*)");
                if (col != 0) {
                    columns[col->name()] = col;
                    colorder.push_back(col);
                }
                else {
                    LOGGER(ibis::gVerbose >= 0)
                        << "Error -- bord::ctor failed to allocate column "
                        << cname << " for in-memory partition " << tn;
                    throw "bord::ctor failed to allocate a column"
                        IBIS_FILE_LINE;
                }
            }
            else if (refcol != 0) {
                ibis::TYPE_T t = refcol->type();
                if (refcol->type() == ibis::CATEGORY)
                    t = ibis::UINT;
                ibis::bord::column *col = new ibis::bord::column
                    (this, t, cname, 0, sc.aggName(j));
                if (col != 0) {
                    if (refcol->type() == ibis::CATEGORY) {
                        col->loadIndex();
                        col->setDictionary(static_cast<const ibis::category*>
                                           (refcol)->getDictionary());
                    }
                    else if (refcol->type() == ibis::UINT) {
                        const ibis::bord::column *bc =
                            dynamic_cast<const ibis::bord::column*>(refcol);
                        if (bc != 0)
                            col->setDictionary(bc->getDictionary());
                    }
                    if (var.getDecoration() != 0)
                        col->setTimeFormat(var.getDecoration());
                    columns[col->name()] = col;
                    colorder.push_back(col);
                }
                else {
                    LOGGER(ibis::gVerbose >= 0)
                        << "Error -- bord::ctor failed to allocate column "
                        << cname << " for in-memory partition " << tn;
                    throw "bord::ctor failed to allocate a column"
                        IBIS_FILE_LINE;
                }
            }
            else {
                LOGGER(ibis::gVerbose >= 0)
                    << "Error -- bord::ctor failed to locate column "
                    << var.variableName() << " in data partition "
                    << ref.name();
                throw "bord::ctor failed to locate a needed column"
                    IBIS_FILE_LINE;
            }
            break;}
        default: {
            ibis::bord::column *col = new ibis::bord::column
                (this, ibis::DOUBLE, cname, 0, sc.aggName(j));
            if (col != 0) {
                columns[col->name()] = col;
                colorder.push_back(col);
            }
            else {
                LOGGER(ibis::gVerbose >= 0)
                    << "Error -- bord::ctor failed to allocate column "
                    << cname << " for in-memory partition " << tn;
                throw "bord::ctor failed to allocate a column"
                    IBIS_FILE_LINE;
            }
            break;}
        }
    }

    state = ibis::part::STABLE_STATE;
    LOGGER(ibis::gVerbose > 1)
        << "Constructed in-memory data partition "
        << (m_name != 0 ? m_name : "<unnamed>") << " -- " << m_desc
        << " -- with " << columns.size() << " column"
        << (columns.size() > 1U ? "s" : "");
} // ctor

/// Constructor.  It produces an empty data partition for storing values to
/// be selected by the select clause.  The reference data partition list is
/// used to determine the data types.  For columns, the type is determined
/// by the first data partition in the list.  However, for categorical
/// values it checks to see if all the data partitions have the same
/// dictionary before deciding what type to use.  If the data partitions
/// have the same dictionary, then it uses an integer representation for
/// the column, otherwise it keeps the strings explcitly.  Normally, we
/// would expect the integer reprepresentation to be more compact and more
/// efficient to use.
///
/// @note The list of partitions, ref, can not be empty.
ibis::bord::bord(const char *tn, const char *td,
                 const ibis::selectClause &sc, const ibis::constPartList &ref)
    : ibis::part("in-core") {
    if (ref.empty())
        throw "Can not construct a bord with an empty list of parts"
            IBIS_FILE_LINE;

    if (td != 0 && *td != 0) {
        m_desc = td;
    }
    else {
        std::ostringstream oss;
        oss << "in-memory data partition for select clause " << sc;
        m_desc = oss.str();
    }
    if (tn != 0 && *tn != 0) {
        m_name = ibis::util::strnewdup(tn);
    }
    else {
        m_name = ibis::util::strnewdup(ibis::util::randName(m_desc).c_str());
    }
    desc_ = m_desc;
    name_ = m_name;
    const size_t nagg = sc.aggSize();
    for (size_t j = 0; j < nagg; ++ j) {
        const char* cname = sc.aggName(j);
        const ibis::math::term* ctrm = sc.aggExpr(j);
        if (cname == 0 || *cname == 0 || ctrm == 0) continue;
        cname = ibis::part::skipPrefix(cname);

        switch (ctrm->termType()) {
        case ibis::math::UNDEF_TERM:
        case ibis::math::NUMBER:
        case ibis::math::STRING:
            break;
        case ibis::math::VARIABLE: {
            const ibis::math::variable &var =
                *static_cast<const ibis::math::variable*>(ctrm);
            const char* vname = var.variableName();
            if (*vname == '*') { // special name
                ibis::bord::column *col = new ibis::bord::column
                    (this, ibis::UINT, "*", 0, "count(*)");
                if (col != 0) {
                    columns[col->name()] = col;
                    colorder.push_back(col);
                }
                else {
                    LOGGER(ibis::gVerbose >= 0)
                        << "Error -- bord::ctor failed to allocate column "
                        << cname << " for in-memory partition " << tn;
                    throw "bord::ctor failed to allocate a column"
                        IBIS_FILE_LINE;
                }
            }
            else { // normal name
                const ibis::column* refcol = 0;
                for (unsigned i = 0; refcol == 0 && i < ref.size(); ++ i) {
                    refcol = ref[0]->getColumn(var.variableName());
                    if (refcol == 0) {
                        size_t nch = std::strlen(ref[i]->name());
                        if (0 == strnicmp(ref[i]->name(), vname, nch) &&
                            vname[nch] == '_') {
                            refcol = ref[i]->getColumn(vname+nch+1);
                        }
                    }
                }
                if (refcol != 0) {
                    ibis::TYPE_T t = refcol->type();
                    if (refcol->type() == ibis::CATEGORY) {
                        const ibis::dictionary *dic0 =
                            static_cast<const ibis::category*>(refcol)
                            ->getDictionary();
                        bool samedict = (dic0 != 0);
                        for (unsigned i = 1; samedict && i < ref.size(); ++ i) {
                            const ibis::category *cat1 =
                                dynamic_cast<const ibis::category*>
                                (ref[i]->getColumn(refcol->name()));
                            if (cat1 != 0) {
                                const ibis::dictionary *dic1 =
                                    cat1->getDictionary();
                                samedict = dic0->equal_to(*dic1);
                            }
                        }
                        t = (samedict ? ibis::UINT : ibis::CATEGORY);
                    }
                    ibis::bord::column *col = new ibis::bord::column
                        (this, t, cname, 0, sc.aggName(j));
                    if (col != 0) {
                        if (refcol->type() == ibis::CATEGORY &&
                            t == ibis::UINT) {
                            col->loadIndex();
                            col->setDictionary(refcol->getDictionary());
                        }
                        else if (refcol->type() == ibis::UINT) {
                            col->setDictionary(refcol->getDictionary());
                        }
                        columns[col->name()] = col;
                        colorder.push_back(col);
                    }
                    else {
                        LOGGER(ibis::gVerbose >= 0)
                            << "Error -- bord::ctor failed to allocate column "
                            << cname << " for in-memory partition " << tn;
                        throw "bord::ctor failed to allocate a column"
                            IBIS_FILE_LINE;
                    }
                }
                else {
                    LOGGER(ibis::gVerbose >= 0)
                        << "Error -- bord::ctor failed to locate column "
                        << var.variableName() << " in " << ref.size()
                        << " data partition" << (ref.size() > 1 ? "s" : "");
                    throw "bord::ctor failed to locate a needed column"
                        IBIS_FILE_LINE;
                }
            }
            break;}
        case ibis::math::STRINGFUNCTION1: {
            ibis::bord::column *col = new ibis::bord::column
                (this, ibis::TEXT, cname, 0, sc.aggName(j));
            if (col != 0) {
                columns[col->name()] = col;
                colorder.push_back(col);
            }
            else {
                LOGGER(ibis::gVerbose >= 0)
                    << "Error -- bord::ctor failed to allocate column "
                    << cname << " for in-memory partition " << tn;
                throw "bord::ctor failed to allocate a column" IBIS_FILE_LINE;
            }
            break;}
        default: {
            ibis::bord::column *col = new ibis::bord::column
                (this, ibis::DOUBLE, cname, 0, sc.aggName(j));
            if (col != 0) {
                columns[col->name()] = col;
                colorder.push_back(col);
            }
            else {
                LOGGER(ibis::gVerbose >= 0)
                    << "Error -- bord::ctor failed to allocate column "
                    << cname << " for in-memory partition " << tn;
                throw "bord::ctor failed to allocate a column" IBIS_FILE_LINE;
            }
            break;}
        }
    }

    state = ibis::part::STABLE_STATE;
    LOGGER(ibis::gVerbose > 1)
        << "Constructed in-memory data partition "
        << (m_name != 0 ? m_name : "<unnamed>") << " -- " << m_desc
        << " -- with " << columns.size() << " column"
        << (columns.size() > 1U ? "s" : "");
} // ctor

/// Clear the existing content.
void ibis::bord::clear() {
    LOGGER(ibis::gVerbose > 5 && name_.empty() == false)
        << "bord::clear -- clearing " << ibis::table::name_;
} // ibis::bord::clear

/// @note The pointers returned are pointing to names stored internally.
/// The caller should not attempt to free these pointers.
ibis::table::stringArray ibis::bord::columnNames() const {
    return ibis::part::columnNames();
} // ibis::bord::columnNames

ibis::table::typeArray ibis::bord::columnTypes() const {
    return ibis::part::columnTypes();
} // ibis::bord::columnTypes

int64_t ibis::bord::getColumnAsBytes(const char *cn, char *vals,
                                     uint64_t begin, uint64_t end) const {
    const ibis::bord::column *col =
        dynamic_cast<const ibis::bord::column*>(getColumn(cn));
    if (col == 0)
        return -1;

    if (col->type() != ibis::BYTE && col->type() != ibis::UBYTE)
        return -2;

    const array_t<signed char>* arr =
        static_cast<const array_t<signed char>*>(col->getArray());
    if (arr == 0) return -3;

    uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
    if (end > sz || end == 0)
        end = sz;
    if (begin >= end)
        return 0;

    sz = end - begin;
    memcpy(vals, arr->begin()+begin, sz);
    return sz;
} // ibis::bord::getColumnAsBytes

int64_t
ibis::bord::getColumnAsUBytes(const char *cn, unsigned char *vals,
                              uint64_t begin, uint64_t end) const {
    const ibis::bord::column *col =
        dynamic_cast<const ibis::bord::column*>(getColumn(cn));
    if (col == 0)
        return -1;

    if (col->type() != ibis::BYTE && col->type() != ibis::UBYTE)
        return -2;

    const array_t<unsigned char>* arr =
        static_cast<const array_t<unsigned char>*>(col->getArray());
    if (arr == 0) return -3;

    uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
    if (end == 0 || end > sz)
        end = sz;
    if (begin >= end)
        return 0;

    sz = end - begin;
    memcpy(vals, arr->begin()+begin, sz);
    return sz;
} // ibis::bord::getColumnAsUBytes

int64_t ibis::bord::getColumnAsShorts(const char *cn, int16_t *vals,
                                      uint64_t begin, uint64_t end) const {
    const ibis::bord::column *col =
        dynamic_cast<const ibis::bord::column*>(getColumn(cn));
    if (col == 0)
        return -1;

    if (col->type() == ibis::SHORT || col->type() == ibis::USHORT) {
        const array_t<int16_t>* arr =
            static_cast<const array_t<int16_t>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        memcpy(vals, arr->begin()+begin, sz*2U);
        return sz;
    }
    else if (col->type() == ibis::BYTE) {
        const array_t<signed char>* arr =
            static_cast<const array_t<signed char>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        std::copy(arr->begin()+begin, arr->begin()+end, vals);
        return sz;
    }
    else if (col->type() == ibis::UBYTE) {
        const array_t<unsigned char>* arr =
            static_cast<const array_t<unsigned char>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        std::copy(arr->begin()+begin, arr->begin()+end, vals);
        return sz;
    }
    else {
        return -2;
    }
} // ibis::bord::getColumnAsShorts

int64_t ibis::bord::getColumnAsUShorts(const char *cn, uint16_t *vals,
                                       uint64_t begin, uint64_t end) const {
    const ibis::bord::column *col =
        dynamic_cast<const ibis::bord::column*>(getColumn(cn));
    if (col == 0)
        return -1;

    if (col->type() == ibis::SHORT || col->type() == ibis::USHORT) {
        const array_t<uint16_t>* arr =
            static_cast<const array_t<uint16_t>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        memcpy(vals, arr->begin()+begin, sz*2U);
        return sz;
    }
    else if (col->type() == ibis::BYTE || col->type() == ibis::UBYTE) {
        const array_t<unsigned char>* arr =
            static_cast<const array_t<unsigned char>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        std::copy(arr->begin()+begin, arr->begin()+end, vals);
        return sz;
    }
    else {
        return -2;
    }
} // ibis::bord::getColumnAsUShorts

int64_t ibis::bord::getColumnAsInts(const char *cn, int32_t *vals,
                                    uint64_t begin, uint64_t end) const {
    const ibis::bord::column *col =
        dynamic_cast<const ibis::bord::column*>(getColumn(cn));
    if (col == 0)
        return -1;

    if (col->type() == ibis::INT || col->type() == ibis::UINT) {
        const array_t<int32_t>* arr =
            static_cast<const array_t<int32_t>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        memcpy(vals, arr->begin()+begin, sz*4U);
        return sz;
    }
    else if (col->type() == ibis::SHORT) {
        const array_t<int16_t>* arr =
            static_cast<const array_t<int16_t>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        std::copy(arr->begin()+begin, arr->begin()+end, vals);
        return sz;
    }
    else if (col->type() == ibis::USHORT) {
        const array_t<uint16_t>* arr =
            static_cast<const array_t<uint16_t>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        std::copy(arr->begin()+begin, arr->begin()+end, vals);
        return sz;
    }
    else if (col->type() == ibis::BYTE) {
        const array_t<signed char>* arr =
            static_cast<const array_t<signed char>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        std::copy(arr->begin()+begin, arr->begin()+end, vals);
        return sz;
    }
    else if (col->type() == ibis::UBYTE) {
        const array_t<unsigned char>* arr =
            static_cast<const array_t<unsigned char>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        std::copy(arr->begin()+begin, arr->begin()+end, vals);
        return sz;
    }
    else {
        return -2;
    }
} // ibis::bord::getColumnAsInts

int64_t ibis::bord::getColumnAsUInts(const char *cn, uint32_t *vals,
                                     uint64_t begin, uint64_t end) const {
    const ibis::bord::column *col =
        dynamic_cast<const ibis::bord::column*>(getColumn(cn));
    if (col == 0)
        return -1;

    if (col->type() == ibis::INT || col->type() == ibis::UINT) {
        const array_t<uint32_t>* arr =
            static_cast<const array_t<uint32_t>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        memcpy(vals, arr->begin()+begin, sz*4U);
        return sz;
    }
    else if (col->type() == ibis::SHORT || col->type() == ibis::USHORT) {
        const array_t<uint16_t>* arr =
            static_cast<const array_t<uint16_t>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        std::copy(arr->begin()+begin, arr->begin()+end, vals);
        return sz;
    }
    else if (col->type() == ibis::BYTE || col->type() == ibis::UBYTE) {
        const array_t<unsigned char>* arr =
            static_cast<const array_t<unsigned char>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        std::copy(arr->begin()+begin, arr->begin()+end, vals);
        return sz;
    }
    else {
        return -2;
    }
} // ibis::bord::getColumnAsUInts

int64_t ibis::bord::getColumnAsLongs(const char *cn, int64_t *vals,
                                     uint64_t begin, uint64_t end) const {
    const ibis::bord::column *col =
        dynamic_cast<const ibis::bord::column*>(getColumn(cn));
    if (col == 0)
        return -1;

    if (col->type() == ibis::LONG || col->type() == ibis::ULONG) {
        const array_t<int64_t>* arr =
            static_cast<const array_t<int64_t>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        memcpy(vals, arr->begin()+begin, sz*8U);
        return sz;
    }
    else if (col->type() == ibis::INT) {
        const array_t<int32_t>* arr =
            static_cast<const array_t<int32_t>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        std::copy(arr->begin()+begin, arr->begin()+end, vals);
        return sz;
    }
    else if (col->type() == ibis::UINT) {
        const array_t<uint32_t>* arr =
            static_cast<const array_t<uint32_t>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        std::copy(arr->begin()+begin, arr->begin()+end, vals);
        return sz;
    }
    else if (col->type() == ibis::SHORT) {
        const array_t<int16_t>* arr =
            static_cast<const array_t<int16_t>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        std::copy(arr->begin()+begin, arr->begin()+end, vals);
        return sz;
    }
    else if (col->type() == ibis::USHORT) {
        const array_t<uint16_t>* arr =
            static_cast<const array_t<uint16_t>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || sz > end)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        std::copy(arr->begin()+begin, arr->begin()+end, vals);
        return sz;
    }
    else if (col->type() == ibis::BYTE) {
        const array_t<signed char>* arr =
            static_cast<const array_t<signed char>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        std::copy(arr->begin()+begin, arr->begin()+end, vals);
        return sz;
    }
    else if (col->type() == ibis::UBYTE) {
        const array_t<unsigned char>* arr =
            static_cast<const array_t<unsigned char>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        std::copy(arr->begin()+begin, arr->begin()+end, vals);
        return sz;
    }
    else {
        return -2;
    }
} // ibis::bord::getColumnAsLongs

int64_t ibis::bord::getColumnAsULongs(const char *cn, uint64_t *vals,
                                      uint64_t begin, uint64_t end) const {
    const ibis::bord::column *col =
        dynamic_cast<const ibis::bord::column*>(getColumn(cn));
    if (col == 0)
        return -1;

    if (col->type() == ibis::LONG || col->type() == ibis::ULONG) {
        const array_t<uint64_t>* arr =
            static_cast<const array_t<uint64_t>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        memcpy(vals, arr->begin()+begin, sz*8U);
        return sz;
    }
    else if (col->type() == ibis::INT || col->type() == ibis::UINT) {
        const array_t<uint32_t>* arr =
            static_cast<const array_t<uint32_t>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        std::copy(arr->begin()+begin, arr->begin()+end, vals);
        return sz;
    }
    else if (col->type() == ibis::SHORT || col->type() == ibis::USHORT) {
        const array_t<uint16_t>* arr =
            static_cast<const array_t<uint16_t>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        std::copy(arr->begin()+begin, arr->begin()+end, vals);
        return sz;
    }
    else if (col->type() == ibis::BYTE || col->type() == ibis::UBYTE) {
        const array_t<unsigned char>* arr =
            static_cast<const array_t<unsigned char>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        std::copy(arr->begin()+begin, arr->begin()+end, vals);
        return sz;
    }
    else {
        return -2;
    }
} // ibis::bord::getColumnAsULongs

int64_t ibis::bord::getColumnAsFloats(const char *cn, float *vals,
                                      uint64_t begin, uint64_t end) const {
    const ibis::bord::column *col =
        dynamic_cast<const ibis::bord::column*>(getColumn(cn));
    if (col == 0)
        return -1;

    switch (col->type()) {
    case ibis::FLOAT: {
        const array_t<float>* arr =
            static_cast<const array_t<float>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        memcpy(vals, arr->begin()+begin, sz*4U);
        return sz;
    }
    case ibis::SHORT: {
        const array_t<int16_t>* arr =
            static_cast<const array_t<int16_t>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        std::copy(arr->begin()+begin, arr->begin()+end, vals);
        return sz;
    }
    case ibis::USHORT: {
        const array_t<uint16_t>* arr =
            static_cast<const array_t<uint16_t>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        std::copy(arr->begin()+begin, arr->begin()+end, vals);
        return sz;
    }
    case ibis::BYTE: {
        const array_t<signed char>* arr =
            static_cast<const array_t<signed char>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        std::copy(arr->begin()+begin, arr->begin()+end, vals);
        return sz;
    }
    case ibis::UBYTE: {
        const array_t<unsigned char>* arr =
            static_cast<const array_t<unsigned char>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        std::copy(arr->begin()+begin, arr->begin()+end, vals);
        return sz;
    }
    default:
        break;
    }
    return -2;
} // ibis::bord::getColumnAsFloats

int64_t ibis::bord::getColumnAsDoubles(const char *cn, double *vals,
                                       uint64_t begin, uint64_t end) const {
    const ibis::bord::column *col =
        dynamic_cast<const ibis::bord::column*>(getColumn(cn));
    if (col == 0)
        return -1;

    switch (col->type()) {
    case ibis::DOUBLE: {
        const array_t<double>* arr =
            static_cast<const array_t<double>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        memcpy(vals, arr->begin()+begin, sz*8U);
        return sz;
    }
    case ibis::FLOAT: {
        const array_t<float>* arr =
            static_cast<const array_t<float>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        std::copy(arr->begin()+begin, arr->begin()+end, vals);
        return sz;
    }
    case ibis::INT: {
        const array_t<int32_t>* arr =
            static_cast<const array_t<int32_t>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        std::copy(arr->begin()+begin, arr->begin()+end, vals);
        return sz;
    }
    case ibis::UINT: {
        const array_t<uint32_t>* arr =
            static_cast<const array_t<uint32_t>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        std::copy(arr->begin()+begin, arr->begin()+end, vals);
        return sz;
    }
    case ibis::SHORT: {
        const array_t<int16_t>* arr =
            static_cast<const array_t<int16_t>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        std::copy(arr->begin()+begin, arr->begin()+end, vals);
        return sz;
    }
    case ibis::USHORT: {
        const array_t<uint16_t>* arr =
            static_cast<const array_t<uint16_t>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        std::copy(arr->begin()+begin, arr->begin()+end, vals);
        return sz;
    }
    case ibis::BYTE: {
        const array_t<signed char>* arr =
            static_cast<const array_t<signed char>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        std::copy(arr->begin()+begin, arr->begin()+end, vals);
        return sz;
    }
    case ibis::UBYTE: {
        const array_t<unsigned char>* arr =
            static_cast<const array_t<unsigned char>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        std::copy(arr->begin()+begin, arr->begin()+end, vals);
        return sz;
    }
    default:
        break;
    }
    return -2;
} // ibis::bord::getColumnAsDoubles

int64_t ibis::bord::getColumnAsDoubles(const char* cn,
                                       std::vector<double>& vals,
                                       uint64_t begin, uint64_t end) const {
    const ibis::bord::column *col =
        dynamic_cast<const ibis::bord::column*>(getColumn(cn));
    if (col == 0)
        return -1;

    switch (col->type()) {
    case ibis::DOUBLE: {
        const array_t<double>* arr =
            static_cast<const array_t<double>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        vals.resize(sz);
        std::copy(arr->begin()+begin, arr->begin()+end, vals.begin());
        return sz;
    }
    case ibis::FLOAT: {
        const array_t<float>* arr =
            static_cast<const array_t<float>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        vals.resize(sz);
        std::copy(arr->begin()+begin, arr->begin()+end, vals.begin());
        return sz;
    }
    case ibis::INT: {
        const array_t<int32_t>* arr =
            static_cast<const array_t<int32_t>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        vals.resize(sz);
        std::copy(arr->begin()+begin, arr->begin()+end, vals.begin());
        return sz;
    }
    case ibis::UINT: {
        const array_t<uint32_t>* arr =
            static_cast<const array_t<uint32_t>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        vals.resize(sz);
        std::copy(arr->begin()+begin, arr->begin()+end, vals.begin());
        return sz;
    }
    case ibis::SHORT: {
        const array_t<int16_t>* arr =
            static_cast<const array_t<int16_t>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        vals.resize(sz);
        std::copy(arr->begin()+begin, arr->begin()+end, vals.begin());
        return sz;
    }
    case ibis::USHORT: {
        const array_t<uint16_t>* arr =
            static_cast<const array_t<uint16_t>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        vals.resize(sz);
        std::copy(arr->begin()+begin, arr->begin()+end, vals.begin());
        return sz;
    }
    case ibis::BYTE: {
        const array_t<signed char>* arr =
            static_cast<const array_t<signed char>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        vals.resize(sz);
        std::copy(arr->begin()+begin, arr->begin()+end, vals.begin());
        return sz;
    }
    case ibis::UBYTE: {
        const array_t<unsigned char>* arr =
            static_cast<const array_t<unsigned char>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        vals.resize(sz);
        std::copy(arr->begin()+begin, arr->begin()+end, vals.begin());
        return sz;
    }
    default:
        break;
    }
    return -2;
} // ibis::bord::getColumnAsDoubles

int64_t ibis::bord::getColumnAsStrings(const char *cn,
                                       std::vector<std::string> &vals,
                                       uint64_t begin, uint64_t end) const {
    const ibis::bord::column *col =
        dynamic_cast<ibis::bord::column*>(getColumn(cn));
    if (col == 0)
        return -1;

    switch (col->type()) {
    case ibis::CATEGORY:
    case ibis::TEXT: {
        const std::vector<std::string>* arr =
            static_cast<const std::vector<std::string>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        vals.resize(sz);
        std::copy(arr->begin()+begin, arr->begin()+end, vals.begin());
        return sz;
    }
    case ibis::DOUBLE: {
        const array_t<double> *arr =
            static_cast<const array_t<double>*>(col->getArray());
        if (arr == 0) return -3;

        const uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        vals.reserve(sz);
        for (uint32_t i = begin; i < end; ++ i) {
            std::ostringstream oss;
            oss << (*arr)[i];
            vals.push_back(oss.str());
        }
        return sz;}
    case ibis::FLOAT: {
        const array_t<float> *arr =
            static_cast<const array_t<float>*>(col->getArray());
        if (arr == 0) return -3;

        const uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        vals.reserve(sz);
        for (uint32_t i = begin; i < end; ++ i) {
            std::ostringstream oss;
            oss << (*arr)[i];
            vals.push_back(oss.str());
        }
        return sz;}
    case ibis::LONG: {
        const array_t<int64_t> *arr =
            static_cast<const array_t<int64_t>*>(col->getArray());
        if (arr == 0) return -3;

        const uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        vals.reserve(sz);
        for (uint32_t i = begin; i < end; ++ i) {
            std::ostringstream oss;
            oss << (*arr)[i];
            vals.push_back(oss.str());
        }
        return sz;}
    case ibis::OID:
    case ibis::ULONG: {
        const array_t<uint64_t> *arr =
            static_cast<const array_t<uint64_t>*>(col->getArray());
        if (arr == 0) return -3;

        const uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        vals.reserve(sz);
        for (uint32_t i = begin; i < end; ++ i) {
            std::ostringstream oss;
            oss << (*arr)[i];
            vals.push_back(oss.str());
        }
        return sz;}
    case ibis::INT: {
        const array_t<int32_t> *arr =
            static_cast<const array_t<int32_t>*>(col->getArray());
        if (arr == 0) return -3;

        const uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        vals.reserve(sz);
        for (uint32_t i = begin; i < end; ++ i) {
            std::ostringstream oss;
            oss << (*arr)[i];
            vals.push_back(oss.str());
        }
        return sz;}

    // case ibis::CATEGORY:{
    //  const array_t<uint32_t> *arr =
    //      static_cast<const array_t<uint32_t>*>(col->getArray());
    //  if (arr == 0) return -3;

    //  uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
    //  if (begin >= end)
    //      return 0;

    //  vals.reserve(sz);
    //  const ibis::dictionary* aDic = col->getDictionary();
    //  for (uint32_t i = 0; i < sz; ++ i) {
    //      vals[i] = (*aDic)[(*arr)[i+begin]];
    //  }
    //  return sz;}
    case ibis::UINT: {
        const array_t<uint32_t> *arr =
            static_cast<const array_t<uint32_t>*>(col->getArray());
        if (arr == 0) return -3;

        const uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        vals.reserve(sz);
        for (uint32_t i = begin; i < end; ++ i) {
            if (col->getDictionary() == 0) {
                std::ostringstream oss;
                oss << (*arr)[i];
                vals.push_back(oss.str());
            }
            else if (col->getDictionary()->size() >= (*arr)[i]) {
                vals[i] = col->getDictionary()->operator[]((*arr)[i]);
            }
            else {
                std::ostringstream oss;
                oss << (*arr)[i];
                vals.push_back(oss.str());
            }
        }
        return sz;}
    case ibis::SHORT: {
        const array_t<int16_t> *arr =
            static_cast<const array_t<int16_t>*>(col->getArray());
        if (arr == 0) return -3;

        const uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        vals.reserve(sz);
        for (uint32_t i = begin; i < end; ++ i) {
            std::ostringstream oss;
            oss << (*arr)[i];
            vals.push_back(oss.str());
        }
        return sz;}
    case ibis::USHORT: {
        const array_t<uint16_t> *arr =
            static_cast<const array_t<uint16_t>*>(col->getArray());
        if (arr == 0) return -3;

        const uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        vals.reserve(sz);
        for (uint32_t i = begin; i < end; ++ i) {
            std::ostringstream oss;
            oss << (*arr)[i];
            vals.push_back(oss.str());
        }
        return sz;}
    case ibis::BYTE: {
        const array_t<signed char> *arr =
            static_cast<const array_t<signed char>*>(col->getArray());
        if (arr == 0) return -3;

        const uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        vals.reserve(sz);
        for (uint32_t i = begin; i < end; ++ i) {
            std::ostringstream oss;
            oss << static_cast<int>((*arr)[i]);
            vals.push_back(oss.str());
        }
        return sz;}
    case ibis::UBYTE: {
        const array_t<unsigned char> *arr =
            static_cast<const array_t<unsigned char>*>(col->getArray());
        if (arr == 0) return -3;

        const uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        vals.reserve(sz);
        for (uint32_t i = begin; i < end; ++ i) {
            std::ostringstream oss;
            oss << static_cast<int>((*arr)[i]);
            vals.push_back(oss.str());
        }
        return sz;}
    default:
        break;
    }
    return -2;
} // ibis::bord::getColumnAsStrings

int64_t ibis::bord::getColumnAsOpaques(const char *cn,
                                       std::vector<ibis::opaque> &vals,
                                       uint64_t begin, uint64_t end) const {
    const ibis::bord::column *col =
        dynamic_cast<ibis::bord::column*>(getColumn(cn));
    if (col == 0)
        return -1;

    switch (col->type()) {
    case ibis::BLOB: {
        const std::vector<ibis::opaque>* arr =
            static_cast<const std::vector<ibis::opaque>*>(col->getArray());
        if (arr == 0) return -3;
        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        vals.resize(sz);
        std::copy(arr->begin()+begin, arr->begin()+end, vals.begin());
        return sz;}
    case ibis::CATEGORY:
    case ibis::TEXT: {
        const std::vector<std::string>* arr =
            static_cast<const std::vector<std::string>*>(col->getArray());
        if (arr == 0) return -3;

        uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        sz = end - begin;
        vals.resize(sz);
        for (size_t j = 0; j < sz; ++ j)
            vals[j].copy((*arr)[j+begin].data(), (*arr)[j+begin].size());
        return sz;
    }
    case ibis::DOUBLE: {
        const array_t<double> *arr =
            static_cast<const array_t<double>*>(col->getArray());
        if (arr == 0) return -3;

        const uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        vals.resize(sz);
        for (uint32_t i = 0; i < sz; ++ i) {
            vals[i].copy((const char*)&((*arr)[i+begin]), sizeof(double));
        }
        return sz;}
    case ibis::FLOAT: {
        const array_t<float> *arr =
            static_cast<const array_t<float>*>(col->getArray());
        if (arr == 0) return -3;

        const uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        vals.resize(sz);
        for (uint32_t i = 0; i < sz; ++ i) {
            vals[i].copy((const char*)&((*arr)[i+begin]), sizeof(float));
        }
        return sz;}
    case ibis::LONG: {
        const array_t<int64_t> *arr =
            static_cast<const array_t<int64_t>*>(col->getArray());
        if (arr == 0) return -3;

        const uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        vals.resize(sz);
        for (uint32_t i = 0; i < sz; ++ i) {
            vals[i].copy((const char*)&((*arr)[i+begin]), sizeof(int64_t));
        }
        return sz;}
    case ibis::OID:
    case ibis::ULONG: {
        const array_t<uint64_t> *arr =
            static_cast<const array_t<uint64_t>*>(col->getArray());
        if (arr == 0) return -3;

        const uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        vals.resize(sz);
        for (uint32_t i = 0; i < sz; ++ i) {
            vals[i].copy((const char*)&((*arr)[i+begin]), sizeof(uint64_t));
        }
        return sz;}
    case ibis::INT: {
        const array_t<int32_t> *arr =
            static_cast<const array_t<int32_t>*>(col->getArray());
        if (arr == 0) return -3;

        const uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        vals.resize(sz);
        for (uint32_t i = 0; i < sz; ++ i) {
            vals[i].copy((const char*)&((*arr)[i+begin]), sizeof(int32_t));
        }
        return sz;}
    case ibis::UINT: {
        const array_t<uint32_t> *arr =
            static_cast<const array_t<uint32_t>*>(col->getArray());
        if (arr == 0) return -3;

        const uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        vals.resize(sz);
        for (uint32_t i = 0; i < sz; ++ i) {
            if (col->getDictionary() == 0) {
                vals[i].copy((const char*)&((*arr)[i+begin]), sizeof(uint32_t));
            }
            else if (col->getDictionary()->size() >= (*arr)[i]) {
                const char* str = col->getDictionary()->operator[]((*arr)[i]);
                vals[i].copy(str, std::strlen(str));
            }
            else {
                vals[i].copy((const char*)&((*arr)[i+begin]), sizeof(uint32_t));
            }
        }
        return sz;}
    case ibis::SHORT: {
        const array_t<int16_t> *arr =
            static_cast<const array_t<int16_t>*>(col->getArray());
        if (arr == 0) return -3;

        const uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        vals.resize(sz);
        for (uint32_t i = 0; i < sz; ++ i) {
            vals[i].copy((const char*)&((*arr)[i+begin]), sizeof(int16_t));
        }
        return sz;}
    case ibis::USHORT: {
        const array_t<uint16_t> *arr =
            static_cast<const array_t<uint16_t>*>(col->getArray());
        if (arr == 0) return -3;

        const uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        vals.resize(sz);
        for (uint32_t i = 0; i < sz; ++ i) {
            vals[i].copy((const char*)&((*arr)[i+begin]), sizeof(uint16_t));
        }
        return sz;}
    case ibis::BYTE: {
        const array_t<signed char> *arr =
            static_cast<const array_t<signed char>*>(col->getArray());
        if (arr == 0) return -3;

        const uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        vals.resize(sz);
        for (uint32_t i = 0; i < sz; ++ i) {
            vals[i].copy((const char*)&((*arr)[i+begin]), sizeof(char));
        }
        return sz;}
    case ibis::UBYTE: {
        const array_t<unsigned char> *arr =
            static_cast<const array_t<unsigned char>*>(col->getArray());
        if (arr == 0) return -3;

        const uint32_t sz = (nEvents <= arr->size() ? nEvents : arr->size());
        if (end == 0 || end > sz)
            end = sz;
        if (begin >= end)
            return 0;

        vals.resize(sz);
        for (uint32_t i = 0; i < sz; ++ i) {
            vals[i].copy((const char*)&((*arr)[i+begin]),
                         sizeof(unsigned char));
        }
        return sz;}
    default:
        break;
    }
    return -2;
} // ibis::bord::getColumnAsOpaques

double ibis::bord::getColumnMin(const char* cn) const {
    return getActualMin(cn);
} // ibis::bord::getColumnMin

double ibis::bord::getColumnMax(const char* cn) const {
    return getActualMax(cn);
} // ibis::bord::getColumnMax

long ibis::bord::getHistogram(const char *constraints,
                              const char *cname,
                              double begin, double end, double stride,
                              std::vector<uint32_t>& counts) const {
    if (sizeof(uint32_t) == sizeof(uint32_t)) {
        return get1DDistribution
            (constraints, cname, begin, end, stride,
             reinterpret_cast<std::vector<uint32_t>&>(counts));
    }
    else {
        std::vector<uint32_t> tmp;
        long ierr = get1DDistribution(constraints, cname, begin, end,
                                      stride, tmp);
        if (ierr >= 0) {
            counts.resize(tmp.size());
            std::copy(tmp.begin(), tmp.end(), counts.begin());
        }
        return ierr;
    }
} // ibis::bord::getHistogram

long ibis::bord::getHistogram2D(const char *constraints,
                                const char *cname1,
                                double begin1, double end1, double stride1,
                                const char* cname2,
                                double begin2, double end2, double stride2,
                                std::vector<uint32_t>& counts) const {
    if (sizeof(uint32_t) == sizeof(uint32_t)) {
        return get2DDistribution
            (constraints, cname1, begin1, end1, stride1,
             cname2, begin2, end2, stride2,
             reinterpret_cast<std::vector<uint32_t>&>(counts));
    }
    else {
        std::vector<uint32_t> tmp;
        long ierr = get2DDistribution
            (constraints, cname1, begin1, end1, stride1,
             cname2, begin2, end2, stride2, tmp);
        if (ierr >= 0) {
            counts.resize(tmp.size());
            std::copy(tmp.begin(), tmp.end(), counts.begin());
        }
        return ierr;
    }
} // ibis::bord::getHistogram2D

long ibis::bord::getHistogram3D(const char *constraints,
                                const char *cname1,
                                double begin1, double end1, double stride1,
                                const char *cname2,
                                double begin2, double end2, double stride2,
                                const char *cname3,
                                double begin3, double end3, double stride3,
                                std::vector<uint32_t>& counts) const {
    if (sizeof(uint32_t) == sizeof(uint32_t)) {
        return get3DDistribution
            (constraints, cname1, begin1, end1, stride1,
             cname2, begin2, end2, stride2,
             cname3, begin3, end3, stride3,
             reinterpret_cast<std::vector<uint32_t>&>(counts));
    }
    else {
        std::vector<uint32_t> tmp;
        long ierr = get3DDistribution
            (constraints, cname1, begin1, end1, stride1,
             cname2, begin2, end2, stride2,
             cname3, begin3, end3, stride3, tmp);
        if (ierr >= 0) {
            counts.resize(tmp.size());
            std::copy(tmp.begin(), tmp.end(), counts.begin());
        }
        return ierr;
    }
} // ibis::bord::getHistogram3D

void ibis::bord::estimate(const char *cond,
                          uint64_t &nmin, uint64_t &nmax) const {
    nmin = 0;
    nmax = nEvents;
    ibis::countQuery q;
    int ierr = q.setWhereClause(cond);
    if (ierr < 0) return;

    ierr = q.setPartition(this);
    if (ierr < 0) return;

    ierr = q.estimate();
    if (ierr >= 0) {
        nmin = q.getMinNumHits();
        nmax = q.getMaxNumHits();
    }
} // ibis::bord::estimate

void ibis::bord::estimate(const ibis::qExpr *cond,
                          uint64_t &nmin, uint64_t &nmax) const {
    nmin = 0;
    nmax = nEvents;
    ibis::countQuery q;
    int ierr = q.setWhereClause(cond);
    if (ierr < 0) return;

    ierr = q.setPartition(this);
    if (ierr < 0) return;

    ierr = q.estimate();
    if (ierr >= 0) {
        nmin = q.getMinNumHits();
        nmax = q.getMaxNumHits();
    }
} // ibis::bord::estimate

ibis::table* ibis::bord::select(const char *sel, const char *cond) const {
    ibis::constPartList prts(1);
    prts[0] = this;
    return ibis::table::select(prts, sel, cond);
} // ibis::bord::select

/// Compute the number of hits.
int64_t ibis::bord::computeHits(const char *cond) const {
    int64_t res = -1;
    ibis::query q(ibis::util::userName(), this);
    q.setWhereClause(cond);
    res = q.evaluate();
    if (res >= 0)
        res = q.getNumHits();
    return res;
} // ibis::bord::computeHits

int ibis::bord::getPartitions(ibis::constPartList &lst) const {
    lst.resize(1);
    lst[0] = this;
    return 1;
} // ibis::bord::getPartitions

void ibis::bord::describe(std::ostream& out) const {
    out << "Table (in memory) " << name_ << " (" << m_desc
        << ") contsists of " << columns.size() << " column"
        << (columns.size()>1 ? "s" : "") << " and " << nEvents
        << " row" << (nEvents>1 ? "s" : "");
    if (colorder.empty()) {
        for (ibis::part::columnList::const_iterator it = columns.begin();
             it != columns.end(); ++ it) {
            out << "\n" << (*it).first << "\t"
                << ibis::TYPESTRING[(int)(*it).second->type()];
            if (static_cast<const ibis::bord::column*>(it->second)
                ->getDictionary() != 0)
                out << " (dictionary size: "
                    << static_cast<const ibis::bord::column*>(it->second)
                    ->getDictionary()->size() << ')';
            if (it->second->description() != 0 && ibis::gVerbose > 1)
                out << "\t" << it->second->description();
        }
    }
    else if (colorder.size() == columns.size()) {
        for (uint32_t i = 0; i < columns.size(); ++ i) {
            out << "\n" << colorder[i]->name() << "\t"
                << ibis::TYPESTRING[(int)colorder[i]->type()];
            if (static_cast<const ibis::bord::column*>(colorder[i])
                ->getDictionary() != 0)
                out << " (dictionary size: "
                    << static_cast<const ibis::bord::column*>(colorder[i])
                    ->getDictionary()->size() << ')';
            if (colorder[i]->description() != 0 && ibis::gVerbose > 1)
                out << "\t" << colorder[i]->description();
        }
    }
    else {
        std::set<const char*, ibis::lessi> names;
        for (ibis::part::columnList::const_iterator it = columns.begin();
             it != columns.end(); ++ it)
            names.insert((*it).first);
        for (uint32_t i = 0; i < colorder.size(); ++ i) {
            out << "\n" << colorder[i]->name() << "\t"
                << ibis::TYPESTRING[(int)colorder[i]->type()];
            if (static_cast<const ibis::bord::column*>(colorder[i])
                ->getDictionary() != 0)
                out << " (dictionary size: "
                    << static_cast<const ibis::bord::column*>(colorder[i])
                    ->getDictionary()->size() << ')';
            if (colorder[i]->description() != 0 && ibis::gVerbose > 1)
                out << "\t" << colorder[i]->description();
            names.erase(colorder[i]->name());
        }
        for (std::set<const char*, ibis::lessi>::const_iterator it =
                 names.begin(); it != names.end(); ++ it) {
            ibis::part::columnList::const_iterator cit = columns.find(*it);
            out << "\n" << (*cit).first << "\t"
                << ibis::TYPESTRING[(int)(*cit).second->type()];
            if (static_cast<const ibis::bord::column*>(cit->second)
                ->getDictionary() != 0)
                out << " (dictionary size: "
                    << static_cast<const ibis::bord::column*>(cit->second)
                    ->getDictionary()->size() << ')';
            if (cit->second->description() != 0 && ibis::gVerbose > 1)
                out << "\t" << (*cit).second->description();
        }
    }
    out << std::endl;
} // ibis::bord::describe

void ibis::bord::dumpNames(std::ostream& out, const char* del) const {
    if (columns.empty()) return;
    if (del == 0) del = ", ";

    if (colorder.empty()) {
        for (ibis::part::columnList::const_iterator it = columns.begin();
             it != columns.end(); ++ it) {
            if (it != columns.begin())
                out << del;
            out << (*it).first;
            if (static_cast<const ibis::bord::column*>(it->second)
                ->getDictionary() != 0 && ibis::gVerbose > 2)
                out << " (k)";
        }
    }
    else if (colorder.size() == columns.size()) {
        out << colorder[0]->name();
        if (static_cast<const ibis::bord::column*>(colorder[0])
            ->getDictionary() != 0 && ibis::gVerbose > 2)
            out << " (k)";
        for (uint32_t i = 1; i < columns.size(); ++ i) {
            out << del << colorder[i]->name();
            if (static_cast<const ibis::bord::column*>(colorder[i])
                ->getDictionary() != 0 && ibis::gVerbose > 2)
                out << " (k)";
        }
    }
    else {
        std::set<const char*, ibis::lessi> names;
        for (ibis::part::columnList::const_iterator it = columns.begin();
             it != columns.end(); ++ it)
            names.insert((*it).first);
        for (uint32_t i = 0; i < colorder.size(); ++ i) {
            if (i > 0)
                out << del;
            out << colorder[i]->name();
            if (static_cast<const ibis::bord::column*>(colorder[i])
                ->getDictionary() != 0 && ibis::gVerbose > 2)
                out << " (k)";
            names.erase(colorder[i]->name());
        }
        for (std::set<const char*, ibis::lessi>::const_iterator it =
                 names.begin(); it != names.end(); ++ it) {
            out << del << *it;
            ibis::part::columnList::const_iterator cit = columns.find(*it);
            if (static_cast<const ibis::bord::column*>(cit->second)
                ->getDictionary() != 0 && ibis::gVerbose > 2)
                out << " (k)";
        }
    }
    out << std::endl;
} // ibis::bord::dumpNames

int ibis::bord::dump(std::ostream& out, const char* del) const {
        if (std::strcmp(del, "JSON")==0)
            return dumpJSON(out, nEvents);
        else
            return dump(out, nEvents, del);
} // ibis::bord::dump

/// Dump out the first nr rows in JSON format.
int ibis::bord::dumpJSON(std::ostream& out, uint64_t nr) const {
    const uint32_t ncol = columns.size();
    if (ncol == 0 || nr == 0) return 0;

    std::vector<const ibis::bord::column*> clist;
    if (colorder.empty()) { // alphabetic ordering
        for (ibis::part::columnList::const_iterator it = columns.begin();
             it != columns.end(); ++ it) {
            const ibis::bord::column* col =
                dynamic_cast<const ibis::bord::column*>((*it).second);
            if (col != 0)
                clist.push_back(col);
        }
    }
    else if (colorder.size() == ncol) { // use external order
        for (uint32_t i = 0; i < ncol; ++ i) {
            const ibis::bord::column* col =
                dynamic_cast<const ibis::bord::column*>(colorder[i]);
            if (col != 0)
                clist.push_back(col);
        }
    }
    else { // externally specified ones are ordered first
        std::set<const char*, ibis::lessi> names;
        for (ibis::part::columnList::const_iterator it = columns.begin();
             it != columns.end(); ++ it)
            names.insert((*it).first);
        for (uint32_t i = 0; i < colorder.size(); ++ i) {
            const ibis::bord::column* col =
                dynamic_cast<const ibis::bord::column*>(colorder[i]);
            if (col != 0) {
                clist.push_back(col);
                names.erase(col->name());
            }
        }
        for (std::set<const char*, ibis::lessi>::const_iterator it =
                 names.begin();
             it != names.end(); ++ it) {
            ibis::part::columnList::const_iterator cit = columns.find(*it);
            const ibis::bord::column* col =
                dynamic_cast<const ibis::bord::column*>((*cit).second);
            if (col != 0)
                clist.push_back(col);
        }
    }
    if (clist.size() < ncol) return -3;

    int ierr = 0;
    // print the first row with error checking
    out << "[[";
    ierr = clist[0]->dump(out, 0U);
    if (ierr < 0) return ierr;
    for (uint32_t j = 1; j < ncol; ++ j) {
        out << ",";
        ierr = clist[j]->dump(out, 0U);
        if (ierr < 0) return ierr;
    }
    out << "]";
    if (! out) return -4;
    // print the remaining rows without checking the return values from
    // functions called
    if (nr > nEvents) nr = nEvents;
    for (uint32_t i = 1; i < nr; ++ i) {
        out << ",[";
        (void) clist[0]->dump(out, i);
        for (uint32_t j = 1; j < ncol; ++ j) {
            out << ",";
            (void) clist[j]->dump(out, i);
        }
        out << "]";
    }
    if (! out)
        ierr = -4;
    out << "]";
    return ierr;
} // ibis::bord::dumpJSON

/**
   Print the first nr rows of the data to the given output stream.

   The return values:
   @code
   0  -- normal (successful) completion
   -1  -- no data in-memory
   -2  -- unknown data type
   -3  -- some columns not ibis::bord::column (not in-memory)
   -4  -- error in the output stream
   @endcode
*/
int ibis::bord::dump(std::ostream& out, uint64_t nr,
                     const char* del) const {
    const uint32_t ncol = columns.size();
    if (ncol == 0 || nr == 0) return 0;
    if (del == 0) del = ",";

    std::vector<const ibis::bord::column*> clist;
    if (colorder.empty()) { // alphabetic ordering
        for (ibis::part::columnList::const_iterator it = columns.begin();
             it != columns.end(); ++ it) {
            const ibis::bord::column* col =
                dynamic_cast<const ibis::bord::column*>((*it).second);
            if (col != 0)
                clist.push_back(col);
        }
    }
    else if (colorder.size() == ncol) { // use external order
        for (uint32_t i = 0; i < ncol; ++ i) {
            const ibis::bord::column* col =
                dynamic_cast<const ibis::bord::column*>(colorder[i]);
            if (col != 0)
                clist.push_back(col);
        }
    }
    else { // externally specified ones are ordered first
        std::set<const char*, ibis::lessi> names;
        for (ibis::part::columnList::const_iterator it = columns.begin();
             it != columns.end(); ++ it)
            names.insert((*it).first);
        for (uint32_t i = 0; i < colorder.size(); ++ i) {
            const ibis::bord::column* col =
                dynamic_cast<const ibis::bord::column*>(colorder[i]);
            if (col != 0) {
                clist.push_back(col);
                names.erase(col->name());
            }
        }
        for (std::set<const char*, ibis::lessi>::const_iterator it =
                 names.begin();
             it != names.end(); ++ it) {
            ibis::part::columnList::const_iterator cit = columns.find(*it);
            const ibis::bord::column* col =
                dynamic_cast<const ibis::bord::column*>((*cit).second);
            if (col != 0)
                clist.push_back(col);
        }
    }
    if (clist.size() < ncol) return -3;

    int ierr = 0;
    // print the first row with error checking
    ierr = clist[0]->dump(out, 0U);
    if (ierr < 0) return ierr;
    for (uint32_t j = 1; j < ncol; ++ j) {
        out << del;
        ierr = clist[j]->dump(out, 0U);
        if (ierr < 0) return ierr;
    }
    out << "\n";
    if (! out) return -4;
    // print the remaining rows without checking the return values from
    // functions called
    if (nr > nEvents) nr = nEvents;
    for (uint32_t i = 1; i < nr; ++ i) {
        (void) clist[0]->dump(out, i);
        for (uint32_t j = 1; j < ncol; ++ j) {
            out << del;
            (void) clist[j]->dump(out, i);
        }
        out << "\n";
    }
    if (! out)
        ierr = -4;
    return ierr;
} // ibis::bord::dump

/// Print nr rows starting with row offset.  Note that the row number
/// starts with 0, i.e., the first row is row 0.
int ibis::bord::dump(std::ostream& out, uint64_t offset, uint64_t nr,
                     const char* del) const {
    const uint32_t ncol = columns.size();
    if (ncol == 0 || nr == 0 || offset >= nEvents) return 0;
    if (del == 0) del = ",";

    std::vector<const ibis::bord::column*> clist;
    if (colorder.empty()) { // alphabetic ordering
        for (ibis::part::columnList::const_iterator it = columns.begin();
             it != columns.end(); ++ it) {
            const ibis::bord::column* col =
                dynamic_cast<const ibis::bord::column*>((*it).second);
            if (col != 0)
                clist.push_back(col);
        }
    }
    else if (colorder.size() == ncol) { // use external order
        for (uint32_t i = 0; i < ncol; ++ i) {
            const ibis::bord::column* col =
                dynamic_cast<const ibis::bord::column*>(colorder[i]);
            if (col != 0)
                clist.push_back(col);
        }
    }
    else { // externally specified ones are ordered first
        std::set<const char*, ibis::lessi> names;
        for (ibis::part::columnList::const_iterator it = columns.begin();
             it != columns.end(); ++ it)
            names.insert((*it).first);
        for (uint32_t i = 0; i < colorder.size(); ++ i) {
            const ibis::bord::column* col =
                dynamic_cast<const ibis::bord::column*>(colorder[i]);
            if (col != 0) {
                clist.push_back(col);
                names.erase(col->name());
            }
        }
        for (std::set<const char*, ibis::lessi>::const_iterator it =
                 names.begin();
             it != names.end(); ++ it) {
            ibis::part::columnList::const_iterator cit = columns.find(*it);
            const ibis::bord::column* col =
                dynamic_cast<const ibis::bord::column*>((*cit).second);
            if (col != 0)
                clist.push_back(col);
        }
    }
    if (clist.size() < ncol) return -3;

    int ierr = 0;
    // print row offset with error checking
    ierr = clist[0]->dump(out, offset);
    if (ierr < 0) return ierr;
    for (uint32_t j = 1; j < ncol; ++ j) {
        out << del;
        ierr = clist[j]->dump(out, offset);
        if (ierr < 0) return ierr;
    }
    out << "\n";
    if (! out) return -4;
    // print the remaining rows without checking the return values from
    // functions called
    nr += offset;
    if (nr > nEvents) nr = nEvents;
    for (uint32_t i = offset+1; i < nr; ++ i) {
        (void) clist[0]->dump(out, i);
        for (uint32_t j = 1; j < ncol; ++ j) {
            out << del;
            (void) clist[j]->dump(out, i);
        }
        out << "\n";
    }
    if (! out)
        ierr = -4;
    return ierr;
} // ibis::bord::dump

int ibis::bord::column::dump(std::ostream& out, uint32_t i) const {
    int ierr = -1;
    if (buffer == 0) {
        out << "(no data in memory)";
        return ierr;
    }

    switch (m_type) {
    case ibis::BYTE: {
        const array_t<signed char>* vals =
            static_cast<const array_t<signed char>*>(buffer);
        if (i < vals->size()) {
            if (m_utscribe != 0)
                (*m_utscribe)(out, (int64_t)((*vals)[i]));
            else
                out << (int)((*vals)[i]);
            ierr = 0;
        }
        else {
            ierr = -2;
        }
        break;}
    case ibis::UBYTE: {
        const array_t<unsigned char>* vals =
            static_cast<const array_t<unsigned char>*>(buffer);
        if (i < vals->size()) {
            if (m_utscribe != 0)
                (*m_utscribe)(out, (int64_t)((*vals)[i]));
            else
                out << (unsigned)((*vals)[i]);
            ierr = 0;
        }
        else {
            ierr = -2;
        }
        break;}
    case ibis::SHORT: {
        const array_t<int16_t>* vals =
            static_cast<const array_t<int16_t>*>(buffer);
        if (i < vals->size()) {
            if (m_utscribe != 0)
                (*m_utscribe)(out, (int64_t)((*vals)[i]));
            else
                out << (*vals)[i];
            ierr = 0;
        }
        else {
            ierr = -2;
        }
        break;}
    case ibis::USHORT: {
        const array_t<uint16_t>* vals =
            static_cast<const array_t<uint16_t>*>(buffer);
        if (i < vals->size()) {
            if (m_utscribe != 0)
                (*m_utscribe)(out, (int64_t)((*vals)[i]));
            else
                out << (*vals)[i];
            ierr = 0;
        }
        else {
            ierr = -2;
        }
        break;}
    case ibis::INT: {
        const array_t<int32_t>* vals =
            static_cast<const array_t<int32_t>*>(buffer);
        if (i < vals->size()) {
            if (m_utscribe != 0)
                (*m_utscribe)(out, (int64_t)((*vals)[i]));
            else
                out << (*vals)[i];
            ierr = 0;
        }
        else {
            ierr = -2;
        }
        break;}
    case ibis::UINT: {
        const array_t<uint32_t>* vals =
            static_cast<const array_t<uint32_t>*>(buffer);
        if (i < vals->size()) {
            if (m_utscribe != 0) {
                (*m_utscribe)(out, (int64_t)((*vals)[i]));
            }
            else if (dic == 0) {
                out << (*vals)[i];
            }
            else if ((*vals)[i] >= dic->size()) {
                out << (*vals)[i];
            }
            else {
                out << '"' << (*dic)[(*vals)[i]] << '"';
            }
            ierr = 0;
        }
        else {
            ierr = -2;
        }
        break;}
    case ibis::LONG: {
        const array_t<int64_t>* vals =
            static_cast<const array_t<int64_t>*>(buffer);
        if (i < vals->size()) {
            if (m_utscribe != 0)
                (*m_utscribe)(out, ((*vals)[i]));
            else
                out << (*vals)[i];
            ierr = 0;
        }
        else {
            ierr = -2;
        }
        break;}
    case ibis::ULONG: {
        const array_t<uint64_t>* vals =
            static_cast<const array_t<uint64_t>*>(buffer);
        if (i < vals->size()) {
            if (m_utscribe != 0)
                (*m_utscribe)(out, (int64_t)((*vals)[i]));
            else
                out << (*vals)[i];
            ierr = 0;
        }
        else {
            ierr = -2;
        }
        break;}
    case ibis::FLOAT: {
        const array_t<float>* vals =
            static_cast<const array_t<float>*>(buffer);
        if (i < vals->size()) {
            if (m_utscribe != 0)
                (*m_utscribe)(out, (int64_t)((*vals)[i]));
            else
                out << std::setprecision(7) << (*vals)[i];
            ierr = 0;
        }
        else {
            ierr = -2;
        }
        break;}
    case ibis::DOUBLE: {
        const array_t<double>* vals =
            static_cast<const array_t<double>*>(buffer);
        if (i < vals->size()) {
            if (m_utscribe != 0)
                (*m_utscribe)(out, (int64_t)((*vals)[i]));
            else
                out << std::setprecision(15) << (*vals)[i];
            ierr = 0;
        }
        else {
            ierr = -2;
        }
        break;}
    case ibis::OID: {
        const array_t<rid_t>* vals =
            static_cast<const array_t<rid_t>*>(buffer);
        if (i < vals->size()) {
            out << (*vals)[i];
            ierr = 0;
        }
        else {
            ierr = -2;
        }
        break;}
    case ibis::BLOB: {
        const std::vector<ibis::opaque>* vals =
            static_cast<const std::vector<ibis::opaque>*>(buffer);
        if (i < vals->size()) {
            out << (*vals)[i];
            ierr = 0;
        }
        else {
            ierr = -2;
        }
        break;}
    case ibis::CATEGORY:
    case ibis::TEXT: {
        std::string tmp;
        getString(i, tmp);
        out << '"' << tmp << '"';
        ierr = 0;
        break;}
    default: {
        ierr = -2;
        break;}
    }
    return ierr;
} // ibis::bord::column::dump

/// Write the content of partition into the specified directory dir.  The
/// directory dir must be writable.  If the second and third arguments are
/// valid, the output data will use them as the name and the description of
/// the data partition.
int ibis::bord::backup(const char* dir, const char* tname,
                       const char* tdesc) const {
    if (dir == 0 || *dir == 0) return -1;
    int ierr = ibis::util::makeDir(dir);
    if (ierr < 0) return ierr;

    time_t currtime = time(0); // current time
    ibis::horometer timer;
    if (ibis::gVerbose > 0)
        timer.start();
    ibis::bitvector msk0, msk1;
    msk0.set(1, nEvents);

    columnList extra; // columns in dir, but not in this data partition
    std::string oldnm, olddesc, oldidx, oldtags;
    ibis::bitvector::word_t nold = 0;
    { // read the existing meta data file in the directory dir
        ibis::part tmp(dir, static_cast<const char*>(0));
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

            oldtags = tmp.metaTags();
            if (tmp.indexSpec() != 0 && *(tmp.indexSpec()) != 0)
                oldidx = tmp.indexSpec();
            unsigned nconflicts = 0;
            for (unsigned it = 0; it < tmp.nColumns(); ++ it) {
                const ibis::column &old = *(tmp.getColumn(it));
                const ibis::column *col = tmp.getColumn(old.name());
                if (col == 0) { // found an extra
                    ibis::column *ctmp = new ibis::column(old);
                    extra[ctmp->name()] = ctmp;
                }
                else { // check for conflicting types
                    bool conflict = false;
                    switch (col->type()) {
                    default:
                        conflict = (old.type() != col->type());
                        break;
                    case ibis::BYTE:
                    case ibis::UBYTE:
                        conflict = (old.type() != ibis::BYTE &&
                                    old.type() != ibis::UBYTE);
                        break;
                    case ibis::SHORT:
                    case ibis::USHORT:
                        conflict = (old.type() != ibis::SHORT &&
                                    old.type() != ibis::USHORT);
                        break;
                    case ibis::INT:
                    case ibis::UINT:
                        conflict = (old.type() != ibis::INT &&
                                    old.type() != ibis::UINT);
                        break;
                    case ibis::LONG:
                    case ibis::ULONG:
                        conflict = (old.type() != ibis::LONG &&
                                    old.type() != ibis::ULONG);
                        break;
                    }
                    if (conflict) {
                        ++ nconflicts;
                        LOGGER(ibis::gVerbose > 0)
                            << "Warning -- bord::backup(" << dir
                            << ") column " << old.name()
                            << " has conflicting types specified, previously "
                            << ibis::TYPESTRING[(int)old.type()]
                            << ", currently "
                            << ibis::TYPESTRING[(int)col->type()];
                    }
                }
            }
            if (nconflicts > 0) {
                LOGGER(ibis::gVerbose >= 0)
                    << "bord::backup(" << dir
                    << ") can not proceed because " << nconflicts
                    << " column" << (nconflicts>1 ? "s" : "")
                    << " contains conflicting type specifications";
                return -2;
            }
            else {
                LOGGER(ibis::gVerbose > 2)
                    << "bord::backup(" << dir
                    << ") found existing data partition named "
                    << tmp.name() << " with " << tmp.nRows()
                    << " row" << (tmp.nRows()>1 ? "s" : "")
                    << " and " << tmp.nColumns() << " column"
                    << (tmp.nColumns()>1?"s":"")
                    << ", will append " << nEvents << " new row"
                    << (nEvents>1 ? "s" : "");
            }
            tmp.emptyCache(); // empty cached content from dir
        }
    }

    if (tname == 0 || *tname == 0) {
        if (! oldnm.empty()) {
            tname = oldnm.c_str();
        }
        else {
            if (! isalpha(*m_name))  {
                const char* str = strrchr(dir, FASTBIT_DIRSEP);
                if (str != 0)
                    ++ str;
                else
                    str = dir;
                oldnm = str;
                oldnm += m_name;
                tname = oldnm.c_str();
            }
            else {
                tname = m_name;
            }
        }
    }
    if (tdesc == 0 || *tdesc == 0) {
        if (! olddesc.empty())
            tdesc = olddesc.c_str();
        else
            tdesc = m_desc.c_str();
    }
    LOGGER(ibis::gVerbose > 1)
        << "bord::backup starting to write " << nEvents << " row"
        << (nEvents>1?"s":"") << " and " << columns.size() << " column"
        << (columns.size()>1?"s":"") << " to " << dir << " as data partition "
        << tname << " to " << dir;
    char stamp[28];
    ibis::util::secondsToString(currtime, stamp);
    std::string mdfile = dir;
    mdfile += FASTBIT_DIRSEP;
    mdfile += "-part.txt";
    std::ofstream md(mdfile.c_str());
    if (! md) {
        LOGGER(ibis::gVerbose > 0)
            << "bord::backup(" << dir << ") failed to open metadata file "
            "\"-part.txt\"";
        return -3; // metadata file not ready
    }

    md << "# meta data for data partition " << tname
       << " written by bord::backup on " << stamp << "\n\n"
       << "BEGIN HEADER\nName = " << tname << "\nDescription = "
       << tdesc << "\nNumber_of_rows = " << nEvents+nold
       << "\nNumber_of_columns = " << columns.size()+extra.size()
       << "\nTimestamp = " << currtime;
    if (idxstr != 0 && *idxstr != 0) {
        md << "\nindex = " << idxstr;
    }
    else { // try to find the default index specification
        std::string idxkey = "ibis.";
        idxkey += tname;
        idxkey += ".index";
        const char* str = ibis::gParameters()[idxkey.c_str()];
        if (str != 0 && *str != 0)
            md << "\nindex = " << str;
    }
    md << "\nEND HEADER\n";

    for (columnList::const_iterator it = columns.begin();
         it != columns.end(); ++ it) {
        const column& col = *(static_cast<column*>((*it).second));
        std::string cnm = dir;
        cnm += FASTBIT_DIRSEP;
        cnm += (*it).first;
        if (col.type() == ibis::UINT && col.getDictionary() != 0) {
            std::string dict = cnm;
            dict += ".dic";
            ierr = col.getDictionary()->write(dict.c_str());
            LOGGER(ierr < 0 && ibis::gVerbose > 0)
                << "Warning -- bord::backup failed to write a dictionary to "
                "file \"" << dict << "\", ierr = " << ierr;
            cnm += ".int";
        }
        int fdes = UnixOpen(cnm.c_str(), OPEN_WRITEADD, OPEN_FILEMODE);
        if (fdes < 0) {
            LOGGER(ibis::gVerbose >= 0)
                << "bord::backup(" << dir << ") failed to open file "
                << cnm << " for writing";
            return -4;
        }
        IBIS_BLOCK_GUARD(UnixClose, fdes);
#if defined(_WIN32) && defined(_MSC_VER)
        (void)_setmode(fdes, _O_BINARY);
#endif
        LOGGER(ibis::gVerbose > 2)
            << "bord::backup opened file " << cnm
            << " to write data for column " << (*it).first;
        std::string mskfile = cnm; // mask file name
        mskfile += ".msk";
        msk1.read(mskfile.c_str());

        switch (col.type()) {
        case ibis::BYTE: {
            std::unique_ptr< array_t<signed char> >
                values(col.selectBytes(msk0));
            if (values.get() != 0) {
                ierr = ibis::part::writeColumn
                    (fdes, nold, nEvents, 0, *values,
                     (signed char)0x7F, msk1, msk0);
            }
            else {
                ierr = -4;
            }
            break;}
        case ibis::UBYTE: {
            std::unique_ptr< array_t<unsigned char> >
                values(col.selectUBytes(msk0));
            if (values.get() != 0)
                ierr = ibis::part::writeColumn
                    (fdes, nold, nEvents, 0, *values,
                     (unsigned char)0xFF, msk1, msk0);
            else
                ierr = -4;
            break;}
        case ibis::SHORT: {
            std::unique_ptr< array_t<int16_t> > values(col.selectShorts(msk0));
            if (values.get() != 0)
                ierr = ibis::part::writeColumn
                    (fdes, nold, nEvents, 0, *values,
                     (int16_t)0x7FFF, msk1, msk0);
            else
                ierr = -4;
            break;}
        case ibis::USHORT: {
            std::unique_ptr< array_t<uint16_t> > values(col.selectUShorts(msk0));
            if (values.get() != 0)
                ierr = ibis::part::writeColumn
                    (fdes, nold, nEvents, 0, *values,
                     (uint16_t)0xFFFF, msk1, msk0);
            else
                ierr = -4;
            break;}
        case ibis::INT: {
            std::unique_ptr< array_t<int32_t> > values(col.selectInts(msk0));
            if (values.get() != 0)
                ierr = ibis::part::writeColumn
                    (fdes, nold, nEvents, 0, *values,
                     (int32_t)0x7FFFFFFF, msk1, msk0);
            else
                ierr = -4;
            break;}
        case ibis::UINT: {
            std::unique_ptr< array_t<uint32_t> > values(col.selectUInts(msk0));
            if (values.get() != 0) {
                ierr = ibis::part::writeColumn
                    (fdes, nold, nEvents, 0, *values,
                     (uint32_t)0xFFFFFFFF, msk1, msk0);
            }
            else
                ierr = -4;
            break;}
        case ibis::LONG: {
            std::unique_ptr< array_t<int64_t> > values(col.selectLongs(msk0));
            if (values.get() != 0)
                ierr = ibis::part::writeColumn<int64_t>
                    (fdes, nold, nEvents, 0, *values,
                     0x7FFFFFFFFFFFFFFFLL, msk1, msk0);
            else
                ierr = -4;
            break;}
        case ibis::OID:
        case ibis::ULONG: {
            std::unique_ptr< array_t<uint64_t> > values(col.selectULongs(msk0));
            if (values.get() != 0)
                ierr = ibis::part::writeColumn<uint64_t>
                    (fdes, nold, nEvents, 0, *values,
                     0xFFFFFFFFFFFFFFFFULL, msk1, msk0);
            else
                ierr = -4;
            break;}
        case ibis::FLOAT: {
            std::unique_ptr< array_t<float> > values(col.selectFloats(msk0));
            if (values.get() != 0)
                ierr = ibis::part::writeColumn
                    (fdes, nold, nEvents, 0, *values, FASTBIT_FLOAT_NULL,
                     msk1, msk0);
            else
                ierr = -4;
            break;}
        case ibis::DOUBLE: {
            std::unique_ptr< array_t<double> > values(col.selectDoubles(msk0));
            if (values.get() != 0)
                ierr = ibis::part::writeColumn
                    (fdes, nold, nEvents, 0, *values, FASTBIT_DOUBLE_NULL,
                     msk1, msk0);
            else
                ierr = -4;
            break;}
        case ibis::TEXT:
        case ibis::CATEGORY: {
            std::unique_ptr< std::vector<std::string> >
                values(col.selectStrings(msk0));
            if (values.get() != 0)
                ierr = ibis::part::writeStrings
                    (cnm.c_str(), nold, nEvents, 0, *values, msk1, msk0);
            else
                ierr =-4;
            break;}
        case ibis::BLOB: {
            std::unique_ptr< std::vector<ibis::opaque> >
                values(col.selectOpaques(msk0));
            std::string spname = cnm;
            spname += ".sp";
            int sdes = UnixOpen(spname.c_str(), OPEN_READWRITE, OPEN_FILEMODE);
            if (sdes < 0) {
                LOGGER(ibis::gVerbose >= 0)
                    << "bord::backup(" << dir << ") failed to open file "
                    << spname << " for writing the starting positions";
                return -5;
            }
            IBIS_BLOCK_GUARD(UnixClose, sdes);
#if defined(_WIN32) && defined(_MSC_VER)
            (void)_setmode(sdes, _O_BINARY);
#endif
            if (values.get() != 0) {
                ierr = ibis::part::writeOpaques
                    (fdes, sdes, nold, nEvents, 0,
                     *static_cast< const std::vector<ibis::opaque>* >
                     (values.get()),
                     msk1, msk0);
            }
            else {
                ierr = -4;
            }
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
                << "bord::backup(" << dir << ") failed to write column "
                << (*it).first << " (type " << ibis::TYPESTRING[(int)col.type()]
                << ") to " << cnm;
            return ierr;
        }

        LOGGER(msk1.cnt() != msk1.size() && ibis::gVerbose > 1)
            << "Warning -- bord::backup(" << dir
            << ") expected msk1 to contain only 1s for column " << col.name()
            << ", but it has only " << msk1.cnt() << " out of " << msk1.size();
        remove(mskfile.c_str());

        md << "\nBegin Column\nname = " << (*it).first << "\ndata_type = ";
        if (col.type() != ibis::UINT || col.getDictionary() == 0)
            md << ibis::TYPESTRING[(int) col.type()];
        else
            md << "CATEGORY";
        if (col.indexSpec() != 0 && *(col.indexSpec()) != 0) {
            md << "\nindex = " << col.indexSpec();
        }
        else if (col.type() == ibis::BLOB) {
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
    }

    for (columnList::const_iterator it = extra.begin();
         it != extra.end(); ++ it) {
        const ibis::column &col = *(it->second);
        md << "\nBegin Column\nname = " << it->first
           << "\ndata_type = " << ibis::TYPESTRING[(int) col.type()];
        if (col.indexSpec() != 0 && *(col.indexSpec()) != 0)
            md << "\nindex = " << col.indexSpec();
        delete (it->second);
    }
    md.close(); // close the file
    extra.clear();
    ibis::fileManager::instance().flushDir(dir);
    if (ibis::gVerbose > 0) {
        timer.stop();
        ibis::util::logger()()
            << "bord::backup completed writing partition "
            << tname << " (" << tdesc << ") with " << columns.size()
            << " column" << (columns.size()>1 ? "s" : "") << " and "
            << nEvents << " row" << (nEvents>1 ? "s" : "") << ") to " << dir
            << " using " << timer.CPUTime() << " sec(CPU), "
            << timer.realTime() << " sec(elapsed)";
    }

    return 0;
} // ibis::bord::backup

ibis::table* ibis::bord::groupby(const char* keys) const {
    ibis::selectClause sel(keys);
    return groupby(sel);
} // ibis::bord::groupby

ibis::table*
ibis::bord::groupby(const ibis::table::stringArray& keys) const {
    ibis::selectClause sel(keys);
    return groupby(sel);
} // ibis::bord::groupby

/// The actual function to perform the group by operation.
///
/// @note The input argument can only contain column names and supported
/// aggregation functions with column names arguments.  No futher
/// arithmetic operations are allowed!
ibis::table*
ibis::bord::xgroupby(const ibis::selectClause& sel) const {
    if (sel.empty())
        return 0;

    std::string td = "Select ";
    td += *sel;
    td += " From ";
    td += m_name;
    td += " (";
    td += m_desc;
    td += ')';
    LOGGER(ibis::gVerbose > 3) << "bord::groupby -- \"" << td << '"';
    readLock lock(this, td.c_str());
    std::string tn = ibis::util::randName(td);
    if (nEvents == 0)
        return new ibis::tabula(tn.c_str(), td.c_str(), nEvents);

    // create bundle
    std::unique_ptr<ibis::bundle> bdl(ibis::bundle::create(*this, sel));
    if (bdl.get() == 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- bord::groupby failed to create "
            "bundle for \"" << *sel << "\" from in-memory data";

        return 0;
    }
    const uint32_t nr = bdl->size();
    if (nr == 0) {
        if (ibis::gVerbose > 0) {
            ibis::util::logger lg;
            lg() << "Warning -- bord::groupby(";
            sel.print(lg());
            lg() << ") produced no answer on a table with nRows = "
                 << nEvents;
        }
        return 0;
    }

    const uint32_t nc1 = sel.aggSize();
    if (nc1 == 0)
        return new ibis::tabula(tn.c_str(), td.c_str(), nr);

    // can we finish this in one run?
    const ibis::selectClause::mathTerms& xtms(sel.getTerms());
    bool onerun = (xtms.size() == sel.aggSize());
    for (unsigned j = 0; j < xtms.size() && onerun; ++ j)
        onerun = (xtms[j]->termType() == ibis::math::VARIABLE);

    // prepare the types and values for the new table
    std::vector<const ibis::dictionary*> dct(nc1, 0);
    std::vector<std::string> nms(nc1), des(nc1);
    ibis::table::stringArray  nmc(nc1), dec(nc1);
    ibis::table::bufferArray  buf(nc1, 0);
    ibis::table::typeArray    tps(nc1, ibis::UNKNOWN_TYPE);
    ibis::util::guard gbuf
        = ibis::util::makeGuard(ibis::table::freeBuffers,
                                ibis::util::ref(buf),
                                ibis::util::ref(tps));
    uint32_t jbdl = 0;
#ifdef FASTBIT_ALWAYS_OUTPUT_COUNTS
    bool countstar = false;
#endif
    for (uint32_t i = 0; i < nc1; ++ i) {
        void *bptr = 0;
        nms[i] = (onerun ? sel.termName(i) : sel.aggName(i));
        nmc[i] = nms[i].c_str();
        des[i] = sel.aggDescription(i);
        dec[i] = des[i].c_str();
        bool iscstar = (sel.aggExpr(i)->termType() == ibis::math::VARIABLE &&
                        sel.getAggregator(i) == ibis::selectClause::CNT);
        if (iscstar)
            iscstar = (*(static_cast<const ibis::math::variable*>
                         (sel.aggExpr(i))->variableName()));
        if (iscstar) {
#ifdef FASTBIT_ALWAYS_OUTPUT_COUNTS
            countstar = true;
#endif
            array_t<uint32_t>* cnts = new array_t<uint32_t>;
            bdl->rowCounts(*cnts);
            tps[i] = ibis::UINT;
            buf[i] = cnts;
            if (! onerun) {
                std::ostringstream oss;
                oss << "__" << std::hex << i;
                nms[i] = oss.str();
                nmc[i] = nms[i].c_str();
            }
            continue;
        }
        else if (jbdl < bdl->width()) {
            tps[i] = bdl->columnType(jbdl);
            bptr = bdl->columnArray(jbdl);
            ++ jbdl;
        }
        else {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- bord::groupby exhausted all columns in bundle, "
                "not enough information to construct the result table";
            return 0;
        }

        if (bptr == 0) {
            buf[i] = 0;
            continue;
        }

        switch (tps[i]) {
        case ibis::BYTE:
            buf[i] = new array_t<signed char>
                (* static_cast<const array_t<signed char>*>(bptr));
            break;
        case ibis::UBYTE:
            buf[i] = new array_t<unsigned char>
                (* static_cast<const array_t<unsigned char>*>(bptr));
            break;
        case ibis::SHORT:
            buf[i] = new array_t<int16_t>
                (* static_cast<const array_t<int16_t>*>(bptr));
            break;
        case ibis::USHORT: {
            buf[i] = new array_t<uint16_t>
                (* static_cast<const array_t<uint16_t>*>(bptr));
            break;}
        case ibis::INT:
            buf[i] = new array_t<int32_t>
                (* static_cast<const array_t<int32_t>*>(bptr));
            break;
        case ibis::UINT: {
            buf[i] = new array_t<uint32_t>
                (* static_cast<const array_t<uint32_t>*>(bptr));
            const ibis::bord::column *bc =
                dynamic_cast<const ibis::bord::column*>
                (bdl->columnPointer(jbdl));
            if (bc != 0)
                dct[i] = bc->getDictionary();
            break;}
        case ibis::LONG:
            buf[i] = new array_t<int64_t>
                (* static_cast<const array_t<int64_t>*>(bptr));
            break;
        case ibis::ULONG:
            buf[i] = new array_t<uint64_t>
                (* static_cast<const array_t<uint64_t>*>(bptr));
            break;
        case ibis::FLOAT:
            buf[i] = new array_t<float>
                (* static_cast<const array_t<float>*>(bptr));
            break;
        case ibis::DOUBLE:
            buf[i] = new array_t<double>
                (* static_cast<const array_t<double>*>(bptr));
            break;
        case ibis::CATEGORY:
        case ibis::TEXT: {
            std::vector<std::string> &bstr =
                * static_cast<std::vector<std::string>*>(bptr);
            std::vector<std::string> *tmp =
                new std::vector<std::string>(bstr.size());
            for (uint32_t j = 0; j < bstr.size(); ++ j)
                (*tmp)[j] = bstr[j];
            buf[i] = tmp;
            break;}
        // case ibis::CATEGORY: {
        //     buf[i] = new array_t<uint32_t>
        //      (* static_cast<const array_t<uint32_t>*>(bptr));
        //     dct[i] = static_cast<const ibis::category*>
        //      (bdl->columnPointer(jbdl))->getDictionary();
        //     break;}
        default:
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- " << td << " can not process column "
                << nms[i] << " (" << des[i] << ") of type "
                << ibis::TYPESTRING[static_cast<int>(tps[i])];
            buf[i] = 0;
            break;
        }
    }
#ifdef FASTBIT_ALWAYS_OUTPUT_COUNTS
    if (! countstar) {// if count(*) is not already there, add it
        array_t<uint32_t>* cnts = new array_t<uint32_t>;
        bdl->rowCounts(*cnts);
        nmc.push_back("count0");
        dec.push_back("COUNT(*)");
        tps.push_back(ibis::UINT);
        buf.push_back(cnts);
    }
#endif
    std::unique_ptr<ibis::bord>
        brd1(new ibis::bord(tn.c_str(), td.c_str(), nr, buf, tps, nmc, &dec,
                            &dct));
    if (brd1.get() == 0)
        return 0;
    if (ibis::gVerbose > 2) {
        ibis::util::logger lg;
        lg() << "bord::groupby -- creates an in-memory data partition with "
             << brd1->nRows() << " row" << (brd1->nRows()>1?"s":"")
             << " and " << brd1->nColumns() << " column"
             << (brd1->nColumns()>1?"s":"");
        if (ibis::gVerbose > 4) {
            lg() << "\n";
            brd1->describe(lg());
        }
    }

    delete bdl.release(); // free the bundle
    if (onerun) {
        gbuf.dismiss();
        return brd1.release();
    }

    // not quite done yet, evaluate the top-level arithmetic expressions
    ibis::bitvector msk;
    const unsigned nc2 = xtms.size();
    msk.set(1, brd1->nRows());
    nms.resize(nc2);
    des.resize(nc2);
    nmc.resize(nc2);
    dec.resize(nc2);
    buf.resize(nc2);
    tps.resize(nc2);
    dct.resize(nc2);
    for (unsigned j = 0; j < nc2; ++ j) {
        tps[j] = ibis::UNKNOWN_TYPE;
        buf[j] = 0;
        dct[j] = 0;
    }

    for (unsigned j = 0; j < nc2; ++ j) {
        nms[j] = sel.termName(j);
        nmc[j] = nms[j].c_str();
        const ibis::math::term* tm = xtms[j];
        if (tm == 0 || tm->termType() == ibis::math::UNDEF_TERM) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bord[" << name_ << "]::groupby(" << sel
                << ") failed to process term " << j << " named \"" << nms[j]
                << '"';
            return 0;
        }

        std::ostringstream oss;
        oss << *tm;
        des[j] = oss.str();
        dec[j] = des[j].c_str();

        switch (tm->termType()) {
        case ibis::math::NUMBER: {
            tps[j] = ibis::DOUBLE;
            buf[j] = new ibis::array_t<double>(nr, tm->eval());
            break;}
        case ibis::math::STRING: {
            tps[j] = ibis::CATEGORY;
            const std::string val = (const char*)*
                (static_cast<const ibis::math::literal*>(tm));
            buf[j] = new std::vector<std::string>(nr, val);
            break;}
        case ibis::math::VARIABLE: {
            const char* var =
                static_cast<const ibis::math::variable*>(tm)->variableName();
            brd1->copyColumn(var, tps[j], buf[j], dct[j]);
            break;}
        case ibis::math::STRINGFUNCTION1: {
            tps[j] = ibis::TEXT;
            buf[j] = new std::vector<std::string>;
            brd1->calculate
                (*static_cast<const ibis::math::stringFunction1*>(tm), msk,
                 *static_cast<std::vector<std::string>*>(buf[j]));
            break;}
        default: {
            tps[j] = ibis::DOUBLE;
            buf[j] = new ibis::array_t<double>;
            brd1->calculate(*tm, msk, *static_cast<array_t<double>*>(buf[j]));
            break;}
        }
    }

    std::unique_ptr<ibis::table>
        brd2(new ibis::bord(tn.c_str(), td.c_str(), nr, buf, tps, nmc, &dec,
                            &dct));
    if (brd2.get() != 0)
        // buf has been successfully transferred to the new table object
        gbuf.dismiss();
    return brd2.release();
} // ibis::bord::xgroupby

ibis::table*
ibis::bord::groupby(const ibis::selectClause& sel) const {
    std::unique_ptr<ibis::bord> brd1;
    if (sel.needsEval(*this)) {
        std::unique_ptr<ibis::bord> brd0(evaluateTerms(sel, 0));
        if (brd0.get() != 0)
            brd1.reset(groupbya(*brd0, sel));
        else
            brd1.reset(ibis::bord::groupbya(*this, sel));
    }
    else {
        brd1.reset(ibis::bord::groupbya(*this, sel));
    }
    if (brd1.get() == 0)
        return 0;

    // can we finish this in one run?
    const ibis::selectClause::mathTerms& xtms(sel.getTerms());
    bool onerun = (xtms.size() == sel.aggSize());
    for (unsigned j = 0; j < xtms.size() && onerun; ++ j)
        onerun = (xtms[j]->termType() == ibis::math::VARIABLE);
    if (onerun) {
        brd1->renameColumns(sel);
        if (ibis::gVerbose > 2) {
            ibis::util::logger lg;
            lg() << "bord::groupby -- completed ";
            brd1->describe(lg());
        }

        return brd1.release();
    }

    std::unique_ptr<ibis::bord> brd2(ibis::bord::groupbyc(*brd1, sel));
    if (ibis::gVerbose > 2) {
        ibis::util::logger lg;
        lg() << "bord::groupby -- completed ";
        brd2->describe(lg());
    }

    return brd2.release();
} // ibis::bord::groupby

/// Perform the aggregation operations specified in the select clause.  If
/// there is any further computation on the aggregated values, the user
/// needs to call groupbyc to complete those operations.  This separation
/// allows one to possibly conduct group by operations on multiple data
/// partitions one partition at a time, which reduces the memory
/// requirement.
ibis::bord*
ibis::bord::groupbya(const ibis::bord& src, const ibis::selectClause& sel) {
    if (sel.empty() || sel.aggSize() == 0 || src.nRows() == 0)
        return 0;

    std::string td = "Select ";
    td += sel.aggDescription(0);
    for (unsigned j = 1; j < sel.aggSize(); ++ j) {
        td += ", ";
        td += sel.aggDescription(j);
    }
    td += " From ";
    td += src.part::name();
    LOGGER(ibis::gVerbose > 3)
        << "bord::groupbya -- processing aggregations for \"" << td << '"';
    std::string tn = ibis::util::randName(td);

    readLock lock(&src, td.c_str());
    // create bundle -- perform the actual aggregation operations here
    std::unique_ptr<ibis::bundle> bdl(ibis::bundle::create(src, sel));
    if (bdl.get() == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bord::groupbya failed to create bundle for \""
            << td << "\"";
        return 0;
    }
    const uint32_t nr = bdl->size();
    if (nr == 0) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- bord::groupbya produced no answer for " << td;
        return 0;
    }

    // prepare the types and values for the new table
    const uint32_t nca = sel.aggSize();
    std::vector<const ibis::dictionary*> dct(nca, 0);
    std::vector<std::string> nms(nca), des(nca);
    ibis::table::stringArray  nmc(nca), dec(nca);
    ibis::table::bufferArray  buf(nca, 0);
    ibis::table::typeArray    tps(nca, ibis::UNKNOWN_TYPE);
    IBIS_BLOCK_GUARD(ibis::table::freeBuffers, ibis::util::ref(buf),
                     ibis::util::ref(tps));
    uint32_t jbdl = 0;
#ifdef FASTBIT_ALWAYS_OUTPUT_COUNTS
    bool countstar = false;
#endif
    // loop through each column of the aggregation results to reformat the
    // in-memory data for an ibis::bord object
    for (uint32_t i = 0; i < nca; ++ i) {
        void *bptr = 0;
        nms[i] = sel.aggName(i);
        nmc[i] = nms[i].c_str();
        des[i] = sel.aggDescription(i);
        dec[i] = des[i].c_str();
        const ibis::column *refcol = (jbdl < bdl->width() ?
                                      bdl->columnPointer(jbdl) : 0);
        bool iscstar = (sel.aggExpr(i)->termType() == ibis::math::VARIABLE &&
                        sel.getAggregator(i) == ibis::selectClause::CNT);
        if (iscstar)
            iscstar = (*(static_cast<const ibis::math::variable*>
                         (sel.aggExpr(i))->variableName()) == '*');
        if (iscstar) {
#ifdef FASTBIT_ALWAYS_OUTPUT_COUNTS
            countstar = true;
#endif
            array_t<uint32_t>* cnts = new array_t<uint32_t>;
            bdl->rowCounts(*cnts);
            tps[i] = ibis::UINT;
            buf[i] = cnts;
            continue;
        }
        else if (jbdl < bdl->width()) {
            tps[i] = bdl->columnType(jbdl);
            bptr = bdl->columnArray(jbdl);
            ++ jbdl;
        }
        else {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- bord::groupbya exhausted columns in bundle, "
                "not enough information to construct the result table";
            return 0;
        }

        if (bptr == 0) {
            buf[i] = 0;
            continue;
        }

        switch (tps[i]) {
        case ibis::BYTE:
            buf[i] = new array_t<signed char>
                (* static_cast<const array_t<signed char>*>(bptr));
            break;
        case ibis::UBYTE:
            buf[i] = new array_t<unsigned char>
                (* static_cast<const array_t<unsigned char>*>(bptr));
            break;
        case ibis::SHORT:
            buf[i] = new array_t<int16_t>
                (* static_cast<const array_t<int16_t>*>(bptr));
            break;
        case ibis::USHORT:
            buf[i] = new array_t<uint16_t>
                (* static_cast<const array_t<uint16_t>*>(bptr));
            break;
        case ibis::INT:
            buf[i] = new array_t<int32_t>
                (* static_cast<const array_t<int32_t>*>(bptr));
            break;
        case ibis::UINT: {
            buf[i] = new array_t<uint32_t>
                (* static_cast<const array_t<uint32_t>*>(bptr));
            dct[i] = refcol->getDictionary();
            break;}
        case ibis::LONG:
            buf[i] = new array_t<int64_t>
                (* static_cast<const array_t<int64_t>*>(bptr));
            break;
        case ibis::ULONG:
            buf[i] = new array_t<uint64_t>
                (* static_cast<const array_t<uint64_t>*>(bptr));
            break;
        case ibis::FLOAT:
            buf[i] = new array_t<float>
                (* static_cast<const array_t<float>*>(bptr));
            break;
        case ibis::DOUBLE:
            buf[i] = new array_t<double>
                (* static_cast<const array_t<double>*>(bptr));
            break;
        case ibis::CATEGORY:
        case ibis::TEXT: {
            std::vector<std::string> &bstr =
                * static_cast<std::vector<std::string>*>(bptr);
            std::vector<std::string> *tmp =
                new std::vector<std::string>(bstr.size());
            for (uint32_t j = 0; j < bstr.size(); ++ j)
                (*tmp)[j] = bstr[j];
            buf[i] = tmp;
            break;}
        // case ibis::CATEGORY: { // stored as UINT, copy pointer to dictionary
        //     buf[i] = new array_t<uint32_t>
        //      (* static_cast<const array_t<uint32_t>*>(bptr));
        //     dct[i] = static_cast<const ibis::category*>
        //      (refcol)->getDictionary();
        //     break;}
        default:
            LOGGER(ibis::gVerbose >= 0)
                << "Warning -- " << td << " can not process column "
                << nms[i] << " (" << des[i] << ") of type "
                << ibis::TYPESTRING[static_cast<int>(tps[i])];
            buf[i] = 0;
            break;
        }
    }
#ifdef FASTBIT_ALWAYS_OUTPUT_COUNTS
    if (! countstar) {// if count(*) is not already there, add it
        array_t<uint32_t>* cnts = new array_t<uint32_t>;
        bdl->rowCounts(*cnts);
        nmc.push_back("count0");
        dec.push_back("COUNT(*)");
        tps.push_back(ibis::UINT);
        buf.push_back(cnts);
    }
#endif
    return (new ibis::bord(tn.c_str(), td.c_str(), nr, buf, tps, nmc, &dec,
                           &dct));
} // ibis::bord::groupbya

/// The function to perform the final computations specified by the select
/// clause.  This is to be called after all the aggregation operations have
/// been performed.  The objective to separate the aggregation operations
/// and the final arithmetic operations is to allow the aggregation
/// operations to be performed on different data partitions separately.
///
/// @note The incoming ibis::bord object must be the output from
/// ibis::bord::groupbya.
ibis::bord*
ibis::bord::groupbyc(const ibis::bord& src, const ibis::selectClause& sel) {
    if (sel.empty())
        return 0;

    const unsigned nr = src.nRows();
    const unsigned ncx = sel.numTerms();
    if (nr == 0 || ncx == 0)
        return 0;

    std::string td = "Select ";
    td += *sel;
    td += " From ";
    td += src.part::name();
    td += " (";
    td += src.part::description();
    td += ')';
    LOGGER(ibis::gVerbose > 3)
        << "bord::groupbyc -- starting the final computations for \""
        << td << '"';
    readLock lock(&src, td.c_str());
    std::string tn = ibis::util::randName(td);

    // prepare the types and values for the new table
    const ibis::selectClause::mathTerms& xtms(sel.getTerms());
    std::vector<const ibis::dictionary*> dct(ncx, 0);
    std::vector<std::string> nms(ncx), des(ncx);
    ibis::table::stringArray  nmc(ncx), dec(ncx);
    ibis::table::bufferArray  buf(ncx, 0);
    ibis::table::typeArray    tps(ncx, ibis::UNKNOWN_TYPE);
    IBIS_BLOCK_GUARD(ibis::table::freeBuffers, ibis::util::ref(buf),
                     ibis::util::ref(tps));
    ibis::bitvector msk;
    msk.set(1, src.nRows());

    for (unsigned j = 0; j < ncx; ++ j) {
        nms[j] = sel.termName(j);
        nmc[j] = nms[j].c_str();
        const ibis::math::term* tm = xtms[j];
        if (tm == 0 || tm->termType() == ibis::math::UNDEF_TERM) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bord::groupbyc(" << td
                << ") failed to process term " << j << " named \"" << nms[j]
                << '"';
            return 0;
        }

        std::ostringstream oss;
        oss << *tm;
        des[j] = oss.str();
        dec[j] = des[j].c_str();

        switch (tm->termType()) {
        case ibis::math::NUMBER: {
            tps[j] = ibis::DOUBLE;
            buf[j] = new ibis::array_t<double>(nr, tm->eval());
            break;}
        case ibis::math::STRING: {
            tps[j] = ibis::CATEGORY;
            const std::string val = (const char*)*
                (static_cast<const ibis::math::literal*>(tm));
            buf[j] = new std::vector<std::string>(nr, val);
            break;}
        case ibis::math::VARIABLE: {
            const char* var =
                static_cast<const ibis::math::variable*>(tm)->variableName();
            src.copyColumn(var, tps[j], buf[j], dct[j]);
            break;}
        case ibis::math::STRINGFUNCTION1: {
            tps[j] = ibis::TEXT;
            buf[j] = new std::vector<std::string>;
            src.calculate
                (*static_cast<const ibis::math::stringFunction1*>(tm), msk,
                 *static_cast<std::vector<std::string>*>(buf[j]));
            break;}
        default: {
            tps[j] = ibis::DOUBLE;
            buf[j] = new ibis::array_t<double>;
            src.calculate(*tm, msk, *static_cast<array_t<double>*>(buf[j]));
            break;}
        }
    }

    return(new ibis::bord(tn.c_str(), td.c_str(), nr, buf, tps, nmc, &dec,
                          &dct));
} // ibis::bord::groupbyc

void ibis::bord::orderby(const ibis::table::stringArray& keys) {
    std::vector<bool> directions;
    (void) reorder(keys, directions);
} // ibis::bord::orderby

void ibis::bord::orderby(const ibis::table::stringArray& keys,
                         const std::vector<bool>& directions) {
    (void) reorder(keys, directions);
} // ibis::bord::orderby

long ibis::bord::reorder() {
    return ibis::part::reorder();
} // ibis::bord::reorder

long ibis::bord::reorder(const ibis::table::stringArray& keys) {
    std::vector<bool> directions;
    return reorder(keys, directions);
} // ibis::bord::reorder

long ibis::bord::reorder(const ibis::table::stringArray& cols,
                         const std::vector<bool>& directions) {
    long ierr = 0;
    if (nRows() == 0 || nColumns() == 0) return ierr;

    std::string evt = "bord[";
    evt += m_name;
    evt += "]::reorder";
    if (ibis::gVerbose > 3) {
        ibis::util::logger lg;
        lg() << evt << " -- reordering with " << cols[0];
        for (unsigned j = 1; j < cols.size(); ++ j)
            lg() << ", " << cols[j];
    }

    writeLock lock(this, evt.c_str()); // can't process other operations
    for (columnList::const_iterator it = columns.begin();
         it != columns.end();
         ++ it) { // purge all index files
        (*it).second->unloadIndex();
        (*it).second->purgeIndexFile();
    }

    // look through all columns to match the incoming column names
    typedef std::vector<ibis::column*> colVector;
    std::set<const char*, ibis::lessi> used;
    colVector keys, load; // sort according to the keys
    for (ibis::table::stringArray::const_iterator nit = cols.begin();
         nit != cols.end(); ++ nit) {
        ibis::part::columnList::iterator it = columns.find(*nit);
        if (it != columns.end()) {
            used.insert((*it).first);
            if ((*it).second->upperBound() > (*it).second->lowerBound()) {
                keys.push_back((*it).second);
            }
            else {
                (*it).second->computeMinMax();
                if ((*it).second->upperBound() > (*it).second->lowerBound())
                    keys.push_back((*it).second);
                else
                    load.push_back((*it).second);
            }
        }
        else {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- " << evt << " can not find a column named "
                << *nit;
        }
    }

    if (keys.empty()) { // use all integral values
        if (ibis::gVerbose > 0) {
            if (cols.empty()) {
                LOGGER(true)
                    << evt << " -- user did not specify ordering keys, will "
                    "attempt to use all integer columns as ordering keys";
            }
            else {
                std::ostringstream oss;
                oss << cols[0];
                for (unsigned i = 1; i < cols.size(); ++ i)
                    oss << ", " << cols[i];
                LOGGER(true)
                    << evt << " -- user specified ordering keys \"" << oss.str()
                    << "\" does not match any numerical columns with more "
                    "than one distinct value, will attempt to use "
                    "all integer columns as ordering keys";
            }
        }

        load.clear();
        keys.clear();
        array_t<double> width;
        for (ibis::part::columnList::iterator it = columns.begin();
             it != columns.end(); ++ it) {
            if (! (*it).second->isInteger()) {
                load.push_back((*it).second);
            }
            else if ((*it).second->upperBound() > (*it).second->lowerBound()) {
                keys.push_back((*it).second);
                width.push_back((*it).second->upperBound() -
                                (*it).second->lowerBound());
            }
            else {
                bool asc;
                double cmin, cmax;
                (*it).second->computeMinMax(0, cmin, cmax, asc);
                if (cmax > cmin) {
                    keys.push_back((*it).second);
                    width.push_back(cmax - cmin);
                }
                else {
                    load.push_back((*it).second);
                }
            }
        }
        if (keys.empty()) return -1; //no integral values to use as key
        if (keys.size() > 1) {
            colVector tmp(keys.size());
            array_t<uint32_t> idx;
            width.sort(idx);
            for (uint32_t i = 0; i < keys.size(); ++ i)
                tmp[i] = keys[idx[i]];
            tmp.swap(keys);
        }
    }
    else {
        for (ibis::part::columnList::const_iterator it = columns.begin();
             it != columns.end(); ++ it) {
            std::set<const char*, ibis::lessi>::const_iterator uit =
                used.find((*it).first);
            if (uit == used.end())
                load.push_back((*it).second);
        }
    }
    if (keys.empty()) {
        LOGGER(ibis::gVerbose > 1)
            << evt << " no keys found for sorting, do nothing";
        return -2;
    }
    if (ibis::gVerbose > 1) {
        std::ostringstream oss;
        oss << evt << '(' << keys[0]->name();
        for (unsigned i = 1; i < keys.size(); ++ i)
            oss << ", " << keys[i]->name();
        oss << ')';
        evt = oss.str();
    }
    ibis::util::timer mytimer(evt.c_str(), 1);

    ierr = nEvents;
    array_t<uint32_t> ind1;
    { // the sorting loop -- use a block to limit the scope of starts and ind0
        array_t<uint32_t>  starts, ind0;
        for (uint32_t i = 0; i < keys.size(); ++ i) {
            ibis::bord::column* col =
                dynamic_cast<ibis::bord::column*>(keys[i]);
            if (col == 0) {
                logError("reorder", "all columns must be in-memory");
                return -3;
            }

            const bool asc = (directions.size()>i?directions[i]:true);
            switch (keys[i]->type()) {
            case ibis::CATEGORY:
            case ibis::TEXT:
                ierr = sortStrings(* static_cast<std::vector<std::string>*>
                                   (col->getArray()), starts, ind0, ind1, asc);
                break;
            case ibis::DOUBLE:
                ierr = sortValues(* static_cast<array_t<double>*>
                                  (col->getArray()), starts, ind0, ind1, asc);
                break;
            case ibis::FLOAT:
                ierr = sortValues(* static_cast<array_t<float>*>
                                  (col->getArray()), starts, ind0, ind1, asc);
                break;
            case ibis::ULONG:
                ierr = sortValues(* static_cast<array_t<uint64_t>*>
                                  (col->getArray()), starts, ind0, ind1, asc);
                break;
            case ibis::LONG:
                ierr = sortValues(* static_cast<array_t<int64_t>*>
                                  (col->getArray()), starts, ind0, ind1, asc);
                break;
            case ibis::UINT:
                ierr = sortValues(* static_cast<array_t<uint32_t>*>
                                  (col->getArray()), starts, ind0, ind1, asc);
                break;
            case ibis::INT:
                ierr = sortValues(* static_cast<array_t<int32_t>*>
                                  (col->getArray()), starts, ind0, ind1, asc);
                break;
            case ibis::USHORT:
                ierr = sortValues(* static_cast<array_t<uint16_t>*>
                                  (col->getArray()), starts, ind0, ind1, asc);
                break;
            case ibis::SHORT:
                ierr = sortValues(* static_cast<array_t<int16_t>*>
                                  (col->getArray()), starts, ind0, ind1, asc);
                break;
            case ibis::UBYTE:
                ierr = sortValues(* static_cast<array_t<unsigned char>*>
                                  (col->getArray()), starts, ind0, ind1, asc);
                break;
            case ibis::BYTE:
                ierr = sortValues(* static_cast<array_t<signed char>*>
                                  (col->getArray()), starts, ind0, ind1, asc);
                break;
            default:
                logWarning("reorder", "column %s type %d is not supported",
                           keys[i]->name(), static_cast<int>(keys[i]->type()));
                break;
            }

            if (ierr == static_cast<long>(nRows())) {
                ind1.swap(ind0);
            }
            else {
                logError("reorder", "failed to reorder column %s, ierr=%ld.  "
                         "data files are no longer consistent!",
                         keys[i]->name(), ierr);
            }
        }
    }
#if DEBUG+0 > 0 || _DEBUG+0 > 0
    {
        ibis::util::logger lg(4);
        lg() << "DEBUG -- bord[" << ibis::part::name() << "]::reorder --\n";
        std::vector<bool> marks(ind1.size(), false);
        for (uint32_t i = 0; i < ind1.size(); ++ i) {
            if (ibis::gVerbose > 6)
                lg() << "ind[" << i << "]=" << ind1[i] << "\n";
            if (ind1[i] < marks.size())
                marks[ind1[i]] = true;
        }
        bool isperm = true;
        for (uint32_t i = 0; isperm && i < marks.size(); ++ i)
            isperm = marks[i];
        if (isperm)
            lg() << "array ind IS a permutation\n";
        else
            lg() << "array ind is NOT a permutation\n";
    }
#endif
    for (ibis::part::columnList::const_iterator it = columns.begin();
         it != columns.end();
         ++ it) { // update the m_sorted flag of each column
        (*it).second->isSorted((*it).second == keys[0]);
    }

    for (uint32_t i = 0; i < load.size(); ++ i) {
        ibis::bord::column* col = dynamic_cast<ibis::bord::column*>(load[i]);
        if (col == 0) {
            logError("reorder", "all columns must be in-memory");
            return -4;
        }

        switch (load[i]->type()) {

        case ibis::CATEGORY:
        case ibis::TEXT:
            ierr = reorderStrings(* static_cast<std::vector<std::string>*>
                                  (col->getArray()), ind1);
            break;
        case ibis::DOUBLE:
            ierr = reorderValues(* static_cast<array_t<double>*>
                                 (col->getArray()), ind1);
            break;
        case ibis::FLOAT:
            ierr = reorderValues(* static_cast<array_t<float>*>
                                 (col->getArray()), ind1);
            break;
        case ibis::ULONG:
            ierr = reorderValues(* static_cast<array_t<uint64_t>*>
                                 (col->getArray()), ind1);
            break;
        case ibis::LONG:
            ierr = reorderValues(* static_cast<array_t<int64_t>*>
                                 (col->getArray()), ind1);
            break;
        case ibis::UINT:
            ierr = reorderValues(* static_cast<array_t<uint32_t>*>
                                 (col->getArray()), ind1);
            break;
        case ibis::INT:
            ierr = reorderValues(* static_cast<array_t<int32_t>*>
                                 (col->getArray()), ind1);
            break;
        case ibis::USHORT:
            ierr = reorderValues(* static_cast<array_t<uint16_t>*>
                                 (col->getArray()), ind1);
            break;
        case ibis::SHORT:
            ierr = reorderValues(* static_cast<array_t<int16_t>*>
                                 (col->getArray()), ind1);
            break;
        case ibis::UBYTE:
            ierr = reorderValues(* static_cast<array_t<unsigned char>*>
                                 (col->getArray()), ind1);
            break;
        case ibis::BYTE:
            ierr = reorderValues(* static_cast<array_t<signed char>*>
                                 (col->getArray()), ind1);
            break;
        default:
            logWarning("reorder", "column %s type %s is not supported",
                       keys[i]->name(),
                       ibis::TYPESTRING[static_cast<int>(keys[i]->type())]);
            break;
        }
    }
    return ierr;
} // ibis::bord::reorder

/// A simple sorting procedure.  The incoming values in vals are divided
/// into segements with starts.  Within each segement, this function orders
/// the values in ascending order by default unless ascending[i] is present
/// and is false.
///
/// @note This function uses a simple algorithm and requires space for a
/// copy of vals plus a copy of starts.
template <typename T>
long ibis::bord::sortValues(array_t<T>& vals,
                            array_t<uint32_t>& starts,
                            array_t<uint32_t>& idxout,
                            const array_t<uint32_t>& idxin,
                            bool ascending) const {
    ibis::horometer timer;
    if (ibis::gVerbose > 4)
        timer.start();

    if (vals.size() != nEvents ||
        (idxin.size() != vals.size() && ! idxin.empty())) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- bord[" << ibis::part::name() << "]::sortValues<"
            << typeid(T).name() << "> can not proceed because array sizes do "
            "not match, both vals.size(" << vals.size() << ") and idxin.size("
            << idxin.size() << ") are expected to be " << nEvents;
        return -3;
    }
    if (idxin.empty() || starts.size() < 2 || starts[0] != 0
        || starts.back() != vals.size()) {
        starts.resize(2);
        starts[0] = 0;
        starts[1] = vals.size();
        LOGGER(ibis::gVerbose > 1)
            << "bord[" << ibis::part::name() << "]::sortValues<"
            << typeid(T).name() << "> (re)set array starts to contain [0, "
            << nEvents << "]";
    }

    uint32_t nseg = starts.size() - 1;
    if (nseg > nEvents) { // no sorting necessary
        idxout.copy(idxin);
    }
    else if (nseg > 1) { // sort multiple blocks
        idxout.resize(nEvents);
        array_t<uint32_t> starts2;
        array_t<T> tmp(nEvents);

        for (uint32_t iseg = 0; iseg < nseg; ++ iseg) {
            const uint32_t segstart = starts[iseg];
            const uint32_t segsize = starts[iseg+1]-starts[iseg];
            if (segsize > 2) {
                // copy the segment into a temporary array, then sort it
                array_t<uint32_t> ind0;
                tmp.resize(segsize);
                for (unsigned i = 0; i < segsize; ++ i)
                    tmp[i] = vals[idxin[i+segstart]];
                tmp.sort(ind0);
                if (! ascending)
                    std::reverse(ind0.begin(), ind0.end());

                starts2.push_back(segstart);
                T last = tmp[ind0[0]];
                idxout[segstart] = idxin[ind0[0] + segstart];
                for (unsigned i = 1; i < segsize; ++ i) {
                    idxout[i+segstart] = idxin[ind0[i] + segstart];
                    if (tmp[ind0[i]] > last) {
                        starts2.push_back(i + segstart);
                        last = tmp[ind0[i]];
                    }
                }
            }
            else if (segsize == 2) { // two-element segment
                if (vals[idxin[segstart]] < vals[idxin[segstart+1]]) {
                    if (ascending) {// in the right order
                        idxout[segstart] = idxin[segstart];
                        idxout[segstart+1] = idxin[segstart+1];
                    }
                    else {
                        idxout[segstart] = idxin[segstart+1];
                        idxout[segstart+1] = idxin[segstart];
                    }
                    starts2.push_back(segstart);
                    starts2.push_back(segstart+1);
                }
                else if (vals[idxin[segstart]] == vals[idxin[segstart+1]]) {
                    idxout[segstart] = idxin[segstart];
                    idxout[segstart+1] = idxin[segstart+1];
                    starts2.push_back(segstart);
                }
                else { // assum the 1st value is larger (could be
                       // incomparable though)
                    if (ascending) {
                        idxout[segstart] = idxin[segstart+1];
                        idxout[segstart+1] = idxin[segstart];
                    }
                    else {
                        idxout[segstart] = idxin[segstart];
                        idxout[segstart+1] = idxin[segstart+1];
                    }
                    starts2.push_back(segstart);
                    starts2.push_back(segstart+1);
                }
            }
            else { // segment contains only one element
                starts2.push_back(segstart);
                idxout[segstart] = idxin[segstart];
            }
        }
        starts2.push_back(nEvents);
        starts.swap(starts2);

        // place values in the new order
        tmp.resize(nEvents);
        for (uint32_t i = 0; i < nEvents; ++ i)
            tmp[i] = vals[idxout[i]];
        vals.swap(tmp);
    }
    else { // all in one block
        idxout.resize(nEvents);
        for (uint32_t j = 0; j < nEvents; ++ j)
            idxout[j] = j;
        ibis::util::sortKeys(vals, idxout);
        if (! ascending) {
            std::reverse(vals.begin(), vals.end());
            std::reverse(idxout.begin(), idxout.end());
        }

        starts.clear();
        starts.push_back(0U);
        T last = vals[0];
        for (uint32_t i = 1; i < nEvents; ++ i) {
            if (vals[i] > last) {
                starts.push_back(i);
                last = vals[i];
            }
        }
        starts.push_back(nEvents);
    }

    if (ibis::gVerbose > 4) {
        timer.stop();
        nseg = starts.size() - 1;
        LOGGER(1)
            << "bord::sortValues -- reordered " << nEvents << " value"
            << (nEvents>1 ? "s" : "") << " (into " << nseg
            << " segment" << (nseg>1 ? "s" : "") << ") in "
            << timer.CPUTime() << " sec(CPU), "
            << timer.realTime() << " sec(elapsed)";
    }
    return nEvents;
} // ibis::bord::sortValues

/// Sort the string values.  It preserves the previous order determined
/// represented by idxin and starts.
long ibis::bord::sortStrings(std::vector<std::string>& vals,
                             array_t<uint32_t>& starts,
                             array_t<uint32_t>& idxout,
                             const array_t<uint32_t>& idxin,
                             bool ascending) const {
    ibis::horometer timer;
    if (ibis::gVerbose > 4)
        timer.start();

    if (vals.size() != nEvents ||
        (idxin.size() != vals.size() && ! idxin.empty())) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- bord[" << ibis::part::name() << "]::sortStrings "
            " can not proceed because array sizes do not match, both "
            "vals.size(" << vals.size() << ") and idxin.size(" << idxin.size()
            << ") are expected to be " << nEvents;
        return -3;
    }
    if (idxin.empty() || starts.size() < 2 || starts[0] != 0
        || starts.back() != vals.size()) {
        starts.resize(2);
        starts[0] = 0;
        starts[1] = vals.size();
        LOGGER(ibis::gVerbose > 1)
            << "bord[" << ibis::part::name() << "]::sortStrings -- (re)set "
            "array starts to contain [0, " << nEvents << "]";
    }

    uint32_t nseg = starts.size() - 1;
    if (nseg > nEvents) { // no sorting necessary
        idxout.copy(idxin);
    }
    else if (nseg > 1) { // sort multiple blocks
        idxout.resize(nEvents);
        array_t<uint32_t> starts2;
        std::vector<std::string> tmp(nEvents);

        for (uint32_t iseg = 0; iseg < nseg; ++ iseg) {
            const uint32_t segstart = starts[iseg];
            const uint32_t segsize = starts[iseg+1]-starts[iseg];
            if (segsize > 2) {
                // copy the segment into a temporary array, then sort it
                tmp.resize(segsize);
                array_t<uint32_t> ind0(segsize);
                for (unsigned i = segstart; i < starts[iseg+1]; ++ i) {
                    tmp[i-segstart] = vals[idxin[i]];
                    ind0[i-segstart] = idxin[i];
                }
                // sort tmp and move ind0
                ibis::util::sortStrings(tmp, ind0);
                if (! ascending)
                    std::reverse(ind0.begin(), ind0.end());

                starts2.push_back(segstart);
                uint32_t last = 0;
                idxout[segstart] = ind0[0];
                for (unsigned i = 1; i < segsize; ++ i) {
                    idxout[i+segstart] = ind0[i];
                    if (tmp[i].compare(tmp[last]) != 0) {
                        starts2.push_back(i + segstart);
                        last = i;
                    }
                }
            }
            else if (segsize == 2) {
                int cmp = vals[idxin[segstart]].compare
                    (vals[idxin[segstart+1]]);
                if (cmp < 0) { // in the right order, different strings
                    if (ascending) {
                        idxout[segstart] = idxin[segstart];
                        idxout[segstart+1] = idxin[segstart+1];
                    }
                    else {
                        idxout[segstart] = idxin[segstart+1];
                        idxout[segstart+1] = idxin[segstart];
                    }
                    starts2.push_back(segstart);
                    starts2.push_back(segstart+1);
                }
                else if (cmp == 0) { // two strings are the same
                    idxout[segstart] = idxin[segstart];
                    idxout[segstart+1] = idxin[segstart+1];
                    starts2.push_back(segstart);
                }
                else { // in the wrong order, different strings
                    if (ascending) {
                        idxout[segstart] = idxin[segstart+1];
                        idxout[segstart+1] = idxin[segstart];
                    }
                    else {
                        idxout[segstart] = idxin[segstart];
                        idxout[segstart+1] = idxin[segstart+1];
                    }
                    starts2.push_back(segstart);
                    starts2.push_back(segstart+1);
                }
            }
            else { // segment contains only one element
                starts2.push_back(segstart);
                idxout[segstart] = idxin[segstart];
            }
        }
        starts2.push_back(nEvents);
        starts.swap(starts2);
        // place values in the new order
        tmp.resize(nEvents);
        for (uint32_t i = 0; i < nEvents; ++ i)
            tmp[i].swap(vals[idxout[i]]);
        vals.swap(tmp);
    }
    else { // all in one block
        idxout.resize(nEvents);
        for (uint32_t j = 0; j < nEvents; ++ j)
            idxout[j] = j;
        ibis::util::sortStrings(vals, idxout);
        if (! ascending) {
            std::reverse(vals.begin(), vals.end());
            std::reverse(idxout.begin(), idxout.end());
        }

        starts.clear();
        starts.push_back(0U);
        uint32_t last = 0;
        for (uint32_t i = 1; i < nEvents; ++ i) {
            if (vals[i].compare(vals[last]) != 0) {
                starts.push_back(i);
                last = i;
            }
        }
        starts.push_back(nEvents);
    }

    if (ibis::gVerbose > 4) {
        timer.stop();
        nseg = starts.size() - 1;
        logMessage("sortStrings", "reordered %lu string%s (into %lu "
                   "segment%s) in %g sec(CPU), %g sec(elapsed)",
                   static_cast<long unsigned>(nEvents), (nEvents>1 ? "s" : ""),
                   static_cast<long unsigned>(nseg), (nseg>1 ? "s" : ""),
                   timer.CPUTime(), timer.realTime());
    }
    return nEvents;
} // ibis::bord::sortStrings

template <typename T>
long ibis::bord::reorderValues(array_t<T>& vals,
                               const array_t<uint32_t>& ind) const {
    if (vals.size() != nEvents || ind.size() != vals.size()) {
        if (ibis::gVerbose > 1)
            logMessage("reorderValues", "array sizes do not match, both "
                       "vals.size(%ld) and ind.size(%ld) are expected to "
                       "be %ld", static_cast<long>(vals.size()),
                       static_cast<long>(ind.size()),
                       static_cast<long>(nEvents));
        return -3;
    }
    array_t<T> tmp(vals.size());
    for (uint32_t i = 0; i < vals.size(); ++ i)
        tmp[i] = vals[ind[i]];
    tmp.swap(vals);
    return nEvents;
} // ibis::bord::reorderValues

/// Reorder the vector of strings.  To avoid recreating the content of the
/// string values, this function uses swap operations to move the existing
/// strings into their new locations.  It only works if ind is a proper
/// permutation of integers between 0 and vals.size() (include 0 but
/// exclude vals.size()), however, it does not check whether the input
/// array is a proper permutation.
long ibis::bord::reorderStrings(std::vector<std::string>& vals,
                                const array_t<uint32_t>& ind) const {
    if (vals.size() != nEvents || ind.size() != vals.size()) {
        if (ibis::gVerbose > 1)
            logMessage("reorderValues", "array sizes do not match, both "
                       "vals.size(%ld) and ind.size(%ld) are expected to "
                       "be %ld", static_cast<long>(vals.size()),
                       static_cast<long>(ind.size()),
                       static_cast<long>(nEvents));
        return -3;
    }
    std::vector<std::string> tmp(vals.size());
    for (uint32_t i = 0; i < vals.size(); ++ i)
        tmp[i].swap(vals[ind[i]]);
    tmp.swap(vals);
    return nEvents;
} // ibis::bord::reorderValues

void ibis::bord::reverseRows() {
    for (ibis::part::columnList::iterator it = columns.begin();
         it != columns.end(); ++ it) {
        reinterpret_cast<ibis::bord::column*>((*it).second)->reverseRows();
    }
} // ibis::bord::reverseRows

/// Reset the number of rows in the data partition to be @c nr.  If the
/// existing data partitioning has no more than @nr rows, nothing is done,
/// otherwise, the first @c nr rows are kept.
///
/// It retunrs 0 to indicate normal completion, a negative number to
/// indicate error.
int ibis::bord::limit(uint32_t nr) {
    int ierr = 0;
    if (nEvents <= nr) return ierr;

    for (ibis::part::columnList::iterator it = columns.begin();
         it != columns.end(); ++ it) {
        int ier2 = reinterpret_cast<ibis::bord::column*>((*it).second)->limit(nr);
        if (ier2 < 0 && ier2 < ierr)
            ierr = ier2;
    }
    nEvents = nr;
    return ierr;
} // ibis::bord::limit

/// Evaluate the arithmetic expressions in the select clause to derive an
/// in-memory data table.
ibis::bord*
ibis::bord::evaluateTerms(const ibis::selectClause& sel,
                          const char* desc) const {
    std::string mydesc;
    if (desc == 0 || *desc == 0) {
        mydesc = "SELECT ";
        mydesc += sel.getString();
        mydesc += " FROM ";
        mydesc += m_name;
        desc = mydesc.c_str();
    }
    std::string tn = ibis::util::randName(desc);
    if (nEvents == 0 || columns.empty() || sel.empty()) {
        return 0;
    }
    if (ibis::gVerbose > 4) {
        ibis::util::logger lg(4);
        lg() << "bord[" << ibis::part::name() << "]::evaluateTerms processing "
             << desc << " to produce an in-memory data partition named " << tn;
        if (ibis::gVerbose > 6)
            print(lg());
    }

    long ierr;
    ibis::bitvector msk;
    msk.set(1, nEvents);
    ibis::table::bufferArray  buf;
    ibis::table::typeArray    ct;
    ibis::table::stringArray  cn;
    ibis::table::stringArray  cd;
    std::vector<std::string> cdesc;
    std::vector<const ibis::dictionary*> dct;
    ibis::util::guard gbuf =
        ibis::util::makeGuard(ibis::table::freeBuffers, ibis::util::ref(buf),
                              ibis::util::ref(ct));
    for (uint32_t j = 0; j < sel.aggSize(); ++ j) {
        const ibis::math::term* t = sel.aggExpr(j);
        std::string de = sel.aggDescription(j);
        LOGGER(ibis::gVerbose > 4)
            << "bord[" << ibis::part::name() << "] -- evaluating term # " << j
            << ": \"" << de << '"';

        dct.push_back(0);
        switch (t->termType()) {
        default: {
            cdesc.push_back(de);
            cn.push_back(sel.aggName(j));
            ct.push_back(ibis::DOUBLE);
            buf.push_back(new ibis::array_t<double>(nEvents));
            ierr = calculate
                (*t, msk, *static_cast< ibis::array_t<double>* >(buf.back()));
            if (ierr != (long)nEvents) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- bord::evaluateTerms(" << desc
                    << ") failed to evaluate term " << j << " ("
                    << cdesc.back() << "), ierr = " << ierr;
                return 0;
            }
            break;}
        case ibis::math::NUMBER: { // a fixed number
            cn.push_back(sel.aggName(j));
            ct.push_back(ibis::DOUBLE);
            cdesc.push_back(de);
            buf.push_back(new ibis::array_t<double>(nEvents, t->eval()));
            break;}
        case ibis::math::STRING: { // a string literal
            cn.push_back(sel.aggName(j));
            ct.push_back(ibis::CATEGORY);
            cdesc.push_back(de);
            buf[j] = new std::vector<std::string>
                (nEvents, (const char*)*
                 static_cast<const ibis::math::literal*>(t));
            break;}
        case ibis::math::VARIABLE: {
            const char* cn1 = static_cast<const ibis::math::variable*>
                (t)->variableName();
            if (*cn1 == '*') { // must be count(*)
                cn.push_back(cn1);
                cdesc.push_back(de);
                ct.push_back(ibis::UINT);
                ibis::part::columnList::const_iterator it = columns.find("*");
                if (it == columns.end()) { // new values
                    buf.push_back(new ibis::array_t<uint32_t>(nEvents, 1U));
                }
                else { // copy existing values
                    buf.push_back(new ibis::array_t<uint32_t>
                                  (it->second->getRawData()));
                }
                continue;
            }

            const ibis::column *col = getColumn(cn1);
            if (col == 0) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- bord::evaluateTerms(" << desc
                    << ") failed to find column # " << j << " named " << cn1;
                continue;
            }

            cn.push_back(cn1);
            cdesc.push_back(de);
            ct.push_back(col->type());
            switch (col->type()) {
            default: {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- bord::evaluateTerms(" << desc
                    << ") can not handle column " << j << " type "
                    << ibis::TYPESTRING[(int)ct.back()];
                return 0;}
            case ibis::BYTE:
                buf.push_back(new ibis::array_t<signed char>);
                ierr = col->selectValues(msk, buf.back());
                if (ierr < 0) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- bord::evaluateTerms(" << desc
                        << ") expected to retrieve " << nEvents
                        << " values for column " << j << " (" << cn1
                        << ", " << ibis::TYPESTRING[(int)ct.back()]
                        << "), but got " << ierr;
                    return 0;
                }
                break;
            case ibis::UBYTE:
                buf.push_back(new ibis::array_t<unsigned char>);
                ierr = col->selectValues(msk, buf.back());
                if (ierr < 0) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- bord::evaluateTerms(" << desc
                        << ") expected to retrieve " << nEvents
                        << " values for column " << j << " (" << cn1
                        << ", " << ibis::TYPESTRING[(int)ct.back()]
                        << "), but got " << ierr;
                    return 0;
                }
                break;
            case ibis::SHORT:
                buf.push_back(new ibis::array_t<int16_t>);
                ierr = col->selectValues(msk, buf.back());
                if (ierr < 0) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- bord::evaluateTerms(" << desc
                        << ") expected to retrieve " << nEvents
                        << " values for column " << j << " (" << cn1
                        << ", " << ibis::TYPESTRING[(int)ct.back()]
                        << "), but got " << ierr;
                    return 0;
                }
                break;
            case ibis::USHORT:
                buf.push_back(new ibis::array_t<uint16_t>);
                ierr = col->selectValues(msk, buf.back());
                if (ierr < 0) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- bord::evaluateTerms(" << desc
                        << ") expected to retrieve " << nEvents
                        << " values for column " << j << " (" << cn1
                        << ", " << ibis::TYPESTRING[(int)ct.back()]
                        << "), but got " << ierr;
                    return 0;
                }
                break;
            case ibis::INT:
                buf.push_back(new ibis::array_t<int32_t>);
                ierr = col->selectValues(msk, buf.back());
                if (ierr < 0) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- bord::evaluateTerms(" << desc
                        << ") expected to retrieve " << nEvents
                        << " values for column " << j << " (" << cn1
                        << ", " << ibis::TYPESTRING[(int)ct.back()]
                        << "), but got " << ierr;
                    return 0;
                }
                break;
            case ibis::UINT: {
                buf.push_back(new ibis::array_t<uint32_t>);
                ierr = col->selectValues(msk, buf.back());
                if (ierr < 0) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- bord::evaluateTerms(" << desc
                        << ") expected to retrieve " << nEvents
                        << " values for column " << j << " (" << cn1
                        << ", " << ibis::TYPESTRING[(int)ct.back()]
                        << "), but got " << ierr;
                    return 0;
                }
                const ibis::bord::column *bc =
                    dynamic_cast<const ibis::bord::column*>(col);
                if (bc != 0)
                    dct.back() = bc->getDictionary();
                break;}
            case ibis::LONG:
                buf.push_back(new ibis::array_t<int64_t>);
                ierr = col->selectValues(msk, buf.back());
                if (ierr < 0) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- bord::evaluateTerms(" << desc
                        << ") expected to retrieve " << nEvents
                        << " values for column " << j << " (" << cn1
                        << ", " << ibis::TYPESTRING[(int)ct.back()]
                        << "), but got " << ierr;
                    return 0;
                }
                break;
            case ibis::ULONG:
                buf.push_back(new ibis::array_t<uint64_t>);
                ierr = col->selectValues(msk, buf.back());
                if (ierr < 0) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- bord::evaluateTerms(" << desc
                        << ") expected to retrieve " << nEvents
                        << " values for column " << j << " (" << cn1
                        << ", " << ibis::TYPESTRING[(int)ct.back()]
                        << "), but got " << ierr;
                    return 0;
                }
                break;
            case ibis::FLOAT:
                buf.push_back(new ibis::array_t<float>);
                ierr = col->selectValues(msk, buf.back());
                if (ierr < 0) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- bord::evaluateTerms(" << desc
                        << ") expected to retrieve " << nEvents
                        << " values for column " << j << " (" << cn1
                        << ", " << ibis::TYPESTRING[(int)ct.back()]
                        << "), but got " << ierr;
                    return 0;
                }
                break;
            case ibis::DOUBLE:
                buf.push_back(new ibis::array_t<double>);
                ierr = col->selectValues(msk, buf.back());
                if (ierr < 0) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- bord::evaluateTerms(" << desc
                        << ") expected to retrieve " << nEvents
                        << " values for column " << j << " (" << cn1
                        << ", " << ibis::TYPESTRING[(int)ct.back()]
                        << "), but got " << ierr;
                    return 0;
                }
                break;
            case ibis::TEXT:
            case ibis::CATEGORY:
                buf.push_back(col->selectStrings(msk));
                if (buf[j] == 0 || static_cast<std::vector<std::string>*>
                    (buf[j])->size() != nEvents) {
                    LOGGER(ibis::gVerbose > 0)
                        << "Warning -- bord::evaluateTerms(" << desc
                        << ") expected to retrieve " << nEvents
                        << " values for column " << j << " (" << cn1
                        << ", " << ibis::TYPESTRING[(int)ct.back()]
                        << "), but got "
                        << (buf.back() != 0 ?
                            static_cast<std::vector<std::string>*>
                            (buf.back())->size() : 0U);
                    return 0;
                }
                if (col->getDictionary() != 0)
                    dct.back() = col->getDictionary();
                break;
            }
            break;}
        } // switch (t->termType())
    } // for (uint32_t j...

    for (unsigned j = 0; j < cdesc.size(); ++ j)
        cd.push_back(cdesc[j].c_str());
    std::unique_ptr<ibis::bord> brd1
        (new ibis::bord(tn.c_str(), desc, (uint64_t)nEvents, buf, ct, cn, &cd));
    if (brd1.get() != 0)
        gbuf.dismiss();
    return brd1.release();
} // ibis::bord::evaluateTerms

/// Convert the integer representation of categorical columns back to the
/// string representation.  The argument is used to determine if the
/// original column was categorical values.
///
/// Upon successful completion of this function, it returns the number of
/// rows in the column.  It returns a negative number to indicate errors.
int ibis::bord::restoreCategoriesAsStrings(const ibis::part& ref) {
    int ierr = nEvents;
    for (columnList::iterator it = columns.begin();
         it != columns.end(); ++ it) {
        if (it->second->type() == ibis::UINT) {
            const ibis::category* cat =
                dynamic_cast<const ibis::category*>(ref.getColumn(it->first));
            if (cat != 0) {
                ierr = static_cast<ibis::bord::column*>(it->second)
                    ->restoreCategoriesAsStrings(*cat);
                if (ierr < 0)
                    return ierr;
            }
        }
    }
    return ierr;
} // ibis::bord::restoreCategoriesAsStrings

/// Copy the type and values of the named column.  It uses a shallow copy
/// for integers and floating-point numbers.
void ibis::bord::copyColumn(const char* nm, ibis::TYPE_T& t, void*& buf,
                            const ibis::dictionary*& dic) const {
    const ibis::column* col = getColumn(nm);
    if (col == 0) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- bord[" << name_ << "]::copyColumn failed to find "
            "a column named " << nm;
        t = ibis::UNKNOWN_TYPE;
        buf = 0;
        return;
    }

    t = col->type();
    switch (col->type()) {
    case ibis::BYTE:
        buf = new ibis::array_t<signed char>;
        col->getValuesArray(buf);
        break;
    case ibis::UBYTE:
        buf = new ibis::array_t<unsigned char>;
        col->getValuesArray(buf);
        break;
    case ibis::SHORT:
        buf = new ibis::array_t<int16_t>;
        col->getValuesArray(buf);
        break;
    case ibis::USHORT:
        buf = new ibis::array_t<uint16_t>;
        col->getValuesArray(buf);
        break;
    case ibis::INT:
        buf = new ibis::array_t<int32_t>;
        col->getValuesArray(buf);
        break;
    case ibis::UINT: {
        buf = new ibis::array_t<uint32_t>;
        col->getValuesArray(buf);
        const ibis::bord::column *bc =
            dynamic_cast<const ibis::bord::column*>(col);
        if (bc != 0)
            dic = bc->getDictionary();
        break;}
    case ibis::LONG:
        buf = new ibis::array_t<int64_t>;
        col->getValuesArray(buf);
        break;
    case ibis::ULONG:
        buf = new ibis::array_t<uint64_t>;
        col->getValuesArray(buf);
        break;
    case ibis::FLOAT:
        buf = new ibis::array_t<float>;
        col->getValuesArray(buf);
        break;
    case ibis::DOUBLE:
        buf = new ibis::array_t<double>;
        col->getValuesArray(buf);
        break;
    case ibis::TEXT:
        buf = new std::vector<std::string>;
        col->getValuesArray(buf);
        break;
    case ibis::CATEGORY:
        dic = static_cast<const ibis::category*>(col)->getDictionary();
        buf = new std::vector<std::string>;
        col->getValuesArray(buf);
        break;
    default:
        t = ibis::UNKNOWN_TYPE;
        buf = 0;
        break;
    }
} // ibis::bord::copyColumn

int ibis::bord::renameColumns(const ibis::selectClause& sel) {
    ibis::selectClause::nameMap nmap;
    int ierr = sel.getAliases(nmap);
    if (ierr <= 0) return ierr;

    for (ibis::selectClause::nameMap::const_iterator it = nmap.begin();
         it != nmap.end(); ++ it) {
        ibis::part::columnList::iterator cit = columns.find(it->first);
        if (cit != columns.end()) {
            ibis::column *col = cit->second;
            columns.erase(cit);
            col->name(it->second);
            columns[col->name()] = col;
            LOGGER(ibis::gVerbose > 5)
                << "bord::renameColumns -- " << it->first << " --> "
                << col->name();
        }
        else {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- bord::renameColumns can not find a column named "
                << it->first << " to change it to " << it->second;
        }
    }

    ierr = 0;
    // re-establish the order of columns according to xtrms_
    colorder.clear();
    const unsigned ntrms = sel.getTerms().size();
    for (unsigned j = 0; j < ntrms; ++ j) {
        const char *tn = sel.termName(j);
        const ibis::column *col = getColumn(tn);
        if (col != 0) {
            colorder.push_back(col);
        }
        else {
            -- ierr;
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- bord::renameColumns can not find a column named "
                << tn << ", but the select clause contains the name as term "
                << j;
        }
    }
    return ierr;
} // ibis::bord::renameColumns

/// Append the values marked 1 to this data partition.  This is an
/// in-memory operation and therefore can only accomodate relatively small
/// number of rows.
///
/// It returns the number of rows added upon successful completion.
/// Otherwise it returns a negative number to indicate error.
int ibis::bord::append(const ibis::selectClause& sc, const ibis::part& prt,
                       const ibis::bitvector &mask) {
    int ierr = 0;
    if (mask.cnt() == 0) return ierr;

    const ibis::selectClause::StringToInt& colmap = sc.getOrdered();
    const uint32_t nagg = sc.aggSize();
    const uint32_t nh   = nEvents;
    const uint32_t nqq  = mask.cnt();
    std::string mesg    = "bord[";
    mesg += m_name;
    mesg += "]::append";
    if ((uint64_t)nh + (uint64_t)nqq > 0x7FFFFFFFUL) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << mesg << " can not proceed because the "
            "resulting data partition would be too large (" << nh << " + "
            << nqq << " = " << nh+nqq << " rows)";
        return -18;
    }
    if (ibis::gVerbose > 4) {
        ibis::util::logger lg;
        lg() << mesg << " -- to process " << nqq << " row" << (nqq>1?"s":"")
             << " from partition " << prt.name() << ", # of existing rows = "
             << nh;
        if (ibis::gVerbose > 6) {
            lg() << "\n    colmap[" << colmap.size() << "]";
            for (ibis::selectClause::StringToInt::const_iterator it
                     = colmap.begin(); it != colmap.end(); ++ it) {
                lg() << "\n\t" << it->first << "\t--> " << it->second;
                if (it->second < nagg)
                    lg() << " (" << *(sc.aggExpr(it->second)) << ")";
            }
        }
    }

    amask.adjustSize(0, nh);
    ibis::bitvector newseg; // mask for the new segment of data
    newseg.set(1, nqq);
    for (columnList::iterator cit = columns.begin();
         cit != columns.end() && ierr >= 0; ++ cit) {
        ibis::bord::column& col =
            *(static_cast<ibis::bord::column*>(cit->second));
        ibis::selectClause::StringToInt::const_iterator mit =
            colmap.find(cit->first);
        if (mit == colmap.end()) {
            mit = colmap.find(col.description());
            if (mit == colmap.end()) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg << " failed to locate "
                    << cit->first << " in the list of names in " << sc;
                return -13;
            }
        }
        const uint32_t itm = mit->second;
        if (itm >= nagg) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- " << mesg << " mapped " << col.name()
                << " into term " << itm << " which is outside of " << sc;
            return -14;
        }

        const ibis::math::term *aterm = sc.aggExpr(itm);
        switch (aterm->termType()) {
        case ibis::math::UNDEF_TERM:
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- " << mesg << " -- can not handle a "
                "math::term of undefined type";
            ierr = -15;
            break;
        case ibis::math::VARIABLE: {
            const ibis::math::variable &var =
                *static_cast<const ibis::math::variable*>(aterm);
            const ibis::column* scol = prt.getColumn(var.variableName());
            if (scol == 0) {
                if (*(var.variableName()) == '*') { // counts
                    col.addCounts(nEvents+nqq);
                }
                else {
                    LOGGER(ibis::gVerbose > 1)
                        << "Warning -- " << mesg << " -- \""
                        << var.variableName()
                        << "\" is not a column of partition " << prt.name();
                    ierr = -16;
                }
            }
            else {
                LOGGER(ibis::gVerbose > 4)
                    << mesg << " is to add " << nqq << " element"
                    << (nqq>1?"s":"") << " to column \"" << cit->first
                    << "\" from column \"" << scol->name()
                    << "\" of partition " << prt.name();
                ierr = col.append(*scol, mask);
            }
            if (col.getTimeFormat() == 0) {
                if (var.getDecoration() != 0 && *(var.getDecoration()) != 0)
                    col.setTimeFormat(var.getDecoration());
                else if (scol != 0 && scol->getTimeFormat() != 0)
                    col.setTimeFormat(*(scol->getTimeFormat()));
            }
            break;}
        case ibis::math::STRINGFUNCTION1: {
            std::vector<std::string> tmp;
            ierr = prt.calculate
                (*static_cast<const ibis::math::stringFunction1*>(aterm),
                 mask, tmp);
            if (ierr > 0) {
                LOGGER(ibis::gVerbose > 2)
                    << mesg << " -- adding " << tmp.size() << " element"
                    << (tmp.size()>1?"s":"") << " to column " << cit->first
                    << " from " << *aterm;
                ierr = col.append(&tmp, newseg);
            }
            break;}
        default: {
            array_t<double> tmp;
            ierr = prt.calculate(*aterm, mask, tmp);
            if (ierr > 0) {
                LOGGER(ibis::gVerbose > 2)
                    << mesg << " -- adding " << tmp.size() << " element"
                    << (tmp.size()>1?"s":"") << " to column " << cit->first
                    << " from " << *aterm;
                ierr = col.append(&tmp, newseg);
            }
            break;}
        }
    }
    if (ierr >= 0) {
        ierr = nqq;
        nEvents += nqq;
        amask.adjustSize(nEvents, nEvents);
        LOGGER(ibis::gVerbose > 3)
            << mesg << " added " << nqq << " row" << (nqq>1?"s":"")
            << " to make a total of " << nEvents;
    }
    return ierr;
} // ibis::bord::append

/// Append the rows satisfying the specified range expression.  This
/// function assumes the select clause only needs the column involved in
/// the range condition to complete its operations.
///
/// It returns the number rows satisfying the range expression on success,
/// otherwise it returns a negative value.
int ibis::bord::append(const ibis::selectClause &sc, const ibis::part& prt,
                       const ibis::qContinuousRange &cnd) {
    const ibis::column *scol = prt.getColumn(cnd.colName());
    if (scol == 0) return -12;
    std::string mesg = "bord[";
    mesg += m_name;
    mesg += "]::append";

    // use a temporary bord to hold the new data
    ibis::bord btmp;
    ibis::bord::column *ctmp = new ibis::bord::column
        (&btmp, scol->type(), scol->name());
    btmp.columns[ctmp->name()] = ctmp;
    int ierr = ctmp->append(*scol, cnd);
    if (ierr < 0) {
        LOGGER(ibis::gVerbose > 2)
            << "Warning -- " << mesg << " failed to retrieve values "
            << "satisfying \"" << cnd << " from partition " << prt.name()
            << ", ierr = " << ierr;
        return -17;
    }
    if (ierr == 0)
        return ierr;
    if ((uint64_t)nEvents+(uint64_t)ierr > 0x7FFFFFFFUL) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << mesg << " can not proceed because the "
            "resulting data partition is too large (" << nEvents << " + "
            << ierr << " = " << nEvents + ierr << " rows)";
        return -18;
    }

    btmp.nEvents = ierr;
    const uint32_t nh = nEvents;
    const uint32_t nqq = ierr;
    amask.adjustSize(0, nh);
    ibis::bitvector newseg; // mask for the new segment of data
    newseg.set(1, nqq);
    const ibis::selectClause::StringToInt& colmap = sc.getOrdered();
    for (columnList::iterator cit = columns.begin();
         cit != columns.end() && ierr >= 0; ++ cit) {
        ibis::bord::column& col =
            *(static_cast<ibis::bord::column*>(cit->second));
        ibis::selectClause::StringToInt::const_iterator mit =
            colmap.find(cit->first);
        if (mit == colmap.end()) {
            mit = colmap.find(col.description());
            if (mit == colmap.end()) {
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- " << mesg << " failed to locate "
                    << cit->first << " in the list of names in " << sc;
                return -13;
            }
        }
        const uint32_t itm = mit->second;
        if (itm >= sc.aggSize()) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- " << mesg << " mapped " << col.name()
                << " into term " << itm << " which is outside of " << sc;
            return -14;
        }
        const ibis::math::term *aterm = sc.aggExpr(itm);

        switch (aterm->termType()) {
        case ibis::math::UNDEF_TERM:
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- " << mesg << " -- can not handle a "
                "math::term of undefined type";
            ierr = -15;
            break;
        case ibis::math::VARIABLE: {
            const ibis::math::variable &var =
                *static_cast<const ibis::math::variable*>(aterm);
            scol = btmp.getColumn(var.variableName());
            if (scol == 0) {
                if (*(var.variableName()) == '*') { // counts
                    col.addCounts(nEvents+nqq);
                }
                else {
                    LOGGER(ibis::gVerbose > 1)
                        << "Warning -- " << mesg << " -- \""
                        << var.variableName()
                        << "\" is unexpected";
                    ierr = -16;
                }
            }
            else {
                LOGGER(ibis::gVerbose > 4)
                    << mesg << " -- adding " << nqq << " element"
                    << (nqq>1?"s":"") << " to column " << cit->first
                    << " from column " << scol->name()
                    << " of partition " << prt.name();
                ierr = col.append(*scol, newseg);
            }
            if (col.getTimeFormat() == 0) {
                if (var.getDecoration() != 0 && *(var.getDecoration()) != 0)
                    col.setTimeFormat(var.getDecoration());
                else if (scol != 0 && scol->getTimeFormat() != 0)
                    col.setTimeFormat(*(scol->getTimeFormat()));
            }
            break;}
        case ibis::math::STRINGFUNCTION1: {
            std::vector<std::string> tmp;
            ierr = btmp.calculate
                (*static_cast<const ibis::math::stringFunction1*>(aterm),
                 newseg, tmp);
            if (ierr > 0) {
                LOGGER(ibis::gVerbose > 2)
                    << mesg << " -- adding " << tmp.size() << " element"
                    << (tmp.size()>1?"s":"") << " to column " << cit->first
                    << " from " << *aterm;
                ierr = col.append(&tmp, newseg);
            }
            break;}
        default: {
            array_t<double> tmp;
            ierr = btmp.calculate(*aterm, newseg, tmp);
            if (ierr > 0) {
                LOGGER(ibis::gVerbose > 2)
                    << mesg << " -- adding " << tmp.size() << " element"
                    << (tmp.size()>1?"s":"") << " to column " << cit->first
                    << " from " << *aterm;
                ierr = col.append(&tmp, newseg);
            }
            break;}
        }
    }
    if (ierr >= 0) {
        ierr = nqq;
        nEvents += nqq;
        amask.adjustSize(nEvents, nEvents);
        LOGGER(ibis::gVerbose > 3)
            << mesg << " -- added " << nqq << " row" << (nqq>1?"s":"")
            << " to make a total of " << nEvents;
    }
    return ierr;
} // ibis::bord::append

ibis::table::cursor* ibis::bord::createCursor() const {
    return new ibis::bord::cursor(*this);
} // ibis::bord::createCursor

// // explicit template function instantiations
// template int
// ibis::bord::addIncoreData<signed char>(void*&, const array_t<signed char>&,
//                                     uint32_t, const signed char);
// template int
// ibis::bord::addIncoreData<unsigned char>(void*&, const array_t<unsigned char>&,
//                                       uint32_t, const unsigned char);
// template int
// ibis::bord::addIncoreData<int16_t>(void*&, const array_t<int16_t>&, uint32_t,
//                                 const int16_t);
// template int
// ibis::bord::addIncoreData<uint16_t>(void*&, const array_t<uint16_t>&, uint32_t,
//                                  const uint16_t);
// template int
// ibis::bord::addIncoreData<int32_t>(void*&, const array_t<int32_t>&, uint32_t,
//                                 const int32_t);
// template int
// ibis::bord::addIncoreData<uint32_t>(void*&, const array_t<uint32_t>&, uint32_t,
//                                  const uint32_t);
// template int
// ibis::bord::addIncoreData<int64_t>(void*&, const array_t<int64_t>&, uint32_t,
//                                 const int64_t);
// template int
// ibis::bord::addIncoreData<uint64_t>(void*&, const array_t<uint64_t>&, uint32_t,
//                                  const uint64_t);
// template int
// ibis::bord::addIncoreData<float>(void*&, const array_t<float>&, uint32_t,
//                               const float);
// template int
// ibis::bord::addIncoreData<double>(void*&, const array_t<double>&, uint32_t,
//                                const double);

/// Allocate a buffer of the specified type and size.
void* ibis::table::allocateBuffer(ibis::TYPE_T type, size_t sz) {
    void* ret = 0;
    switch (type) {
    default:
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- table::allocateBuffer("
            << ibis::TYPESTRING[(int)type] << ", "
            << sz << ") unable to handle the data type";
        break;
    case ibis::OID:
        ret = new array_t<rid_t>(sz);
        break;
    case ibis::BYTE:
        ret = new array_t<signed char>(sz);
        break;
    case ibis::UBYTE:
        ret = new array_t<unsigned char>(sz);
        break;
    case ibis::SHORT:
        ret = new array_t<int16_t>(sz);
        break;
    case ibis::USHORT:
        ret = new array_t<uint16_t>(sz);
        break;
    case ibis::INT:
        ret = new array_t<int32_t>(sz);
        break;
    case ibis::UINT:
        ret = new array_t<uint32_t>(sz);
        break;
    case ibis::LONG:
        ret = new array_t<int64_t>(sz);
        break;
    case ibis::ULONG:
        ret = new array_t<uint64_t>(sz);
        break;
    case ibis::FLOAT:
        ret = new array_t<float>(sz);
        break;
    case ibis::DOUBLE:
        ret = new array_t<double>(sz);
        break;
    case ibis::TEXT:
    case ibis::CATEGORY:
        ret = new std::vector<std::string>(sz);
        break;
    }
    return ret;
} // ibis::table::allocateBuffer

/// Freeing a buffer for storing in-memory values.
///
/// List of actual data types for the incoming buffer, which is assumed to
/// be ibis::bord::column::buffer:
/// - ibis::BIT: ibis::bitvector
/// - ibis::OID: ibis::array_t<ibis::rid_t>
/// - ibis::BYTE: ibis::array_t<signed char>
/// - ibis::UBYTE: ibis::array_t<unsigned char>
/// - ibis::SHORT: ibis::array_t<int16_t>
/// - ibis::USHORT: ibis::array_t<uint16_t>
/// - ibis::INT: ibis::array_t<int32_t>
/// - ibis::UINT: ibis::array_t<uint32_t>
/// - ibis::LONG: ibis::array_t<int64_t>
/// - ibis::ULONG: ibis::array_t<uint64_t>
/// - ibis::FLOAT: ibis::array_t<float>
/// - ibis::DOUBLE: ibis::array_t<double>
/// - ibis::TEXT, ibis::CATEGORY: std::vector<std::string>
void ibis::table::freeBuffer(void *buffer, ibis::TYPE_T type) {
    if (buffer != 0) {
        switch (type) {
        default:
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- table::freeBuffer(" << buffer << ", "
                << (int) type << ") unable to handle data type "
                << ibis::TYPESTRING[(int)type];
            break;
        case ibis::OID:
            delete static_cast<array_t<rid_t>*>(buffer);
            break;
        case ibis::BIT:
            delete static_cast<bitvector*>(buffer);
            break;
        case ibis::BYTE:
            delete static_cast<array_t<signed char>*>(buffer);
            break;
        case ibis::UBYTE:
            delete static_cast<array_t<unsigned char>*>(buffer);
            break;
        case ibis::SHORT:
            delete static_cast<array_t<int16_t>*>(buffer);
            break;
        case ibis::USHORT:
            delete static_cast<array_t<uint16_t>*>(buffer);
            break;
        case ibis::INT:
            delete static_cast<array_t<int32_t>*>(buffer);
            break;
        case ibis::UINT:
            delete static_cast<array_t<uint32_t>*>(buffer);
            break;
        case ibis::LONG:
            delete static_cast<array_t<int64_t>*>(buffer);
            break;
        case ibis::ULONG:
            delete static_cast<array_t<uint64_t>*>(buffer);
            break;
        case ibis::FLOAT:
            delete static_cast<array_t<float>*>(buffer);
            break;
        case ibis::DOUBLE:
            delete static_cast<array_t<double>*>(buffer);
            break;
        case ibis::TEXT:
        case ibis::CATEGORY:
            delete static_cast<std::vector<std::string>*>(buffer);
            break;
        }
    }
} // ibis::table::freeBuffer

/// Freeing a list of buffers.
///
/// @sa ibis::table::freeBuffer
void ibis::table::freeBuffers(ibis::table::bufferArray& buf,
                              ibis::table::typeArray& typ) {
    LOGGER(ibis::gVerbose > 3)
        << "table::freeBuffers to free buf[" << buf.size() << "] and typ["
        << typ.size() << "]";
    const size_t nbt = (buf.size() <= typ.size() ? buf.size() : typ.size());
    LOGGER((nbt < buf.size() || nbt < typ.size()) && ibis::gVerbose > 1)
        << "Warning -- freeBuffers expects buf[" << buf.size()
        << "] and typ[" << typ.size()
        << "] to be the same size, but they are not";

    for (size_t j = 0; j < buf.size(); ++ j) {
        if (buf[j] != 0) {
            switch (typ[j]) {
            default:
                LOGGER(ibis::gVerbose > 1)
                    << "Warning -- table::freeBuffers cann't free "
                    << buf[j] << " of type " << ibis::TYPESTRING[(int)typ[j]];
                break;
            case ibis::OID:
                delete static_cast<array_t<ibis::rid_t>*>(buf[j]);
                break;
            case ibis::BIT:
                delete static_cast<bitvector*>(buf[j]);
                break;
            case ibis::BYTE:
                delete static_cast<array_t<signed char>*>(buf[j]);
                break;
            case ibis::UBYTE:
                delete static_cast<array_t<unsigned char>*>(buf[j]);
                break;
            case ibis::SHORT:
                delete static_cast<array_t<int16_t>*>(buf[j]);
                break;
            case ibis::USHORT:
                delete static_cast<array_t<uint16_t>*>(buf[j]);
                break;
            case ibis::INT:
                delete static_cast<array_t<int32_t>*>(buf[j]);
                break;
            case ibis::UINT:
                delete static_cast<array_t<uint32_t>*>(buf[j]);
                break;
            case ibis::LONG:
                delete static_cast<array_t<int64_t>*>(buf[j]);
                break;
            case ibis::ULONG:
                delete static_cast<array_t<uint64_t>*>(buf[j]);
                break;
            case ibis::FLOAT:
                delete static_cast<array_t<float>*>(buf[j]);
                break;
            case ibis::DOUBLE:
                delete static_cast<array_t<double>*>(buf[j]);
                break;
            case ibis::TEXT:
            case ibis::CATEGORY:
                delete static_cast<std::vector<std::string>*>(buf[j]);
                break;
            }
        }
    }
    buf.clear();
    typ.clear();
} // ibis::table::freeBuffers

/// Constructor.
ibis::bord::column::column(const ibis::bord *tbl, ibis::TYPE_T t,
                           const char *cn, void *st, const char *de,
                           double lo, double hi)
    : ibis::column(tbl, t, cn, de, lo, hi), buffer(st), xreader(0),
      xmeta(0), dic(0) {
    if (buffer != 0) { // check the size of buffer
        uint32_t nr = 0;
        switch (m_type) {
        case ibis::BIT: {
            buffer = new ibis::bitvector(*static_cast<bitvector*>(st));
            nr = static_cast<bitvector*>(st)->size();
            break;}
        case ibis::BYTE: {
            nr = static_cast<array_t<signed char>*>(st)->size();
            if (nr != static_cast<array_t<signed char>*>(st)->size()) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- too many values for bord::column ("
                    << static_cast<array_t<signed char>*>(st)->size()
                    << "), it wraps to " << nr;
                throw "too many values for bord::column" IBIS_FILE_LINE;
            }
            break;}
        case ibis::UBYTE: {
            nr = static_cast<array_t<unsigned char>*>(st)->size();
            if (nr != static_cast<array_t<unsigned char>*>(st)->size()) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- too many values for bord::column ("
                    << static_cast<array_t<unsigned char>*>(st)->size()
                    << "), it wraps to " << nr;
                throw "too many values for bord::column" IBIS_FILE_LINE;
            }
            break;}
        case ibis::SHORT: {
            nr = static_cast<array_t<int16_t>*>(st)->size();
            if (nr != static_cast<array_t<int16_t>*>(st)->size()) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- too many values for bord::column ("
                    << static_cast<array_t<int16_t>*>(st)->size()
                    << "), it wraps to " << nr;
                throw "too many values for bord::column" IBIS_FILE_LINE;
            }
            break;}
        case ibis::USHORT: {
            nr = static_cast<array_t<uint16_t>*>(st)->size();
            if (nr != static_cast<array_t<uint16_t>*>(st)->size()) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- too many values for bord::column ("
                    << static_cast<array_t<uint16_t>*>(st)->size()
                    << "), it wraps to " << nr;
                throw "too many values for bord::column" IBIS_FILE_LINE;
            }
            break;}
        case ibis::INT: {
            nr = static_cast<array_t<int32_t>*>(st)->size();
            if (nr != static_cast<array_t<int32_t>*>(st)->size()) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- too many values for bord::column ("
                    << static_cast<array_t<int32_t>*>(st)->size()
                    << "), it wraps to " << nr;
                throw "too many values for bord::column" IBIS_FILE_LINE;
            }
            break;}
        case ibis::UINT: {
            nr = static_cast<array_t<uint32_t>*>(st)->size();
            if (nr != static_cast<array_t<uint32_t>*>(st)->size()) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- too many values for bord::column ("
                    << static_cast<array_t<uint32_t>*>(st)->size()
                    << "), it wraps to " << nr;
                throw "too many values for bord::column" IBIS_FILE_LINE;
            }
            break;}
        case ibis::LONG: {
            nr = static_cast<array_t<int64_t>*>(st)->size();
            if (nr != static_cast<array_t<int64_t>*>(st)->size()) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- too many values for bord::column ("
                    << static_cast<array_t<int64_t>*>(st)->size()
                    << "), it wraps to " << nr;
                throw "too many values for bord::column" IBIS_FILE_LINE;
            }
            break;}
        case ibis::ULONG: {
            nr = static_cast<array_t<uint64_t>*>(st)->size();
            if (nr != static_cast<array_t<uint64_t>*>(st)->size()) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- too many values for bord::column ("
                    << static_cast<array_t<uint64_t>*>(st)->size()
                    << "), it wraps to " << nr;
                throw "too many values for bord::column" IBIS_FILE_LINE;
            }
            break;}
        case ibis::FLOAT: {
            nr = static_cast<array_t<float>*>(st)->size();
            if (nr != static_cast<array_t<float>*>(st)->size()) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- too many values for bord::column ("
                    << static_cast<array_t<float>*>(st)->size()
                    << "), it wraps to " << nr;
                throw "too many values for bord::column" IBIS_FILE_LINE;
            }
            break;}
        case ibis::DOUBLE: {
            nr = static_cast<array_t<double>*>(st)->size();
            if (nr != static_cast<array_t<double>*>(st)->size()) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- too many values for bord::column ("
                    << static_cast<array_t<double>*>(st)->size()
                    << "), it wraps to " << nr;
                throw "too many values for bord::column" IBIS_FILE_LINE;
            }
            break;}
        case ibis::TEXT: {
            nr = static_cast<std::vector<std::string>*>(st)->size();
            if (nr != static_cast<std::vector<std::string>*>(st)->size()) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- too many values for bord::column ("
                    << static_cast<std::vector<std::string>*>(st)->size()
                    << "), it wraps to " << nr;
                throw "too many values for bord::column" IBIS_FILE_LINE;
            }
            break;}
        case ibis::CATEGORY: {
            nr = static_cast<std::vector<std::string>*>(st)->size();
            // std::vector<std::string> *stv =
            //  static_cast<std::vector<std::string>*>(st);
            // std::vector<std::string> * tmpSortedDic =
            //  new std::vector<std::string>(nr);
            // for (size_t i = 0 ; i < nr ; i++)
            //  (*tmpSortedDic)[i] = (((*stv)[i]));
            // sort(tmpSortedDic->begin(), tmpSortedDic->end());
            // dic = new ibis::dictionary();
            // dic->insert("");
            // for (size_t i = 0 ; i < tmpSortedDic->size() ; i++)
            //  dic->insert((*tmpSortedDic)[i].c_str());
            // delete tmpSortedDic;
            // array_t<uint32_t> *tmp = new array_t<uint32_t>();
            // tmp->resize(nr);
            // for (size_t i = 0 ; i < nr ; i++)
            //  (*tmp)[i] = dic->insert(((*stv)[i]).c_str());
            // buffer = tmp;
            // delete stv;
            if (nr != static_cast<std::vector<std::string>*>(st)->size()) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- too many values for bord::column ("
                    << static_cast<std::vector<std::string>*>(st)->size()
                    << "), it wraps to " << nr;
                throw "too many values for bord::column" IBIS_FILE_LINE;
            }
            break;}
        case ibis::OID: {
            nr = static_cast<array_t<rid_t>*>(st)->size();
            if (nr != static_cast<array_t<rid_t>*>(st)->size()) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- too many values for bord::column ("
                    << static_cast<array_t<rid_t>*>(st)->size()
                    << "), it wraps to " << nr;
                throw "too many values for bord::column" IBIS_FILE_LINE;
            }
            break;}
        case ibis::BLOB: {
            nr = static_cast<std::vector<opaque>*>(st)->size();
            if (nr != static_cast<array_t<opaque>*>(st)->size()) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- too many values for bord::column ("
                    << static_cast<array_t<opaque>*>(st)->size()
                    << "), it wraps to " << nr;
                throw "too many values for bord::column" IBIS_FILE_LINE;
            }
            break;}
        default: {
            LOGGER(ibis::gVerbose >= 0)
                << "Error -- bord::column::ctor can not handle column ("
                << cn << ") with type " << ibis::TYPESTRING[(int)t];
            throw "bord::column unexpected type" IBIS_FILE_LINE;}
        }
        if (tbl != 0) {
            mask_.adjustSize(nr, tbl->nRows());
            LOGGER(nr != tbl->nRows() && ibis::gVerbose > 4)
                << "Warning -- bord::column " << tbl->m_name << '.' << cn
                << " has " << nr << " row" << (nr>1?"s":"") << ", but expected "
                << tbl->nRows();
        }
        else if (buffer != &mask_) {
            mask_.set(1, nr);
        }
        dataflag = 1;
    }
    // else { // allocate buffer
    //     switch (m_type) {
    //     case ibis::BYTE: {
    //         buffer = new array_t<signed char>;
    //         break;}
    //     case ibis::UBYTE: {
    //         buffer = new array_t<unsigned char>;
    //         break;}
    //     case ibis::SHORT: {
    //         buffer = new array_t<int16_t>;
    //         break;}
    //     case ibis::USHORT: {
    //         buffer = new array_t<uint16_t>;
    //         break;}
    //     case ibis::INT: {
    //         buffer = new array_t<int32_t>;
    //         break;}
    //     case ibis::UINT: {
    //         buffer = new array_t<uint32_t>;
    //         break;}
    //     case ibis::LONG: {
    //         buffer = new array_t<int64_t>;
    //         break;}
    //     case ibis::ULONG: {
    //         buffer = new array_t<uint64_t>;
    //         break;}
    //     case ibis::FLOAT: {
    //         buffer = new array_t<float>;
    //         break;}
    //     case ibis::DOUBLE: {
    //         buffer = new array_t<double>;
    //         break;}
    //     case ibis::TEXT:{
    //         buffer = new std::vector<std::string>;
    //         break;}
    //     case ibis::CATEGORY: {
    //         buffer = new std::vector<std::string>;
    //         //dic = new ibis::dictionary();
    //         break;}
    //     case ibis::OID: {
    //         buffer = new array_t<rid_t>;
    //         break;}
    //     case ibis::BLOB: {
    //         buffer = new std::vector<opaque>;
    //         break;}
    //     default: {
    //         LOGGER(ibis::gVerbose >= 0)
    //          << "Error -- bord::column::ctor can not handle column ("
    //          << cn << ") with type " << ibis::TYPESTRING[(int)t];
    //         throw "bord::column unexpected type";}
    //     }
    // }
    LOGGER(ibis::gVerbose > 5 && !m_name.empty())
        << "initialized bord::column " << fullname() << " @ "
        << this << " (" << ibis::TYPESTRING[(int)m_type] << ") from "
        << mask_.size() << " value" << (mask_.size()>1?"s":"") << " @ "
        << st;
} // ibis::bord::column::column

/// Constructor.
///@note Transfer the ownership of @c st to the new @c column object.
ibis::bord::column::column(const ibis::bord *tbl,
                           const ibis::column& old, void *st)
    : ibis::column(tbl, old.type(), old.name(), old.description(),
                   old.lowerBound(), old.upperBound()),
      buffer(st), xreader(0), xmeta(0), dic(0) {
    old.getNullMask(mask_);
    dataflag = (st != 0 ? 1 : -1);
} // ibis::bord::column::column

/// Constructor.
/// Use the external reader.  This is meant for fix-sized elements only,
/// i.e., integers and floating-point numbers.  More specifically, this is
/// not for strings.
ibis::bord::column::column
(FastBitReadExtArray rd, void *ctx, uint64_t *dims, uint64_t nd,
 ibis::TYPE_T t, const char *name, const char *desc, double lo, double hi)
    : ibis::column(0, t, name, desc, lo, hi), buffer(0), xreader(rd),
      xmeta(ctx), dic(0) {
    if (rd == 0 || nd == 0 || dims == 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "collis::ctor must have a valid reader and a valid dims array";
        throw "collis::ctor must have a valid reader and a valid dims array"
            IBIS_FILE_LINE;
    }

    uint64_t nr = *dims;
    for (uint64_t j = 1; j < nd; ++ j)
        nr *= dims[j];

    if (nr > 0x7FFFFFFFUL) {
        LOGGER(ibis::gVerbose >= 0)
            << "collis::ctor can not proceed because array contains "
            << nr << " elements, which is above the 2 billion limit";
        throw "collis::ctor can not handle more than 2 billion elements";
    }
    mask_.set(1, nr);
    (void) setMeshShape(dims, nd);
} // ibis::bord::column::column

/// Constructor.
ibis::bord::column::column(ibis::TYPE_T t, const char *nm, void *st,
                           uint64_t *dim, uint64_t nd)
    : ibis::column(0, t, nm), buffer(st), xreader(0), xmeta(0), dic(0),
      shape(dim, nd) {
    uint64_t nt = 1;
    for (unsigned j = 0; j < nd; ++ j)
        nt *= dim[j];
    if (nt <= 0x7FFFFFFFUL) {
        mask_.set(1, nt);
        dataflag = (st != 0 ? 1 : -1);
    }
    else {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- due to limitation of internal data structure, "
            "a column object can not have more than 0x7FFFFFFF rows, but "
            "the current spec is for " << nt;
        throw "exceeded limit on max no. rows (0x7FFFFFFF)";
    }
} // ibis::bord::column::column

/// Copy constructor.  Performs a shallow copy of the storage buffer.
ibis::bord::column::column(const ibis::bord::column &c)
    : ibis::column(c), buffer(0), xreader(c.xreader), xmeta(c.xmeta),
      dic(c.dic), shape(c.shape) {
    if (c.idx != 0) // to deal with the index only columns
        idx = c.idx->dup();

    if (c.buffer == 0) return;
    switch (c.m_type) {
    case ibis::BIT: {
        buffer = new bitvector(* static_cast<bitvector*>(c.buffer));
        mask_.copy(c.mask_);
        break;}
    case ibis::BYTE: {
        buffer = new array_t<signed char>
            (* static_cast<array_t<signed char>*>(c.buffer));
        mask_.copy(c.mask_);
        break;}
    case ibis::UBYTE: {
        buffer = new array_t<unsigned char>
            (* static_cast<array_t<unsigned char>*>(c.buffer));
        mask_.copy(c.mask_);
        break;}
    case ibis::SHORT: {
        buffer = new array_t<int16_t>
            (* static_cast<array_t<int16_t>*>(c.buffer));
        mask_.copy(c.mask_);
        break;}
    case ibis::USHORT: {
        buffer = new array_t<uint16_t>
            (* static_cast<array_t<uint16_t>*>(c.buffer));
        mask_.copy(c.mask_);
        break;}
    case ibis::INT: {
        buffer = new array_t<int32_t>
            (* static_cast<array_t<int32_t>*>(c.buffer));
        mask_.copy(c.mask_);
        break;}
    case ibis::UINT: {
        buffer = new array_t<uint32_t>
            (* static_cast<array_t<uint32_t>*>(c.buffer));
        mask_.copy(c.mask_);
        break;}
    case ibis::LONG: {
        buffer = new array_t<int64_t>
            (* static_cast<array_t<int64_t>*>(c.buffer));
        mask_.copy(c.mask_);
        break;}
    case ibis::ULONG: {
        buffer = new array_t<uint64_t>
            (* static_cast<array_t<uint64_t>*>(c.buffer));
        mask_.copy(c.mask_);
        break;}
    case ibis::FLOAT: {
        buffer = new array_t<float>
            (* static_cast<array_t<float>*>(c.buffer));
        mask_.copy(c.mask_);
        break;}
    case ibis::DOUBLE: {
        buffer = new array_t<double>
            (* static_cast<array_t<double>*>(c.buffer));
        mask_.copy(c.mask_);
        break;}
    case ibis::CATEGORY:
    case ibis::TEXT: {
        buffer = new std::vector<std::string>
            (* static_cast<std::vector<std::string>*>(c.buffer));
        // const std::vector<std::string> &rhs
        //     (* static_cast<std::vector<std::string>*>(c.buffer));
        // std::vector<std::string> *lhs = new std::vector<std::string>;
        // lhs->insert(lhs->end(), rhs.begin(), rhs.end());
        // buffer = lhs;
        mask_.copy(c.mask_);
        break;}
    case ibis::OID: {
        buffer = new array_t<rid_t>
            (* static_cast<array_t<rid_t>*>(c.buffer));
        mask_.copy(c.mask_);
        break;}
    case ibis::BLOB: {
        buffer = new std::vector<ibis::opaque>
            (* static_cast<std::vector<ibis::opaque>*>(c.buffer));
        mask_.copy(c.mask_);
        break;}
    default: {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- bord::column::ctor can not copy column ("
            << c.name() << ") with type " << ibis::TYPESTRING[(int)c.type()];
        break;}
    }
    dataflag = (buffer != 0 ? 1 : -1);
} // ibis::bord::column::column

ibis::bord::column::~column() {
#if defined(DEBUG) || defined(_DEBUG)
    LOGGER(ibis::gVerbose > 5)
        << "DEBUG -- bord::column[" << (thePart ? thePart->name() : "")
        << '.' << m_name << "] freed buffer at " << buffer;
#endif
    if (buffer != &mask_)
        ibis::table::freeBuffer(buffer, m_type);
} // ibis::bord::column::~column

/// Retrieve the raw data buffer as an ibis::fileManager::storage.  Since
/// this function exposes the internal storage representation, it should
/// not be relied upon for general uses.  This is mostly a convenience
/// for FastBit internal development!
///
/// @note Only fix-sized columns are stored using
/// ibis::fileManager::storage objects.  It will return a nil pointer for
/// string-valued columns.
ibis::fileManager::storage*
ibis::bord::column::getRawData() const {
    ibis::fileManager::storage *str = 0;
    if (buffer == 0 && xreader != 0 && shape.size() > 0) {
        int ierr = -1;
        array_t<uint64_t> starts(shape.size(), 0ULL);
        switch (m_type) {
        default:
            break;
        case ibis::BYTE: {
            array_t<signed char> *tmp =
                new array_t<signed char>(mask_.size());
            ierr = xreader(xmeta, shape.size(), starts.begin(),
                           const_cast<uint64_t*>(shape.begin()),
                           tmp->begin());
            if (ierr >= 0) {
                const_cast<ibis::bord::column*>(this)->setDataflag(1);
                const_cast<ibis::bord::column*>(this)->buffer = tmp;
                str = tmp->getStorage();
                return str;
            }
            break;}
        }
    }
    if (buffer == 0)
        return str;

    switch (m_type) {
    case ibis::OID: {
        str = static_cast<array_t<ibis::rid_t>*>(buffer)->getStorage();
        break;}
    case ibis::BYTE: {
        str = static_cast<array_t<signed char>*>(buffer)->getStorage();
        break;}
    case ibis::UBYTE: {
        str = static_cast<array_t<unsigned char>*>(buffer)->getStorage();
        break;}
    case ibis::SHORT: {
        str = static_cast<array_t<int16_t>*>(buffer)->getStorage();
        break;}
    case ibis::USHORT: {
        str = static_cast<array_t<uint16_t>*>(buffer)->getStorage();
        break;}
    case ibis::INT: {
        str = static_cast<array_t<int32_t>*>(buffer)->getStorage();
        break;}
    case ibis::UINT: {
        str = static_cast<array_t<uint32_t>*>(buffer)->getStorage();
        break;}
    case ibis::LONG: {
        str = static_cast<array_t<int64_t>*>(buffer)->getStorage();
        break;}
    case ibis::ULONG: {
        str = static_cast<array_t<uint64_t>*>(buffer)->getStorage();
        break;}
    case ibis::FLOAT: {
        str = static_cast<array_t<float>*>(buffer)->getStorage();
        break;}
    case ibis::DOUBLE: {
        str = static_cast<array_t<double>*>(buffer)->getStorage();
        break;}
    default: {
        break;}
    }

    return str;
} // ibis::bord::column::getRawData

void ibis::bord::column::computeMinMax() {
    computeMinMax(static_cast<const char*>(0), lower, upper, m_sorted);
} // ibis::bord::column::computeMinMax

void ibis::bord::column::computeMinMax(const char *dir) {
    computeMinMax(dir, lower, upper, m_sorted);
} // ibis::bord::column::computeMinMax

void ibis::bord::column::computeMinMax(const char *, double &min,
                                       double &max, bool &asc) const {
    if (buffer == 0) return;

    switch (m_type) {
    case ibis::BIT: {
        min = 0;
        max = 1;
        asc = false;
        break;}
    case ibis::UBYTE: {
        const array_t<unsigned char> &val =
            * static_cast<const array_t<unsigned char>*>(buffer);

        ibis::column::actualMinMax(val, mask_, min, max, asc);
        break;}
    case ibis::BYTE: {
        const array_t<signed char> &val =
            * static_cast<const array_t<signed char>*>(buffer);

        ibis::column::actualMinMax(val, mask_, min, max, asc);
        break;}
    case ibis::USHORT: {
        const array_t<uint16_t> &val =
            * static_cast<const array_t<uint16_t>*>(buffer);

        ibis::column::actualMinMax(val, mask_, min, max, asc);
        break;}
    case ibis::SHORT: {
        const array_t<int16_t> &val =
            * static_cast<const array_t<int16_t>*>(buffer);

        ibis::column::actualMinMax(val, mask_, min, max, asc);
        break;}
    case ibis::UINT: {
        const array_t<uint32_t> &val =
            * static_cast<const array_t<uint32_t>*>(buffer);

        ibis::column::actualMinMax(val, mask_, min, max, asc);
        break;}
    case ibis::INT: {
        const array_t<int32_t> &val =
            *static_cast<const array_t<int32_t>*>(buffer);

        ibis::column::actualMinMax(val, mask_, min, max, asc);
        break;}
    case ibis::OID:
    case ibis::ULONG: {
        const array_t<uint64_t> &val =
            * static_cast<const array_t<uint64_t>*>(buffer);

        ibis::column::actualMinMax(val, mask_, min, max, asc);
        break;}
    case ibis::LONG: {
        const array_t<int64_t> &val =
            *static_cast<const array_t<int64_t>*>(buffer);

        ibis::column::actualMinMax(val, mask_, min, max, asc);
        break;}
    case ibis::FLOAT: {
        const array_t<float> &val =
            * static_cast<const array_t<float>*>(buffer);

        ibis::column::actualMinMax(val, mask_, min, max, asc);
        break;}
    case ibis::DOUBLE: {
        const array_t<double> &val =
            * static_cast<const array_t<double>*>(buffer);

        ibis::column::actualMinMax(val, mask_, min, max, asc);
        break;}
    default:
        LOGGER(ibis::gVerbose > 4)
            << "column[" << (thePart ? thePart->name() : "")
            << '.' << m_name << "]::computeMinMax -- column type "
            << TYPESTRING[static_cast<int>(m_type)] << " is not one of the "
            "supported types (int, uint, float, double)";
        asc = false;
        min = 0;
        max = (thePart != 0) ? thePart->nRows() : -DBL_MAX;
    } // switch(m_type)
} // ibis::bord::column::computeMinMax

long ibis::bord::column::evaluateRange(const ibis::qContinuousRange& cmp,
                                       const ibis::bitvector& mask,
                                       ibis::bitvector& res) const {
    long ierr = -1;
    ibis::bitvector mymask(mask);
    if (mask_.size() > 0)
        mymask &= mask_;
    if (thePart != 0)
        mymask.adjustSize(0, thePart->nRows());

    std::string evt = "column";
    if (ibis::gVerbose > 1) {
        evt += '[';
        evt += fullname();
        evt += ']';
    }
    evt += "::evaluateRange";
    if (ibis::gVerbose > 1) {
        std::ostringstream oss;
        oss << '(' << cmp;
        if (ibis::gVerbose > 3)
            oss << ", mask(" << mymask.cnt() << ", " << mymask.size() << ')';
        oss << ')';
        evt += oss.str();
    }
    if (buffer == 0) {
        if (idx == 0) {
            if (getRawData() == 0) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " can not proceed because "
                    "it has not data or index";
                return -3;
            }
        }
    }

    if (cmp.leftOperator() == ibis::qExpr::OP_UNDEFINED &&
        cmp.rightOperator() == ibis::qExpr::OP_UNDEFINED) {
        getNullMask(res);
        res &= mymask;
        return res.sloppyCount();
    }

    if (m_type == ibis::BIT) {
        const bool has0 = cmp.inRange(0.0);
        const bool has1 = cmp.inRange(1.0);
        if (has0) {
            if (! has1)
                mymask -= *static_cast<bitvector*>(buffer);
        }
        else if (has1) {
            mymask &= *static_cast<bitvector*>(buffer);
        }
        else {
            mymask.set(0, mask.size());
        }
        res.copy(mymask);
        return res.sloppyCount();
    }

    if (m_type == ibis::UNKNOWN_TYPE || m_type == ibis::UDT ||
        m_type == ibis::BLOB || m_type == ibis::TEXT) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- " << evt << " can not work with column type "
            << ibis::TYPESTRING[(int)m_type];
        ierr = -4;
        return ierr;
    }
    if (! cmp.overlap(lower, upper)) {
        res.set(0, mymask.size());
        return 0;
    }

    ibis::bitvector bv2;
    try {
        { // use a block to limit the scope of index lock
            indexLock lock(this, evt.c_str());
            if (idx != 0) {
                if (hasRawData()) {
                    double cost = idx->estimateCost(cmp);
                    // use index only if the cost of using its estimate cost is
                    // less than N/2 bytes
                    if (cost < mask.size() * 0.5 + 999.0) {
                        idx->estimate(cmp, res, bv2);
                    }
                    else {
                        LOGGER(ibis::gVerbose > 1)
                            << evt << " will not use the index because the "
                            "cost (" << cost << ") is too high";
                    }
                }
                else { // no raw data, must use index
                    idx->estimate(cmp, res, bv2);
                }
            }
            else if (m_sorted) {
                ierr = searchSorted(cmp, res);
                if (ierr < 0)
                    res.clear();
            }
        }
        if (res.size() != mymask.size() && m_sorted) {
            ierr = searchSorted(cmp, res);
            if (ierr < 0)
                res.clear();
        }
        if (res.size() != mymask.size()) { // short index
            if (bv2.size() != res.size())
                bv2.copy(res);
            bv2.adjustSize(mymask.size(), mymask.size());
            res.adjustSize(0, mymask.size());
        }
        res &= mymask;
        if (res.size() == bv2.size()) { // need scan
            bv2 &= mymask;
            bv2 -= res;
            if (bv2.cnt() > 0) {
                mymask.swap(bv2);
            }
        }
        else {
            mymask.clear();
        }

        if (mymask.cnt() == 0) {
            ierr = res.sloppyCount();
            LOGGER(ibis::gVerbose > 3)
                << evt << " completed with ierr = " << ierr;
            LOGGER(ibis::gVerbose > 8)
                << evt << " result --\n" << res;
            return ierr;
        }
    }
    catch (std::exception &se) {
        LOGGER(ibis::gVerbose > 0)
            << evt << " received a std::exception -- " << se.what();
        mymask.copy(mask);
        mymask &= mask_;
        res.clear();
    }
    catch (const char* str) {
        LOGGER(ibis::gVerbose > 0)
            << evt << " received a string exception -- " << str;
        mymask.copy(mask);
        mymask &= mask_;
        res.clear();
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << evt << " received a unanticipated excetpion";
        mymask.copy(mask);
        mymask &= mask_;
        res.clear();
    }

    if (mymask.cnt() == 0) {
        res.swap(mymask);
        return 0;
    }
    if (buffer == 0) return -4;

    switch (m_type) {
    case ibis::UBYTE: {
        const array_t<unsigned char> &val =
            * static_cast<const array_t<unsigned char>*>(buffer);
        ierr = ibis::part::doScan(val, cmp, mymask, bv2);
        break;}
    case ibis::BYTE: {
        const array_t<signed char> &val =
            * static_cast<const array_t<signed char>*>(buffer);
        ierr = ibis::part::doScan(val, cmp, mymask, bv2);
        break;}
    case ibis::USHORT: {
        const array_t<uint16_t> &val =
            * static_cast<const array_t<uint16_t>*>(buffer);
        ierr = ibis::part::doScan(val, cmp, mymask, bv2);
        break;}
    case ibis::SHORT: {
        const array_t<int16_t> &val =
            * static_cast<const array_t<int16_t>*>(buffer);
        ierr = ibis::part::doScan(val, cmp, mymask, bv2);
        break;}
    case ibis::UINT: {
        const array_t<uint32_t> &val =
            * static_cast<const array_t<uint32_t>*>(buffer);
        ierr = ibis::part::doScan(val, cmp, mymask, bv2);
        break;}
    case ibis::INT: {
        const array_t<int32_t> &val =
            * static_cast<const array_t<int32_t>*>(buffer);
        ierr = ibis::part::doScan(val, cmp, mymask, bv2);
        break;}
    case ibis::OID:
    case ibis::ULONG: {
        const array_t<uint64_t> &val =
            * static_cast<const array_t<uint64_t>*>(buffer);
        ierr = ibis::part::doScan(val, cmp, mymask, bv2);
        break;}
    case ibis::LONG: {
        const array_t<int64_t> &val =
            * static_cast<const array_t<int64_t>*>(buffer);
        ierr = ibis::part::doScan(val, cmp, mymask, bv2);
        break;}
    case ibis::FLOAT: {
        const array_t<float> &val =
            * static_cast<const array_t<float>*>(buffer);
        ierr = ibis::part::doScan(val, cmp, mymask, bv2);
        break;}
    case ibis::DOUBLE: {
        const array_t<double> &val =
            * static_cast<const array_t<double>*>(buffer);
        ierr = ibis::part::doScan(val, cmp, mymask, bv2);
        break;}
    default:
        LOGGER(ibis::gVerbose > 2)
            << "Warning -- " << evt << " deos not support column type "
            << TYPESTRING[static_cast<int>(m_type)]
            << ", only supports integers and floats";
        ierr = -2;
    } // switch(m_type)

    if (ierr > 0) {
        if (res.sloppyCount() > 0)
            res |= bv2;
        else
            res.swap(bv2);
    }
    else if (ierr == 0) {
        ierr = res.sloppyCount();
    }
    LOGGER(ibis::gVerbose > 3)
        << evt << " completed with ierr = " << ierr;
    return ierr;
} // ibis::bord::column::evaluateRange

long ibis::bord::column::evaluateRange(const ibis::qDiscreteRange& cmp,
                                       const ibis::bitvector& mask,
                                       ibis::bitvector& res) const {
    long ierr = -1;
    std::string evt = "column[";
    evt += (thePart ? thePart->name() : "?");
    evt += ".";
    evt += m_name;
    evt += "]::evaluateRange";
    if (ibis::gVerbose > 1) {
        std::ostringstream oss;
        oss << '(' << cmp;
        if (ibis::gVerbose > 3)
            oss << ", mask(" << mask.cnt() << ", " << mask.size() << ')';
        oss << ')';
        evt += oss.str();
    }

    ibis::bitvector bv1, mymask(mask);
    if (mask_.size() > 0)
        mymask &= mask_;
    if (thePart != 0)
        mymask.adjustSize(0, thePart->nRows());

    if (m_type == ibis::BIT) {
        const bool has0 = cmp.inRange(0.0);
        const bool has1 = cmp.inRange(1.0);
        if (has0) {
            if (! has1)
                mymask -= *static_cast<bitvector*>(buffer);
        }
        else if (has1) {
            mymask &= *static_cast<bitvector*>(buffer);
        }
        else {
            mymask.set(0, mask.size());
        }
        res.copy(mymask);
        return res.sloppyCount();
    }

    if (m_type == ibis::UNKNOWN_TYPE || m_type == ibis::UDT ||
        m_type == ibis::BLOB || m_type == ibis::TEXT) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- " << evt << " can not work with column type "
            << ibis::TYPESTRING[(int)m_type];
        ierr = -4;
        return ierr;
    }
    if (m_type != ibis::FLOAT && m_type != ibis::DOUBLE &&
        cmp.getValues().size() ==
        1+(cmp.getValues().back()-cmp.getValues().front())) {
        // a special case -- actually a continuous range
        ibis::qContinuousRange cr(cmp.getValues().front(), ibis::qExpr::OP_LE,
                                  cmp.colName(), ibis::qExpr::OP_LE,
                                  cmp.getValues().back());
        return evaluateRange(cr, mask, res);
    }
    if (! cmp.overlap(lower, upper)) {
        res.set(0, mask.size());
        return 0;
    }

    try {
        indexLock lock(this, evt.c_str());
        if (idx != 0) {
            if (hasRawData()) { // consider both index and raw data
                double idxcost = idx->estimateCost(cmp) *
                    (1.0 + log((double)cmp.getValues().size()));
                if (m_sorted && idxcost > mymask.size()) {
                    ierr = searchSorted(cmp, res);
                    if (ierr == 0) {
                        res &= mymask;
                        return res.sloppyCount();
                    }
                }

                if (idxcost <= (elementSize()+4.0) * mask.size() + 999.0) {
                    // the normal indexing option
                    ierr = idx->evaluate(cmp, res);
                    if (ierr >= 0) {
                        if (res.size() < mymask.size()) { // short index, scan
                            bv1.appendFill(0, res.size());
                            bv1.appendFill(1, mymask.size()-res.size());
                            bv1 &= mymask;
                            if (bv1.cnt() == 0) {
                                res &= mymask;
                                return res.sloppyCount();
                            }
                            else {
                                res &= mymask;
                                mymask.swap(bv1);
                            }
                        }
                        else {
                            res &= mymask;
                            return res.sloppyCount();
                        }
                    }
                }
            }
            else {
                ierr = idx->evaluate(cmp, res);
            }
            if (ierr < 0) { // index::evaluate failed, try index::estimate
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                LOGGER(ibis::gVerbose > 2)
                    << "INFO -- " << evt << " -- idx(" << idx->name()
                    << ")->evaluate returned " << ierr
                    << ", try index::estimate";
#endif
                idx->estimate(cmp, res, bv1);
                if (res.size() != mymask.size()) {
                    if (bv1.size() == res.size()) {
                        bv1.adjustSize(mymask.size(), mymask.size());
                    }
                    else if (bv1.size() == 0) {
                        bv1.copy(res);
                        bv1.adjustSize(mymask.size(), mymask.size());
                    }
                    res.adjustSize(0, mymask.size());
                }
                res &= mymask;
                if (bv1.size() == res.size()) {
                    bv1 &= mymask;
                    bv1 -= res;
                    if (bv1.cnt() == 0) {
                        return res.sloppyCount();
                    }
                    else {
                        mymask.swap(bv1);
                    }
                }
                else { // assume to have exact answer already
                    return res.sloppyCount();
                }
            }
        }

        if (mymask.cnt() == 0) {
            ierr = res.sloppyCount();
            LOGGER(ibis::gVerbose > 3)
                << evt << " completed with ierr = " << ierr;
            return ierr;
        }
    }
    catch (std::exception &se) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning-- " << evt << " received a std::exception -- "
            << se.what();
    }
    catch (const char* str) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " received a string exception -- "
            << str;
    }
    catch (...) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning" << evt << " received a unanticipated excetpion";
    }

    switch (m_type) {
    case ibis::UBYTE: {
        const array_t<unsigned char> &val =
            * static_cast<const array_t<unsigned char>*>(buffer);
        ierr = ibis::part::doScan(val, cmp, mymask, bv1);
        break;}
    case ibis::BYTE: {
        const array_t<signed char> &val =
            * static_cast<const array_t<signed char>*>(buffer);
        ierr = ibis::part::doScan(val, cmp, mymask, bv1);
        break;}
    case ibis::USHORT: {
        const array_t<uint16_t> &val =
            * static_cast<const array_t<uint16_t>*>(buffer);
        ierr = ibis::part::doScan(val, cmp, mymask, bv1);
        break;}
    case ibis::SHORT: {
        const array_t<int16_t> &val =
            * static_cast<const array_t<int16_t>*>(buffer);
        ierr = ibis::part::doScan(val, cmp, mymask, bv1);
        break;}
    case ibis::UINT: {
        const array_t<uint32_t> &val =
            * static_cast<const array_t<uint32_t>*>(buffer);
        ierr = ibis::part::doScan(val, cmp, mymask, bv1);
        break;}
    case ibis::INT: {
        const array_t<int32_t> &val =
            * static_cast<const array_t<int32_t>*>(buffer);
        ierr = ibis::part::doScan(val, cmp, mymask, bv1);
        break;}
    case ibis::OID:
    case ibis::ULONG: {
        const array_t<uint64_t> &val =
            * static_cast<const array_t<uint64_t>*>(buffer);
        ierr = ibis::part::doScan(val, cmp, mymask, bv1);
        break;}
    case ibis::LONG: {
        const array_t<int64_t> &val =
            * static_cast<const array_t<int64_t>*>(buffer);
        ierr = ibis::part::doScan(val, cmp, mymask, bv1);
        break;}
    case ibis::FLOAT: {
        const array_t<float> &val =
            * static_cast<const array_t<float>*>(buffer);
        ierr = ibis::part::doScan(val, cmp, mymask, bv1);
        break;}
    case ibis::DOUBLE: {
        const array_t<double> &val =
            * static_cast<const array_t<double>*>(buffer);
        ierr = ibis::part::doScan(val, cmp, mymask, bv1);
        break;}
    default:
        LOGGER(ibis::gVerbose > 2)
            << "Warning -- " << evt << " deos not support column type "
            << TYPESTRING[static_cast<int>(m_type)]
            << ", only supports integers and floats";
        ierr = -2;
    } // switch(m_type)

    if (ierr > 0) {
        if (res.sloppyCount() > 0)
            res |= bv1;
        else
            res.swap(bv1);
    }
    else if (ierr >= 0) {
        ierr = res.sloppyCount();
    }
    LOGGER(ibis::gVerbose > 3)
        << evt << " completed with ierr = " << ierr;
    return ierr;
} // ibis::bord::column::evaluateRange

/// Locate the strings that match the given string.  The comaprison is case
/// sensitive.  If the incoming string is a nil pointer, it matches nothing.
long ibis::bord::column::stringSearch(const char* str,
                                      ibis::bitvector& hits) const {
    if (str == 0) { // null string can not match any thing
        hits.set(0, thePart ? thePart->nRows() : 0);
        return 0;
    }

    std::string evt = "column[";
    evt += (thePart ? thePart->name() : "");
    evt += '.';
    evt += m_name;
    evt += "]::stringSearch(";
    evt += (str ? str : "");
    evt += ')';
    if (buffer == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " can not proceed with a nil buffer";
        return -1;
    }
    if (m_type == ibis::TEXT || m_type == ibis::CATEGORY) {
        // buffer is std::vector<std::string>*
        ibis::util::timer mytimer(evt.c_str(), 3);
        const std::vector<std::string>&
            vals(*static_cast<const std::vector<std::string>*>(buffer));

        hits.clear();
        for (size_t j = 0; j < vals.size(); ++ j) {
            if (vals[j].compare(str) == 0)
                hits.setBit(j, 1);
        }
        hits.adjustSize(0, thePart ? thePart->nRows() : vals.size());
    }
    else if (m_type == ibis::UINT && dic != 0) {
        // buffer is ibis::array_t<uint32_t>
        ibis::util::timer mytimer(evt.c_str(), 3);
        const uint32_t stri = (*dic)[str];
        const array_t<uint32_t>&
            vals(*static_cast<const array_t<uint32_t>*>(buffer));

        hits.clear();
        for (size_t j = 0; j < vals.size(); ++ j) {
            if (vals[j] == stri)
                hits.setBit(j, 1);
        }
        hits.adjustSize(0, thePart ? thePart->nRows() : vals.size());
    }
    else {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " is not supported on column type "
            << ibis::TYPESTRING[(int)m_type];
        return -2;
    }

    return hits.cnt();
} // ibis::bord::column::stringSearch

long ibis::bord::column::stringSearch(const std::vector<std::string>& str,
                                      ibis::bitvector& hits) const {
    if (str.empty()) { // null string can not match any thing
        hits.set(0, thePart ? thePart->nRows() : 0);
        return 0;
    }

    std::string evt = "column[";
    evt += (thePart ? thePart->name() : "");
    evt += '.';
    evt += m_name;
    evt += "]::stringSearch(<...>)";
    if (buffer == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " can not proceed with a nil buffer";
        return -1;
    }

    if (m_type == ibis::TEXT || m_type == ibis::CATEGORY) {
        // buffer is std::vector<std::string>*
        ibis::util::timer mytimer(evt.c_str(), 3);
        const std::vector<std::string>&
            vals(*static_cast<const std::vector<std::string>*>(buffer));

        hits.clear();
        bool hit;
        for (size_t j = 0; j < vals.size(); ++ j) {
            hit = false;
            for (size_t i = 0; i < str.size() && !hit; ++ i)
                hit = (vals[j].compare(str[i]) == 0);
            if (hit)
                hits.setBit(j, 1);
        }
        hits.adjustSize(0, thePart ? thePart->nRows() : vals.size());
    }
    else if (m_type == ibis::UINT && dic != 0) {
        // buffer is ibis::array_t<uint32_t>
        ibis::util::timer mytimer(evt.c_str(), 3);
        const array_t<uint32_t>&
            vals(*static_cast<const array_t<uint32_t>*>(buffer));
        ibis::array_t<uint32_t> stri(str.size());
        for (unsigned j = 0; j < str.size(); ++ j)
            stri[j] = (*dic)[str[j].c_str()];
        stri.deduplicate();

        hits.clear();
        if (hits.size() == 1) {
            for (size_t j = 0; j < vals.size(); ++ j) {
                if (vals[j] == stri[0])
                    hits.setBit(j, 1);
            }
        }
        else if (hits.size() == 2) {
            for (size_t j = 0; j < vals.size(); ++ j) {
                if (vals[j] == stri[0] || vals[j] == stri[1])
                    hits.setBit(j, 1);
            }
        }
        else {
            for (size_t j = 0; j < vals.size(); ++ j) {
                if (vals[j] == stri[stri.find_upper(vals[j])])
                    hits.setBit(j, 1);
            }
        }
        hits.adjustSize(0, thePart ? thePart->nRows() : vals.size());
    }
    else {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " is not supported on column type "
            << ibis::TYPESTRING[(int)m_type];
        return -1;
    }
    return hits.cnt();
} // ibis::bord::column::stringSearch

/// Compute an estimate of the maximum number of possible matches.  This is
/// a trivial implementation that does not actually perform any meaningful
/// checks.  It simply returns the number of strings in memory as the
/// estimate.
long ibis::bord::column::stringSearch(const char* str) const {
    if (m_type != ibis::TEXT && m_type != ibis::CATEGORY &&
        (m_type == ibis::UINT && dic != 0)) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- column[" << (thePart ? thePart->name() : "") << '.'
            << m_name << "]::stringSearch is not supported on column type "
            << ibis::TYPESTRING[(int)m_type];
        return -1;
    }
    if (buffer == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- column[" << (thePart ? thePart->name() : "") << '.'
            << m_name << "]::stringSearch can not proceed with a nil buffer";
        return -2;
    }
    if (str == 0) return 0;

    const array_t<uint32_t>&
        vals(*static_cast<const array_t<uint32_t>*>(buffer));
    return vals.size();
} // ibis::bord::column::stringSearch

/// Compute an estimate of the maximum number of possible matches.  This is
/// a trivial implementation that does not actually perform any meaningful
/// checks.  It simply returns the number of strings in memory as the
/// estimate.
long
ibis::bord::column::stringSearch(const std::vector<std::string>& str) const {
    if (m_type != ibis::TEXT && m_type != ibis::CATEGORY &&
        (m_type == ibis::UINT && dic != 0)) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- column[" << (thePart ? thePart->name() : "") << '.'
            << m_name << "]::stringSearch is not supported on column type "
            << ibis::TYPESTRING[(int)m_type];
        return -1;
    }
    if (buffer == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- column[" << (thePart ? thePart->name() : "") << '.'
            << m_name << "]::stringSearch can not proceed with a nil buffer";
        return -2;
    }
    if (str.empty()) return 0;

    const array_t<uint32_t>&
        vals(*static_cast<const array_t<uint32_t>*>(buffer));
    return vals.size();
} // ibis::bord::column::stringSearch

/// Find the given keyword and return the rows.
long ibis::bord::column::keywordSearch(const char* key,
                                       ibis::bitvector& hits) const {
    hits.clear();
    if (key == 0 || *key == 0) return 0;

    std::string evt = "bord::column[";
    evt += (thePart ? thePart->name() : "");
    evt += '.';
    evt += m_name;
    evt += "]::keywordSearch";
    if (m_type != ibis::TEXT && m_type != ibis::CATEGORY) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " is not supported on column type "
            << ibis::TYPESTRING[(int)m_type];
        return -1;
    }
    if (buffer == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt
            << "]::keywordSearch can not proceed with a nil buffer";
        return -2;
    }

    const std::vector<std::string>&
        vals(*static_cast<const std::vector<std::string>*>(buffer));
    ibis::fileManager::buffer<char> buf(1024);
    ibis::keywords::tokenizer tknz;
    std::vector<const char*> ks;
    ibis::util::timer mytimer(evt.c_str(), 3);
    for (unsigned j = 0; j < vals.size(); ++ j) {
        if (vals[j].empty()) continue;

        if (buf.size() < vals[j].size()) {
            if (buf.resize(vals[j].size()+buf.size()) < vals[j].size()) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " failed to allocate space "
                    "for storing string value in row " << j;
                hits.clear();
                return -3;
            }
        }
        (void) memcpy(buf.address(), vals[j].c_str(), vals[j].size());
        (void) tknz(ks, buf.address());
        LOGGER(ks.empty() && ibis::gVerbose > 2)
            << evt << " could not extract any token from string \""
            << vals[j] << "\"";

        bool hit = false;
        for (unsigned i = 0; i < ks.size() && !hit; ++ i)
            hit = (0 == std::strcmp(key, ks[i]));
        if (hit)
            hits.setBit(j, 1);
    }
    hits.adjustSize(0, thePart ? thePart->nRows() : vals.size());
    return hits.cnt();
} // ibis::bord::column::keywordSearch

/// Return an upper bound on the number of matches.
long ibis::bord::column::keywordSearch(const char* str) const {
    if (m_type != ibis::TEXT && m_type != ibis::CATEGORY) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- column[" << (thePart ? thePart->name() : "") << '.'
            << m_name << "]::keywordSearch is not supported on column type "
            << ibis::TYPESTRING[(int)m_type];
        return -1;
    }
    if (buffer == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- column[" << (thePart ? thePart->name() : "") << '.'
            << m_name << "]::keywordSearch can not proceed with a nil buffer";
        return -2;
    }
    if (str == 0) return 0;

    const std::vector<std::string>&
        vals(*static_cast<const std::vector<std::string>*>(buffer));
    return vals.size();
} // ibis::bord::column::keywordSearch

/// Find the given keyword and return the rows.
long ibis::bord::column::keywordSearch(const std::vector<std::string> &keys,
                                       ibis::bitvector& hits) const {
    hits.clear();
    if (keys.empty()) return 0;

    std::string evt = "bord::column[";
    evt += (thePart ? thePart->name() : "");
    evt += '.';
    evt += m_name;
    evt += "]::keywordSearch";
    if (m_type != ibis::TEXT && m_type != ibis::CATEGORY) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " is not supported on column type "
            << ibis::TYPESTRING[(int)m_type];
        return -1;
    }
    if (buffer == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt
            << "]::keywordSearch can not proceed with a nil buffer";
        return -2;
    }

    const std::vector<std::string>&
        vals(*static_cast<const std::vector<std::string>*>(buffer));
    ibis::fileManager::buffer<char> buf(1024);
    ibis::keywords::tokenizer tknz;
    std::vector<const char*> ks;
    ibis::util::timer mytimer(evt.c_str(), 3);
    for (unsigned j = 0; j < vals.size(); ++ j) {
        if (vals[j].empty()) continue;

        if (buf.size() < vals[j].size()) {
            if (buf.resize(vals[j].size()+buf.size()) < vals[j].size()) {
                LOGGER(ibis::gVerbose > 0)
                    << "Warning -- " << evt << " failed to allocate space "
                    "for storing string value in row " << j;
                hits.clear();
                return -3;
            }
        }
        (void) memcpy(buf.address(), vals[j].c_str(), vals[j].size());
        (void) tknz(ks, buf.address());
        LOGGER(ks.empty() && ibis::gVerbose > 2)
            << evt << " could not extract any token from string \""
            << vals[j] << "\"";

        bool hit = false;
        for (unsigned i = 0; i < ks.size() && !hit; ++ i)
            for (unsigned j = 0; j < keys.size() && !hit; ++ j)
                hit = (0 == std::strcmp(keys[j].c_str(), ks[i]));
        if (hit)
            hits.setBit(j, 1);
    }
    hits.adjustSize(0, thePart ? thePart->nRows() : vals.size());
    return hits.cnt();
} // ibis::bord::column::keywordSearch

/// Return an upper bound on the number of matches.
long ibis::bord::column::keywordSearch(const std::vector<std::string>&) const {
    if (m_type != ibis::TEXT && m_type != ibis::CATEGORY) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- column[" << (thePart ? thePart->name() : "") << '.'
            << m_name << "]::keywordSearch is not supported on column type "
            << ibis::TYPESTRING[(int)m_type];
        return -1;
    }
    if (buffer == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- column[" << (thePart ? thePart->name() : "") << '.'
            << m_name << "]::keywordSearch can not proceed with a nil buffer";
        return -2;
    }

    const std::vector<std::string>&
        vals(*static_cast<const std::vector<std::string>*>(buffer));
    return vals.size();
} // ibis::bord::column::keywordSearch

/// Compute an estimate of the maximum number of possible matches.  This is
/// a trivial implementation that does not actually perform any meaningful
/// checks.  It simply returns the number of strings in memory as the
/// estimate.
long ibis::bord::column::patternSearch(const char* pat) const {
    if (m_type != ibis::TEXT && m_type != ibis::CATEGORY) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- column[" << (thePart ? thePart->name() : "") << '.'
            << m_name << "]::patternSearch is not supported on column type "
            << ibis::TYPESTRING[(int)m_type];
        return -1;
    }
    if (buffer == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- column[" << (thePart ? thePart->name() : "") << '.'
            << m_name << "]::patternSearch can not proceed with a nil buffer";
        return -2;
    }
    if (pat == 0) return 0;

    const array_t<uint32_t>&
        vals(*static_cast<const array_t<uint32_t>*>(buffer));
    return vals.size();
} // ibis::bord::column::patternSearch

long ibis::bord::column::patternSearch(const char* pat,
                                       ibis::bitvector &hits) const {
    std::string evt = "column[";
    evt += (thePart ? thePart->name() : "");
    evt += '.';
    evt += m_name;
    evt += "]::patternSearch(";
    evt += (pat ? pat : "");
    evt += ')';
    if (m_type != ibis::TEXT && m_type != ibis::CATEGORY) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " is not supported on column type "
            << ibis::TYPESTRING[(int)m_type];
        return -1;
    }
    if (buffer == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- " << evt << " can not proceed with a nil buffer";
        return -2;
    }

    ibis::util::timer mytimer(evt.c_str(), 3);
    if (0 == dic) {
        const std::vector<std::string>&
            vals(*static_cast<const std::vector<std::string>*>(buffer));

        hits.clear();
        for (size_t j = 0; j < vals.size(); ++ j) {
            if (ibis::util::strMatch(vals[j].c_str(), pat))
                hits.setBit(j, 1);
        }

        hits.adjustSize(0, thePart ? thePart->nRows() : vals.size());
    }
    else {
        const array_t<uint32_t>&
            vals(*static_cast<const array_t<uint32_t>*>(buffer));

        if (pat == 0) { // null string can not match any thing
            hits.set(0, thePart ? thePart->nRows() : vals.size());
            return 0;
        }

        hits.clear();

        array_t<uint32_t> stri;
        for (size_t j = 0; j < (*dic).size(); ++ j) {
            if (ibis::util::strMatch((*dic)[j], pat))
                stri.push_back(j);
        }

        hits.clear();
        for (size_t j = 0; j < vals.size(); ++ j) {
            bool hit = false;
            for (size_t i = 0; i < stri.size() && hit == false; ++ i) {
                hit = (stri[i] == vals[j]);
            }
            if (hit) {
                hits.setBit(j, 1);
            }
        }

        hits.adjustSize(0, thePart ? thePart->nRows() : vals.size());
    }
    return hits.cnt();
} // ibis::bord::column::patternSearch

ibis::array_t<signed char>*
ibis::bord::column::selectBytes(const ibis::bitvector &mask) const {
    ibis::array_t<signed char>* array = new array_t<signed char>;
    const uint32_t tot = mask.cnt();
    if (tot == 0 || buffer == 0)
        return array;

    ibis::horometer timer;
    if (ibis::gVerbose > 5)
        timer.start();
    array->reserve(mask.cnt());
    if (m_type == ibis::BIT) {
        const bitvector &bm = * static_cast<const bitvector*>(buffer);
        bitvector::indexSet idx = mask.firstIndexSet();
        bitvector::const_iterator bit = bm.begin();
        bitvector::word_t pos = 0;
        const size_t nprop = bm.size();
        if (nprop >= mask.size()) {
            while (idx.nIndices() > 0) {
                const bitvector::word_t *iis = idx.indices();
                if (idx.isRange()) {
                    bit += (*iis - pos);
                    for (unsigned j = *iis; j < iis[1]; ++ j, ++ bit) {
                        array->push_back(*bit);
                    }
                    pos = iis[1];
                }
                else {
                    for (unsigned j = 0; j < idx.nIndices(); ++ j) {
                        bit += (iis[j] - pos);
                        pos = iis[j];
                        array->push_back(*bit);
                    }
                }
                ++ idx;
            }
        }
        else { // needs to check for array bounds
            while (idx.nIndices() > 0 && pos < nprop) {
                const bitvector::word_t *iis = idx.indices();
                if (idx.isRange()) {
                    bit += (*iis - pos);
                    unsigned jmax = (nprop >= iis[1] ? iis[1] : nprop);
                    for (unsigned j = *iis; j < jmax; ++ j, ++ bit) {
                        array->push_back(*bit);
                    }
                    pos = iis[1];
                }
                else {
                    for (unsigned j = 0; j < idx.nIndices() && pos < nprop;
                         ++ j) {
                        bit += (iis[j] - pos);
                        pos = iis[j];
                        array->push_back(*bit);
                    }
                }
                ++ idx;
            }
        }
    }
    else if (m_type == ibis::BYTE) {
        const array_t<signed char> &prop =
            * static_cast<const array_t<signed char>*>(buffer);
        const uint32_t nprop = prop.size();
        uint32_t i = 0;
        if (tot < nprop)
            array->resize(tot);
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (tot >= nprop) {
            ibis::array_t<signed char> tmp(prop);
            array->swap(tmp);
            i = nprop;
        }
        else if (nprop >= mask.size()) { // no need to check loop bounds
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectBytes", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else {
        logWarning("selectBytes", "incompatible data type");
    }
    if (ibis::gVerbose > 5) {
        timer.stop();
        long unsigned cnt = mask.cnt();
        logMessage("selectBytes", "retrieving %lu integer%s "
                   "took %g sec(CPU), %g sec(elapsed)",
                   static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                   timer.CPUTime(), timer.realTime());
    }
    return array;
} // ibis::bord::column::selectBytes

ibis::array_t<unsigned char>*
ibis::bord::column::selectUBytes(const ibis::bitvector& mask) const {
    array_t<unsigned char>* array = new array_t<unsigned char>;
    const uint32_t tot = mask.cnt();
    if (tot == 0 || buffer == 0)
        return array;
    ibis::horometer timer;
    if (ibis::gVerbose > 5)
        timer.start();

    if (m_type == UBYTE) {
        const array_t<unsigned char> &prop =
            * static_cast<const array_t<unsigned char>*>(buffer);
        const uint32_t nprop = prop.size();
        uint32_t i = 0;
        if (tot < nprop)
            array->resize(tot);
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (tot >= nprop) {
            ibis::array_t<unsigned char> tmp(prop);
            array->swap(tmp);
            i = nprop;
        }
        else if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectUBytes", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else {
        logWarning("selectUBytes", "incompatible data type");
    }
    if (ibis::gVerbose > 5) {
        timer.stop();
        long unsigned cnt = mask.cnt();
        logMessage("selectUBytes", "retrieving %lu unsigned integer%s "
                   "took %g sec(CPU), %g sec(elapsed)",
                   static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                   timer.CPUTime(), timer.realTime());
    }
    return array;
} // ibis::bord::column::selectUBytes

ibis::array_t<int16_t>*
ibis::bord::column::selectShorts(const ibis::bitvector &mask) const {
    ibis::array_t<int16_t>* array = new array_t<int16_t>;
    const uint32_t tot = mask.cnt();
    if (tot == 0 || buffer == 0)
        return array;

    ibis::horometer timer;
    if (ibis::gVerbose > 5)
        timer.start();
    if (m_type == ibis::SHORT) {
        const array_t<int16_t> &prop =
            * static_cast<const array_t<int16_t>*>(buffer);
        const uint32_t nprop = prop.size();
        uint32_t i = 0;
        if (tot < nprop)
            array->resize(tot);
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (tot >= nprop) {
            ibis::array_t<int16_t> tmp(prop);
            array->swap(tmp);
            i = nprop;
        }
        else if (nprop >= mask.size()) { // no need to check loop bounds
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectShorts", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else if (m_type == ibis::BYTE) {
        const array_t<signed char> &prop =
            * static_cast<const array_t<signed char>*>(buffer);
        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) { // no need to check loop bounds
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectShorts", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else if (m_type == ibis::UBYTE) {
        const array_t<unsigned char> &prop =
            * static_cast<const array_t<unsigned char>*>(buffer);
        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) { // no need to check loop bounds
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectShorts", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else {
        logWarning("selectShorts", "incompatible data type");
    }
    if (ibis::gVerbose > 5) {
        timer.stop();
        long unsigned cnt = mask.cnt();
        logMessage("selectShorts", "retrieving %lu integer%s "
                   "took %g sec(CPU), %g sec(elapsed)",
                   static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                   timer.CPUTime(), timer.realTime());
    }
    return array;
} // ibis::bord::column::selectShorts

ibis::array_t<uint16_t>*
ibis::bord::column::selectUShorts(const ibis::bitvector& mask) const {
    array_t<uint16_t>* array = new array_t<uint16_t>;
    const uint32_t tot = mask.cnt();
    if (tot == 0 || buffer == 0)
        return array;
    ibis::horometer timer;
    if (ibis::gVerbose > 5)
        timer.start();

    if (m_type == USHORT) {
        const array_t<uint16_t> &prop =
            * static_cast<const array_t<uint16_t>*>(buffer);
        const uint32_t nprop = prop.size();
        uint32_t i = 0;
        if (tot < nprop)
            array->resize(tot);
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (tot >= nprop) {
            ibis::array_t<uint16_t> tmp(prop);
            array->swap(tmp);
            i = nprop;
        }
        else if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectUShorts", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else if (m_type == UBYTE) {
        const array_t<unsigned char> &prop =
            * static_cast<const array_t<unsigned char>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectUShorts", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else {
        logWarning("selectUShorts", "incompatible data type");
    }
    if (ibis::gVerbose > 5) {
        timer.stop();
        long unsigned cnt = mask.cnt();
        logMessage("selectUShorts", "retrieving %lu unsigned integer%s "
                   "took %g sec(CPU), %g sec(elapsed)",
                   static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                   timer.CPUTime(), timer.realTime());
    }
    return array;
} // ibis::bord::column::selectUShorts

ibis::array_t<int32_t>*
ibis::bord::column::selectInts(const ibis::bitvector &mask) const {
    ibis::array_t<int32_t>* array = new array_t<int32_t>;
    const uint32_t tot = mask.cnt();
    if (tot == 0 || buffer == 0)
        return array;

    ibis::horometer timer;
    if (ibis::gVerbose > 5)
        timer.start();
    if (m_type == ibis::INT) {
        const array_t<int32_t> &prop =
            * static_cast<const array_t<int32_t>*>(buffer);
        const long unsigned nprop = prop.size();
        uint32_t i = 0;
#if DEBUG+0 > 0 || _DEBUG+0 > 0
        logMessage("DEBUG", "selectInts mask.size(%lu) and nprop=%lu",
                   static_cast<long unsigned>(mask.size()), nprop);
#endif
        if (tot < nprop)
            array->resize(tot);
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (tot >= nprop) {
            ibis::array_t<int32_t> tmp(prop);
            array->swap(tmp);
            i = nprop;
        }
        else if (nprop >= mask.size()) { // no need to check loop bounds
#if DEBUG+0 > 0 || _DEBUG+0 > 0
            logMessage("DEBUG", "entering unchecked loops");
#endif
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                    logMessage("DEBUG", "copying range [%lu, %lu), i=%lu",
                               static_cast<long unsigned>(*idx0),
                               static_cast<long unsigned>(idx0[1]),
                               static_cast<long unsigned>(i));
#endif
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                        logMessage("DEBUG", "copying value %lu to i=%lu",
                                   static_cast<long unsigned>(idx0[j]),
                                   static_cast<long unsigned>(i));
#endif
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
#if DEBUG+0 > 0 || _DEBUG+0 > 0
            logMessage("DEBUG", "entering checked loops");
#endif
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                    logMessage("DEBUG", "copying range [%lu, %lu), i=%lu",
                               static_cast<long unsigned>(*idx0),
                               static_cast<long unsigned>
                               (idx0[1]<=nprop ? idx0[1] : nprop),
                               static_cast<long unsigned>(i));
#endif
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                        logMessage("DEBUG", "copying value %lu to i=%lu",
                                   static_cast<long unsigned>(idx0[j]),
                                   static_cast<long unsigned>(i));
#endif
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectInts", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else if (m_type == ibis::SHORT) {
        const array_t<int16_t> &prop =
            * static_cast<const array_t<int16_t>*>(buffer);
        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) { // no need to check loop bounds
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectInts", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else if (m_type == ibis::USHORT) {
        const array_t<uint16_t> &prop =
            * static_cast<const array_t<uint16_t>*>(buffer);
        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) { // no need to check loop bounds
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectInts", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else if (m_type == ibis::BYTE) {
        const array_t<signed char> &prop =
            * static_cast<const array_t<signed char>*>(buffer);
        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) { // no need to check loop bounds
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectInts", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else if (m_type == ibis::UBYTE) {
        const array_t<unsigned char> &prop =
            * static_cast<const array_t<unsigned char>*>(buffer);
        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) { // no need to check loop bounds
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectInts", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else {
        logWarning("selectInts", "incompatible data type");
    }
    if (ibis::gVerbose > 5) {
        timer.stop();
        long unsigned cnt = mask.cnt();
        logMessage("selectInts", "retrieving %lu integer%s "
                   "took %g sec(CPU), %g sec(elapsed)",
                   static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                   timer.CPUTime(), timer.realTime());
    }
    return array;
} // ibis::bord::column::selectInts

/// Can be called on columns of unsigned integral types, UINT, CATEGORY,
/// USHORT, and UBYTE.
ibis::array_t<uint32_t>*
ibis::bord::column::selectUInts(const ibis::bitvector& mask) const {
    array_t<uint32_t>* array = new array_t<uint32_t>;
    const uint32_t tot = mask.cnt();
    if (tot == 0 || buffer == 0)
        return array;
    ibis::horometer timer;
    if (ibis::gVerbose > 5)
        timer.start();

    if (m_type == ibis::UINT) {
        const array_t<uint32_t> &prop =
            * static_cast<const array_t<uint32_t>*>(buffer);
        const uint32_t nprop = prop.size();
        uint32_t i = 0;
        if (tot < nprop)
            array->resize(tot);
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (tot >= nprop) {
            ibis::array_t<uint32_t> tmp(prop);
            array->swap(tmp);
            i = nprop;
        }
        else if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectUInts", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else if (m_type == USHORT) {
        const array_t<uint16_t> &prop =
            * static_cast<const array_t<uint16_t>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectUInts", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else if (m_type == UBYTE) {
        const array_t<unsigned char> &prop =
            * static_cast<const array_t<unsigned char>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectUInts", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else if (m_type == ibis::CATEGORY) {
        const std::vector<std::string> *prop =
            static_cast<const std::vector<std::string>*>(buffer);
        const size_t nprop = prop->size();
        if (dic == 0) {
            ibis::dictionary *dic = new ibis::dictionary();
            for (size_t j = 0; j < nprop; ++ j)
                dic->insert((*prop)[j].c_str());
            const_cast<ibis::bord::column*>(this)->dic = dic;
        }

        array->resize(nprop);
        for (size_t j = 0; j < nprop; ++ j)
            (*array)[j] = (*dic)[(*prop)[j].c_str()];
    }
    else {
        logWarning("selectUInts", "incompatible data type");
    }
    if (ibis::gVerbose > 5) {
        timer.stop();
        long unsigned cnt = mask.cnt();
        logMessage("selectUInts", "retrieving %lu unsigned integer%s "
                   "took %g sec(CPU), %g sec(elapsed)",
                   static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                   timer.CPUTime(), timer.realTime());
    }
    return array;
} // ibis::bord::column::selectUInts

/// Can be called on all integral types.  Note that 64-byte unsigned
/// integers are simply treated as signed integer.  This may cause the
/// values to be interperted incorrectly.  Shorter version of unsigned
/// integers are treated correctly as positive values.
ibis::array_t<int64_t>*
ibis::bord::column::selectLongs(const ibis::bitvector& mask) const {
    ibis::array_t<int64_t>* array = new array_t<int64_t>;
    const uint32_t tot = mask.cnt();
    if (tot == 0 || buffer == 0)
        return array;

    ibis::horometer timer;
    if (ibis::gVerbose > 5)
        timer.start();
    if (m_type == ibis::LONG) {
        const array_t<int64_t> &prop =
            * static_cast<const array_t<int64_t>*>(buffer);
        const long unsigned nprop = prop.size();
        uint32_t i = 0;
        if (tot < nprop)
            array->resize(tot);
#if DEBUG+0 > 0 || _DEBUG+0 > 0
        logMessage("DEBUG", "selectLongs mask.size(%lu) and nprop=%lu",
                   static_cast<long unsigned>(mask.size()), nprop);
#endif
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (tot >= mask.size()) { // use shallow copy
            ibis::array_t<int64_t> tmp(prop);
            tmp.swap(*array);
            i = nprop;
        }
        else if (nprop >= mask.size()) { // no need to check loop bounds
#if DEBUG+0 > 0 || _DEBUG+0 > 0
            logMessage("DEBUG", "entering unchecked loops");
#endif
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                    logMessage("DEBUG", "copying range [%lu, %lu), i=%lu",
                               static_cast<long unsigned>(*idx0),
                               static_cast<long unsigned>(idx0[1]),
                               static_cast<long unsigned>(i));
#endif
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                        logMessage("DEBUG", "copying value %lu to i=%lu",
                                   static_cast<long unsigned>(idx0[j]),
                                   static_cast<long unsigned>(i));
#endif
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
#if DEBUG+0 > 0 || _DEBUG+0 > 0
            logMessage("DEBUG", "entering checked loops");
#endif
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                    logMessage("DEBUG", "copying range [%lu, %lu), i=%lu",
                               static_cast<long unsigned>(*idx0),
                               static_cast<long unsigned>
                               (idx0[1]<=nprop ? idx0[1] : nprop),
                               static_cast<long unsigned>(i));
#endif
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                        logMessage("DEBUG", "copying value %lu to i=%lu",
                                   static_cast<long unsigned>(idx0[j]),
                                   static_cast<long unsigned>(i));
#endif
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectLongs", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else if (m_type == ibis::UINT || m_type == ibis::CATEGORY ||
             m_type == ibis::TEXT) {
        const array_t<uint32_t> &prop =
            * static_cast<const array_t<uint32_t>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const long unsigned nprop = prop.size();
#if DEBUG+0 > 0 || _DEBUG+0 > 0
        logMessage("DEBUG", "selectLongs mask.size(%lu) and nprop=%lu",
                   static_cast<long unsigned>(mask.size()), nprop);
#endif
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) { // no need to check loop bounds
#if DEBUG+0 > 0 || _DEBUG+0 > 0
            logMessage("DEBUG", "entering unchecked loops");
#endif
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                    logMessage("DEBUG", "copying range [%lu, %lu), i=%lu",
                               static_cast<long unsigned>(*idx0),
                               static_cast<long unsigned>(idx0[1]),
                               static_cast<long unsigned>(i));
#endif
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                        logMessage("DEBUG", "copying value %lu to i=%lu",
                                   static_cast<long unsigned>(idx0[j]),
                                   static_cast<long unsigned>(i));
#endif
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
#if DEBUG+0 > 0 || _DEBUG+0 > 0
            logMessage("DEBUG", "entering checked loops");
#endif
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                    logMessage("DEBUG", "copying range [%lu, %lu), i=%lu",
                               static_cast<long unsigned>(*idx0),
                               static_cast<long unsigned>
                               (idx0[1]<=nprop ? idx0[1] : nprop),
                               static_cast<long unsigned>(i));
#endif
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                        logMessage("DEBUG", "copying value %lu to i=%lu",
                                   static_cast<long unsigned>(idx0[j]),
                                   static_cast<long unsigned>(i));
#endif
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectLongs", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else if (m_type == ibis::INT) {
        const array_t<int32_t> &prop =
            * static_cast<const array_t<int32_t>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const long unsigned nprop = prop.size();
#if DEBUG+0 > 0 || _DEBUG+0 > 0
        logMessage("DEBUG", "selectLongs mask.size(%lu) and nprop=%lu",
                   static_cast<long unsigned>(mask.size()), nprop);
#endif
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) { // no need to check loop bounds
#if DEBUG+0 > 0 || _DEBUG+0 > 0
            logMessage("DEBUG", "entering unchecked loops");
#endif
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                    logMessage("DEBUG", "copying range [%lu, %lu), i=%lu",
                               static_cast<long unsigned>(*idx0),
                               static_cast<long unsigned>(idx0[1]),
                               static_cast<long unsigned>(i));
#endif
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                        logMessage("DEBUG", "copying value %lu to i=%lu",
                                   static_cast<long unsigned>(idx0[j]),
                                   static_cast<long unsigned>(i));
#endif
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
#if DEBUG+0 > 0 || _DEBUG+0 > 0
            logMessage("DEBUG", "entering checked loops");
#endif
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                    logMessage("DEBUG", "copying range [%lu, %lu), i=%lu",
                               static_cast<long unsigned>(*idx0),
                               static_cast<long unsigned>
                               (idx0[1]<=nprop ? idx0[1] : nprop),
                               static_cast<long unsigned>(i));
#endif
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                        logMessage("DEBUG", "copying value %lu to i=%lu",
                                   static_cast<long unsigned>(idx0[j]),
                                   static_cast<long unsigned>(i));
#endif
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectLongs", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else if (m_type == ibis::USHORT) {
        const array_t<uint16_t> &prop =
            * static_cast<const array_t<uint16_t>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) { // no need to check loop bounds
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectLongs", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else if (m_type == SHORT) {
        const array_t<int16_t> &prop =
            * static_cast<const array_t<int16_t>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) { // no need to check loop bounds
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectLongs", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else if (m_type == UBYTE) {
        const array_t<unsigned char> &prop =
            * static_cast<const array_t<unsigned char>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) { // no need to check loop bounds
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectLongs", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else if (m_type == BYTE) {
        const array_t<signed char> &prop =
            * static_cast<const array_t<signed char>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) { // no need to check loop bounds
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectLongs", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else {
        logWarning("selectLongs", "incompatible data type");
    }
    if (ibis::gVerbose > 5) {
        timer.stop();
        long unsigned cnt = mask.cnt();
        logMessage("selectLongs", "retrieving %lu integer%s "
                   "took %g sec(CPU), %g sec(elapsed)",
                   static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                   timer.CPUTime(), timer.realTime());
    }
    return array;
} // ibis::bord::column::selectLongs

ibis::array_t<uint64_t>*
ibis::bord::column::selectULongs(const ibis::bitvector& mask) const {
    ibis::array_t<uint64_t>* array = new array_t<uint64_t>;
    const uint32_t tot = mask.cnt();
    if (tot == 0 || buffer == 0)
        return array;

    ibis::horometer timer;
    if (ibis::gVerbose > 5)
        timer.start();
    if (m_type == ibis::ULONG) {
        const array_t<uint64_t> &prop =
            * static_cast<const array_t<uint64_t>*>(buffer);
        const long unsigned nprop = prop.size();
#if DEBUG+0 > 0 || _DEBUG+0 > 0
        logMessage("DEBUG", "selectULongs mask.size(%lu) and nprop=%lu",
                   static_cast<long unsigned>(mask.size()), nprop);
#endif
        uint32_t i = 0;
        if (tot < nprop)
            array->resize(tot);
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (tot >= nprop) {
            ibis::array_t<uint64_t> tmp(prop);
            array->swap(tmp);
            i = nprop;
        }
        else if (nprop >= mask.size()) { // no need to check loop bounds
#if DEBUG+0 > 0 || _DEBUG+0 > 0
            logMessage("DEBUG", "entering unchecked loops");
#endif
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                    logMessage("DEBUG", "copying range [%lu, %lu), i=%lu",
                               static_cast<long unsigned>(*idx0),
                               static_cast<long unsigned>(idx0[1]),
                               static_cast<long unsigned>(i));
#endif
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                        logMessage("DEBUG", "copying value %lu to i=%lu",
                                   static_cast<long unsigned>(idx0[j]),
                                   static_cast<long unsigned>(i));
#endif
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
#if DEBUG+0 > 0 || _DEBUG+0 > 0
            logMessage("DEBUG", "entering checked loops");
#endif
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                    logMessage("DEBUG", "copying range [%lu, %lu), i=%lu",
                               static_cast<long unsigned>(*idx0),
                               static_cast<long unsigned>
                               (idx0[1]<=nprop ? idx0[1] : nprop),
                               static_cast<long unsigned>(i));
#endif
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                        logMessage("DEBUG", "copying value %lu to i=%lu",
                                   static_cast<long unsigned>(idx0[j]),
                                   static_cast<long unsigned>(i));
#endif
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectULongs", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else if (m_type == ibis::UINT || m_type == ibis::CATEGORY ||
             m_type == ibis::TEXT) {
        const array_t<uint32_t> &prop =
            * static_cast<const array_t<uint32_t>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const long unsigned nprop = prop.size();
#if DEBUG+0 > 0 || _DEBUG+0 > 0
        logMessage("DEBUG", "selectULongs mask.size(%lu) and nprop=%lu",
                   static_cast<long unsigned>(mask.size()), nprop);
#endif
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) { // no need to check loop bounds
#if DEBUG+0 > 0 || _DEBUG+0 > 0
            logMessage("DEBUG", "entering unchecked loops");
#endif
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                    logMessage("DEBUG", "copying range [%lu, %lu), i=%lu",
                               static_cast<long unsigned>(*idx0),
                               static_cast<long unsigned>(idx0[1]),
                               static_cast<long unsigned>(i));
#endif
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                        logMessage("DEBUG", "copying value %lu to i=%lu",
                                   static_cast<long unsigned>(idx0[j]),
                                   static_cast<long unsigned>(i));
#endif
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
#if DEBUG+0 > 0 || _DEBUG+0 > 0
            logMessage("DEBUG", "entering checked loops");
#endif
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                    logMessage("DEBUG", "copying range [%lu, %lu), i=%lu",
                               static_cast<long unsigned>(*idx0),
                               static_cast<long unsigned>
                               (idx0[1]<=nprop ? idx0[1] : nprop),
                               static_cast<long unsigned>(i));
#endif
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                        logMessage("DEBUG", "copying value %lu to i=%lu",
                                   static_cast<long unsigned>(idx0[j]),
                                   static_cast<long unsigned>(i));
#endif
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectULongs", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else if (m_type == ibis::INT) {
        const array_t<int32_t> &prop =
            * static_cast<const array_t<int32_t>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const long unsigned nprop = prop.size();
#if DEBUG+0 > 0 || _DEBUG+0 > 0
        logMessage("DEBUG", "selectULongs mask.size(%lu) and nprop=%lu",
                   static_cast<long unsigned>(mask.size()), nprop);
#endif
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) { // no need to check loop bounds
#if DEBUG+0 > 0 || _DEBUG+0 > 0
            logMessage("DEBUG", "entering unchecked loops");
#endif
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                    logMessage("DEBUG", "copying range [%lu, %lu), i=%lu",
                               static_cast<long unsigned>(*idx0),
                               static_cast<long unsigned>(idx0[1]),
                               static_cast<long unsigned>(i));
#endif
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                        logMessage("DEBUG", "copying value %lu to i=%lu",
                                   static_cast<long unsigned>(idx0[j]),
                                   static_cast<long unsigned>(i));
#endif
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
#if DEBUG+0 > 0 || _DEBUG+0 > 0
            logMessage("DEBUG", "entering checked loops");
#endif
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                    logMessage("DEBUG", "copying range [%lu, %lu), i=%lu",
                               static_cast<long unsigned>(*idx0),
                               static_cast<long unsigned>
                               (idx0[1]<=nprop ? idx0[1] : nprop),
                               static_cast<long unsigned>(i));
#endif
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                        logMessage("DEBUG", "copying value %lu to i=%lu",
                                   static_cast<long unsigned>(idx0[j]),
                                   static_cast<long unsigned>(i));
#endif
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectULongs", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else if (m_type == ibis::USHORT) {
        const array_t<uint16_t> &prop =
            * static_cast<const array_t<uint16_t>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) { // no need to check loop bounds
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectULongs", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else if (m_type == SHORT) {
        const array_t<int16_t> &prop =
            * static_cast<const array_t<int16_t>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) { // no need to check loop bounds
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectULongs", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else if (m_type == UBYTE) {
        const array_t<unsigned char> &prop =
            * static_cast<const array_t<unsigned char>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) { // no need to check loop bounds
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectULongs", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else if (m_type == BYTE) {
        const array_t<signed char> &prop =
            * static_cast<const array_t<signed char>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) { // no need to check loop bounds
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectULongs", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else {
        logWarning("selectULongs", "incompatible data type");
    }
    if (ibis::gVerbose > 5) {
        timer.stop();
        long unsigned cnt = mask.cnt();
        logMessage("selectULongs", "retrieving %lu integer%s "
                   "took %g sec(CPU), %g sec(elapsed)",
                   static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                   timer.CPUTime(), timer.realTime());
    }
    return array;
} // ibis::bord::column::selectULongs

/// Put selected values of a float column into an array.
ibis::array_t<float>*
ibis::bord::column::selectFloats(const ibis::bitvector& mask) const {
    ibis::array_t<float>* array = new array_t<float>;
    const uint32_t tot = mask.cnt();
    if (tot == 0 || buffer == 0)
        return array;
    ibis::horometer timer;
    if (ibis::gVerbose > 5)
        timer.start();

    if (m_type == FLOAT) {
        const array_t<float> &prop =
            * static_cast<const array_t<float>*>(buffer);
        const uint32_t nprop = prop.size();

        uint32_t i = 0;
        if (tot < nprop)
            array->resize(tot);
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (tot >= nprop) {
            ibis::array_t<float> tmp(prop);
            array->swap(tmp);
            i = nprop;
        }
        else if (nprop >= mask.size()) { // no need to check loop bounds
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectFloats", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else if (m_type == ibis::USHORT) {
        const array_t<uint16_t> &prop =
            * static_cast<const array_t<uint16_t>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) { // no need to check loop bounds
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectFloats", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else if (m_type == SHORT) {
        const array_t<int16_t> &prop =
            * static_cast<const array_t<int16_t>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) { // no need to check loop bounds
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectFloats", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else if (m_type == UBYTE) {
        const array_t<unsigned char> &prop =
            * static_cast<const array_t<unsigned char>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) { // no need to check loop bounds
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectFloats", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else if (m_type == BYTE) {
        const array_t<signed char> &prop =
            * static_cast<const array_t<signed char>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) { // no need to check loop bounds
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j < idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else { // need to check loop bounds against nprop
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j < (idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }
        if (i != tot) {
            array->resize(i);
            logWarning("selectFloats", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
    }
    else {
        logWarning("selectFloats", "incompatible data type");
    }
    if (ibis::gVerbose > 5) {
        timer.stop();
        long unsigned cnt = mask.cnt();
        logMessage("selectFloats", "retrieving %lu float value%s "
                   "took %g sec(CPU), %g sec(elapsed)",
                   static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                   timer.CPUTime(), timer.realTime());
    }
    return array;
} // ibis::bord::column::selectFloats

/// Put the selected values into an array as doubles.
///
/// @note Any column type could be selected as doubles.  Other selectXXs
/// function only work with data types that can be safely converted.  This
/// is the only function that allows one to convert to a different type.
/// This is mainly to support aggregation operations involving arithmetic
/// operation, however, it will truncate 64-bit integers to only 48-bit
/// precision (because a double only has 48-bit mantisa).
ibis::array_t<double>*
ibis::bord::column::selectDoubles(const ibis::bitvector& mask) const {
    ibis::array_t<double>* array = new array_t<double>;
    const uint32_t tot = mask.cnt();
    if (tot == 0 || buffer == 0)
        return array;
    ibis::horometer timer;
    if (ibis::gVerbose > 5)
        timer.start();

    switch(m_type) {
    case ibis::ULONG: {
        const array_t<uint64_t> &prop =
            * static_cast<const array_t<uint64_t>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j<(idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectDoubles", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
        else if (ibis::gVerbose > 5) {
            timer.stop();
            long unsigned cnt = mask.cnt();
            logMessage("selectDoubles", "retrieving %lu unsigned integer%s "
                       "took %g sec(CPU), %g sec(elapsed)",
                       static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                       timer.CPUTime(), timer.realTime());
        }
        break;}
    case ibis::LONG: {
        const array_t<int64_t> &prop =
            * static_cast<const array_t<int64_t>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j<(idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectDoubles", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
        else if (ibis::gVerbose > 5) {
            timer.stop();
            long unsigned cnt = mask.cnt();
            logMessage("selectDoubles", "retrieving %lu integer%s "
                       "took %g sec(CPU), %g sec(elapsed)",
                       static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                       timer.CPUTime(), timer.realTime());
        }
        break;}
    case ibis::CATEGORY:
    case ibis::UINT: {
        const array_t<uint32_t> &prop =
            * static_cast<const array_t<uint32_t>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j<(idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectDoubles", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
        else if (ibis::gVerbose > 5) {
            timer.stop();
            long unsigned cnt = mask.cnt();
            logMessage("selectDoubles", "retrieving %lu unsigned integer%s "
                       "took %g sec(CPU), %g sec(elapsed)",
                       static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                       timer.CPUTime(), timer.realTime());
        }
        break;}
    case ibis::INT: {
        const array_t<int32_t> &prop =
            * static_cast<const array_t<int32_t>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j<(idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectDoubles", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
        else if (ibis::gVerbose > 5) {
            timer.stop();
            long unsigned cnt = mask.cnt();
            logMessage("selectDoubles", "retrieving %lu integer%s "
                       "took %g sec(CPU), %g sec(elapsed)",
                       static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                       timer.CPUTime(), timer.realTime());
        }
        break;}
    case ibis::USHORT: {
        const array_t<uint16_t> &prop =
            * static_cast<const array_t<uint16_t>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j<(idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectDoubles", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
        else if (ibis::gVerbose > 5) {
            timer.stop();
            long unsigned cnt = mask.cnt();
            logMessage("selectDoubles", "retrieving %lu unsigned short "
                       "integer%s took %g sec(CPU), %g sec(elapsed)",
                       static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                       timer.CPUTime(), timer.realTime());
        }
        break;}
    case ibis::SHORT: {
        const array_t<int16_t> &prop =
            * static_cast<const array_t<int16_t>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j<(idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectDoubles", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
        else if (ibis::gVerbose > 5) {
            timer.stop();
            long unsigned cnt = mask.cnt();
            logMessage("selectDoubles", "retrieving %lu short integer%s "
                       "took %g sec(CPU), %g sec(elapsed)",
                       static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                       timer.CPUTime(), timer.realTime());
        }
        break;}
    case ibis::UBYTE: {
        const array_t<unsigned char> &prop =
            * static_cast<const array_t<unsigned char>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j<(idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectDoubles", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
        else if (ibis::gVerbose > 5) {
            timer.stop();
            long unsigned cnt = mask.cnt();
            logMessage("selectDoubles", "retrieving %lu unsigned 1-byte "
                       "integer%s took %g sec(CPU), %g sec(elapsed)",
                       static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                       timer.CPUTime(), timer.realTime());
        }
        break;}
    case ibis::BYTE: {
        const array_t<signed char> &prop =
            * static_cast<const array_t<signed char>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j<(idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectDoubles", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
        else if (ibis::gVerbose > 5) {
            timer.stop();
            long unsigned cnt = mask.cnt();
            logMessage("selectDoubles", "retrieving %lu 1-byte integer%s "
                       "took %g sec(CPU), %g sec(elapsed)",
                       static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                       timer.CPUTime(), timer.realTime());
        }
        break;}
    case ibis::FLOAT: {
        const array_t<float> &prop =
            * static_cast<const array_t<float>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j<(idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectDoubles", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
        else if (ibis::gVerbose > 5) {
            timer.stop();
            long unsigned cnt = mask.cnt();
            logMessage("selectDoubles", "retrieving %lu float value%s "
                       "took %g sec(CPU), %g sec(elapsed)",
                       static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                       timer.CPUTime(), timer.realTime());
        }
        break;}
    case ibis::DOUBLE: {
        const array_t<double> &prop =
            * static_cast<const array_t<double>*>(buffer);
        const uint32_t nprop = prop.size();
        uint32_t i = 0;
        if (tot < nprop)
            array->resize(tot);
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (tot >= nprop) {
            ibis::array_t<double> tmp(prop);
            array->swap(tmp);
            i = nprop;
        }
        else if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j<(idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop)
                            (*array)[i] = (prop[idx0[j]]);
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectDoubles", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
        else if (ibis::gVerbose > 5) {
            timer.stop();
            long unsigned cnt = mask.cnt();
            logMessage("selectDoubles", "retrieving %lu double value%s "
                       "took %g sec(CPU), %g sec(elapsed)",
                       static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                       timer.CPUTime(), timer.realTime());
        }
        break;}
    default: {
        logWarning("selectDoubles", "incompatible data type");
        break;}
    }
    return array;
} // ibis::bord::column::selectDoubles

/// Output the selected values as strings.  Most data types can be
/// converted and shown as strings.
std::vector<std::string>*
ibis::bord::column::selectStrings(const ibis::bitvector& mask) const {
    std::vector<std::string>* array = new std::vector<std::string>;
    const uint32_t tot = mask.cnt();
    if (tot == 0 || buffer == 0)
        return array;
    ibis::horometer timer;
    if (ibis::gVerbose > 5)
        timer.start();

    switch(m_type) {
    case ibis::ULONG: {
        const array_t<uint64_t> &prop =
            * static_cast<const array_t<uint64_t>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        std::ostringstream oss;
                        oss << prop[j];
                        (*array)[i] = oss.str();
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        std::ostringstream oss;
                        oss << (prop[idx0[j]]);
                        (*array)[i] = oss.str();
                    }
                }
                ++ index;
            }
        }
        else {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j<(idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        std::ostringstream oss;
                        oss << (prop[j]);
                        (*array)[i] = oss.str();
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop) {
                            std::ostringstream oss;
                            oss << (prop[idx0[j]]);
                            (*array)[i] = oss.str();
                        }
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectStrings", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
        else if (ibis::gVerbose > 5) {
            timer.stop();
            long unsigned cnt = mask.cnt();
            logMessage("selectStrings", "retrieving %lu unsigned integer%s "
                       "took %g sec(CPU), %g sec(elapsed)",
                       static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                       timer.CPUTime(), timer.realTime());
        }
        break;}
    case ibis::LONG: {
        const array_t<int64_t> &prop =
            * static_cast<const array_t<int64_t>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        std::ostringstream oss;
                        oss << (prop[j]);
                        (*array)[i] = oss.str();
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        std::ostringstream oss;
                        oss << (prop[idx0[j]]);
                        (*array)[i] = oss.str();
                    }
                }
                ++ index;
            }
        }
        else {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j<(idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        std::ostringstream oss;
                        oss << (prop[j]);
                        (*array)[i] = oss.str();
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop) {
                            std::ostringstream oss;
                            oss << (prop[idx0[j]]);
                            (*array)[i] = oss.str();
                        }
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectStrings", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
        else if (ibis::gVerbose > 5) {
            timer.stop();
            long unsigned cnt = mask.cnt();
            logMessage("selectStrings", "retrieving %lu integer%s "
                       "took %g sec(CPU), %g sec(elapsed)",
                       static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                       timer.CPUTime(), timer.realTime());
        }
        break;}
    case ibis::UINT: {
        const array_t<uint32_t> &prop =
            * static_cast<const array_t<uint32_t>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        std::ostringstream oss;
                        oss << prop[j];
                        (*array)[i] = oss.str();
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        std::ostringstream oss;
                        oss << (prop[idx0[j]]);
                        (*array)[i] = oss.str();
                    }
                }
                ++ index;
            }
        }
        else {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j<(idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        std::ostringstream oss;
                        oss << (prop[j]);
                        (*array)[i] = oss.str();
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop) {
                            std::ostringstream oss;
                            oss << (prop[idx0[j]]);
                            (*array)[i] = oss.str();
                        }
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectStrings", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
        else if (ibis::gVerbose > 5) {
            timer.stop();
            long unsigned cnt = mask.cnt();
            logMessage("selectStrings", "retrieving %lu unsigned integer%s "
                       "took %g sec(CPU), %g sec(elapsed)",
                       static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                       timer.CPUTime(), timer.realTime());
        }
        break;}
    case ibis::INT: {
        const array_t<int32_t> &prop =
            * static_cast<const array_t<int32_t>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        std::ostringstream oss;
                        oss << (prop[j]);
                        (*array)[i] = oss.str();
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        std::ostringstream oss;
                        oss << (prop[idx0[j]]);
                        (*array)[i] = oss.str();
                    }
                }
                ++ index;
            }
        }
        else {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j<(idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        std::ostringstream oss;
                        oss << (prop[j]);
                        (*array)[i] = oss.str();
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop) {
                            std::ostringstream oss;
                            oss << (prop[idx0[j]]);
                            (*array)[i] = oss.str();
                        }
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectStrings", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
        else if (ibis::gVerbose > 5) {
            timer.stop();
            long unsigned cnt = mask.cnt();
            logMessage("selectStrings", "retrieving %lu integer%s "
                       "took %g sec(CPU), %g sec(elapsed)",
                       static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                       timer.CPUTime(), timer.realTime());
        }
        break;}
    case ibis::USHORT: {
        const array_t<uint16_t> &prop =
            * static_cast<const array_t<uint16_t>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        std::ostringstream oss;
                        oss << (prop[j]);
                        (*array)[i] = oss.str();
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        std::ostringstream oss;
                        oss << (prop[idx0[j]]);
                        (*array)[i] = oss.str();
                    }
                }
                ++ index;
            }
        }
        else {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j<(idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        std::ostringstream oss;
                        oss << (prop[j]);
                        (*array)[i] = oss.str();
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop) {
                            std::ostringstream oss;
                            oss << (prop[idx0[j]]);
                            (*array)[i] = oss.str();
                        }
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectStrings", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
        else if (ibis::gVerbose > 5) {
            timer.stop();
            long unsigned cnt = mask.cnt();
            logMessage("selectStrings", "retrieving %lu unsigned short "
                       "integer%s took %g sec(CPU), %g sec(elapsed)",
                       static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                       timer.CPUTime(), timer.realTime());
        }
        break;}
    case ibis::SHORT: {
        const array_t<int16_t> &prop =
            * static_cast<const array_t<int16_t>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        std::ostringstream oss;
                        oss << (prop[j]);
                        (*array)[i] = oss.str();
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        std::ostringstream oss;
                        oss << (prop[idx0[j]]);
                        (*array)[i] = oss.str();
                    }
                }
                ++ index;
            }
        }
        else {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j<(idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        std::ostringstream oss;
                        oss << (prop[j]);
                        (*array)[i] = oss.str();
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop) {
                            std::ostringstream oss;
                            oss << (prop[idx0[j]]);
                            (*array)[i] = oss.str();
                        }
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectStrings", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
        else if (ibis::gVerbose > 5) {
            timer.stop();
            long unsigned cnt = mask.cnt();
            logMessage("selectStrings", "retrieving %lu short integer%s "
                       "took %g sec(CPU), %g sec(elapsed)",
                       static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                       timer.CPUTime(), timer.realTime());
        }
        break;}
    case ibis::UBYTE: {
        const array_t<unsigned char> &prop =
            * static_cast<const array_t<unsigned char>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        std::ostringstream oss;
                        oss << (unsigned) (prop[j]);
                        (*array)[i] = oss.str();
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        std::ostringstream oss;
                        oss << (unsigned)(prop[idx0[j]]);
                        (*array)[i] = oss.str();
                    }
                }
                ++ index;
            }
        }
        else {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j<(idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        std::ostringstream oss;
                        oss << (unsigned) (prop[j]);
                        (*array)[i] = oss.str();
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop) {
                            std::ostringstream oss;
                            oss << (unsigned)(prop[idx0[j]]);
                            (*array)[i] = oss.str();
                        }
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectStrings", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
        else if (ibis::gVerbose > 5) {
            timer.stop();
            long unsigned cnt = mask.cnt();
            logMessage("selectStrings", "retrieving %lu unsigned 1-byte "
                       "integer%s took %g sec(CPU), %g sec(elapsed)",
                       static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                       timer.CPUTime(), timer.realTime());
        }
        break;}
    case ibis::BYTE: {
        const array_t<signed char> &prop =
            * static_cast<const array_t<signed char>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        std::ostringstream oss;
                        oss << (int) (prop[j]);
                        (*array)[i] = oss.str();
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        std::ostringstream oss;
                        oss << (int) (prop[idx0[j]]);
                        (*array)[i] = oss.str();
                    }
                }
                ++ index;
            }
        }
        else {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j<(idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        std::ostringstream oss;
                        oss << (int) (prop[j]);
                        (*array)[i] = oss.str();
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop) {
                            std::ostringstream oss;
                            oss << (int) (prop[idx0[j]]);
                            (*array)[i] = oss.str();
                        }
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectStrings", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
        else if (ibis::gVerbose > 5) {
            timer.stop();
            long unsigned cnt = mask.cnt();
            logMessage("selectStrings", "retrieving %lu 1-byte integer%s "
                       "took %g sec(CPU), %g sec(elapsed)",
                       static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                       timer.CPUTime(), timer.realTime());
        }
        break;}
    case ibis::FLOAT: {
        const array_t<float> &prop =
            * static_cast<const array_t<float>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        std::ostringstream oss;
                        oss << (prop[j]);
                        (*array)[i] = oss.str();
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        std::ostringstream oss;
                        oss << (prop[idx0[j]]);
                        (*array)[i] = oss.str();
                    }
                }
                ++ index;
            }
        }
        else {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j<(idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        std::ostringstream oss;
                        oss << (prop[j]);
                        (*array)[i] = oss.str();
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop) {
                            std::ostringstream oss;
                            oss << (prop[idx0[j]]);
                            (*array)[i] = oss.str();
                        }
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectStrings", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
        else if (ibis::gVerbose > 5) {
            timer.stop();
            long unsigned cnt = mask.cnt();
            logMessage("selectStrings", "retrieving %lu float value%s "
                       "took %g sec(CPU), %g sec(elapsed)",
                       static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                       timer.CPUTime(), timer.realTime());
        }
        break;}
    case ibis::DOUBLE: {
        const array_t<double> &prop =
            * static_cast<const array_t<double>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        std::ostringstream oss;
                        oss << (prop[j]);
                        (*array)[i] = oss.str();
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        std::ostringstream oss;
                        oss << (prop[idx0[j]]);
                        (*array)[i] = oss.str();
                    }
                }
                ++ index;
            }
        }
        else {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j<(idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        std::ostringstream oss;
                        oss << (prop[j]);
                        (*array)[i] = oss.str();
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop) {
                            std::ostringstream oss;
                            oss << (prop[idx0[j]]);
                            (*array)[i] = oss.str();
                        }
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectStrings", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
        else if (ibis::gVerbose > 5) {
            timer.stop();
            long unsigned cnt = mask.cnt();
            logMessage("selectStrings", "retrieving %lu double value%s "
                       "took %g sec(CPU), %g sec(elapsed)",
                       static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                       timer.CPUTime(), timer.realTime());
        }
        break;}
    case ibis::OID: {
        const array_t<ibis::rid_t> &prop =
            * static_cast<const array_t<ibis::rid_t>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        std::ostringstream oss;
                        oss << (prop[j]);
                        (*array)[i] = oss.str();
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        std::ostringstream oss;
                        oss << (prop[idx0[j]]);
                        (*array)[i] = oss.str();
                    }
                }
                ++ index;
            }
        }
        else {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j<(idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        std::ostringstream oss;
                        oss << (prop[j]);
                        (*array)[i] = oss.str();
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop) {
                            std::ostringstream oss;
                            oss << (prop[idx0[j]]);
                            (*array)[i] = oss.str();
                        }
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectStrings", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
        else if (ibis::gVerbose > 5) {
            timer.stop();
            long unsigned cnt = mask.cnt();
            logMessage("selectStrings", "retrieving %lu ibis::rid_t value%s "
                       "took %g sec(CPU), %g sec(elapsed)",
                       static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                       timer.CPUTime(), timer.realTime());
        }
        break;}
    // case ibis::CATEGORY: {
    //  const array_t<uint32_t> &prop =
    //      * static_cast<const array_t<uint32_t>*>(buffer);

    //  uint32_t i = 0;
    //  array->resize(tot);
    //  const uint32_t nprop = prop.size();
    //  ibis::bitvector::indexSet index = mask.firstIndexSet();
    //  if (nprop >= mask.size()) {
    //      while (index.nIndices() > 0) {
    //          const ibis::bitvector::word_t *idx0 = index.indices();
    //          if (index.isRange()) {
    //              for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
    //                  (*array)[i] = (*dic)[(prop[j])];
    //              }
    //          }
    //          else {
    //              for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
    //                  (*array)[i] = (*dic)[ (prop[idx0[j]])];
    //              }
    //          }
    //          ++ index;
    // //           }
    //  }
    //  else {
    //      while (index.nIndices() > 0) {
    //          const ibis::bitvector::word_t *idx0 = index.indices();
    //          if (*idx0 >= nprop) break;
    //          if (index.isRange()) {
    //              for (uint32_t j = *idx0;
    //                   j<(idx0[1]<=nprop ? idx0[1] : nprop);
    //                   ++j, ++i) {
    //                  (*array)[i] = (*dic)[(prop[j])];
    //              }
    //          }
    //          else {
    //              for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
    //                  if (idx0[j] < nprop) {
    //                      (*array)[i] = (*dic)[(prop[idx0[j]])];
    //                  }
    //                  else
    //                      break;
    //              }
    //          }
    //          ++ index;
    //      }
    //  }

    //  if (i != tot) {
    //      array->resize(i);
    //      logWarning("selectStrings", "expects to retrieve %lu elements "
    //                 "but only got %lu", static_cast<long unsigned>(tot),
    //                 static_cast<long unsigned>(i));
    //  }
    //  else if (ibis::gVerbose > 5) {
    //      timer.stop();
    //      long unsigned cnt = mask.cnt();
    //      logMessage("selectStrings", "retrieving %lu string value%s "
    //                 "took %g sec(CPU), %g sec(elapsed)",
    //                 static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
    //                 timer.CPUTime(), timer.realTime());
    //  }
    //  break;}
    case ibis::CATEGORY:
    case ibis::TEXT: {
        const std::vector<std::string> &prop =
            * static_cast<const std::vector<std::string>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i] = (prop[idx0[j]]);
                    }
                }
                ++ index;
            }
        }
        else {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j<(idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i] = (prop[j]);
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop) {
                            (*array)[i] = (prop[idx0[j]]);
                        }
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectStrings", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
        else if (ibis::gVerbose > 5) {
            timer.stop();
            long unsigned cnt = mask.cnt();
            logMessage("selectStrings", "retrieving %lu string value%s "
                       "took %g sec(CPU), %g sec(elapsed)",
                       static_cast<long unsigned>(cnt),
                       (cnt > 1 ? "s" : ""),
                       timer.CPUTime(), timer.realTime());
        }
        break;}
    default: {
        logWarning("selectStrings", "incompatible data type");
        break;}
    }
    return array;
} // ibis::bord::column::selectStrings

/// Return the string at the <code>i</code>th row.  If the raw data is not
/// present, but a dictionary is present, then this function return the
/// string value corresponding to the integer value i.  Note that this
/// fall-back option does not conform to the original intention of this
/// function.
int ibis::bord::column::getString(uint32_t i, std::string &val) const {
    val.erase();
    if (buffer != 0 && (m_type == ibis::TEXT || m_type == ibis::CATEGORY)) {
        std::vector<std::string> *str_column =
            static_cast<std::vector<std::string> *>(buffer);
        if ( i < str_column->size())
            val = str_column->at(i);
    }
    else if (dic != 0) {
        const char *tmp  = (*dic)[i];
        if (tmp != 0)
            val = tmp;
    }
    return 0;
} // ibis::bord::column::getString

std::vector<ibis::opaque>*
ibis::bord::column::selectOpaques(const bitvector& mask) const {
    std::vector<ibis::opaque>* array = new std::vector<ibis::opaque>;
    const uint32_t tot = mask.cnt();
    if (tot == 0 || buffer == 0)
        return array;
    ibis::horometer timer;
    if (ibis::gVerbose > 5)
        timer.start();

    switch(m_type) {
    case ibis::DOUBLE:
    case ibis::ULONG:
    case ibis::LONG:
    case ibis::OID: {
        const array_t<int64_t> &prop =
            * static_cast<const array_t<int64_t>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        (*array)[i].copy(&(prop[j]), sizeof(int64_t));
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i].copy(&(prop[idx0[j]]), sizeof(int64_t));
                    }
                }
                ++ index;
            }
        }
        else {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j<(idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i].copy(&(prop[j]), sizeof(int64_t));
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop) {
                            (*array)[i].copy(&(prop[idx0[j]]), sizeof(int64_t));
                        }
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectOpaques", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
        else if (ibis::gVerbose > 5) {
            timer.stop();
            long unsigned cnt = mask.cnt();
            logMessage("selectOpaques", "retrieving %lu integer%s "
                       "took %g sec(CPU), %g sec(elapsed)",
                       static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                       timer.CPUTime(), timer.realTime());
        }
        break;}
    case ibis::UINT: {
        const array_t<uint32_t> &prop =
            * static_cast<const array_t<uint32_t>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        if (dic == 0) {
                            (*array)[i].copy(&(prop[j]), sizeof(uint32_t));
                        }
                        else if (prop[j] < dic->size()) {
                            const char* str =(*dic)[prop[j]];
                            (*array)[i].copy(str, std::strlen(str));
                        }
                        else {
                            (*array)[i].copy(&(prop[j]), sizeof(uint32_t));
                        }
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (dic == 0) {
                            (*array)[i].copy(&(prop[idx0[j]]), sizeof(uint32_t));
                        }
                        else if (prop[idx0[j]] < dic->size()) {
                            const char* str =(*dic)[prop[idx0[j]]];
                            (*array)[i].copy(str, std::strlen(str));
                        }
                        else {
                            (*array)[i].copy(&(prop[idx0[j]]), sizeof(uint32_t));
                        }
                    }
                }
                ++ index;
            }
        }
        else {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j<(idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        if (dic == 0) {
                            (*array)[i].copy(&(prop[j]), sizeof(uint32_t));
                        }
                        else if (prop[j] < dic->size()) {
                            const char* str =(*dic)[prop[j]];
                            (*array)[i].copy(str, std::strlen(str));
                        }
                        else {
                            (*array)[i].copy(&(prop[j]), sizeof(uint32_t));
                        }
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop) {
                            if (dic == 0) {
                                (*array)[i].copy(&(prop[idx0[j]]),
                                                 sizeof(uint32_t));
                            }
                            else if (prop[idx0[j]] < dic->size()) {
                                const char* str =(*dic)[prop[idx0[j]]];
                                (*array)[i].copy(str, std::strlen(str));
                            }
                            else {
                                (*array)[i].copy(&(prop[idx0[j]]),
                                                 sizeof(uint32_t));
                            }
                        }
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectOpaques", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
        else if (ibis::gVerbose > 5) {
            timer.stop();
            long unsigned cnt = mask.cnt();
            logMessage("selectOpaques", "retrieving %lu unsigned integer%s "
                       "took %g sec(CPU), %g sec(elapsed)",
                       static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                       timer.CPUTime(), timer.realTime());
        }
        break;}
    case ibis::FLOAT:
    case ibis::INT: {
        const array_t<int32_t> &prop =
            * static_cast<const array_t<int32_t>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        (*array)[i].copy(&(prop[j]), sizeof(int32_t));
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i].copy(&(prop[idx0[j]]), sizeof(int32_t));
                    }
                }
                ++ index;
            }
        }
        else {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j<(idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i].copy(&(prop[j]), sizeof(int32_t));
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop) {
                            (*array)[i].copy(&(prop[idx0[j]]), sizeof(int32_t));
                        }
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectOpaques", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
        else if (ibis::gVerbose > 5) {
            timer.stop();
            long unsigned cnt = mask.cnt();
            logMessage("selectOpaques", "retrieving %lu integer%s "
                       "took %g sec(CPU), %g sec(elapsed)",
                       static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                       timer.CPUTime(), timer.realTime());
        }
        break;}
    case ibis::USHORT:
    case ibis::SHORT: {
        const array_t<int16_t> &prop =
            * static_cast<const array_t<int16_t>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        (*array)[i].copy(&(prop[j]), sizeof(int16_t));
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i].copy(&(prop[idx0[j]]), sizeof(int16_t));
                    }
                }
                ++ index;
            }
        }
        else {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j<(idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i].copy(&(prop[j]), sizeof(int16_t));
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop) {
                            (*array)[i].copy(&(prop[idx0[j]]), sizeof(int16_t));
                        }
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectOpaques", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
        else if (ibis::gVerbose > 5) {
            timer.stop();
            long unsigned cnt = mask.cnt();
            logMessage("selectOpaques", "retrieving %lu short integer%s "
                       "took %g sec(CPU), %g sec(elapsed)",
                       static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                       timer.CPUTime(), timer.realTime());
        }
        break;}
    case ibis::UBYTE:
    case ibis::BYTE: {
        const array_t<char> &prop =
            * static_cast<const array_t<char>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        (*array)[i].copy(&(prop[j]), sizeof(char));
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i].copy(&(prop[idx0[j]]), sizeof(char));
                    }
                }
                ++ index;
            }
        }
        else {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j<(idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i].copy(&(prop[j]), sizeof(char));
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop) {
                            (*array)[i].copy(&(prop[idx0[j]]), sizeof(char));
                        }
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectOpaques", "expects to retrieve %lu elements "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
        else if (ibis::gVerbose > 5) {
            timer.stop();
            long unsigned cnt = mask.cnt();
            logMessage("selectOpaques", "retrieving %lu 1-byte integer%s "
                       "took %g sec(CPU), %g sec(elapsed)",
                       static_cast<long unsigned>(cnt), (cnt > 1 ? "s" : ""),
                       timer.CPUTime(), timer.realTime());
        }
        break;}
    case ibis::CATEGORY:
    case ibis::TEXT: {
        const std::vector<std::string> &prop =
            * static_cast<const std::vector<std::string>*>(buffer);

        uint32_t i = 0;
        array->resize(tot);
        const uint32_t nprop = prop.size();
        ibis::bitvector::indexSet index = mask.firstIndexSet();
        if (nprop >= mask.size()) {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (index.isRange()) {
                    for (uint32_t j = *idx0; j<idx0[1]; ++j, ++i) {
                        (*array)[i].copy(prop[j].data(), prop[j].size());
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        (*array)[i].copy(prop[idx0[j]].data(),
                                         prop[idx0[j]].size());
                    }
                }
                ++ index;
            }
        }
        else {
            while (index.nIndices() > 0) {
                const ibis::bitvector::word_t *idx0 = index.indices();
                if (*idx0 >= nprop) break;
                if (index.isRange()) {
                    for (uint32_t j = *idx0;
                         j<(idx0[1]<=nprop ? idx0[1] : nprop);
                         ++j, ++i) {
                        (*array)[i].copy(prop[j].data(), prop[j].size());
                    }
                }
                else {
                    for (uint32_t j = 0; j<index.nIndices(); ++j, ++i) {
                        if (idx0[j] < nprop) {
                            (*array)[i].copy(prop[idx0[j]].data(),
                                             prop[idx0[j]].size());
                        }
                        else
                            break;
                    }
                }
                ++ index;
            }
        }

        if (i != tot) {
            array->resize(i);
            logWarning("selectOpaques", "expects to retrieve %lu strings "
                       "but only got %lu", static_cast<long unsigned>(tot),
                       static_cast<long unsigned>(i));
        }
        else if (ibis::gVerbose > 5) {
            timer.stop();
            long unsigned cnt = mask.cnt();
            logMessage("selectOpaques", "retrieving %lu string value%s "
                       "took %g sec(CPU), %g sec(elapsed)",
                       static_cast<long unsigned>(cnt),
                       (cnt > 1 ? "s" : ""),
                       timer.CPUTime(), timer.realTime());
        }
        break;}
    default: {
        logWarning("selectOpaques", "incompatible data type");
        break;}
    }
    return array;
} // ibis::bord::column::selectOpaques

/// Makes a copy of the in-memory data.  Uses a shallow copy for
/// ibis::array_t objects, but a deap copy for the string values.
int ibis::bord::column::getValuesArray(void* vals) const {
    int ierr = 0;
    if (vals == 0)
        return -1;

    switch (m_type) {
    default: {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bord[" << (thePart?thePart->name():"")
            << "]::column[" << m_name << "]::getValuesArray does not yet "
            "support column type " << ibis::TYPESTRING[(int)m_type];
        ierr = -2;
        break;}
    case ibis::BYTE: {
        array_t<signed char> &local = *static_cast<array_t<signed char>*>(vals);
        if (buffer != 0) {
            local.copy(* static_cast<const array_t<signed char>*>(buffer));
        }
        else if (xreader != 0) {
            array_t<uint64_t> starts(shape.size(), 0ULL);
            local.resize(mask_.size());
            ierr = xreader(xmeta, shape.size(), starts.begin(),
                           const_cast<uint64_t*>(shape.begin()),
                           local.begin());
            if (ierr >= 0) {
                const_cast<ibis::bord::column*>(this)->buffer =
                    new ibis::array_t<signed char>(local);
            }
        }
        else {
            ierr = -3;
        }
        break;}
    case ibis::UBYTE: {
        array_t<unsigned char> &local =
            *static_cast<array_t<unsigned char>*>(vals);
        if (buffer != 0) {
            local.copy(* static_cast<const array_t<unsigned char>*>(buffer));
        }
        else if (xreader != 0) {
            array_t<uint64_t> starts(shape.size(), 0ULL);
            local.resize(mask_.size());
            ierr = xreader(xmeta, shape.size(), starts.begin(),
                           const_cast<uint64_t*>(shape.begin()),
                           local.begin());
            if (ierr >= 0) {
                const_cast<ibis::bord::column*>(this)->buffer =
                    new ibis::array_t<unsigned char>(local);
            }
        }
        else {
            ierr = -3;
        }
        break;}
    case ibis::SHORT: {
        array_t<int16_t> &local = *static_cast<array_t<int16_t>*>(vals);
        if (buffer != 0) {
            local.copy(* static_cast<const array_t<int16_t>*>(buffer));
        }
        else if (xreader != 0) {
            array_t<uint64_t> starts(shape.size(), 0ULL);
            local.resize(mask_.size());
            ierr = xreader(xmeta, shape.size(), starts.begin(),
                           const_cast<uint64_t*>(shape.begin()),
                           local.begin());
            if (ierr >= 0) {
                const_cast<ibis::bord::column*>(this)->buffer =
                    new ibis::array_t<int16_t>(local);
            }
        }
        else {
            ierr = -3;
        }
        break;}
    case ibis::USHORT: {
        array_t<uint16_t> &local = *static_cast<array_t<uint16_t>*>(vals);
        if (buffer != 0) {
            local.copy(* static_cast<const array_t<uint16_t>*>(buffer));
        }
        else if (xreader != 0) {
            array_t<uint64_t> starts(shape.size(), 0ULL);
            local.resize(mask_.size());
            ierr = xreader(xmeta, shape.size(), starts.begin(),
                           const_cast<uint64_t*>(shape.begin()),
                           local.begin());
            if (ierr >= 0) {
                const_cast<ibis::bord::column*>(this)->buffer =
                    new ibis::array_t<uint16_t>(local);
            }
        }
        else {
            ierr = -3;
        }
        break;}
    case ibis::INT: {
        array_t<int32_t> &local = *static_cast<array_t<int32_t>*>(vals);
        if (buffer != 0) {
            local.copy(* static_cast<const array_t<int32_t>*>(buffer));
        }
        else if (xreader != 0) {
            array_t<uint64_t> starts(shape.size(), 0ULL);
            local.resize(mask_.size());
            ierr = xreader(xmeta, shape.size(), starts.begin(),
                           const_cast<uint64_t*>(shape.begin()),
                           local.begin());
            if (ierr >= 0) {
                const_cast<ibis::bord::column*>(this)->buffer =
                    new ibis::array_t<int32_t>(local);
            }
        }
        else {
            ierr = -3;
        }
        break;}
    case ibis::UINT: {
        array_t<uint32_t> &local = *static_cast<array_t<uint32_t>*>(vals);
        if (buffer != 0) {
            local.copy(* static_cast<const array_t<uint32_t>*>(buffer));
        }
        else if (xreader != 0) {
            array_t<uint64_t> starts(shape.size(), 0ULL);
            local.resize(mask_.size());
            ierr = xreader(xmeta, shape.size(), starts.begin(),
                           const_cast<uint64_t*>(shape.begin()),
                           local.begin());
            if (ierr >= 0) {
                const_cast<ibis::bord::column*>(this)->buffer =
                    new ibis::array_t<uint32_t>(local);
            }
        }
        else {
            ierr = -3;
        }
        break;}
    case ibis::LONG: {
        array_t<int64_t> &local = *static_cast<array_t<int64_t>*>(vals);
        if (buffer != 0) {
            local.copy(* static_cast<const array_t<int64_t>*>(buffer));
        }
        else if (xreader != 0) {
            array_t<uint64_t> starts(shape.size(), 0ULL);
            local.resize(mask_.size());
            ierr = xreader(xmeta, shape.size(), starts.begin(),
                           const_cast<uint64_t*>(shape.begin()),
                           local.begin());
            if (ierr >= 0) {
                const_cast<ibis::bord::column*>(this)->buffer =
                    new ibis::array_t<int64_t>(local);
            }
        }
        else {
            ierr = -3;
        }
        break;}
    case ibis::ULONG: {
        array_t<uint64_t> &local = *static_cast<array_t<uint64_t>*>(vals);
        if (buffer != 0) {
            local.copy(* static_cast<const array_t<uint64_t>*>(buffer));
        }
        else if (xreader != 0) {
            array_t<uint64_t> starts(shape.size(), 0ULL);
            local.resize(mask_.size());
            ierr = xreader(xmeta, shape.size(), starts.begin(),
                           const_cast<uint64_t*>(shape.begin()),
                           local.begin());
            if (ierr >= 0) {
                const_cast<ibis::bord::column*>(this)->buffer =
                    new ibis::array_t<uint64_t>(local);
            }
        }
        else {
            ierr = -3;
        }
        break;}
    case ibis::FLOAT: {
        array_t<float> &local = *static_cast<array_t<float>*>(vals);
        if (buffer != 0) {
            local.copy(* static_cast<const array_t<float>*>(buffer));
        }
        else if (xreader != 0) {
            array_t<uint64_t> starts(shape.size(), 0ULL);
            local.resize(mask_.size());
            ierr = xreader(xmeta, shape.size(), starts.begin(),
                           const_cast<uint64_t*>(shape.begin()),
                           local.begin());
            if (ierr >= 0) {
                const_cast<ibis::bord::column*>(this)->buffer =
                    new ibis::array_t<float>(local);
            }
        }
        else {
            ierr = -3;
        }
        break;}
    case ibis::DOUBLE: {
        array_t<double> &local = *static_cast<array_t<double>*>(vals);
        if (buffer != 0) {
            local.copy(* static_cast<const array_t<double>*>(buffer));
        }
        else if (xreader != 0) {
            array_t<uint64_t> starts(shape.size(), 0ULL);
            local.resize(mask_.size());
            ierr = xreader(xmeta, shape.size(), starts.begin(),
                           const_cast<uint64_t*>(shape.begin()),
                           local.begin());
            if (ierr >= 0) {
                const_cast<ibis::bord::column*>(this)->buffer =
                    new ibis::array_t<double>(local);
            }
        }
        else {
            ierr = -3;
        }
        break;}
    case ibis::CATEGORY:
    case ibis::TEXT: {
        if (buffer != 0) {
            std::vector<std::string>
                tmp(* static_cast<const std::vector<std::string>*>(buffer));
            static_cast<std::vector<std::string>*>(vals)->swap(tmp);
        }
        else {
            ierr = -4;
        }
        break;}
    }
    return ierr;
} // ibis::bord::column::getValuesArray

void ibis::bord::column::reverseRows() {
    switch(m_type) {
    case ibis::ULONG: {
        array_t<uint64_t> &prop =
            * static_cast<array_t<uint64_t>*>(buffer);
        std::reverse(prop.begin(), prop.end());
        break;}
    case ibis::LONG: {
        array_t<int64_t> &prop =
            * static_cast<array_t<int64_t>*>(buffer);
        std::reverse(prop.begin(), prop.end());
        break;}
    case ibis::UINT: {
        array_t<uint32_t> &prop =
            * static_cast<array_t<uint32_t>*>(buffer);
        std::reverse(prop.begin(), prop.end());
        break;}
    case ibis::INT: {
        array_t<int32_t> &prop =
            * static_cast<array_t<int32_t>*>(buffer);
        std::reverse(prop.begin(), prop.end());
        break;}
    case ibis::USHORT: {
        array_t<uint16_t> &prop =
            * static_cast<array_t<uint16_t>*>(buffer);
        std::reverse(prop.begin(), prop.end());
        break;}
    case ibis::SHORT: {
        array_t<int16_t> &prop =
            * static_cast<array_t<int16_t>*>(buffer);
        std::reverse(prop.begin(), prop.end());
        break;}
    case ibis::UBYTE: {
        array_t<unsigned char> &prop =
            * static_cast<array_t<unsigned char>*>(buffer);
        std::reverse(prop.begin(), prop.end());
        break;}
    case ibis::BYTE: {
        array_t<signed char> &prop =
            * static_cast<array_t<signed char>*>(buffer);
        std::reverse(prop.begin(), prop.end());
        break;}
    case ibis::FLOAT: {
        array_t<float> &prop =
            * static_cast<array_t<float>*>(buffer);
        std::reverse(prop.begin(), prop.end());
        break;}
    case ibis::DOUBLE: {
        array_t<double> &prop =
            * static_cast<array_t<double>*>(buffer);
        std::reverse(prop.begin(), prop.end());
        break;}
    case ibis::CATEGORY:
    case ibis::TEXT: {
        std::vector<std::string> &prop =
            * static_cast<std::vector<std::string>*>(buffer);
        std::reverse(prop.begin(), prop.end());
        break;}
    default: {
        logWarning("reverseRows", "incompatible data type");
        break;}
    }
} // ibis::bord::column::reverseRows

/// Reduce the number of rows stored in this column object to @c nr.  It
/// does nothing if the current size is no more than @c nr.
///
/// It returns 0 upcon successful completion, -1 otherwise.
int ibis::bord::column::limit(uint32_t nr) {
    int ierr = 0;
    switch(m_type) {
    case ibis::ULONG: {
        array_t<uint64_t> &prop =
            * static_cast<array_t<uint64_t>*>(buffer);
        if (nr < prop.size()) {
            prop.resize(nr);
            prop.nosharing();
        }
        break;}
    case ibis::LONG: {
        array_t<int64_t> &prop =
            * static_cast<array_t<int64_t>*>(buffer);
        if (nr < prop.size()) {
            prop.resize(nr);
            prop.nosharing();
        }
        break;}
    case ibis::UINT: {
        array_t<uint32_t> &prop =
            * static_cast<array_t<uint32_t>*>(buffer);
        if (nr < prop.size()) {
            prop.resize(nr);
            prop.nosharing();
        }
        break;}
    case ibis::INT: {
        array_t<int32_t> &prop =
            * static_cast<array_t<int32_t>*>(buffer);
        if (nr < prop.size()) {
            prop.resize(nr);
            prop.nosharing();
        }
        break;}
    case ibis::USHORT: {
        array_t<uint16_t> &prop =
            * static_cast<array_t<uint16_t>*>(buffer);
        if (nr < prop.size()) {
            prop.resize(nr);
            prop.nosharing();
        }
        break;}
    case ibis::SHORT: {
        array_t<int16_t> &prop =
            * static_cast<array_t<int16_t>*>(buffer);
        if (nr < prop.size()) {
            prop.resize(nr);
            prop.nosharing();
        }
        break;}
    case ibis::UBYTE: {
        array_t<unsigned char> &prop =
            * static_cast<array_t<unsigned char>*>(buffer);
        if (nr < prop.size()) {
            prop.resize(nr);
            prop.nosharing();
        }
        break;}
    case ibis::BYTE: {
        array_t<signed char> &prop =
            * static_cast<array_t<signed char>*>(buffer);
        if (nr < prop.size()) {
            prop.resize(nr);
            prop.nosharing();
        }
        break;}
    case ibis::FLOAT: {
        array_t<float> &prop =
            * static_cast<array_t<float>*>(buffer);
        if (nr < prop.size()) {
            prop.resize(nr);
            prop.nosharing();
        }
        break;}
    case ibis::DOUBLE: {
        array_t<double> &prop =
            * static_cast<array_t<double>*>(buffer);
        if (nr < prop.size()) {
            prop.resize(nr);
            prop.nosharing();
        }
        break;}
    case ibis::CATEGORY:
    case ibis::TEXT: {
        std::vector<std::string> &prop =
            * static_cast<std::vector<std::string>*>(buffer);
        if (nr < prop.size())
            prop.resize(nr);
        break;}
    default: {
        logWarning("limit", "incompatible data type");
        ierr = -1;
        break;}
    }
    return ierr;
} // ibis::bord::column::limit

/// Convert the integer representation back to the string representation.
/// The existing data type must be ibis::UINT and the column with the same
/// in in the given ibis::part prt must be of type ibis::CATEGORY.
int ibis::bord::column::restoreCategoriesAsStrings(const ibis::category& cat) {
    if (m_type != ibis::UINT) // must be uint32_t
        return -2;

    ibis::array_t<uint32_t> *arrint =
        static_cast<ibis::array_t<uint32_t>*>(buffer);
    const int nr = (thePart != 0 ? (thePart->nRows() <= arrint->size() ?
                                    thePart->nRows() : arrint->size())
                    : arrint->size());
    std::vector<std::string> *arrstr = new std::vector<std::string>(nr);
    if (dic != 0) {
        for (int j = 0; j < nr; ++ j)
            (*arrstr)[j] = (*dic)[(*arrint)[j]];
    }
    else {
        for (int j = 0; j < nr; ++ j)
            cat.getString((*arrint)[j], (*arrstr)[j]);
    }
    delete arrint; // free the storage for the integers.
    m_type = ibis::CATEGORY;
    buffer = arrstr;
    return nr;
} // ibis::bord::column::restoreCategoriesAsStrings

long ibis::bord::column::append(const char* dt, const char* df,
                                const uint32_t nold, const uint32_t nnew,
                                uint32_t nbuf, char* buf) {
    return ibis::column::append(dt, df, nold, nnew, nbuf, buf);
} // ibis::bord::column::append

/// Append user supplied data to the current column.  The incoming values
/// is carried by a void*, which is cast to the same type as the buffer
/// used by the column.  The mask is used to indicate which values in the
/// incoming array are to be included.
long ibis::bord::column::append(const void* vals, const ibis::bitvector& msk) {
    int ierr = 0;
    if (vals == 0 || msk.size() == 0 || msk.cnt() == 0)
        return ierr;
    if (buffer == 0 && mask_.cnt() > 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- column[" << fullname() << "]::append can not "
            "proceed because the existing data is not in memory";
        return -20;
    }

    switch (m_type) {
    case ibis::BYTE: {
        ibis::bord::column::addIncoreData<signed char>
            (reinterpret_cast<array_t<signed char>*&>(buffer),
             (thePart ? thePart->nRows() : mask_.size()),
             *static_cast<const array_t<signed char>*>(vals),
             static_cast<signed char>(0x7F));
        break;}
    case ibis::UBYTE: {
        ibis::bord::column::addIncoreData<unsigned char>
            (reinterpret_cast<array_t<unsigned char>*&>(buffer),
             (thePart ? thePart->nRows() : mask_.size()),
             *static_cast<const array_t<unsigned char>*>(vals),
             static_cast<unsigned char>(0xFF));
        break;}
    case ibis::SHORT: {
        addIncoreData(reinterpret_cast<array_t<int16_t>*&>(buffer),
                      (thePart ? thePart->nRows() : mask_.size()),
                      *static_cast<const array_t<int16_t>*>(vals),
                      static_cast<int16_t>(0x7FFF));
        break;}
    case ibis::USHORT: {
        addIncoreData(reinterpret_cast<array_t<uint16_t>*&>(buffer),
                      (thePart ? thePart->nRows() : mask_.size()),
                      *static_cast<const array_t<uint16_t>*>(vals),
                      static_cast<uint16_t>(0xFFFF));
        break;}
    case ibis::INT: {
        addIncoreData(reinterpret_cast<array_t<int32_t>*&>(buffer),
                      (thePart ? thePart->nRows() : mask_.size()),
                      *static_cast<const array_t<int32_t>*>(vals),
                      static_cast<int32_t>(0x7FFFFFFF));
        break;}
    case ibis::UINT: {
        addIncoreData(reinterpret_cast<array_t<uint32_t>*&>(buffer),
                      (thePart ? thePart->nRows() : mask_.size()),
                      *static_cast<const array_t<uint32_t>*>(vals),
                      static_cast<uint32_t>(0xFFFFFFFF));
        break;}
    case ibis::LONG: {
        addIncoreData(reinterpret_cast<array_t<int64_t>*&>(buffer),
                      (thePart ? thePart->nRows() : mask_.size()),
                      *static_cast<const array_t<int64_t>*>(vals),
                      static_cast<int64_t>(0x7FFFFFFFFFFFFFFFLL));
        break;}
    case ibis::ULONG: {
        addIncoreData(reinterpret_cast<array_t<uint64_t>*&>(buffer),
                      (thePart ? thePart->nRows() : mask_.size()),
                      *static_cast<const array_t<uint64_t>*>(vals),
                      static_cast<uint64_t>(0xFFFFFFFFFFFFFFFFLL));
        break;}
    case ibis::FLOAT: {
        addIncoreData(reinterpret_cast<array_t<float>*&>(buffer),
                      (thePart ? thePart->nRows() : mask_.size()),
                      *static_cast<const array_t<float>*>(vals),
                      FASTBIT_FLOAT_NULL);
        break;}
    case ibis::DOUBLE: {
        addIncoreData(reinterpret_cast<array_t<double>*&>(buffer),
                      (thePart ? thePart->nRows() : mask_.size()),
                      *static_cast<const array_t<double>*>(vals),
                      FASTBIT_DOUBLE_NULL);
        break;}
    // case ibis::CATEGORY: {
    //  const std::vector<std::string>*stv = static_cast<const std::vector<std::string>*>(vals);
    //  array_t<uint32_t> * ibuffer = new array_t<uint32_t>();

    //  ibuffer->resize(stv->size());
    //  for (size_t i = 0 ; i < stv->size() ; i++) (*ibuffer)[i]=dic->insert(((*stv)[i]).c_str());
    //  addIncoreData(reinterpret_cast<array_t<uint32_t>*&>(buffer),
    //                (thePart ? thePart->nRows() : mask_.size()),
    //                *static_cast<const array_t<uint32_t>*>(ibuffer),
    //                static_cast<uint32_t>(0));
    //  delete ibuffer;
    //  break;
    // }
    case ibis::CATEGORY:
    case ibis::TEXT: {
        addStrings(reinterpret_cast<std::vector<std::string>*&>(buffer),
                   (thePart ? thePart->nRows() : mask_.size()),
                   *static_cast<const std::vector<std::string>*>(vals));
        break;}
    case ibis::BLOB: {
        addBlobs(reinterpret_cast<std::vector<ibis::opaque>*&>(buffer),
                 (thePart ? thePart->nRows() : mask_.size()),
                 *static_cast<const std::vector<ibis::opaque>*>(vals));
        break;}
    case ibis::BIT: {
        if (buffer == 0) {
            buffer = new ibis::bitvector(*static_cast<const bitvector*>(vals));
        }
        else {
            *static_cast<bitvector*>(buffer) +=
                *static_cast<const bitvector*>(vals);
        }
        break;}
    default: {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- column[" << fullname()
            << "]::append -- unable to process column " << m_name
            << " (type " << ibis::TYPESTRING[(int)m_type] << ")";
        ierr = -17;
        break;}
    }

    if (ierr == 0) {
        if (thePart != 0)
            mask_.adjustSize(0, thePart->nRows());
        mask_ += msk;
        LOGGER(ibis::gVerbose > 4)
            << fullname() << "::append added " << msk.size() << " value"
            << (msk.size()>1?"s":"") << " from " << vals << " to " << buffer
            << ", new mask.cnt() = " << mask_.cnt() << " and mask.size() = "
            << mask_.size();
    }
    else {
        LOGGER(ibis::gVerbose > 4)
            << "Warning -- " << fullname() << "::append failed to add "
            << msk.size() << " value" << (msk.size()>1?"s":"") << " from "
            << vals << " to " << buffer << ", ierr = " << ierr;
    }
    unloadIndex();
    return ierr;
} // ibis::bord::column::append

/// Append selected values from the given column to the current column.
/// This function extracts the values using the given mask from scol, and
/// then append the values to the current column.  The type of scol must be
/// ligitimately converted to the type of this column.  It returns the
/// number of values added to the column on success, or a negative number
/// to indicate errors.
long ibis::bord::column::append(const ibis::column& scol,
                                const ibis::bitvector& msk) {
    if (msk.size() == 0 || msk.cnt() == 0) return 0;
    int ierr = 0;
    switch (m_type) {
    case ibis::BYTE: {
        std::unique_ptr< array_t<signed char> > vals(scol.selectBytes(msk));
        if (vals.get() != 0)
            ierr = ibis::bord::column::addIncoreData<signed char>
                (reinterpret_cast<array_t<signed char>*&>(buffer),
                 thePart->nRows(), *vals,
                 static_cast<signed char>(0x7F));
        else
            ierr = -18;
        break;}
    case ibis::UBYTE: {
        std::unique_ptr< array_t<unsigned char> > vals(scol.selectUBytes(msk));
        if (vals.get() != 0)
            ierr = ibis::bord::column::addIncoreData<unsigned char>
                (reinterpret_cast<array_t<unsigned char>*&>(buffer),
                 thePart->nRows(), *vals,
                 static_cast<unsigned char>(0xFF));
        else
            ierr = -18;
        break;}
    case ibis::SHORT: {
        std::unique_ptr< array_t<int16_t> > vals(scol.selectShorts(msk));
        if (vals.get() != 0)
            ierr = addIncoreData(reinterpret_cast<array_t<int16_t>*&>(buffer),
                                 thePart->nRows(), *vals,
                                 static_cast<int16_t>(0x7FFF));
        else
            ierr = -18;
        break;}
    case ibis::USHORT: {
        std::unique_ptr< array_t<uint16_t> > vals(scol.selectUShorts(msk));
        if (vals.get() != 0)
            ierr = addIncoreData(reinterpret_cast<array_t<uint16_t>*&>(buffer),
                                 thePart->nRows(), *vals,
                                 static_cast<uint16_t>(0xFFFF));
        else
            ierr = -18;
        break;}
    case ibis::INT: {
        std::unique_ptr< array_t<int32_t> > vals(scol.selectInts(msk));
        if (vals.get() != 0)
            ierr = addIncoreData(reinterpret_cast<array_t<int32_t>*&>(buffer),
                                 thePart->nRows(), *vals,
                                 static_cast<int32_t>(0x7FFFFFFF));
        else
            ierr = -18;
        break;}
    case ibis::UINT: {
        std::unique_ptr< array_t<uint32_t> > vals(scol.selectUInts(msk));
        if (dic == 0) {
            const ibis::bord::column *bc =
                dynamic_cast<const ibis::bord::column*>(&scol);
            if (bc != 0)
                dic = bc->dic;
        }
        if (vals.get() != 0)
            ierr = addIncoreData(reinterpret_cast<array_t<uint32_t>*&>(buffer),
                                 thePart->nRows(), *vals,
                                 static_cast<uint32_t>(0xFFFFFFFF));
        else
            ierr = -18;
        break;}
    case ibis::LONG: {
        std::unique_ptr< array_t<int64_t> > vals(scol.selectLongs(msk));
        if (vals.get() != 0)
            ierr = addIncoreData(reinterpret_cast<array_t<int64_t>*&>(buffer),
                                 thePart->nRows(), *vals,
                                 static_cast<int64_t>(0x7FFFFFFFFFFFFFFFLL));
        else
            ierr = -18;
        break;}
    case ibis::ULONG: {
        std::unique_ptr< array_t<uint64_t> > vals(scol.selectULongs(msk));
        if (vals.get() != 0)
            ierr = addIncoreData(reinterpret_cast<array_t<uint64_t>*&>(buffer),
                                 thePart->nRows(), *vals,
                                 static_cast<uint64_t>(0xFFFFFFFFFFFFFFFFLL));
        else
            ierr = -18;
        break;}
    case ibis::FLOAT: {
        std::unique_ptr< array_t<float> > vals(scol.selectFloats(msk));
        if (vals.get() != 0)
            ierr = addIncoreData(reinterpret_cast<array_t<float>*&>(buffer),
                                 thePart->nRows(), *vals,
                                 FASTBIT_FLOAT_NULL);
        else
            ierr = -18;
        break;}
    case ibis::DOUBLE: {
        std::unique_ptr< array_t<double> > vals(scol.selectDoubles(msk));
        if (vals.get() != 0)
            ierr = addIncoreData(reinterpret_cast<array_t<double>*&>(buffer),
                                 thePart->nRows(), *vals,
                                 FASTBIT_DOUBLE_NULL);
        else
            ierr = -18;
        break;}
    // case ibis::CATEGORY: {
    //  std::vector<std::string>*vals = scol.selectStrings(msk);
    //  if (vals != 0){
    //      array_t<uint32_t> * ibuffer = new array_t<uint32_t>();

    //      ibuffer->resize(vals->size());
    //      for (size_t i = 0 ; i < vals->size() ; i++) (*ibuffer)[i]=dic->insert(((*vals)[i]).c_str());
    //      ierr = addIncoreData(reinterpret_cast<array_t<uint32_t>*&>(buffer),
    //                           thePart->nRows(),
    //                           *static_cast<const array_t<uint32_t>*>(ibuffer),
    //                           static_cast<uint32_t>(0));
    //      delete ibuffer;
    //      delete vals;
    //  } else {
    //      ierr = -18;
    //  }
    //  break;}
    case ibis::CATEGORY:
    case ibis::TEXT: {
        std::unique_ptr< std::vector<std::string> >
            vals(scol.selectStrings(msk));
        if (vals.get() != 0)
            ierr = addStrings
                (reinterpret_cast<std::vector<std::string>*&>(buffer),
                 thePart->nRows(), *vals);
        else
            ierr = -18;
        break;}
    case ibis::BLOB: {
        std::unique_ptr< std::vector<ibis::opaque> >
            vals(scol.selectOpaques(msk));
        if (vals.get() != 0)
            ierr = addBlobs
                (reinterpret_cast<std::vector<ibis::opaque>*&>(buffer),
                 thePart->nRows(), *vals);
        else
            ierr = -18;
        break;}
    default: {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- column[" << thePart->name() << '.' << m_name
            << "]::append -- unable to process column " << m_name
            << " (type " << ibis::TYPESTRING[(int)m_type] << ")";
        ierr = -17;
        break;}
    }

    if (ierr > 0) {
        const ibis::bitvector::word_t sz = thePart->nRows() + ierr;
        mask_.adjustSize(sz, sz);
    }
    return ierr;
} // ibis::bord::column::append

/// Append selected values from the given column to the current column.
/// This function extracts the values using the given range condition on
/// scol, and then append the values to the current column.  The type of
/// scol must be ligitimately converted to the type of this column.
///
///  It returns 0 to indicate success, a negative number to indicate error.
long ibis::bord::column::append(const ibis::column& scol,
                                const ibis::qContinuousRange& cnd) {
    int ierr = 0;
    switch (m_type) {
    case ibis::BYTE: {
        array_t<signed char> vals;
        ierr = scol.selectValues(cnd, &vals);
        if (ierr > 0)
            ierr = ibis::bord::column::addIncoreData<signed char>
                (reinterpret_cast<array_t<signed char>*&>(buffer),
                 thePart->nRows(), vals,
                 static_cast<signed char>(0x7F));
        break;}
    case ibis::UBYTE: {
        array_t<unsigned char> vals;
        ierr = scol.selectValues(cnd, &vals);
        if (ierr > 0)
            ierr = ibis::bord::column::addIncoreData<unsigned char>
                (reinterpret_cast<array_t<unsigned char>*&>(buffer),
                 thePart->nRows(), vals,
                 static_cast<unsigned char>(0xFF));
        break;}
    case ibis::SHORT: {
        array_t<int16_t> vals;
        ierr = scol.selectValues(cnd, &vals);
        if (ierr > 0)
            ierr = addIncoreData(reinterpret_cast<array_t<int16_t>*&>(buffer),
                                 thePart->nRows(), vals,
                                 static_cast<int16_t>(0x7FFF));
        break;}
    case ibis::USHORT: {
        array_t<uint16_t> vals;
        ierr = scol.selectValues(cnd, &vals);
        if (ierr > 0)
            ierr = addIncoreData(reinterpret_cast<array_t<uint16_t>*&>(buffer),
                                 thePart->nRows(), vals,
                                 static_cast<uint16_t>(0xFFFF));
        break;}
    case ibis::INT: {
        array_t<int32_t> vals;
        ierr = scol.selectValues(cnd, &vals);
        if (ierr > 0)
            ierr = addIncoreData(reinterpret_cast<array_t<int32_t>*&>(buffer),
                                 thePart->nRows(), vals,
                                 static_cast<int32_t>(0x7FFFFFFF));
        break;}
    case ibis::UINT: {
        array_t<uint32_t> vals;
        ierr = scol.selectValues(cnd, &vals);
        if (ierr > 0)
            ierr = addIncoreData(reinterpret_cast<array_t<uint32_t>*&>(buffer),
                                 thePart->nRows(), vals,
                                 static_cast<uint32_t>(0xFFFFFFFF));
        break;}
    case ibis::LONG: {
        array_t<int64_t> vals;
        ierr = scol.selectValues(cnd, &vals);
        if (ierr > 0)
            ierr = addIncoreData(reinterpret_cast<array_t<int64_t>*&>(buffer),
                                 thePart->nRows(), vals,
                                 static_cast<int64_t>(0x7FFFFFFFFFFFFFFFLL));
        break;}
    case ibis::ULONG: {
        array_t<uint64_t> vals;
        ierr = scol.selectValues(cnd, &vals);
        if (ierr > 0)
            ierr = addIncoreData(reinterpret_cast<array_t<uint64_t>*&>(buffer),
                                 thePart->nRows(), vals,
                                 static_cast<uint64_t>(0xFFFFFFFFFFFFFFFFLL));
        break;}
    case ibis::FLOAT: {
        array_t<float> vals;
        ierr = scol.selectValues(cnd, &vals);
        if (ierr > 0)
            ierr = addIncoreData(reinterpret_cast<array_t<float>*&>(buffer),
                                 thePart->nRows(), vals, FASTBIT_FLOAT_NULL);
        break;}
    case ibis::DOUBLE: {
        array_t<double> vals;
        ierr = scol.selectValues(cnd, &vals);
        if (ierr > 0)
            ierr = addIncoreData(reinterpret_cast<array_t<double>*&>(buffer),
                                 thePart->nRows(), vals, FASTBIT_DOUBLE_NULL);
        else
            ierr = -18;
        break;}
    default: {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- column[" << thePart->name() << '.' << m_name
            << "]::append -- unable to process column " << m_name
            << " (type " << ibis::TYPESTRING[(int)m_type] << ")";
        ierr = -17;
        break;}
    }
    if (ierr > 0) {
        const ibis::bitvector::word_t sz = thePart->nRows() + ierr;
        mask_.adjustSize(sz, sz);
    }
    return ierr;
} // ibis::bord::column::append

/// Extend the buffer to have nr elements.  All new elements have the value
/// 1U.
void ibis::bord::column::addCounts(uint32_t nr) {
    if (*m_name.c_str() != '*' || m_type != ibis::UINT) return;
    if (buffer == 0) {
        buffer = new ibis::array_t<uint32_t>(nr, 1U);
    }
    else {
        ibis::array_t<uint32_t> *ubuf =
            static_cast<ibis::array_t<uint32_t>*>(buffer);
        if (nr > ubuf->size())
            ubuf->insert(ubuf->end(), nr - ubuf->size(), 1U);
    }
} // ibis::bord::column::addCounts

template <typename T> int
ibis::bord::column::addIncoreData(array_t<T>*& toa, uint32_t nold,
                                  const array_t<T>& from, const T special) {
    const int nqq = from.size();

    if (toa == 0)
        toa = new array_t<T>();
    if (nqq > 0) {
        if (nold > 0) {
            toa->reserve(nold+nqq);
            if ((size_t)nold > toa->size())
                toa->insert(toa->end(), nold - toa->size(), special);
            toa->insert(toa->end(), from.begin(), from.end());
        }
        else {
            toa->copy(from);
        }
    }
    return nqq;
} // ibis::bord::column::addIncoreData

int ibis::bord::column::addStrings(std::vector<std::string>*& to,
                                   uint32_t nold,
                                   const std::vector<std::string>& from) {
    const int nqq = from.size();
    if (to == 0)
        to = new std::vector<std::string>();
    std::vector<std::string>& target = *to;
    target.reserve(nold+nqq);
    if (nold > (long)target.size()) {
        const std::string dummy;
        target.insert(target.end(), nold-target.size(), dummy);
    }
    if (nqq > 0)
        target.insert(target.end(), from.begin(), from.end());
    return nqq;
} // ibis::bord::column::addStrings

int ibis::bord::column::addBlobs(std::vector<ibis::opaque>*& to,
                                 uint32_t nold,
                                 const std::vector<ibis::opaque>& from) {
    const int nqq = from.size();
    if (to == 0)
        to = new std::vector<ibis::opaque>();
    std::vector<ibis::opaque>& target = *to;
    target.reserve(nold+nqq);
    if (nold > (long)target.size()) {
        const ibis::opaque dummy;
        target.insert(target.end(), nold-target.size(), dummy);
    }
    if (nqq > 0)
        target.insert(target.end(), from.begin(), from.end());
    return nqq;
} // ibis::bord::column::addBlobs

/// Does this column have the same values as the other.
bool ibis::bord::column::equal_to(const ibis::bord::column &other) const {
    if (m_type != other.m_type) return false;
    if (buffer == 0 || other.buffer == 0) return false;
    if (buffer == other.buffer) return true;

    switch (m_type) {
    default:
        return false;
    case ibis::BYTE: {
        const ibis::array_t<signed char> &v0 =
            *static_cast<const ibis::array_t<signed char>*>(buffer);
        const ibis::array_t<signed char> &v1 =
            *static_cast<const ibis::array_t<signed char>*>(other.buffer);
        return v0.equal_to(v1);}
    case ibis::UBYTE: {
        const ibis::array_t<unsigned char> &v0 =
            *static_cast<const ibis::array_t<unsigned char>*>(buffer);
        const ibis::array_t<unsigned char> &v1 =
            *static_cast<const ibis::array_t<unsigned char>*>(other.buffer);
        return v0.equal_to(v1);}
    case ibis::SHORT: {
        const ibis::array_t<int16_t> &v0 =
            *static_cast<const ibis::array_t<int16_t>*>(buffer);
        const ibis::array_t<int16_t> &v1 =
            *static_cast<const ibis::array_t<int16_t>*>(other.buffer);
        return v0.equal_to(v1);}
    case ibis::USHORT: {
        const ibis::array_t<uint16_t> &v0 =
            *static_cast<const ibis::array_t<uint16_t>*>(buffer);
        const ibis::array_t<uint16_t> &v1 =
            *static_cast<const ibis::array_t<uint16_t>*>(other.buffer);
        return v0.equal_to(v1);}
    case ibis::INT: {
        const ibis::array_t<int32_t> &v0 =
            *static_cast<const ibis::array_t<int32_t>*>(buffer);
        const ibis::array_t<int32_t> &v1 =
            *static_cast<const ibis::array_t<int32_t>*>(other.buffer);
        return v0.equal_to(v1);}
    case ibis::UINT: {
        const ibis::array_t<uint32_t> &v0 =
            *static_cast<const ibis::array_t<uint32_t>*>(buffer);
        const ibis::array_t<uint32_t> &v1 =
            *static_cast<const ibis::array_t<uint32_t>*>(other.buffer);
        return v0.equal_to(v1);}
    case ibis::LONG: {
        const ibis::array_t<int64_t> &v0 =
            *static_cast<const ibis::array_t<int64_t>*>(buffer);
        const ibis::array_t<int64_t> &v1 =
            *static_cast<const ibis::array_t<int64_t>*>(other.buffer);
        return v0.equal_to(v1);}
    case ibis::ULONG: {
        const ibis::array_t<uint64_t> &v0 =
            *static_cast<const ibis::array_t<uint64_t>*>(buffer);
        const ibis::array_t<uint64_t> &v1 =
            *static_cast<const ibis::array_t<uint64_t>*>(other.buffer);
        return v0.equal_to(v1);}
    case ibis::FLOAT: {
        const ibis::array_t<float> &v0 =
            *static_cast<const ibis::array_t<float>*>(buffer);
        const ibis::array_t<float> &v1 =
            *static_cast<const ibis::array_t<float>*>(other.buffer);
        return v0.equal_to(v1);}
    case ibis::DOUBLE: {
        const ibis::array_t<double> &v0 =
            *static_cast<const ibis::array_t<double>*>(buffer);
        const ibis::array_t<double> &v1 =
            *static_cast<const ibis::array_t<double>*>(other.buffer);
        return v0.equal_to(v1);}
    // case ibis::CATEGORY:{
    //  const ibis::dictionary * aDic1 = other.getDictionary();
    //  if (dic->equal_to(*aDic1)) {
    //      const ibis::array_t<uint32_t> &v0 = *static_cast<const ibis::array_t<uint32_t>*>(buffer);
    //      const ibis::array_t<uint32_t> &v1 = *static_cast<const ibis::array_t<uint32_t>*>(other.buffer);
    //      return v0.equal_to(v1);
    //  } else return false;

    // }
    case ibis::CATEGORY:
    case ibis::TEXT: {
        const std::vector<std::string> &v0 =
            *static_cast<const std::vector<std::string>*>(buffer);
        const std::vector<std::string> &v1 =
            *static_cast<const std::vector<std::string>*>(other.buffer);
        bool match = (v0.size() == v1.size());
        for (size_t j = 0; match && j < v0.size(); ++ j)
            match = (0 == v0[j].compare(v1[j]));
        return match;}
    }
} // ibis::bord::column::equal_to

/// Specify the shape of the array.  The name is meant to suggest that the
/// data was originally defined on a mesh.
int ibis::bord::column::setMeshShape(uint64_t *dims, uint64_t nd) {
    uint64_t n = *dims;
    for (unsigned j = 1; j < nd; ++ j)
        n *= dims[j];
    if (n > 0x7FFFFFFFUL) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- column[" << fullname() << "]::setMeshShape can not "
            "proceed because the number of elements (" << n
            << ") exceeds 0x7FFFFFFF";
        return 0;
    }

    shape.clear();
    mask_.set(1, n);
    shape.insert(shape.end(), dims, dims+nd);
    return 0;
} // ibis::bord::column::setMeshShape

bool ibis::bord::column::hasRawData() const {
    if (dataflag == 0)
        dataflag = (buffer != 0 ? 1 : -1);
    return (dataflag > 0);
} // ibis::bord::column::hasRawData

/// Constructor.  It retrieves the columns from the table object using that
/// function ibis::part::getColumn(uint32_t), which preserves the order
/// specified in the original table construction, but may leave the columns
/// in an arbitrary order.
ibis::bord::cursor::cursor(const ibis::bord &t)
    : buffer(t.nColumns()), tab(t), curRow(-1) {
    if (buffer.empty()) return;
    for (uint32_t j = 0; j < t.nColumns(); ++ j) {
        const ibis::bord::column *col =
            dynamic_cast<const ibis::bord::column*>(t.getColumn(j));
        if (col != 0) {
            buffer[j].cname = col->name();
            buffer[j].ctype = col->type();
            buffer[j].cval = col->getArray();
            buffer[j].dic = col->getDictionary();
            bufmap[col->name()] = j;
        }
    }
} // ibis::bord::cursor::cursor

/// Print the content of the current row.
int ibis::bord::cursor::dump(std::ostream& out, const char* del) const {
    if (curRow < 0 || curRow >= (int64_t) tab.nRows())
        return -1;
    if (! out) return -4;

    const uint32_t cr = static_cast<uint32_t>(curRow);
    int ierr = dumpIJ(out, cr, 0U);
    if (ierr < 0) return ierr;
    if (del == 0) del = ", ";
    for (uint32_t j = 1; j < buffer.size(); ++ j) {
        out << del;
        ierr = dumpIJ(out, cr, j);
        if (ierr < 0) return ierr;
    }
    out << "\n";
    return (out ? ierr : -4);
} // ibis::bord::cursor::dump

void ibis::bord::cursor::fillRow(ibis::table::row& res) const {
    res.clear();
    for (uint32_t j = 0; j < buffer.size(); ++ j) {
        switch (buffer[j].ctype) {
        case ibis::BYTE: {
            res.bytesnames.push_back(buffer[j].cname);
            if (buffer[j].cval) {
                res.bytesvalues.push_back
                    ((* static_cast<const array_t<const char>*>
                      (buffer[j].cval))[curRow]);
            }
            else {
                res.bytesvalues.push_back(0x7F);
            }
            break;}
        case ibis::UBYTE: {
            res.ubytesnames.push_back(buffer[j].cname);
            if (buffer[j].cval) {
                res.ubytesvalues.push_back
                    ((* static_cast<const array_t<const unsigned char>*>
                      (buffer[j].cval))[curRow]);
            }
            else {
                res.ubytesvalues.push_back(0xFF);
            }
            break;}
        case ibis::SHORT: {
            res.shortsnames.push_back(buffer[j].cname);
            if (buffer[j].cval) {
                res.shortsvalues.push_back
                    ((* static_cast<const array_t<int16_t>*>
                      (buffer[j].cval))[curRow]);
            }
            else {
                res.shortsvalues.push_back(0x7FFF);
            }
            break;}
        case ibis::USHORT: {
            res.ushortsnames.push_back(buffer[j].cname);
            if (buffer[j].cval) {
                res.ushortsvalues.push_back
                    ((* static_cast<const array_t<const uint16_t>*>
                      (buffer[j].cval))[curRow]);
            }
            else {
                res.ushortsvalues.push_back(0xFFFF);
            }
            break;}
        case ibis::INT: {
            res.intsnames.push_back(buffer[j].cname);
            if (buffer[j].cval) {
                res.intsvalues.push_back
                    ((* static_cast<const array_t<int32_t>*>
                      (buffer[j].cval))[curRow]);
            }
            else {
                res.intsvalues.push_back(0x7FFFFFFF);
            }
            break;}
        case ibis::UINT: {
            if (buffer[j].cval && buffer[j].dic ) { // categorical values
                const array_t<uint32_t> *vals =
                    static_cast<const array_t<uint32_t>*>(buffer[j].cval);
                const uint32_t val = (*vals)[curRow];
                res.catsnames.push_back(buffer[j].cname);
                res.catsvalues.push_back((*buffer[j].dic)[val]);
            }
            else { // uint32_t
                res.uintsnames.push_back(buffer[j].cname);
                if (buffer[j].cval) {
                    res.uintsvalues.push_back
                        ((* static_cast<const array_t<const uint32_t>*>
                          (buffer[j].cval))[curRow]);
                }
                else {
                    res.uintsvalues.push_back(0xFFFFFFFF);
                }
            }
            break;}
        case ibis::LONG: {
            res.longsnames.push_back(buffer[j].cname);
            if (buffer[j].cval) {
                res.longsvalues.push_back
                    ((* static_cast<const array_t<int64_t>*>
                      (buffer[j].cval))[curRow]);
            }
            else {
                res.longsvalues.push_back(0x7FFFFFFFFFFFFFFFLL);
            }
            break;}
        case ibis::ULONG: {
            res.ulongsnames.push_back(buffer[j].cname);
            if (buffer[j].cval) {
                res.ulongsvalues.push_back
                    ((* static_cast<const array_t<const uint64_t>*>
                      (buffer[j].cval))[curRow]);
            }
            else {
                res.ulongsvalues.push_back(0xFFFFFFFFFFFFFFFFULL);
            }
            break;}
        case ibis::FLOAT: {
            res.floatsnames.push_back(buffer[j].cname);
            if (buffer[j].cval) {
                res.floatsvalues.push_back
                    ((* static_cast<const array_t<float>*>
                      (buffer[j].cval))[curRow]);
            }
            else {
                res.floatsvalues.push_back(FASTBIT_FLOAT_NULL);
            }
            break;}
        case ibis::DOUBLE: {
            res.doublesnames.push_back(buffer[j].cname);
            if (buffer[j].cval) {
                res.doublesvalues.push_back
                    ((* static_cast<const array_t<const double>*>
                      (buffer[j].cval))[curRow]);
            }
            else {
                res.doublesvalues.push_back(FASTBIT_DOUBLE_NULL);
            }
            break;}
        // case ibis::CATEGORY: {
        //     res.catsnames.push_back(buffer[j].cname);
        //     if (buffer[j].cval) {
        //      uint32_t v = (* static_cast<const array_t<uint32_t>*>(buffer[j].cval))[curRow];
        //      const ibis::dictionary * aDic = buffer[j].dic;
        //      res.catsvalues.push_back((*aDic)[v]);
        //     }
        //     else {
        //      res.catsvalues.push_back("");
        //     }
        //     break;}
        case ibis::CATEGORY:
        case ibis::TEXT: {
            res.textsnames.push_back(buffer[j].cname);
            if (buffer[j].cval) {
                res.textsvalues.push_back
                    ((* static_cast<const std::vector<std::string>*>
                      (buffer[j].cval))[curRow]);
            }
            else {
                res.textsvalues.push_back("");
            }
            break;}
        default: { // unexpected
            if (ibis::gVerbose > 1)
                ibis::util::logMessage
                    ("Warning", "bord::cursor::fillRow is not expected "
                     "to encounter data type %s (column name %s)",
                     ibis::TYPESTRING[(int)buffer[j].ctype], buffer[j].cname);
            break;}
        }
    }
} // ibis::bord::cursor::fillRow

int ibis::bord::cursor::getColumnAsByte(uint32_t j, char& val) const {
    if (curRow < 0 || curRow >= (int64_t) tab.nRows())
        return -1;
    if (buffer[j].cval == 0)
        return -2;

    int ierr;
    switch (buffer[j].ctype) {
    case ibis::BYTE:
    case ibis::UBYTE: {
        val = (* static_cast<const array_t<signed char>*>(buffer[j].cval))
            [curRow];
        ierr = 0;
        break;}
    default: {
        ierr = -1;
        break;}
    }
    return ierr;
} // ibis::bord::cursor::getColumnAsByte

int ibis::bord::cursor::getColumnAsUByte(uint32_t j, unsigned char& val) const {
    if (curRow < 0 || curRow >= (int64_t) tab.nRows())
        return -1;
    if (buffer[j].cval == 0)
        return -2;

    int ierr;
    switch (buffer[j].ctype) {
    case ibis::BYTE:
    case ibis::UBYTE: {
        val = (* static_cast<const array_t<unsigned char>*>(buffer[j].cval))
            [curRow];
        ierr = 0;
        break;}
    default: {
        ierr = -1;
        break;}
    }
    return ierr;
} // ibis::bord::cursor::getColumnAsUByte

int ibis::bord::cursor::getColumnAsShort(uint32_t j, int16_t& val) const {
    if (curRow < 0 || curRow >= (int64_t) tab.nRows())
        return -1;
    if (buffer[j].cval == 0)
        return -2;

    int ierr;
    switch (buffer[j].ctype) {
    case ibis::BYTE: {
        val = (* static_cast<const array_t<signed char>*>(buffer[j].cval))
            [curRow];
        ierr = 0;
        break;}
    case ibis::UBYTE: {
        val = (* static_cast<const array_t<unsigned char>*>
               (buffer[j].cval))[curRow];
        ierr = 0;
        break;}
    case ibis::SHORT:
    case ibis::USHORT: {
        val = (* static_cast<const array_t<int16_t>*>(buffer[j].cval))
            [curRow];
        ierr = 0;
        break;}
    default: {
        ierr = -1;
        break;}
    }
    return ierr;
} // ibis::bord::cursor::getColumnAsShort

int ibis::bord::cursor::getColumnAsUShort(uint32_t j, uint16_t& val) const {
    if (curRow < 0 || curRow >= (int64_t) tab.nRows())
        return -1;
    if (buffer[j].cval == 0)
        return -2;

    int ierr;
    switch (buffer[j].ctype) {
    case ibis::BYTE:
    case ibis::UBYTE: {
        val = (* static_cast<const array_t<unsigned char>*>(buffer[j].cval))
            [curRow];
        ierr = 0;
        break;}
    case ibis::SHORT:
    case ibis::USHORT: {
        val = (* static_cast<const array_t<uint16_t>*>(buffer[j].cval))[curRow];
        ierr = 0;
        break;}
    default: {
        ierr = -1;
        break;}
    }
    return ierr;
} // ibis::bord::cursor::getColumnAsUShort

int ibis::bord::cursor::getColumnAsInt(uint32_t j, int32_t& val) const {
    if (curRow < 0 || curRow >= (int64_t) tab.nRows())
        return -1;
    if (buffer[j].cval == 0)
        return -2;

    int ierr;
    switch (buffer[j].ctype) {
    case ibis::BYTE: {
        val = (* static_cast<const array_t<signed char>*>(buffer[j].cval))
            [curRow];
        ierr = 0;
        break;}
    case ibis::UBYTE: {
        val = (* static_cast<const array_t<unsigned char>*>
               (buffer[j].cval))[curRow];
        ierr = 0;
        break;}
    case ibis::SHORT: {
        val = (* static_cast<const array_t<int16_t>*>
               (buffer[j].cval))[curRow];
        ierr = 0;
        break;}
    case ibis::USHORT: {
        val = (* static_cast<const array_t<uint16_t>*>
               (buffer[j].cval))[curRow];
        ierr = 0;
        break;}
    case ibis::INT:
    case ibis::UINT: {
        val = (* static_cast<const array_t<int32_t>*>
               (buffer[j].cval))[curRow];
        ierr = 0;
        break;}
    default: {
        ierr = -1;
        break;}
    }
    return ierr;
} // ibis::bord::cursor::getColumnAsInt

int ibis::bord::cursor::getColumnAsUInt(uint32_t j, uint32_t& val) const {
    if (curRow < 0 || curRow >= (int64_t) tab.nRows())
        return -1;
    if (buffer[j].cval == 0)
        return -2;

    int ierr;
    switch (buffer[j].ctype) {
    case ibis::BYTE:
    case ibis::UBYTE: {
        val = (* static_cast<const array_t<unsigned char>*>
               (buffer[j].cval))[curRow];
        ierr = 0;
        break;}
    case ibis::SHORT:
    case ibis::USHORT: {
        val = (* static_cast<const array_t<uint16_t>*>
               (buffer[j].cval))[curRow];
        ierr = 0;
        break;}
    case ibis::INT:
    case ibis::UINT: {
        val = (* static_cast<const array_t<uint32_t>*>
               (buffer[j].cval))[curRow];
        ierr = 0;
        break;}
    default: {
        ierr = -1;
        break;}
    }
    return ierr;
} // ibis::bord::cursor::getColumnAsUInt

int ibis::bord::cursor::getColumnAsLong(uint32_t j, int64_t& val) const {
    if (curRow < 0 || curRow >= (int64_t) tab.nRows())
        return -1;
    if (buffer[j].cval == 0)
        return -2;

    int ierr;
    switch (buffer[j].ctype) {
    case ibis::BYTE: {
        val = (* static_cast<const array_t<signed char>*>(buffer[j].cval))
            [curRow];
        ierr = 0;
        break;}
    case ibis::UBYTE: {
        val = (* static_cast<const array_t<unsigned char>*>
               (buffer[j].cval))[curRow];
        ierr = 0;
        break;}
    case ibis::SHORT: {
        val = (* static_cast<const array_t<int16_t>*>
               (buffer[j].cval))[curRow];
        ierr = 0;
        break;}
    case ibis::USHORT: {
        val = (* static_cast<const array_t<uint16_t>*>
               (buffer[j].cval))[curRow];
        ierr = 0;
        break;}
    case ibis::INT: {
        val = (* static_cast<const array_t<int32_t>*>
               (buffer[j].cval))[curRow];
        ierr = 0;
        break;}
    case ibis::UINT: {
        val = (* static_cast<const array_t<uint32_t>*>
               (buffer[j].cval))[curRow];
        ierr = 0;
        break;}
    case ibis::LONG:
    case ibis::ULONG: {
        val = (* static_cast<const array_t<int64_t>*>
               (buffer[j].cval))[curRow];
        ierr = 0;
        break;}
    default: {
        ierr = -1;
        break;}
    }
    return ierr;
} // ibis::bord::cursor::getColumnAsLong

int ibis::bord::cursor::getColumnAsULong(uint32_t j, uint64_t& val) const {
    if (curRow < 0 || curRow >= (int64_t) tab.nRows())
        return -1;
    if (buffer[j].cval == 0)
        return -2;

    int ierr;
    switch (buffer[j].ctype) {
    case ibis::BYTE:
    case ibis::UBYTE: {
        val = (* static_cast<const array_t<unsigned char>*>
               (buffer[j].cval))[curRow];
        ierr = 0;
        break;}
    case ibis::SHORT:
    case ibis::USHORT: {
        val = (* static_cast<const array_t<uint16_t>*>
               (buffer[j].cval))[curRow];
        ierr = 0;
        break;}
    case ibis::INT:
    case ibis::UINT: {
        val = (* static_cast<const array_t<uint32_t>*>
               (buffer[j].cval))[curRow];
        ierr = 0;
        break;}
    case ibis::LONG:
    case ibis::ULONG: {
        val = (* static_cast<const array_t<uint64_t>*>
               (buffer[j].cval))[curRow];
        ierr = 0;
        break;}
    default: {
        ierr = -1;
        break;}
    }
    return ierr;
} // ibis::bord::cursor::getColumnAsULong

int ibis::bord::cursor::getColumnAsFloat(uint32_t j, float& val) const {
    if (curRow < 0 || curRow >= (int64_t) tab.nRows())
        return -1;
    if (buffer[j].cval == 0)
        return -2;

    int ierr;
    switch (buffer[j].ctype) {
    case ibis::BYTE: {
        val = (* static_cast<const array_t<signed char>*>(buffer[j].cval))
            [curRow];
        ierr = 0;
        break;}
    case ibis::UBYTE: {
        val = (* static_cast<const array_t<unsigned char>*>
               (buffer[j].cval))[curRow];
        ierr = 0;
        break;}
    case ibis::SHORT: {
        val = (* static_cast<const array_t<int16_t>*>
               (buffer[j].cval))[curRow];
        ierr = 0;
        break;}
    case ibis::USHORT: {
        val = (* static_cast<const array_t<uint16_t>*>
               (buffer[j].cval))[curRow];
        ierr = 0;
        break;}
    case ibis::FLOAT: {
        val = (* static_cast<const array_t<float>*>
               (buffer[j].cval))[curRow];
        ierr = 0;
        break;}
    default: {
        ierr = -1;
        break;}
    }
    return ierr;
} // ibis::bord::cursor::getColumnAsFloat

int ibis::bord::cursor::getColumnAsDouble(uint32_t j, double& val) const {
    if (curRow < 0 || curRow >= (int64_t) tab.nRows())
        return -1;
    if (buffer[j].cval == 0)
        return -2;

    int ierr;
    switch (buffer[j].ctype) {
    case ibis::BYTE: {
        val = (* static_cast<const array_t<signed char>*>
               (buffer[j].cval))[curRow];
        ierr = 0;
        break;}
    case ibis::UBYTE: {
        val = (* static_cast<const array_t<unsigned char>*>
               (buffer[j].cval))[curRow];
        ierr = 0;
        break;}
    case ibis::SHORT: {
        val = (* static_cast<const array_t<int16_t>*>
               (buffer[j].cval))[curRow];
        ierr = 0;
        break;}
    case ibis::USHORT: {
        val = (* static_cast<const array_t<uint16_t>*>
               (buffer[j].cval))[curRow];
        ierr = 0;
        break;}
    case ibis::INT: {
        val = (* static_cast<const array_t<int32_t>*>
               (buffer[j].cval))[curRow];
        ierr = 0;
        break;}
    case ibis::UINT: {
        val = (* static_cast<const array_t<uint32_t>*>
               (buffer[j].cval))[curRow];
        ierr = 0;
        break;}
    case ibis::FLOAT: {
        val = (* static_cast<const array_t<float>*>(buffer[j].cval))[curRow];
        ierr = 0;
        break;}
    case ibis::DOUBLE: {
        val = (* static_cast<const array_t<double>*>(buffer[j].cval))[curRow];
        ierr = 0;
        break;}
    default: {
        ierr = -1;
        break;}
    }
    return ierr;
} // ibis::bord::cursor::getColumnAsDouble

int ibis::bord::cursor::getColumnAsString(uint32_t j, std::string& val) const {
    if (curRow < 0 || curRow >= (int64_t) tab.nRows())
        return -1;
    if (buffer[j].cval == 0)
        return -2;

    int ierr;
    std::ostringstream oss;
    switch (buffer[j].ctype) {
    case ibis::BYTE: {
        oss << static_cast<int>((* static_cast<const array_t<signed char>*>
                                 (buffer[j].cval))[curRow]);
        val = oss.str();
        ierr = 0;
        break;}
    case ibis::UBYTE: {
        oss << static_cast<unsigned>
            ((* static_cast<const array_t<unsigned char>*>
              (buffer[j].cval))[curRow]);
        val = oss.str();
        ierr = 0;
        break;}
    case ibis::SHORT: {
        oss << (* static_cast<const array_t<int16_t>*>
                (buffer[j].cval))[curRow];
        val = oss.str();
        ierr = 0;
        break;}
    case ibis::USHORT: {
        oss << (* static_cast<const array_t<uint16_t>*>
                (buffer[j].cval))[curRow];
        val = oss.str();
        ierr = 0;
        break;}
    case ibis::INT: {
        oss << (* static_cast<const array_t<int32_t>*>
                (buffer[j].cval))[curRow];
        val = oss.str();
        ierr = 0;
        break;}
    case ibis::UINT: {
        const ibis::array_t<uint32_t> &arr =
            (* static_cast<const array_t<uint32_t>*>(buffer[j].cval));
        if (buffer[j].dic == 0) {
            oss << arr[curRow];
            val = oss.str();
        }
        else if (buffer[j].dic->size() >= arr[curRow]) {
            val = buffer[j].dic->operator[]
                ((* static_cast<const array_t<uint32_t>*>
                  (buffer[j].cval))[curRow]);
        }
        else {
            oss << arr[curRow];
            val = oss.str();
        }
        ierr = 0;
        break;}
    case ibis::LONG: {
        oss << (* static_cast<const array_t<int64_t>*>
                (buffer[j].cval))[curRow];
        val = oss.str();
        ierr = 0;
        break;}
    case ibis::ULONG: {
        oss << (* static_cast<const array_t<uint64_t>*>
                (buffer[j].cval))[curRow];
        val = oss.str();
        ierr = 0;
        break;}
    case ibis::FLOAT: {
        oss << (* static_cast<const array_t<float>*>
                (buffer[j].cval))[curRow];
        val = oss.str();
        ierr = 0;
        break;}
    case ibis::DOUBLE: {
        oss << (* static_cast<const array_t<double>*>
                (buffer[j].cval))[curRow];
        val = oss.str();
        ierr = 0;
        break;}
    case ibis::CATEGORY:{
        const std::vector<std::string> *v =
            static_cast<const std::vector<std::string>*>(buffer[j].cval);
        if (v->size() > (unsigned long)curRow) {
            val = (*v)[curRow];
            ierr = 0;
        }
        else {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- bord::cursor::getColumnAsString failed to "
                "recover the value of column " << j;
            ierr = -4;
        }
        break;}
    case ibis::TEXT: {
        const ibis::column* col = tab.getColumn(buffer[j].cname);
        if (col != 0) {
            col->getString(static_cast<uint32_t>(curRow), val);
            ierr = 0;
        }
        else {
            ierr = -1;
        }
        break;}
    default: {
        ierr = -1;
        break;}
    }
    return ierr;
} // ibis::bord::cursor::getColumnAsString

int ibis::bord::cursor::getColumnAsOpaque(uint32_t j, ibis::opaque& val) const {
    if (curRow < 0 || curRow >= (int64_t) tab.nRows())
        return -1;
    if (buffer[j].cval == 0)
        return -2;

    int ierr;
    std::ostringstream oss;
    switch (buffer[j].ctype) {
    case ibis::BYTE: {
        const ibis::array_t<signed char> &arr =
            * static_cast<const ibis::array_t<signed char>*>(buffer[j].cval);
        if (arr.size() > (unsigned) curRow) {
            val.copy((const char*)(arr.begin()+curRow), sizeof(char));
            ierr = 0;
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::UBYTE: {
        const ibis::array_t<unsigned char> &arr =
            * static_cast<const ibis::array_t<unsigned char>*>(buffer[j].cval);
        if (arr.size() > (unsigned) curRow) {
            val.copy((const char*)(arr.begin()+curRow), sizeof(char));
            ierr = 0;
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::SHORT: {
        const ibis::array_t<int16_t> &arr =
            * static_cast<const ibis::array_t<int16_t>*>(buffer[j].cval);
        if (arr.size() > (unsigned) curRow) {
            val.copy((const char*)(arr.begin()+curRow), sizeof(int16_t));
            ierr = 0;
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::USHORT: {
        const ibis::array_t<uint16_t> &arr =
            * static_cast<const ibis::array_t<uint16_t>*>(buffer[j].cval);
        if (arr.size() > (unsigned) curRow) {
            val.copy((const char*)(arr.begin()+curRow), sizeof(uint16_t));
            ierr = 0;
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::INT: {
        const ibis::array_t<int32_t> &arr =
            * static_cast<const ibis::array_t<int32_t>*>(buffer[j].cval);
        if (arr.size() > (unsigned) curRow) {
            val.copy((const char*)(arr.begin()+curRow), sizeof(int32_t));
            ierr = 0;
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::UINT: {
        const ibis::array_t<uint32_t> &arr =
            (* static_cast<const array_t<uint32_t>*>(buffer[j].cval));
        if (arr.size() > (unsigned) curRow) {
            if (buffer[j].dic == 0) {
                val.copy((const char*)(arr.begin()+curRow), sizeof(int32_t));
            }
            else if (buffer[j].dic->size() >= arr[curRow]) {
                const char* str = buffer[j].dic->operator[](arr[curRow]);
                val.copy(str, std::strlen(str));
            }
            else {
                val.copy((const char*)(arr.begin()+curRow), sizeof(int32_t));
            }
            ierr = 0;
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::LONG: {
        const ibis::array_t<int64_t> &arr =
            * static_cast<const ibis::array_t<int64_t>*>(buffer[j].cval);
        if (arr.size() > (unsigned) curRow) {
            val.copy((const char*)(arr.begin()+curRow), sizeof(int64_t));
            ierr = 0;
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::ULONG: {
        const ibis::array_t<uint64_t> &arr =
            * static_cast<const ibis::array_t<uint64_t>*>(buffer[j].cval);
        if (arr.size() > (unsigned) curRow) {
            val.copy((const char*)(arr.begin()+curRow), sizeof(int64_t));
            ierr = 0;
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::FLOAT: {
        const ibis::array_t<float> &arr =
            * static_cast<const ibis::array_t<float>*>(buffer[j].cval);
        if (arr.size() > (unsigned) curRow) {
            val.copy((const char*)(arr.begin()+curRow), sizeof(float));
            ierr = 0;
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::DOUBLE: {
        const ibis::array_t<double> &arr =
            * static_cast<const ibis::array_t<double>*>(buffer[j].cval);
        if (arr.size() > (unsigned) curRow) {
            val.copy((const char*)(arr.begin()+curRow), sizeof(double));
            ierr = 0;
        }
        else {
            ierr = -4;
        }
        break;}
    case ibis::TEXT:
    case ibis::CATEGORY:{
        const std::vector<std::string> &arr =
            * static_cast<const std::vector<std::string>*>(buffer[j].cval);
        if (arr.size() > (unsigned long)curRow) {
            val.copy(arr[curRow].data(), arr[curRow].size());
            ierr = 0;
        }
        else {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- bord::cursor::getColumnAsOpaque failed to "
                "recover the value of column " << j;
            ierr = -4;
        }
        break;}
    case ibis::BLOB: {
        const std::vector<ibis::opaque> &arr =
            * static_cast<const std::vector<ibis::opaque>*>(buffer[j].cval);
        if (arr.size() > (unsigned) curRow) {
            val = arr[curRow];
            ierr = 0;
        }
        else {
            ierr = -4;
        }
        break;}
    default: {
        ierr = -1;
        break;}
    }
    return ierr;
} // ibis::bord::cursor::getColumnAsOpaque

/// Constructor.
///
/// @param[in] nd: number of dimensions specified for the hyperslab.
///
/// @param[in] start: starting positions of each dimension specified.  If
/// this pointer is nil, the default value is 0.  @param[in] stride? stride
/// of each dimension specified.  If this pointer is nil, the default value
/// is 1.
///
/// @param[in] count: the number of elements to be used in each dimension.
/// If this pointer is nil, the default value is the maximum value of
/// 64-bit unsigned integer, which implies that all points from start to
/// the end of the dimensions are included in the hyperslab.  If the count
/// value is 0 for any dimension, then the hyperslab is empty (i.e., not
/// selecting any values).
///
/// @param[in] block: the number of consecutive elements to be picked for
/// the hyperlab in each stride.  It specifies a value for each dimension
/// when this pointer is not nil.  If this pointer is nil, the default
/// value is 1.  If the block size in any dimension is 0, the byperslab
/// select no elements and is called an empty hyperslab.
ibis::hyperslab::hyperslab(unsigned nd, const uint64_t *start,
                           const uint64_t *stride, const uint64_t *count,
                           const uint64_t *block) : ndim(nd), vals(4*nd) {
    for (unsigned j = 0; j < nd; ++ j) {
        unsigned j4 = j*4;
        vals[j4]   = (start!=0 ?start[j] :0);
        vals[j4+1] = (stride!=0?stride[j]:1);
        vals[j4+2] = (count!=0 ?count[j] :std::numeric_limits<uint64_t>::max());
        vals[j4+3] = (block!=0 ?block[j] :1);
    }
} // ibis::hyperslab::hyperslab

/// Convert to a bitvector.
///
/// The argument mdim is the number of dimension of the full mesh, which
/// must be at least as large as ndim of the hyperslab object.  The
/// argument dim is the full extend of the mesh dimensions.  When the value
/// of mdim is larger than the member variable ndim, the number of
/// dimensions specified are assumed to the first ndim-dimensions of the
/// mesh.  The remaining dimensions are assumed to the faster varying
/// dimension in the linearization order of the array.
///
/// @note Since the bitvector class uses 32-bit words to represent the
/// number of bits, this function will not work properly for mesh with more
/// than 2^32 points!
void ibis::hyperslab::tobitvector(unsigned mdim, const uint64_t *dim,
                                  ibis::bitvector &bv) const {
    bv.clear();
    if (mdim < ndim || mdim == 0) {
        LOGGER(ibis::gVerbose > 2)
            << "Warning -- hyperslab::tobitvector encountered parameter "
            "error: mdim (" << mdim << ") < ndim (" << ndim << ')';
        return;
    }

    ibis::array_t<ibis::bitvector::word_t> cub(ndim);
    uint64_t tot = 1;
    for (unsigned j = ndim; j > 0;) {
        -- j;
        tot *= dim[j];
        cub[j] = tot;
        if (cub[j] != tot) {
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- hyperslab::tobitvector " << (ndim-j)
                << "-d hypercube contains " << tot
                << " points, which CANNOT be represented in a 32-bit integer";
            return;
        }
    }
    if (tot == 0) {
        if (ibis::gVerbose > 0) {
            ibis::util::logger lg;
            lg() << "Warning -- hyperslab::tobitvector encounters an empty "
                 "domain [" << dim[0];
            for (unsigned j = 1; j < ndim; ++ j)
                lg() << ", " << dim[j];
        }
        return;
    }

    switch (ndim) {
    case 1: { // 1-D
        if (vals[2] == 0 || vals[3] == 0) break;
        if (mdim > 1) {
            for (size_t i1 = 0; i1 < vals[2]; ++ i1) {
                ibis::bitvector::word_t boffset = (vals[0] + vals[1]*i1)*cub[1];
                bv.adjustSize(0, boffset);
                bv.appendFill(1, vals[3]*cub[1]);
            }
        }
        else {
            for (size_t i1 = 0; i1 < vals[2]; ++ i1) {
                bv.adjustSize(0, vals[0] + vals[1]*i1);
                bv.appendFill(1, vals[3]);
            }
        }
        break;}
    case 2: { // 2-D
        if (vals[2] == 0 || vals[3] == 0 ||
            vals[6] == 0 || vals[7] == 0) break;
        if (mdim > 2) {
            for (unsigned i1 = 0; i1 < vals[2]; ++ i1) {
                ibis::bitvector::word_t j1 = (vals[0] + vals[1]*i1);
                for (unsigned i1b = 0; i1b < vals[3]; ++ i1b) {
                    for (unsigned i2 = 0; i2 < vals[6]; ++ i2) {
                        ibis::bitvector::word_t j2 = (j1 + i1b) * cub[1]
                            + vals[4] + i2*vals[5];
                        bv.adjustSize(0, j2*cub[2]);
                        bv.appendFill(1, vals[7]*cub[2]);
                    }
                }
            }
        }
        else {
            for (unsigned i1 = 0; i1 < vals[2]; ++ i1) {
                ibis::bitvector::word_t j1 = (vals[0] + vals[1]*i1);
                for (unsigned i1b = 0; i1b < vals[3]; ++ i1b) {
                    for (unsigned i2 = 0; i2 < vals[6]; ++ i2) {
                        ibis::bitvector::word_t j2 = (j1 + i1b) * cub[1]
                            + vals[4] + i2*vals[5];
                        bv.adjustSize(0, j2);
                        bv.appendFill(1, vals[7]);
                    }
                }
            }
        }
        break;}
    case 3: { // 3-D
        if (vals[2] == 0 || vals[3] == 0 ||
            vals[6] == 0 || vals[7] == 0 ||
            vals[10] == 0 || vals[11] == 0) break;
        break;}
    default: { // general n-dimensional case
        break;}
    }

    // ensure the output has exactly nmax bits
    bv.adjustSize(0, cub[0]);
} // ibis::hyperslab::tobitvector
