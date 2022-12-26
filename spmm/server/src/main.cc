#include <l4/re/error_helper>
#include <l4/re/util/br_manager>
#include <l4/re/util/object_registry>
#include <l4/util/util.h>

#include "simple-l4re-allocator.h"
#include "ds-l4re-allocator.h"
#include "simple-lock.h"
#include "simple-manager.h"
#include "simple-memory.h"
#include "simple-queue.h"
#include "simple-statistics.h"
#include "simple-worker.h"

using L4Re::chkcap;

L4Re::Util::Registry_server<L4Re::Util::Br_manager_hooks> server;

int main(void)
{
  printf("\n");

  Spmm::SimpleL4ReAllocator *allocator  = new Spmm::SimpleL4ReAllocator();
  //Spmm::DsL4ReAllocator     *allocator  = new Spmm::DsL4ReAllocator(65536);
  Spmm::SimpleLock          *lock       = new Spmm::SimpleLock();
  Spmm::SimpleMemory        *memory     = new Spmm::SimpleMemory();
  Spmm::SimpleQueue         *queue      = new Spmm::SimpleQueue();
  Spmm::SimpleStatistics    *statistics = new Spmm::SimpleStatistics();
  Spmm::SimpleWorker        *worker     = new Spmm::SimpleWorker(131072, 10000);

  Spmm::SimpleManager *manager;
  manager = new Spmm::SimpleManager(allocator, lock, memory, queue, statistics,
                                    worker);

  Spmm::L4ReAllocator *l4re_allocator;
  l4re_allocator = static_cast<Spmm::L4ReAllocator *>(allocator);
  chkcap(server.registry()->register_obj(l4re_allocator, "spmm_allocator"),
         "register allocator");
  server.loop();
  server.registry()->unregister_obj(l4re_allocator);

  delete manager;
  delete worker;
  delete statistics;
  delete queue;
  delete memory;
  delete lock;
  delete allocator;

  return 0;
}
