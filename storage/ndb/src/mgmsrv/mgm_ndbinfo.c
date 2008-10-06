
#include <BaseString.hpp>
#include <ndbinfo.h>

int print_ndbinfo_table_mgm(struct ndbinfo_table* t, BaseString &out)
{
  int i;
  out.appfmt("%d\n",t->ncols);

  for(i=0;i<t->ncols;i++)
    out.appfmt("%s\n",t->col[i].name);

  return 0;
}
