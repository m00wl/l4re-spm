#include <l4/re/dataspace> 
#include <l4/re/env>
#include <l4/re/error_helper>
#include <l4/re/util/cap_alloc>
#include <l4/sys/err.h>
#include <l4/sys/types.h>
#include <l4/util/util.h>

#include <cstdio>
#include <cstring>

using L4Re::chkcap;
using L4Re::chksys;

int main(void)
{
  L4Re::Env const *env = L4Re::Env::env();
  L4::Cap<L4Re::Dataspace> ds;

  // get dataspace from SPMM.
  ds = env->get_cap<L4Re::Dataspace>("spm_dataspace");
  chkcap(ds, "spm_dataspace not valid");

  // prepare address space.
  l4_addr_t addr = 0;
  l4_size_t size = ds->size();
  L4Re::Rm::Flags rm_flags = L4Re::Rm::F::RWX | L4Re::Rm::F::Search_addr;
  chksys(env->rm()->attach(&addr, size, rm_flags, ds),
         "spm_dataspace as attach");
  L4Re::Dataspace::Flags ds_flags = L4Re::Dataspace::F::RWX;
  chksys(ds->map_region(0, ds_flags, addr, addr + size),
         "spm_dataspace mem map");

  printf("obtained spm_dataspace [addr: 0x%08lX, size: %ld bytes]\n",
         addr, size);

  // fabricate optimal scenario.
  void *ptr = reinterpret_cast<void *>(addr);
  memset(ptr, 0xFE, size);
  printf("fabricated optimal scenario.\n");

  // turn optimal into suboptimal scenario.
  //l4_addr_t it = addr + L4_PAGESIZE - 1;
  //l4_uint8_t n = 0;
  //while (it < addr + size)
  //{
  //  *reinterpret_cast<l4_uint8_t *>(it) = n;
  //  n++;
  //  it += L4_PAGESIZE;
  //}
  //printf("fabricated suboptimal scenario.\n");

  // prevent termination.
  printf("scanning can start now...\n");
  l4_sleep_forever();

  return 0;
}
