// File: $id$
// Author: John Wu <John.Wu at ACM.org> Lawrence Berkeley National Laboratory
// Copyright (c) 2007-2016 the Regents of the University of California
//
// This file contains the implementation of various merge functions.  Due
// to heavy use of templating, this set of functions may require a
// significant amount of time to compile.
#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(disable:4786)   // some identifier longer than 256 characters
#endif
#include "bord.h"       // ibis::bord

/// Merge the incoming data partition with this one.  This function is
/// intended to combine partial results produced by ibis::bord::groupbya;
/// both this and rhs must be produced with the same select clause sel.  It
/// only work with separable aggregation operators.
///
/// It returns the number of rows in the combined result upon a successful
/// completion, otherwise, it returns a negative number.
int ibis::bord::merge(const ibis::bord &rhs, const ibis::selectClause& sel) {
    int ierr = -1;
    if (columns.size() != rhs.columns.size() ||
        columns.size() != sel.aggSize()) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- bord::merge expects the same number of columns in "
            << this->part::name() << " (" << nColumns() << "), "
            << rhs.part::name() << " (" << rhs.nColumns()
            << ") and the select clauses (" << sel.aggSize() << ")";
        return -1;
    }
    LOGGER(ibis::gVerbose > 2)
        << "bord::merge -- merging " << ibis::table::name() << " (" << nRows()
        << ") with " << rhs.ibis::table::name() << " (" << rhs.nRows() << ')';

    // divide the columns into keys and vals
    std::vector<ibis::bord::column*> keys, keyr, vals, valr;
    std::vector<ibis::selectClause::AGREGADO> agg;
    for (unsigned i = 0; i < sel.aggSize(); ++ i) {
        const char* nm = sel.aggName(i);
        ibis::bord::column *cs =
            dynamic_cast<ibis::bord::column*>(getColumn(nm));
        ibis::bord::column *cr =
            dynamic_cast<ibis::bord::column*>(rhs.getColumn(nm));
        if (cs == 0 || cr == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- bord::merge expects a column named " << nm
                << " from data partition " << this->part::name() << " and "
                << rhs.part::name();
            return -2;
        }
        if (cs->type() != cr->type()) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- bord::merge expects the columns named " << nm
                << " from data partition " << this->part::name() << " and "
                << rhs.part::name() << " to have the same type";
            return -3;
        }
        if (cs->getArray() == 0 || cr->getArray() == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- bord::merge column " << nm
                << " from data partition " << this->part::name() << " and "
                << rhs.part::name() << " must have data in memory";
            return -4;
        }

        ibis::selectClause::AGREGADO a0 = sel.getAggregator(i);
        if (a0 == ibis::selectClause::NIL_AGGR) { // a group-by key
            keys.push_back(cs);
            keyr.push_back(cr);
        }
        else if (a0 == ibis::selectClause::CNT ||
                 a0 == ibis::selectClause::SUM ||
                 a0 == ibis::selectClause::MAX ||
                 a0 == ibis::selectClause::MIN) { // a separable operator
            agg.push_back(a0);
            vals.push_back(cs);
            valr.push_back(cr);
        }
        else { // can not deal with this operator in this function
            return -5;
        }
    }
    if (keys.size() != keyr.size() || vals.size() != valr.size())
        return -2;
    if (ibis::gVerbose > 3) {
        ibis::util::logger lg;
        lg() << "bord::merge -- merging " << this->part::name() << " and "
             << rhs.part::name() << " into " << this->part::name() << " using ";
        if (keys.size() == 0) {
            lg() << "no keys";
        }
        else {
            lg() << '(' << keys[0]->name();
            for (unsigned j = 1; j < keys.size(); ++ j)
                lg() << ", " << keys[j]->name();
            lg() << ") as key" << (keys.size()>1 ? "s" : "");
        }
        if (ibis::gVerbose > 8) {
            const uint64_t nprt = (ibis::gVerbose>40 ? 1000000 :
                                   (1U << (ibis::gVerbose/2)));
            lg() << "\nthis partition:\n";
            dump(lg(), nprt, ", ");
            lg() << "other partition:\n";
            rhs.dump(lg(), nprt, ", ");
        }
    }

    bool match = (this->part::nRows() == rhs.part::nRows());
    for (uint32_t jc = 0; match && jc < keys.size(); ++ jc) {
        match = keys[jc]->equal_to(*keyr[jc]);
    }

    if (match) { // all the keys match, work on the columns one at a time
        ierr = merge0(vals, valr, agg);
    }
    else {
        if (keys.size() == 1) {
            if (vals.size() == 1)
                ierr = merge11(*keys[0], *vals[0], *keyr[0], *valr[0], agg[0]);
            else if (vals.size() == 2)
                ierr = merge12(*keys[0], *vals[0], *vals[1],
                               *keyr[0], *valr[0], *valr[1],
                               agg[0], agg[1]);
            else
                ierr = merge10(*keys[0], vals, *keyr[0], valr, agg);
        }
        else if (keys.size() == 2) {
            if (vals.size() == 1) {
                ierr = merge21(*keys[0], *keys[1], *vals[0],
                               *keyr[0], *keyr[1], *valr[0], agg[0]);
            }
            else {
                ierr = merge20(*keys[0], *keys[1], vals,
                               *keyr[0], *keyr[1], valr, agg);
            }
        }
        else { // a generic version
            ierr = merger(keys, vals, keyr, valr, agg);
        }

        // update the number of rows
        if (ierr > 0)
            nEvents = ierr;
        else
            nEvents = 0;
    }

    if (ibis::gVerbose > 2) {
        ibis::util::logger lg;
        lg() << "bord[" << ibis::table::name() << "]::merge completed " << nRows()
             << " rows (memory cache used: "
             << ibis::util::groupby1000(ibis::fileManager::bytesInUse()) << ")";
        if (ibis::gVerbose > 4) {
            lg() << "\n";
            describe(lg());
        }
        if (ibis::gVerbose > 6) {
            uint64_t nprt = ((nEvents>>ibis::gVerbose)>1 ? nEvents :
                             (1ULL<<ibis::gVerbose));
            lg() << "\n";
            dump(lg(), nprt, ", ");
        }
    }
    return ierr;
} // ibis::bord::merge

/// Merge values from two partial results and place the final resules in
/// the first argument.  This is the most generic version that expects the
/// keys to not match and therefore needs to produce a new set of values.
/// It also uses the generic algorithm for comparisons, where each
/// comparison of a pair of values requires a function call.
int ibis::bord::merger(std::vector<ibis::bord::column*> &keys,
                       std::vector<ibis::bord::column*> &vals,
                       const std::vector<ibis::bord::column*> &keyr,
                       const std::vector<ibis::bord::column*> &valr,
                       const std::vector<selectClause::AGREGADO> &agg) {
    // number of columns must match, their types must match
    if (keys.size() != keyr.size() || vals.size() != valr.size() ||
        vals.size() != agg.size())
        return -1;
    for (unsigned j = 0; j < keyr.size(); ++ j) {
        if (keys[j]->type() != keyr[j]->type() ||
            keys[j]->getArray() == 0 || keyr[j]->getArray() == 0)
            return -2;
    }
    for (unsigned j = 0; j < agg.size(); ++ j) {
        if (vals[j]->type() != valr[j]->type() ||
            vals[j]->getArray() == 0 || valr[j]->getArray() == 0)
            return -3;
        if (agg[j] != ibis::selectClause::CNT &&
            agg[j] != ibis::selectClause::SUM &&
            agg[j] != ibis::selectClause::MIN &&
            agg[j] != ibis::selectClause::MAX)
            return -4;
    }

    // make a copy of keys and vals as keyt and valt
    std::vector<ibis::bord::column*> keyt, valt;
    IBIS_BLOCK_GUARD(ibis::util::clearVec<ibis::bord::column>,
                     ibis::util::ref(keyt));
    IBIS_BLOCK_GUARD(ibis::util::clearVec<ibis::bord::column>,
                     ibis::util::ref(valt));
    for (unsigned j = 0; j < keys.size(); ++ j) {
        keyt.push_back(new ibis::bord::column(*keys[j]));
        keys[j]->limit(0);
    }
    for (unsigned j = 0; j < vals.size(); ++ j) {
        valt.push_back(new ibis::bord::column(*vals[j]));
        vals[j]->limit(0);
    }

    int ierr = 0;
    uint32_t ir = 0, it = 0;
    const uint32_t nk = keyr.size();
    const uint32_t nv = valr.size();
    const uint32_t nr = keyr[0]->partition()->nRows();
    const uint32_t nt = keyt[0]->partition()->nRows();
    while (ir < nr && it < nt) {
        bool match = true;
        uint32_t j0 = 0;
        while (match && j0 < nk) {
            if (keyt[j0]->equal_to(*keyr[j0], it, ir))
                j0 += 1;
            else
                match = false;
        }
        if (match) {
            for (unsigned j1 = 0; j1 < nk; ++ j1)
                keys[j1]->append(keyt[j1]->getArray(), it);
            for (unsigned j1 = 0; j1 < nv; ++ j1)
                vals[j1]->append(valt[j1]->getArray(), it,
                                 valr[j1]->getArray(), ir, agg[j1]);
            ++ it;
            ++ ir;
        }
        else if (keyt[j0]->less_than(*keyr[j0], it, ir)) {
            for (unsigned j1 = 0; j1 < nk; ++ j1)
                keys[j1]->append(keyt[j1]->getArray(), it);
            for (unsigned j1 = 0; j1 < nv; ++ j1)
                vals[j1]->append(valt[j1]->getArray(), it);
            ++ it;
        }
        else {
            for (unsigned j1 = 0; j1 < nk; ++ j1)
                keys[j1]->append(keyr[j1]->getArray(), ir);
            for (unsigned j1 = 0; j1 < nv; ++ j1)
                vals[j1]->append(valr[j1]->getArray(), ir);
            ++ ir;
        }
        ++ ierr;
    }

    while (ir < nr) {
        for (unsigned j1 = 0; j1 < nk; ++ j1)
            keys[j1]->append(keyr[j1]->getArray(), ir);
        for (unsigned j1 = 0; j1 < nv; ++ j1)
            vals[j1]->append(valr[j1]->getArray(), ir);
        ++ ierr;
        ++ ir;
    }
    while (it < nt) {
        for (unsigned j1 = 0; j1 < nk; ++ j1)
            keys[j1]->append(keyt[j1]->getArray(), it);
        for (unsigned j1 = 0; j1 < nv; ++ j1)
            vals[j1]->append(valt[j1]->getArray(), it);
        ++ ierr;
        ++ it;
    }
    return ierr;
} // ibis::bord::merger


/// Merge values according to the given operators.  The corresponding
/// group-by keys match, only the values needs to be updated.
int ibis::bord::merge0(std::vector<ibis::bord::column*> &vals,
                       const std::vector<ibis::bord::column*> &valr,
                       const std::vector<selectClause::AGREGADO>& agg) {
    if (vals.size() != valr.size() || vals.size() != agg.size())
        return -6;

    int ierr = 0;
    for (uint32_t jc = 0; jc < agg.size(); ++ jc) {
        if (vals[jc] == 0 || valr[jc] == 0)
            return -1;
        if (vals[jc]->getArray() == 0 || valr[jc]->getArray() == 0)
            return -2;
        if (vals[jc]->type() != valr[jc]->type())
            return -3;

        switch (vals[jc]->type()) {
        case ibis::BYTE:
            ierr = merge0T(*static_cast<array_t<signed char>*>
                           (vals[jc]->getArray()),
                           *static_cast<array_t<signed char>*>
                           (valr[jc]->getArray()),
                           agg[jc]);
            break;
        case ibis::UBYTE:
            ierr = merge0T(*static_cast<array_t<unsigned char>*>
                           (vals[jc]->getArray()),
                           *static_cast<array_t<unsigned char>*>
                           (valr[jc]->getArray()),
                           agg[jc]);
            break;
        case ibis::SHORT:
            ierr = merge0T(*static_cast<array_t<int16_t>*>
                           (vals[jc]->getArray()),
                           *static_cast<array_t<int16_t>*>
                           (valr[jc]->getArray()),
                           agg[jc]);
            break;
        case ibis::USHORT:
            ierr = merge0T(*static_cast<array_t<uint16_t>*>
                           (vals[jc]->getArray()),
                           *static_cast<array_t<uint16_t>*>
                           (valr[jc]->getArray()),
                           agg[jc]);
            break;
        case ibis::INT:
            ierr = merge0T(*static_cast<array_t<int32_t>*>
                           (vals[jc]->getArray()),
                           *static_cast<array_t<int32_t>*>
                           (valr[jc]->getArray()),
                           agg[jc]);
            break;
        case ibis::UINT:
            ierr = merge0T(*static_cast<array_t<uint32_t>*>
                           (vals[jc]->getArray()),
                           *static_cast<array_t<uint32_t>*>
                           (valr[jc]->getArray()),
                           agg[jc]);
            break;
        case ibis::LONG:
            ierr = merge0T(*static_cast<array_t<int64_t>*>
                           (vals[jc]->getArray()),
                           *static_cast<array_t<int64_t>*>
                           (valr[jc]->getArray()),
                           agg[jc]);
            break;
        case ibis::ULONG:
            ierr = merge0T(*static_cast<array_t<uint64_t>*>
                           (vals[jc]->getArray()),
                           *static_cast<array_t<uint64_t>*>
                           (valr[jc]->getArray()),
                           agg[jc]);
            break;
        case ibis::FLOAT:
            ierr = merge0T(*static_cast<array_t<float>*>
                           (vals[jc]->getArray()),
                           *static_cast<array_t<float>*>
                           (valr[jc]->getArray()),
                           agg[jc]);
            break;
        case ibis::DOUBLE:
            ierr = merge0T(*static_cast<array_t<double>*>
                           (vals[jc]->getArray()),
                           *static_cast<array_t<double>*>
                           (valr[jc]->getArray()),
                           agg[jc]);
            break;
        default:
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bord::merge0 can not deal with vals[" << jc
                << "] (" << vals[jc]->name() << ") of type "
                << ibis::TYPESTRING[(int)vals[jc]->type()];
            ierr = -5;
            break;
        }
    }
    return ierr;
} // ibis::bord::merge0

/// Template function to perform the merger operations on arrays with
/// matching keys.
template <typename T> int
ibis::bord::merge0T(ibis::array_t<T>& vs, const ibis::array_t<T>& vr,
                    ibis::selectClause::AGREGADO ag) {
    if (vs.size() != vr.size()) return -11;
    switch (ag) {
    default:
        return -12;
    case ibis::selectClause::CNT:
    case ibis::selectClause::SUM:
        for (size_t j = 0; j < vr.size(); ++ j)
            vs[j] += vr[j];
        break;
    case ibis::selectClause::MAX:
        for (size_t j = 0; j < vr.size(); ++ j)
            if (vs[j] < vr[j])
                vs[j] = vr[j];
        break;
    case ibis::selectClause::MIN:
        for (size_t j = 0; j < vr.size(); ++ j)
            if (vs[j] > vr[j])
                vs[j] = vr[j];
        break;
    }
    return vs.size();
} // ibis::bord::merge0T

/// Merge with one key column and an arbitrary number of value columns.
int ibis::bord::merge10(ibis::bord::column &k1,
                        std::vector<ibis::bord::column*> &v1,
                        const ibis::bord::column &k2,
                        const std::vector<ibis::bord::column*> &v2,
                        const std::vector<ibis::selectClause::AGREGADO> &agg) {
    int ierr = -1;
    if (k1.type() != k2.type())
        return ierr;
    if (v1.size() != v2.size() || v1.size() != agg.size())
        return ierr;

    std::vector<ibis::bord::column*> av1(v1.size());
    IBIS_BLOCK_GUARD(ibis::util::clearVec<ibis::bord::column>,
                     ibis::util::ref(av1));
    for (unsigned j = 0; j < v1.size(); ++ j)
        av1[j] = new ibis::bord::column(*v1[j]);

    switch (k1.type()) {
    default:
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bord::merge10 can not deal with k1 ("
            << k1.name() << ") of type "
            << ibis::TYPESTRING[(int)k1.type()];
        return -6;
    case ibis::BYTE: {
        ibis::array_t<signed char> &ak0 =
            * static_cast<ibis::array_t<signed char>*>(k1.getArray());
        const ibis::array_t<signed char> &ak2 =
            * static_cast<const ibis::array_t<signed char>*>(k2.getArray());
        const ibis::array_t<signed char> ak1(ak0);
        ak0.nosharing();
        ierr = merge10T(ak0, v1, ak1, av1, ak2, v2, agg);
        break;}
    case ibis::UBYTE: {
        ibis::array_t<unsigned char> &ak0 =
            * static_cast<ibis::array_t<unsigned char>*>(k1.getArray());
        const ibis::array_t<unsigned char> &ak2 =
            * static_cast<const ibis::array_t<unsigned char>*>(k2.getArray());
        const ibis::array_t<unsigned char> ak1(ak0);
        ak0.nosharing();
        ierr = merge10T(ak0, v1, ak1, av1, ak2, v2, agg);
        break;}
    case ibis::SHORT: {
        ibis::array_t<int16_t> &ak0 =
            * static_cast<ibis::array_t<int16_t>*>(k1.getArray());
        const ibis::array_t<int16_t> &ak2 =
            * static_cast<const ibis::array_t<int16_t>*>(k2.getArray());
        const ibis::array_t<int16_t> ak1(ak0);
        ak0.nosharing();
        ierr = merge10T(ak0, v1, ak1, av1, ak2, v2, agg);
        break;}
    case ibis::USHORT: {
        ibis::array_t<uint16_t> &ak0 =
            * static_cast<ibis::array_t<uint16_t>*>(k1.getArray());
        const ibis::array_t<uint16_t> &ak2 =
            * static_cast<const ibis::array_t<uint16_t>*>(k2.getArray());
        const ibis::array_t<uint16_t> ak1(ak0);
        ak0.nosharing();
        ierr = merge10T(ak0, v1, ak1, av1, ak2, v2, agg);
        break;}
    case ibis::INT: {
        ibis::array_t<int32_t> &ak0 =
            * static_cast<ibis::array_t<int32_t>*>(k1.getArray());
        const ibis::array_t<int32_t> &ak2 =
            * static_cast<const ibis::array_t<int32_t>*>(k2.getArray());
        const ibis::array_t<int32_t> ak1(ak0);
        ak0.nosharing();
        ierr = merge10T(ak0, v1, ak1, av1, ak2, v2, agg);
        break;}
    case ibis::UINT: {
        ibis::array_t<uint32_t> &ak0 =
            * static_cast<ibis::array_t<uint32_t>*>(k1.getArray());
        const ibis::array_t<uint32_t> &ak2 =
            * static_cast<const ibis::array_t<uint32_t>*>(k2.getArray());
        const ibis::array_t<uint32_t> ak1(ak0);
        ak0.nosharing();
        ierr = merge10T(ak0, v1, ak1, av1, ak2, v2, agg);
        break;}
    case ibis::LONG: {
        ibis::array_t<int64_t> &ak0 =
            * static_cast<ibis::array_t<int64_t>*>(k1.getArray());
        const ibis::array_t<int64_t> &ak2 =
            * static_cast<const ibis::array_t<int64_t>*>(k2.getArray());
        const ibis::array_t<int64_t> ak1(ak0);
        ak0.nosharing();
        ierr = merge10T(ak0, v1, ak1, av1, ak2, v2, agg);
        break;}
    case ibis::ULONG: {
        ibis::array_t<uint64_t> &ak0 =
            * static_cast<ibis::array_t<uint64_t>*>(k1.getArray());
        const ibis::array_t<uint64_t> &ak2 =
            * static_cast<const ibis::array_t<uint64_t>*>(k2.getArray());
        const ibis::array_t<uint64_t> ak1(ak0);
        ak0.nosharing();
        ierr = merge10T(ak0, v1, ak1, av1, ak2, v2, agg);
        break;}
    case ibis::FLOAT: {
        ibis::array_t<float> &ak0 =
            * static_cast<ibis::array_t<float>*>(k1.getArray());
        const ibis::array_t<float> &ak2 =
            * static_cast<const ibis::array_t<float>*>(k2.getArray());
        const ibis::array_t<float> ak1(ak0);
        ak0.nosharing();
        ierr = merge10T(ak0, v1, ak1, av1, ak2, v2, agg);
        break;}
    case ibis::DOUBLE: {
        ibis::array_t<double> &ak0 =
            * static_cast<ibis::array_t<double>*>(k1.getArray());
        const ibis::array_t<double> &ak2 =
            * static_cast<const ibis::array_t<double>*>(k2.getArray());
        const ibis::array_t<double> ak1(ak0);
        ak0.nosharing();
        ierr = merge10T(ak0, v1, ak1, av1, ak2, v2, agg);
        break;}
    case ibis::CATEGORY:
    case ibis::TEXT: {
        std::vector<std::string> &ak0 =
            * static_cast<std::vector<std::string>*>(k1.getArray());
        const std::vector<std::string> &ak2 =
            * static_cast<const std::vector<std::string>*>(k2.getArray());
        const std::vector<std::string> ak1(ak0);
        ierr = merge10S(ak0, v1, ak1, av1, ak2, v2, agg);
        break;}
    }
    return ierr;
} // ibis::bord::merge10

/// Perform merge operation with one key column and an arbitrary number of
/// value columns.
template <typename Tk> int
ibis::bord::merge10T(ibis::array_t<Tk> &kout,
                     std::vector<ibis::bord::column*> &vout,
                     const ibis::array_t<Tk> &kin1,
                     const std::vector<ibis::bord::column*> &vin1,
                     const ibis::array_t<Tk> &kin2,
                     const std::vector<ibis::bord::column*> &vin2,
                     const std::vector<ibis::selectClause::AGREGADO> &agg) {
    int ierr = -1;
    kout.clear();
    for (size_t j = 0; j < vout.size(); ++ j)
        vout[j]->limit(0);
    if (vout.size() != vin1.size() || vout.size() != vin2.size() ||
        vout.size() != agg.size())
        return ierr;

    uint32_t i1 = 0;
    uint32_t i2 = 0;
    while (i1 < kin1.size() && i2 < kin2.size()) {
        if (kin1[i1] == kin2[i2]) {
            kout.push_back(kin1[i1]);
            for (unsigned j = 0; j < vin1.size(); ++ j)
                vout[j]->append(vin1[j]->getArray(), i1,
                                vin2[j]->getArray(), i2, agg[j]);
            ++ i1;
            ++ i2;
        }
        else if (kin1[i1] < kin2[i2]) {
            kout.push_back(kin1[i1]);
            for (unsigned j = 0; j < vin1.size(); ++ j)
                vout[j]->append(vin1[j]->getArray(), i1);
            ++ i1;
        }
        else {
            kout.push_back(kin2[i2]);
            for (unsigned j = 0; j < vin2.size(); ++ j)
                vout[j]->append(vin2[j]->getArray(), i2);
            ++ i2;
        }
    }

    while (i1 < kin1.size()) {
        kout.push_back(kin1[i1]);
        for (unsigned j = 0; j < vin1.size(); ++ j)
            vout[j]->append(vin1[j]->getArray(), i1);
        ++ i1;
    }

    while (i2 < kin2.size()) {
        kout.push_back(kin2[i2]);
        for (unsigned j = 0; j < vin2.size(); ++ j)
            vout[j]->append(vin2[j]->getArray(), i2);
        ++ i2;
    }
    ierr = kout.size();
    return ierr;
} // ibis::bord::merge10T

int
ibis::bord::merge10S(std::vector<std::string> &kout,
                     std::vector<ibis::bord::column*> &vout,
                     const std::vector<std::string> &kin1,
                     const std::vector<ibis::bord::column*> &vin1,
                     const std::vector<std::string> &kin2,
                     const std::vector<ibis::bord::column*> &vin2,
                     const std::vector<ibis::selectClause::AGREGADO> &agg) {
    int ierr = -1;
    kout.clear();
    for (size_t j = 0; j < vout.size(); ++ j)
        vout[j]->limit(0);
    if (vout.size() != vin1.size() || vout.size() != vin2.size() ||
        vout.size() != agg.size())
        return ierr;

    uint32_t i1 = 0;
    uint32_t i2 = 0;
    while (i1 < kin1.size() && i2 < kin2.size()) {
        if (kin1[i1] == kin2[i2]) {
            kout.push_back(kin1[i1]);
            for (unsigned j = 0; j < vin1.size(); ++ j)
                vout[j]->append(vin1[j]->getArray(), i1,
                                vin2[j]->getArray(), i2, agg[j]);
            ++ i1;
            ++ i2;
        }
        else if (kin1[i1] < kin2[i2]) {
            kout.push_back(kin1[i1]);
            for (unsigned j = 0; j < vin1.size(); ++ j)
                vout[j]->append(vin1[j]->getArray(), i1);
            ++ i1;
        }
        else {
            kout.push_back(kin2[i2]);
            for (unsigned j = 0; j < vin2.size(); ++ j)
                vout[j]->append(vin2[j]->getArray(), i2);
            ++ i2;
        }
    }

    while (i1 < kin1.size()) {
        kout.push_back(kin1[i1]);
        for (unsigned j = 0; j < vin1.size(); ++ j)
            vout[j]->append(vin1[j]->getArray(), i1);
        ++ i1;
    }

    while (i2 < kin2.size()) {
        kout.push_back(kin2[i2]);
        for (unsigned j = 0; j < vin2.size(); ++ j)
            vout[j]->append(vin2[j]->getArray(), i2);
        ++ i2;
    }
    ierr = kout.size();
    return ierr;
} // ibis::bord::merge10S

/// Function to merge one column as key and one column as value.
int ibis::bord::merge11(ibis::bord::column &k1,
                        ibis::bord::column &v1,
                        const ibis::bord::column &k2,
                        const ibis::bord::column &v2,
                        ibis::selectClause::AGREGADO agg) {
    if (k1.type() != k2.type() || v1.type() != v2.type()) {
        LOGGER(ibis::gVerbose > 2)
            << "Warning -- bord::merge11 expects the same types and sizes "
            "for the keys and values";
        return -1;
    }

    int ierr = -1;
    switch (k1.type()) {
    default:
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bord::merge11 can not deal with k1 ("
            << k1.name() << ") of type "
            << ibis::TYPESTRING[(int)k1.type()];
        return -2;
    case ibis::BYTE: {
        const ibis::array_t<signed char> &ak2 =
            *static_cast<const ibis::array_t<signed char>*>(k2.getArray());
        ibis::array_t<signed char> &ak0 =
            *static_cast<ibis::array_t<signed char>*>(k1.getArray());
        const ibis::array_t<signed char> ak1(ak0);
        ak0.nosharing();
        switch (v1.type()) {
        default:
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bord::merge11 can not deal with v1 ("
                << v1.name() << ") of type "
                << ibis::TYPESTRING[(int)v1.type()];
            return -3;
        case ibis::BYTE: {
            const ibis::array_t<signed char> &av2 =
                *static_cast<const ibis::array_t<signed char>*>(v2.getArray());
            ibis::array_t<signed char> &av0 =
                *static_cast<ibis::array_t<signed char>*>(v1.getArray());
            const ibis::array_t<signed char> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::UBYTE: {
            const ibis::array_t<unsigned char> &av2 =
                *static_cast<const ibis::array_t<unsigned char>*>(v2.getArray());
            ibis::array_t<unsigned char> &av0 =
                *static_cast<ibis::array_t<unsigned char>*>(v1.getArray());
            const ibis::array_t<unsigned char> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::SHORT: {
            const ibis::array_t<int16_t> &av2 =
                *static_cast<const ibis::array_t<int16_t>*>(v2.getArray());
            ibis::array_t<int16_t> &av0 =
                *static_cast<ibis::array_t<int16_t>*>(v1.getArray());
            const ibis::array_t<int16_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::USHORT: {
            const ibis::array_t<uint16_t> &av2 =
                *static_cast<const ibis::array_t<uint16_t>*>(v2.getArray());
            ibis::array_t<uint16_t> &av0 =
                *static_cast<ibis::array_t<uint16_t>*>(v1.getArray());
            const ibis::array_t<uint16_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::INT: {
            const ibis::array_t<int32_t> &av2 =
                *static_cast<const ibis::array_t<int32_t>*>(v2.getArray());
            ibis::array_t<int32_t> &av0 =
                *static_cast<ibis::array_t<int32_t>*>(v1.getArray());
            const ibis::array_t<int32_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::UINT: {
            const ibis::array_t<uint32_t> &av2 =
                *static_cast<const ibis::array_t<uint32_t>*>(v2.getArray());
            ibis::array_t<uint32_t> &av0 =
                *static_cast<ibis::array_t<uint32_t>*>(v1.getArray());
            const ibis::array_t<uint32_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::LONG: {
            const ibis::array_t<int64_t> &av2 =
                *static_cast<const ibis::array_t<int64_t>*>(v2.getArray());
            ibis::array_t<int64_t> &av0 =
                *static_cast<ibis::array_t<int64_t>*>(v1.getArray());
            const ibis::array_t<int64_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::ULONG: {
            const ibis::array_t<uint64_t> &av2 =
                *static_cast<const ibis::array_t<uint64_t>*>(v2.getArray());
            ibis::array_t<uint64_t> &av0 =
                *static_cast<ibis::array_t<uint64_t>*>(v1.getArray());
            const ibis::array_t<uint64_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::FLOAT: {
            const ibis::array_t<float> &av2 =
                *static_cast<const ibis::array_t<float>*>(v2.getArray());
            ibis::array_t<float> &av0 =
                *static_cast<ibis::array_t<float>*>(v1.getArray());
            const ibis::array_t<float> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::DOUBLE: {
            const ibis::array_t<double> &av2 =
                *static_cast<const ibis::array_t<double>*>(v2.getArray());
            ibis::array_t<double> &av0 =
                *static_cast<ibis::array_t<double>*>(v1.getArray());
            const ibis::array_t<double> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        } // v1.type()
        break;}
    case ibis::UBYTE: {
        const ibis::array_t<unsigned char> &ak2 =
            *static_cast<const ibis::array_t<unsigned char>*>(k2.getArray());
        ibis::array_t<unsigned char> &ak0 =
            *static_cast<ibis::array_t<unsigned char>*>(k1.getArray());
        const ibis::array_t<unsigned char> ak1(ak0);
        ak0.nosharing();
        switch (v1.type()) {
        default:
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bord::merge11 can not deal with v1 ("
                << v1.name() << ") of type "
                << ibis::TYPESTRING[(int)v1.type()];
            return -3;
        case ibis::BYTE: {
            const ibis::array_t<signed char> &av2 =
                *static_cast<const ibis::array_t<signed char>*>(v2.getArray());
            ibis::array_t<signed char> &av0 =
                *static_cast<ibis::array_t<signed char>*>(v1.getArray());
            const ibis::array_t<signed char> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::UBYTE: {
            const ibis::array_t<unsigned char> &av2 =
                *static_cast<const ibis::array_t<unsigned char>*>(v2.getArray());
            ibis::array_t<unsigned char> &av0 =
                *static_cast<ibis::array_t<unsigned char>*>(v1.getArray());
            const ibis::array_t<unsigned char> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::SHORT: {
            const ibis::array_t<int16_t> &av2 =
                *static_cast<const ibis::array_t<int16_t>*>(v2.getArray());
            ibis::array_t<int16_t> &av0 =
                *static_cast<ibis::array_t<int16_t>*>(v1.getArray());
            const ibis::array_t<int16_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::USHORT: {
            const ibis::array_t<uint16_t> &av2 =
                *static_cast<const ibis::array_t<uint16_t>*>(v2.getArray());
            ibis::array_t<uint16_t> &av0 =
                *static_cast<ibis::array_t<uint16_t>*>(v1.getArray());
            const ibis::array_t<uint16_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::INT: {
            const ibis::array_t<int32_t> &av2 =
                *static_cast<const ibis::array_t<int32_t>*>(v2.getArray());
            ibis::array_t<int32_t> &av0 =
                *static_cast<ibis::array_t<int32_t>*>(v1.getArray());
            const ibis::array_t<int32_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::UINT: {
            const ibis::array_t<uint32_t> &av2 =
                *static_cast<const ibis::array_t<uint32_t>*>(v2.getArray());
            ibis::array_t<uint32_t> &av0 =
                *static_cast<ibis::array_t<uint32_t>*>(v1.getArray());
            const ibis::array_t<uint32_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::LONG: {
            const ibis::array_t<int64_t> &av2 =
                *static_cast<const ibis::array_t<int64_t>*>(v2.getArray());
            ibis::array_t<int64_t> &av0 =
                *static_cast<ibis::array_t<int64_t>*>(v1.getArray());
            const ibis::array_t<int64_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::ULONG: {
            const ibis::array_t<uint64_t> &av2 =
                *static_cast<const ibis::array_t<uint64_t>*>(v2.getArray());
            ibis::array_t<uint64_t> &av0 =
                *static_cast<ibis::array_t<uint64_t>*>(v1.getArray());
            const ibis::array_t<uint64_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::FLOAT: {
            const ibis::array_t<float> &av2 =
                *static_cast<const ibis::array_t<float>*>(v2.getArray());
            ibis::array_t<float> &av0 =
                *static_cast<ibis::array_t<float>*>(v1.getArray());
            const ibis::array_t<float> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::DOUBLE: {
            const ibis::array_t<double> &av2 =
                *static_cast<const ibis::array_t<double>*>(v2.getArray());
            ibis::array_t<double> &av0 =
                *static_cast<ibis::array_t<double>*>(v1.getArray());
            const ibis::array_t<double> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        } // v1.type()
        break;}
    case ibis::SHORT: {
        const ibis::array_t<int16_t> &ak2 =
            *static_cast<const ibis::array_t<int16_t>*>(k2.getArray());
        ibis::array_t<int16_t> &ak0 =
            *static_cast<ibis::array_t<int16_t>*>(k1.getArray());
        const ibis::array_t<int16_t> ak1(ak0);
        ak0.nosharing();
        switch (v1.type()) {
        default:
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bord::merge11 can not deal with v1 ("
                << v1.name() << ") of type "
                << ibis::TYPESTRING[(int)v1.type()];
            return -3;
        case ibis::BYTE: {
            const ibis::array_t<signed char> &av2 =
                *static_cast<const ibis::array_t<signed char>*>(v2.getArray());
            ibis::array_t<signed char> &av0 =
                *static_cast<ibis::array_t<signed char>*>(v1.getArray());
            const ibis::array_t<signed char> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::UBYTE: {
            const ibis::array_t<unsigned char> &av2 =
                *static_cast<const ibis::array_t<unsigned char>*>(v2.getArray());
            ibis::array_t<unsigned char> &av0 =
                *static_cast<ibis::array_t<unsigned char>*>(v1.getArray());
            const ibis::array_t<unsigned char> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::SHORT: {
            const ibis::array_t<int16_t> &av2 =
                *static_cast<const ibis::array_t<int16_t>*>(v2.getArray());
            ibis::array_t<int16_t> &av0 =
                *static_cast<ibis::array_t<int16_t>*>(v1.getArray());
            const ibis::array_t<int16_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::USHORT: {
            const ibis::array_t<uint16_t> &av2 =
                *static_cast<const ibis::array_t<uint16_t>*>(v2.getArray());
            ibis::array_t<uint16_t> &av0 =
                *static_cast<ibis::array_t<uint16_t>*>(v1.getArray());
            const ibis::array_t<uint16_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::INT: {
            const ibis::array_t<int32_t> &av2 =
                *static_cast<const ibis::array_t<int32_t>*>(v2.getArray());
            ibis::array_t<int32_t> &av0 =
                *static_cast<ibis::array_t<int32_t>*>(v1.getArray());
            const ibis::array_t<int32_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::UINT: {
            const ibis::array_t<uint32_t> &av2 =
                *static_cast<const ibis::array_t<uint32_t>*>(v2.getArray());
            ibis::array_t<uint32_t> &av0 =
                *static_cast<ibis::array_t<uint32_t>*>(v1.getArray());
            const ibis::array_t<uint32_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::LONG: {
            const ibis::array_t<int64_t> &av2 =
                *static_cast<const ibis::array_t<int64_t>*>(v2.getArray());
            ibis::array_t<int64_t> &av0 =
                *static_cast<ibis::array_t<int64_t>*>(v1.getArray());
            const ibis::array_t<int64_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::ULONG: {
            const ibis::array_t<uint64_t> &av2 =
                *static_cast<const ibis::array_t<uint64_t>*>(v2.getArray());
            ibis::array_t<uint64_t> &av0 =
                *static_cast<ibis::array_t<uint64_t>*>(v1.getArray());
            const ibis::array_t<uint64_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::FLOAT: {
            const ibis::array_t<float> &av2 =
                *static_cast<const ibis::array_t<float>*>(v2.getArray());
            ibis::array_t<float> &av0 =
                *static_cast<ibis::array_t<float>*>(v1.getArray());
            const ibis::array_t<float> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::DOUBLE: {
            const ibis::array_t<double> &av2 =
                *static_cast<const ibis::array_t<double>*>(v2.getArray());
            ibis::array_t<double> &av0 =
                *static_cast<ibis::array_t<double>*>(v1.getArray());
            const ibis::array_t<double> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        } // v1.type()
        break;}
    case ibis::USHORT: {
        const ibis::array_t<uint16_t> &ak2 =
            *static_cast<const ibis::array_t<uint16_t>*>(k2.getArray());
        ibis::array_t<uint16_t> &ak0 =
            *static_cast<ibis::array_t<uint16_t>*>(k1.getArray());
        const ibis::array_t<uint16_t> ak1(ak0);
        ak0.nosharing();
        switch (v1.type()) {
        default:
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bord::merge11 can not deal with v1 ("
                << v1.name() << ") of type "
                << ibis::TYPESTRING[(int)v1.type()];
            return -3;
        case ibis::BYTE: {
            const ibis::array_t<signed char> &av2 =
                *static_cast<const ibis::array_t<signed char>*>(v2.getArray());
            ibis::array_t<signed char> &av0 =
                *static_cast<ibis::array_t<signed char>*>(v1.getArray());
            const ibis::array_t<signed char> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::UBYTE: {
            const ibis::array_t<unsigned char> &av2 =
                *static_cast<const ibis::array_t<unsigned char>*>(v2.getArray());
            ibis::array_t<unsigned char> &av0 =
                *static_cast<ibis::array_t<unsigned char>*>(v1.getArray());
            const ibis::array_t<unsigned char> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::SHORT: {
            const ibis::array_t<int16_t> &av2 =
                *static_cast<const ibis::array_t<int16_t>*>(v2.getArray());
            ibis::array_t<int16_t> &av0 =
                *static_cast<ibis::array_t<int16_t>*>(v1.getArray());
            const ibis::array_t<int16_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::USHORT: {
            const ibis::array_t<uint16_t> &av2 =
                *static_cast<const ibis::array_t<uint16_t>*>(v2.getArray());
            ibis::array_t<uint16_t> &av0 =
                *static_cast<ibis::array_t<uint16_t>*>(v1.getArray());
            const ibis::array_t<uint16_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::INT: {
            const ibis::array_t<int32_t> &av2 =
                *static_cast<const ibis::array_t<int32_t>*>(v2.getArray());
            ibis::array_t<int32_t> &av0 =
                *static_cast<ibis::array_t<int32_t>*>(v1.getArray());
            const ibis::array_t<int32_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::UINT: {
            const ibis::array_t<uint32_t> &av2 =
                *static_cast<const ibis::array_t<uint32_t>*>(v2.getArray());
            ibis::array_t<uint32_t> &av0 =
                *static_cast<ibis::array_t<uint32_t>*>(v1.getArray());
            const ibis::array_t<uint32_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::LONG: {
            const ibis::array_t<int64_t> &av2 =
                *static_cast<const ibis::array_t<int64_t>*>(v2.getArray());
            ibis::array_t<int64_t> &av0 =
                *static_cast<ibis::array_t<int64_t>*>(v1.getArray());
            const ibis::array_t<int64_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::ULONG: {
            const ibis::array_t<uint64_t> &av2 =
                *static_cast<const ibis::array_t<uint64_t>*>(v2.getArray());
            ibis::array_t<uint64_t> &av0 =
                *static_cast<ibis::array_t<uint64_t>*>(v1.getArray());
            const ibis::array_t<uint64_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::FLOAT: {
            const ibis::array_t<float> &av2 =
                *static_cast<const ibis::array_t<float>*>(v2.getArray());
            ibis::array_t<float> &av0 =
                *static_cast<ibis::array_t<float>*>(v1.getArray());
            const ibis::array_t<float> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::DOUBLE: {
            const ibis::array_t<double> &av2 =
                *static_cast<const ibis::array_t<double>*>(v2.getArray());
            ibis::array_t<double> &av0 =
                *static_cast<ibis::array_t<double>*>(v1.getArray());
            const ibis::array_t<double> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        } // v1.type()
        break;}
    case ibis::INT: {
        const ibis::array_t<int32_t> &ak2 =
            *static_cast<const ibis::array_t<int32_t>*>(k2.getArray());
        ibis::array_t<int32_t> &ak0 =
            *static_cast<ibis::array_t<int32_t>*>(k1.getArray());
        const ibis::array_t<int32_t> ak1(ak0);
        ak0.nosharing();
        switch (v1.type()) {
        default:
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bord::merge11 can not deal with v1 ("
                << v1.name() << ") of type "
                << ibis::TYPESTRING[(int)v1.type()];
            return -3;
        case ibis::BYTE: {
            const ibis::array_t<signed char> &av2 =
                *static_cast<const ibis::array_t<signed char>*>(v2.getArray());
            ibis::array_t<signed char> &av0 =
                *static_cast<ibis::array_t<signed char>*>(v1.getArray());
            const ibis::array_t<signed char> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::UBYTE: {
            const ibis::array_t<unsigned char> &av2 =
                *static_cast<const ibis::array_t<unsigned char>*>(v2.getArray());
            ibis::array_t<unsigned char> &av0 =
                *static_cast<ibis::array_t<unsigned char>*>(v1.getArray());
            const ibis::array_t<unsigned char> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::SHORT: {
            const ibis::array_t<int16_t> &av2 =
                *static_cast<const ibis::array_t<int16_t>*>(v2.getArray());
            ibis::array_t<int16_t> &av0 =
                *static_cast<ibis::array_t<int16_t>*>(v1.getArray());
            const ibis::array_t<int16_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::USHORT: {
            const ibis::array_t<uint16_t> &av2 =
                *static_cast<const ibis::array_t<uint16_t>*>(v2.getArray());
            ibis::array_t<uint16_t> &av0 =
                *static_cast<ibis::array_t<uint16_t>*>(v1.getArray());
            const ibis::array_t<uint16_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::INT: {
            const ibis::array_t<int32_t> &av2 =
                *static_cast<const ibis::array_t<int32_t>*>(v2.getArray());
            ibis::array_t<int32_t> &av0 =
                *static_cast<ibis::array_t<int32_t>*>(v1.getArray());
            const ibis::array_t<int32_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::UINT: {
            const ibis::array_t<uint32_t> &av2 =
                *static_cast<const ibis::array_t<uint32_t>*>(v2.getArray());
            ibis::array_t<uint32_t> &av0 =
                *static_cast<ibis::array_t<uint32_t>*>(v1.getArray());
            const ibis::array_t<uint32_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::LONG: {
            const ibis::array_t<int64_t> &av2 =
                *static_cast<const ibis::array_t<int64_t>*>(v2.getArray());
            ibis::array_t<int64_t> &av0 =
                *static_cast<ibis::array_t<int64_t>*>(v1.getArray());
            const ibis::array_t<int64_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::ULONG: {
            const ibis::array_t<uint64_t> &av2 =
                *static_cast<const ibis::array_t<uint64_t>*>(v2.getArray());
            ibis::array_t<uint64_t> &av0 =
                *static_cast<ibis::array_t<uint64_t>*>(v1.getArray());
            const ibis::array_t<uint64_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::FLOAT: {
            const ibis::array_t<float> &av2 =
                *static_cast<const ibis::array_t<float>*>(v2.getArray());
            ibis::array_t<float> &av0 =
                *static_cast<ibis::array_t<float>*>(v1.getArray());
            const ibis::array_t<float> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::DOUBLE: {
            const ibis::array_t<double> &av2 =
                *static_cast<const ibis::array_t<double>*>(v2.getArray());
            ibis::array_t<double> &av0 =
                *static_cast<ibis::array_t<double>*>(v1.getArray());
            const ibis::array_t<double> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        } // v1.type()
        break;}
    case ibis::UINT: {
        const ibis::array_t<uint32_t> &ak2 =
            *static_cast<const ibis::array_t<uint32_t>*>(k2.getArray());
        ibis::array_t<uint32_t> &ak0 =
            *static_cast<ibis::array_t<uint32_t>*>(k1.getArray());
        const ibis::array_t<uint32_t> ak1(ak0);
        ak0.nosharing();
        switch (v1.type()) {
        default:
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bord::merge11 can not deal with v1 ("
                << v1.name() << ") of type "
                << ibis::TYPESTRING[(int)v1.type()];
            return -3;
        case ibis::BYTE: {
            const ibis::array_t<signed char> &av2 =
                *static_cast<const ibis::array_t<signed char>*>(v2.getArray());
            ibis::array_t<signed char> &av0 =
                *static_cast<ibis::array_t<signed char>*>(v1.getArray());
            const ibis::array_t<signed char> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::UBYTE: {
            const ibis::array_t<unsigned char> &av2 =
                *static_cast<const ibis::array_t<unsigned char>*>(v2.getArray());
            ibis::array_t<unsigned char> &av0 =
                *static_cast<ibis::array_t<unsigned char>*>(v1.getArray());
            const ibis::array_t<unsigned char> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::SHORT: {
            const ibis::array_t<int16_t> &av2 =
                *static_cast<const ibis::array_t<int16_t>*>(v2.getArray());
            ibis::array_t<int16_t> &av0 =
                *static_cast<ibis::array_t<int16_t>*>(v1.getArray());
            const ibis::array_t<int16_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::USHORT: {
            const ibis::array_t<uint16_t> &av2 =
                *static_cast<const ibis::array_t<uint16_t>*>(v2.getArray());
            ibis::array_t<uint16_t> &av0 =
                *static_cast<ibis::array_t<uint16_t>*>(v1.getArray());
            const ibis::array_t<uint16_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::INT: {
            const ibis::array_t<int32_t> &av2 =
                *static_cast<const ibis::array_t<int32_t>*>(v2.getArray());
            ibis::array_t<int32_t> &av0 =
                *static_cast<ibis::array_t<int32_t>*>(v1.getArray());
            const ibis::array_t<int32_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::UINT: {
            const ibis::array_t<uint32_t> &av2 =
                *static_cast<const ibis::array_t<uint32_t>*>(v2.getArray());
            ibis::array_t<uint32_t> &av0 =
                *static_cast<ibis::array_t<uint32_t>*>(v1.getArray());
            const ibis::array_t<uint32_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::LONG: {
            const ibis::array_t<int64_t> &av2 =
                *static_cast<const ibis::array_t<int64_t>*>(v2.getArray());
            ibis::array_t<int64_t> &av0 =
                *static_cast<ibis::array_t<int64_t>*>(v1.getArray());
            const ibis::array_t<int64_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::ULONG: {
            const ibis::array_t<uint64_t> &av2 =
                *static_cast<const ibis::array_t<uint64_t>*>(v2.getArray());
            ibis::array_t<uint64_t> &av0 =
                *static_cast<ibis::array_t<uint64_t>*>(v1.getArray());
            const ibis::array_t<uint64_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::FLOAT: {
            const ibis::array_t<float> &av2 =
                *static_cast<const ibis::array_t<float>*>(v2.getArray());
            ibis::array_t<float> &av0 =
                *static_cast<ibis::array_t<float>*>(v1.getArray());
            const ibis::array_t<float> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::DOUBLE: {
            const ibis::array_t<double> &av2 =
                *static_cast<const ibis::array_t<double>*>(v2.getArray());
            ibis::array_t<double> &av0 =
                *static_cast<ibis::array_t<double>*>(v1.getArray());
            const ibis::array_t<double> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        } // v1.type()
        break;}
    case ibis::LONG: {
        const ibis::array_t<int64_t> &ak2 =
            *static_cast<const ibis::array_t<int64_t>*>(k2.getArray());
        ibis::array_t<int64_t> &ak0 =
            *static_cast<ibis::array_t<int64_t>*>(k1.getArray());
        const ibis::array_t<int64_t> ak1(ak0);
        ak0.nosharing();
        switch (v1.type()) {
        default:
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bord::merge11 can not deal with v1 ("
                << v1.name() << ") of type "
                << ibis::TYPESTRING[(int)v1.type()];
            return -3;
        case ibis::BYTE: {
            const ibis::array_t<signed char> &av2 =
                *static_cast<const ibis::array_t<signed char>*>(v2.getArray());
            ibis::array_t<signed char> &av0 =
                *static_cast<ibis::array_t<signed char>*>(v1.getArray());
            const ibis::array_t<signed char> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::UBYTE: {
            const ibis::array_t<unsigned char> &av2 =
                *static_cast<const ibis::array_t<unsigned char>*>(v2.getArray());
            ibis::array_t<unsigned char> &av0 =
                *static_cast<ibis::array_t<unsigned char>*>(v1.getArray());
            const ibis::array_t<unsigned char> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::SHORT: {
            const ibis::array_t<int16_t> &av2 =
                *static_cast<const ibis::array_t<int16_t>*>(v2.getArray());
            ibis::array_t<int16_t> &av0 =
                *static_cast<ibis::array_t<int16_t>*>(v1.getArray());
            const ibis::array_t<int16_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::USHORT: {
            const ibis::array_t<uint16_t> &av2 =
                *static_cast<const ibis::array_t<uint16_t>*>(v2.getArray());
            ibis::array_t<uint16_t> &av0 =
                *static_cast<ibis::array_t<uint16_t>*>(v1.getArray());
            const ibis::array_t<uint16_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::INT: {
            const ibis::array_t<int32_t> &av2 =
                *static_cast<const ibis::array_t<int32_t>*>(v2.getArray());
            ibis::array_t<int32_t> &av0 =
                *static_cast<ibis::array_t<int32_t>*>(v1.getArray());
            const ibis::array_t<int32_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::UINT: {
            const ibis::array_t<uint32_t> &av2 =
                *static_cast<const ibis::array_t<uint32_t>*>(v2.getArray());
            ibis::array_t<uint32_t> &av0 =
                *static_cast<ibis::array_t<uint32_t>*>(v1.getArray());
            const ibis::array_t<uint32_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::LONG: {
            const ibis::array_t<int64_t> &av2 =
                *static_cast<const ibis::array_t<int64_t>*>(v2.getArray());
            ibis::array_t<int64_t> &av0 =
                *static_cast<ibis::array_t<int64_t>*>(v1.getArray());
            const ibis::array_t<int64_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::ULONG: {
            const ibis::array_t<uint64_t> &av2 =
                *static_cast<const ibis::array_t<uint64_t>*>(v2.getArray());
            ibis::array_t<uint64_t> &av0 =
                *static_cast<ibis::array_t<uint64_t>*>(v1.getArray());
            const ibis::array_t<uint64_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::FLOAT: {
            const ibis::array_t<float> &av2 =
                *static_cast<const ibis::array_t<float>*>(v2.getArray());
            ibis::array_t<float> &av0 =
                *static_cast<ibis::array_t<float>*>(v1.getArray());
            const ibis::array_t<float> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::DOUBLE: {
            const ibis::array_t<double> &av2 =
                *static_cast<const ibis::array_t<double>*>(v2.getArray());
            ibis::array_t<double> &av0 =
                *static_cast<ibis::array_t<double>*>(v1.getArray());
            const ibis::array_t<double> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        } // v1.type()
        break;}
    case ibis::ULONG: {
        const ibis::array_t<uint64_t> &ak2 =
            *static_cast<const ibis::array_t<uint64_t>*>(k2.getArray());
        ibis::array_t<uint64_t> &ak0 =
            *static_cast<ibis::array_t<uint64_t>*>(k1.getArray());
        const ibis::array_t<uint64_t> ak1(ak0);
        ak0.nosharing();
        switch (v1.type()) {
        default:
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bord::merge11 can not deal with v1 ("
                << v1.name() << ") of type "
                << ibis::TYPESTRING[(int)v1.type()];
            return -3;
        case ibis::BYTE: {
            const ibis::array_t<signed char> &av2 =
                *static_cast<const ibis::array_t<signed char>*>(v2.getArray());
            ibis::array_t<signed char> &av0 =
                *static_cast<ibis::array_t<signed char>*>(v1.getArray());
            const ibis::array_t<signed char> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::UBYTE: {
            const ibis::array_t<unsigned char> &av2 =
                *static_cast<const ibis::array_t<unsigned char>*>(v2.getArray());
            ibis::array_t<unsigned char> &av0 =
                *static_cast<ibis::array_t<unsigned char>*>(v1.getArray());
            const ibis::array_t<unsigned char> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::SHORT: {
            const ibis::array_t<int16_t> &av2 =
                *static_cast<const ibis::array_t<int16_t>*>(v2.getArray());
            ibis::array_t<int16_t> &av0 =
                *static_cast<ibis::array_t<int16_t>*>(v1.getArray());
            const ibis::array_t<int16_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::USHORT: {
            const ibis::array_t<uint16_t> &av2 =
                *static_cast<const ibis::array_t<uint16_t>*>(v2.getArray());
            ibis::array_t<uint16_t> &av0 =
                *static_cast<ibis::array_t<uint16_t>*>(v1.getArray());
            const ibis::array_t<uint16_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::INT: {
            const ibis::array_t<int32_t> &av2 =
                *static_cast<const ibis::array_t<int32_t>*>(v2.getArray());
            ibis::array_t<int32_t> &av0 =
                *static_cast<ibis::array_t<int32_t>*>(v1.getArray());
            const ibis::array_t<int32_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::UINT: {
            const ibis::array_t<uint32_t> &av2 =
                *static_cast<const ibis::array_t<uint32_t>*>(v2.getArray());
            ibis::array_t<uint32_t> &av0 =
                *static_cast<ibis::array_t<uint32_t>*>(v1.getArray());
            const ibis::array_t<uint32_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::LONG: {
            const ibis::array_t<int64_t> &av2 =
                *static_cast<const ibis::array_t<int64_t>*>(v2.getArray());
            ibis::array_t<int64_t> &av0 =
                *static_cast<ibis::array_t<int64_t>*>(v1.getArray());
            const ibis::array_t<int64_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::ULONG: {
            const ibis::array_t<uint64_t> &av2 =
                *static_cast<const ibis::array_t<uint64_t>*>(v2.getArray());
            ibis::array_t<uint64_t> &av0 =
                *static_cast<ibis::array_t<uint64_t>*>(v1.getArray());
            const ibis::array_t<uint64_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::FLOAT: {
            const ibis::array_t<float> &av2 =
                *static_cast<const ibis::array_t<float>*>(v2.getArray());
            ibis::array_t<float> &av0 =
                *static_cast<ibis::array_t<float>*>(v1.getArray());
            const ibis::array_t<float> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::DOUBLE: {
            const ibis::array_t<double> &av2 =
                *static_cast<const ibis::array_t<double>*>(v2.getArray());
            ibis::array_t<double> &av0 =
                *static_cast<ibis::array_t<double>*>(v1.getArray());
            const ibis::array_t<double> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        } // v1.type()
        break;}
    case ibis::FLOAT: {
        const ibis::array_t<float> &ak2 =
            *static_cast<const ibis::array_t<float>*>(k2.getArray());
        ibis::array_t<float> &ak0 =
            *static_cast<ibis::array_t<float>*>(k1.getArray());
        const ibis::array_t<float> ak1(ak0);
        ak0.nosharing();
        switch (v1.type()) {
        default:
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bord::merge11 can not deal with v1 ("
                << v1.name() << ") of type "
                << ibis::TYPESTRING[(int)v1.type()];
            return -3;
        case ibis::BYTE: {
            const ibis::array_t<signed char> &av2 =
                *static_cast<const ibis::array_t<signed char>*>(v2.getArray());
            ibis::array_t<signed char> &av0 =
                *static_cast<ibis::array_t<signed char>*>(v1.getArray());
            const ibis::array_t<signed char> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::UBYTE: {
            const ibis::array_t<unsigned char> &av2 =
                *static_cast<const ibis::array_t<unsigned char>*>(v2.getArray());
            ibis::array_t<unsigned char> &av0 =
                *static_cast<ibis::array_t<unsigned char>*>(v1.getArray());
            const ibis::array_t<unsigned char> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::SHORT: {
            const ibis::array_t<int16_t> &av2 =
                *static_cast<const ibis::array_t<int16_t>*>(v2.getArray());
            ibis::array_t<int16_t> &av0 =
                *static_cast<ibis::array_t<int16_t>*>(v1.getArray());
            const ibis::array_t<int16_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::USHORT: {
            const ibis::array_t<uint16_t> &av2 =
                *static_cast<const ibis::array_t<uint16_t>*>(v2.getArray());
            ibis::array_t<uint16_t> &av0 =
                *static_cast<ibis::array_t<uint16_t>*>(v1.getArray());
            const ibis::array_t<uint16_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::INT: {
            const ibis::array_t<int32_t> &av2 =
                *static_cast<const ibis::array_t<int32_t>*>(v2.getArray());
            ibis::array_t<int32_t> &av0 =
                *static_cast<ibis::array_t<int32_t>*>(v1.getArray());
            const ibis::array_t<int32_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::UINT: {
            const ibis::array_t<uint32_t> &av2 =
                *static_cast<const ibis::array_t<uint32_t>*>(v2.getArray());
            ibis::array_t<uint32_t> &av0 =
                *static_cast<ibis::array_t<uint32_t>*>(v1.getArray());
            const ibis::array_t<uint32_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::LONG: {
            const ibis::array_t<int64_t> &av2 =
                *static_cast<const ibis::array_t<int64_t>*>(v2.getArray());
            ibis::array_t<int64_t> &av0 =
                *static_cast<ibis::array_t<int64_t>*>(v1.getArray());
            const ibis::array_t<int64_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::ULONG: {
            const ibis::array_t<uint64_t> &av2 =
                *static_cast<const ibis::array_t<uint64_t>*>(v2.getArray());
            ibis::array_t<uint64_t> &av0 =
                *static_cast<ibis::array_t<uint64_t>*>(v1.getArray());
            const ibis::array_t<uint64_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::FLOAT: {
            const ibis::array_t<float> &av2 =
                *static_cast<const ibis::array_t<float>*>(v2.getArray());
            ibis::array_t<float> &av0 =
                *static_cast<ibis::array_t<float>*>(v1.getArray());
            const ibis::array_t<float> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::DOUBLE: {
            const ibis::array_t<double> &av2 =
                *static_cast<const ibis::array_t<double>*>(v2.getArray());
            ibis::array_t<double> &av0 =
                *static_cast<ibis::array_t<double>*>(v1.getArray());
            const ibis::array_t<double> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        } // v1.type()
        break;}
    case ibis::DOUBLE: {
        const ibis::array_t<double> &ak2 =
            *static_cast<const ibis::array_t<double>*>(k2.getArray());
        ibis::array_t<double> &ak0 =
            *static_cast<ibis::array_t<double>*>(k1.getArray());
        const ibis::array_t<double> ak1(ak0);
        ak0.nosharing();
        switch (v1.type()) {
        default:
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bord::merge11 can not deal with v1 ("
                << v1.name() << ") of type "
                << ibis::TYPESTRING[(int)v1.type()];
            return -3;
        case ibis::BYTE: {
            const ibis::array_t<signed char> &av2 =
                *static_cast<const ibis::array_t<signed char>*>(v2.getArray());
            ibis::array_t<signed char> &av0 =
                *static_cast<ibis::array_t<signed char>*>(v1.getArray());
            const ibis::array_t<signed char> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::UBYTE: {
            const ibis::array_t<unsigned char> &av2 =
                *static_cast<const ibis::array_t<unsigned char>*>(v2.getArray());
            ibis::array_t<unsigned char> &av0 =
                *static_cast<ibis::array_t<unsigned char>*>(v1.getArray());
            const ibis::array_t<unsigned char> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::SHORT: {
            const ibis::array_t<int16_t> &av2 =
                *static_cast<const ibis::array_t<int16_t>*>(v2.getArray());
            ibis::array_t<int16_t> &av0 =
                *static_cast<ibis::array_t<int16_t>*>(v1.getArray());
            const ibis::array_t<int16_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::USHORT: {
            const ibis::array_t<uint16_t> &av2 =
                *static_cast<const ibis::array_t<uint16_t>*>(v2.getArray());
            ibis::array_t<uint16_t> &av0 =
                *static_cast<ibis::array_t<uint16_t>*>(v1.getArray());
            const ibis::array_t<uint16_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::INT: {
            const ibis::array_t<int32_t> &av2 =
                *static_cast<const ibis::array_t<int32_t>*>(v2.getArray());
            ibis::array_t<int32_t> &av0 =
                *static_cast<ibis::array_t<int32_t>*>(v1.getArray());
            const ibis::array_t<int32_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::UINT: {
            const ibis::array_t<uint32_t> &av2 =
                *static_cast<const ibis::array_t<uint32_t>*>(v2.getArray());
            ibis::array_t<uint32_t> &av0 =
                *static_cast<ibis::array_t<uint32_t>*>(v1.getArray());
            const ibis::array_t<uint32_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::LONG: {
            const ibis::array_t<int64_t> &av2 =
                *static_cast<const ibis::array_t<int64_t>*>(v2.getArray());
            ibis::array_t<int64_t> &av0 =
                *static_cast<ibis::array_t<int64_t>*>(v1.getArray());
            const ibis::array_t<int64_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::ULONG: {
            const ibis::array_t<uint64_t> &av2 =
                *static_cast<const ibis::array_t<uint64_t>*>(v2.getArray());
            ibis::array_t<uint64_t> &av0 =
                *static_cast<ibis::array_t<uint64_t>*>(v1.getArray());
            const ibis::array_t<uint64_t> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::FLOAT: {
            const ibis::array_t<float> &av2 =
                *static_cast<const ibis::array_t<float>*>(v2.getArray());
            ibis::array_t<float> &av0 =
                *static_cast<ibis::array_t<float>*>(v1.getArray());
            const ibis::array_t<float> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::DOUBLE: {
            const ibis::array_t<double> &av2 =
                *static_cast<const ibis::array_t<double>*>(v2.getArray());
            ibis::array_t<double> &av0 =
                *static_cast<ibis::array_t<double>*>(v1.getArray());
            const ibis::array_t<double> av1(av0);
            av0.nosharing();
            ierr = merge11T(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        } // v1.type()
        break;}
    case ibis::CATEGORY:
    case ibis::TEXT: {
        const std::vector<std::string> &ak2 =
            *static_cast<const std::vector<std::string>*>(k2.getArray());
        std::vector<std::string> &ak0 =
            *static_cast<std::vector<std::string>*>(k1.getArray());
        const std::vector<std::string> ak1(ak0);
        switch (v1.type()) {
        default:
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bord::merge11 can not deal with v1 ("
                << v1.name() << ") of type "
                << ibis::TYPESTRING[(int)v1.type()];
            return -3;
        case ibis::BYTE: {
            const ibis::array_t<signed char> &av2 =
                *static_cast<const ibis::array_t<signed char>*>(v2.getArray());
            ibis::array_t<signed char> &av0 =
                *static_cast<ibis::array_t<signed char>*>(v1.getArray());
            const ibis::array_t<signed char> av1(av0);
            av0.nosharing();
            ierr = merge11S(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::UBYTE: {
            const ibis::array_t<unsigned char> &av2 =
                *static_cast<const ibis::array_t<unsigned char>*>(v2.getArray());
            ibis::array_t<unsigned char> &av0 =
                *static_cast<ibis::array_t<unsigned char>*>(v1.getArray());
            const ibis::array_t<unsigned char> av1(av0);
            av0.nosharing();
            ierr = merge11S(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::SHORT: {
            const ibis::array_t<int16_t> &av2 =
                *static_cast<const ibis::array_t<int16_t>*>(v2.getArray());
            ibis::array_t<int16_t> &av0 =
                *static_cast<ibis::array_t<int16_t>*>(v1.getArray());
            const ibis::array_t<int16_t> av1(av0);
            av0.nosharing();
            ierr = merge11S(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::USHORT: {
            const ibis::array_t<uint16_t> &av2 =
                *static_cast<const ibis::array_t<uint16_t>*>(v2.getArray());
            ibis::array_t<uint16_t> &av0 =
                *static_cast<ibis::array_t<uint16_t>*>(v1.getArray());
            const ibis::array_t<uint16_t> av1(av0);
            av0.nosharing();
            ierr = merge11S(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::INT: {
            const ibis::array_t<int32_t> &av2 =
                *static_cast<const ibis::array_t<int32_t>*>(v2.getArray());
            ibis::array_t<int32_t> &av0 =
                *static_cast<ibis::array_t<int32_t>*>(v1.getArray());
            const ibis::array_t<int32_t> av1(av0);
            av0.nosharing();
            ierr = merge11S(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::UINT: {
            const ibis::array_t<uint32_t> &av2 =
                *static_cast<const ibis::array_t<uint32_t>*>(v2.getArray());
            ibis::array_t<uint32_t> &av0 =
                *static_cast<ibis::array_t<uint32_t>*>(v1.getArray());
            const ibis::array_t<uint32_t> av1(av0);
            av0.nosharing();
            ierr = merge11S(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::LONG: {
            const ibis::array_t<int64_t> &av2 =
                *static_cast<const ibis::array_t<int64_t>*>(v2.getArray());
            ibis::array_t<int64_t> &av0 =
                *static_cast<ibis::array_t<int64_t>*>(v1.getArray());
            const ibis::array_t<int64_t> av1(av0);
            av0.nosharing();
            ierr = merge11S(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::ULONG: {
            const ibis::array_t<uint64_t> &av2 =
                *static_cast<const ibis::array_t<uint64_t>*>(v2.getArray());
            ibis::array_t<uint64_t> &av0 =
                *static_cast<ibis::array_t<uint64_t>*>(v1.getArray());
            const ibis::array_t<uint64_t> av1(av0);
            av0.nosharing();
            ierr = merge11S(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::FLOAT: {
            const ibis::array_t<float> &av2 =
                *static_cast<const ibis::array_t<float>*>(v2.getArray());
            ibis::array_t<float> &av0 =
                *static_cast<ibis::array_t<float>*>(v1.getArray());
            const ibis::array_t<float> av1(av0);
            av0.nosharing();
            ierr = merge11S(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        case ibis::DOUBLE: {
            const ibis::array_t<double> &av2 =
                *static_cast<const ibis::array_t<double>*>(v2.getArray());
            ibis::array_t<double> &av0 =
                *static_cast<ibis::array_t<double>*>(v1.getArray());
            const ibis::array_t<double> av1(av0);
            av0.nosharing();
            ierr = merge11S(ak0, av0, ak1, av1, ak2, av2, agg);
            break;}
        } // v1.type()
        break;}
    } // k1.type()
    return ierr;
} // ibis::bord::merge11

/// Template to perform merge operation with one column as key and one
/// column as value.
template <typename Tk, typename Tv> int
ibis::bord::merge11T(ibis::array_t<Tk> &kout,
                     ibis::array_t<Tv> &vout,
                     const ibis::array_t<Tk> &kin1,
                     const ibis::array_t<Tv> &vin1,
                     const ibis::array_t<Tk> &kin2,
                     const ibis::array_t<Tv> &vin2,
                     ibis::selectClause::AGREGADO agg) {
    kout.clear();
    vout.clear();
    if (kin1.size() != vin1.size() ||
        kin2.size() != vin2.size())
        return -10;
    if (kin1.empty() || vin1.empty()) {
        kout.copy(kin2);
        vout.copy(vin2);
        return kin2.size();
    }
    else if (kin2.empty() || vin2.empty()) {
        kout.copy(kin1);
        vout.copy(vin1);
        return kin1.size();
    }

    size_t i1 = 0;
    size_t i2 = 0;
    while (i1 < kin1.size() && i2 < kin2.size()) {
        if (kin1[i1] == kin2[i2]) {
            switch (agg) {
            default:
                kout.clear();
                vout.clear();
                return -6;
            case ibis::selectClause::CNT:
            case ibis::selectClause::SUM:
                vout.push_back(vin1[i1] + vin2[i2]);
                break;
            case ibis::selectClause::MIN:
                vout.push_back(vin1[i1] <= vin2[i2] ? vin1[i1] : vin2[i2]);
                break;
            case ibis::selectClause::MAX:
                vout.push_back(vin1[i1] >= vin2[i2] ? vin1[i1] : vin2[i2]);
                break;
            }
            kout.push_back(kin1[i1]);
            ++ i1;
            ++ i2;
        }
        else if (kin1[i1] < kin2[i2]) {
            kout.push_back(kin1[i1]);
            vout.push_back(vin1[i1]);
            ++ i1;
        }
        else {
            kout.push_back(kin2[i2]);
            vout.push_back(vin2[i2]);
            ++ i2;
        }
    }

    while (i1 < kin1.size()) {
        kout.push_back(kin1[i1]);
        vout.push_back(vin1[i1]);
        ++ i1;
    }
    while (i2 < kin2.size()) {
        kout.push_back(kin2[i2]);
        vout.push_back(vin2[i2]);
        ++ i2;
    }
    return kout.size();
} // ibis::bord::merge11T

template <typename Tv> int
ibis::bord::merge11S(std::vector<std::string> &kout,
                     ibis::array_t<Tv> &vout,
                     const std::vector<std::string> &kin1,
                     const ibis::array_t<Tv> &vin1,
                     const std::vector<std::string> &kin2,
                     const ibis::array_t<Tv> &vin2,
                     ibis::selectClause::AGREGADO agg) {
    kout.clear();
    vout.clear();
    if (kin1.size() != vin1.size() ||
        kin2.size() != vin2.size())
        return -10;
    if (kin1.empty() || vin1.empty()) {
        kout.insert(kout.end(), kin2.begin(), kin2.end());
        vout.copy(vin2);
        return kin2.size();
    }
    else if (kin2.empty() || vin2.empty()) {
        kout.insert(kout.end(), kin1.begin(), kin1.end());
        vout.copy(vin1);
        return kin1.size();
    }

    size_t i1 = 0;
    size_t i2 = 0;
    while (i1 < kin1.size() && i2 < kin2.size()) {
        if (kin1[i1] == kin2[i2]) {
            switch (agg) {
            default:
                kout.clear();
                vout.clear();
                return -6;
            case ibis::selectClause::CNT:
            case ibis::selectClause::SUM:
                vout.push_back(vin1[i1] + vin2[i2]);
                break;
            case ibis::selectClause::MIN:
                vout.push_back(vin1[i1] <= vin2[i2] ? vin1[i1] : vin2[i2]);
                break;
            case ibis::selectClause::MAX:
                vout.push_back(vin1[i1] >= vin2[i2] ? vin1[i1] : vin2[i2]);
                break;
            }
            kout.push_back(kin1[i1]);
            ++ i1;
            ++ i2;
        }
        else if (kin1[i1] < kin2[i2]) {
            kout.push_back(kin1[i1]);
            vout.push_back(vin1[i1]);
            ++ i1;
        }
        else {
            kout.push_back(kin2[i2]);
            vout.push_back(vin2[i2]);
            ++ i2;
        }
    }

    while (i1 < kin1.size()) {
        kout.push_back(kin1[i1]);
        vout.push_back(vin1[i1]);
        ++ i1;
    }
    while (i2 < kin2.size()) {
        kout.push_back(kin2[i2]);
        vout.push_back(vin2[i2]);
        ++ i2;
    }
    return kout.size();
} // ibis::bord::merge11S

/// Merge two aggregations sharing the same key.
int ibis::bord::merge12(ibis::bord::column &k1,
                        ibis::bord::column &u1,
                        ibis::bord::column &v1,
                        const ibis::bord::column &k2,
                        const ibis::bord::column &u2,
                        const ibis::bord::column &v2,
                        ibis::selectClause::AGREGADO au,
                        ibis::selectClause::AGREGADO av) {
    int ierr = -1;
    if (k1.type() != k2.type() || u1.type() != u2.type() ||
        v1.type() != v2.type()) {
        LOGGER(ibis::gVerbose > 2)
            << "Warning -- bord::merge12 expects the same types and sizes "
            "for the keys and values";
        return ierr;
    }

    switch (k1.type()) {
    default:
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bord::merge12 can not deal with k1 ("
            << k1.name() << ") of type "
            << ibis::TYPESTRING[(int)k1.type()];
        return -2;
    case ibis::BYTE: {
        const ibis::array_t<signed char> &ak2 =
            *static_cast<const ibis::array_t<signed char>*>(k2.getArray());
        ibis::array_t<signed char> &ak0 =
            *static_cast<ibis::array_t<signed char>*>(k1.getArray());
        const ibis::array_t<signed char> ak1(ak0);
        ak0.nosharing();
        ierr = merge12T1(ak0, ak1, ak2, u1, v1, u2, v2, au, av);
        break;}
    case ibis::UBYTE: {
        const ibis::array_t<unsigned char> &ak2 =
            *static_cast<const ibis::array_t<unsigned char>*>(k2.getArray());
        ibis::array_t<unsigned char> &ak0 =
            *static_cast<ibis::array_t<unsigned char>*>(k1.getArray());
        const ibis::array_t<unsigned char> ak1(ak0);
        ak0.nosharing();
        ierr = merge12T1(ak0, ak1, ak2, u1, v1, u2, v2, au, av);
        break;}
    case ibis::SHORT: {
        const ibis::array_t<int16_t> &ak2 =
            *static_cast<const ibis::array_t<int16_t>*>(k2.getArray());
        ibis::array_t<int16_t> &ak0 =
            *static_cast<ibis::array_t<int16_t>*>(k1.getArray());
        const ibis::array_t<int16_t> ak1(ak0);
        ak0.nosharing();
        ierr = merge12T1(ak0, ak1, ak2, u1, v1, u2, v2, au, av);
        break;}
    case ibis::USHORT: {
        const ibis::array_t<uint16_t> &ak2 =
            *static_cast<const ibis::array_t<uint16_t>*>(k2.getArray());
        ibis::array_t<uint16_t> &ak0 =
            *static_cast<ibis::array_t<uint16_t>*>(k1.getArray());
        const ibis::array_t<uint16_t> ak1(ak0);
        ak0.nosharing();
        ierr = merge12T1(ak0, ak1, ak2, u1, v1, u2, v2, au, av);
        break;}
    case ibis::INT: {
        const ibis::array_t<int32_t> &ak2 =
            *static_cast<const ibis::array_t<int32_t>*>(k2.getArray());
        ibis::array_t<int32_t> &ak0 =
            *static_cast<ibis::array_t<int32_t>*>(k1.getArray());
        const ibis::array_t<int32_t> ak1(ak0);
        ak0.nosharing();
        ierr = merge12T1(ak0, ak1, ak2, u1, v1, u2, v2, au, av);
        break;}
    case ibis::UINT: {
        const ibis::array_t<uint32_t> &ak2 =
            *static_cast<const ibis::array_t<uint32_t>*>(k2.getArray());
        ibis::array_t<uint32_t> &ak0 =
            *static_cast<ibis::array_t<uint32_t>*>(k1.getArray());
        const ibis::array_t<uint32_t> ak1(ak0);
        ak0.nosharing();
        ierr = merge12T1(ak0, ak1, ak2, u1, v1, u2, v2, au, av);
        break;}
    case ibis::LONG: {
        const ibis::array_t<int64_t> &ak2 =
            *static_cast<const ibis::array_t<int64_t>*>(k2.getArray());
        ibis::array_t<int64_t> &ak0 =
            *static_cast<ibis::array_t<int64_t>*>(k1.getArray());
        const ibis::array_t<int64_t> ak1(ak0);
        ak0.nosharing();
        ierr = merge12T1(ak0, ak1, ak2, u1, v1, u2, v2, au, av);
        break;}
    case ibis::ULONG: {
        const ibis::array_t<uint64_t> &ak2 =
            *static_cast<const ibis::array_t<uint64_t>*>(k2.getArray());
        ibis::array_t<uint64_t> &ak0 =
            *static_cast<ibis::array_t<uint64_t>*>(k1.getArray());
        const ibis::array_t<uint64_t> ak1(ak0);
        ak0.nosharing();
        ierr = merge12T1(ak0, ak1, ak2, u1, v1, u2, v2, au, av);
        break;}
    case ibis::FLOAT: {
        const ibis::array_t<float> &ak2 =
            *static_cast<const ibis::array_t<float>*>(k2.getArray());
        ibis::array_t<float> &ak0 =
            *static_cast<ibis::array_t<float>*>(k1.getArray());
        const ibis::array_t<float> ak1(ak0);
        ak0.nosharing();
        ierr = merge12T1(ak0, ak1, ak2, u1, v1, u2, v2, au, av);
        break;}
    case ibis::DOUBLE: {
        const ibis::array_t<double> &ak2 =
            *static_cast<const ibis::array_t<double>*>(k2.getArray());
        ibis::array_t<double> &ak0 =
            *static_cast<ibis::array_t<double>*>(k1.getArray());
        const ibis::array_t<double> ak1(ak0);
        ak0.nosharing();
        ierr = merge12T1(ak0, ak1, ak2, u1, v1, u2, v2, au, av);
        break;}
    case ibis::CATEGORY:
    case ibis::TEXT: {
        const std::vector<std::string> &ak2 =
            *static_cast<const std::vector<std::string>*>(k2.getArray());
        std::vector<std::string> &ak0 =
            *static_cast<std::vector<std::string>*>(k1.getArray());
        const std::vector<std::string> ak1(ak0);
        ierr = merge12S1(ak0, ak1, ak2, u1, v1, u2, v2, au, av);
        break;}
    }
    return ierr;
} // ibis::bord::merge12

template <typename Tk> int
ibis::bord::merge12T1(ibis::array_t<Tk> &kout,
                      const ibis::array_t<Tk> &kin1,
                      const ibis::array_t<Tk> &kin2,
                      ibis::bord::column &u1,
                      ibis::bord::column &v1,
                      const ibis::bord::column &u2,
                      const ibis::bord::column &v2,
                      ibis::selectClause::AGREGADO au,
                      ibis::selectClause::AGREGADO av) {
    int ierr = -1;
    if (u1.type() != u2.type() || v1.type() != v2.type())
        return ierr;

    switch (u1.type()) {
    default:
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bord::merge12T1 can not deal with u1 ("
            << u1.name() << ") of type "
            << ibis::TYPESTRING[(int)u1.type()];
        return ierr;
    case ibis::BYTE: {
        const ibis::array_t<signed char>& au2 =
            *static_cast<const ibis::array_t<signed char>*>(u2.getArray());
        ibis::array_t<signed char>& au0 =
            *static_cast<ibis::array_t<signed char>*>(u1.getArray());
        const ibis::array_t<signed char> au1(au0);
        au0.nosharing();
        switch(v1.type()) {
        default:
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bord::merge12T1 can not deal with v1 ("
                << v1.name() << ") of type "
                << ibis::TYPESTRING[(int)v1.type()];
            return ierr;
        case ibis::BYTE: {
            const ibis::array_t<signed char>& av2 =
                *static_cast<const ibis::array_t<signed char>*>(v2.getArray());
            ibis::array_t<signed char>& av0 =
                *static_cast<ibis::array_t<signed char>*>(v1.getArray());
            const ibis::array_t<signed char> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::UBYTE: {
            const ibis::array_t<unsigned char>& av2 =
                *static_cast<const ibis::array_t<unsigned char>*>(v2.getArray());
            ibis::array_t<unsigned char>& av0 =
                *static_cast<ibis::array_t<unsigned char>*>(v1.getArray());
            const ibis::array_t<unsigned char> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::SHORT: {
            const ibis::array_t<int16_t>& av2 =
                *static_cast<const ibis::array_t<int16_t>*>(v2.getArray());
            ibis::array_t<int16_t>& av0 =
                *static_cast<ibis::array_t<int16_t>*>(v1.getArray());
            const ibis::array_t<int16_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::USHORT: {
            const ibis::array_t<uint16_t>& av2 =
                *static_cast<const ibis::array_t<uint16_t>*>(v2.getArray());
            ibis::array_t<uint16_t>& av0 =
                *static_cast<ibis::array_t<uint16_t>*>(v1.getArray());
            const ibis::array_t<uint16_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::INT: {
            const ibis::array_t<int32_t>& av2 =
                *static_cast<const ibis::array_t<int32_t>*>(v2.getArray());
            ibis::array_t<int32_t>& av0 =
                *static_cast<ibis::array_t<int32_t>*>(v1.getArray());
            const ibis::array_t<int32_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::UINT: {
            const ibis::array_t<uint32_t>& av2 =
                *static_cast<const ibis::array_t<uint32_t>*>(v2.getArray());
            ibis::array_t<uint32_t>& av0 =
                *static_cast<ibis::array_t<uint32_t>*>(v1.getArray());
            const ibis::array_t<uint32_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::LONG: {
            const ibis::array_t<int64_t>& av2 =
                *static_cast<const ibis::array_t<int64_t>*>(v2.getArray());
            ibis::array_t<int64_t>& av0 =
                *static_cast<ibis::array_t<int64_t>*>(v1.getArray());
            const ibis::array_t<int64_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::ULONG: {
            const ibis::array_t<uint64_t>& av2 =
                *static_cast<const ibis::array_t<uint64_t>*>(v2.getArray());
            ibis::array_t<uint64_t>& av0 =
                *static_cast<ibis::array_t<uint64_t>*>(v1.getArray());
            const ibis::array_t<uint64_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::FLOAT: {
            const ibis::array_t<float>& av2 =
                *static_cast<const ibis::array_t<float>*>(v2.getArray());
            ibis::array_t<float>& av0 =
                *static_cast<ibis::array_t<float>*>(v1.getArray());
            const ibis::array_t<float> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::DOUBLE: {
            const ibis::array_t<double>& av2 =
                *static_cast<const ibis::array_t<double>*>(v2.getArray());
            ibis::array_t<double>& av0 =
                *static_cast<ibis::array_t<double>*>(v1.getArray());
            const ibis::array_t<double> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        }
        break;}
    case ibis::UBYTE: {
        const ibis::array_t<unsigned char>& au2 =
            *static_cast<const ibis::array_t<unsigned char>*>(u2.getArray());
        ibis::array_t<unsigned char>& au0 =
            *static_cast<ibis::array_t<unsigned char>*>(u1.getArray());
        const ibis::array_t<unsigned char> au1(au0);
        au0.nosharing();
        switch(v1.type()) {
        default:
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bord::merge12T1 can not deal with v1 ("
                << v1.name() << ") of type "
                << ibis::TYPESTRING[(int)v1.type()];
            return ierr;
        case ibis::BYTE: {
            const ibis::array_t<signed char>& av2 =
                *static_cast<const ibis::array_t<signed char>*>(v2.getArray());
            ibis::array_t<signed char>& av0 =
                *static_cast<ibis::array_t<signed char>*>(v1.getArray());
            const ibis::array_t<signed char> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::UBYTE: {
            const ibis::array_t<unsigned char>& av2 =
                *static_cast<const ibis::array_t<unsigned char>*>(v2.getArray());
            ibis::array_t<unsigned char>& av0 =
                *static_cast<ibis::array_t<unsigned char>*>(v1.getArray());
            const ibis::array_t<unsigned char> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::SHORT: {
            const ibis::array_t<int16_t>& av2 =
                *static_cast<const ibis::array_t<int16_t>*>(v2.getArray());
            ibis::array_t<int16_t>& av0 =
                *static_cast<ibis::array_t<int16_t>*>(v1.getArray());
            const ibis::array_t<int16_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::USHORT: {
            const ibis::array_t<uint16_t>& av2 =
                *static_cast<const ibis::array_t<uint16_t>*>(v2.getArray());
            ibis::array_t<uint16_t>& av0 =
                *static_cast<ibis::array_t<uint16_t>*>(v1.getArray());
            const ibis::array_t<uint16_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::INT: {
            const ibis::array_t<int32_t>& av2 =
                *static_cast<const ibis::array_t<int32_t>*>(v2.getArray());
            ibis::array_t<int32_t>& av0 =
                *static_cast<ibis::array_t<int32_t>*>(v1.getArray());
            const ibis::array_t<int32_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::UINT: {
            const ibis::array_t<uint32_t>& av2 =
                *static_cast<const ibis::array_t<uint32_t>*>(v2.getArray());
            ibis::array_t<uint32_t>& av0 =
                *static_cast<ibis::array_t<uint32_t>*>(v1.getArray());
            const ibis::array_t<uint32_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::LONG: {
            const ibis::array_t<int64_t>& av2 =
                *static_cast<const ibis::array_t<int64_t>*>(v2.getArray());
            ibis::array_t<int64_t>& av0 =
                *static_cast<ibis::array_t<int64_t>*>(v1.getArray());
            const ibis::array_t<int64_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::ULONG: {
            const ibis::array_t<uint64_t>& av2 =
                *static_cast<const ibis::array_t<uint64_t>*>(v2.getArray());
            ibis::array_t<uint64_t>& av0 =
                *static_cast<ibis::array_t<uint64_t>*>(v1.getArray());
            const ibis::array_t<uint64_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::FLOAT: {
            const ibis::array_t<float>& av2 =
                *static_cast<const ibis::array_t<float>*>(v2.getArray());
            ibis::array_t<float>& av0 =
                *static_cast<ibis::array_t<float>*>(v1.getArray());
            const ibis::array_t<float> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::DOUBLE: {
            const ibis::array_t<double>& av2 =
                *static_cast<const ibis::array_t<double>*>(v2.getArray());
            ibis::array_t<double>& av0 =
                *static_cast<ibis::array_t<double>*>(v1.getArray());
            const ibis::array_t<double> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        }
        break;}
    case ibis::SHORT: {
        const ibis::array_t<int16_t>& au2 =
            *static_cast<const ibis::array_t<int16_t>*>(u2.getArray());
        ibis::array_t<int16_t>& au0 =
            *static_cast<ibis::array_t<int16_t>*>(u1.getArray());
        const ibis::array_t<int16_t> au1(au0);
        au0.nosharing();
        switch(v1.type()) {
        default:
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bord::merge12T1 can not deal with v1 ("
                << v1.name() << ") of type "
                << ibis::TYPESTRING[(int)v1.type()];
            return ierr;
        case ibis::BYTE: {
            const ibis::array_t<signed char>& av2 =
                *static_cast<const ibis::array_t<signed char>*>(v2.getArray());
            ibis::array_t<signed char>& av0 =
                *static_cast<ibis::array_t<signed char>*>(v1.getArray());
            const ibis::array_t<signed char> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::UBYTE: {
            const ibis::array_t<unsigned char>& av2 =
                *static_cast<const ibis::array_t<unsigned char>*>(v2.getArray());
            ibis::array_t<unsigned char>& av0 =
                *static_cast<ibis::array_t<unsigned char>*>(v1.getArray());
            const ibis::array_t<unsigned char> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::SHORT: {
            const ibis::array_t<int16_t>& av2 =
                *static_cast<const ibis::array_t<int16_t>*>(v2.getArray());
            ibis::array_t<int16_t>& av0 =
                *static_cast<ibis::array_t<int16_t>*>(v1.getArray());
            const ibis::array_t<int16_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::USHORT: {
            const ibis::array_t<uint16_t>& av2 =
                *static_cast<const ibis::array_t<uint16_t>*>(v2.getArray());
            ibis::array_t<uint16_t>& av0 =
                *static_cast<ibis::array_t<uint16_t>*>(v1.getArray());
            const ibis::array_t<uint16_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::INT: {
            const ibis::array_t<int32_t>& av2 =
                *static_cast<const ibis::array_t<int32_t>*>(v2.getArray());
            ibis::array_t<int32_t>& av0 =
                *static_cast<ibis::array_t<int32_t>*>(v1.getArray());
            const ibis::array_t<int32_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::UINT: {
            const ibis::array_t<uint32_t>& av2 =
                *static_cast<const ibis::array_t<uint32_t>*>(v2.getArray());
            ibis::array_t<uint32_t>& av0 =
                *static_cast<ibis::array_t<uint32_t>*>(v1.getArray());
            const ibis::array_t<uint32_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::LONG: {
            const ibis::array_t<int64_t>& av2 =
                *static_cast<const ibis::array_t<int64_t>*>(v2.getArray());
            ibis::array_t<int64_t>& av0 =
                *static_cast<ibis::array_t<int64_t>*>(v1.getArray());
            const ibis::array_t<int64_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::ULONG: {
            const ibis::array_t<uint64_t>& av2 =
                *static_cast<const ibis::array_t<uint64_t>*>(v2.getArray());
            ibis::array_t<uint64_t>& av0 =
                *static_cast<ibis::array_t<uint64_t>*>(v1.getArray());
            const ibis::array_t<uint64_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::FLOAT: {
            const ibis::array_t<float>& av2 =
                *static_cast<const ibis::array_t<float>*>(v2.getArray());
            ibis::array_t<float>& av0 =
                *static_cast<ibis::array_t<float>*>(v1.getArray());
            const ibis::array_t<float> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::DOUBLE: {
            const ibis::array_t<double>& av2 =
                *static_cast<const ibis::array_t<double>*>(v2.getArray());
            ibis::array_t<double>& av0 =
                *static_cast<ibis::array_t<double>*>(v1.getArray());
            const ibis::array_t<double> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        }
        break;}
    case ibis::USHORT: {
        const ibis::array_t<uint16_t>& au2 =
            *static_cast<const ibis::array_t<uint16_t>*>(u2.getArray());
        ibis::array_t<uint16_t>& au0 =
            *static_cast<ibis::array_t<uint16_t>*>(u1.getArray());
        const ibis::array_t<uint16_t> au1(au0);
        au0.nosharing();
        switch(v1.type()) {
        default:
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bord::merge12T1 can not deal with v1 ("
                << v1.name() << ") of type "
                << ibis::TYPESTRING[(int)v1.type()];
            return ierr;
        case ibis::BYTE: {
            const ibis::array_t<signed char>& av2 =
                *static_cast<const ibis::array_t<signed char>*>(v2.getArray());
            ibis::array_t<signed char>& av0 =
                *static_cast<ibis::array_t<signed char>*>(v1.getArray());
            const ibis::array_t<signed char> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::UBYTE: {
            const ibis::array_t<unsigned char>& av2 =
                *static_cast<const ibis::array_t<unsigned char>*>(v2.getArray());
            ibis::array_t<unsigned char>& av0 =
                *static_cast<ibis::array_t<unsigned char>*>(v1.getArray());
            const ibis::array_t<unsigned char> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::SHORT: {
            const ibis::array_t<int16_t>& av2 =
                *static_cast<const ibis::array_t<int16_t>*>(v2.getArray());
            ibis::array_t<int16_t>& av0 =
                *static_cast<ibis::array_t<int16_t>*>(v1.getArray());
            const ibis::array_t<int16_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::USHORT: {
            const ibis::array_t<uint16_t>& av2 =
                *static_cast<const ibis::array_t<uint16_t>*>(v2.getArray());
            ibis::array_t<uint16_t>& av0 =
                *static_cast<ibis::array_t<uint16_t>*>(v1.getArray());
            const ibis::array_t<uint16_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::INT: {
            const ibis::array_t<int32_t>& av2 =
                *static_cast<const ibis::array_t<int32_t>*>(v2.getArray());
            ibis::array_t<int32_t>& av0 =
                *static_cast<ibis::array_t<int32_t>*>(v1.getArray());
            const ibis::array_t<int32_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::UINT: {
            const ibis::array_t<uint32_t>& av2 =
                *static_cast<const ibis::array_t<uint32_t>*>(v2.getArray());
            ibis::array_t<uint32_t>& av0 =
                *static_cast<ibis::array_t<uint32_t>*>(v1.getArray());
            const ibis::array_t<uint32_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::LONG: {
            const ibis::array_t<int64_t>& av2 =
                *static_cast<const ibis::array_t<int64_t>*>(v2.getArray());
            ibis::array_t<int64_t>& av0 =
                *static_cast<ibis::array_t<int64_t>*>(v1.getArray());
            const ibis::array_t<int64_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::ULONG: {
            const ibis::array_t<uint64_t>& av2 =
                *static_cast<const ibis::array_t<uint64_t>*>(v2.getArray());
            ibis::array_t<uint64_t>& av0 =
                *static_cast<ibis::array_t<uint64_t>*>(v1.getArray());
            const ibis::array_t<uint64_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::FLOAT: {
            const ibis::array_t<float>& av2 =
                *static_cast<const ibis::array_t<float>*>(v2.getArray());
            ibis::array_t<float>& av0 =
                *static_cast<ibis::array_t<float>*>(v1.getArray());
            const ibis::array_t<float> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::DOUBLE: {
            const ibis::array_t<double>& av2 =
                *static_cast<const ibis::array_t<double>*>(v2.getArray());
            ibis::array_t<double>& av0 =
                *static_cast<ibis::array_t<double>*>(v1.getArray());
            const ibis::array_t<double> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        }
        break;}
    case ibis::INT: {
        const ibis::array_t<int32_t>& au2 =
            *static_cast<const ibis::array_t<int32_t>*>(u2.getArray());
        ibis::array_t<int32_t>& au0 =
            *static_cast<ibis::array_t<int32_t>*>(u1.getArray());
        const ibis::array_t<int32_t> au1(au0);
        au0.nosharing();
        switch(v1.type()) {
        default:
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bord::merge12T1 can not deal with v1 ("
                << v1.name() << ") of type "
                << ibis::TYPESTRING[(int)v1.type()];
            return ierr;
        case ibis::BYTE: {
            const ibis::array_t<signed char>& av2 =
                *static_cast<const ibis::array_t<signed char>*>(v2.getArray());
            ibis::array_t<signed char>& av0 =
                *static_cast<ibis::array_t<signed char>*>(v1.getArray());
            const ibis::array_t<signed char> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::UBYTE: {
            const ibis::array_t<unsigned char>& av2 =
                *static_cast<const ibis::array_t<unsigned char>*>(v2.getArray());
            ibis::array_t<unsigned char>& av0 =
                *static_cast<ibis::array_t<unsigned char>*>(v1.getArray());
            const ibis::array_t<unsigned char> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::SHORT: {
            const ibis::array_t<int16_t>& av2 =
                *static_cast<const ibis::array_t<int16_t>*>(v2.getArray());
            ibis::array_t<int16_t>& av0 =
                *static_cast<ibis::array_t<int16_t>*>(v1.getArray());
            const ibis::array_t<int16_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::USHORT: {
            const ibis::array_t<uint16_t>& av2 =
                *static_cast<const ibis::array_t<uint16_t>*>(v2.getArray());
            ibis::array_t<uint16_t>& av0 =
                *static_cast<ibis::array_t<uint16_t>*>(v1.getArray());
            const ibis::array_t<uint16_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::INT: {
            const ibis::array_t<int32_t>& av2 =
                *static_cast<const ibis::array_t<int32_t>*>(v2.getArray());
            ibis::array_t<int32_t>& av0 =
                *static_cast<ibis::array_t<int32_t>*>(v1.getArray());
            const ibis::array_t<int32_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::UINT: {
            const ibis::array_t<uint32_t>& av2 =
                *static_cast<const ibis::array_t<uint32_t>*>(v2.getArray());
            ibis::array_t<uint32_t>& av0 =
                *static_cast<ibis::array_t<uint32_t>*>(v1.getArray());
            const ibis::array_t<uint32_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::LONG: {
            const ibis::array_t<int64_t>& av2 =
                *static_cast<const ibis::array_t<int64_t>*>(v2.getArray());
            ibis::array_t<int64_t>& av0 =
                *static_cast<ibis::array_t<int64_t>*>(v1.getArray());
            const ibis::array_t<int64_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::ULONG: {
            const ibis::array_t<uint64_t>& av2 =
                *static_cast<const ibis::array_t<uint64_t>*>(v2.getArray());
            ibis::array_t<uint64_t>& av0 =
                *static_cast<ibis::array_t<uint64_t>*>(v1.getArray());
            const ibis::array_t<uint64_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::FLOAT: {
            const ibis::array_t<float>& av2 =
                *static_cast<const ibis::array_t<float>*>(v2.getArray());
            ibis::array_t<float>& av0 =
                *static_cast<ibis::array_t<float>*>(v1.getArray());
            const ibis::array_t<float> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::DOUBLE: {
            const ibis::array_t<double>& av2 =
                *static_cast<const ibis::array_t<double>*>(v2.getArray());
            ibis::array_t<double>& av0 =
                *static_cast<ibis::array_t<double>*>(v1.getArray());
            const ibis::array_t<double> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        }
        break;}
    case ibis::UINT: {
        const ibis::array_t<uint32_t>& au2 =
            *static_cast<const ibis::array_t<uint32_t>*>(u2.getArray());
        ibis::array_t<uint32_t>& au0 =
            *static_cast<ibis::array_t<uint32_t>*>(u1.getArray());
        const ibis::array_t<uint32_t> au1(au0);
        au0.nosharing();
        switch(v1.type()) {
        default:
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bord::merge12T1 can not deal with v1 ("
                << v1.name() << ") of type "
                << ibis::TYPESTRING[(int)v1.type()];
            return ierr;
        case ibis::BYTE: {
            const ibis::array_t<signed char>& av2 =
                *static_cast<const ibis::array_t<signed char>*>(v2.getArray());
            ibis::array_t<signed char>& av0 =
                *static_cast<ibis::array_t<signed char>*>(v1.getArray());
            const ibis::array_t<signed char> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::UBYTE: {
            const ibis::array_t<unsigned char>& av2 =
                *static_cast<const ibis::array_t<unsigned char>*>(v2.getArray());
            ibis::array_t<unsigned char>& av0 =
                *static_cast<ibis::array_t<unsigned char>*>(v1.getArray());
            const ibis::array_t<unsigned char> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::SHORT: {
            const ibis::array_t<int16_t>& av2 =
                *static_cast<const ibis::array_t<int16_t>*>(v2.getArray());
            ibis::array_t<int16_t>& av0 =
                *static_cast<ibis::array_t<int16_t>*>(v1.getArray());
            const ibis::array_t<int16_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::USHORT: {
            const ibis::array_t<uint16_t>& av2 =
                *static_cast<const ibis::array_t<uint16_t>*>(v2.getArray());
            ibis::array_t<uint16_t>& av0 =
                *static_cast<ibis::array_t<uint16_t>*>(v1.getArray());
            const ibis::array_t<uint16_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::INT: {
            const ibis::array_t<int32_t>& av2 =
                *static_cast<const ibis::array_t<int32_t>*>(v2.getArray());
            ibis::array_t<int32_t>& av0 =
                *static_cast<ibis::array_t<int32_t>*>(v1.getArray());
            const ibis::array_t<int32_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::UINT: {
            const ibis::array_t<uint32_t>& av2 =
                *static_cast<const ibis::array_t<uint32_t>*>(v2.getArray());
            ibis::array_t<uint32_t>& av0 =
                *static_cast<ibis::array_t<uint32_t>*>(v1.getArray());
            const ibis::array_t<uint32_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::LONG: {
            const ibis::array_t<int64_t>& av2 =
                *static_cast<const ibis::array_t<int64_t>*>(v2.getArray());
            ibis::array_t<int64_t>& av0 =
                *static_cast<ibis::array_t<int64_t>*>(v1.getArray());
            const ibis::array_t<int64_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::ULONG: {
            const ibis::array_t<uint64_t>& av2 =
                *static_cast<const ibis::array_t<uint64_t>*>(v2.getArray());
            ibis::array_t<uint64_t>& av0 =
                *static_cast<ibis::array_t<uint64_t>*>(v1.getArray());
            const ibis::array_t<uint64_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::FLOAT: {
            const ibis::array_t<float>& av2 =
                *static_cast<const ibis::array_t<float>*>(v2.getArray());
            ibis::array_t<float>& av0 =
                *static_cast<ibis::array_t<float>*>(v1.getArray());
            const ibis::array_t<float> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::DOUBLE: {
            const ibis::array_t<double>& av2 =
                *static_cast<const ibis::array_t<double>*>(v2.getArray());
            ibis::array_t<double>& av0 =
                *static_cast<ibis::array_t<double>*>(v1.getArray());
            const ibis::array_t<double> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        }
        break;}
    case ibis::LONG: {
        const ibis::array_t<int64_t>& au2 =
            *static_cast<const ibis::array_t<int64_t>*>(u2.getArray());
        ibis::array_t<int64_t>& au0 =
            *static_cast<ibis::array_t<int64_t>*>(u1.getArray());
        const ibis::array_t<int64_t> au1(au0);
        au0.nosharing();
        switch(v1.type()) {
        default:
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bord::merge12T1 can not deal with v1 ("
                << v1.name() << ") of type "
                << ibis::TYPESTRING[(int)v1.type()];
            return ierr;
        case ibis::BYTE: {
            const ibis::array_t<signed char>& av2 =
                *static_cast<const ibis::array_t<signed char>*>(v2.getArray());
            ibis::array_t<signed char>& av0 =
                *static_cast<ibis::array_t<signed char>*>(v1.getArray());
            const ibis::array_t<signed char> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::UBYTE: {
            const ibis::array_t<unsigned char>& av2 =
                *static_cast<const ibis::array_t<unsigned char>*>(v2.getArray());
            ibis::array_t<unsigned char>& av0 =
                *static_cast<ibis::array_t<unsigned char>*>(v1.getArray());
            const ibis::array_t<unsigned char> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::SHORT: {
            const ibis::array_t<int16_t>& av2 =
                *static_cast<const ibis::array_t<int16_t>*>(v2.getArray());
            ibis::array_t<int16_t>& av0 =
                *static_cast<ibis::array_t<int16_t>*>(v1.getArray());
            const ibis::array_t<int16_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::USHORT: {
            const ibis::array_t<uint16_t>& av2 =
                *static_cast<const ibis::array_t<uint16_t>*>(v2.getArray());
            ibis::array_t<uint16_t>& av0 =
                *static_cast<ibis::array_t<uint16_t>*>(v1.getArray());
            const ibis::array_t<uint16_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::INT: {
            const ibis::array_t<int32_t>& av2 =
                *static_cast<const ibis::array_t<int32_t>*>(v2.getArray());
            ibis::array_t<int32_t>& av0 =
                *static_cast<ibis::array_t<int32_t>*>(v1.getArray());
            const ibis::array_t<int32_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::UINT: {
            const ibis::array_t<uint32_t>& av2 =
                *static_cast<const ibis::array_t<uint32_t>*>(v2.getArray());
            ibis::array_t<uint32_t>& av0 =
                *static_cast<ibis::array_t<uint32_t>*>(v1.getArray());
            const ibis::array_t<uint32_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::LONG: {
            const ibis::array_t<int64_t>& av2 =
                *static_cast<const ibis::array_t<int64_t>*>(v2.getArray());
            ibis::array_t<int64_t>& av0 =
                *static_cast<ibis::array_t<int64_t>*>(v1.getArray());
            const ibis::array_t<int64_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::ULONG: {
            const ibis::array_t<uint64_t>& av2 =
                *static_cast<const ibis::array_t<uint64_t>*>(v2.getArray());
            ibis::array_t<uint64_t>& av0 =
                *static_cast<ibis::array_t<uint64_t>*>(v1.getArray());
            const ibis::array_t<uint64_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::FLOAT: {
            const ibis::array_t<float>& av2 =
                *static_cast<const ibis::array_t<float>*>(v2.getArray());
            ibis::array_t<float>& av0 =
                *static_cast<ibis::array_t<float>*>(v1.getArray());
            const ibis::array_t<float> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::DOUBLE: {
            const ibis::array_t<double>& av2 =
                *static_cast<const ibis::array_t<double>*>(v2.getArray());
            ibis::array_t<double>& av0 =
                *static_cast<ibis::array_t<double>*>(v1.getArray());
            const ibis::array_t<double> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        }
        break;}
    case ibis::ULONG: {
        const ibis::array_t<uint64_t>& au2 =
            *static_cast<const ibis::array_t<uint64_t>*>(u2.getArray());
        ibis::array_t<uint64_t>& au0 =
            *static_cast<ibis::array_t<uint64_t>*>(u1.getArray());
        const ibis::array_t<uint64_t> au1(au0);
        au0.nosharing();
        switch(v1.type()) {
        default:
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bord::merge12T1 can not deal with v1 ("
                << v1.name() << ") of type "
                << ibis::TYPESTRING[(int)v1.type()];
            return ierr;
        case ibis::BYTE: {
            const ibis::array_t<signed char>& av2 =
                *static_cast<const ibis::array_t<signed char>*>(v2.getArray());
            ibis::array_t<signed char>& av0 =
                *static_cast<ibis::array_t<signed char>*>(v1.getArray());
            const ibis::array_t<signed char> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::UBYTE: {
            const ibis::array_t<unsigned char>& av2 =
                *static_cast<const ibis::array_t<unsigned char>*>(v2.getArray());
            ibis::array_t<unsigned char>& av0 =
                *static_cast<ibis::array_t<unsigned char>*>(v1.getArray());
            const ibis::array_t<unsigned char> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::SHORT: {
            const ibis::array_t<int16_t>& av2 =
                *static_cast<const ibis::array_t<int16_t>*>(v2.getArray());
            ibis::array_t<int16_t>& av0 =
                *static_cast<ibis::array_t<int16_t>*>(v1.getArray());
            const ibis::array_t<int16_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::USHORT: {
            const ibis::array_t<uint16_t>& av2 =
                *static_cast<const ibis::array_t<uint16_t>*>(v2.getArray());
            ibis::array_t<uint16_t>& av0 =
                *static_cast<ibis::array_t<uint16_t>*>(v1.getArray());
            const ibis::array_t<uint16_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::INT: {
            const ibis::array_t<int32_t>& av2 =
                *static_cast<const ibis::array_t<int32_t>*>(v2.getArray());
            ibis::array_t<int32_t>& av0 =
                *static_cast<ibis::array_t<int32_t>*>(v1.getArray());
            const ibis::array_t<int32_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::UINT: {
            const ibis::array_t<uint32_t>& av2 =
                *static_cast<const ibis::array_t<uint32_t>*>(v2.getArray());
            ibis::array_t<uint32_t>& av0 =
                *static_cast<ibis::array_t<uint32_t>*>(v1.getArray());
            const ibis::array_t<uint32_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::LONG: {
            const ibis::array_t<int64_t>& av2 =
                *static_cast<const ibis::array_t<int64_t>*>(v2.getArray());
            ibis::array_t<int64_t>& av0 =
                *static_cast<ibis::array_t<int64_t>*>(v1.getArray());
            const ibis::array_t<int64_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::ULONG: {
            const ibis::array_t<uint64_t>& av2 =
                *static_cast<const ibis::array_t<uint64_t>*>(v2.getArray());
            ibis::array_t<uint64_t>& av0 =
                *static_cast<ibis::array_t<uint64_t>*>(v1.getArray());
            const ibis::array_t<uint64_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::FLOAT: {
            const ibis::array_t<float>& av2 =
                *static_cast<const ibis::array_t<float>*>(v2.getArray());
            ibis::array_t<float>& av0 =
                *static_cast<ibis::array_t<float>*>(v1.getArray());
            const ibis::array_t<float> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::DOUBLE: {
            const ibis::array_t<double>& av2 =
                *static_cast<const ibis::array_t<double>*>(v2.getArray());
            ibis::array_t<double>& av0 =
                *static_cast<ibis::array_t<double>*>(v1.getArray());
            const ibis::array_t<double> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        }
        break;}
    case ibis::FLOAT: {
        const ibis::array_t<float>& au2 =
            *static_cast<const ibis::array_t<float>*>(u2.getArray());
        ibis::array_t<float>& au0 =
            *static_cast<ibis::array_t<float>*>(u1.getArray());
        const ibis::array_t<float> au1(au0);
        au0.nosharing();
        switch(v1.type()) {
        default:
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bord::merge12T1 can not deal with v1 ("
                << v1.name() << ") of type "
                << ibis::TYPESTRING[(int)v1.type()];
            return ierr;
        case ibis::BYTE: {
            const ibis::array_t<signed char>& av2 =
                *static_cast<const ibis::array_t<signed char>*>(v2.getArray());
            ibis::array_t<signed char>& av0 =
                *static_cast<ibis::array_t<signed char>*>(v1.getArray());
            const ibis::array_t<signed char> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::UBYTE: {
            const ibis::array_t<unsigned char>& av2 =
                *static_cast<const ibis::array_t<unsigned char>*>(v2.getArray());
            ibis::array_t<unsigned char>& av0 =
                *static_cast<ibis::array_t<unsigned char>*>(v1.getArray());
            const ibis::array_t<unsigned char> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::SHORT: {
            const ibis::array_t<int16_t>& av2 =
                *static_cast<const ibis::array_t<int16_t>*>(v2.getArray());
            ibis::array_t<int16_t>& av0 =
                *static_cast<ibis::array_t<int16_t>*>(v1.getArray());
            const ibis::array_t<int16_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::USHORT: {
            const ibis::array_t<uint16_t>& av2 =
                *static_cast<const ibis::array_t<uint16_t>*>(v2.getArray());
            ibis::array_t<uint16_t>& av0 =
                *static_cast<ibis::array_t<uint16_t>*>(v1.getArray());
            const ibis::array_t<uint16_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::INT: {
            const ibis::array_t<int32_t>& av2 =
                *static_cast<const ibis::array_t<int32_t>*>(v2.getArray());
            ibis::array_t<int32_t>& av0 =
                *static_cast<ibis::array_t<int32_t>*>(v1.getArray());
            const ibis::array_t<int32_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::UINT: {
            const ibis::array_t<uint32_t>& av2 =
                *static_cast<const ibis::array_t<uint32_t>*>(v2.getArray());
            ibis::array_t<uint32_t>& av0 =
                *static_cast<ibis::array_t<uint32_t>*>(v1.getArray());
            const ibis::array_t<uint32_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::LONG: {
            const ibis::array_t<int64_t>& av2 =
                *static_cast<const ibis::array_t<int64_t>*>(v2.getArray());
            ibis::array_t<int64_t>& av0 =
                *static_cast<ibis::array_t<int64_t>*>(v1.getArray());
            const ibis::array_t<int64_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::ULONG: {
            const ibis::array_t<uint64_t>& av2 =
                *static_cast<const ibis::array_t<uint64_t>*>(v2.getArray());
            ibis::array_t<uint64_t>& av0 =
                *static_cast<ibis::array_t<uint64_t>*>(v1.getArray());
            const ibis::array_t<uint64_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::FLOAT: {
            const ibis::array_t<float>& av2 =
                *static_cast<const ibis::array_t<float>*>(v2.getArray());
            ibis::array_t<float>& av0 =
                *static_cast<ibis::array_t<float>*>(v1.getArray());
            const ibis::array_t<float> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::DOUBLE: {
            const ibis::array_t<double>& av2 =
                *static_cast<const ibis::array_t<double>*>(v2.getArray());
            ibis::array_t<double>& av0 =
                *static_cast<ibis::array_t<double>*>(v1.getArray());
            const ibis::array_t<double> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        }
        break;}
    case ibis::DOUBLE: {
        const ibis::array_t<double>& au2 =
            *static_cast<const ibis::array_t<double>*>(u2.getArray());
        ibis::array_t<double>& au0 =
            *static_cast<ibis::array_t<double>*>(u1.getArray());
        const ibis::array_t<double> au1(au0);
        au0.nosharing();
        switch(v1.type()) {
        default:
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bord::merge12T1 can not deal with v1 ("
                << v1.name() << ") of type "
                << ibis::TYPESTRING[(int)v1.type()];
            return ierr;
        case ibis::BYTE: {
            const ibis::array_t<signed char>& av2 =
                *static_cast<const ibis::array_t<signed char>*>(v2.getArray());
            ibis::array_t<signed char>& av0 =
                *static_cast<ibis::array_t<signed char>*>(v1.getArray());
            const ibis::array_t<signed char> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::UBYTE: {
            const ibis::array_t<unsigned char>& av2 =
                *static_cast<const ibis::array_t<unsigned char>*>(v2.getArray());
            ibis::array_t<unsigned char>& av0 =
                *static_cast<ibis::array_t<unsigned char>*>(v1.getArray());
            const ibis::array_t<unsigned char> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::SHORT: {
            const ibis::array_t<int16_t>& av2 =
                *static_cast<const ibis::array_t<int16_t>*>(v2.getArray());
            ibis::array_t<int16_t>& av0 =
                *static_cast<ibis::array_t<int16_t>*>(v1.getArray());
            const ibis::array_t<int16_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::USHORT: {
            const ibis::array_t<uint16_t>& av2 =
                *static_cast<const ibis::array_t<uint16_t>*>(v2.getArray());
            ibis::array_t<uint16_t>& av0 =
                *static_cast<ibis::array_t<uint16_t>*>(v1.getArray());
            const ibis::array_t<uint16_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::INT: {
            const ibis::array_t<int32_t>& av2 =
                *static_cast<const ibis::array_t<int32_t>*>(v2.getArray());
            ibis::array_t<int32_t>& av0 =
                *static_cast<ibis::array_t<int32_t>*>(v1.getArray());
            const ibis::array_t<int32_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::UINT: {
            const ibis::array_t<uint32_t>& av2 =
                *static_cast<const ibis::array_t<uint32_t>*>(v2.getArray());
            ibis::array_t<uint32_t>& av0 =
                *static_cast<ibis::array_t<uint32_t>*>(v1.getArray());
            const ibis::array_t<uint32_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::LONG: {
            const ibis::array_t<int64_t>& av2 =
                *static_cast<const ibis::array_t<int64_t>*>(v2.getArray());
            ibis::array_t<int64_t>& av0 =
                *static_cast<ibis::array_t<int64_t>*>(v1.getArray());
            const ibis::array_t<int64_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::ULONG: {
            const ibis::array_t<uint64_t>& av2 =
                *static_cast<const ibis::array_t<uint64_t>*>(v2.getArray());
            ibis::array_t<uint64_t>& av0 =
                *static_cast<ibis::array_t<uint64_t>*>(v1.getArray());
            const ibis::array_t<uint64_t> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::FLOAT: {
            const ibis::array_t<float>& av2 =
                *static_cast<const ibis::array_t<float>*>(v2.getArray());
            ibis::array_t<float>& av0 =
                *static_cast<ibis::array_t<float>*>(v1.getArray());
            const ibis::array_t<float> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::DOUBLE: {
            const ibis::array_t<double>& av2 =
                *static_cast<const ibis::array_t<double>*>(v2.getArray());
            ibis::array_t<double>& av0 =
                *static_cast<ibis::array_t<double>*>(v1.getArray());
            const ibis::array_t<double> av1(av0);
            av0.nosharing();
            ierr = merge12T(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        }
        break;}
    }
    return ierr;
} // ibis::bord::merge12T1

template <typename Tk, typename Tu, typename Tv> int
ibis::bord::merge12T(ibis::array_t<Tk> &kout,
                     ibis::array_t<Tu> &uout,
                     ibis::array_t<Tv> &vout,
                     const ibis::array_t<Tk> &kin1,
                     const ibis::array_t<Tu> &uin1,
                     const ibis::array_t<Tv> &vin1,
                     const ibis::array_t<Tk> &kin2,
                     const ibis::array_t<Tu> &uin2,
                     const ibis::array_t<Tv> &vin2,
                     ibis::selectClause::AGREGADO au,
                     ibis::selectClause::AGREGADO av) {
    kout.clear();
    uout.clear();
    vout.clear();
    int ierr = -1;
    if (kin1.size() != uin1.size() || kin1.size() != vin1.size() ||
        kin2.size() != uin2.size() || kin2.size() != vin2.size())
        return ierr;

    size_t j1 = 0;
    size_t j2 = 0;
    while (j1 < kin1.size() && j2 < kin2.size()) {
        if (kin1[j1] == kin2[j2]) { // same key value
            switch (au) {
            default:
                return ierr;
            case ibis::selectClause::CNT:
            case ibis::selectClause::SUM:
                uout.push_back(uin1[j1] + uin2[j2]);
                break;
            case ibis::selectClause::MAX:
                uout.push_back(uin1[j1] >= uin2[j2] ? uin1[j1] : uin2[j2]);
                break;
            case ibis::selectClause::MIN:
                uout.push_back(uin1[j1] <= uin2[j2] ? uin1[j1] : uin2[j2]);
                break;
            }
            switch (av) {
            default:
                return ierr;
            case ibis::selectClause::CNT:
            case ibis::selectClause::SUM:
                vout.push_back(vin1[j1] + vin2[j2]);
                break;
            case ibis::selectClause::MAX:
                vout.push_back(vin1[j1] >= vin2[j2] ? vin1[j1] : vin2[j2]);
                break;
            case ibis::selectClause::MIN:
                vout.push_back(vin1[j1] <= vin2[j2] ? vin1[j1] : vin2[j2]);
                break;
            }
            kout.push_back(kin1[j1]);
            ++ j1;
            ++ j2;
        }
        else if (kin1[j1] < kin2[j2]) {
            uout.push_back(uin1[j1]);
            vout.push_back(vin1[j1]);
            kout.push_back(kin1[j1]);
            ++ j1;
        }
        else {
            uout.push_back(uin2[j2]);
            vout.push_back(vin2[j2]);
            kout.push_back(kin2[j2]);
            ++ j2;
        }
    }

    while (j1 < kin1.size()) {
        kout.push_back(kin1[j1]);
        uout.push_back(uin1[j1]);
        vout.push_back(vin1[j1]);
        ++ j1;
    }
    while (j2 < kin2.size()) {
        kout.push_back(kin2[j2]);
        uout.push_back(uin2[j2]);
        vout.push_back(vin2[j2]);
        ++ j2;
    }
    return kout.size();
} // ibis::bord::merge12T

int
ibis::bord::merge12S1(std::vector<std::string> &kout,
                      const std::vector<std::string> &kin1,
                      const std::vector<std::string> &kin2,
                      ibis::bord::column &u1,
                      ibis::bord::column &v1,
                      const ibis::bord::column &u2,
                      const ibis::bord::column &v2,
                      ibis::selectClause::AGREGADO au,
                      ibis::selectClause::AGREGADO av) {
    int ierr = -1;
    if (u1.type() != u2.type() || v1.type() != v2.type())
        return ierr;

    switch (u1.type()) {
    default:
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bord::merge12S1 can not deal with u1 ("
            << u1.name() << ") of type "
            << ibis::TYPESTRING[(int)u1.type()];
        return ierr;
    case ibis::BYTE: {
        const ibis::array_t<signed char>& au2 =
            *static_cast<const ibis::array_t<signed char>*>(u2.getArray());
        ibis::array_t<signed char>& au0 =
            *static_cast<ibis::array_t<signed char>*>(u1.getArray());
        const ibis::array_t<signed char> au1(au0);
        au0.nosharing();
        switch(v1.type()) {
        default:
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bord::merge12S1 can not deal with v1 ("
                << v1.name() << ") of type "
                << ibis::TYPESTRING[(int)v1.type()];
            return ierr;
        case ibis::BYTE: {
            const ibis::array_t<signed char>& av2 =
                *static_cast<const ibis::array_t<signed char>*>(v2.getArray());
            ibis::array_t<signed char>& av0 =
                *static_cast<ibis::array_t<signed char>*>(v1.getArray());
            const ibis::array_t<signed char> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::UBYTE: {
            const ibis::array_t<unsigned char>& av2 =
                *static_cast<const ibis::array_t<unsigned char>*>(v2.getArray());
            ibis::array_t<unsigned char>& av0 =
                *static_cast<ibis::array_t<unsigned char>*>(v1.getArray());
            const ibis::array_t<unsigned char> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::SHORT: {
            const ibis::array_t<int16_t>& av2 =
                *static_cast<const ibis::array_t<int16_t>*>(v2.getArray());
            ibis::array_t<int16_t>& av0 =
                *static_cast<ibis::array_t<int16_t>*>(v1.getArray());
            const ibis::array_t<int16_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::USHORT: {
            const ibis::array_t<uint16_t>& av2 =
                *static_cast<const ibis::array_t<uint16_t>*>(v2.getArray());
            ibis::array_t<uint16_t>& av0 =
                *static_cast<ibis::array_t<uint16_t>*>(v1.getArray());
            const ibis::array_t<uint16_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::INT: {
            const ibis::array_t<int32_t>& av2 =
                *static_cast<const ibis::array_t<int32_t>*>(v2.getArray());
            ibis::array_t<int32_t>& av0 =
                *static_cast<ibis::array_t<int32_t>*>(v1.getArray());
            const ibis::array_t<int32_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::UINT: {
            const ibis::array_t<uint32_t>& av2 =
                *static_cast<const ibis::array_t<uint32_t>*>(v2.getArray());
            ibis::array_t<uint32_t>& av0 =
                *static_cast<ibis::array_t<uint32_t>*>(v1.getArray());
            const ibis::array_t<uint32_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::LONG: {
            const ibis::array_t<int64_t>& av2 =
                *static_cast<const ibis::array_t<int64_t>*>(v2.getArray());
            ibis::array_t<int64_t>& av0 =
                *static_cast<ibis::array_t<int64_t>*>(v1.getArray());
            const ibis::array_t<int64_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::ULONG: {
            const ibis::array_t<uint64_t>& av2 =
                *static_cast<const ibis::array_t<uint64_t>*>(v2.getArray());
            ibis::array_t<uint64_t>& av0 =
                *static_cast<ibis::array_t<uint64_t>*>(v1.getArray());
            const ibis::array_t<uint64_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::FLOAT: {
            const ibis::array_t<float>& av2 =
                *static_cast<const ibis::array_t<float>*>(v2.getArray());
            ibis::array_t<float>& av0 =
                *static_cast<ibis::array_t<float>*>(v1.getArray());
            const ibis::array_t<float> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::DOUBLE: {
            const ibis::array_t<double>& av2 =
                *static_cast<const ibis::array_t<double>*>(v2.getArray());
            ibis::array_t<double>& av0 =
                *static_cast<ibis::array_t<double>*>(v1.getArray());
            const ibis::array_t<double> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        }
        break;}
    case ibis::UBYTE: {
        const ibis::array_t<unsigned char>& au2 =
            *static_cast<const ibis::array_t<unsigned char>*>(u2.getArray());
        ibis::array_t<unsigned char>& au0 =
            *static_cast<ibis::array_t<unsigned char>*>(u1.getArray());
        const ibis::array_t<unsigned char> au1(au0);
        au0.nosharing();
        switch(v1.type()) {
        default:
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bord::merge12S1 can not deal with v1 ("
                << v1.name() << ") of type "
                << ibis::TYPESTRING[(int)v1.type()];
            return ierr;
        case ibis::BYTE: {
            const ibis::array_t<signed char>& av2 =
                *static_cast<const ibis::array_t<signed char>*>(v2.getArray());
            ibis::array_t<signed char>& av0 =
                *static_cast<ibis::array_t<signed char>*>(v1.getArray());
            const ibis::array_t<signed char> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::UBYTE: {
            const ibis::array_t<unsigned char>& av2 =
                *static_cast<const ibis::array_t<unsigned char>*>(v2.getArray());
            ibis::array_t<unsigned char>& av0 =
                *static_cast<ibis::array_t<unsigned char>*>(v1.getArray());
            const ibis::array_t<unsigned char> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::SHORT: {
            const ibis::array_t<int16_t>& av2 =
                *static_cast<const ibis::array_t<int16_t>*>(v2.getArray());
            ibis::array_t<int16_t>& av0 =
                *static_cast<ibis::array_t<int16_t>*>(v1.getArray());
            const ibis::array_t<int16_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::USHORT: {
            const ibis::array_t<uint16_t>& av2 =
                *static_cast<const ibis::array_t<uint16_t>*>(v2.getArray());
            ibis::array_t<uint16_t>& av0 =
                *static_cast<ibis::array_t<uint16_t>*>(v1.getArray());
            const ibis::array_t<uint16_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::INT: {
            const ibis::array_t<int32_t>& av2 =
                *static_cast<const ibis::array_t<int32_t>*>(v2.getArray());
            ibis::array_t<int32_t>& av0 =
                *static_cast<ibis::array_t<int32_t>*>(v1.getArray());
            const ibis::array_t<int32_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::UINT: {
            const ibis::array_t<uint32_t>& av2 =
                *static_cast<const ibis::array_t<uint32_t>*>(v2.getArray());
            ibis::array_t<uint32_t>& av0 =
                *static_cast<ibis::array_t<uint32_t>*>(v1.getArray());
            const ibis::array_t<uint32_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::LONG: {
            const ibis::array_t<int64_t>& av2 =
                *static_cast<const ibis::array_t<int64_t>*>(v2.getArray());
            ibis::array_t<int64_t>& av0 =
                *static_cast<ibis::array_t<int64_t>*>(v1.getArray());
            const ibis::array_t<int64_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::ULONG: {
            const ibis::array_t<uint64_t>& av2 =
                *static_cast<const ibis::array_t<uint64_t>*>(v2.getArray());
            ibis::array_t<uint64_t>& av0 =
                *static_cast<ibis::array_t<uint64_t>*>(v1.getArray());
            const ibis::array_t<uint64_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::FLOAT: {
            const ibis::array_t<float>& av2 =
                *static_cast<const ibis::array_t<float>*>(v2.getArray());
            ibis::array_t<float>& av0 =
                *static_cast<ibis::array_t<float>*>(v1.getArray());
            const ibis::array_t<float> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::DOUBLE: {
            const ibis::array_t<double>& av2 =
                *static_cast<const ibis::array_t<double>*>(v2.getArray());
            ibis::array_t<double>& av0 =
                *static_cast<ibis::array_t<double>*>(v1.getArray());
            const ibis::array_t<double> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        }
        break;}
    case ibis::SHORT: {
        const ibis::array_t<int16_t>& au2 =
            *static_cast<const ibis::array_t<int16_t>*>(u2.getArray());
        ibis::array_t<int16_t>& au0 =
            *static_cast<ibis::array_t<int16_t>*>(u1.getArray());
        const ibis::array_t<int16_t> au1(au0);
        au0.nosharing();
        switch(v1.type()) {
        default:
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bord::merge12S1 can not deal with v1 ("
                << v1.name() << ") of type "
                << ibis::TYPESTRING[(int)v1.type()];
            return ierr;
        case ibis::BYTE: {
            const ibis::array_t<signed char>& av2 =
                *static_cast<const ibis::array_t<signed char>*>(v2.getArray());
            ibis::array_t<signed char>& av0 =
                *static_cast<ibis::array_t<signed char>*>(v1.getArray());
            const ibis::array_t<signed char> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::UBYTE: {
            const ibis::array_t<unsigned char>& av2 =
                *static_cast<const ibis::array_t<unsigned char>*>(v2.getArray());
            ibis::array_t<unsigned char>& av0 =
                *static_cast<ibis::array_t<unsigned char>*>(v1.getArray());
            const ibis::array_t<unsigned char> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::SHORT: {
            const ibis::array_t<int16_t>& av2 =
                *static_cast<const ibis::array_t<int16_t>*>(v2.getArray());
            ibis::array_t<int16_t>& av0 =
                *static_cast<ibis::array_t<int16_t>*>(v1.getArray());
            const ibis::array_t<int16_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::USHORT: {
            const ibis::array_t<uint16_t>& av2 =
                *static_cast<const ibis::array_t<uint16_t>*>(v2.getArray());
            ibis::array_t<uint16_t>& av0 =
                *static_cast<ibis::array_t<uint16_t>*>(v1.getArray());
            const ibis::array_t<uint16_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::INT: {
            const ibis::array_t<int32_t>& av2 =
                *static_cast<const ibis::array_t<int32_t>*>(v2.getArray());
            ibis::array_t<int32_t>& av0 =
                *static_cast<ibis::array_t<int32_t>*>(v1.getArray());
            const ibis::array_t<int32_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::UINT: {
            const ibis::array_t<uint32_t>& av2 =
                *static_cast<const ibis::array_t<uint32_t>*>(v2.getArray());
            ibis::array_t<uint32_t>& av0 =
                *static_cast<ibis::array_t<uint32_t>*>(v1.getArray());
            const ibis::array_t<uint32_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::LONG: {
            const ibis::array_t<int64_t>& av2 =
                *static_cast<const ibis::array_t<int64_t>*>(v2.getArray());
            ibis::array_t<int64_t>& av0 =
                *static_cast<ibis::array_t<int64_t>*>(v1.getArray());
            const ibis::array_t<int64_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::ULONG: {
            const ibis::array_t<uint64_t>& av2 =
                *static_cast<const ibis::array_t<uint64_t>*>(v2.getArray());
            ibis::array_t<uint64_t>& av0 =
                *static_cast<ibis::array_t<uint64_t>*>(v1.getArray());
            const ibis::array_t<uint64_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::FLOAT: {
            const ibis::array_t<float>& av2 =
                *static_cast<const ibis::array_t<float>*>(v2.getArray());
            ibis::array_t<float>& av0 =
                *static_cast<ibis::array_t<float>*>(v1.getArray());
            const ibis::array_t<float> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::DOUBLE: {
            const ibis::array_t<double>& av2 =
                *static_cast<const ibis::array_t<double>*>(v2.getArray());
            ibis::array_t<double>& av0 =
                *static_cast<ibis::array_t<double>*>(v1.getArray());
            const ibis::array_t<double> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        }
        break;}
    case ibis::USHORT: {
        const ibis::array_t<uint16_t>& au2 =
            *static_cast<const ibis::array_t<uint16_t>*>(u2.getArray());
        ibis::array_t<uint16_t>& au0 =
            *static_cast<ibis::array_t<uint16_t>*>(u1.getArray());
        const ibis::array_t<uint16_t> au1(au0);
        au0.nosharing();
        switch(v1.type()) {
        default:
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bord::merge12S1 can not deal with v1 ("
                << v1.name() << ") of type "
                << ibis::TYPESTRING[(int)v1.type()];
            return ierr;
        case ibis::BYTE: {
            const ibis::array_t<signed char>& av2 =
                *static_cast<const ibis::array_t<signed char>*>(v2.getArray());
            ibis::array_t<signed char>& av0 =
                *static_cast<ibis::array_t<signed char>*>(v1.getArray());
            const ibis::array_t<signed char> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::UBYTE: {
            const ibis::array_t<unsigned char>& av2 =
                *static_cast<const ibis::array_t<unsigned char>*>(v2.getArray());
            ibis::array_t<unsigned char>& av0 =
                *static_cast<ibis::array_t<unsigned char>*>(v1.getArray());
            const ibis::array_t<unsigned char> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::SHORT: {
            const ibis::array_t<int16_t>& av2 =
                *static_cast<const ibis::array_t<int16_t>*>(v2.getArray());
            ibis::array_t<int16_t>& av0 =
                *static_cast<ibis::array_t<int16_t>*>(v1.getArray());
            const ibis::array_t<int16_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::USHORT: {
            const ibis::array_t<uint16_t>& av2 =
                *static_cast<const ibis::array_t<uint16_t>*>(v2.getArray());
            ibis::array_t<uint16_t>& av0 =
                *static_cast<ibis::array_t<uint16_t>*>(v1.getArray());
            const ibis::array_t<uint16_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::INT: {
            const ibis::array_t<int32_t>& av2 =
                *static_cast<const ibis::array_t<int32_t>*>(v2.getArray());
            ibis::array_t<int32_t>& av0 =
                *static_cast<ibis::array_t<int32_t>*>(v1.getArray());
            const ibis::array_t<int32_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::UINT: {
            const ibis::array_t<uint32_t>& av2 =
                *static_cast<const ibis::array_t<uint32_t>*>(v2.getArray());
            ibis::array_t<uint32_t>& av0 =
                *static_cast<ibis::array_t<uint32_t>*>(v1.getArray());
            const ibis::array_t<uint32_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::LONG: {
            const ibis::array_t<int64_t>& av2 =
                *static_cast<const ibis::array_t<int64_t>*>(v2.getArray());
            ibis::array_t<int64_t>& av0 =
                *static_cast<ibis::array_t<int64_t>*>(v1.getArray());
            const ibis::array_t<int64_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::ULONG: {
            const ibis::array_t<uint64_t>& av2 =
                *static_cast<const ibis::array_t<uint64_t>*>(v2.getArray());
            ibis::array_t<uint64_t>& av0 =
                *static_cast<ibis::array_t<uint64_t>*>(v1.getArray());
            const ibis::array_t<uint64_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::FLOAT: {
            const ibis::array_t<float>& av2 =
                *static_cast<const ibis::array_t<float>*>(v2.getArray());
            ibis::array_t<float>& av0 =
                *static_cast<ibis::array_t<float>*>(v1.getArray());
            const ibis::array_t<float> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::DOUBLE: {
            const ibis::array_t<double>& av2 =
                *static_cast<const ibis::array_t<double>*>(v2.getArray());
            ibis::array_t<double>& av0 =
                *static_cast<ibis::array_t<double>*>(v1.getArray());
            const ibis::array_t<double> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        }
        break;}
    case ibis::INT: {
        const ibis::array_t<int32_t>& au2 =
            *static_cast<const ibis::array_t<int32_t>*>(u2.getArray());
        ibis::array_t<int32_t>& au0 =
            *static_cast<ibis::array_t<int32_t>*>(u1.getArray());
        const ibis::array_t<int32_t> au1(au0);
        au0.nosharing();
        switch(v1.type()) {
        default:
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bord::merge12S1 can not deal with v1 ("
                << v1.name() << ") of type "
                << ibis::TYPESTRING[(int)v1.type()];
            return ierr;
        case ibis::BYTE: {
            const ibis::array_t<signed char>& av2 =
                *static_cast<const ibis::array_t<signed char>*>(v2.getArray());
            ibis::array_t<signed char>& av0 =
                *static_cast<ibis::array_t<signed char>*>(v1.getArray());
            const ibis::array_t<signed char> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::UBYTE: {
            const ibis::array_t<unsigned char>& av2 =
                *static_cast<const ibis::array_t<unsigned char>*>(v2.getArray());
            ibis::array_t<unsigned char>& av0 =
                *static_cast<ibis::array_t<unsigned char>*>(v1.getArray());
            const ibis::array_t<unsigned char> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::SHORT: {
            const ibis::array_t<int16_t>& av2 =
                *static_cast<const ibis::array_t<int16_t>*>(v2.getArray());
            ibis::array_t<int16_t>& av0 =
                *static_cast<ibis::array_t<int16_t>*>(v1.getArray());
            const ibis::array_t<int16_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::USHORT: {
            const ibis::array_t<uint16_t>& av2 =
                *static_cast<const ibis::array_t<uint16_t>*>(v2.getArray());
            ibis::array_t<uint16_t>& av0 =
                *static_cast<ibis::array_t<uint16_t>*>(v1.getArray());
            const ibis::array_t<uint16_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::INT: {
            const ibis::array_t<int32_t>& av2 =
                *static_cast<const ibis::array_t<int32_t>*>(v2.getArray());
            ibis::array_t<int32_t>& av0 =
                *static_cast<ibis::array_t<int32_t>*>(v1.getArray());
            const ibis::array_t<int32_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::UINT: {
            const ibis::array_t<uint32_t>& av2 =
                *static_cast<const ibis::array_t<uint32_t>*>(v2.getArray());
            ibis::array_t<uint32_t>& av0 =
                *static_cast<ibis::array_t<uint32_t>*>(v1.getArray());
            const ibis::array_t<uint32_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::LONG: {
            const ibis::array_t<int64_t>& av2 =
                *static_cast<const ibis::array_t<int64_t>*>(v2.getArray());
            ibis::array_t<int64_t>& av0 =
                *static_cast<ibis::array_t<int64_t>*>(v1.getArray());
            const ibis::array_t<int64_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::ULONG: {
            const ibis::array_t<uint64_t>& av2 =
                *static_cast<const ibis::array_t<uint64_t>*>(v2.getArray());
            ibis::array_t<uint64_t>& av0 =
                *static_cast<ibis::array_t<uint64_t>*>(v1.getArray());
            const ibis::array_t<uint64_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::FLOAT: {
            const ibis::array_t<float>& av2 =
                *static_cast<const ibis::array_t<float>*>(v2.getArray());
            ibis::array_t<float>& av0 =
                *static_cast<ibis::array_t<float>*>(v1.getArray());
            const ibis::array_t<float> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::DOUBLE: {
            const ibis::array_t<double>& av2 =
                *static_cast<const ibis::array_t<double>*>(v2.getArray());
            ibis::array_t<double>& av0 =
                *static_cast<ibis::array_t<double>*>(v1.getArray());
            const ibis::array_t<double> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        }
        break;}
    case ibis::UINT: {
        const ibis::array_t<uint32_t>& au2 =
            *static_cast<const ibis::array_t<uint32_t>*>(u2.getArray());
        ibis::array_t<uint32_t>& au0 =
            *static_cast<ibis::array_t<uint32_t>*>(u1.getArray());
        const ibis::array_t<uint32_t> au1(au0);
        au0.nosharing();
        switch(v1.type()) {
        default:
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bord::merge12S1 can not deal with v1 ("
                << v1.name() << ") of type "
                << ibis::TYPESTRING[(int)v1.type()];
            return ierr;
        case ibis::BYTE: {
            const ibis::array_t<signed char>& av2 =
                *static_cast<const ibis::array_t<signed char>*>(v2.getArray());
            ibis::array_t<signed char>& av0 =
                *static_cast<ibis::array_t<signed char>*>(v1.getArray());
            const ibis::array_t<signed char> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::UBYTE: {
            const ibis::array_t<unsigned char>& av2 =
                *static_cast<const ibis::array_t<unsigned char>*>(v2.getArray());
            ibis::array_t<unsigned char>& av0 =
                *static_cast<ibis::array_t<unsigned char>*>(v1.getArray());
            const ibis::array_t<unsigned char> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::SHORT: {
            const ibis::array_t<int16_t>& av2 =
                *static_cast<const ibis::array_t<int16_t>*>(v2.getArray());
            ibis::array_t<int16_t>& av0 =
                *static_cast<ibis::array_t<int16_t>*>(v1.getArray());
            const ibis::array_t<int16_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::USHORT: {
            const ibis::array_t<uint16_t>& av2 =
                *static_cast<const ibis::array_t<uint16_t>*>(v2.getArray());
            ibis::array_t<uint16_t>& av0 =
                *static_cast<ibis::array_t<uint16_t>*>(v1.getArray());
            const ibis::array_t<uint16_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::INT: {
            const ibis::array_t<int32_t>& av2 =
                *static_cast<const ibis::array_t<int32_t>*>(v2.getArray());
            ibis::array_t<int32_t>& av0 =
                *static_cast<ibis::array_t<int32_t>*>(v1.getArray());
            const ibis::array_t<int32_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::UINT: {
            const ibis::array_t<uint32_t>& av2 =
                *static_cast<const ibis::array_t<uint32_t>*>(v2.getArray());
            ibis::array_t<uint32_t>& av0 =
                *static_cast<ibis::array_t<uint32_t>*>(v1.getArray());
            const ibis::array_t<uint32_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::LONG: {
            const ibis::array_t<int64_t>& av2 =
                *static_cast<const ibis::array_t<int64_t>*>(v2.getArray());
            ibis::array_t<int64_t>& av0 =
                *static_cast<ibis::array_t<int64_t>*>(v1.getArray());
            const ibis::array_t<int64_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::ULONG: {
            const ibis::array_t<uint64_t>& av2 =
                *static_cast<const ibis::array_t<uint64_t>*>(v2.getArray());
            ibis::array_t<uint64_t>& av0 =
                *static_cast<ibis::array_t<uint64_t>*>(v1.getArray());
            const ibis::array_t<uint64_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::FLOAT: {
            const ibis::array_t<float>& av2 =
                *static_cast<const ibis::array_t<float>*>(v2.getArray());
            ibis::array_t<float>& av0 =
                *static_cast<ibis::array_t<float>*>(v1.getArray());
            const ibis::array_t<float> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::DOUBLE: {
            const ibis::array_t<double>& av2 =
                *static_cast<const ibis::array_t<double>*>(v2.getArray());
            ibis::array_t<double>& av0 =
                *static_cast<ibis::array_t<double>*>(v1.getArray());
            const ibis::array_t<double> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        }
        break;}
    case ibis::LONG: {
        const ibis::array_t<int64_t>& au2 =
            *static_cast<const ibis::array_t<int64_t>*>(u2.getArray());
        ibis::array_t<int64_t>& au0 =
            *static_cast<ibis::array_t<int64_t>*>(u1.getArray());
        const ibis::array_t<int64_t> au1(au0);
        au0.nosharing();
        switch(v1.type()) {
        default:
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bord::merge12S1 can not deal with v1 ("
                << v1.name() << ") of type "
                << ibis::TYPESTRING[(int)v1.type()];
            return ierr;
        case ibis::BYTE: {
            const ibis::array_t<signed char>& av2 =
                *static_cast<const ibis::array_t<signed char>*>(v2.getArray());
            ibis::array_t<signed char>& av0 =
                *static_cast<ibis::array_t<signed char>*>(v1.getArray());
            const ibis::array_t<signed char> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::UBYTE: {
            const ibis::array_t<unsigned char>& av2 =
                *static_cast<const ibis::array_t<unsigned char>*>(v2.getArray());
            ibis::array_t<unsigned char>& av0 =
                *static_cast<ibis::array_t<unsigned char>*>(v1.getArray());
            const ibis::array_t<unsigned char> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::SHORT: {
            const ibis::array_t<int16_t>& av2 =
                *static_cast<const ibis::array_t<int16_t>*>(v2.getArray());
            ibis::array_t<int16_t>& av0 =
                *static_cast<ibis::array_t<int16_t>*>(v1.getArray());
            const ibis::array_t<int16_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::USHORT: {
            const ibis::array_t<uint16_t>& av2 =
                *static_cast<const ibis::array_t<uint16_t>*>(v2.getArray());
            ibis::array_t<uint16_t>& av0 =
                *static_cast<ibis::array_t<uint16_t>*>(v1.getArray());
            const ibis::array_t<uint16_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::INT: {
            const ibis::array_t<int32_t>& av2 =
                *static_cast<const ibis::array_t<int32_t>*>(v2.getArray());
            ibis::array_t<int32_t>& av0 =
                *static_cast<ibis::array_t<int32_t>*>(v1.getArray());
            const ibis::array_t<int32_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::UINT: {
            const ibis::array_t<uint32_t>& av2 =
                *static_cast<const ibis::array_t<uint32_t>*>(v2.getArray());
            ibis::array_t<uint32_t>& av0 =
                *static_cast<ibis::array_t<uint32_t>*>(v1.getArray());
            const ibis::array_t<uint32_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::LONG: {
            const ibis::array_t<int64_t>& av2 =
                *static_cast<const ibis::array_t<int64_t>*>(v2.getArray());
            ibis::array_t<int64_t>& av0 =
                *static_cast<ibis::array_t<int64_t>*>(v1.getArray());
            const ibis::array_t<int64_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::ULONG: {
            const ibis::array_t<uint64_t>& av2 =
                *static_cast<const ibis::array_t<uint64_t>*>(v2.getArray());
            ibis::array_t<uint64_t>& av0 =
                *static_cast<ibis::array_t<uint64_t>*>(v1.getArray());
            const ibis::array_t<uint64_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::FLOAT: {
            const ibis::array_t<float>& av2 =
                *static_cast<const ibis::array_t<float>*>(v2.getArray());
            ibis::array_t<float>& av0 =
                *static_cast<ibis::array_t<float>*>(v1.getArray());
            const ibis::array_t<float> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::DOUBLE: {
            const ibis::array_t<double>& av2 =
                *static_cast<const ibis::array_t<double>*>(v2.getArray());
            ibis::array_t<double>& av0 =
                *static_cast<ibis::array_t<double>*>(v1.getArray());
            const ibis::array_t<double> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        }
        break;}
    case ibis::ULONG: {
        const ibis::array_t<uint64_t>& au2 =
            *static_cast<const ibis::array_t<uint64_t>*>(u2.getArray());
        ibis::array_t<uint64_t>& au0 =
            *static_cast<ibis::array_t<uint64_t>*>(u1.getArray());
        const ibis::array_t<uint64_t> au1(au0);
        au0.nosharing();
        switch(v1.type()) {
        default:
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bord::merge12S1 can not deal with v1 ("
                << v1.name() << ") of type "
                << ibis::TYPESTRING[(int)v1.type()];
            return ierr;
        case ibis::BYTE: {
            const ibis::array_t<signed char>& av2 =
                *static_cast<const ibis::array_t<signed char>*>(v2.getArray());
            ibis::array_t<signed char>& av0 =
                *static_cast<ibis::array_t<signed char>*>(v1.getArray());
            const ibis::array_t<signed char> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::UBYTE: {
            const ibis::array_t<unsigned char>& av2 =
                *static_cast<const ibis::array_t<unsigned char>*>(v2.getArray());
            ibis::array_t<unsigned char>& av0 =
                *static_cast<ibis::array_t<unsigned char>*>(v1.getArray());
            const ibis::array_t<unsigned char> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::SHORT: {
            const ibis::array_t<int16_t>& av2 =
                *static_cast<const ibis::array_t<int16_t>*>(v2.getArray());
            ibis::array_t<int16_t>& av0 =
                *static_cast<ibis::array_t<int16_t>*>(v1.getArray());
            const ibis::array_t<int16_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::USHORT: {
            const ibis::array_t<uint16_t>& av2 =
                *static_cast<const ibis::array_t<uint16_t>*>(v2.getArray());
            ibis::array_t<uint16_t>& av0 =
                *static_cast<ibis::array_t<uint16_t>*>(v1.getArray());
            const ibis::array_t<uint16_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::INT: {
            const ibis::array_t<int32_t>& av2 =
                *static_cast<const ibis::array_t<int32_t>*>(v2.getArray());
            ibis::array_t<int32_t>& av0 =
                *static_cast<ibis::array_t<int32_t>*>(v1.getArray());
            const ibis::array_t<int32_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::UINT: {
            const ibis::array_t<uint32_t>& av2 =
                *static_cast<const ibis::array_t<uint32_t>*>(v2.getArray());
            ibis::array_t<uint32_t>& av0 =
                *static_cast<ibis::array_t<uint32_t>*>(v1.getArray());
            const ibis::array_t<uint32_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::LONG: {
            const ibis::array_t<int64_t>& av2 =
                *static_cast<const ibis::array_t<int64_t>*>(v2.getArray());
            ibis::array_t<int64_t>& av0 =
                *static_cast<ibis::array_t<int64_t>*>(v1.getArray());
            const ibis::array_t<int64_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::ULONG: {
            const ibis::array_t<uint64_t>& av2 =
                *static_cast<const ibis::array_t<uint64_t>*>(v2.getArray());
            ibis::array_t<uint64_t>& av0 =
                *static_cast<ibis::array_t<uint64_t>*>(v1.getArray());
            const ibis::array_t<uint64_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::FLOAT: {
            const ibis::array_t<float>& av2 =
                *static_cast<const ibis::array_t<float>*>(v2.getArray());
            ibis::array_t<float>& av0 =
                *static_cast<ibis::array_t<float>*>(v1.getArray());
            const ibis::array_t<float> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::DOUBLE: {
            const ibis::array_t<double>& av2 =
                *static_cast<const ibis::array_t<double>*>(v2.getArray());
            ibis::array_t<double>& av0 =
                *static_cast<ibis::array_t<double>*>(v1.getArray());
            const ibis::array_t<double> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        }
        break;}
    case ibis::FLOAT: {
        const ibis::array_t<float>& au2 =
            *static_cast<const ibis::array_t<float>*>(u2.getArray());
        ibis::array_t<float>& au0 =
            *static_cast<ibis::array_t<float>*>(u1.getArray());
        const ibis::array_t<float> au1(au0);
        au0.nosharing();
        switch(v1.type()) {
        default:
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bord::merge12S1 can not deal with v1 ("
                << v1.name() << ") of type "
                << ibis::TYPESTRING[(int)v1.type()];
            return ierr;
        case ibis::BYTE: {
            const ibis::array_t<signed char>& av2 =
                *static_cast<const ibis::array_t<signed char>*>(v2.getArray());
            ibis::array_t<signed char>& av0 =
                *static_cast<ibis::array_t<signed char>*>(v1.getArray());
            const ibis::array_t<signed char> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::UBYTE: {
            const ibis::array_t<unsigned char>& av2 =
                *static_cast<const ibis::array_t<unsigned char>*>(v2.getArray());
            ibis::array_t<unsigned char>& av0 =
                *static_cast<ibis::array_t<unsigned char>*>(v1.getArray());
            const ibis::array_t<unsigned char> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::SHORT: {
            const ibis::array_t<int16_t>& av2 =
                *static_cast<const ibis::array_t<int16_t>*>(v2.getArray());
            ibis::array_t<int16_t>& av0 =
                *static_cast<ibis::array_t<int16_t>*>(v1.getArray());
            const ibis::array_t<int16_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::USHORT: {
            const ibis::array_t<uint16_t>& av2 =
                *static_cast<const ibis::array_t<uint16_t>*>(v2.getArray());
            ibis::array_t<uint16_t>& av0 =
                *static_cast<ibis::array_t<uint16_t>*>(v1.getArray());
            const ibis::array_t<uint16_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::INT: {
            const ibis::array_t<int32_t>& av2 =
                *static_cast<const ibis::array_t<int32_t>*>(v2.getArray());
            ibis::array_t<int32_t>& av0 =
                *static_cast<ibis::array_t<int32_t>*>(v1.getArray());
            const ibis::array_t<int32_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::UINT: {
            const ibis::array_t<uint32_t>& av2 =
                *static_cast<const ibis::array_t<uint32_t>*>(v2.getArray());
            ibis::array_t<uint32_t>& av0 =
                *static_cast<ibis::array_t<uint32_t>*>(v1.getArray());
            const ibis::array_t<uint32_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::LONG: {
            const ibis::array_t<int64_t>& av2 =
                *static_cast<const ibis::array_t<int64_t>*>(v2.getArray());
            ibis::array_t<int64_t>& av0 =
                *static_cast<ibis::array_t<int64_t>*>(v1.getArray());
            const ibis::array_t<int64_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::ULONG: {
            const ibis::array_t<uint64_t>& av2 =
                *static_cast<const ibis::array_t<uint64_t>*>(v2.getArray());
            ibis::array_t<uint64_t>& av0 =
                *static_cast<ibis::array_t<uint64_t>*>(v1.getArray());
            const ibis::array_t<uint64_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::FLOAT: {
            const ibis::array_t<float>& av2 =
                *static_cast<const ibis::array_t<float>*>(v2.getArray());
            ibis::array_t<float>& av0 =
                *static_cast<ibis::array_t<float>*>(v1.getArray());
            const ibis::array_t<float> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::DOUBLE: {
            const ibis::array_t<double>& av2 =
                *static_cast<const ibis::array_t<double>*>(v2.getArray());
            ibis::array_t<double>& av0 =
                *static_cast<ibis::array_t<double>*>(v1.getArray());
            const ibis::array_t<double> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        }
        break;}
    case ibis::DOUBLE: {
        const ibis::array_t<double>& au2 =
            *static_cast<const ibis::array_t<double>*>(u2.getArray());
        ibis::array_t<double>& au0 =
            *static_cast<ibis::array_t<double>*>(u1.getArray());
        const ibis::array_t<double> au1(au0);
        au0.nosharing();
        switch(v1.type()) {
        default:
            LOGGER(ibis::gVerbose > 0)
                << "Warning -- bord::merge12S1 can not deal with v1 ("
                << v1.name() << ") of type "
                << ibis::TYPESTRING[(int)v1.type()];
            return ierr;
        case ibis::BYTE: {
            const ibis::array_t<signed char>& av2 =
                *static_cast<const ibis::array_t<signed char>*>(v2.getArray());
            ibis::array_t<signed char>& av0 =
                *static_cast<ibis::array_t<signed char>*>(v1.getArray());
            const ibis::array_t<signed char> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::UBYTE: {
            const ibis::array_t<unsigned char>& av2 =
                *static_cast<const ibis::array_t<unsigned char>*>(v2.getArray());
            ibis::array_t<unsigned char>& av0 =
                *static_cast<ibis::array_t<unsigned char>*>(v1.getArray());
            const ibis::array_t<unsigned char> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::SHORT: {
            const ibis::array_t<int16_t>& av2 =
                *static_cast<const ibis::array_t<int16_t>*>(v2.getArray());
            ibis::array_t<int16_t>& av0 =
                *static_cast<ibis::array_t<int16_t>*>(v1.getArray());
            const ibis::array_t<int16_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::USHORT: {
            const ibis::array_t<uint16_t>& av2 =
                *static_cast<const ibis::array_t<uint16_t>*>(v2.getArray());
            ibis::array_t<uint16_t>& av0 =
                *static_cast<ibis::array_t<uint16_t>*>(v1.getArray());
            const ibis::array_t<uint16_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::INT: {
            const ibis::array_t<int32_t>& av2 =
                *static_cast<const ibis::array_t<int32_t>*>(v2.getArray());
            ibis::array_t<int32_t>& av0 =
                *static_cast<ibis::array_t<int32_t>*>(v1.getArray());
            const ibis::array_t<int32_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::UINT: {
            const ibis::array_t<uint32_t>& av2 =
                *static_cast<const ibis::array_t<uint32_t>*>(v2.getArray());
            ibis::array_t<uint32_t>& av0 =
                *static_cast<ibis::array_t<uint32_t>*>(v1.getArray());
            const ibis::array_t<uint32_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::LONG: {
            const ibis::array_t<int64_t>& av2 =
                *static_cast<const ibis::array_t<int64_t>*>(v2.getArray());
            ibis::array_t<int64_t>& av0 =
                *static_cast<ibis::array_t<int64_t>*>(v1.getArray());
            const ibis::array_t<int64_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::ULONG: {
            const ibis::array_t<uint64_t>& av2 =
                *static_cast<const ibis::array_t<uint64_t>*>(v2.getArray());
            ibis::array_t<uint64_t>& av0 =
                *static_cast<ibis::array_t<uint64_t>*>(v1.getArray());
            const ibis::array_t<uint64_t> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::FLOAT: {
            const ibis::array_t<float>& av2 =
                *static_cast<const ibis::array_t<float>*>(v2.getArray());
            ibis::array_t<float>& av0 =
                *static_cast<ibis::array_t<float>*>(v1.getArray());
            const ibis::array_t<float> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        case ibis::DOUBLE: {
            const ibis::array_t<double>& av2 =
                *static_cast<const ibis::array_t<double>*>(v2.getArray());
            ibis::array_t<double>& av0 =
                *static_cast<ibis::array_t<double>*>(v1.getArray());
            const ibis::array_t<double> av1(av0);
            av0.nosharing();
            ierr = merge12S(kout, au0, av0, kin1, au1, av1, kin2, au2, av2,
                            au, av);
            break;}
        }
        break;}
    }
    return ierr;
} // ibis::bord::merge12S1

template <typename Tu, typename Tv> int
ibis::bord::merge12S(std::vector<std::string> &kout,
                     ibis::array_t<Tu> &uout,
                     ibis::array_t<Tv> &vout,
                     const std::vector<std::string> &kin1,
                     const ibis::array_t<Tu> &uin1,
                     const ibis::array_t<Tv> &vin1,
                     const std::vector<std::string> &kin2,
                     const ibis::array_t<Tu> &uin2,
                     const ibis::array_t<Tv> &vin2,
                     ibis::selectClause::AGREGADO au,
                     ibis::selectClause::AGREGADO av) {
    kout.clear();
    uout.clear();
    vout.clear();
    int ierr = -1;
    if (kin1.size() != uin1.size() || kin1.size() != vin1.size() ||
        kin2.size() != uin2.size() || kin2.size() != vin2.size())
        return ierr;

    size_t j1 = 0;
    size_t j2 = 0;
    while (j1 < kin1.size() && j2 < kin2.size()) {
        if (kin1[j1] == kin2[j2]) { // same key value
            switch (au) {
            default:
                return ierr;
            case ibis::selectClause::CNT:
            case ibis::selectClause::SUM:
                uout.push_back(uin1[j1] + uin2[j2]);
                break;
            case ibis::selectClause::MAX:
                uout.push_back(uin1[j1] >= uin2[j2] ? uin1[j1] : uin2[j2]);
                break;
            case ibis::selectClause::MIN:
                uout.push_back(uin1[j1] <= uin2[j2] ? uin1[j1] : uin2[j2]);
                break;
            }
            switch (av) {
            default:
                return ierr;
            case ibis::selectClause::CNT:
            case ibis::selectClause::SUM:
                vout.push_back(vin1[j1] + vin2[j2]);
                break;
            case ibis::selectClause::MAX:
                vout.push_back(vin1[j1] >= vin2[j2] ? vin1[j1] : vin2[j2]);
                break;
            case ibis::selectClause::MIN:
                vout.push_back(vin1[j1] <= vin2[j2] ? vin1[j1] : vin2[j2]);
                break;
            }
            kout.push_back(kin1[j1]);
            ++ j1;
            ++ j2;
        }
        else if (kin1[j1] < kin2[j2]) {
            uout.push_back(uin1[j1]);
            vout.push_back(vin1[j1]);
            kout.push_back(kin1[j1]);
            ++ j1;
        }
        else {
            uout.push_back(uin2[j2]);
            vout.push_back(vin2[j2]);
            kout.push_back(kin2[j2]);
            ++ j2;
        }
    }

    while (j1 < kin1.size()) {
        kout.push_back(kin1[j1]);
        uout.push_back(uin1[j1]);
        vout.push_back(vin1[j1]);
        ++ j1;
    }
    while (j2 < kin2.size()) {
        kout.push_back(kin2[j2]);
        uout.push_back(uin2[j2]);
        vout.push_back(vin2[j2]);
        ++ j2;
    }
    return kout.size();
} // ibis::bord::merge12S

/// Merge with two key columns and arbitrary number of value columns.
int ibis::bord::merge20(ibis::bord::column &k11,
                        ibis::bord::column &k21,
                        std::vector<ibis::bord::column*> &v1,
                        const ibis::bord::column &k12,
                        const ibis::bord::column &k22,
                        const std::vector<ibis::bord::column*> &v2,
                        const std::vector<ibis::selectClause::AGREGADO> &agg) {
    int ierr = -1;
    if (k11.type() != k12.type()) return ierr;

    switch (k11.type()) {
    default:
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bord::merge20 can not deal with k11 ("
            << k11.name() << ") of type "
            << ibis::TYPESTRING[(int)k11.type()];
        return ierr;
    case ibis::BYTE: {
        const ibis::array_t<signed char> &ak12 =
            *static_cast<const ibis::array_t<signed char>*>(k12.getArray());
        ibis::array_t<signed char> &ak10 =
            *static_cast<ibis::array_t<signed char>*>(k11.getArray());
        const ibis::array_t<signed char> ak11(ak10);
        ak10.nosharing();
        ierr = merge20T1(ak10, ak11, ak12, k21, v1, k22, v2, agg);
        break;}
    case ibis::UBYTE: {
        const ibis::array_t<unsigned char> &ak12 =
            *static_cast<const ibis::array_t<unsigned char>*>(k12.getArray());
        ibis::array_t<unsigned char> &ak10 =
            *static_cast<ibis::array_t<unsigned char>*>(k11.getArray());
        const ibis::array_t<unsigned char> ak11(ak10);
        ak10.nosharing();
        ierr = merge20T1(ak10, ak11, ak12, k21, v1, k22, v2, agg);
        break;}
    case ibis::SHORT: {
        const ibis::array_t<int16_t> &ak12 =
            *static_cast<const ibis::array_t<int16_t>*>(k12.getArray());
        ibis::array_t<int16_t> &ak10 =
            *static_cast<ibis::array_t<int16_t>*>(k11.getArray());
        const ibis::array_t<int16_t> ak11(ak10);
        ak10.nosharing();
        ierr = merge20T1(ak10, ak11, ak12, k21, v1, k22, v2, agg);
        break;}
    case ibis::USHORT: {
        const ibis::array_t<uint16_t> &ak12 =
            *static_cast<const ibis::array_t<uint16_t>*>(k12.getArray());
        ibis::array_t<uint16_t> &ak10 =
            *static_cast<ibis::array_t<uint16_t>*>(k11.getArray());
        const ibis::array_t<uint16_t> ak11(ak10);
        ak10.nosharing();
        ierr = merge20T1(ak10, ak11, ak12, k21, v1, k22, v2, agg);
        break;}
    case ibis::INT: {
        const ibis::array_t<int32_t> &ak12 =
            *static_cast<const ibis::array_t<int32_t>*>(k12.getArray());
        ibis::array_t<int32_t> &ak10 =
            *static_cast<ibis::array_t<int32_t>*>(k11.getArray());
        const ibis::array_t<int32_t> ak11(ak10);
        ak10.nosharing();
        ierr = merge20T1(ak10, ak11, ak12, k21, v1, k22, v2, agg);
        break;}
    case ibis::UINT: {
        const ibis::array_t<uint32_t> &ak12 =
            *static_cast<const ibis::array_t<uint32_t>*>(k12.getArray());
        ibis::array_t<uint32_t> &ak10 =
            *static_cast<ibis::array_t<uint32_t>*>(k11.getArray());
        const ibis::array_t<uint32_t> ak11(ak10);
        ak10.nosharing();
        ierr = merge20T1(ak10, ak11, ak12, k21, v1, k22, v2, agg);
        break;}
    case ibis::LONG: {
        const ibis::array_t<int64_t> &ak12 =
            *static_cast<const ibis::array_t<int64_t>*>(k12.getArray());
        ibis::array_t<int64_t> &ak10 =
            *static_cast<ibis::array_t<int64_t>*>(k11.getArray());
        const ibis::array_t<int64_t> ak11(ak10);
        ak10.nosharing();
        ierr = merge20T1(ak10, ak11, ak12, k21, v1, k22, v2, agg);
        break;}
    case ibis::ULONG: {
        const ibis::array_t<uint64_t> &ak12 =
            *static_cast<const ibis::array_t<uint64_t>*>(k12.getArray());
        ibis::array_t<uint64_t> &ak10 =
            *static_cast<ibis::array_t<uint64_t>*>(k11.getArray());
        const ibis::array_t<uint64_t> ak11(ak10);
        ak10.nosharing();
        ierr = merge20T1(ak10, ak11, ak12, k21, v1, k22, v2, agg);
        break;}
    case ibis::FLOAT: {
        const ibis::array_t<float> &ak12 =
            *static_cast<const ibis::array_t<float>*>(k12.getArray());
        ibis::array_t<float> &ak10 =
            *static_cast<ibis::array_t<float>*>(k11.getArray());
        const ibis::array_t<float> ak11(ak10);
        ak10.nosharing();
        ierr = merge20T1(ak10, ak11, ak12, k21, v1, k22, v2, agg);
        break;}
    case ibis::DOUBLE: {
        const ibis::array_t<double> &ak12 =
            *static_cast<const ibis::array_t<double>*>(k12.getArray());
        ibis::array_t<double> &ak10 =
            *static_cast<ibis::array_t<double>*>(k11.getArray());
        const ibis::array_t<double> ak11(ak10);
        ak10.nosharing();
        ierr = merge20T1(ak10, ak11, ak12, k21, v1, k22, v2, agg);
        break;}
    case ibis::CATEGORY:
    case ibis::TEXT: {
        const std::vector<std::string> &ak12 =
            *static_cast<const std::vector<std::string>*>(k12.getArray());
        std::vector<std::string> &ak10 =
            *static_cast<std::vector<std::string>*>(k11.getArray());
        const std::vector<std::string> ak11(ak10);
        ierr = merge20S1(ak10, ak11, ak12, k21, v1, k22, v2, agg);
        break;}
    }
    return ierr;
} // ibis::bord::merge20

/// Merge with two key columns and arbitrary number of value columns.
/// The first key column is templated.
template <typename Tk1> int
ibis::bord::merge20T1(ibis::array_t<Tk1> &k1out,
                      const ibis::array_t<Tk1> &k1in1,
                      const ibis::array_t<Tk1> &k1in2,
                      ibis::bord::column &k21,
                      std::vector<ibis::bord::column*> &vin1,
                      const ibis::bord::column &k22,
                      const std::vector<ibis::bord::column*> &vin2,
                      const std::vector<ibis::selectClause::AGREGADO> &agg) {
    int ierr = -1;
    if (k21.type() != k22.type()) return ierr;

    std::vector<ibis::bord::column*> av1(vin1.size());
    IBIS_BLOCK_GUARD(ibis::util::clearVec<ibis::bord::column>,
                     ibis::util::ref(av1));
    for (unsigned j = 0; j < vin1.size(); ++ j)
        av1[j] = new ibis::bord::column(*vin1[j]);

    switch (k21.type()) {
    default:
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bord::merge20T1 can not deal with k21 ("
            << k21.name() << ") of type "
            << ibis::TYPESTRING[(int)k21.type()];
        return ierr;
    case ibis::BYTE: {
        const ibis::array_t<signed char> &ak22 =
            *static_cast<const ibis::array_t<signed char>*>(k22.getArray());
        ibis::array_t<signed char> &ak20 =
            *static_cast<ibis::array_t<signed char>*>(k21.getArray());
        const ibis::array_t<signed char> ak21(ak20);
        ak20.nosharing();
        ierr = merge20T2(k1out, ak20, vin1, k1in1, ak21, av1,
                         k1in2, ak22, vin2, agg);
        break;}
    case ibis::UBYTE: {
        const ibis::array_t<unsigned char> &ak22 =
            *static_cast<const ibis::array_t<unsigned char>*>(k22.getArray());
        ibis::array_t<unsigned char> &ak20 =
            *static_cast<ibis::array_t<unsigned char>*>(k21.getArray());
        const ibis::array_t<unsigned char> ak21(ak20);
        ak20.nosharing();
        ierr = merge20T2(k1out, ak20, vin1, k1in1, ak21, av1,
                         k1in2, ak22, vin2, agg);
        break;}
    case ibis::SHORT: {
        const ibis::array_t<int16_t> &ak22 =
            *static_cast<const ibis::array_t<int16_t>*>(k22.getArray());
        ibis::array_t<int16_t> &ak20 =
            *static_cast<ibis::array_t<int16_t>*>(k21.getArray());
        const ibis::array_t<int16_t> ak21(ak20);
        ak20.nosharing();
        ierr = merge20T2(k1out, ak20, vin1, k1in1, ak21, av1,
                         k1in2, ak22, vin2, agg);
        break;}
    case ibis::USHORT: {
        const ibis::array_t<uint16_t> &ak22 =
            *static_cast<const ibis::array_t<uint16_t>*>(k22.getArray());
        ibis::array_t<uint16_t> &ak20 =
            *static_cast<ibis::array_t<uint16_t>*>(k21.getArray());
        const ibis::array_t<uint16_t> ak21(ak20);
        ak20.nosharing();
        ierr = merge20T2(k1out, ak20, vin1, k1in1, ak21, av1,
                         k1in2, ak22, vin2, agg);
        break;}
    case ibis::INT: {
        const ibis::array_t<int32_t> &ak22 =
            *static_cast<const ibis::array_t<int32_t>*>(k22.getArray());
        ibis::array_t<int32_t> &ak20 =
            *static_cast<ibis::array_t<int32_t>*>(k21.getArray());
        const ibis::array_t<int32_t> ak21(ak20);
        ak20.nosharing();
        ierr = merge20T2(k1out, ak20, vin1, k1in1, ak21, av1,
                         k1in2, ak22, vin2, agg);
        break;}
    case ibis::UINT: {
        const ibis::array_t<uint32_t> &ak22 =
            *static_cast<const ibis::array_t<uint32_t>*>(k22.getArray());
        ibis::array_t<uint32_t> &ak20 =
            *static_cast<ibis::array_t<uint32_t>*>(k21.getArray());
        const ibis::array_t<uint32_t> ak21(ak20);
        ak20.nosharing();
        ierr = merge20T2(k1out, ak20, vin1, k1in1, ak21, av1,
                         k1in2, ak22, vin2, agg);
        break;}
    case ibis::LONG: {
        const ibis::array_t<int64_t> &ak22 =
            *static_cast<const ibis::array_t<int64_t>*>(k22.getArray());
        ibis::array_t<int64_t> &ak20 =
            *static_cast<ibis::array_t<int64_t>*>(k21.getArray());
        const ibis::array_t<int64_t> ak21(ak20);
        ak20.nosharing();
        ierr = merge20T2(k1out, ak20, vin1, k1in1, ak21, av1,
                         k1in2, ak22, vin2, agg);
        break;}
    case ibis::ULONG: {
        const ibis::array_t<uint64_t> &ak22 =
            *static_cast<const ibis::array_t<uint64_t>*>(k22.getArray());
        ibis::array_t<uint64_t> &ak20 =
            *static_cast<ibis::array_t<uint64_t>*>(k21.getArray());
        const ibis::array_t<uint64_t> ak21(ak20);
        ak20.nosharing();
        ierr = merge20T2(k1out, ak20, vin1, k1in1, ak21, av1,
                         k1in2, ak22, vin2, agg);
        break;}
    case ibis::FLOAT: {
        const ibis::array_t<float> &ak22 =
            *static_cast<const ibis::array_t<float>*>(k22.getArray());
        ibis::array_t<float> &ak20 =
            *static_cast<ibis::array_t<float>*>(k21.getArray());
        const ibis::array_t<float> ak21(ak20);
        ak20.nosharing();
        ierr = merge20T2(k1out, ak20, vin1, k1in1, ak21, av1,
                         k1in2, ak22, vin2, agg);
        break;}
    case ibis::DOUBLE: {
        const ibis::array_t<double> &ak22 =
            *static_cast<const ibis::array_t<double>*>(k22.getArray());
        ibis::array_t<double> &ak20 =
            *static_cast<ibis::array_t<double>*>(k21.getArray());
        const ibis::array_t<double> ak21(ak20);
        ak20.nosharing();
        ierr = merge20T2(k1out, ak20, vin1, k1in1, ak21, av1,
                         k1in2, ak22, vin2, agg);
        break;}
    case ibis::CATEGORY:
    case ibis::TEXT: {
        const std::vector<std::string> &ak22 =
            *static_cast<const std::vector<std::string>*>(k22.getArray());
        std::vector<std::string> &ak20 =
            *static_cast<std::vector<std::string>*>(k21.getArray());
        const std::vector<std::string> ak21(ak20);
        ierr = merge20S3(k1out, ak20, vin1, k1in1, ak21, av1,
                         k1in2, ak22, vin2, agg);
        break;}
    }
    return ierr;
} // ibis::bord::merge20T1

/// Merge in-memory table with two keys and more than one value columns.
/// Both key columns are templated.
template <typename Tk1, typename Tk2> int
ibis::bord::merge20T2(ibis::array_t<Tk1> &k1out,
                      ibis::array_t<Tk2> &k2out,
                      std::vector<ibis::bord::column*> &vout,
                      const ibis::array_t<Tk1> &k1in1,
                      const ibis::array_t<Tk2> &k2in1,
                      const std::vector<ibis::bord::column*> &vin1,
                      const ibis::array_t<Tk1> &k1in2,
                      const ibis::array_t<Tk2> &k2in2,
                      const std::vector<ibis::bord::column*> &vin2,
                      const std::vector<ibis::selectClause::AGREGADO> &agg) {
    int ierr = -1;
    k1out.clear();
    k2out.clear();
    for (size_t j = 0; j < vout.size(); ++ j)
        vout[j]->limit(0);
    if (vout.size() != vin1.size() || vout.size() != vin2.size() ||
        vout.size() != agg.size())
        return ierr;

    size_t j1 = 0;
    size_t j2 = 0;
    while (j1 < k1in1.size() && j2 < k1in2.size()) {
        if (k1in1[j1] == k1in2[j2]) {
            if (k2in1[j1] == k2in2[j2]) {
                k1out.push_back(k1in1[j1]);
                k2out.push_back(k2in1[j1]);
                for (unsigned j = 0; j < vin1.size(); ++ j)
                    vout[j]->append(vin1[j]->getArray(), j1,
                                    vin2[j]->getArray(), j2, agg[j]);
                ++ j1;
                ++ j2;
            }
            else if (k2in1[j1] < k2in2[j2]) {
                k1out.push_back(k1in1[j1]);
                k2out.push_back(k2in1[j1]);
                for (unsigned j = 0; j < vin1.size(); ++ j)
                    vout[j]->append(vin1[j]->getArray(), j1);
                ++ j1;
            }
            else {
                k1out.push_back(k1in2[j2]);
                k2out.push_back(k2in2[j2]);
                for (unsigned j = 0; j < vin2.size(); ++ j)
                    vout[j]->append(vin2[j]->getArray(), j2);
                ++ j2;
            }
        }
        else if (k1in1[j1] < k1in2[j2]) {
            k1out.push_back(k1in1[j1]);
            k2out.push_back(k2in1[j1]);
            for (unsigned j = 0; j < vin1.size(); ++ j)
                vout[j]->append(vin1[j]->getArray(), j1);
            ++ j1;
        }
        else {
            k1out.push_back(k1in2[j2]);
            k2out.push_back(k2in2[j2]);
            for (unsigned j = 0; j < vin2.size(); ++ j)
                vout[j]->append(vin2[j]->getArray(), j2);
            ++ j2;
        }
    }

    while (j1 < k1in1.size()) {
        k1out.push_back(k1in1[j1]);
        k2out.push_back(k2in1[j1]);
        for (unsigned j = 0; j < vin1.size(); ++ j)
            vout[j]->append(vin1[j]->getArray(), j1);
        ++ j1;
    }

    while (j2 < k1in2.size()) {
        k1out.push_back(k1in2[j2]);
        k2out.push_back(k2in2[j2]);
        for (unsigned j = 0; j < vin2.size(); ++ j)
            vout[j]->append(vin2[j]->getArray(), j2);
        ++ j2;
    }

    ierr = k1out.size();
    return ierr;
} // ibis::bord::merge20T2

int
ibis::bord::merge20S1(std::vector<std::string> &k1out,
                      const std::vector<std::string> &k1in1,
                      const std::vector<std::string> &k1in2,
                      ibis::bord::column &k21,
                      std::vector<ibis::bord::column*> &vin1,
                      const ibis::bord::column &k22,
                      const std::vector<ibis::bord::column*> &vin2,
                      const std::vector<ibis::selectClause::AGREGADO> &agg) {
    int ierr = -1;
    if (k21.type() != k22.type()) return ierr;

    std::vector<ibis::bord::column*> av1(vin1.size());
    IBIS_BLOCK_GUARD(ibis::util::clearVec<ibis::bord::column>,
                     ibis::util::ref(av1));
    for (unsigned j = 0; j < vin1.size(); ++ j)
        av1[j] = new ibis::bord::column(*vin1[j]);

    switch (k21.type()) {
    default:
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bord::merge20S1 can not deal with k21 ("
            << k21.name() << ") of type "
            << ibis::TYPESTRING[(int)k21.type()];
        return ierr;
    case ibis::BYTE: {
        const ibis::array_t<signed char> &ak22 =
            *static_cast<const ibis::array_t<signed char>*>(k22.getArray());
        ibis::array_t<signed char> &ak20 =
            *static_cast<ibis::array_t<signed char>*>(k21.getArray());
        const ibis::array_t<signed char> ak21(ak20);
        ak20.nosharing();
        ierr = merge20S2(k1out, ak20, vin1, k1in1, ak21, av1,
                         k1in2, ak22, vin2, agg);
        break;}
    case ibis::UBYTE: {
        const ibis::array_t<unsigned char> &ak22 =
            *static_cast<const ibis::array_t<unsigned char>*>(k22.getArray());
        ibis::array_t<unsigned char> &ak20 =
            *static_cast<ibis::array_t<unsigned char>*>(k21.getArray());
        const ibis::array_t<unsigned char> ak21(ak20);
        ak20.nosharing();
        ierr = merge20S2(k1out, ak20, vin1, k1in1, ak21, av1,
                         k1in2, ak22, vin2, agg);
        break;}
    case ibis::SHORT: {
        const ibis::array_t<int16_t> &ak22 =
            *static_cast<const ibis::array_t<int16_t>*>(k22.getArray());
        ibis::array_t<int16_t> &ak20 =
            *static_cast<ibis::array_t<int16_t>*>(k21.getArray());
        const ibis::array_t<int16_t> ak21(ak20);
        ak20.nosharing();
        ierr = merge20S2(k1out, ak20, vin1, k1in1, ak21, av1,
                         k1in2, ak22, vin2, agg);
        break;}
    case ibis::USHORT: {
        const ibis::array_t<uint16_t> &ak22 =
            *static_cast<const ibis::array_t<uint16_t>*>(k22.getArray());
        ibis::array_t<uint16_t> &ak20 =
            *static_cast<ibis::array_t<uint16_t>*>(k21.getArray());
        const ibis::array_t<uint16_t> ak21(ak20);
        ak20.nosharing();
        ierr = merge20S2(k1out, ak20, vin1, k1in1, ak21, av1,
                         k1in2, ak22, vin2, agg);
        break;}
    case ibis::INT: {
        const ibis::array_t<int32_t> &ak22 =
            *static_cast<const ibis::array_t<int32_t>*>(k22.getArray());
        ibis::array_t<int32_t> &ak20 =
            *static_cast<ibis::array_t<int32_t>*>(k21.getArray());
        const ibis::array_t<int32_t> ak21(ak20);
        ak20.nosharing();
        ierr = merge20S2(k1out, ak20, vin1, k1in1, ak21, av1,
                         k1in2, ak22, vin2, agg);
        break;}
    case ibis::UINT: {
        const ibis::array_t<uint32_t> &ak22 =
            *static_cast<const ibis::array_t<uint32_t>*>(k22.getArray());
        ibis::array_t<uint32_t> &ak20 =
            *static_cast<ibis::array_t<uint32_t>*>(k21.getArray());
        const ibis::array_t<uint32_t> ak21(ak20);
        ak20.nosharing();
        ierr = merge20S2(k1out, ak20, vin1, k1in1, ak21, av1,
                         k1in2, ak22, vin2, agg);
        break;}
    case ibis::LONG: {
        const ibis::array_t<int64_t> &ak22 =
            *static_cast<const ibis::array_t<int64_t>*>(k22.getArray());
        ibis::array_t<int64_t> &ak20 =
            *static_cast<ibis::array_t<int64_t>*>(k21.getArray());
        const ibis::array_t<int64_t> ak21(ak20);
        ak20.nosharing();
        ierr = merge20S2(k1out, ak20, vin1, k1in1, ak21, av1,
                         k1in2, ak22, vin2, agg);
        break;}
    case ibis::ULONG: {
        const ibis::array_t<uint64_t> &ak22 =
            *static_cast<const ibis::array_t<uint64_t>*>(k22.getArray());
        ibis::array_t<uint64_t> &ak20 =
            *static_cast<ibis::array_t<uint64_t>*>(k21.getArray());
        const ibis::array_t<uint64_t> ak21(ak20);
        ak20.nosharing();
        ierr = merge20S2(k1out, ak20, vin1, k1in1, ak21, av1,
                         k1in2, ak22, vin2, agg);
        break;}
    case ibis::FLOAT: {
        const ibis::array_t<float> &ak22 =
            *static_cast<const ibis::array_t<float>*>(k22.getArray());
        ibis::array_t<float> &ak20 =
            *static_cast<ibis::array_t<float>*>(k21.getArray());
        const ibis::array_t<float> ak21(ak20);
        ak20.nosharing();
        ierr = merge20S2(k1out, ak20, vin1, k1in1, ak21, av1,
                         k1in2, ak22, vin2, agg);
        break;}
    case ibis::DOUBLE: {
        const ibis::array_t<double> &ak22 =
            *static_cast<const ibis::array_t<double>*>(k22.getArray());
        ibis::array_t<double> &ak20 =
            *static_cast<ibis::array_t<double>*>(k21.getArray());
        const ibis::array_t<double> ak21(ak20);
        ak20.nosharing();
        ierr = merge20S2(k1out, ak20, vin1, k1in1, ak21, av1,
                         k1in2, ak22, vin2, agg);
        break;}
    case ibis::CATEGORY:
    case ibis::TEXT: {
        const std::vector<std::string> &ak22 =
            *static_cast<const std::vector<std::string>*>(k22.getArray());
        std::vector<std::string> &ak20 =
            *static_cast<std::vector<std::string>*>(k21.getArray());
        const std::vector<std::string> ak21(ak20);
        ierr = merge20S0(k1out, ak20, vin1, k1in1, ak21, av1,
                         k1in2, ak22, vin2, agg);
        break;}
    }
    return ierr;
} // ibis::bord::merge20S1

int
ibis::bord::merge20S0(std::vector<std::string> &k1out,
                      std::vector<std::string> &k2out,
                      std::vector<ibis::bord::column*> &vout,
                      const std::vector<std::string> &k1in1,
                      const std::vector<std::string> &k2in1,
                      const std::vector<ibis::bord::column*> &vin1,
                      const std::vector<std::string> &k1in2,
                      const std::vector<std::string> &k2in2,
                      const std::vector<ibis::bord::column*> &vin2,
                      const std::vector<ibis::selectClause::AGREGADO> &agg) {
    int ierr = -1;
    k1out.clear();
    k2out.clear();
    for (size_t j = 0; j < vout.size(); ++ j)
        vout[j]->limit(0);
    if (vout.size() != vin1.size() || vout.size() != vin2.size() ||
        vout.size() != agg.size())
        return ierr;

    size_t j1 = 0;
    size_t j2 = 0;
    while (j1 < k1in1.size() && j2 < k1in2.size()) {
        if (k1in1[j1] == k1in2[j2]) {
            if (k2in1[j1] == k2in2[j2]) {
                k1out.push_back(k1in1[j1]);
                k2out.push_back(k2in1[j1]);
                for (unsigned j = 0; j < vin1.size(); ++ j)
                    vout[j]->append(vin1[j]->getArray(), j1,
                                    vin2[j]->getArray(), j2, agg[j]);
                ++ j1;
                ++ j2;
            }
            else if (k2in1[j1] < k2in2[j2]) {
                k1out.push_back(k1in1[j1]);
                k2out.push_back(k2in1[j1]);
                for (unsigned j = 0; j < vin1.size(); ++ j)
                    vout[j]->append(vin1[j]->getArray(), j1);
                ++ j1;
            }
            else {
                k1out.push_back(k1in2[j2]);
                k2out.push_back(k2in2[j2]);
                for (unsigned j = 0; j < vin2.size(); ++ j)
                    vout[j]->append(vin2[j]->getArray(), j2);
                ++ j2;
            }
        }
        else if (k1in1[j1] < k1in2[j2]) {
            k1out.push_back(k1in1[j1]);
            k2out.push_back(k2in1[j1]);
            for (unsigned j = 0; j < vin1.size(); ++ j)
                vout[j]->append(vin1[j]->getArray(), j1);
            ++ j1;
        }
        else {
            k1out.push_back(k1in2[j2]);
            k2out.push_back(k2in2[j2]);
            for (unsigned j = 0; j < vin2.size(); ++ j)
                vout[j]->append(vin2[j]->getArray(), j2);
            ++ j2;
        }
    }

    while (j1 < k1in1.size()) {
        k1out.push_back(k1in1[j1]);
        k2out.push_back(k2in1[j1]);
        for (unsigned j = 0; j < vin1.size(); ++ j)
            vout[j]->append(vin1[j]->getArray(), j1);
        ++ j1;
    }

    while (j2 < k1in2.size()) {
        k1out.push_back(k1in2[j2]);
        k2out.push_back(k2in2[j2]);
        for (unsigned j = 0; j < vin2.size(); ++ j)
            vout[j]->append(vin2[j]->getArray(), j2);
        ++ j2;
    }

    ierr = k1out.size();
    return ierr;
} // ibis::bord::merge20S0

template <typename Tk2> int
ibis::bord::merge20S2(std::vector<std::string> &k1out,
                      ibis::array_t<Tk2> &k2out,
                      std::vector<ibis::bord::column*> &vout,
                      const std::vector<std::string> &k1in1,
                      const ibis::array_t<Tk2> &k2in1,
                      const std::vector<ibis::bord::column*> &vin1,
                      const std::vector<std::string> &k1in2,
                      const ibis::array_t<Tk2> &k2in2,
                      const std::vector<ibis::bord::column*> &vin2,
                      const std::vector<ibis::selectClause::AGREGADO> &agg) {
    int ierr = -1;
    k1out.clear();
    k2out.clear();
    for (size_t j = 0; j < vout.size(); ++ j)
        vout[j]->limit(0);
    if (vout.size() != vin1.size() || vout.size() != vin2.size() ||
        vout.size() != agg.size())
        return ierr;

    size_t j1 = 0;
    size_t j2 = 0;
    while (j1 < k1in1.size() && j2 < k1in2.size()) {
        if (k1in1[j1] == k1in2[j2]) {
            if (k2in1[j1] == k2in2[j2]) {
                k1out.push_back(k1in1[j1]);
                k2out.push_back(k2in1[j1]);
                for (unsigned j = 0; j < vin1.size(); ++ j)
                    vout[j]->append(vin1[j]->getArray(), j1,
                                    vin2[j]->getArray(), j2, agg[j]);
                ++ j1;
                ++ j2;
            }
            else if (k2in1[j1] < k2in2[j2]) {
                k1out.push_back(k1in1[j1]);
                k2out.push_back(k2in1[j1]);
                for (unsigned j = 0; j < vin1.size(); ++ j)
                    vout[j]->append(vin1[j]->getArray(), j1);
                ++ j1;
            }
            else {
                k1out.push_back(k1in2[j2]);
                k2out.push_back(k2in2[j2]);
                for (unsigned j = 0; j < vin2.size(); ++ j)
                    vout[j]->append(vin2[j]->getArray(), j2);
                ++ j2;
            }
        }
        else if (k1in1[j1] < k1in2[j2]) {
            k1out.push_back(k1in1[j1]);
            k2out.push_back(k2in1[j1]);
            for (unsigned j = 0; j < vin1.size(); ++ j)
                vout[j]->append(vin1[j]->getArray(), j1);
            ++ j1;
        }
        else {
            k1out.push_back(k1in2[j2]);
            k2out.push_back(k2in2[j2]);
            for (unsigned j = 0; j < vin2.size(); ++ j)
                vout[j]->append(vin2[j]->getArray(), j2);
            ++ j2;
        }
    }

    while (j1 < k1in1.size()) {
        k1out.push_back(k1in1[j1]);
        k2out.push_back(k2in1[j1]);
        for (unsigned j = 0; j < vin1.size(); ++ j)
            vout[j]->append(vin1[j]->getArray(), j1);
        ++ j1;
    }

    while (j2 < k1in2.size()) {
        k1out.push_back(k1in2[j2]);
        k2out.push_back(k2in2[j2]);
        for (unsigned j = 0; j < vin2.size(); ++ j)
            vout[j]->append(vin2[j]->getArray(), j2);
        ++ j2;
    }

    ierr = k1out.size();
    return ierr;
} // ibis::bord::merge20S2

template <typename Tk1> int
ibis::bord::merge20S3(ibis::array_t<Tk1> &k1out,
                      std::vector<std::string> &k2out,
                      std::vector<ibis::bord::column*> &vout,
                      const ibis::array_t<Tk1> &k1in1,
                      const std::vector<std::string> &k2in1,
                      const std::vector<ibis::bord::column*> &vin1,
                      const ibis::array_t<Tk1> &k1in2,
                      const std::vector<std::string> &k2in2,
                      const std::vector<ibis::bord::column*> &vin2,
                      const std::vector<ibis::selectClause::AGREGADO> &agg) {
    int ierr = -1;
    k1out.clear();
    k2out.clear();
    for (size_t j = 0; j < vout.size(); ++ j)
        vout[j]->limit(0);
    if (vout.size() != vin1.size() || vout.size() != vin2.size() ||
        vout.size() != agg.size())
        return ierr;

    size_t j1 = 0;
    size_t j2 = 0;
    while (j1 < k1in1.size() && j2 < k1in2.size()) {
        if (k1in1[j1] == k1in2[j2]) {
            if (k2in1[j1] == k2in2[j2]) {
                k1out.push_back(k1in1[j1]);
                k2out.push_back(k2in1[j1]);
                for (unsigned j = 0; j < vin1.size(); ++ j)
                    vout[j]->append(vin1[j]->getArray(), j1,
                                    vin2[j]->getArray(), j2, agg[j]);
                ++ j1;
                ++ j2;
            }
            else if (k2in1[j1] < k2in2[j2]) {
                k1out.push_back(k1in1[j1]);
                k2out.push_back(k2in1[j1]);
                for (unsigned j = 0; j < vin1.size(); ++ j)
                    vout[j]->append(vin1[j]->getArray(), j1);
                ++ j1;
            }
            else {
                k1out.push_back(k1in2[j2]);
                k2out.push_back(k2in2[j2]);
                for (unsigned j = 0; j < vin2.size(); ++ j)
                    vout[j]->append(vin2[j]->getArray(), j2);
                ++ j2;
            }
        }
        else if (k1in1[j1] < k1in2[j2]) {
            k1out.push_back(k1in1[j1]);
            k2out.push_back(k2in1[j1]);
            for (unsigned j = 0; j < vin1.size(); ++ j)
                vout[j]->append(vin1[j]->getArray(), j1);
            ++ j1;
        }
        else {
            k1out.push_back(k1in2[j2]);
            k2out.push_back(k2in2[j2]);
            for (unsigned j = 0; j < vin2.size(); ++ j)
                vout[j]->append(vin2[j]->getArray(), j2);
            ++ j2;
        }
    }

    while (j1 < k1in1.size()) {
        k1out.push_back(k1in1[j1]);
        k2out.push_back(k2in1[j1]);
        for (unsigned j = 0; j < vin1.size(); ++ j)
            vout[j]->append(vin1[j]->getArray(), j1);
        ++ j1;
    }

    while (j2 < k1in2.size()) {
        k1out.push_back(k1in2[j2]);
        k2out.push_back(k2in2[j2]);
        for (unsigned j = 0; j < vin2.size(); ++ j)
            vout[j]->append(vin2[j]->getArray(), j2);
        ++ j2;
    }

    ierr = k1out.size();
    return ierr;
} // ibis::bord::merge20S3

/// Merge two key columns with one value column.
int ibis::bord::merge21(ibis::bord::column &k11,
                        ibis::bord::column &k21,
                        ibis::bord::column &v1,
                        const ibis::bord::column &k12,
                        const ibis::bord::column &k22,
                        const ibis::bord::column &v2,
                        ibis::selectClause::AGREGADO ag) {
    int ierr = -1;
    if (k11.type() != k12.type()) return ierr;

    switch (k11.type()) {
    default:
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bord::merge21 can not deal with k11 ("
            << k11.name() << ") of type "
            << ibis::TYPESTRING[(int)k11.type()];
        return ierr;
    case ibis::BYTE: {
        const ibis::array_t<signed char> &ak12 =
            *static_cast<const ibis::array_t<signed char>*>(k12.getArray());
        ibis::array_t<signed char> &ak10 =
            *static_cast<ibis::array_t<signed char>*>(k11.getArray());
        const ibis::array_t<signed char> ak11(ak10);
        ak10.nosharing();
        ierr = merge21T1(ak10, ak11, ak12, k21, v1, k22, v2, ag);
        break;}
    case ibis::UBYTE: {
        const ibis::array_t<unsigned char> &ak12 =
            *static_cast<const ibis::array_t<unsigned char>*>(k12.getArray());
        ibis::array_t<unsigned char> &ak10 =
            *static_cast<ibis::array_t<unsigned char>*>(k11.getArray());
        const ibis::array_t<unsigned char> ak11(ak10);
        ak10.nosharing();
        ierr = merge21T1(ak10, ak11, ak12, k21, v1, k22, v2, ag);
        break;}
    case ibis::SHORT: {
        const ibis::array_t<int16_t> &ak12 =
            *static_cast<const ibis::array_t<int16_t>*>(k12.getArray());
        ibis::array_t<int16_t> &ak10 =
            *static_cast<ibis::array_t<int16_t>*>(k11.getArray());
        const ibis::array_t<int16_t> ak11(ak10);
        ak10.nosharing();
        ierr = merge21T1(ak10, ak11, ak12, k21, v1, k22, v2, ag);
        break;}
    case ibis::USHORT: {
        const ibis::array_t<uint16_t> &ak12 =
            *static_cast<const ibis::array_t<uint16_t>*>(k12.getArray());
        ibis::array_t<uint16_t> &ak10 =
            *static_cast<ibis::array_t<uint16_t>*>(k11.getArray());
        const ibis::array_t<uint16_t> ak11(ak10);
        ak10.nosharing();
        ierr = merge21T1(ak10, ak11, ak12, k21, v1, k22, v2, ag);
        break;}
    case ibis::INT: {
        const ibis::array_t<int32_t> &ak12 =
            *static_cast<const ibis::array_t<int32_t>*>(k12.getArray());
        ibis::array_t<int32_t> &ak10 =
            *static_cast<ibis::array_t<int32_t>*>(k11.getArray());
        const ibis::array_t<int32_t> ak11(ak10);
        ak10.nosharing();
        ierr = merge21T1(ak10, ak11, ak12, k21, v1, k22, v2, ag);
        break;}
    case ibis::UINT: {
        const ibis::array_t<uint32_t> &ak12 =
            *static_cast<const ibis::array_t<uint32_t>*>(k12.getArray());
        ibis::array_t<uint32_t> &ak10 =
            *static_cast<ibis::array_t<uint32_t>*>(k11.getArray());
        const ibis::array_t<uint32_t> ak11(ak10);
        ak10.nosharing();
        ierr = merge21T1(ak10, ak11, ak12, k21, v1, k22, v2, ag);
        break;}
    case ibis::LONG: {
        const ibis::array_t<int64_t> &ak12 =
            *static_cast<const ibis::array_t<int64_t>*>(k12.getArray());
        ibis::array_t<int64_t> &ak10 =
            *static_cast<ibis::array_t<int64_t>*>(k11.getArray());
        const ibis::array_t<int64_t> ak11(ak10);
        ak10.nosharing();
        ierr = merge21T1(ak10, ak11, ak12, k21, v1, k22, v2, ag);
        break;}
    case ibis::ULONG: {
        const ibis::array_t<uint64_t> &ak12 =
            *static_cast<const ibis::array_t<uint64_t>*>(k12.getArray());
        ibis::array_t<uint64_t> &ak10 =
            *static_cast<ibis::array_t<uint64_t>*>(k11.getArray());
        const ibis::array_t<uint64_t> ak11(ak10);
        ak10.nosharing();
        ierr = merge21T1(ak10, ak11, ak12, k21, v1, k22, v2, ag);
        break;}
    case ibis::FLOAT: {
        const ibis::array_t<float> &ak12 =
            *static_cast<const ibis::array_t<float>*>(k12.getArray());
        ibis::array_t<float> &ak10 =
            *static_cast<ibis::array_t<float>*>(k11.getArray());
        const ibis::array_t<float> ak11(ak10);
        ak10.nosharing();
        ierr = merge21T1(ak10, ak11, ak12, k21, v1, k22, v2, ag);
        break;}
    case ibis::DOUBLE: {
        const ibis::array_t<double> &ak12 =
            *static_cast<const ibis::array_t<double>*>(k12.getArray());
        ibis::array_t<double> &ak10 =
            *static_cast<ibis::array_t<double>*>(k11.getArray());
        const ibis::array_t<double> ak11(ak10);
        ak10.nosharing();
        ierr = merge21T1(ak10, ak11, ak12, k21, v1, k22, v2, ag);
        break;}
    case ibis::CATEGORY:
    case ibis::TEXT: {
        const std::vector<std::string> &ak12 =
            *static_cast<const std::vector<std::string>*>(k12.getArray());
        std::vector<std::string> &ak10 =
            *static_cast<std::vector<std::string>*>(k11.getArray());
        const std::vector<std::string> ak11(ak10);
        ierr = merge21S1(ak10, ak11, ak12, k21, v1, k22, v2, ag);
        break;}
    }

    return ierr;
} // ibis::bord::merge21

/// Merge two key columns with one value column.
/// The first key column is templated.
template <typename Tk1>
int ibis::bord::merge21T1(ibis::array_t<Tk1> &k1out,
                          const ibis::array_t<Tk1> &k1in1,
                          const ibis::array_t<Tk1> &k1in2,
                          ibis::bord::column &k21,
                          ibis::bord::column &v1,
                          const ibis::bord::column &k22,
                          const ibis::bord::column &v2,
                          ibis::selectClause::AGREGADO ag) {
    int ierr = -1;
    if (k21.type() != k22.type()) return ierr;

    switch (k21.type()) {
    default:
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bord::merge21T1 can not deal with k21 ("
            << k21.name() << ") of type "
            << ibis::TYPESTRING[(int)k21.type()];
        return ierr;
    case ibis::BYTE: {
        const ibis::array_t<signed char> &ak22 =
            *static_cast<const ibis::array_t<signed char>*>(k22.getArray());
        ibis::array_t<signed char> &ak20 =
            *static_cast<ibis::array_t<signed char>*>(k21.getArray());
        const ibis::array_t<signed char> ak21(ak20);
        ak20.nosharing();
        ierr = merge21T2(k1out, ak20, k1in1, ak21, k1in2, ak22, v1, v2, ag);
        break;}
    case ibis::UBYTE: {
        const ibis::array_t<unsigned char> &ak22 =
            *static_cast<const ibis::array_t<unsigned char>*>(k22.getArray());
        ibis::array_t<unsigned char> &ak20 =
            *static_cast<ibis::array_t<unsigned char>*>(k21.getArray());
        const ibis::array_t<unsigned char> ak21(ak20);
        ak20.nosharing();
        ierr = merge21T2(k1out, ak20, k1in1, ak21, k1in2, ak22, v1, v2, ag);
        break;}
    case ibis::SHORT: {
        const ibis::array_t<int16_t> &ak22 =
            *static_cast<const ibis::array_t<int16_t>*>(k22.getArray());
        ibis::array_t<int16_t> &ak20 =
            *static_cast<ibis::array_t<int16_t>*>(k21.getArray());
        const ibis::array_t<int16_t> ak21(ak20);
        ak20.nosharing();
        ierr = merge21T2(k1out, ak20, k1in1, ak21, k1in2, ak22, v1, v2, ag);
        break;}
    case ibis::USHORT: {
        const ibis::array_t<uint16_t> &ak22 =
            *static_cast<const ibis::array_t<uint16_t>*>(k22.getArray());
        ibis::array_t<uint16_t> &ak20 =
            *static_cast<ibis::array_t<uint16_t>*>(k21.getArray());
        const ibis::array_t<uint16_t> ak21(ak20);
        ak20.nosharing();
        ierr = merge21T2(k1out, ak20, k1in1, ak21, k1in2, ak22, v1, v2, ag);
        break;}
    case ibis::INT: {
        const ibis::array_t<int32_t> &ak22 =
            *static_cast<const ibis::array_t<int32_t>*>(k22.getArray());
        ibis::array_t<int32_t> &ak20 =
            *static_cast<ibis::array_t<int32_t>*>(k21.getArray());
        const ibis::array_t<int32_t> ak21(ak20);
        ak20.nosharing();
        ierr = merge21T2(k1out, ak20, k1in1, ak21, k1in2, ak22, v1, v2, ag);
        break;}
    case ibis::UINT: {
        const ibis::array_t<uint32_t> &ak22 =
            *static_cast<const ibis::array_t<uint32_t>*>(k22.getArray());
        ibis::array_t<uint32_t> &ak20 =
            *static_cast<ibis::array_t<uint32_t>*>(k21.getArray());
        const ibis::array_t<uint32_t> ak21(ak20);
        ak20.nosharing();
        ierr = merge21T2(k1out, ak20, k1in1, ak21, k1in2, ak22, v1, v2, ag);
        break;}
    case ibis::LONG: {
        const ibis::array_t<int64_t> &ak22 =
            *static_cast<const ibis::array_t<int64_t>*>(k22.getArray());
        ibis::array_t<int64_t> &ak20 =
            *static_cast<ibis::array_t<int64_t>*>(k21.getArray());
        const ibis::array_t<int64_t> ak21(ak20);
        ak20.nosharing();
        ierr = merge21T2(k1out, ak20, k1in1, ak21, k1in2, ak22, v1, v2, ag);
        break;}
    case ibis::ULONG: {
        const ibis::array_t<uint64_t> &ak22 =
            *static_cast<const ibis::array_t<uint64_t>*>(k22.getArray());
        ibis::array_t<uint64_t> &ak20 =
            *static_cast<ibis::array_t<uint64_t>*>(k21.getArray());
        const ibis::array_t<uint64_t> ak21(ak20);
        ak20.nosharing();
        ierr = merge21T2(k1out, ak20, k1in1, ak21, k1in2, ak22, v1, v2, ag);
        break;}
    case ibis::FLOAT: {
        const ibis::array_t<float> &ak22 =
            *static_cast<const ibis::array_t<float>*>(k22.getArray());
        ibis::array_t<float> &ak20 =
            *static_cast<ibis::array_t<float>*>(k21.getArray());
        const ibis::array_t<float> ak21(ak20);
        ak20.nosharing();
        ierr = merge21T2(k1out, ak20, k1in1, ak21, k1in2, ak22, v1, v2, ag);
        break;}
    case ibis::DOUBLE: {
        const ibis::array_t<double> &ak22 =
            *static_cast<const ibis::array_t<double>*>(k22.getArray());
        ibis::array_t<double> &ak20 =
            *static_cast<ibis::array_t<double>*>(k21.getArray());
        const ibis::array_t<double> ak21(ak20);
        ak20.nosharing();
        ierr = merge21T2(k1out, ak20, k1in1, ak21, k1in2, ak22, v1, v2, ag);
        break;}
    case ibis::CATEGORY:
    case ibis::TEXT: {
        const std::vector<std::string> &ak22 =
            *static_cast<const std::vector<std::string>*>(k22.getArray());
        std::vector<std::string> &ak20 =
            *static_cast<std::vector<std::string>*>(k21.getArray());
        const std::vector<std::string> ak21(ak20);
        ierr = merge21S6(k1out, ak20, k1in1, ak21, k1in2, ak22, v1, v2, ag);
        break;}
    }

    return ierr;
} // ibis::bord::merge21T1

/// Merge two key columns with one value column.
/// The two key columns are templated.
template <typename Tk1, typename Tk2>
int ibis::bord::merge21T2(ibis::array_t<Tk1> &k1out,
                          ibis::array_t<Tk2> &k2out,
                          const ibis::array_t<Tk1> &k1in1,
                          const ibis::array_t<Tk2> &k2in1,
                          const ibis::array_t<Tk1> &k1in2,
                          const ibis::array_t<Tk2> &k2in2,
                          ibis::bord::column &v1,
                          const ibis::bord::column &v2,
                          ibis::selectClause::AGREGADO ag) {
    int ierr = -1;
    if (v1.type() != v2.type()) return ierr;

    switch (v1.type()) {
    default:
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bord::merge21T2 can not deal with v1 ("
            << v1.name() << ") of type "
            << ibis::TYPESTRING[(int)v1.type()];
        return ierr;
    case ibis::BYTE: {
        const ibis::array_t<signed char> &av2 =
            *static_cast<const ibis::array_t<signed char>*>(v2.getArray());
        ibis::array_t<signed char> &av0 =
            *static_cast<ibis::array_t<signed char>*>(v1.getArray());
        const ibis::array_t<signed char> av1(av0);
        av0.nosharing();
        ierr = merge21T3(k1out, k2out, av0,
                         k1in1, k2in1, av1,
                         k1in2, k2in2, av2, ag);
        break;}
    case ibis::UBYTE: {
        const ibis::array_t<unsigned char> &av2 =
            *static_cast<const ibis::array_t<unsigned char>*>(v2.getArray());
        ibis::array_t<unsigned char> &av0 =
            *static_cast<ibis::array_t<unsigned char>*>(v1.getArray());
        const ibis::array_t<unsigned char> av1(av0);
        av0.nosharing();
        ierr = merge21T3(k1out, k2out, av0,
                         k1in1, k2in1, av1,
                         k1in2, k2in2, av2, ag);
        break;}
    case ibis::SHORT: {
        const ibis::array_t<int16_t> &av2 =
            *static_cast<const ibis::array_t<int16_t>*>(v2.getArray());
        ibis::array_t<int16_t> &av0 =
            *static_cast<ibis::array_t<int16_t>*>(v1.getArray());
        const ibis::array_t<int16_t> av1(av0);
        av0.nosharing();
        ierr = merge21T3(k1out, k2out, av0,
                         k1in1, k2in1, av1,
                         k1in2, k2in2, av2, ag);
        break;}
    case ibis::USHORT: {
        const ibis::array_t<uint16_t> &av2 =
            *static_cast<const ibis::array_t<uint16_t>*>(v2.getArray());
        ibis::array_t<uint16_t> &av0 =
            *static_cast<ibis::array_t<uint16_t>*>(v1.getArray());
        const ibis::array_t<uint16_t> av1(av0);
        av0.nosharing();
        ierr = merge21T3(k1out, k2out, av0,
                         k1in1, k2in1, av1,
                         k1in2, k2in2, av2, ag);
        break;}
    case ibis::INT: {
        const ibis::array_t<int32_t> &av2 =
            *static_cast<const ibis::array_t<int32_t>*>(v2.getArray());
        ibis::array_t<int32_t> &av0 =
            *static_cast<ibis::array_t<int32_t>*>(v1.getArray());
        const ibis::array_t<int32_t> av1(av0);
        av0.nosharing();
        ierr = merge21T3(k1out, k2out, av0,
                         k1in1, k2in1, av1,
                         k1in2, k2in2, av2, ag);
        break;}
    case ibis::UINT: {
        const ibis::array_t<uint32_t> &av2 =
            *static_cast<const ibis::array_t<uint32_t>*>(v2.getArray());
        ibis::array_t<uint32_t> &av0 =
            *static_cast<ibis::array_t<uint32_t>*>(v1.getArray());
        const ibis::array_t<uint32_t> av1(av0);
        av0.nosharing();
        ierr = merge21T3(k1out, k2out, av0,
                         k1in1, k2in1, av1,
                         k1in2, k2in2, av2, ag);
        break;}
    case ibis::LONG: {
        const ibis::array_t<int64_t> &av2 =
            *static_cast<const ibis::array_t<int64_t>*>(v2.getArray());
        ibis::array_t<int64_t> &av0 =
            *static_cast<ibis::array_t<int64_t>*>(v1.getArray());
        const ibis::array_t<int64_t> av1(av0);
        av0.nosharing();
        ierr = merge21T3(k1out, k2out, av0,
                         k1in1, k2in1, av1,
                         k1in2, k2in2, av2, ag);
        break;}
    case ibis::ULONG: {
        const ibis::array_t<uint64_t> &av2 =
            *static_cast<const ibis::array_t<uint64_t>*>(v2.getArray());
        ibis::array_t<uint64_t> &av0 =
            *static_cast<ibis::array_t<uint64_t>*>(v1.getArray());
        const ibis::array_t<uint64_t> av1(av0);
        av0.nosharing();
        ierr = merge21T3(k1out, k2out, av0,
                         k1in1, k2in1, av1,
                         k1in2, k2in2, av2, ag);
        break;}
    case ibis::FLOAT: {
        const ibis::array_t<float> &av2 =
            *static_cast<const ibis::array_t<float>*>(v2.getArray());
        ibis::array_t<float> &av0 =
            *static_cast<ibis::array_t<float>*>(v1.getArray());
        const ibis::array_t<float> av1(av0);
        av0.nosharing();
        ierr = merge21T3(k1out, k2out, av0,
                         k1in1, k2in1, av1,
                         k1in2, k2in2, av2, ag);
        break;}
    case ibis::DOUBLE: {
        const ibis::array_t<double> &av2 =
            *static_cast<const ibis::array_t<double>*>(v2.getArray());
        ibis::array_t<double> &av0 =
            *static_cast<ibis::array_t<double>*>(v1.getArray());
        const ibis::array_t<double> av1(av0);
        av0.nosharing();
        ierr = merge21T3(k1out, k2out, av0,
                         k1in1, k2in1, av1,
                         k1in2, k2in2, av2, ag);
        break;}
    }

    return ierr;
} // ibis::bord::merge21T2

/// Merge two key columns with one value column.
/// The two key columns and the value column are all templated.
template <typename Tk1, typename Tk2, typename Tv>
int ibis::bord::merge21T3(ibis::array_t<Tk1> &k1out,
                          ibis::array_t<Tk2> &k2out,
                          ibis::array_t<Tv>  &vout,
                          const ibis::array_t<Tk1> &k1in1,
                          const ibis::array_t<Tk2> &k2in1,
                          const ibis::array_t<Tv>  &vin1,
                          const ibis::array_t<Tk1> &k1in2,
                          const ibis::array_t<Tk2> &k2in2,
                          const ibis::array_t<Tv>  &vin2,
                          ibis::selectClause::AGREGADO av) {
    k1out.clear();
    k2out.clear();
    vout.clear();
    int ierr = -1;
    if (k1in1.size() != k2in1.size() || k1in1.size() != vin1.size() ||
        k1in2.size() != k2in2.size() || k1in2.size() != vin2.size())
        return ierr;

    size_t j1 = 0; // for k1in1, k2in1, vin1
    size_t j2 = 0; // for k1in2, k2in2, vin2
    while (j1 < k1in1.size() && j2 < k1in2.size()) {
        if (k1in1[j1] == k1in2[j2]) { // same k1
            if (k2in1[j1] == k2in2[j2]) { // same k2
                switch (av) {
                default:
                    return ierr;
                case ibis::selectClause::CNT:
                case ibis::selectClause::SUM:
                    vout.push_back(vin1[j1] + vin2[j2]);
                    break;
                case ibis::selectClause::MAX:
                    vout.push_back(vin1[j1] >= vin2[j2] ? vin1[j1] : vin2[j2]);
                    break;
                case ibis::selectClause::MIN:
                    vout.push_back(vin1[j1] <= vin2[j2] ? vin1[j1] : vin2[j2]);
                    break;
                }
                k1out.push_back(k1in1[j1]);
                k2out.push_back(k2in1[j1]);
                ++ j1;
                ++ j2;
            }
            else if (k2in1[j1] < k2in2[j2]) { // copy from *1
                k1out.push_back(k1in1[j1]);
                k2out.push_back(k2in1[j1]);
                vout.push_back(vin1[j1]);
                ++ j1;
            }
            else { // copy from *2
                k1out.push_back(k1in2[j2]);
                k2out.push_back(k2in2[j2]);
                vout.push_back(vin2[j2]);
                ++ j2;
            }
        }
        else if (k1in1[j1] < k1in2[j2]) { // copy from *1
            k1out.push_back(k1in1[j1]);
            k2out.push_back(k2in1[j1]);
            vout.push_back(vin1[j1]);
            ++ j1;
        }
        else { // copy from *2
            k1out.push_back(k1in2[j2]);
            k2out.push_back(k2in2[j2]);
            vout.push_back(vin2[j2]);
            ++ j2;
        }
    }

    while (j1 < k1in1.size()) {
        k1out.push_back(k1in1[j1]);
        k2out.push_back(k2in1[j1]);
        vout.push_back(vin1[j1]);
        ++ j1;
    }

    while (j2 < k1in2.size()) {
        k1out.push_back(k1in2[j2]);
        k2out.push_back(k2in2[j2]);
        vout.push_back(vin2[j2]);
        ++ j2;
    }

    return k1out.size();
} // ibis::bord::merge21T3

int ibis::bord::merge21S1(std::vector<std::string> &k1out,
                          const std::vector<std::string> &k1in1,
                          const std::vector<std::string> &k1in2,
                          ibis::bord::column &k21,
                          ibis::bord::column &v1,
                          const ibis::bord::column &k22,
                          const ibis::bord::column &v2,
                          ibis::selectClause::AGREGADO ag) {
    int ierr = -1;
    if (k21.type() != k22.type()) return ierr;

    switch (k21.type()) {
    default:
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bord::merge21S1 can not deal with k21 ("
            << k21.name() << ") of type "
            << ibis::TYPESTRING[(int)k21.type()];
        return ierr;
    case ibis::BYTE: {
        const ibis::array_t<signed char> &ak22 =
            *static_cast<const ibis::array_t<signed char>*>(k22.getArray());
        ibis::array_t<signed char> &ak20 =
            *static_cast<ibis::array_t<signed char>*>(k21.getArray());
        const ibis::array_t<signed char> ak21(ak20);
        ak20.nosharing();
        ierr = merge21S2(k1out, ak20, k1in1, ak21, k1in2, ak22, v1, v2, ag);
        break;}
    case ibis::UBYTE: {
        const ibis::array_t<unsigned char> &ak22 =
            *static_cast<const ibis::array_t<unsigned char>*>(k22.getArray());
        ibis::array_t<unsigned char> &ak20 =
            *static_cast<ibis::array_t<unsigned char>*>(k21.getArray());
        const ibis::array_t<unsigned char> ak21(ak20);
        ak20.nosharing();
        ierr = merge21S2(k1out, ak20, k1in1, ak21, k1in2, ak22, v1, v2, ag);
        break;}
    case ibis::SHORT: {
        const ibis::array_t<int16_t> &ak22 =
            *static_cast<const ibis::array_t<int16_t>*>(k22.getArray());
        ibis::array_t<int16_t> &ak20 =
            *static_cast<ibis::array_t<int16_t>*>(k21.getArray());
        const ibis::array_t<int16_t> ak21(ak20);
        ak20.nosharing();
        ierr = merge21S2(k1out, ak20, k1in1, ak21, k1in2, ak22, v1, v2, ag);
        break;}
    case ibis::USHORT: {
        const ibis::array_t<uint16_t> &ak22 =
            *static_cast<const ibis::array_t<uint16_t>*>(k22.getArray());
        ibis::array_t<uint16_t> &ak20 =
            *static_cast<ibis::array_t<uint16_t>*>(k21.getArray());
        const ibis::array_t<uint16_t> ak21(ak20);
        ak20.nosharing();
        ierr = merge21S2(k1out, ak20, k1in1, ak21, k1in2, ak22, v1, v2, ag);
        break;}
    case ibis::INT: {
        const ibis::array_t<int32_t> &ak22 =
            *static_cast<const ibis::array_t<int32_t>*>(k22.getArray());
        ibis::array_t<int32_t> &ak20 =
            *static_cast<ibis::array_t<int32_t>*>(k21.getArray());
        const ibis::array_t<int32_t> ak21(ak20);
        ak20.nosharing();
        ierr = merge21S2(k1out, ak20, k1in1, ak21, k1in2, ak22, v1, v2, ag);
        break;}
    case ibis::UINT: {
        const ibis::array_t<uint32_t> &ak22 =
            *static_cast<const ibis::array_t<uint32_t>*>(k22.getArray());
        ibis::array_t<uint32_t> &ak20 =
            *static_cast<ibis::array_t<uint32_t>*>(k21.getArray());
        const ibis::array_t<uint32_t> ak21(ak20);
        ak20.nosharing();
        ierr = merge21S2(k1out, ak20, k1in1, ak21, k1in2, ak22, v1, v2, ag);
        break;}
    case ibis::LONG: {
        const ibis::array_t<int64_t> &ak22 =
            *static_cast<const ibis::array_t<int64_t>*>(k22.getArray());
        ibis::array_t<int64_t> &ak20 =
            *static_cast<ibis::array_t<int64_t>*>(k21.getArray());
        const ibis::array_t<int64_t> ak21(ak20);
        ak20.nosharing();
        ierr = merge21S2(k1out, ak20, k1in1, ak21, k1in2, ak22, v1, v2, ag);
        break;}
    case ibis::ULONG: {
        const ibis::array_t<uint64_t> &ak22 =
            *static_cast<const ibis::array_t<uint64_t>*>(k22.getArray());
        ibis::array_t<uint64_t> &ak20 =
            *static_cast<ibis::array_t<uint64_t>*>(k21.getArray());
        const ibis::array_t<uint64_t> ak21(ak20);
        ak20.nosharing();
        ierr = merge21S2(k1out, ak20, k1in1, ak21, k1in2, ak22, v1, v2, ag);
        break;}
    case ibis::FLOAT: {
        const ibis::array_t<float> &ak22 =
            *static_cast<const ibis::array_t<float>*>(k22.getArray());
        ibis::array_t<float> &ak20 =
            *static_cast<ibis::array_t<float>*>(k21.getArray());
        const ibis::array_t<float> ak21(ak20);
        ak20.nosharing();
        ierr = merge21S2(k1out, ak20, k1in1, ak21, k1in2, ak22, v1, v2, ag);
        break;}
    case ibis::DOUBLE: {
        const ibis::array_t<double> &ak22 =
            *static_cast<const ibis::array_t<double>*>(k22.getArray());
        ibis::array_t<double> &ak20 =
            *static_cast<ibis::array_t<double>*>(k21.getArray());
        const ibis::array_t<double> ak21(ak20);
        ak20.nosharing();
        ierr = merge21S2(k1out, ak20, k1in1, ak21, k1in2, ak22, v1, v2, ag);
        break;}
    case ibis::CATEGORY:
    case ibis::TEXT: {
        const std::vector<std::string> &ak22 =
            *static_cast<const std::vector<std::string>*>(k22.getArray());
        std::vector<std::string> &ak20 =
            *static_cast<std::vector<std::string>*>(k21.getArray());
        const std::vector<std::string> ak21(ak20);
        ierr = merge21S4(k1out, ak20, k1in1, ak21, k1in2, ak22, v1, v2, ag);
        break;}
    }

    return ierr;
} // ibis::bord::merge21S1

template <typename Tk2>
int ibis::bord::merge21S2(std::vector<std::string> &k1out,
                          ibis::array_t<Tk2> &k2out,
                          const std::vector<std::string> &k1in1,
                          const ibis::array_t<Tk2> &k2in1,
                          const std::vector<std::string> &k1in2,
                          const ibis::array_t<Tk2> &k2in2,
                          ibis::bord::column &v1,
                          const ibis::bord::column &v2,
                          ibis::selectClause::AGREGADO ag) {
    int ierr = -1;
    if (v1.type() != v2.type()) return ierr;

    switch (v1.type()) {
    default:
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bord::merge21S2 can not deal with v1 ("
            << v1.name() << ") of type "
            << ibis::TYPESTRING[(int)v1.type()];
        return ierr;
    case ibis::BYTE: {
        const ibis::array_t<signed char> &av2 =
            *static_cast<const ibis::array_t<signed char>*>(v2.getArray());
        ibis::array_t<signed char> &av0 =
            *static_cast<ibis::array_t<signed char>*>(v1.getArray());
        const ibis::array_t<signed char> av1(av0);
        av0.nosharing();
        ierr = merge21S3(k1out, k2out, av0,
                         k1in1, k2in1, av1,
                         k1in2, k2in2, av2, ag);
        break;}
    case ibis::UBYTE: {
        const ibis::array_t<unsigned char> &av2 =
            *static_cast<const ibis::array_t<unsigned char>*>(v2.getArray());
        ibis::array_t<unsigned char> &av0 =
            *static_cast<ibis::array_t<unsigned char>*>(v1.getArray());
        const ibis::array_t<unsigned char> av1(av0);
        av0.nosharing();
        ierr = merge21S3(k1out, k2out, av0,
                         k1in1, k2in1, av1,
                         k1in2, k2in2, av2, ag);
        break;}
    case ibis::SHORT: {
        const ibis::array_t<int16_t> &av2 =
            *static_cast<const ibis::array_t<int16_t>*>(v2.getArray());
        ibis::array_t<int16_t> &av0 =
            *static_cast<ibis::array_t<int16_t>*>(v1.getArray());
        const ibis::array_t<int16_t> av1(av0);
        av0.nosharing();
        ierr = merge21S3(k1out, k2out, av0,
                         k1in1, k2in1, av1,
                         k1in2, k2in2, av2, ag);
        break;}
    case ibis::USHORT: {
        const ibis::array_t<uint16_t> &av2 =
            *static_cast<const ibis::array_t<uint16_t>*>(v2.getArray());
        ibis::array_t<uint16_t> &av0 =
            *static_cast<ibis::array_t<uint16_t>*>(v1.getArray());
        const ibis::array_t<uint16_t> av1(av0);
        av0.nosharing();
        ierr = merge21S3(k1out, k2out, av0,
                         k1in1, k2in1, av1,
                         k1in2, k2in2, av2, ag);
        break;}
    case ibis::INT: {
        const ibis::array_t<int32_t> &av2 =
            *static_cast<const ibis::array_t<int32_t>*>(v2.getArray());
        ibis::array_t<int32_t> &av0 =
            *static_cast<ibis::array_t<int32_t>*>(v1.getArray());
        const ibis::array_t<int32_t> av1(av0);
        av0.nosharing();
        ierr = merge21S3(k1out, k2out, av0,
                         k1in1, k2in1, av1,
                         k1in2, k2in2, av2, ag);
        break;}
    case ibis::UINT: {
        const ibis::array_t<uint32_t> &av2 =
            *static_cast<const ibis::array_t<uint32_t>*>(v2.getArray());
        ibis::array_t<uint32_t> &av0 =
            *static_cast<ibis::array_t<uint32_t>*>(v1.getArray());
        const ibis::array_t<uint32_t> av1(av0);
        av0.nosharing();
        ierr = merge21S3(k1out, k2out, av0,
                         k1in1, k2in1, av1,
                         k1in2, k2in2, av2, ag);
        break;}
    case ibis::LONG: {
        const ibis::array_t<int64_t> &av2 =
            *static_cast<const ibis::array_t<int64_t>*>(v2.getArray());
        ibis::array_t<int64_t> &av0 =
            *static_cast<ibis::array_t<int64_t>*>(v1.getArray());
        const ibis::array_t<int64_t> av1(av0);
        av0.nosharing();
        ierr = merge21S3(k1out, k2out, av0,
                         k1in1, k2in1, av1,
                         k1in2, k2in2, av2, ag);
        break;}
    case ibis::ULONG: {
        const ibis::array_t<uint64_t> &av2 =
            *static_cast<const ibis::array_t<uint64_t>*>(v2.getArray());
        ibis::array_t<uint64_t> &av0 =
            *static_cast<ibis::array_t<uint64_t>*>(v1.getArray());
        const ibis::array_t<uint64_t> av1(av0);
        av0.nosharing();
        ierr = merge21S3(k1out, k2out, av0,
                         k1in1, k2in1, av1,
                         k1in2, k2in2, av2, ag);
        break;}
    case ibis::FLOAT: {
        const ibis::array_t<float> &av2 =
            *static_cast<const ibis::array_t<float>*>(v2.getArray());
        ibis::array_t<float> &av0 =
            *static_cast<ibis::array_t<float>*>(v1.getArray());
        const ibis::array_t<float> av1(av0);
        av0.nosharing();
        ierr = merge21S3(k1out, k2out, av0,
                         k1in1, k2in1, av1,
                         k1in2, k2in2, av2, ag);
        break;}
    case ibis::DOUBLE: {
        const ibis::array_t<double> &av2 =
            *static_cast<const ibis::array_t<double>*>(v2.getArray());
        ibis::array_t<double> &av0 =
            *static_cast<ibis::array_t<double>*>(v1.getArray());
        const ibis::array_t<double> av1(av0);
        av0.nosharing();
        ierr = merge21S3(k1out, k2out, av0,
                         k1in1, k2in1, av1,
                         k1in2, k2in2, av2, ag);
        break;}
    }

    return ierr;
} // ibis::bord::merge21S2

template <typename Tk2, typename Tv>
int ibis::bord::merge21S3(std::vector<std::string> &k1out,
                          ibis::array_t<Tk2> &k2out,
                          ibis::array_t<Tv>  &vout,
                          const std::vector<std::string> &k1in1,
                          const ibis::array_t<Tk2> &k2in1,
                          const ibis::array_t<Tv>  &vin1,
                          const std::vector<std::string> &k1in2,
                          const ibis::array_t<Tk2> &k2in2,
                          const ibis::array_t<Tv>  &vin2,
                          ibis::selectClause::AGREGADO av) {
    k1out.clear();
    k2out.clear();
    vout.clear();
    int ierr = -1;
    if (k1in1.size() != k2in1.size() || k1in1.size() != vin1.size() ||
        k1in2.size() != k2in2.size() || k1in2.size() != vin2.size())
        return ierr;

    size_t j1 = 0; // for k1in1, k2in1, vin1
    size_t j2 = 0; // for k1in2, k2in2, vin2
    while (j1 < k1in1.size() && j2 < k1in2.size()) {
        if (k1in1[j1] == k1in2[j2]) { // same k1
            if (k2in1[j1] == k2in2[j2]) { // same k2
                switch (av) {
                default:
                    return ierr;
                case ibis::selectClause::CNT:
                case ibis::selectClause::SUM:
                    vout.push_back(vin1[j1] + vin2[j2]);
                    break;
                case ibis::selectClause::MAX:
                    vout.push_back(vin1[j1] >= vin2[j2] ? vin1[j1] : vin2[j2]);
                    break;
                case ibis::selectClause::MIN:
                    vout.push_back(vin1[j1] <= vin2[j2] ? vin1[j1] : vin2[j2]);
                    break;
                }
                k1out.push_back(k1in1[j1]);
                k2out.push_back(k2in1[j1]);
                ++ j1;
                ++ j2;
            }
            else if (k2in1[j1] < k2in2[j2]) { // copy from *1
                k1out.push_back(k1in1[j1]);
                k2out.push_back(k2in1[j1]);
                vout.push_back(vin1[j1]);
                ++ j1;
            }
            else { // copy from *2
                k1out.push_back(k1in2[j2]);
                k2out.push_back(k2in2[j2]);
                vout.push_back(vin2[j2]);
                ++ j2;
            }
        }
        else if (k1in1[j1] < k1in2[j2]) { // copy from *1
            k1out.push_back(k1in1[j1]);
            k2out.push_back(k2in1[j1]);
            vout.push_back(vin1[j1]);
            ++ j1;
        }
        else { // copy from *2
            k1out.push_back(k1in2[j2]);
            k2out.push_back(k2in2[j2]);
            vout.push_back(vin2[j2]);
            ++ j2;
        }
    }

    while (j1 < k1in1.size()) {
        k1out.push_back(k1in1[j1]);
        k2out.push_back(k2in1[j1]);
        vout.push_back(vin1[j1]);
        ++ j1;
    }

    while (j2 < k1in2.size()) {
        k1out.push_back(k1in2[j2]);
        k2out.push_back(k2in2[j2]);
        vout.push_back(vin2[j2]);
        ++ j2;
    }

    return k1out.size();
} // ibis::bord::merge21S3

int ibis::bord::merge21S4(std::vector<std::string> &k1out,
                          std::vector<std::string> &k2out,
                          const std::vector<std::string> &k1in1,
                          const std::vector<std::string> &k2in1,
                          const std::vector<std::string> &k1in2,
                          const std::vector<std::string> &k2in2,
                          ibis::bord::column &v1,
                          const ibis::bord::column &v2,
                          ibis::selectClause::AGREGADO ag) {
    int ierr = -1;
    if (v1.type() != v2.type()) return ierr;

    switch (v1.type()) {
    default:
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bord::merge21S4 can not deal with v1 ("
            << v1.name() << ") of type "
            << ibis::TYPESTRING[(int)v1.type()];
        return ierr;
    case ibis::BYTE: {
        const ibis::array_t<signed char> &av2 =
            *static_cast<const ibis::array_t<signed char>*>(v2.getArray());
        ibis::array_t<signed char> &av0 =
            *static_cast<ibis::array_t<signed char>*>(v1.getArray());
        const ibis::array_t<signed char> av1(av0);
        av0.nosharing();
        ierr = merge21S5(k1out, k2out, av0,
                         k1in1, k2in1, av1,
                         k1in2, k2in2, av2, ag);
        break;}
    case ibis::UBYTE: {
        const ibis::array_t<unsigned char> &av2 =
            *static_cast<const ibis::array_t<unsigned char>*>(v2.getArray());
        ibis::array_t<unsigned char> &av0 =
            *static_cast<ibis::array_t<unsigned char>*>(v1.getArray());
        const ibis::array_t<unsigned char> av1(av0);
        av0.nosharing();
        ierr = merge21S5(k1out, k2out, av0,
                         k1in1, k2in1, av1,
                         k1in2, k2in2, av2, ag);
        break;}
    case ibis::SHORT: {
        const ibis::array_t<int16_t> &av2 =
            *static_cast<const ibis::array_t<int16_t>*>(v2.getArray());
        ibis::array_t<int16_t> &av0 =
            *static_cast<ibis::array_t<int16_t>*>(v1.getArray());
        const ibis::array_t<int16_t> av1(av0);
        av0.nosharing();
        ierr = merge21S5(k1out, k2out, av0,
                         k1in1, k2in1, av1,
                         k1in2, k2in2, av2, ag);
        break;}
    case ibis::USHORT: {
        const ibis::array_t<uint16_t> &av2 =
            *static_cast<const ibis::array_t<uint16_t>*>(v2.getArray());
        ibis::array_t<uint16_t> &av0 =
            *static_cast<ibis::array_t<uint16_t>*>(v1.getArray());
        const ibis::array_t<uint16_t> av1(av0);
        av0.nosharing();
        ierr = merge21S5(k1out, k2out, av0,
                         k1in1, k2in1, av1,
                         k1in2, k2in2, av2, ag);
        break;}
    case ibis::INT: {
        const ibis::array_t<int32_t> &av2 =
            *static_cast<const ibis::array_t<int32_t>*>(v2.getArray());
        ibis::array_t<int32_t> &av0 =
            *static_cast<ibis::array_t<int32_t>*>(v1.getArray());
        const ibis::array_t<int32_t> av1(av0);
        av0.nosharing();
        ierr = merge21S5(k1out, k2out, av0,
                         k1in1, k2in1, av1,
                         k1in2, k2in2, av2, ag);
        break;}
    case ibis::UINT: {
        const ibis::array_t<uint32_t> &av2 =
            *static_cast<const ibis::array_t<uint32_t>*>(v2.getArray());
        ibis::array_t<uint32_t> &av0 =
            *static_cast<ibis::array_t<uint32_t>*>(v1.getArray());
        const ibis::array_t<uint32_t> av1(av0);
        av0.nosharing();
        ierr = merge21S5(k1out, k2out, av0,
                         k1in1, k2in1, av1,
                         k1in2, k2in2, av2, ag);
        break;}
    case ibis::LONG: {
        const ibis::array_t<int64_t> &av2 =
            *static_cast<const ibis::array_t<int64_t>*>(v2.getArray());
        ibis::array_t<int64_t> &av0 =
            *static_cast<ibis::array_t<int64_t>*>(v1.getArray());
        const ibis::array_t<int64_t> av1(av0);
        av0.nosharing();
        ierr = merge21S5(k1out, k2out, av0,
                         k1in1, k2in1, av1,
                         k1in2, k2in2, av2, ag);
        break;}
    case ibis::ULONG: {
        const ibis::array_t<uint64_t> &av2 =
            *static_cast<const ibis::array_t<uint64_t>*>(v2.getArray());
        ibis::array_t<uint64_t> &av0 =
            *static_cast<ibis::array_t<uint64_t>*>(v1.getArray());
        const ibis::array_t<uint64_t> av1(av0);
        av0.nosharing();
        ierr = merge21S5(k1out, k2out, av0,
                         k1in1, k2in1, av1,
                         k1in2, k2in2, av2, ag);
        break;}
    case ibis::FLOAT: {
        const ibis::array_t<float> &av2 =
            *static_cast<const ibis::array_t<float>*>(v2.getArray());
        ibis::array_t<float> &av0 =
            *static_cast<ibis::array_t<float>*>(v1.getArray());
        const ibis::array_t<float> av1(av0);
        av0.nosharing();
        ierr = merge21S5(k1out, k2out, av0,
                         k1in1, k2in1, av1,
                         k1in2, k2in2, av2, ag);
        break;}
    case ibis::DOUBLE: {
        const ibis::array_t<double> &av2 =
            *static_cast<const ibis::array_t<double>*>(v2.getArray());
        ibis::array_t<double> &av0 =
            *static_cast<ibis::array_t<double>*>(v1.getArray());
        const ibis::array_t<double> av1(av0);
        av0.nosharing();
        ierr = merge21S5(k1out, k2out, av0,
                         k1in1, k2in1, av1,
                         k1in2, k2in2, av2, ag);
        break;}
    }

    return ierr;
} // ibis::bord::merge21S4

template <typename Tv>
int ibis::bord::merge21S5(std::vector<std::string> &k1out,
                          std::vector<std::string> &k2out,
                          ibis::array_t<Tv>  &vout,
                          const std::vector<std::string> &k1in1,
                          const std::vector<std::string> &k2in1,
                          const ibis::array_t<Tv>  &vin1,
                          const std::vector<std::string> &k1in2,
                          const std::vector<std::string> &k2in2,
                          const ibis::array_t<Tv>  &vin2,
                          ibis::selectClause::AGREGADO av) {
    k1out.clear();
    k2out.clear();
    vout.clear();
    int ierr = -1;
    if (k1in1.size() != k2in1.size() || k1in1.size() != vin1.size() ||
        k1in2.size() != k2in2.size() || k1in2.size() != vin2.size())
        return ierr;

    size_t j1 = 0; // for k1in1, k2in1, vin1
    size_t j2 = 0; // for k1in2, k2in2, vin2
    while (j1 < k1in1.size() && j2 < k1in2.size()) {
        if (k1in1[j1] == k1in2[j2]) { // same k1
            if (k2in1[j1] == k2in2[j2]) { // same k2
                switch (av) {
                default:
                    return ierr;
                case ibis::selectClause::CNT:
                case ibis::selectClause::SUM:
                    vout.push_back(vin1[j1] + vin2[j2]);
                    break;
                case ibis::selectClause::MAX:
                    vout.push_back(vin1[j1] >= vin2[j2] ? vin1[j1] : vin2[j2]);
                    break;
                case ibis::selectClause::MIN:
                    vout.push_back(vin1[j1] <= vin2[j2] ? vin1[j1] : vin2[j2]);
                    break;
                }
                k1out.push_back(k1in1[j1]);
                k2out.push_back(k2in1[j1]);
                ++ j1;
                ++ j2;
            }
            else if (k2in1[j1] < k2in2[j2]) { // copy from *1
                k1out.push_back(k1in1[j1]);
                k2out.push_back(k2in1[j1]);
                vout.push_back(vin1[j1]);
                ++ j1;
            }
            else { // copy from *2
                k1out.push_back(k1in2[j2]);
                k2out.push_back(k2in2[j2]);
                vout.push_back(vin2[j2]);
                ++ j2;
            }
        }
        else if (k1in1[j1] < k1in2[j2]) { // copy from *1
            k1out.push_back(k1in1[j1]);
            k2out.push_back(k2in1[j1]);
            vout.push_back(vin1[j1]);
            ++ j1;
        }
        else { // copy from *2
            k1out.push_back(k1in2[j2]);
            k2out.push_back(k2in2[j2]);
            vout.push_back(vin2[j2]);
            ++ j2;
        }
    }

    while (j1 < k1in1.size()) {
        k1out.push_back(k1in1[j1]);
        k2out.push_back(k2in1[j1]);
        vout.push_back(vin1[j1]);
        ++ j1;
    }

    while (j2 < k1in2.size()) {
        k1out.push_back(k1in2[j2]);
        k2out.push_back(k2in2[j2]);
        vout.push_back(vin2[j2]);
        ++ j2;
    }

    return k1out.size();
} // ibis::bord::merge21S5

template <typename Tk1>
int ibis::bord::merge21S6(ibis::array_t<Tk1> &k1out,
                          std::vector<std::string> &k2out,
                          const ibis::array_t<Tk1> &k1in1,
                          const std::vector<std::string> &k2in1,
                          const ibis::array_t<Tk1> &k1in2,
                          const std::vector<std::string> &k2in2,
                          ibis::bord::column &v1,
                          const ibis::bord::column &v2,
                          ibis::selectClause::AGREGADO ag) {
    int ierr = -1;
    if (v1.type() != v2.type()) return ierr;

    switch (v1.type()) {
    default:
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- bord::merge21S6 can not deal with v1 ("
            << v1.name() << ") of type "
            << ibis::TYPESTRING[(int)v1.type()];
        return ierr;
    case ibis::BYTE: {
        const ibis::array_t<signed char> &av2 =
            *static_cast<const ibis::array_t<signed char>*>(v2.getArray());
        ibis::array_t<signed char> &av0 =
            *static_cast<ibis::array_t<signed char>*>(v1.getArray());
        const ibis::array_t<signed char> av1(av0);
        av0.nosharing();
        ierr = merge21S7(k1out, k2out, av0,
                         k1in1, k2in1, av1,
                         k1in2, k2in2, av2, ag);
        break;}
    case ibis::UBYTE: {
        const ibis::array_t<unsigned char> &av2 =
            *static_cast<const ibis::array_t<unsigned char>*>(v2.getArray());
        ibis::array_t<unsigned char> &av0 =
            *static_cast<ibis::array_t<unsigned char>*>(v1.getArray());
        const ibis::array_t<unsigned char> av1(av0);
        av0.nosharing();
        ierr = merge21S7(k1out, k2out, av0,
                         k1in1, k2in1, av1,
                         k1in2, k2in2, av2, ag);
        break;}
    case ibis::SHORT: {
        const ibis::array_t<int16_t> &av2 =
            *static_cast<const ibis::array_t<int16_t>*>(v2.getArray());
        ibis::array_t<int16_t> &av0 =
            *static_cast<ibis::array_t<int16_t>*>(v1.getArray());
        const ibis::array_t<int16_t> av1(av0);
        av0.nosharing();
        ierr = merge21S7(k1out, k2out, av0,
                         k1in1, k2in1, av1,
                         k1in2, k2in2, av2, ag);
        break;}
    case ibis::USHORT: {
        const ibis::array_t<uint16_t> &av2 =
            *static_cast<const ibis::array_t<uint16_t>*>(v2.getArray());
        ibis::array_t<uint16_t> &av0 =
            *static_cast<ibis::array_t<uint16_t>*>(v1.getArray());
        const ibis::array_t<uint16_t> av1(av0);
        av0.nosharing();
        ierr = merge21S7(k1out, k2out, av0,
                         k1in1, k2in1, av1,
                         k1in2, k2in2, av2, ag);
        break;}
    case ibis::INT: {
        const ibis::array_t<int32_t> &av2 =
            *static_cast<const ibis::array_t<int32_t>*>(v2.getArray());
        ibis::array_t<int32_t> &av0 =
            *static_cast<ibis::array_t<int32_t>*>(v1.getArray());
        const ibis::array_t<int32_t> av1(av0);
        av0.nosharing();
        ierr = merge21S7(k1out, k2out, av0,
                         k1in1, k2in1, av1,
                         k1in2, k2in2, av2, ag);
        break;}
    case ibis::UINT: {
        const ibis::array_t<uint32_t> &av2 =
            *static_cast<const ibis::array_t<uint32_t>*>(v2.getArray());
        ibis::array_t<uint32_t> &av0 =
            *static_cast<ibis::array_t<uint32_t>*>(v1.getArray());
        const ibis::array_t<uint32_t> av1(av0);
        av0.nosharing();
        ierr = merge21S7(k1out, k2out, av0,
                         k1in1, k2in1, av1,
                         k1in2, k2in2, av2, ag);
        break;}
    case ibis::LONG: {
        const ibis::array_t<int64_t> &av2 =
            *static_cast<const ibis::array_t<int64_t>*>(v2.getArray());
        ibis::array_t<int64_t> &av0 =
            *static_cast<ibis::array_t<int64_t>*>(v1.getArray());
        const ibis::array_t<int64_t> av1(av0);
        av0.nosharing();
        ierr = merge21S7(k1out, k2out, av0,
                         k1in1, k2in1, av1,
                         k1in2, k2in2, av2, ag);
        break;}
    case ibis::ULONG: {
        const ibis::array_t<uint64_t> &av2 =
            *static_cast<const ibis::array_t<uint64_t>*>(v2.getArray());
        ibis::array_t<uint64_t> &av0 =
            *static_cast<ibis::array_t<uint64_t>*>(v1.getArray());
        const ibis::array_t<uint64_t> av1(av0);
        av0.nosharing();
        ierr = merge21S7(k1out, k2out, av0,
                         k1in1, k2in1, av1,
                         k1in2, k2in2, av2, ag);
        break;}
    case ibis::FLOAT: {
        const ibis::array_t<float> &av2 =
            *static_cast<const ibis::array_t<float>*>(v2.getArray());
        ibis::array_t<float> &av0 =
            *static_cast<ibis::array_t<float>*>(v1.getArray());
        const ibis::array_t<float> av1(av0);
        av0.nosharing();
        ierr = merge21S7(k1out, k2out, av0,
                         k1in1, k2in1, av1,
                         k1in2, k2in2, av2, ag);
        break;}
    case ibis::DOUBLE: {
        const ibis::array_t<double> &av2 =
            *static_cast<const ibis::array_t<double>*>(v2.getArray());
        ibis::array_t<double> &av0 =
            *static_cast<ibis::array_t<double>*>(v1.getArray());
        const ibis::array_t<double> av1(av0);
        av0.nosharing();
        ierr = merge21S7(k1out, k2out, av0,
                         k1in1, k2in1, av1,
                         k1in2, k2in2, av2, ag);
        break;}
    }

    return ierr;
} // ibis::bord::merge21S6

template <typename Tk1, typename Tv>
int ibis::bord::merge21S7(ibis::array_t<Tk1> &k1out,
                          std::vector<std::string> &k2out,
                          ibis::array_t<Tv>  &vout,
                          const ibis::array_t<Tk1> &k1in1,
                          const std::vector<std::string> &k2in1,
                          const ibis::array_t<Tv>  &vin1,
                          const ibis::array_t<Tk1> &k1in2,
                          const std::vector<std::string> &k2in2,
                          const ibis::array_t<Tv>  &vin2,
                          ibis::selectClause::AGREGADO av) {
    k1out.clear();
    k2out.clear();
    vout.clear();
    int ierr = -1;
    if (k1in1.size() != k2in1.size() || k1in1.size() != vin1.size() ||
        k1in2.size() != k2in2.size() || k1in2.size() != vin2.size())
        return ierr;

    size_t j1 = 0; // for k1in1, k2in1, vin1
    size_t j2 = 0; // for k1in2, k2in2, vin2
    while (j1 < k1in1.size() && j2 < k1in2.size()) {
        if (k1in1[j1] == k1in2[j2]) { // same k1
            if (k2in1[j1] == k2in2[j2]) { // same k2
                switch (av) {
                default:
                    return ierr;
                case ibis::selectClause::CNT:
                case ibis::selectClause::SUM:
                    vout.push_back(vin1[j1] + vin2[j2]);
                    break;
                case ibis::selectClause::MAX:
                    vout.push_back(vin1[j1] >= vin2[j2] ? vin1[j1] : vin2[j2]);
                    break;
                case ibis::selectClause::MIN:
                    vout.push_back(vin1[j1] <= vin2[j2] ? vin1[j1] : vin2[j2]);
                    break;
                }
                k1out.push_back(k1in1[j1]);
                k2out.push_back(k2in1[j1]);
                ++ j1;
                ++ j2;
            }
            else if (k2in1[j1] < k2in2[j2]) { // copy from *1
                k1out.push_back(k1in1[j1]);
                k2out.push_back(k2in1[j1]);
                vout.push_back(vin1[j1]);
                ++ j1;
            }
            else { // copy from *2
                k1out.push_back(k1in2[j2]);
                k2out.push_back(k2in2[j2]);
                vout.push_back(vin2[j2]);
                ++ j2;
            }
        }
        else if (k1in1[j1] < k1in2[j2]) { // copy from *1
            k1out.push_back(k1in1[j1]);
            k2out.push_back(k2in1[j1]);
            vout.push_back(vin1[j1]);
            ++ j1;
        }
        else { // copy from *2
            k1out.push_back(k1in2[j2]);
            k2out.push_back(k2in2[j2]);
            vout.push_back(vin2[j2]);
            ++ j2;
        }
    }

    while (j1 < k1in1.size()) {
        k1out.push_back(k1in1[j1]);
        k2out.push_back(k2in1[j1]);
        vout.push_back(vin1[j1]);
        ++ j1;
    }

    while (j2 < k1in2.size()) {
        k1out.push_back(k1in2[j2]);
        k2out.push_back(k2in2[j2]);
        vout.push_back(vin2[j2]);
        ++ j2;
    }

    return k1out.size();
} // ibis::bord::merge21S7
