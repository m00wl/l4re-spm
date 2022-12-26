#pragma once

#include <mutex>

#include "lock.h"

namespace Spmm
{

// simply synchronise hard on every operation by using a shared mutex.
// this will serialise every merge, scan und unmerge operation.
class SimpleLock : public Lock
{
private:
  // needs to be recursive so that the same thread can accquire the lock
  // multiple times.
  std::recursive_mutex _m;
public:
  void lock_page([[maybe_unused]] page_t page) override { _m.lock(); }
  void unlock_page([[maybe_unused]] page_t page) override { _m.unlock(); }
};

} //Spmm
