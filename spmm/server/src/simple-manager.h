#pragma once

#include <l4/re/error_helper>
#include <l4/re/util/br_manager>
#include <l4/re/util/object_registry>
#include <pthread-l4.h>

#include <cstdio>

#include "allocator.h"
#include "lock.h"
#include "manager.h"
#include "memory.h"
#include "queue.h"
#include "statistics.h"
#include "simple-statistics.h"
#include "worker.h"

using L4Re::chksys;

namespace Spmm
{

// a very simple manager which combines exactly one instance of every component
// and mediates between them.
class SimpleManager : public Manager
{
private:
  L4ReAllocator * _allocator;
  Lock *          _lock;
  Memory *        _memory;
  Queue *         _queue;
  Statistics *    _statistics;
  Worker *        _worker;

  void _start_thread(void *(*start_routine) (void *arg), void *arg)
  {
    pthread_t thread;
    pthread_attr_t thread_attributes;
    pthread_attr_init(&thread_attributes);
    if (pthread_create(&thread, &thread_attributes, start_routine, arg))
      chksys(-L4_ENOSYS, "pthread_create failure");
    pthread_attr_destroy(&thread_attributes);
  }

  static void *_as_worker(void *arg)
  {
    static_cast<Spmm::Worker *>(arg)->run();
    return nullptr;
  }

  static void *_as_statistics_reporter(void *arg)
  {
    static_cast<Spmm::SimpleStatistics *>(arg)->report();
    return nullptr;
  }

public:
  SimpleManager(L4ReAllocator *allocator, Lock *lock, Memory *memory,
                Queue *queue, Statistics *statistics, Worker *worker)
    : _allocator(allocator), _lock(lock), _memory(memory), _queue(queue),
      _statistics(statistics), _worker(worker)
  {
    // take ownership of the components.
    _allocator->set_manager(this);
    _lock->set_manager(this);
    _memory->set_manager(this);
    _queue->set_manager(this);
    _statistics->set_manager(this);
    _worker->set_manager(this);

    // start running the worker.
    _start_thread(_as_worker, _worker);

    // start the statistics reporter.
    _start_thread(_as_statistics_reporter, _statistics);
  }

  // simple plugbox that connects every function to its respective component.

  // memory:
  long merge_pages([[maybe_unused]] Component *caller, page_t page1,
                   page_t page2, MemoryFlags flags) const override
  { return _memory->merge_pages(page1, page2, flags); }

  long unmerge_page([[maybe_unused]] Component *caller,
                    page_t page) const override
  { return _memory->unmerge_page(page); }

  bool is_merged_page([[maybe_unused]] Component *caller,
                      page_t page) const override
  { return _memory->is_merged_page(page); }

  // lock:
  void lock_page([[maybe_unused]] Component *caller, page_t page) const override
  { _lock->lock_page(page); }

  void unlock_page([[maybe_unused]] Component *caller,
                   page_t page) const override
  { _lock->unlock_page(page); }

  // allocator:
  page_t allocate_page([[maybe_unused]] Component *caller, AllocatorFlags flags,
                       l4_addr_t hint = 0) const override
  { return _allocator->allocate_page(flags, hint); }

  void free_page([[maybe_unused]] Component *caller, AllocatorFlags flags,
                 page_t page) const override
  { _allocator->free_page(flags, page); }

  // queue:
  void register_page([[maybe_unused]] Component *caller,
                     page_t page) const override
  { _queue->register_page(page); }

  void unregister_page([[maybe_unused]] Component *caller,
                       page_t page) const override
  { _queue->unregister_page(page); }

  page_t get_next_page([[maybe_unused]] Component *caller) const override
  { return _queue->get_next_page(); }

  // worker:
  void run([[maybe_unused]] Component *caller) const override
  { _worker->run(); }

  bool page_unmerge_notification([[maybe_unused]] Component *caller,
                                 page_t page) const override
  { return _worker->page_unmerge_notification(page); }

  // statistics:
  void inc_pages_shared([[maybe_unused]] Component *caller) const override
  { _statistics->inc_pages_shared(); }

  void dec_pages_shared([[maybe_unused]] Component *caller) const override
  { _statistics->dec_pages_shared(); }

  void inc_pages_sharing([[maybe_unused]] Component *caller) const override
  { _statistics->inc_pages_sharing(); }

  void dec_pages_sharing([[maybe_unused]] Component *caller) const override
  { _statistics->dec_pages_sharing(); }

  void inc_pages_unshared([[maybe_unused]] Component *caller) const override
  { _statistics->inc_pages_unshared(); }

  void dec_pages_unshared([[maybe_unused]] Component *caller) const override
  { _statistics->dec_pages_unshared(); }

  void inc_full_scans([[maybe_unused]] Component *caller) const override
  { _statistics->inc_full_scans(); }
};

} //Spmm
