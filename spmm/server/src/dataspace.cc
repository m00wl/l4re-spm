#include "dataspace.h"

namespace Spmm {

Dataspace::Dataspace(l4_addr_t mem_start,
                     l4_size_t mem_size,
                     L4Re::Dataspace::Flags mem_flags,
                     Spmm::Manager *manager)
{
  _ds_start    = mem_start;
  _ds_size     = mem_size;
  _rw_flags    = mem_flags;
  _map_flags   = L4::Ipc::Gen_fpage<L4::Ipc::Snd_item>::Map;
  _cache_flags = L4::Ipc::Gen_fpage<L4::Ipc::Snd_item>::Cached;
  set_manager(manager);
}

int
Dataspace::map_hook(L4Re::Dataspace::Offset offs,
                    L4Re::Dataspace::Flags flags,
                    [[maybe_unused]] L4Re::Dataspace::Map_addr min,
                    [[maybe_unused]] L4Re::Dataspace::Map_addr max)
{
  // check if write page fault
  L4Re::Dataspace::Flags w_or_x = L4Re::Dataspace::F::W | L4Re::Dataspace::F::X;
  if (flags & w_or_x)
  {
    // notice that we pass *every* write page fault to unmerge_page(),
    // since we have no way of knowing whether the faulty page is currently
    // merged or not.
    page_t page = l4_trunc_page(_ds_start + offs);
    manager->unmerge_page(this, page);
  }
  return L4_EOK;
}

} //Spmm
