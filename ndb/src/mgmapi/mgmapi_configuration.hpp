#ifndef MGMAPI_CONFIGURATION_HPP
#define MGMAPI_CONFIGURATION_HPP

#include <ConfigValues.hpp>

struct ndb_mgm_configuration {
  ConfigValues m_config;
};

struct ndb_mgm_configuration_iterator {
  Uint32 m_sectionNo;
  Uint32 m_typeOfSection;
  ConfigValues::ConstIterator m_config;

  ndb_mgm_configuration_iterator(const ndb_mgm_configuration &, unsigned type);
  ~ndb_mgm_configuration_iterator();

  int first();
  int next();
  int valid() const;
  int find(int param, unsigned value);

  int get(int param, unsigned * value) const ;
  int get(int param, Uint64 * value) const ;
  int get(int param, const char ** value) const ;

  //
  void reset();
  int enter();
};

#endif
