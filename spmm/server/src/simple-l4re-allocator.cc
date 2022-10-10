#include "simple-l4re-allocator"

using L4Re::chkcap;
using L4Re::chksys;

namespace Spmm
{

int
SimpleL4ReAllocator::op_create(L4::Factory::Rights,
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

  chksys(L4Re::Env::env()->mem_alloc()->alloc(mem_size, mem_cap, 0, mem_align),
         "L4Re dataspace mem alloc");

  l4_addr_t mem_addr = 0;
  L4Re::Rm::Flags rm_flags = L4Re::Rm::F::RWX
                             | L4Re::Rm::F::Reserved
                             | L4Re::Rm::F::Search_addr;
  chksys(L4Re::Env::env()->rm()->reserve_area(&mem_addr, mem_size, rm_flags),
         "L4Re region manager reserve area");

  chksys(mem_cap->map_region(0, L4Re::Dataspace::F::RWX, mem_addr,
           mem_addr + mem_size),
         "L4Re dataspace mem map");

  Spmm::Dataspace *ds = new Spmm::Dataspace(mem_addr, mem_size, mem_flags);
  _ds_list.push_back(ds);

  chkcap(server.registry()->register_obj(ds),
         "Spmm dataspace register");
  res = L4::Ipc::make_cap_rw(ds->obj_cap());

  printf("handing out dataspace @ addr: 0x%08lX with size: %ld\n", 
          mem_addr, mem_size);

  return L4_EOK;
}

l4_addr_t
SimpleL4ReAllocator::palloc(void)
{
  return 0;
}

void
SimpleL4ReAllocator::pfree(l4_addr_t p)
{
  printf("allocator pfree...\n");
}

} //Spmm