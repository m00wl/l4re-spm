#include <l4/re/error_helper>
#include <l4/re/util/br_manager>
#include <l4/re/util/object_registry>
#include <l4/util/util.h>

#include "simple-core"
#include "simple-l4re-allocator"
#include "simple-queue"
#include "simple-worker"

using L4Re::chkcap;

L4Re::Util::Registry_server<L4Re::Util::Br_manager_hooks> server;

int
main()
{
  Spmm::SimpleL4ReAllocator *a = new Spmm::SimpleL4ReAllocator();
  Spmm::SimpleQueue *q = new Spmm::SimpleQueue();
  Spmm::SimpleWorker *w = new Spmm::SimpleWorker();
  Spmm::SimpleCore *c = new Spmm::SimpleCore(a, q, w);

  //l4_sleep_forever();

  // TODO: move into SimpleCore
  chkcap(server.registry()->register_obj(static_cast<Spmm::L4ReAllocator *>(a), "spmm_allocator"),
         "Spmm allocator register");
  server.loop();
  server.registry()->unregister_obj(static_cast<Spmm::L4ReAllocator *>(a));

  delete a;
  delete q;
  delete w;
  delete c;
  return 0;
}
