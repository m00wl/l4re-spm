#include <l4/re/error_helper>
#include <l4/re/util/br_manager>
#include <l4/re/util/object_registry>
#include <l4/util/util.h>

#include "simple-l4re-allocator"
#include "simple-lock"
#include "simple-manager"
#include "simple-queue"
#include "simple-statistics"
#include "simple-worker"

using L4Re::chkcap;

L4Re::Util::Registry_server<L4Re::Util::Br_manager_hooks> server;

int
main()
{
  Spmm::SimpleL4ReAllocator *a = new Spmm::SimpleL4ReAllocator();
  Spmm::SimpleQueue *q      = new Spmm::SimpleQueue();
  Spmm::SimpleWorker *w     = new Spmm::SimpleWorker(2 * 65536, 10000);
  Spmm::SimpleLock *l       = new Spmm::SimpleLock();
  Spmm::SimpleStatistics *s = new Spmm::SimpleStatistics();
  Spmm::SimpleManager *m    = new Spmm::SimpleManager(a, q, w, l, s);

  chkcap(server.registry()->register_obj(static_cast<Spmm::L4ReAllocator *>(a),
          "spmm_allocator"),
         "Spmm SimpleL4ReAllocator register");
  server.loop();
  server.registry()->unregister_obj(static_cast<Spmm::L4ReAllocator *>(a));

  delete a;
  delete q;
  delete w;
  delete l;
  delete s;
  delete m;

  return 0;
}
