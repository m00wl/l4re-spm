#include "dataspace"
#include "cstdio"

namespace Spmm {

Dataspace::Dataspace(l4_addr_t mem_start,
                     l4_size_t mem_size,
                     L4Re::Dataspace::Flags mem_flags)
{
  _ds_start    = mem_start;
  _ds_size     = mem_size;
  _rw_flags    = mem_flags;
  _map_flags   = L4::Ipc::Gen_fpage<L4::Ipc::Snd_item>::Map;
  _cache_flags = L4::Ipc::Gen_fpage<L4::Ipc::Snd_item>::Cached;
}

int
Dataspace::map_hook(L4Re::Dataspace::Offset offs,
                    L4Re::Dataspace::Flags flags,
                    L4Re::Dataspace::Map_addr min,
                    L4Re::Dataspace::Map_addr max)
{
  printf("mapping requested: off: 0x%08llX\tmin: 0x%08llX\tmax: 0x%08llX\tflags: 0x%lX\n", offs, min, max, flags.raw);
  static_cast<void>(offs);
  static_cast<void>(flags);
  static_cast<void>(min);
  static_cast<void>(max);

  return L4_EOK;
}

} //Spmm
