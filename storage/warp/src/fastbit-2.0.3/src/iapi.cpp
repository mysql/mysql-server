/* File: $Id$
   Author: John Wu <John.Wu at acm.org>
      Lawrence Berkeley National Laboratory
   Copyright (c) 2001-20164-2014 the Regents of the University of California
*/
#include "iapi.h"
#include "bord.h"
#include "ibin.h"
#include "irelic.h"
#include "countQuery.h"
#include <memory>       // std::unique_ptr
#include <unordered_map>

/// A global variable in the file scope to hold all the active arrays known
/// to this interface.
static std::vector<ibis::bord::column*> __fastbit_iapi_all_arrays;

/// Allow for a quick look up of column objects using the address of the base
/// data.
typedef std::unordered_map<const void*, uint64_t> FastBitIAPIAddressMap;
static FastBitIAPIAddressMap __fastbit_iapi_address_map;
/// Allow for a quick look up of column objects using the name of the
/// column.
typedef std::unordered_map<const char*, uint64_t, std::hash<const char*>,
                           std::equal_to<const char*> > FastBitIAPINameMap;
static FastBitIAPINameMap __fastbit_iapi_name_map;
/// Store the query results to avoid recomputing them.
typedef std::unordered_map<FastBitSelectionHandle, ibis::bitvector*>
FastBitIAPISelectionList;
static FastBitIAPISelectionList __fastbit_iapi_selection_list;

/// A global lock with the file scope.
static pthread_mutex_t __fastbit_iapi_lock = PTHREAD_MUTEX_INITIALIZER;

// A local function for converting the types.
inline ibis::TYPE_T __fastbit_iapi_convert_data_type(FastBitDataType t) {
    switch (t) {
    default:
    case FastBitDataTypeUnknown:
        return ibis::UNKNOWN_TYPE;
    case FastBitDataTypeBitCompressed:
        return ibis::BIT;
    case FastBitDataTypeByte:
        return ibis::BYTE;
    case FastBitDataTypeUByte:
        return ibis::UBYTE;
    case FastBitDataTypeShort:
        return ibis::SHORT;
    case FastBitDataTypeUShort:
        return ibis::USHORT;
    case FastBitDataTypeInt:
        return ibis::INT;
    case FastBitDataTypeUInt:
        return ibis::UINT;
    case FastBitDataTypeLong:
        return ibis::LONG;
    case FastBitDataTypeULong:
        return ibis::ULONG;
    case FastBitDataTypeFloat:
        return ibis::FLOAT;
    case FastBitDataTypeDouble:
        return ibis::DOUBLE;
    }
} // __fastbit_iapi_convert_data_type

// A local function for converting a single value to double.  Returns
// FASTBIT_DOUBLE_NULL in case of error.
inline double __fastbit_iapi_convert_data_to_double
(FastBitDataType t, void *v0) {
    double ret = FASTBIT_DOUBLE_NULL;
    switch (t) {
    default:
    case FastBitDataTypeUnknown:
        break;
    case FastBitDataTypeByte:
        ret = *static_cast<signed char*>(v0);
        break;
    case FastBitDataTypeUByte:
        ret = *static_cast<unsigned char*>(v0);
        break;
    case FastBitDataTypeShort:
        ret = *static_cast<int16_t*>(v0);
        break;
    case FastBitDataTypeUShort:
        ret = *static_cast<uint16_t*>(v0);
        break;
    case FastBitDataTypeInt:
        ret = *static_cast<int32_t*>(v0);
        break;
    case FastBitDataTypeUInt:
        ret = *static_cast<uint32_t*>(v0);
        break;
    case FastBitDataTypeLong: {
        int64_t itmp = *static_cast<int64_t*>(v0);
        ret = static_cast<double>(itmp);
        LOGGER(ibis::gVerbose > 0 && itmp != static_cast<int64_t>(ret))
            << "Warning -- __fastbit_iapi_convert_data_to_double converting "
            << itmp << " to " << ret << ", the value has changed";
        break;}
    case FastBitDataTypeULong: {
        uint64_t itmp = *static_cast<uint64_t*>(v0);
        ret = static_cast<double>(itmp);
        LOGGER(ibis::gVerbose > 0 && itmp != static_cast<uint64_t>(ret))
            << "Warning -- __fastbit_iapi_convert_data_to_double converting "
            << itmp << " to " << ret << ", the value has changed";
        break;}
    case FastBitDataTypeFloat:
        ret = *static_cast<float*>(v0);
        break;
    case FastBitDataTypeDouble:
        ret = *static_cast<double*>(v0);
        break;
    }
    return ret;
} // __fastbit_iapi_convert_data_to_double

// Convert comparison operators to FastBit IBIS type.  FastBit IBIS does
// not have an operator for NOT-EQUAL.  This function translates it to
// OP_UNDEFINED.  The caller to this function is to take the OP_UNDEFINED
// return value as NOT-EQUAL.
inline ibis::qExpr::COMPARE
__fastbit_iapi_convert_compare_type(FastBitCompareType t) {
    switch (t) {
    default:
        return ibis::qExpr::OP_UNDEFINED;
    case FastBitCompareLess:
        return ibis::qExpr::OP_LT;
    case FastBitCompareEqual:
        return ibis::qExpr::OP_EQ;
    case FastBitCompareGreater:
        return ibis::qExpr::OP_GT;
    case FastBitCompareLessEqual:
        return ibis::qExpr::OP_LE;
    case FastBitCompareGreaterEqual:
        return ibis::qExpr::OP_GE;
    }
} // __fastbit_iapi_convert_compare_type

/// @note This function assumes the given name is not already in the list
/// of known arrays.
///
/// @note This function returns a nil pointer to indicate error.
///
ibis::bord::column* __fastbit_iapi_register_array
(const char *name, FastBitDataType t, void* addr, uint64_t n) {
    if (name == 0 || *name == 0 || addr == 0 || t == FastBitDataTypeUnknown
        || n == 0)
        return 0;

    LOGGER(ibis::gVerbose > 3)
        << "FastBit IAPI registering array \"" << name << "\" with content at "
        << addr;
    ibis::util::mutexLock
        lock(&__fastbit_iapi_lock, "__fastbit_iapi_register_array");
    FastBitIAPIAddressMap::iterator it = __fastbit_iapi_address_map.find(addr);
    if (it != __fastbit_iapi_address_map.end())
        return __fastbit_iapi_all_arrays[it->second];

    uint64_t pos = __fastbit_iapi_all_arrays.size();
    switch (t) {
    default:
    case FastBitDataTypeUnknown:
        return 0;
    case FastBitDataTypeByte: {
        ibis::array_t<signed char> *buf =
            new ibis::array_t<signed char>((signed char*)addr, n);
        ibis::bord::column *tmp =
            new ibis::bord::column(0, ibis::BYTE, name, buf);
        __fastbit_iapi_all_arrays.push_back(tmp);
        __fastbit_iapi_address_map[addr] = pos;
        __fastbit_iapi_name_map[tmp->name()] = pos;
        return tmp;}
    case FastBitDataTypeUByte: {
        ibis::array_t<unsigned char> *buf
            = new ibis::array_t<unsigned char>((unsigned char*)addr, n);
        ibis::bord::column *tmp =
            new ibis::bord::column(0, ibis::UBYTE, name, buf);
        __fastbit_iapi_all_arrays.push_back(tmp);
        __fastbit_iapi_address_map[addr] = pos;
        __fastbit_iapi_name_map[tmp->name()] = pos;
        return tmp;}
    case FastBitDataTypeShort: {
        ibis::array_t<int16_t> *buf =
            new ibis::array_t<int16_t>((int16_t*)addr, n);
        ibis::bord::column *tmp =
            new ibis::bord::column(0, ibis::SHORT, name, buf);
        __fastbit_iapi_all_arrays.push_back(tmp);
        __fastbit_iapi_address_map[addr] = pos;
        __fastbit_iapi_name_map[tmp->name()] = pos;
        return tmp;}
    case FastBitDataTypeUShort: {
        ibis::array_t<uint16_t> *buf =
            new ibis::array_t<uint16_t>((uint16_t*)addr, n);
        ibis::bord::column *tmp =
            new ibis::bord::column(0, ibis::USHORT, name, buf);
        __fastbit_iapi_all_arrays.push_back(tmp);
        __fastbit_iapi_address_map[addr] = pos;
        __fastbit_iapi_name_map[tmp->name()] = pos;
        return tmp;}
    case FastBitDataTypeInt: {
        ibis::array_t<int32_t> *buf =
            new ibis::array_t<int32_t>((int32_t*)addr, n);
        ibis::bord::column *tmp =
            new ibis::bord::column(0, ibis::INT, name, buf);
        __fastbit_iapi_all_arrays.push_back(tmp);
        __fastbit_iapi_address_map[addr] = pos;
        __fastbit_iapi_name_map[tmp->name()] = pos;
        return tmp;}
    case FastBitDataTypeUInt: {
        ibis::array_t<uint32_t> *buf =
            new ibis::array_t<uint32_t>((uint32_t*)addr, n);
        ibis::bord::column *tmp =
            new ibis::bord::column(0, ibis::UINT, name, buf);
        __fastbit_iapi_all_arrays.push_back(tmp);
        __fastbit_iapi_address_map[addr] = pos;
        __fastbit_iapi_name_map[tmp->name()] = pos;
        return tmp;}
    case FastBitDataTypeLong: {
        ibis::array_t<int64_t> *buf =
            new ibis::array_t<int64_t>((int64_t*)addr, n);
        ibis::bord::column *tmp =
            new ibis::bord::column(0, ibis::LONG, name, buf);
        __fastbit_iapi_all_arrays.push_back(tmp);
        __fastbit_iapi_address_map[addr] = pos;
        __fastbit_iapi_name_map[tmp->name()] = pos;
        return tmp;}
    case FastBitDataTypeULong: {
        ibis::array_t<uint64_t> *buf =
            new ibis::array_t<uint64_t>((uint64_t*)addr, n);
        ibis::bord::column *tmp =
            new ibis::bord::column(0, ibis::ULONG, name, buf);
        __fastbit_iapi_all_arrays.push_back(tmp);
        __fastbit_iapi_address_map[addr] = pos;
        __fastbit_iapi_name_map[tmp->name()] = pos;
        return tmp;}
    case FastBitDataTypeFloat: {
        ibis::array_t<float> *buf =
            new ibis::array_t<float>((float*)addr, n);
        ibis::bord::column *tmp =
            new ibis::bord::column(0, ibis::FLOAT, name, buf);
        __fastbit_iapi_all_arrays.push_back(tmp);
        __fastbit_iapi_address_map[addr] = pos;
        __fastbit_iapi_name_map[tmp->name()] = pos;
        return tmp;}
    case FastBitDataTypeDouble: {
        ibis::array_t<double> *buf =
            new ibis::array_t<double>((double*)addr, n);
        ibis::bord::column *tmp =
            new ibis::bord::column(0, ibis::DOUBLE, name, buf);
        __fastbit_iapi_all_arrays.push_back(tmp);
        __fastbit_iapi_address_map[addr] = pos;
        __fastbit_iapi_name_map[tmp->name()] = pos;
        return tmp;}
    case FastBitDataTypeBitRaw: { // raw bitmap
        const unsigned char *uptr = static_cast<const unsigned char*>(addr);
        ibis::bitvector bv;
        while (bv.size()+8 <= n) {
            bv.appendByte(*uptr);
            ++ uptr;
        }
        if (bv.size() < n) {
            bv += ((*uptr & 0x80) >> 7);
        }
        if (bv.size() < n) {
            bv += ((*uptr & 0x40) >> 6);
        }
        if (bv.size() < n) {
            bv += ((*uptr & 0x20) >> 5);
        }
        if (bv.size() < n) {
            bv += ((*uptr & 0x10) >> 4);
        }
        if (bv.size() < n) {
            bv += ((*uptr & 0x08) >> 3);
        }
        if (bv.size() < n) {
            bv += ((*uptr & 0x04) >> 2);
        }
        if (bv.size() < n) {
            bv += ((*uptr & 0x02) >> 1);
        }
        ibis::bord::column *tmp =
            new ibis::bord::column(0, ibis::BIT, name, &bv);
        __fastbit_iapi_all_arrays.push_back(tmp);
        //__fastbit_iapi_address_map[addr] = pos;
        __fastbit_iapi_name_map[tmp->name()] = pos;
        return tmp;}
    case FastBitDataTypeBitCompressed: { // compressed bitmap
        ibis::bord::column *tmp =
            new ibis::bord::column(0, ibis::BIT, name, addr);
        __fastbit_iapi_all_arrays.push_back(tmp);
        //__fastbit_iapi_address_map[addr] = pos;
        __fastbit_iapi_name_map[tmp->name()] = pos;
        return tmp;}
    }
} // __fastbit_iapi_register_array

/// @note This function assumes the given name is not already in the list
/// of known arrays.
///
/// @note This function returns a nil pointer to indicate error.
///
ibis::bord::column* __fastbit_iapi_register_array_nd
(const char *name, FastBitDataType t, void* addr, uint64_t *dims, uint64_t nd) {
    if (name == 0 || *name == 0 || addr == 0 || t == FastBitDataTypeUnknown ||
        dims == 0 || nd == 0)
        return 0;

    uint64_t n = *dims;
    for (unsigned j = 1; j < nd; ++ j)
        n *= dims[j];
    if (n > 0x7FFFFFFFUL) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- __fastbit_iapi_register_array_nd can not proceed "
            "because the number of elements (" << n << ") exceeds 0x7FFFFFFF";
        return 0;
    }

    LOGGER(ibis::gVerbose > 3)
        << "FastBit IAPI registering array \"" << name << "\" with content at "
        << addr;
    ibis::util::mutexLock
        lock(&__fastbit_iapi_lock, "__fastbit_iapi_register_array_nd");
    FastBitIAPIAddressMap::iterator it = __fastbit_iapi_address_map.find(addr);
    if (it != __fastbit_iapi_address_map.end()) {
        __fastbit_iapi_all_arrays[it->second]->setMeshShape(dims, nd);
        return __fastbit_iapi_all_arrays[it->second];
    }

    uint64_t pos = __fastbit_iapi_all_arrays.size();

    switch (t) {
    default:
    case FastBitDataTypeUnknown:
        return 0;
    case FastBitDataTypeByte: {
        ibis::array_t<signed char> *buf =
            new ibis::array_t<signed char>((signed char*)addr, n);
        ibis::bord::column *tmp =
            new ibis::bord::column(ibis::BYTE, name, buf, dims, nd);
        __fastbit_iapi_all_arrays.push_back(tmp);
        __fastbit_iapi_address_map[addr] = pos;
        __fastbit_iapi_name_map[tmp->name()] = pos;
        return tmp;}
    case FastBitDataTypeUByte: {
        ibis::array_t<unsigned char> *buf
            = new ibis::array_t<unsigned char>((unsigned char*)addr, n);
        ibis::bord::column *tmp =
            new ibis::bord::column(ibis::UBYTE, name, buf, dims, nd);
        __fastbit_iapi_all_arrays.push_back(tmp);
        __fastbit_iapi_address_map[addr] = pos;
        __fastbit_iapi_name_map[tmp->name()] = pos;
        return tmp;}
    case FastBitDataTypeShort: {
        ibis::array_t<int16_t> *buf =
            new ibis::array_t<int16_t>((int16_t*)addr, n);
        ibis::bord::column *tmp =
            new ibis::bord::column(ibis::SHORT, name, buf, dims, nd);
        __fastbit_iapi_all_arrays.push_back(tmp);
        __fastbit_iapi_address_map[addr] = pos;
        __fastbit_iapi_name_map[tmp->name()] = pos;
        return tmp;}
    case FastBitDataTypeUShort: {
        ibis::array_t<uint16_t> *buf =
            new ibis::array_t<uint16_t>((uint16_t*)addr, n);
        ibis::bord::column *tmp =
            new ibis::bord::column(ibis::USHORT, name, buf, dims, nd);
        __fastbit_iapi_all_arrays.push_back(tmp);
        __fastbit_iapi_address_map[addr] = pos;
        __fastbit_iapi_name_map[tmp->name()] = pos;
        return tmp;}
    case FastBitDataTypeInt: {
        ibis::array_t<int32_t> *buf =
            new ibis::array_t<int32_t>((int32_t*)addr, n);
        ibis::bord::column *tmp =
            new ibis::bord::column(ibis::INT, name, buf, dims, nd);
        __fastbit_iapi_all_arrays.push_back(tmp);
        __fastbit_iapi_address_map[addr] = pos;
        __fastbit_iapi_name_map[tmp->name()] = pos;
        return tmp;}
    case FastBitDataTypeUInt: {
        ibis::array_t<uint32_t> *buf =
            new ibis::array_t<uint32_t>((uint32_t*)addr, n);
        ibis::bord::column *tmp =
            new ibis::bord::column(ibis::UINT, name, buf, dims, nd);
        __fastbit_iapi_all_arrays.push_back(tmp);
        __fastbit_iapi_address_map[addr] = pos;
        __fastbit_iapi_name_map[tmp->name()] = pos;
        return tmp;}
    case FastBitDataTypeLong: {
        ibis::array_t<int64_t> *buf =
            new ibis::array_t<int64_t>((int64_t*)addr, n);
        ibis::bord::column *tmp =
            new ibis::bord::column(ibis::LONG, name, buf, dims, nd);
        __fastbit_iapi_all_arrays.push_back(tmp);
        __fastbit_iapi_address_map[addr] = pos;
        __fastbit_iapi_name_map[tmp->name()] = pos;
        return tmp;}
    case FastBitDataTypeULong: {
        ibis::array_t<uint64_t> *buf =
            new ibis::array_t<uint64_t>((uint64_t*)addr, n);
        ibis::bord::column *tmp =
            new ibis::bord::column(ibis::ULONG, name, buf, dims, nd);
        __fastbit_iapi_all_arrays.push_back(tmp);
        __fastbit_iapi_address_map[addr] = pos;
        __fastbit_iapi_name_map[tmp->name()] = pos;
        return tmp;}
    case FastBitDataTypeFloat: {
        ibis::array_t<float> *buf =
            new ibis::array_t<float>((float*)addr, n);
        ibis::bord::column *tmp =
            new ibis::bord::column(ibis::FLOAT, name, buf, dims, nd);
        __fastbit_iapi_all_arrays.push_back(tmp);
        __fastbit_iapi_address_map[addr] = pos;
        __fastbit_iapi_name_map[tmp->name()] = pos;
        return tmp;}
    case FastBitDataTypeDouble: {
        ibis::array_t<double> *buf =
            new ibis::array_t<double>((double*)addr, n);
        ibis::bord::column *tmp =
            new ibis::bord::column(ibis::DOUBLE, name, buf, dims, nd);
        __fastbit_iapi_all_arrays.push_back(tmp);
        __fastbit_iapi_address_map[addr] = pos;
        __fastbit_iapi_name_map[tmp->name()] = pos;
        return tmp;}
    }
} // __fastbit_iapi_register_array_nd

/// Extract the address of the data buffer.
inline void* __fastbit_iapi_get_array_addr(const ibis::bord::column &col) {
    void* tmp = col.getArray();
    if (tmp == 0) return tmp;

    switch (col.type()) {
    default:
        return 0;
    case ibis::BYTE:
        return static_cast<ibis::array_t<signed char>*>(col.getArray())->
            begin();
    case ibis::UBYTE:
        return static_cast<ibis::array_t<unsigned char>*>(col.getArray())->
            begin();
    case ibis::SHORT:
        return static_cast<ibis::array_t<int16_t>*>(col.getArray())->
            begin();
    case ibis::USHORT:
        return static_cast<ibis::array_t<uint16_t>*>(col.getArray())->
            begin();
    case ibis::INT:
        return static_cast<ibis::array_t<int32_t>*>(col.getArray())->
            begin();
    case ibis::UINT:
        return static_cast<ibis::array_t<uint32_t>*>(col.getArray())->
            begin();
    case ibis::LONG:
        return static_cast<ibis::array_t<int64_t>*>(col.getArray())->
            begin();
    case ibis::ULONG:
        return static_cast<ibis::array_t<uint64_t>*>(col.getArray())->
            begin();
    case ibis::FLOAT:
        return static_cast<ibis::array_t<float>*>(col.getArray())->
            begin();
    case ibis::DOUBLE:
        return static_cast<ibis::array_t<double>*>(col.getArray())->
            begin();
    }
} // __fastbit_iapi_get_array_addr

/// @note This function assumes the given name is not already in the list
/// of known arrays.
///
/// @note This function returns a nil pointer to indicate error.
///
ibis::bord::column* __fastbit_iapi_register_array_ext
(const char *name, FastBitDataType t, uint64_t *dims, uint64_t nd, void* ctx,
 FastBitReadExtArray rd) {
    if (name == 0 || *name == 0 || t == FastBitDataTypeUnknown ||
        dims == 0 || nd == 0 || rd == 0)
        return 0;

    uint64_t n = *dims;
    for (unsigned j = 1; j < nd; ++ j)
        n *= dims[j];
    if (n > 0x7FFFFFFFUL) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- __fastbit_iapi_register_array_ext can not proceed "
            "because the number of elements (" << n << ") exceeds 0x7FFFFFFF";
        return 0;
    }

    LOGGER(ibis::gVerbose > 3)
        << "FastBit IAPI registering array \"" << name << "\" with a reader "
        "function at " << rd;
    ibis::bord::column *tmp =
        new ibis::bord::column(rd, ctx, dims, nd,
                               __fastbit_iapi_convert_data_type(t), name);
    if (tmp == 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- __fastbit_iapi_register_array_ext failed to create "
            "an ibis::bord::column object under the name " << name;
        return tmp;
    }

    ibis::util::mutexLock
        lock(&__fastbit_iapi_lock, "__fastbit_iapi_register_array_ext");
    uint64_t pos = __fastbit_iapi_all_arrays.size();
    __fastbit_iapi_name_map[tmp->name()] = pos;
    __fastbit_iapi_all_arrays.push_back(tmp);
    return tmp;
} // __fastbit_iapi_register_array_ext

/// @note This function assumes the given name is not already in the list
/// of known arrays.
///
/// @note This function returns a nil pointer to indicate error.
///
/// If the index can not be properly reconstructed, this function returns a
/// nil pointer.
ibis::bord::column* __fastbit_iapi_register_array_index_only
(const char *name, FastBitDataType t, uint64_t *dims, uint64_t nd,
 double *keys, uint64_t nkeys, int64_t *offsets, uint64_t noffsets,
 void* bms, FastBitReadBitmaps rd) {
    if (name == 0 || *name == 0 || t == FastBitDataTypeUnknown ||
        dims == 0 || nd == 0 || keys == 0 || nkeys == 0 ||
        offsets == 0 || noffsets == 0 || rd == 0)
        return 0;
    LOGGER(ibis::gVerbose > 3)
        << "FastBit IAPI registering array \"" << name << "\" (index-only) with "
        "bitmaps at " << bms;

    ibis::bord::column *tmp =
        new ibis::bord::column(static_cast<const ibis::bord*>(0),
                               __fastbit_iapi_convert_data_type(t), name);
    if (tmp == 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- __fastbit_iapi_register_array_index_only failed to "
            "create an ibis::bord::column object under the name " << name;
        return tmp;
    }

    tmp->setDataflag(-2);
    tmp->setMeshShape(dims, nd);
    int ierr = tmp->attachIndex(keys, nkeys, offsets, noffsets, bms, rd);
    if (ierr < 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- __fastbit_iapi_register_array_index_only failed to "
            "reconstitute index from the given information";
        delete tmp;
        return 0;
    }

    ibis::util::mutexLock
        lock(&__fastbit_iapi_lock, "__fastbit_iapi_register_array_index_only");
    uint64_t pos = __fastbit_iapi_all_arrays.size();
    __fastbit_iapi_name_map[tmp->name()] = pos;
    __fastbit_iapi_all_arrays.push_back(tmp);
    return tmp;
} // __fastbit_iapi_register_array_index_only

ibis::bord::column* __fastbit_iapi_array_by_addr(const void *addr) {
    if (addr == 0) return 0;
    FastBitIAPIAddressMap::const_iterator it =
        __fastbit_iapi_address_map.find(addr);
    if (it != __fastbit_iapi_address_map.end()) {
        if (it->second < __fastbit_iapi_all_arrays.size()) {
            LOGGER(ibis::gVerbose > 6)
                << "__fastbit_iapi_array_by_addr found column from address \""
                << addr << "\" as __fastbit_iapi_all_arrays[" << it->second
                << "] (name=" << __fastbit_iapi_all_arrays[it->second]->name()
                << ", description="
                << __fastbit_iapi_all_arrays[it->second]->description()
                << ")";
            return __fastbit_iapi_all_arrays[it->second];
        }
    }
    return 0;
} // __fastbit_iapi_array_by_addr

ibis::bord::column* __fastbit_iapi_array_by_name(const char *name) {
    if (name == 0 || *name == 0) return 0;
    FastBitIAPINameMap::const_iterator it = __fastbit_iapi_name_map.find(name);
    if (it != __fastbit_iapi_name_map.end()) {
        if (it->second < __fastbit_iapi_all_arrays.size()) {
            LOGGER(ibis::gVerbose > 6)
                << "__fastbit_iapi_array_by_name found column named \"" << name
                << "\" as __fastbit_iapi_all_arrays[" << it->second
                << "] (name=" << __fastbit_iapi_all_arrays[it->second]->name()
                << ", description="
                << __fastbit_iapi_all_arrays[it->second]->description()
                << ")";
            return __fastbit_iapi_all_arrays[it->second];
        }
    }
    return 0;
} // __fastbit_iapi_array_by_name

// An internal utility function that actually modify the global variabls
// keeping track of arrays.  The caller must hold a mutex lock.
void __fastbit_iapi_free_array(uint64_t pos) {
    if (pos >= __fastbit_iapi_all_arrays.size()) return;

    ibis::bord::column *col = __fastbit_iapi_all_arrays[pos];
    if (col != 0) { // erase the entry related to col
        void *addr = __fastbit_iapi_get_array_addr(*col);
        if (addr != 0)
            __fastbit_iapi_address_map.erase(addr);
        __fastbit_iapi_name_map.erase(col->name());
        delete col;
    }

    if (pos < __fastbit_iapi_all_arrays.size()-1) {
        // move the last entry in __fastbit_iapi_all_arrays to the position
        // being vacated
        __fastbit_iapi_all_arrays[pos] = __fastbit_iapi_all_arrays.back();
        col = __fastbit_iapi_all_arrays.back();
        void *addr = __fastbit_iapi_get_array_addr(*col);
        if (addr != 0)
            __fastbit_iapi_address_map[addr] = pos;
        __fastbit_iapi_name_map[col->name()] = pos;
    }
    // remove the vacated entry from __fastbit_iapi_all_arrays
    __fastbit_iapi_all_arrays.resize(__fastbit_iapi_all_arrays.size()-1);
} // __fastbit_iapi_free_array

void __fastbit_free_all_arrays() {
    ibis::util::mutexLock
        lock(&__fastbit_iapi_lock, "__fastbit_free_all_arrays");
    for (unsigned j = 0; j < __fastbit_iapi_all_arrays.size(); ++j)
        delete __fastbit_iapi_all_arrays[j];
    __fastbit_iapi_name_map.clear();
    __fastbit_iapi_all_arrays.clear();
    __fastbit_iapi_address_map.clear();
} // __fastbit_free_all_arrays

void __fastbit_free_all_selected() {
    ibis::util::mutexLock
        lock(&__fastbit_iapi_lock, "__fastbit_free_all_selected");
    for (FastBitIAPISelectionList::iterator it =
             __fastbit_iapi_selection_list.begin();
         it != __fastbit_iapi_selection_list.end(); ++ it)
        delete it->second;
    __fastbit_iapi_selection_list.clear();
} // __fastbit_free_all_selected

void fastbit_iapi_reregister_array(uint64_t i) {
    ibis::bord::column *col = __fastbit_iapi_all_arrays[i];
    __fastbit_iapi_name_map[col->name()] = i;
    switch (col->type()) {
    default:
        break;
    case FastBitDataTypeByte: {
        ibis::array_t<signed char> *buf =
            static_cast<ibis::array_t<signed char>*>(col->getArray());
        __fastbit_iapi_address_map[buf->begin()] = i;
        break;}
    case FastBitDataTypeUByte: {
        ibis::array_t<unsigned char> *buf =
            static_cast<ibis::array_t<unsigned char>*>(col->getArray());
        __fastbit_iapi_address_map[buf->begin()] = i;
        break;}
    case FastBitDataTypeShort: {
        ibis::array_t<int16_t> *buf =
            static_cast<ibis::array_t<int16_t>*>(col->getArray());
        __fastbit_iapi_address_map[buf->begin()] = i;
        break;}
    case FastBitDataTypeUShort: {
        ibis::array_t<uint16_t> *buf =
            static_cast<ibis::array_t<uint16_t>*>(col->getArray());
        __fastbit_iapi_address_map[buf->begin()] = i;
        break;}
    case FastBitDataTypeInt: {
        ibis::array_t<int32_t> *buf =
            static_cast<ibis::array_t<int32_t>*>(col->getArray());
        __fastbit_iapi_address_map[buf->begin()] = i;
        break;}
    case FastBitDataTypeUInt: {
        ibis::array_t<uint32_t> *buf =
            static_cast<ibis::array_t<uint32_t>*>(col->getArray());
        __fastbit_iapi_address_map[buf->begin()] = i;
        break;}
    case FastBitDataTypeLong: {
        ibis::array_t<int64_t> *buf =
            static_cast<ibis::array_t<int64_t>*>(col->getArray());
        __fastbit_iapi_address_map[buf->begin()] = i;
        break;}
    case FastBitDataTypeULong: {
        ibis::array_t<uint64_t> *buf =
            static_cast<ibis::array_t<uint64_t>*>(col->getArray());
        __fastbit_iapi_address_map[buf->begin()] = i;
        break;}
    case FastBitDataTypeFloat: {
        ibis::array_t<float> *buf =
            static_cast<ibis::array_t<float>*>(col->getArray());
        __fastbit_iapi_address_map[buf->begin()] = i;
        break;}
    case FastBitDataTypeDouble: {
        ibis::array_t<double> *buf =
            static_cast<ibis::array_t<double>*>(col->getArray());
        __fastbit_iapi_address_map[buf->begin()] = i;
        break;}
    }
} // fastbit_iapi_reregister_array

void fastbit_iapi_rename_array(uint64_t i) {
    std::ostringstream oss;
    oss << 'A' << i;
    __fastbit_iapi_all_arrays[i]->name(oss.str().c_str());
    fastbit_iapi_reregister_array(i);
} // fastbit_iapi_rename_array

void fastbit_iapi_rename_arrays() {
    ibis::util::mutexLock
        lock(&__fastbit_iapi_lock, "fastbit_iapi_rename_arrays");
    const uint64_t ncols = __fastbit_iapi_all_arrays.size();
    __fastbit_iapi_address_map.clear();
    __fastbit_iapi_name_map.clear();
    uint64_t i = 0;
    uint64_t j = 0;
    do {
        while (i < ncols && __fastbit_iapi_all_arrays[i] != 0) {
            const char *current = __fastbit_iapi_all_arrays[i]->name();
            bool neednewname =
                (*current == 'A' || std::isdigit(current[1]) != 0);
            if (neednewname) {
                uint64_t itmp;
                ++ current;
                if (ibis::util::readUInt(itmp, current) == 0 &&
                    itmp != i)
                    neednewname = true;
            }
            if (neednewname) {
                fastbit_iapi_rename_array(i);
            }
            else {
                fastbit_iapi_reregister_array(i);
            }
            ++ i;
        }
        if (i < ncols) {
            for (j = i+1;
                 j < ncols && __fastbit_iapi_all_arrays[j] == 0;
                 ++ j);
            if (j < ncols) {
                __fastbit_iapi_all_arrays[i] = __fastbit_iapi_all_arrays[j];
                fastbit_iapi_rename_array(i);
                __fastbit_iapi_all_arrays[j] = 0;
                ++ i;
                ++ j;
            }
        }
        else {
            j = i;
        }
    } while (j < ncols);
    // settle the new size
    __fastbit_iapi_all_arrays.resize(i);
} // fastbit_iapi_rename_arrays

const ibis::array_t<uint64_t>& fastbit_iapi_get_mesh_shape
(FastBitSelectionHandle h) {
    const static ibis::array_t<uint64_t> empty;
    while (h->getType() != ibis::qExpr::RANGE &&
           h->getType() != ibis::qExpr::DRANGE)
        h = h->getLeft();
    ibis::qRange *qr = static_cast<ibis::qRange*>(h);
    ibis::bord::column *col = __fastbit_iapi_array_by_name(qr->colName());
    if (col != 0)
        return col->getMeshShape();
    else
        return empty;
} // fastbit_iapi_get_mesh_shape

void fastbit_iapi_gather_columns
(FastBitSelectionHandle h, std::vector<ibis::bord::column*> &all) {
    switch (h->getType()) {
    default: {
        if (h->getLeft())
            fastbit_iapi_gather_columns(h->getLeft(), all);
        if (h->getRight())
            fastbit_iapi_gather_columns(h->getRight(), all);
        break;}
    case ibis::qExpr::COMPRANGE: {
        if (h->getLeft())
            fastbit_iapi_gather_columns(h->getLeft(), all);
        if (h->getRight())
            fastbit_iapi_gather_columns(h->getRight(), all);
        ibis::compRange *cr = static_cast<ibis::compRange*>(h);
        if (cr->getTerm3() != 0)
            fastbit_iapi_gather_columns(cr->getTerm3(), all);
        break;}
    case ibis::qExpr::RANGE:
    case ibis::qExpr::DRANGE: {
        ibis::qRange *qr = static_cast<ibis::qRange*>(h);
        ibis::bord::column *tmp = __fastbit_iapi_array_by_name(qr->colName());
        if (tmp != 0) {
            ibis::util::mutexLock
                lck1(&__fastbit_iapi_lock, "fastbit_iapi_gather_columns");
            all.push_back(tmp);
        }
        break;}
    case ibis::qExpr::STRING: {
        ibis::qString *qr = static_cast<ibis::qString*>(h);
        ibis::bord::column *tmp =
            __fastbit_iapi_array_by_name(qr->leftString());
        if (tmp != 0) {
            ibis::util::mutexLock
                lck1(&__fastbit_iapi_lock, "fastbit_iapi_gather_columns");
            all.push_back(tmp);
        }
        break;}
    case ibis::qExpr::INTHOD: {
        ibis::qIntHod *qr = static_cast<ibis::qIntHod*>(h);
        ibis::bord::column *tmp = __fastbit_iapi_array_by_name(qr->colName());
        if (tmp != 0) {
            ibis::util::mutexLock
                lck1(&__fastbit_iapi_lock, "fastbit_iapi_gather_columns");
            all.push_back(tmp);
        }
        break;}
    case ibis::qExpr::UINTHOD: {
        ibis::qUIntHod *qr = static_cast<ibis::qUIntHod*>(h);
        ibis::bord::column *tmp = __fastbit_iapi_array_by_name(qr->colName());
        if (tmp != 0) {
            ibis::util::mutexLock
                lck1(&__fastbit_iapi_lock, "fastbit_iapi_gather_columns");
            all.push_back(tmp);
        }
        break;}
    }
} // fastbit_iapi_gather_columns

/// Gather all columns into an in-memory data table.
ibis::bord* fastbit_iapi_gather_columns(FastBitSelectionHandle h) {
    std::vector<ibis::bord::column*> cols;
    fastbit_iapi_gather_columns(h, cols);
    return new ibis::bord(cols);
} // fastbit_iapi_gather_columns

const ibis::bitvector* fastbit_iapi_lookup_solution(FastBitSelectionHandle h) {
    FastBitIAPISelectionList::const_iterator it =
        __fastbit_iapi_selection_list.find(h);
    if (it == __fastbit_iapi_selection_list.end())
        return 0;

    return it->second;
} // fastbit_iapi_lookup_solution

/// Copy the values from base to buf.  Only the values marked 1 in the mask
/// are copied.  Additionally, it skips over the first skip elements.
///
/// Return the number of elemented copied.
template <typename T> int64_t
fastbit_iapi_copy_values(const T *base, uint64_t nbase,
                         const ibis::bitvector &mask,
                         T *buf, uint64_t nbuf, uint64_t skip) {
    uint64_t j1 = 0;
    const ibis::bitvector::word_t *ii = 0;
    for (ibis::bitvector::indexSet is = mask.firstIndexSet();
         is.nIndices() > 0 && j1 < nbuf; ++ is) {
        ii = is.indices();
        if (skip > 0) {
            if (skip >= is.nIndices()) {
                skip -= is.nIndices();
                continue;
            }
            if (is.isRange()) {
                for (unsigned j0 = ii[0]+skip; j0 < ii[1] && j1 < nbuf;
                     ++ j0, ++j1) {
                    buf[j1] = base[j0];
                }
            }
            else {
                for (unsigned j0 = skip; j0 < is.nIndices() && j1 < nbuf;
                     ++ j0, ++j1) {
                    buf[j1] = base[ii[j0]];
                }
            }
            skip = 0;
        }
        else {
            if (is.isRange()) {
                for (unsigned j0 = ii[0]; j0 < ii[1] && j1 < nbuf;
                     ++ j0, ++j1) {
                    buf[j1] = base[j0];
                }
            }
            else {
                for (unsigned j0 = 0; j0 < is.nIndices() && j1 < nbuf;
                     ++ j0, ++j1) {
                    buf[j1] = base[ii[j0]];
                }
            }
        }
    }
    return j1;
} // fastbit_iapi_copy_values

/// Extract the position of the rows marked 1 in the mask.  Skipping the
/// first few 'skip' rows marked 1.
/// Return the number of positions copied to 'buf'.
int64_t fastbit_iapi_get_coordinates_1d
(const ibis::bitvector &mask, uint64_t *buf, uint64_t nbuf, uint64_t skip) {
    uint64_t j1 = 0;
    const ibis::bitvector::word_t *ii = 0;
    for (ibis::bitvector::indexSet is = mask.firstIndexSet();
         is.nIndices() > 0 && j1 < nbuf; ++ is) {
        ii = is.indices();
        if (skip > 0) {
            if (skip >= is.nIndices()) {
                skip -= is.nIndices();
                continue;
            }
            if (is.isRange()) {
                for (unsigned j0 = ii[0]+skip; j0 < ii[1] && j1 < nbuf;
                     ++ j0, ++ j1) {
                    buf[j1] = j0;
                }
            }
            else {
                for (unsigned j0 = skip; j0 < is.nIndices() && j1 < nbuf;
                     ++ j0, ++ j1) {
                    buf[j1] = ii[j0];
                }
            }
            skip = 0;
        }
        else {
            if (is.isRange()) {
                for (unsigned j0 = ii[0]; j0 < ii[1] && j1 < nbuf;
                     ++ j0, ++ j1) {
                    buf[j1] = j0;
                }
            }
            else {
                for (unsigned j0 = 0; j0 < is.nIndices() && j1 < nbuf;
                     ++ j0, ++ j1) {
                    buf[j1] = ii[j0];
                }
            }
        }
    }
    return j1;
} // fastbit_iapi_get_coordinates_1d

/// Convert the selected positions to 2-dimensional coordinates.  The
/// argument @c dim1 is the size of the faster varying dimension.  The
/// return value is the number of positions (each position taking up two
/// elements) in 'buf'.
///
/// Note that the argument @c skip refers to the number of positions marked
/// 1 to be skipped and the argument @c nbuf refers to the number of
/// elements in the argument @c buf.
int64_t fastbit_iapi_get_coordinates_2d
(const ibis::bitvector &mask, uint64_t *buf, uint64_t nbuf, uint64_t skip,
 uint64_t dim1) {
    if (dim1 == 0 || nbuf < 2) return -1;

    uint64_t j1 = 0;
    const ibis::bitvector::word_t *ii = 0;
    for (ibis::bitvector::indexSet is = mask.firstIndexSet();
         is.nIndices() > 0 && j1 < nbuf; ++ is) {
        ii = is.indices();
        if (skip > 0) {
            if (skip >= is.nIndices()) {
                skip -= is.nIndices();
                continue;
            }
            if (is.isRange()) {
                for (unsigned j0 = ii[0]+skip; j0 < ii[1] && j1+1 < nbuf;
                     ++ j0, j1 += 2) {
                    buf[j1]   = j0 / dim1;
                    buf[j1+1] = j0 % dim1;
                }
            }
            else {
                for (unsigned j0 = skip; j0 < is.nIndices() && j1+1 < nbuf;
                     ++ j0, j1 += 2) {
                    buf[j1]   = ii[j0] / dim1;
                    buf[j1+1] = ii[j0] % dim1;
                }
            }
            skip = 0;
        }
        else {
            if (is.isRange()) {
                for (unsigned j0 = ii[0]; j0 < ii[1] && j1+1 < nbuf;
                     ++ j0, j1 += 2) {
                    buf[j1]   = j0 / dim1;
                    buf[j1+1] = j0 % dim1;
                }
            }
            else {
                for (unsigned j0 = 0; j0 < is.nIndices() && j1+1 < nbuf;
                     ++ j0, j1 += 2) {
                    buf[j1]   = ii[j0] / dim1;
                    buf[j1+1] = ii[j0] % dim1;
                }
            }
        }
    }
    return (j1>>1);
} // fastbit_iapi_get_coordinates_2d

/// Convert the selected positions to 3-dimension coordinates.  The
/// argument @c dim2 is the size of the fastest varying dimension and @c
/// dim2 is the size of the second fastest varying dimension.
///
/// Note that the argument @c skip refers to the number of positions marked
/// 1 to be skipped and the argument @c nbuf refers to the number of
/// elements in the argument @c buf.
///
/// On successful completion of this function, the return value is the
/// number of points (each taking up three elements) in 'buf'.
int64_t fastbit_iapi_get_coordinates_3d
(const ibis::bitvector &mask, uint64_t *buf, uint64_t nbuf, uint64_t skip,
 uint64_t dim1, uint64_t dim2) {
    if (dim1 == 0 || dim2 == 0 || nbuf < 3) return -1;

    uint64_t j1 = 0;
    const uint64_t dim12 = dim1 * dim2;
    const ibis::bitvector::word_t *ii = 0;
    for (ibis::bitvector::indexSet is = mask.firstIndexSet();
         is.nIndices() > 0 && j1 < nbuf; ++ is) {
        ii = is.indices();
        if (skip > 0) {
            if (skip >= is.nIndices()) {
                skip -= is.nIndices();
                continue;
            }
            if (is.isRange()) {
                for (unsigned j0 = ii[0]+skip; j0 < ii[1] && j1+2 < nbuf;
                     ++ j0, j1 += 3) {
                    buf[j1]   = j0 / dim12;
                    buf[j1+1] = (j0 % dim12) / dim2;
                    buf[j1+2] = j0 % dim2;
                }
            }
            else {
                for (unsigned j0 = skip; j0 < is.nIndices() && j1+2 < nbuf;
                     ++ j0, j1 += 3) {
                    buf[j1]   = ii[j0] / dim12;
                    buf[j1+1] = (ii[j0] % dim12) / dim2;
                    buf[j1+2] = ii[j0] % dim2;
                }
            }
            skip = 0;
        }
        else {
            if (is.isRange()) {
                for (unsigned j0 = ii[0]; j0 < ii[1] && j1+2 < nbuf;
                     ++ j0, j1 += 2) {
                    buf[j1]   = j0 / dim12;
                    buf[j1+1] = (j0 % dim12) / dim2;
                    buf[j1+2] = j0 % dim2;
                }
            }
            else {
                for (unsigned j0 = 0; j0 < is.nIndices() && j1+2 < nbuf;
                     ++ j0, j1 += 3) {
                    buf[j1]   = ii[j0] / dim12;
                    buf[j1+1] = (ii[j0] % dim12) / dim2;
                    buf[j1+2] = ii[j0] % dim2;
                }
            }
        }
    }
    return (j1/3);
} // fastbit_iapi_get_coordinates_3d

/// Convert a global position to N-D coordinates.  It assume nd > 1 without
/// checking.
inline void fastbit_iapi_global_to_nd
(uint64_t nd, uint64_t *coords, uint64_t global, const uint64_t *cumu) {
    for (uint64_t jd = 0; jd+1 < nd; ++jd) {
        coords[jd] = global / cumu[jd+1];
        global -= (global / cumu[jd+1]) * cumu[jd+1];
    }
    coords[nd-1] = global;
} // fasbtit_iapi_global_to_nd

/// Convert the selected positions to N-dimension coordinates.  This
/// function can not be used for cases with 1 dimension.
///
/// Note that the argument @c skip refers to the number of positions marked
/// 1 to be skipped and the argument @c nbuf refers to the number of
/// elements in the argument @c buf.
///
/// On successful completion of this function, the return value is the
/// number of points (each taking up three elements) in 'buf'.
int64_t fastbit_iapi_get_coordinates_nd
(const ibis::bitvector &mask, uint64_t *buf, uint64_t nbuf, uint64_t skip,
 const uint64_t *dims, uint64_t nd) {
    if (dims == 0 || nd < 2 || nbuf < nd) return -1;

    uint64_t j1 = 0;
    uint64_t *cumu = new uint64_t[nd];
    cumu[nd-1] = dims[nd-1];
    for (unsigned j = nd-1; j > 0; -- j) {
        cumu[j-1] = cumu[j] * dims[j-1];
    }

    const ibis::bitvector::word_t *ii = 0;
    for (ibis::bitvector::indexSet is = mask.firstIndexSet();
         is.nIndices() > 0 && j1 < nbuf; ++ is) {
        ii = is.indices();
        if (skip > 0) {
            if (skip >= is.nIndices()) {
                skip -= is.nIndices();
                continue;
            }
            if (is.isRange()) {
                for (unsigned j0 = ii[0]+skip; j0 < ii[1] && j1+nd <= nbuf;
                     ++ j0, j1 += nd) {
                    fastbit_iapi_global_to_nd(nd, buf+j1, j0, cumu);
                }
            }
            else {
                for (unsigned j0 = skip; j0 < is.nIndices() && j1+nd <= nbuf;
                     ++ j0, j1 += nd) {
                    fastbit_iapi_global_to_nd(nd, buf+j1, ii[j0], cumu);
                }
            }
            skip = 0;
        }
        else {
            if (is.isRange()) {
                for (unsigned j0 = ii[0]; j0 < ii[1] && j1+nd <= nbuf;
                     ++ j0, j1 += nd) {
                    fastbit_iapi_global_to_nd(nd, buf+j1, j0, cumu);
                }
            }
            else {
                for (unsigned j0 = 0; j0 < is.nIndices() && j1+2 < nbuf;
                     ++ j0, j1 += nd) {
                    fastbit_iapi_global_to_nd(nd, buf+j1, ii[j0], cumu);
                }
            }
        }
    }
    return (j1/nd);
} // fastbit_iapi_get_coordinates_nd




// *** public functions start here ***

/// The incoming type must of an elementary data type, both buf and bound
/// must be valid pointers.  This function registers the incoming array as
/// ibis::bord::column object.
///
/// It returns a nil value in case of error.
extern "C" FastBitSelectionHandle fastbit_selection_create
(FastBitDataType dtype, void *buf, uint64_t nelm,
 FastBitCompareType ctype, void *bound) {
    if (dtype == FastBitDataTypeUnknown || buf == 0 || nelm == 0 || bound == 0)
        return 0;

    ibis::bord::column *col = __fastbit_iapi_array_by_addr(buf);
    if (col == 0) {
        std::ostringstream oss;
        oss << 'A' << std::hex << buf;
        col = __fastbit_iapi_register_array
            (oss.str().c_str(), dtype, buf, nelm);
        if (col == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- fastbit_selection_create failed to register buf "
                << buf;
            return 0;
        }
    }

    ibis::qExpr::COMPARE cmp = __fastbit_iapi_convert_compare_type(ctype);
    ibis::qExpr *ret = 0;
    bool negate = false;
    if (cmp == ibis::qExpr::OP_UNDEFINED) {
        cmp = ibis::qExpr::OP_EQ;
        negate = true;
    }

    double dval = __fastbit_iapi_convert_data_to_double(dtype, bound);
    if (dval == FASTBIT_DOUBLE_NULL)
        return 0;

    ret = new ibis::qContinuousRange(col->name(), cmp, dval);
    if (ret != 0 && negate) {
        ibis::qExpr *tmp = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
        tmp->setLeft(ret);
        ret = tmp;
    }
    if (ret != 0) {
        LOGGER(ibis::gVerbose > 3)
            << "fastbit_selection_create produced query expression \"" << *ret
            << '"';
    }
    else {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_selection_create failed to create a range "
            "condition on " << buf;
    }
    return ret;
} // fastbit_selection_create

/// The incoming type must of an elementary data type, both buf and bound
/// must be valid pointers.  This function registers the incoming array as
/// ibis::bord::column object.
///
/// It returns a nil value in case of error.
extern "C" FastBitSelectionHandle fastbit_selection_create_nd
(FastBitDataType dtype, void *buf, uint64_t *dims, uint64_t nd,
 FastBitCompareType ctype, void *bound) {
    if (dtype == FastBitDataTypeUnknown || buf == 0 || dims == 0 || nd == 0
        || bound == 0)
        return 0;

    ibis::bord::column *col = __fastbit_iapi_array_by_addr(buf);
    if (col == 0) {
        std::ostringstream oss;
        oss << 'A' << std::hex << buf;
        col = __fastbit_iapi_register_array_nd(oss.str().c_str(), dtype, buf,
                                               dims, nd);
        if (col == 0) {
            LOGGER(ibis::gVerbose > 1)
                << "Warning -- fastbit_selection_create_nd failed to register "
                "buf " << buf;
            return 0;
        }
    }

    ibis::qExpr::COMPARE cmp = __fastbit_iapi_convert_compare_type(ctype);
    ibis::qExpr *ret = 0;
    bool negate = false;
    if (cmp == ibis::qExpr::OP_UNDEFINED) {
        cmp = ibis::qExpr::OP_EQ;
        negate = true;
    }

    double dval = __fastbit_iapi_convert_data_to_double(dtype, bound);
    if (dval == FASTBIT_DOUBLE_NULL)
        return 0;

    ret = new ibis::qContinuousRange(col->name(), cmp, dval);
    if (negate) {
        ibis::qExpr *tmp = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
        tmp->setLeft(ret);
        ret = tmp;
    }
    if (ret != 0) {
        LOGGER(ibis::gVerbose > 3)
            << "fastbit_selection_create_nd produced query expression \"" << *ret
            << '"';
    }
    else {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_selection_create_nd failed to create a range "
            "condition on " << buf;
    }
    return ret;
} // fastbit_selection_create_nd

/// Free the objects representing the selection.  Only the top most level
/// of the object hierarchy, i.e., the last selection handle return by the
/// combine operations, needs to be freed.
extern "C" void fastbit_selection_free(FastBitSelectionHandle h) {
    {
        ibis::util::mutexLock lock(&__fastbit_iapi_lock,
                                   "fastbit_selection_free");
        FastBitIAPISelectionList::iterator it =
            __fastbit_iapi_selection_list.find(h);
        if (it != __fastbit_iapi_selection_list.end()) {
            delete it->second;
            __fastbit_iapi_selection_list.erase(it);
        }
    }
    delete h;
} // fastbit_selection_free


/// Combine two sets of selection conditions into one.
///
/// @note The new object take ownership of the two incoming expressions.
/// This arrangement allows the user to delete the last object produced to
/// free all objects going into building the last combined object.
extern "C" FastBitSelectionHandle fastbit_selection_combine
(FastBitSelectionHandle h1, FastBitCombineType cmb, FastBitSelectionHandle h2) {
    ibis::qExpr *ret = 0;
    if (h1 == 0 || h2 == 0) {
        LOGGER(ibis::gVerbose > 2)
            << "Warning -- fastbit_selection_combine can not proceed with "
            "a nil FastBit selection handle";
        return ret;
    }

    switch (cmb) {
    default:
        break;
    case FastBitCombineAnd: {
        ret = new ibis::qExpr(ibis::qExpr::LOGICAL_AND);
        ret->setLeft(h1);
        ret->setRight(h2);
        break;}
    case FastBitCombineOr: {
        ret = new ibis::qExpr(ibis::qExpr::LOGICAL_OR);
        ret->setLeft(h1);
        ret->setRight(h2);
        break;}
    case FastBitCombineXor: {
        ret = new ibis::qExpr(ibis::qExpr::LOGICAL_XOR);
        ret->setLeft(h1);
        ret->setRight(h2);
        break;}
    case FastBitCombineNand: {
        ibis::qExpr *tmp = new ibis::qExpr(ibis::qExpr::LOGICAL_AND);
        tmp->setLeft(h1);
        tmp->setRight(h2);
        ret = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
        ret->setLeft(tmp);
        break;}
    case FastBitCombineNor: {
        ibis::qExpr *tmp = new ibis::qExpr(ibis::qExpr::LOGICAL_OR);
        tmp->setLeft(h1);
        tmp->setRight(h2);
        ret = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
        ret->setLeft(tmp);
        break;}
    }
    if (ret != 0) {
        LOGGER(ibis::gVerbose > 3)
            << "fastbit_selection_combine successfully combined " << h1
            << " and " << h2 << " into " << *ret;
    }
    else {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_selection_combine failed to combine " << h1
            << " with " << h2;
    }
    return ret;
} // fastbit_selection_combine

/// This function is only meant to provide a rough eastimate of the upper
/// bound of the number of hits.  There is no guarantee on how accurate is
/// the estimation.  This estimation may be sufficient for the purpose of
/// allocating workspace required for reading the selection.
extern "C" int64_t fastbit_selection_estimate(FastBitSelectionHandle h) {
    if (h == 0) {
        return -1;
    }
    const ibis::bitvector *res = fastbit_iapi_lookup_solution(h);
    if (res != 0)
        return res->cnt();

    std::unique_ptr<ibis::bord> brd(fastbit_iapi_gather_columns(h));
    if (brd.get() == 0)
        return -2;

    ibis::countQuery que(brd.get());
    int ierr = que.setWhereClause(h);
    if (ierr < 0)
        return -3;

    ierr = que.estimate();
    if (ierr < 0)
        return -4;
    LOGGER(ibis::gVerbose > 2)
        << "fastbit_selection_estimate: " << que.getWhereClause() << " --> ["
        << que.getMinNumHits() << ", " << que.getMinNumHits() << ']';
    if (que.getMinNumHits() == que.getMaxNumHits()) {
        ibis::util::mutexLock lock(&__fastbit_iapi_lock,
                                   "fastbit_selection_estimate");
        __fastbit_iapi_selection_list[h] =
            new ibis::bitvector(*que.getHitVector());
    }
    return que.getMaxNumHits();
} // fastbit_selection_estimate

/// Compute the numebr of hits.  This function performs the exact
/// evaluation and store the results in a global data structure.
///
/// @note The precise evaluation needs to be performed before reading the
/// data values.  If it is not performed, the read selection function will
/// perform the precise evaluation.
extern "C" int64_t fastbit_selection_evaluate(FastBitSelectionHandle h) {
    if (h == 0) {
        LOGGER(ibis::gVerbose > 2)
            << "Warning -- fastbit_selection_evaluate can not proceed with "
            "a nil FastBit selection handle";
        return -1;
    }

    const ibis::bitvector *res = fastbit_iapi_lookup_solution(h);
    if (res != 0) {
        LOGGER(ibis::gVerbose > 6)
            << "Warning -- fastbit_selection_evaluate returns cached result "
            "for query \"" << *static_cast<const ibis::qExpr*>(h) << '"';
        return res->cnt();
    }

    std::unique_ptr<ibis::bord> brd(fastbit_iapi_gather_columns(h));
    if (brd.get() == 0)
        return -2;
    ibis::countQuery que(brd.get());
    int ierr = que.setWhereClause(h);
    if (ierr < 0)
        return -3;

    ierr = que.evaluate();
    if (ierr < 0)
        return -4;

    LOGGER(ibis::gVerbose > 2)
        << "fastbit_selection_evaluate: " << que.getWhereClause()
        << " ==> " << que.getNumHits();
    ibis::util::mutexLock lock(&__fastbit_iapi_lock,
                               "fastbit_selection_evaluate");
    __fastbit_iapi_selection_list[h] = new ibis::bitvector(*que.getHitVector());
    return que.getNumHits();
} // fastbit_selection_evaluate

/// Fill the buffer (buf) with the next set of values satisfying the
/// selection criteria.
///
/// Both nbase and nbuf are measured in number of elements of the specified
/// type, NOT in bytes.
///
/// The start position is measuremeted as positions in the list of selected
/// values, not positions in the base data.
///
/// The return value is the number of elements successfully read.  In case
/// of error, a negative value is returned.
extern "C" int64_t fastbit_selection_read
(FastBitDataType dtype, const void *base, uint64_t nbase,
 FastBitSelectionHandle h, void *buf, uint64_t nbuf, uint64_t start) {
    if (dtype == FastBitDataTypeUnknown || base == 0 || nbase == 0 ||
        h == 0 || buf == 0 || nbuf == 0) {
        LOGGER(ibis::gVerbose > 2)
            << "Warning -- fastbit_selection_read can not proceed with "
            "a nil FastBit selection handle or nil buffer";
        return -1;
    }
    if (start >= nbase)
        return 0;

    int64_t ierr = fastbit_selection_evaluate(h);
    if (ierr <= 0) return ierr;

    const ibis::bitvector &mask = *__fastbit_iapi_selection_list[h];
    switch(dtype) {
    default:
        return -5;
    case FastBitDataTypeByte:
        ierr = fastbit_iapi_copy_values<signed char>
            (static_cast<const signed char*>(base), nbase, mask,
             static_cast<signed char*>(buf), nbuf, start);
        break;
    case FastBitDataTypeUByte:
        ierr = fastbit_iapi_copy_values<unsigned char>
            (static_cast<const unsigned char*>(base), nbase, mask,
             static_cast<unsigned char*>(buf), nbuf, start);
        break;
    case FastBitDataTypeShort:
        ierr = fastbit_iapi_copy_values<int16_t>
            (static_cast<const int16_t*>(base), nbase, mask,
             static_cast<int16_t*>(buf), nbuf, start);
        break;
    case FastBitDataTypeUShort:
        ierr = fastbit_iapi_copy_values<uint16_t>
            (static_cast<const uint16_t*>(base), nbase, mask,
             static_cast<uint16_t*>(buf), nbuf, start);
        break;
    case FastBitDataTypeInt:
        ierr = fastbit_iapi_copy_values<int32_t>
            (static_cast<const int32_t*>(base), nbase, mask,
             static_cast<int32_t*>(buf), nbuf, start);
        break;
    case FastBitDataTypeUInt:
        ierr = fastbit_iapi_copy_values<uint32_t>
            (static_cast<const uint32_t*>(base), nbase, mask,
             static_cast<uint32_t*>(buf), nbuf, start);
        break;
    case FastBitDataTypeLong:
        ierr = fastbit_iapi_copy_values<int64_t>
            (static_cast<const int64_t*>(base), nbase, mask,
             static_cast<int64_t*>(buf), nbuf, start);
        break;
    case FastBitDataTypeULong:
        ierr = fastbit_iapi_copy_values<uint64_t>
            (static_cast<const uint64_t*>(base), nbase, mask,
             static_cast<uint64_t*>(buf), nbuf, start);
        break;
    case FastBitDataTypeFloat:
        ierr = fastbit_iapi_copy_values<float>
            (static_cast<const float*>(base), nbase, mask,
             static_cast<float*>(buf), nbuf, start);
        break;
    case FastBitDataTypeDouble:
        ierr = fastbit_iapi_copy_values<double>
            (static_cast<const double*>(base), nbase, mask,
             static_cast<double*>(buf), nbuf, start);
        break;
    }
    return ierr;
} // fastbit_selection_read

/// @arg h    the query handle.
/// @arg buf  buffer to carry the output coordinates.
/// @arg nbuf number of elements in the given buffer.
/// @arg skip number of selected points to be skip before the coordinates
///           are placed in @c buf.  This is necessary if the incoming
///           buffer is too small to hold all the points and the caller has
///           to invoke this function repeatedly.
///
/// The shape of the array is determined by shape of the array in the first
/// (left-most) selection condition tree.  The implicit assumption is that
/// all arrays/variables involved in the selection conditions have the same
/// shape.
extern "C" int64_t fastbit_selection_get_coordinates
(FastBitSelectionHandle h, uint64_t *buf, uint64_t nbuf, uint64_t skip) {
    if (h == 0 || buf == 0 || nbuf == 0) {
        LOGGER(ibis::gVerbose > 2)
            << "Warning -- fastbit_selection_get_coordinates can not proceed "
            "with a nil FastBit selection handle or nil buffer";
        return -1;
    }

    int64_t ierr = fastbit_selection_evaluate(h);
    if (ierr <= 0) return ierr;
    if (skip >= (uint64_t)ierr)
        return 0;

    const ibis::bitvector &mask = *__fastbit_iapi_selection_list[h];
    const ibis::array_t<uint64_t> &dims = fastbit_iapi_get_mesh_shape(h);
    if (dims.size() > nbuf) {
        LOGGER(ibis::gVerbose > 2)
            << "Warning -- fastbit_selection_get_coordinates can not write one "
            "set of coordinates into the given buffer, dims.size() = "
            << dims.size() << ", nbuf = " << nbuf;
        return -1;
    }

    switch (dims.size()) {
    case 0:
    case 1:
        ierr = fastbit_iapi_get_coordinates_1d
            (mask, buf, nbuf, skip);
        break;
    case 2:
        ierr = fastbit_iapi_get_coordinates_2d
            (mask, buf, nbuf, skip, dims[1]);
        break;
    case 3:
        ierr = fastbit_iapi_get_coordinates_3d
            (mask, buf, nbuf, skip, dims[1], dims[2]);
        break;
    default:
        ierr = fastbit_iapi_get_coordinates_nd
            (mask, buf, nbuf, skip, dims.begin(), dims.size());
        break;
    }
    return ierr;
} // fastbit_selection_get_coordinates

extern "C" void fastbit_selection_purge_results(FastBitSelectionHandle h) {
    if (h == 0) return;

    ibis::util::mutexLock lock(&__fastbit_iapi_lock,
                               "fastbit_selection_purge_results");
    FastBitIAPISelectionList::iterator it =
        __fastbit_iapi_selection_list.find(h);
    if (it == __fastbit_iapi_selection_list.end()) return;

    delete it->second;
    __fastbit_iapi_selection_list.erase(it);
} // fastbit_selection_purge_results

extern "C" void fastbit_iapi_free_all() {
    __fastbit_free_all_selected();
    __fastbit_free_all_arrays();
} // fastbit_iapi_free_all

extern "C" void fastbit_iapi_free_array(const char *nm) {
    FastBitIAPINameMap::const_iterator it =
        __fastbit_iapi_name_map.find(nm);
    if (it == __fastbit_iapi_name_map.end()) return;

    LOGGER(ibis::gVerbose > 3)
        << "FastBit IAPI freeing array \"" << nm << '"';
    ibis::util::mutexLock lock(&__fastbit_iapi_lock, "fastbit_free_array");
    it = __fastbit_iapi_name_map.find(nm);
    if (it->second < __fastbit_iapi_all_arrays.size()) {
        __fastbit_iapi_free_array(it->second);
    }
    else {
        __fastbit_iapi_name_map.erase(it);
    }
} // fastbit_iapi_free_array

extern "C" void fastbit_iapi_free_array_by_addr(void *addr) {
    FastBitIAPIAddressMap::const_iterator it =
        __fastbit_iapi_address_map.find(addr);
    if (it == __fastbit_iapi_address_map.end()) return;

    LOGGER(ibis::gVerbose > 3)
        << "FastBit IAPI freeing array at " << addr;
    ibis::util::mutexLock lock(&__fastbit_iapi_lock, "fastbit_free_array");
    it = __fastbit_iapi_address_map.find(addr);
    if (it->second < __fastbit_iapi_all_arrays.size()) {
        __fastbit_iapi_free_array(it->second);
    }
    else {
        __fastbit_iapi_address_map.erase(it);
    }
} // fastbit_iapi_free_array_by_addr

/**
   @arg nm name of the array.  The array name @c nm must follow the naming
   convention specified in the documentation for ibis::column.  More
   specifically, the name must start with a underscore (_) or one of the 26
   English alphabets, and the remaining characters in the name must be
   drawn from _, a-z, A-Z, 0-9, '.', and ':'.  Additionally, the column
   names are used without considering the cases of the letters a-z.

   @arg dtype data type.

   @arg buf the data buffer.  For most data types, this is a raw pointer to
   data from user.  For example, if the type is FastBitDataTypeDouble, buf
   is of type 'double *'.  The exception is when the type is either
   FastBitDataTypeBitRaw or FastBitDataTypeBitCompressed.  When the type is
   FastBitDataTypeBitRaw, the buffer is expected to be 'unsigned char*',
   and each bit in the buffer is treated as the literal bits.  When the
   type is FastBitDataTypeBitCompressed, the buffer is expected to
   'ibis::bitvector*'.

   @arg nelm number of elements of the specified type in the data buffer.
   When the data type is FastBitDataTypeBitRaw or
   FastBitDataTypeBitCompressed, nelm refers to the number of bits
   represented by the content of data buffer.

   @return This function returns 0 to indicate success, a positive number
   to indicate that the content has already been registered, a negative
   number to indicate error such as unknown data type, null string for name
   or memory allocation error.
 */
extern "C" int fastbit_iapi_register_array
(const char *nm, FastBitDataType dtype, void *buf, uint64_t nelm) {
    if (nm == 0 || *nm == 0 || dtype == FastBitDataTypeUnknown || buf == 0)
        return -1;

    if (__fastbit_iapi_array_by_addr(buf) != 0) {
        LOGGER(ibis::gVerbose > 2)
            << "fastbit_iapi_register_array determined that buf " << buf
            << " has already been registered";
        return 1;
    }
    if (__fastbit_iapi_array_by_name(nm) != 0) {
        LOGGER(ibis::gVerbose > 2)
            << "fastbit_iapi_register_array determined that name " << nm
            << " has already been registered";
        return 2;
    }
    if (__fastbit_iapi_register_array(nm, dtype, buf, nelm) != 0)
        return 0;
    else
        return -2;
} // fastbit_iapi_register_array

/**
   @arg nm: name of the array to be extended.
   @arg dtype: type of the array.
   @arg addr: address of the new content to be added to the named array.
   @arg nelm: number of elements in the new content.

   The new content is copied to the existing array resulting a large
   array.  The newly extended array contains a copy of the content in the
   buffer at @c addr.

   This function returns an integer error code.  It returns 0 for success,
   or a negative number to indicate error of some sort.
 */
extern "C" int fastbit_iapi_extend_array
(const char *nm, FastBitDataType dtype, void *addr, uint64_t nelm) {
    if (nm == 0 || *nm == 0 || dtype == FastBitDataTypeUnknown || addr == 0)
        return -1;
    ibis::bord::column *col = __fastbit_iapi_array_by_name(nm);
    if (col == 0) { // new array
        if (__fastbit_iapi_register_array(nm, dtype, addr, nelm) != 0)
            return 0; // registered the new array successfully
        else
            return -2;
    }

    ibis::bitvector msk;
    msk.set(1, nelm);
    int ierr = 0;
    switch (dtype) {
    default:
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_iapi_extend_array can not support array "
            << nm << ", only some fixed-sized data types are supported";
        return -3; 
    case FastBitDataTypeByte: {
        ibis::array_t<signed char> *buf =
            new ibis::array_t<signed char>((signed char*)addr, nelm);
        ierr = col->append(buf, msk);
        break;}
    case FastBitDataTypeUByte: {
        ibis::array_t<unsigned char> *buf
            = new ibis::array_t<unsigned char>((unsigned char*)addr, nelm);
        ierr = col->append(buf, msk);
        break;}
    case FastBitDataTypeShort: {
        ibis::array_t<int16_t> *buf =
            new ibis::array_t<int16_t>((int16_t*)addr, nelm);
        ierr = col->append(buf, msk);
        break;}
    case FastBitDataTypeUShort: {
        ibis::array_t<uint16_t> *buf =
            new ibis::array_t<uint16_t>((uint16_t*)addr, nelm);
        ierr = col->append(buf, msk);
        break;}
    case FastBitDataTypeInt: {
        ibis::array_t<int32_t> *buf =
            new ibis::array_t<int32_t>((int32_t*)addr, nelm);
        ierr = col->append(buf, msk);
        break;}
    case FastBitDataTypeUInt: {
        ibis::array_t<uint32_t> *buf =
            new ibis::array_t<uint32_t>((uint32_t*)addr, nelm);
        ierr = col->append(buf, msk);
        break;}
    case FastBitDataTypeLong: {
        ibis::array_t<int64_t> *buf =
            new ibis::array_t<int64_t>((int64_t*)addr, nelm);
        ierr = col->append(buf, msk);
        break;}
    case FastBitDataTypeULong: {
        ibis::array_t<uint64_t> *buf =
            new ibis::array_t<uint64_t>((uint64_t*)addr, nelm);
        ierr = col->append(buf, msk);
        break;}
    case FastBitDataTypeFloat: {
        ibis::array_t<float> *buf =
            new ibis::array_t<float>((float*)addr, nelm);
        ierr = col->append(buf, msk);
        break;}
    case FastBitDataTypeDouble: {
        ibis::array_t<double> *buf =
            new ibis::array_t<double>((double*)addr, nelm);
        ierr = col->append(buf, msk);
        break;}
    case FastBitDataTypeBitRaw: {
        if (col->type() != ibis::BIT) return -3;

        const unsigned char *uptr = static_cast<const unsigned char*>(addr);
        ibis::bitvector bv;
        while (bv.size()+8 <= nelm) {
            bv.appendByte(*uptr);
            ++ uptr;
        }
        if (bv.size() < nelm) {
            bv += ((*uptr & 0x80) >> 7);
        }
        if (bv.size() < nelm) {
            bv += ((*uptr & 0x40) >> 6);
        }
        if (bv.size() < nelm) {
            bv += ((*uptr & 0x20) >> 5);
        }
        if (bv.size() < nelm) {
            bv += ((*uptr & 0x10) >> 4);
        }
        if (bv.size() < nelm) {
            bv += ((*uptr & 0x08) >> 3);
        }
        if (bv.size() < nelm) {
            bv += ((*uptr & 0x04) >> 2);
        }
        if (bv.size() < nelm) {
            bv += ((*uptr & 0x02) >> 1);
        }
        ierr = col->append(&bv, msk);
        break;}
    case FastBitDataTypeBitCompressed: {
        if (col->type() != ibis::BIT) return -4;

        ierr = col->append(addr, msk);
        break;}
    }
    return ierr;
} // fastbit_iapi_extend_array

/**
   @note the array name @c nm must follow the naming convention specified
   in the documentation for ibis::column.  More specifically, the name must
   start with a underscore (_) or one of the 26 English alphabets, and the
   remaining characters in the name must be drawn from _, a-z, A-Z, 0-9,
   '.', and ':'.  Additionally, the column names are used without
   considering the cases of the letters a-z.
 */
extern "C" int fastbit_iapi_register_array_nd
(const char *nm, FastBitDataType dtype, void *buf, uint64_t *dims, uint64_t nd) {
    if (nm == 0 || *nm == 0 || dtype == FastBitDataTypeUnknown || buf == 0 ||
        dims == 0 || nd == 0)
        return -1;

    if (__fastbit_iapi_array_by_addr(buf) != 0) {
        LOGGER(ibis::gVerbose > 2)
            << "fastbit_iapi_register_array determined that buf " << buf
            << " has already been registered";
        return 1;
    }
    if (__fastbit_iapi_array_by_name(nm) != 0) {
        LOGGER(ibis::gVerbose > 2)
            << "fastbit_iapi_register_array determined that name " << nm
            << " has already been registered";
        return 2;
    }
    if (__fastbit_iapi_register_array_nd(nm, dtype, buf, dims, nd) != 0)
        return 0;
    else
        return -2;
} // fastbit_iapi_register_array_nd

/**
   The content of the array is available through FastBitReadExtArray.

   @note the array name @c nm must follow the naming convention specified
   in the documentation for ibis::column.  More specifically, the name must
   start with a underscore (_) or one of the 26 English alphabets, and the
   remaining characters in the name must be drawn from _, a-z, A-Z, 0-9,
   '.', and ':'.  Additionally, the column names are used without
   considering the cases of the letters a-z.
 */
extern "C" int fastbit_iapi_register_array_ext
(const char *nm, FastBitDataType dtype, uint64_t *dims, uint64_t nd,
 void *ctx, FastBitReadExtArray rd) {
    if (nm == 0 || *nm == 0 || dtype == FastBitDataTypeUnknown ||
        dims == 0 || nd == 0 || rd == 0)
        return -1;

    if (__fastbit_iapi_array_by_name(nm) != 0) {
        LOGGER(ibis::gVerbose > 2)
            << "fastbit_iapi_register_array determined that name " << nm
            << " has already been registered";
        return 2;
    }
    if (__fastbit_iapi_register_array_ext(nm, dtype, dims, nd, ctx, rd) != 0)
        return 0;
    else
        return -2;
} // fastbit_iapi_register_array_ext

/**
   Only the index for the array is actually available.

   @note the array name @c nm must follow the naming convention specified
   in the documentation for ibis::column.  More specifically, the name must
   start with a underscore (_) or one of the 26 English alphabets, and the
   remaining characters in the name must be drawn from _, a-z, A-Z, 0-9,
   '.', and ':'.  Additionally, the column names are used without
   considering the cases of the letters a-z.
 */
extern "C" int fastbit_iapi_register_array_index_only
(const char *nm, FastBitDataType dtype, uint64_t *dims, uint64_t nd,
 double *keys, uint64_t nkeys, int64_t *offsets, uint64_t noffsets,
 void* bms, FastBitReadBitmaps rd) {
    if (nm == 0 || *nm == 0 || dtype == FastBitDataTypeUnknown ||
        dims == 0 || nd == 0 || keys == 0 || nkeys == 0 ||
        offsets == 0 || noffsets == 0 || rd == 0)
        return -1;

    if (__fastbit_iapi_array_by_name(nm) != 0) {
        LOGGER(ibis::gVerbose > 2)
            << "fastbit_iapi_register_array determined that name " << nm
            << " has already been registered";
        return 2;
    }
    if (__fastbit_iapi_register_array_index_only
        (nm, dtype, dims, nd, keys, nkeys, offsets, noffsets, bms, rd) != 0)
        return 0;
    else
        return -2;
} // fastbit_iapi_register_array_index_only

/// @arg aname: column name
/// @arg iopt: indexing option
///
/// Returns 0 for success, a negative number for any error or failure.
///
/// @note If one of nkeys, noffsets or nbitmaps is nil, then none of them
/// will be assigned a value.  This is taken as the user does not want to
/// write the index out.
extern "C" int fastbit_iapi_build_index
(const char *aname, const char *iopt) {
    if (aname == 0 || *aname == 0)
        return -1;

    ibis::bord::column *col = __fastbit_iapi_array_by_name(aname);
    if (col == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_iapi_build_index failed to find an array "
            "named " << aname;
        return -2;
    }
    col->loadIndex(iopt);
    if (! col->hasIndex()) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_iapi_build_index failed to create an index "
            "for array " << aname;
        return -3;
    }
    return 0;
} // fastbit_iapi_build_index

extern "C" int fastbit_iapi_deconstruct_index
(const char *aname, double **keys, uint64_t *nkeys,
 int64_t **offsets, uint64_t *noffsets,
 uint32_t **bms, uint64_t *nbms) {
    if (aname == 0 || *aname == 0)
        return -1;

    ibis::bord::column *col = __fastbit_iapi_array_by_name(aname);
    if (col == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_iapi_build_index failed to find an array "
           "named " << aname;
        return -2;
    }

    int ierr;
    ibis::array_t<double> arrk;
    ibis::array_t<int64_t> arro;
    ibis::array_t<uint32_t> arrb;
    ierr = col->indexWrite(arrk, arro, arrb);
    if (ierr >= 0) {
        *nkeys    = arrk.size();
        *keys     = arrk.release();
        *noffsets = arro.size();
        *offsets  = arro.release();
        *nbms     = arrb.size();
        *bms      = arrb.release();
        LOGGER(ibis::gVerbose > 5)
            << "fastbit_iapi_deconstruct_index returns nkeys = " << *nkeys
            << ", noffets = " << *noffsets << ", and nbms = " << *nbms;
    }
    else {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_iapi_deconstruct_index failed, "
            "indexWrite returned " << ierr;
    }
    return ierr;
} // fastbit_iapi_deconstruct_index

extern "C"
FastBitIndexHandle fastbit_iapi_reconstruct_index
(double *keys, uint64_t nkeys, int64_t *offsets, uint64_t noffsets) {
    if (nkeys > noffsets && nkeys == 2*(noffsets-1)) {
        return new ibis::bin(0, static_cast<uint32_t>(noffsets-1),
                             keys, offsets);
    }
    else if (nkeys+1 == noffsets) {
        return new ibis::relic(0, static_cast<uint32_t>(nkeys),
                               keys, offsets);
    }
    else {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_iapi_reconstruct_index encountered "
            "mismatching nkeys (" << nkeys << ") and noffsets (" << noffsets
            << ')';
        return 0;
    }
} // fastbit_iapi_reconstruct_index

/// @arg ih: the index handle.
/// @arg ct: comparision operator.
/// @arg cv: query boundary, the value to be compared.
/// @arg cand0: left-most bin that might have some hits.
/// @arg hit0: left-most bin that are definitely all hits.
/// @arg hit1: right-most bin that are definitely all hits.
/// @arg cand1: right-most bin that are possible hits.
extern "C" int fastbit_iapi_resolve_range
(FastBitIndexHandle ih, FastBitCompareType ct, double cv, uint32_t * cand0,
 uint32_t *hit0, uint32_t *hit1, uint32_t *cand1) {
    if (ih == 0 || cv == 0) return -1;

    ibis::index *ih2 = static_cast<ibis::index*>(ih);
    uint32_t _1, _2, _3, _4;
    ibis::qContinuousRange cr("_", __fastbit_iapi_convert_compare_type(ct), cv);
    switch (ih2->type()) {
    case ibis::index::BINNING:
        static_cast<const ibis::bin*>(ih2)->locate(cr, _1, _4, _2, _3);
        break;
    case ibis::index::RELIC:
        static_cast<const ibis::relic*>(ih2)->locate(cr, _2, _3);
        _1 = _2;
        _4 = _3;
        break;
    default:
        _1 = 0;
        _2 = 0;
        _3 = 0;
        _4 = 0;
    }
    if (cand0 != 0) *cand0 = _1;
    if (hit0 != 0)  *hit0  = _2;
    if (hit1 != 0)  *hit1  = _3;
    if (cand1 != 0) *cand1 = _4;
    return 0;
} // fastbit_iapi_resolve_range

extern "C" int64_t fastbit_iapi_get_number_of_hits
(FastBitIndexHandle ih, uint32_t ib, uint32_t ie, uint32_t *buf) {
    if (ih == 0 || buf == 0) return -1;
    ibis::bitvector res;
    static_cast<ibis::index*>(ih)->sumBins(ib, ie, res, buf);
    return res.cnt();
} // fastbit_iapi_get_number_of_hits

extern "C" int fastbit_iapi_attach_full_index
(const char *aname, double *keys, uint64_t nkeys,
 int64_t *offsets, uint64_t noffsets,
 uint32_t *bms, uint64_t nbms) {
    if (aname == 0 || *aname == 0 || keys == 0 || nkeys == 0 ||
        offsets == 0 || noffsets == 0 || bms == 0 || nbms == 0 ||
        offsets[noffsets-1] > nbms)
        return -1;

    ibis::bord::column *col = __fastbit_iapi_array_by_name(aname);
    if (col == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_iapi_attach_full_index failed to find an "
            "array named " << aname;
        return -2;
    }

    return col->attachIndex(keys, nkeys, offsets, noffsets, bms, nbms);
} // fastbit_iapi_attach_full_index

extern "C" int fastbit_iapi_attach_index
(const char *aname, double *keys, uint64_t nkeys,
 int64_t *offsets, uint64_t noffsets,
 void *bms, FastBitReadBitmaps rd) {
    if (aname == 0 || *aname == 0 || keys == 0 || nkeys == 0 ||
        offsets == 0 || noffsets == 0 || bms == 0 || rd == 0)
        return -1;

    ibis::bord::column *col = __fastbit_iapi_array_by_name(aname);
    if (col == 0) {
        LOGGER(ibis::gVerbose > 0)
            << "Warning -- fastbit_iapi_attach_index failed to find an "
            "array named " << aname;
        return -2;
    }

    return col->attachIndex(keys, nkeys, offsets, noffsets, bms, rd);
} // fastbit_iapi_attach_index

/// Generate a simple one-sided range (OSR) condition for the form "aname
/// compare bound".
///
/// It returns a nil value in case of error.
extern "C" FastBitSelectionHandle fastbit_selection_osr
(const char *aname, FastBitCompareType ctype, double bound) {
    if (aname == 0 || *aname == 0 || bound == FASTBIT_DOUBLE_NULL)
        return 0;

    ibis::bord::column *col = __fastbit_iapi_array_by_name(aname);
    if (col == 0) {
        LOGGER(ibis::gVerbose > 1)
            << "Warning -- fastbit_selection_osr failed to find an array named "
            << aname;
            return 0;
    }

    ibis::qExpr::COMPARE cmp = __fastbit_iapi_convert_compare_type(ctype);
    ibis::qExpr *ret = 0;
    bool negate = false;
    if (cmp == ibis::qExpr::OP_UNDEFINED) {
        cmp = ibis::qExpr::OP_EQ;
        negate = true;
    }

    ret = new ibis::qContinuousRange(aname, cmp, bound);
    if (negate) {
        ibis::qExpr *tmp = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
        tmp->setLeft(ret);
        ret = tmp;
    }
    return ret;
} // fastbit_selection_osr

/**
   @warning the selection/query must have been evaluated already, otherwise
   ther is no bitvector to be used for this function.
 */
extern "C" int fastbit_iapi_register_selection_as_bit_array
(const char *nm, FastBitSelectionHandle h) {
    ibis::bitvector *bv =
        const_cast<ibis::bitvector*>(fastbit_iapi_lookup_solution(h));
    if (bv == 0) return -1;
    return fastbit_iapi_register_array
        (nm, FastBitDataTypeBitCompressed, bv, bv->size());
} // fastbit_iapi_register_selection_as_bit_array

/** 
   @warning the selection/query must have been evaluated already, otherwise
   ther is no bitvector to be used for this function.
 */
extern "C" int fastbit_iapi_extend_bit_array_with_selection
(const char *nm, FastBitSelectionHandle h) {
    ibis::bitvector *bv =
        const_cast<ibis::bitvector*>(fastbit_iapi_lookup_solution(h));
    if (bv == 0) return -1;
    return fastbit_iapi_extend_array
        (nm, FastBitDataTypeBitCompressed, bv, bv->size());
} // fastbit_iapi_extend_bit_array_with_selection

