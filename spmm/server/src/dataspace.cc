#include "dataspace"
#include "cstdio"

namespace Spmm {

Dataspace::Dataspace(l4_addr_t mem_start,
                     l4_size_t mem_size,
                     L4Re::Dataspace::Flags mem_flags,
                     Spmm::Manager *m)
{
  _ds_start    = mem_start;
  _ds_size     = mem_size;
  _rw_flags    = mem_flags;
  _map_flags   = L4::Ipc::Gen_fpage<L4::Ipc::Snd_item>::Map;
  _cache_flags = L4::Ipc::Gen_fpage<L4::Ipc::Snd_item>::Cached;
  this->set_manager(m);
}

int
Dataspace::map_hook(L4Re::Dataspace::Offset offs,
                    L4Re::Dataspace::Flags flags,
                    [[maybe_unused]] L4Re::Dataspace::Map_addr min,
                    [[maybe_unused]] L4Re::Dataspace::Map_addr max)
{
  //printf("mapping requested: off: 0x%08llX\tmin: 0x%08llX\tmax: 0x%08llX\tflags: 0x%lX\n", offs, min, max, flags.raw);
  if (flags & (L4Re::Dataspace::F::W | L4Re::Dataspace::F::X))
  {
    page_t p = l4_trunc_page(offs);
    if (this->manager->is_merged_p(this, p))
      this->manager->unmerge_p(this, p);
  }
  return L4_EOK;
}

} //Spmm
