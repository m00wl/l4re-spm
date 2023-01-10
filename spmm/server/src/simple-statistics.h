#pragma once

#include <chrono>
#include <mutex>

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
  // internal synchronisation
  std::mutex _mutex;

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

  unsigned long _get_current_time_in_ms(void)
  {
    typedef std::chrono::high_resolution_clock hrclock;
    typedef std::chrono::time_point<std::chrono::high_resolution_clock> tp_t;
    typedef std::chrono::milliseconds to_ms;
    unsigned long ms;

    tp_t now = hrclock::now();
    ms = std::chrono::duration_cast<to_ms>(now.time_since_epoch()).count();

    return ms;
  }

public:
  void report(void)
  {
    printf("time, pages_unshared, pages_saved, pages_shared, full_scans\n");
    while(true)
    {
      {
        std::lock_guard<std::mutex> const lock(_mutex);
        printf("%lu, %llu, %llu, %llu, %llu\n", _get_current_time_in_ms(),
               _pages_unshared, _pages_sharing - _pages_shared, _pages_shared,
               _full_scans);
      }
      l4_sleep(5000);
    }
  }

  void inc_pages_shared(void) override
  {
    std::lock_guard<std::mutex> const lock(_mutex);
    _pages_shared++;
  }

  void dec_pages_shared(void) override
  {
    std::lock_guard<std::mutex> const lock(_mutex);
    _pages_shared--;
  }

  void inc_pages_sharing(void) override
  {
    std::lock_guard<std::mutex> const lock(_mutex);
    _pages_sharing++;
  }

  void dec_pages_sharing(void) override
  {
    std::lock_guard<std::mutex> const lock(_mutex);
    _pages_sharing--;
  }

  void inc_pages_unshared(void) override
  {
    std::lock_guard<std::mutex> const lock(_mutex);
    _pages_unshared++;
  }

  void dec_pages_unshared(void) override
  {
    std::lock_guard<std::mutex> const lock(_mutex);
    _pages_unshared--;
  }

  void inc_full_scans(void) override
  {
    std::lock_guard<std::mutex> const lock(_mutex);
    _full_scans++;
    //this->_get_stats();
  }
};

} //Spmm
