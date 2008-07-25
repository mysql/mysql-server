/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "Config.hpp"

#include <mgmapi_config_parameters.h>
#include <NdbOut.hpp>
#include "ConfigInfo.hpp"


Config::Config(struct ndb_mgm_configuration *config_values) :
  m_configValues(config_values)
{
}


Config::~Config() {
  if(m_configValues != 0){
    free(m_configValues);
  }
}

unsigned sections[]=
{
  CFG_SECTION_SYSTEM,
  CFG_SECTION_NODE,
  CFG_SECTION_CONNECTION
};
const size_t num_sections= sizeof(sections)/sizeof(unsigned);

static const ConfigInfo g_info;

void
Config::print() const {

  for(unsigned i= 0; i < num_sections; i++) {
    unsigned section= sections[i];
    ndb_mgm_configuration_iterator it(*m_configValues, section);

    if (it.first())
      continue;

    for(;it.valid();it.next()) {

      Uint32 section_type;
      assert(it.get(CFG_TYPE_OF_SECTION, &section_type) == 0);

      const ConfigInfo::ParamInfo* pinfo= NULL;
      ConfigInfo::ParamInfoIter param_iter(g_info,
                                           section,
                                           section_type);

      ndbout_c("[%s]", g_info.sectionName(section, section_type));

      /*  Loop through the section and print those values that exist */
      Uint32 val;
      Uint64 val64;
      const char* val_str;
      while((pinfo= param_iter.next())){

        if (!it.get(pinfo->_paramId, &val))
          ndbout_c("%s=%u", pinfo->_fname, val);
        else if (!it.get(pinfo->_paramId, &val64))
          ndbout_c("%s=%llu", pinfo->_fname, val64);
        else if (!it.get(pinfo->_paramId, &val_str))
          ndbout_c("%s=%s", pinfo->_fname, val_str);
      }
    }
  }
}
