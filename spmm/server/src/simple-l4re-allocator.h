#pragma once

#include <l4/re/env>
#include <l4/re/error_helper>
#include <l4/re/util/cap_alloc>

#include <cstdio>
#include <list>
#include <sys/mman.h>

#include "allocator.h"

using L4Re::chkcap;
using L4Re::chksys;

extern L4Re::Util::Registry_server<L4Re::Util::Br_manager_hooks> server;

namespace Spmm
{

// simple allocator component that allocates memory through mmap and does not
// return memory on free.
class SimpleL4ReAllocator : public L4ReAllocator
{
  typedef std::list<Spmm::Dataspace *> ds_list_t;
private:
  ds_list_t _ds_list;

  l4_addr_t _allocate(l4_size_t size)
  {
    // allocate memory through mmap.
    int const prot = PROT_READ | PROT_WRITE | PROT_EXEC;
    int const flags = MAP_ANONYMOUS;
    void *ptr = mmap(0, size, prot, flags, -1, 0);
    l4_addr_t res = reinterpret_cast<l4_addr_t>(ptr);
    return res;
  }

public:
  ~SimpleL4ReAllocator()
  {
    for (ds_list_t::value_type &ds_ptr : _ds_list)
    {
      server.registry()->unregister_obj(ds_ptr);
      delete ds_ptr;
    }
  }

  int op_create(L4::Factory::Rights, L4::Ipc::Cap<void> &res, l4_umword_t type,
                L4::Ipc::Varg_list<> &&args) override
  {
    // check protocol.
    if (type != L4Re::Dataspace::Protocol)
      return -L4_ENODEV;

    // sanitize arguments.
    l4_size_t mem_size;
    L4Re::Dataspace::Flags mem_flags;
    [[maybe_unused]] l4_size_t mem_align;

    L4::Ipc::Varg tags[3];
    for (auto &tag : tags)
      tag = args.pop_front();

    if (!tags[0].is_of_int())
      return -L4_EINVAL;
    mem_size = l4_round_size(tags[0].value<l4_size_t>(), L4_PAGESHIFT);

    mem_flags = tags[1].is_of_int() ?
     static_cast<L4Re::Dataspace::Flags>(tags[1].value<unsigned long>())
     : L4Re::Dataspace::F::Ro;

    // mem_align is ignored in this allocator.
    // defaults to page alignment (due to mmap).

    // allocate backing memory.
    l4_addr_t mem_addr = _allocate(mem_size);
    memset(reinterpret_cast<void *>(mem_addr), 0x0, mem_size);

    // prepare dataspace to hand out.
    Spmm::Dataspace *ds;
    ds = new Spmm::Dataspace(mem_addr, mem_size, mem_flags, this->manager);
    _ds_list.push_back(ds);

    chkcap(server.registry()->register_obj(ds), "register new Spmm::Dataspace");
    res = L4::Ipc::make_cap_rw(ds->obj_cap());

    // register pages for SPMM operations.
    for (l4_addr_t i = mem_addr; i < mem_addr + mem_size; i += L4_PAGESIZE)
    {
      manager->register_page(this, i);
      manager->inc_pages_unshared(this);
    }

    printf("handing out dataspace [addr: 0x%08lX, size: %ld bytes]\n",
            mem_addr, mem_size);

    return L4_EOK;
  }

  page_t allocate_page(AllocatorFlags flags,
                       [[maybe_unused]] l4_addr_t hint) override
  {
    // allocate page.
    page_t page = _allocate(L4_PAGESIZE);

    // update statistics.
    if (flags.imm())
      manager->inc_pages_shared(this);
    else //if (flags.vol())
    {
      manager->register_page(this, page);
      manager->inc_pages_unshared(this);
    }
    return page;
  }

  void free_page(AllocatorFlags flags, page_t page) override
  {
    // simply do nothing.
    // update statistics.
    if (flags.imm())
      manager->dec_pages_shared(this);
    else //if (flags.vol())
    {
      manager->unregister_page(this, page);
      manager->dec_pages_unshared(this);
    }
  }
};

} //Spmm
