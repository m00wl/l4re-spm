#pragma once

#include <time.h>

#include "statistics.h"

namespace Spmm
{

class SimpleStatistics : public Statistics
{
private:
  // counters
  l4_uint64_t _pages_shared   = 0;
  l4_uint64_t _pages_sharing  = 0;
  l4_uint64_t _pages_unshared = 0;
  l4_uint64_t _full_scans     = 0;

  void _get_stats()
  {
    time_t my_time = time(NULL);
    printf("SPMM statistics:\n");
    printf("time:\t\t\t%s", ctime(&my_time));
    printf("pages_total:\t\t%llu\n", _pages_unshared + _pages_sharing);
    printf("pages_unshared:\t%llu\n", _pages_unshared);
    printf("pages_sharing:\t%llu\n", _pages_sharing);
    printf("pages_shared:\t\t%llu\n", _pages_shared);
    printf("pages_saved:\t\t%llu\n", _pages_sharing - _pages_shared);
    printf("full_scans:\t\t%llu\n", _full_scans);
    printf("\n");
  }

public:

  void inc_pages_shared(void) override
  { _pages_shared++; }

  void dec_pages_shared(void) override
  { _pages_shared--; }

  void inc_pages_sharing(void) override
  { _pages_sharing++; }

  void dec_pages_sharing(void) override
  { _pages_sharing--; }

  void inc_pages_unshared(void) override
  { _pages_unshared++; }

  void dec_pages_unshared(void) override
  { _pages_unshared--; }

  void inc_full_scans(void) override
  {
    _full_scans++;
    this->_get_stats();
  }
};

} //Spmm
