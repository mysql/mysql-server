/*
   Copyright 2009 Sun Microsystems, Inc.

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

#ifndef _CONFIGFACTORY_HPP
#define _CONFIGFACTORY_HPP

#include <util/Properties.hpp>

struct ConfigFactory
{

  static Uint32 get_ndbt_base_port(void)
  {
    Uint32 port = 0;
    const char* base_port_str = getenv("NDBT_BASE_PORT");
    if (base_port_str)
      port = atoi(base_port_str);
    if (!port)
      port = 11000; // default
    return port;
  }


  static Properties create(unsigned mgmds = 1,
                           unsigned ndbds = 1,
                           unsigned mysqlds = 1)
  {
    Uint32 base_port = get_ndbt_base_port();
    Properties config;
    assert(mgmds >= 1 && ndbds >= 1 && mysqlds >= 1);
    for (unsigned n = 1; n <= ndbds + mgmds + mysqlds; n++)
    {
      const char* node;
      Properties node_settings;

      node_settings.put("NodeId", n);

      if (n <= mgmds)
      {
        node = "ndb_mgmd";
        node_settings.put("HostName", "localhost");
        node_settings.put("PortNumber", base_port + n);
      } else if (n <= mgmds + ndbds)
      {
        node = "ndbd";
        if (ndbds == 1)
          node_settings.put("NoOfReplicas", 1);

      } else if (n <= mgmds + ndbds + mysqlds)
      {
        node = "mysqld";
      } else
        abort();

      config.put(node, n, &node_settings);
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

