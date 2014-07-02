/*
   Copyright (c) 2009, 2014, Oracle and/or it affiliates. All rights reserved.

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

#ifndef _CONFIGFACTORY_HPP
#define _CONFIGFACTORY_HPP

#include <util/Properties.hpp>
#include <kernel/NodeBitmask.hpp>
#include <NdbEnv.h>

struct ConfigFactory
{

  static Uint32 get_ndbt_base_port(void)
  {
    Uint32 port = 0;
    const char* base_port_str = NdbEnv_GetEnv("NDBT_BASE_PORT", (char*)0, 0);
    if (base_port_str)
      port = atoi(base_port_str);
    if (!port)
      port = 11000; // default
    return port;
  }

  static Uint32 getNodeId(NodeBitmask & mask, unsigned arr[], unsigned i)
  {
    Uint32 nodeId = 0;
    if (arr != 0)
    {
      nodeId = arr[i];
    }
    else
    {
      nodeId = mask.find_first();
    }

    require(mask.get(nodeId));
    mask.clear(nodeId);
    return nodeId;
  }

  static Properties create(unsigned mgmds = 1,
                           unsigned ndbds = 1,
                           unsigned mysqlds = 1,
                           unsigned mgmd_nodeids[] = 0,
                           unsigned ndbd_nodeids[] = 0,
                           unsigned mysqld_nodeids[] = 0)
  {
    Uint32 base_port = get_ndbt_base_port() + /* mysqld */ 1;
    Properties config;
    require(mgmds >= 1 && ndbds >= 1 && mysqlds >= 1);
    NodeBitmask mask;
    mask.set();
    mask.clear(Uint32(0));

    for (unsigned i = 0; i < mgmds; i++)
    {
      Uint32 nodeId = getNodeId(mask, mgmd_nodeids, i);
      Properties node_settings;
      node_settings.put("NodeId", nodeId);
      node_settings.put("HostName", "localhost");
      node_settings.put("PortNumber", base_port + i);

      config.put("ndb_mgmd", nodeId, &node_settings);
    }

    for (unsigned i = 0; i < ndbds; i++)
    {
      Uint32 nodeId = getNodeId(mask, ndbd_nodeids, i);
      Properties node_settings;
      node_settings.put("NodeId", nodeId);
      if (ndbds == 1)
        node_settings.put("NoOfReplicas", 1);

      config.put("ndbd", nodeId, &node_settings);
    }

    for (unsigned i = 0; i < mysqlds; i++)
    {
      Uint32 nodeId = getNodeId(mask, mysqld_nodeids, i);
      Properties node_settings;
      node_settings.put("NodeId", nodeId);
      config.put("mysqld", nodeId, &node_settings);
    }

    return config;
  }

  static bool
  put(Properties& config, const char* section, Uint32 section_no,
      const char* key, Uint32 value)
  {
    Properties* p;
    if (!config.getCopy(section, section_no, &p))
      return false;
    if (!p->put(key, value))
      return false;
    if (!config.put(section, section_no, p, true))
      return false;
    return true;
  }

  static bool
  write_config_ini(Properties& config, const char* path)
  {
    FILE* config_file = fopen(path, "w");
    if (config_file == NULL)
      return false;

    Properties::Iterator it(&config);

    while (const char* name = it.next())
    {
      BaseString section_name(name);
      fprintf(config_file, "[%s]\n",
              section_name.substr(0, section_name.lastIndexOf('_')).c_str());

      const Properties* p;
      if (!config.get(name, &p))
        return false;

      Properties::Iterator it2(p);

      while (const char* name2 = it2.next())
      {

        PropertiesType type;
        if (!p->getTypeOf(name2, &type))
          return false;

        switch (type) {
        case PropertiesType_Uint32:
        {
          Uint32 value;
          if (!p->get(name2, &value))
            return false;
          fprintf(config_file, "%s=%u\n", name2, value);
          break;
        }

        case PropertiesType_char:
        {
          const char* value;
          if (!p->get(name2, &value))
            return false;
          fprintf(config_file, "%s=%s\n", name2, value);
          break;
        }

        default:
          abort();
          break;
        }
      }
      fprintf(config_file, "\n");
    }

    fclose(config_file);
    return true;
  }

  static bool
  create_directories(const char* path, Properties & config)
  {
    Properties::Iterator it(&config);

    while (const char* name = it.next())
    {
      BaseString dir;
      dir.assfmt("%s/%s", path, name);
      printf("Creating %s...\n", dir.c_str());
      if (!NdbDir::create(dir.c_str()))
        return false;
    }
    return true;
  }

};

#endif /* _CONFIGFACTORY_HPP */

