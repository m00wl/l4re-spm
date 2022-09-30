#include "alloc"
#include <mutex>

std::mutex mtx;

using L4Re::chkcap;
using L4Re::chksys;

namespace Spmm
{

Allocator::Allocator()
{
  pthread_t t;
  pthread_attr_t a;

  pthread_attr_init(&a);
  a.create_flags |= PTHREAD_L4_ATTR_NO_START;

  if (pthread_create(&t, &a, pthread_func, this))
  {
    printf("pthread_create failure\n");
    while (1);
  }

  _thread_cap = L4::Cap<L4::Thread>(pthread_l4_cap(t));
  chkcap(_thread_cap, "Spmm thread cap check");

  pthread_attr_destroy(&a);
}

Allocator::~Allocator()
{
  for (auto *ds : _ds_list)
  {
    server.registry()->unregister_obj(ds);
    delete ds;
  }
}

int
Allocator::op_create(L4::Factory::Rights,
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

  Spmm::Dataspace *ds = new Spmm::Dataspace(mem_addr, mem_size, mem_flags, mem_cap);
  _ds_list.push_back(ds);

  if (_ds_list.size() == 1)
    chksys(L4Re::Env::env()->scheduler()->run_thread(_thread_cap,
             l4_sched_param(2)),
           "Spmm cmp thread run");

  chkcap(server.registry()->register_obj(ds),
         "Spmm dataspace register");
  res = L4::Ipc::make_cap_rw(ds->obj_cap());

  printf("handing out dataspace @ addr: 0x%08lX with size: %ld\n", 
          mem_addr, mem_size);

  return L4_EOK;
}

void
Allocator::test()
{
  //===========================================================================================
  l4_sleep(30000);
  printf("Hi, I am the merger thread!\n");
  l4_addr_t idx = _ds_list.front()->_ds_start;

  l4_sleep(10000);

  l4_addr_t idy = idx + L4_PAGESIZE;
  l4_fpage_t fpage = l4_fpage(l4_trunc_page(idy), L4_LOG2_PAGESIZE, L4_FPAGE_RWX);
  printf("unmapping fpage: 0x%lX to 0x%lX\n", l4_trunc_page(idy), (long unsigned)(l4_trunc_page(idy) + L4_PAGESIZE));
  chksys(L4Re::Env::env()->task()->unmap(fpage, L4_FP_OTHER_SPACES), "unmap");

  l4_sleep(10000);



  printf("mapping page1 to page2\n");
  chksys(_ds_list.front()->map_region(0, L4Re::Dataspace::F::RWX, idy, idy + L4_PAGESIZE), "map page 1 to page 2");

  ////printf("Printing some memory contents:\n");
  ////printf("\nwriting to RAM:\n");
  ////*(unsigned int *)idx = 0xDEAD;

  ////l4_size_t mem_size = 256 * 1024 * 1024;
  ////L4Re::Dataspace::Flags mem_flags = L4Re::Dataspace::F::Ro;
  ////l4_size_t mem_align = 28;

  ////L4::Cap<L4Re::Dataspace> mem_cap =
  ////  chkcap(L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>(),
  ////         "L4Re dataspace cap alloc");

  ////chksys(L4Re::Env::env()
  ////         ->mem_alloc()
  ////         ->alloc(mem_size, mem_cap, 0, mem_align),
  ////       "L4Re dataspace mem alloc");

  ////l4_addr_t mem_addr = 0;
  ////chksys(L4Re::Env::env()->rm()->attach(&mem_addr, mem_size,
  ////         L4Re::Rm::F::Search_addr | L4Re::Rm::F::R, mem_cap),
  ////       "L4Re dataspace region manager attach");

  ////chksys(mem_cap->map_region(0, L4Re::Dataspace::F::Ro, mem_addr,
  ////         mem_addr + mem_size),
  ////       "L4Re dataspace mem map");

  ////printf("mapped dataspace @ 0x%X\n", mem_addr);

  ////*(unsigned int *)mem_addr = 0xdead;
  ////memcpy((void *)mem_addr, (void *)idx, mem_size);
  ////printf("addr: %X\tval:%X\n", mem_addr, *(unsigned int*)mem_addr);
  //printf("addr: %lX\tval:%lX\n", idx, *(unsigned long *)idx);

  ////l4_fpage_t fpage = l4_fpage(l4_trunc_page(idx), L4_LOG2_PAGESIZE, L4_FPAGE_RWX);
  //{
  //std::lock_guard<std::mutex> lg(mtx);
  //l4_fpage_t fpage = l4_fpage(l4_trunc_size(idx, 28), 28, L4_FPAGE_RWX);
  //printf("unmapping fpage: 0x%lX to 0x%lX\n", l4_trunc_size(idx, 28), (long unsigned)(l4_trunc_size(idx, 28) + (1UL << 28)));
  //chksys(L4Re::Env::env()->task()->unmap(fpage, L4_FP_OTHER_SPACES), "unmap");

  ////L4::Cap<L4Re::Dataspace> mem_cap;

  ////chksys(L4Re::Env::env()->rm()->detach(idx, mem_size, &mem_cap, L4Re::This_task),
  ////       "L4Re dataspace region manager detach");

  ////l4_addr_t idx_x = idx;
  ////chksys(L4Re::Env::env()->rm()->attach(&idx_x, mem_size,
  ////         L4Re::Rm::F::R, mem_cap, 0),
  ////       "L4Re dataspace region manager attach");
  ////printf("atttached dataspace @ 0x%X\n", idx_x);

  ////chksys(mem_cap->map_region(0, L4Re::Dataspace::F::Ro, idx_x,
  ////         idx_x + mem_size),
  ////       "L4Re dataspace mem map");
  ////printf("addr: %X\tval:%X\n", idx, *(unsigned int*)idx);
  //}

  //l4_sleep_forever();
  ////l4_sleep(1000);

  ////{
  ////std::lock_guard<std::mutex> lg(mtx);
  ////l4_fpage_t fpage = l4_fpage(l4_trunc_size(idx, 28), 28, L4_FPAGE_RWX);
  ////printf("unmapping fpage: 0x%lX to 0x%lX\n", l4_trunc_size(idx, 28), (long unsigned)(l4_trunc_size(idx, 28)+ 1UL << 28));
  ////chksys(L4Re::Env::env()->task()->unmap(fpage, L4_FP_ALL_SPACES), "unmap");

  ////L4::Cap<L4Re::Dataspace> mem_cap;

  ////chksys(L4Re::Env::env()->rm()->detach(idx, mem_size, &mem_cap, L4Re::This_task),
  ////       "L4Re dataspace region manager detach");

  ////l4_addr_t idx_x = idx;
  ////chksys(L4Re::Env::env()->rm()->attach(&idx_x, mem_size,
  ////         L4Re::Rm::F::RWX, mem_cap, 0),
  ////       "L4Re dataspace region manager attach");
  ////printf("atttached dataspace @ 0x%X\n", idx_x);

  ////chksys(mem_cap->map_region(0, L4Re::Dataspace::F::RWX, idx_x,
  ////         idx_x + mem_size),
  ////       "L4Re dataspace mem map");
  ////printf("addr: %X\tval:%X\n", idx, *(unsigned int*)idx);
  ////}

  ////printf("printing first page from RAM of VM1:\n"); 
  ////for (int i = 0; i <= ((L4_PAGESIZE / 4) / 8); i++, idx+=36)
  ////{
  ////  printf("addr: 0x%X\tval: 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X\n",
  ////         idx,
  ////         *(unsigned int *)(idx),
  ////         *(unsigned int *)(idx+4),
  ////         *(unsigned int *)(idx+8),
  ////         *(unsigned int *)(idx+16),
  ////         *(unsigned int *)(idx+20),
  ////         *(unsigned int *)(idx+24),
  ////         *(unsigned int *)(idx+28),
  ////         *(unsigned int *)(idx+32));
  ////}

  ////while (1)
  ////{

  ////  //for (int i = 0; i < 10; i++, idx+=4)
  ////  //  printf("addr: 0x%X\tval: 0x%08X\n",
  ////  //         idx,
  ////  //         *(unsigned int *)(idx));
  ////  //l4_sleep(1000);
  ////  //idx+=0x100000;

  ////  //l4_fpage_t fpage = l4_fpage(l4_trunc_page(idx), L4_LOG2_PAGESIZE, L4_FPAGE_RW);
  ////  //l4_fpage_t fpage = l4_fpage(l4_trunc_size(idx, 28), 28, L4_FPAGE_RWX);
  ////  //printf(">>>> unmapping fpage: 0x%lX to 0x%lX\n", l4_trunc_size(idx, 28), (long unsigned)(l4_trunc_size(idx, 28)+pow(2.0, 28.0)));
  ////  //chksys(L4Re::Env::env()->task()->unmap(fpage, L4_FP_ALL_SPACES), "unmap");
  ////}
  ////=========================================================================================================================================

  ////printf("Hi from merger thread!\n");

  ////printf("I am going to sleep!\n");
  ////l4_sleep(30000);

  ////l4_addr_t idx = _ds_list.front()->_ds_start;
  ////l4_size_t mem_size = 8192;
  ////l4_fpage_t fpage = l4_fpage(l4_trunc_page(idx), L4_LOG2_PAGESIZE + 1, L4_FPAGE_RWX);
  ////printf("unmapping fpage: 0x%lX to 0x%lX\n", l4_trunc_page(idx), (long unsigned)(l4_trunc_page(idx) +  mem_size));
  ////chksys(L4Re::Env::env()->task()->unmap(fpage, L4_FP_OTHER_SPACES), "unmap");

  printf("Bye from merger thread!\n");

}

} //Spmm
