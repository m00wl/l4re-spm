#include <l4/re/env>
#include <l4/re/util/object_registry>
#include <l4/re/util/br_manager>
#include <l4/re/error_helper>
#include <l4/sys/cxx/ipc_epiface>

#include <cstdio>
#include <cstdlib>
#include <list>

#include "spm_ds"

using L4Re::chkcap;
using L4Re::chksys;

static L4Re::Util::Registry_server<L4Re::Util::Br_manager_hooks> server;

class Spm_server : public L4::Epiface_t<Spm_server, L4::Factory>
{
private:
  std::list<Spm_ds *> _spm_ds_list;

public:
  ~Spm_server()
  {
    for (auto *spm_ds : _spm_ds_list)
      free(spm_ds);
  }

  int
  op_create(L4::Factory::Rights,
            L4::Ipc::Cap<void> &res,
            l4_umword_t type,
            L4::Ipc::Varg_list<> &&args)
  {
    if (type != L4RE_PROTO_DATASPACE)
      return -L4_ENODEV;

    L4::Ipc::Varg tags[3] = { L4::Ipc::Varg::nil() };

    {
      auto it = args.begin();
      for (int i = 0; (i < 3 || it != args.end()); i++, ++it) 
        tags[i] = *(it);
    }

    //if (!tag[0].is_of<l4_size_t>())
    //  return -L4_EINVAL;
    l4_size_t mem_size =
      l4_round_size(tags[0].is_nil() ? 268435456 : tags[0].value<l4_size_t>(),
                    L4_PAGESHIFT);

    //if (!tag[1].is_of<unsigned long>())
    //  return -L4_EINVAL;
    L4Re::Dataspace::Flags mem_flags =
      tags[1].is_nil() ?
        L4Re::Dataspace::F::RWX
        : static_cast<L4Re::Dataspace::Flags>(tags[1].value<unsigned long>());

    //if (!tag[2].is_of<l4_size_t>())
    //  return -L4_EINVAL;
    l4_size_t mem_align =
      tags[2].is_nil() ? 28 : tags[2].value<l4_size_t>();

    L4::Cap<L4Re::Dataspace> mem_cap =
      chkcap(L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>(),
             "Dataspace cap alloc");

    chksys(L4Re::Env::env()->mem_alloc()->alloc(mem_size, mem_cap,
                                                0, mem_align),
           "Dataspace mem alloc");

    l4_addr_t mem_addr = 0;
    chksys(L4Re::Env::env()->rm()->attach(&mem_addr, mem_size,
             L4Re::Rm::F::Search_addr | L4Re::Rm::F::RWX, mem_cap),
           "Attach Dataspace to address space");

    chksys(mem_cap->map_region(0, L4Re::Dataspace::F::RWX, mem_addr,
             mem_addr + mem_size),
           "Map memory region");

    Spm_ds *spm_ds = new Spm_ds(mem_addr, mem_size, mem_flags);
    _spm_ds_list.push_back(spm_ds);
    chkcap(server.registry()->register_obj(spm_ds),
           "Register new SPM dataspace");
    res = L4::Ipc::make_cap_rw(spm_ds->obj_cap());
    return L4_EOK;
  }
};

int
main()
{
  static Spm_server spm_server;
  chkcap(server.registry()->register_obj(&spm_server, "spm_server"),
         "Register SPM server");
  printf("Welcome to the SPM server!\n");
  server.loop();
  return 0;
}
