#include <l4/re/error_helper>
#include <l4/re/util/br_manager>
#include <l4/re/util/object_registry>

#include "alloc"

using L4Re::chkcap;

L4Re::Util::Registry_server<L4Re::Util::Br_manager_hooks> server;

int
main()
{
  static Spmm::Allocator a;
  chkcap(server.registry()->register_obj(&a, "spmm_alloc"),
         "Spmm allocator register");
  printf("Welcome to the SPMM\n");
  server.loop();
  server.registry()->unregister_obj(&a);
  return 0;
}
