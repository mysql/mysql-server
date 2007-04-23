/* Copyright (C) 2004-2005 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include <ndb_types.h>
#include <mgmapi.h>
#include "mgmapi_configuration.hpp"
#include "../mgmsrv/ConfigInfo.hpp"

ndb_mgm_configuration_iterator::ndb_mgm_configuration_iterator
(const ndb_mgm_configuration & conf, unsigned type_of_section)
  : m_config(conf.m_config)
{
  m_sectionNo = ~0;
  m_typeOfSection = type_of_section;
  first();
}

ndb_mgm_configuration_iterator::~ndb_mgm_configuration_iterator(){
  reset();
}

void 
ndb_mgm_configuration_iterator::reset(){
  if(m_sectionNo != (Uint32)~0){
    m_config.closeSection();
  }
}


int
ndb_mgm_configuration_iterator::enter(){
  bool ok = m_config.openSection(m_typeOfSection, m_sectionNo);
  if(ok){
    return 0;
  }

  reset();
  m_sectionNo = ~0;
  return -1;
}

int
ndb_mgm_configuration_iterator::first(){
  reset();
  m_sectionNo = 0;
  return enter();
}

int
ndb_mgm_configuration_iterator::next(){
  reset();
  m_sectionNo++;
  return enter();
}

int
ndb_mgm_configuration_iterator::valid() const {
  return m_sectionNo != (Uint32)~0;
}

int
ndb_mgm_configuration_iterator::find(int param, unsigned search){
  unsigned val = search + 1;

  while(get(param, &val) == 0 && val != search){
    if(next() != 0)
      break;
  }
  
  if(val == search)
    return 0;
  
  return -1;
}

int
ndb_mgm_configuration_iterator::get(int param, unsigned * value) const {
  return m_config.get(param, value) != true;

}

int 
ndb_mgm_configuration_iterator::get(int param, 
				    unsigned long long * value) const{
  return m_config.get(param, value) != true;
}

int 
ndb_mgm_configuration_iterator::get(int param, const char ** value) const {
  return m_config.get(param, value) != true;
}

/**
 * Published C interface
 */
extern "C"
ndb_mgm_configuration_iterator* 
ndb_mgm_create_configuration_iterator(ndb_mgm_configuration * conf, 
				      unsigned type_of_section){
  ndb_mgm_configuration_iterator* iter = (ndb_mgm_configuration_iterator*)
    malloc(sizeof(ndb_mgm_configuration_iterator));
  if(iter == 0)
    return 0;

  return new(iter) ndb_mgm_configuration_iterator(* conf, type_of_section);
}


extern "C"
void ndb_mgm_destroy_iterator(ndb_mgm_configuration_iterator* iter){
  if(iter != 0){
    iter->~ndb_mgm_configuration_iterator();
    free(iter);
  }
}

extern "C"
int 
ndb_mgm_first(ndb_mgm_configuration_iterator* iter){
  return iter->first();
}

extern "C"
int 
ndb_mgm_next(ndb_mgm_configuration_iterator* iter){
  return iter->next();
}

extern "C"
int 
ndb_mgm_valid(const ndb_mgm_configuration_iterator* iter){
  return iter->valid();
}

extern "C"
int 
ndb_mgm_get_int_parameter(const ndb_mgm_configuration_iterator* iter, 
			  int param, unsigned * value){
  return iter->get(param, value);
}

extern "C"
int 
ndb_mgm_get_int64_parameter(const ndb_mgm_configuration_iterator* iter, 
			    int param, Uint64 * value){
  return iter->get(param, value);
}

extern "C"
int 
ndb_mgm_get_string_parameter(const ndb_mgm_configuration_iterator* iter, 
			     int param, const char  ** value){
  return iter->get(param, value);
}

extern "C"
int 
ndb_mgm_find(ndb_mgm_configuration_iterator* iter,
	     int param, unsigned search){
  return iter->find(param, search);
}

/**
 * Retrieve information about parameter
 * @param info : in - pointer to structure allocated by caller
 * @param size : in/out : pointer to int initialized to sizeof(ndb_mgm_param_info)...will be set to bytes set by function on return
*/
extern "C"
int 
ndb_mgm_get_db_parameter_info(Uint32 paramId, struct ndb_mgm_param_info * info, size_t * size) {
  if ( paramId == 0 ) {
      return -1;
  }

  ConfigInfo data;
  for (int i = 0; i < data.m_NoOfParams; i++) {
    if (paramId == data.m_ParamInfo[i]._paramId && strcmp("DB", data.m_ParamInfo[i]._section) == 0) {
        size_t tmp = 0;
        if (tmp + sizeof(info->m_id) <= *size)
        {
          info->m_id = data.m_ParamInfo[i]._paramId;
          tmp += sizeof(info->m_id);
        }

        if (tmp + sizeof(info->m_name) <= *size)
        {
          info->m_name = data.m_ParamInfo[i]._fname;
          tmp += sizeof(info->m_name);
        }

        *size = tmp;
        return 0;
    }
  }
  return -1;
}
