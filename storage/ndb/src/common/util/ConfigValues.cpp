/*
   Copyright (c) 2004, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <ndb_global.h>
#include <ConfigValues.hpp>
#include <NdbOut.hpp>
#include "util/require.h"

ConfigValues::ConfigValues() {}

ConfigValues::~ConfigValues() {}

ConfigValuesFactory::ConfigValuesFactory() {
  m_cfg = new ConfigValues();
  require(m_cfg != nullptr);
}

ConfigValuesFactory::ConfigValuesFactory(ConfigValues *cfg) { m_cfg = cfg; }

ConfigValuesFactory::~ConfigValuesFactory() { delete m_cfg; }

ConfigValues *ConfigValuesFactory::getConfigValues() {
  ConfigValues *ret = m_cfg;
  m_cfg = nullptr;
  return ret;
}

bool ConfigValuesFactory::createSection(Uint32 section_type, Uint32 type) {
  return m_cfg->createSection(section_type, type);
}

void ConfigValuesFactory::closeSection() { m_cfg->closeSection(); }

bool ConfigValues::ConstIterator::openSection(Uint32 section_type,
                                              Uint32 index) {
  ConfigSection *cs = m_cfg.openSection(section_type, index);
  if (unlikely(cs == nullptr)) {
    return false;
  }
  m_curr_section = cs;
  return true;
}

void ConfigValues::ConstIterator::closeSection() { m_curr_section = nullptr; }

ConfigValues *ConfigValuesFactory::extractCurrentSection(
    const ConfigValues::ConstIterator &cfg) {
  return (ConfigValues *)cfg.m_cfg.copy_current(cfg.m_curr_section);
}

#ifdef __TEST_CV_HASH_HPP

int main(void) {
  srand(time(0));
  for (int t = 0; t < 100; t++) {
    const size_t len = directory(rand() % 1000);

    printf("size = %d\n", len);
    unsigned *buf = new unsigned[len];
    for (size_t key = 0; key < len; key++) {
      Uint32 p = hash(key, len);
      for (size_t j = 0; j < len; j++) {
        buf[j] = p;
        p = nextHash(key, len, p, j + 1);
      }

      for (size_t j = 0; j < len; j++) {
        Uint32 pos = buf[j];
        int unique = 0;
        for (size_t k = j + 1; k < len; k++) {
          if (pos == buf[k]) {
            if (unique > 0)
              printf("size=%d key=%d pos(%d)=%d buf[%d]=%d\n", len, key, j, pos,
                     k, buf[k]);
            unique++;
          }
        }
        if (unique > 1) {
          printf("key = %d size = %d not uniqe!!\n", key, len);
          for (size_t k = 0; k < len; k++) {
            printf("%d ", buf[k]);
          }
          printf("\n");
        }
      }
    }
    delete[] buf;
  }
  return 0;
}

#endif
