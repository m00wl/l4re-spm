#include <l4/re/env>
#include <l4/re/util/object_registry>
#include <l4/re/util/br_manager>
#include <l4/re/error_helper>
#include <l4/sys/cxx/ipc_epiface>

#include <cstdio>
#include <cstdlib>
#include <list>

#include "dataspace"

using L4Re::chkcap;
using L4Re::chksys;

static L4Re::Util::Registry_server<L4Re::Util::Br_manager_hooks> server;

namespace Spmm
{

class Allocator : public L4::Epiface_t<Allocator, L4::Factory>
{
private:
  std::list<Spmm::Dataspace *> _ds_list;

public:
  ~Allocator()
  {
    for (auto *ds : _ds_list) {
      server.registry()->unregister_obj(ds);
      delete ds;
    }
  }

  int
  op_create(L4::Factory::Rights,
            L4::Ipc::Cap<void> &res,
            l4_umword_t type,
            L4::Ipc::Varg_list<> &&args)
  {
    if (type != L4Re::Dataspace::Protocol)
      return -L4_ENODEV;

    L4::Ipc::Varg tags[3];
    for (auto &tag : tags) 
      tag = args.pop_front();

    if (!tags[0].is_of_int())
      return -L4_EINVAL;
    l4_size_t mem_size =
      l4_round_size(tags[0].value<l4_size_t>(), L4_PAGESHIFT);

    L4Re::Dataspace::Flags mem_flags =
      tags[1].is_of_int() ?
        static_cast<L4Re::Dataspace::Flags>(tags[1].value<unsigned long>())
        : L4Re::Dataspace::F::Ro;

    l4_size_t mem_align =
      tags[2].is_of_int() ? tags[2].value<l4_size_t>() : 0;

    L4::Cap<L4Re::Dataspace> mem_cap =
      chkcap(L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>(),
             "L4Re dataspace cap alloc");

    chksys(L4Re::Env::env()
             ->mem_alloc()
             ->alloc(mem_size, mem_cap, 0, mem_align),
           "L4Re dataspace mem alloc");

    l4_addr_t mem_addr = 0;
    chksys(L4Re::Env::env()->rm()->attach(&mem_addr, mem_size,
             L4Re::Rm::F::Search_addr | L4Re::Rm::F::RWX, mem_cap),
           "L4Re dataspace region manager attach");

    chksys(mem_cap->map_region(0, L4Re::Dataspace::F::RWX, mem_addr,
             mem_addr + mem_size),
           "L4Re dataspace mem map");

    Spmm::Dataspace *ds = new Spmm::Dataspace(mem_addr, mem_size, mem_flags);
    _ds_list.push_back(ds);
    chkcap(server.registry()->register_obj(ds),
           "Spmm dataspace register");
    res = L4::Ipc::make_cap_full(ds->obj_cap());
    return L4_EOK;
  }
};

}

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
