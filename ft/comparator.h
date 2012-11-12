/**
 * comparator.h
 */

#ifndef COMPARATOR_H
#define COMPARATOR_H

#include <db.h>
#include <string.h>

#include <ft/ybt.h>
#include <ft/fttypes.h>

namespace toku {

// a comparator object encapsulates the data necessary for 
// comparing two keys in a fractal tree. it further understands
// that points may be positive or negative infinity.

class comparator {
public:
    void set_descriptor(DESCRIPTOR desc) {
        m_fake_db.cmp_descriptor = desc;
    }

    void create(ft_compare_func cmp, DESCRIPTOR desc) {
        m_cmp = cmp;
        memset(&m_fake_db, 0, sizeof(m_fake_db));
        m_fake_db.cmp_descriptor = desc;
    }

    int compare(const DBT *a, const DBT *b) {
        if (toku_dbt_is_infinite(a) || toku_dbt_is_infinite(b)) {
            return toku_dbt_infinite_compare(a, b);
        } else {
            return m_cmp(&m_fake_db, a, b);
        }
    }

private:
    struct __toku_db m_fake_db;
    ft_compare_func m_cmp;
};

} /* namespace toku */

#endif /* COMPARATOR_H */
