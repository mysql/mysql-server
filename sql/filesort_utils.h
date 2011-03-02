#ifndef FILESORT_UTILS_INCLUDED

#include "my_base.h"

/*
  Calculate cost of merge sort

    @param num_rows            Total number of rows.
    @param num_keys_per_buffer Number of keys per buffer.
    @param elem_size           Size of each element.

    Calculates cost of merge sort by simulating call to merge_many_buff().

  @retval
    Computed cost of merge sort in disk seeks.

  @note
    Declared here in order to be able to unit test it,
    since library dependencies have not been sorted out yet.

    See also comments get_merge_many_buffs_cost().
*/

#define FILESORT_UTILS_INCLUDED
double get_merge_many_buffs_cost_fast(ha_rows num_rows,
                                      ha_rows num_keys_per_buffer,
                                      uint    elem_size);

#endif  // FILESORT_UTILS_INCLUDED
