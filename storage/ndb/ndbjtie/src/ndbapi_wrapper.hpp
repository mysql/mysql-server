/*
 Copyright (C) 2009 Sun Microsystems, Inc.
 All rights reserved. Use is subject to license terms.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/
/*
 * ndbapi_wrapper.hpp
 */

#ifndef ndbapi_wrapper_hpp
#define ndbapi_wrapper_hpp

// API to implement against
#include "NdbApi.hpp"

struct NdbApiWrapper {
    static NdbBlob * getBlobHandle() {
        return 0;
    }
    
    static NdbBlob * getBlobHandle(NdbOperation & obj, const char * anAttrName) {
        return obj.getBlobHandle(anAttrName);
    }
    
    static NdbBlob * getBlobHandle(NdbOperation & obj, Uint32 anAttrId) {
        return obj.getBlobHandle(anAttrId);
    }
    
    static NdbBlob * getBlobHandle(const NdbOperation & obj, const char * anAttrName) {
        return obj.getBlobHandle(anAttrName);
    }
    
    static NdbBlob * getBlobHandle(const NdbOperation & obj, Uint32 anAttrId) {
        return obj.getBlobHandle(anAttrId);
    }    

    static int listIndexes(const NdbDictionary::Dictionary & obj, NdbDictionary::Dictionary::List & list, const char * tableName) {
        return obj.listIndexes(list, tableName);
    }
    
    static int listEvents(const NdbDictionary::Dictionary & obj, NdbDictionary::Dictionary::List & list) {
        return obj.listEvents(list);
    }
    
    static int listObjects(const NdbDictionary::Dictionary & obj, NdbDictionary::Dictionary::List & list, NdbDictionary::Object::Type type = NdbDictionary::Object::TypeUndefined) {
        return obj.listObjects(list, type);
    }

    static int getNdbErrorLine(const NdbOperation & obj) {
        return obj.getNdbErrorLine();
    }
};

#endif // ndbapi_wrapper_hpp
