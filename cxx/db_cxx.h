#include <db.h>
#include <string.h>

class Dbt;

// DBT and Dbt objects are interchangeable.  So watch out if you use Dbt to make other classes (e.g., with subclassing)
class Dbt : private DBT
{
 public:

    void *    get_data(void) const     { return data; }
    void      set_data(void *p)        { data = p; }
    
    u_int32_t get_size(void) const     { return size; }
    void      set_size(u_int32_t  p)   { size =  p; }
		       
    DBT *get_DBT(void)                 { return (DBT*)this; }

    Dbt(void);
    ~Dbt();

 private:
    // Nothing here.
}
;

