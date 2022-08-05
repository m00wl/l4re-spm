#include "dataspace"
//#include "cstdio"

//using L4Re::chksys;

Spmm::Dataspace::Dataspace(l4_addr_t mem_start,
                           l4_size_t mem_size,
                           L4Re::Dataspace::Flags mem_flags)
{
  _ds_start    = mem_start;
  _ds_size     = mem_size;
  _rw_flags    = mem_flags;
  _map_flags   = L4::Ipc::Gen_fpage<L4::Ipc::Snd_item>::Grant;
  _cache_flags = L4::Ipc::Gen_fpage<L4::Ipc::Snd_item>::Cached;
}

int
Spmm::Dataspace::map_hook(L4Re::Dataspace::Offset offs,
                          L4Re::Dataspace::Flags flags,
                          L4Re::Dataspace::Map_addr min,
                          L4Re::Dataspace::Map_addr max)
{
  //printf("mapping requested\n");
  //printf("\toff: %llu\n", offs);
  //printf("\tmin: %llu\n", min);
  //printf("\tmax: %llu\n", max);
  static_cast<void>(offs);
  static_cast<void>(flags);
  static_cast<void>(min);
  static_cast<void>(max);

  return L4_EOK;
}
