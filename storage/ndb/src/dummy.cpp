#include "../include/ndbapi/NdbApi.hpp"

typedef void (Ndb_cluster_connection::* fptr)(const char*);

fptr functions[] = {
  (fptr)&Ndb_cluster_connection::set_name,
  0
};
