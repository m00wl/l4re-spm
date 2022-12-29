#pragma once

#include <l4/re/env>
#include <l4/re/error_helper>
#include <l4/re/util/cap_alloc>
#include <l4/util/util.h>

#include <cstdio>
#include <list>
#include <vector>

#include "allocator.h"

using L4Re::chkcap;
using L4Re::chksys;

namespace Spmm
{

// a simple helper class that provides memory at page granular sizes.
// it can serve as the pool of immutable memory pages for another allocator
// component.
class PageAllocator
{
  typedef std::vector<bool> bitmap_t;
private:
  friend class DsL4ReAllocator;
  L4::Cap<L4Re::Dataspace> _buffer_cap;
  l4_addr_t _buffer_addr;
  bitmap_t _free_pages;

  PageAllocator(l4_size_t buffer_size_in_pages)
    : _free_pages(buffer_size_in_pages, true)
  {
    // allocate buffer of pages.
    L4Re::Env const *env = L4Re::Env::env();
    _buffer_cap = chkcap(L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>(),
                        "buffer cap alloc");
    l4_size_t buffer_size = buffer_size_in_pages << L4_PAGESHIFT;
    chksys(env->mem_alloc()->alloc(buffer_size, _buffer_cap),
           "buffer mem alloc");

    // prepare address space.
    _buffer_addr = 0;
    L4Re::Rm::Flags rm_flags = L4Re::Rm::F::RWX
                               | L4Re::Rm::F::Search_addr;
    chksys(env->rm()->attach(&_buffer_addr, buffer_size, rm_flags, _buffer_cap),
           "buffer as attach");

    l4_addr_t buffer_end_addr = _buffer_addr + buffer_size;
    L4Re::Dataspace::Flags ds_flags = L4Re::Dataspace::F::RWX;
    chksys(_buffer_cap->map_region(0, ds_flags, _buffer_addr, buffer_end_addr),
           "buffer mem map");

    printf("initialised internal page pool [addr: 0x%08lX, size: %ld pages]\n",
           _buffer_addr, buffer_size_in_pages);
  };

  PageAllocator(PageAllocator const &pa) = delete;

public:
  page_t allocate_page(void)
  {
    // search for a free page.
    bitmap_t::iterator free_page;
    for (free_page = _free_pages.begin();
         free_page != _free_pages.end();
         free_page++)
    {
      if (*free_page)
        break;
    }

    if (free_page == _free_pages.end())
      chksys(-L4_ENOMEM, "allocator cannot serve this request.");

    // mark page as allocated.
    *free_page = false;

    // convert allocation into pointer.
    l4_size_t free_page_idx = free_page - _free_pages.begin();
    l4_addr_t free_page_addr = _buffer_addr + (free_page_idx << L4_PAGESHIFT);

    l4_touch_rw(reinterpret_cast<void const *>(free_page_addr), L4_PAGESIZE);

    return free_page_addr;
  };

  void free_page(page_t page)
  {
    // find index of page to free.
    l4_size_t page_offs_in_buffer = page - _buffer_addr;
    l4_size_t page_idx = page_offs_in_buffer >> L4_PAGESHIFT;

    // mark page as freed.
    *(_free_pages.begin() + page_idx) = true;

    // return page to the system.
    _buffer_cap->clear(page_offs_in_buffer, L4_PAGESIZE);
  };
};

// an allocator component that manages memory by using dataspaces.
//
// idea:
//
// - for each client allocate an individual dataspace with read-write pages
//   (attached and mapped into the address space of this component).
//   = pool of volatile pages for this client.
//
// - for each client reserve a RegionManager region in the address space of this
//   component (same size as volatile pool)
//   = access window of this client.
//
// - construct the Spmm::Dataspace that a client receives over its access
//   window to enforce the access accordingly.
// - start with identity mapping from client volatile pool to client access
//   window (every page is read-write).
//
// - leverage one singular shared dataspace with read-only pages (that can be
//   selectively mapped to the client access windows).
//   = pool of immutable pages for every client.
//
// - use L4Re::Dataspace::clear() for pages that got mapped over or are no
//   longer needed anymore. Notice that this means it depends on the underlying
//   dataspace implementation whether this allocator returns unused (cleared)
//   pages to the system or not.
//
// - if a client needs back a specific page, calculate offset in its access
//   window, and return the corresponding page at this offset in its volatile
//   pool (edited to contain the right contents).
//
class DsL4ReAllocator : public L4ReAllocator
{
  struct client_info_t
  {
    l4_addr_t acc_window_start;
    l4_addr_t vol_pool_start;
    L4::Cap<L4Re::Dataspace> internal_ds_cap;
  };

  typedef std::list<client_info_t> client_log_t;
  typedef std::list<Spmm::Dataspace *> ds_list_t;

private:
  PageAllocator _page_pool;
  client_log_t _clients;
  ds_list_t _ds_list;

  page_t _retrieve_client_page(l4_addr_t hint)
  {
    // search every client.
    for (client_log_t::value_type &client_info : _clients)
    {
      // check lower bound.
      if (!(client_info.acc_window_start <= hint))
        continue; // with next client.

      // check upper bound.
      l4_size_t mem_size = client_info.internal_ds_cap->size();
      l4_size_t mem_offset = hint - client_info.acc_window_start;
      if (!(mem_offset < mem_size))
        continue; // with next client.

      // found correct client.
      page_t page = client_info.vol_pool_start + mem_offset;
      l4_touch_rw(reinterpret_cast<void const *>(page), L4_PAGESIZE);
      return page;
    }
    // fallthrough.
    // in case the hint does not belong to a client of this allocator, return
    // invalid page.
    return 0;
  };

  void _free_client_page(page_t page)
  {
    // search every client.
    for (client_log_t::value_type &client_info : _clients)
    {
      // check lower bound.
      if (!(client_info.acc_window_start <= page))
        continue; // with next client.

      // check upper bound.
      l4_size_t mem_size = client_info.internal_ds_cap->size();
      l4_size_t mem_offset = page - client_info.acc_window_start;
      if (!(mem_offset < mem_size))
        continue; // with next client.

      // found correct client.
      client_info.internal_ds_cap->clear(mem_offset, L4_PAGESIZE);
      return;
    }
    // fallthrough.
    // in case the page does not belong to a client of this allocator,
    // simply do nothing.
  };

public:
  DsL4ReAllocator(l4_size_t pool_size) : _page_pool(pool_size)
  {};

  ~DsL4ReAllocator()
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
    l4_size_t mem_align;

    L4::Ipc::Varg tags[3];
    for (auto &tag : tags)
      tag = args.pop_front();

    if (!tags[0].is_of_int())
      return -L4_EINVAL;
    mem_size = l4_round_size(tags[0].value<l4_size_t>(), L4_PAGESHIFT);

    mem_flags = tags[1].is_of_int() ?
     static_cast<L4Re::Dataspace::Flags>(tags[1].value<unsigned long>())
     : L4Re::Dataspace::F::Ro;

    mem_align = tags[2].is_of_int() ? tags[2].value<l4_size_t>() : 0;

    // allocate backing memory.
    L4Re::Env const *env = L4Re::Env::env();
    L4::Cap<L4Re::Dataspace> mem_cap;
    mem_cap = chkcap(L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>(),
                     "ds cap alloc");
    chksys(env->mem_alloc()->alloc(mem_size, mem_cap, 0, mem_align),
           "ds mem alloc");

    // attach ds and map into address space (volatile pool).
    l4_addr_t vol_pool_start = 0;
    L4Re::Rm::Flags rm_flags = L4Re::Rm::F::RWX | L4Re::Rm::F::Search_addr;
    chksys(env->rm()->attach(&vol_pool_start, mem_size, rm_flags, mem_cap),
           "ds as attach (volatile pool)");
    L4Re::Dataspace::Flags ds_flags = L4Re::Dataspace::F::RWX;
    l4_addr_t vol_pool_end = vol_pool_start + mem_size;
    chksys(mem_cap->map_region(0, ds_flags, vol_pool_start, vol_pool_end),
           "ds mem map (volatile pool)");

    // reserve region and map into address space (access window).
    l4_addr_t acc_window_start = 0;
    rm_flags |= L4Re::Rm::F::Reserved;
    chksys(env->rm()->reserve_area(&acc_window_start, mem_size, rm_flags),
           "ds as reserve (access window)");
    l4_addr_t acc_window_end = acc_window_start + mem_size;
    chksys(mem_cap->map_region(0, ds_flags, acc_window_start, acc_window_end),
           "ds mem map (access window)");

    // log client information.
    _clients.push_back({acc_window_start, vol_pool_start, mem_cap});

    // prepare dataspace to hand out.
    Spmm::Dataspace *ds;
    ds = new Spmm::Dataspace(acc_window_start, mem_size, mem_flags, manager);
    _ds_list.push_back(ds);

    chkcap(server.registry()->register_obj(ds), "register new Spmm::Dataspace");
    res = L4::Ipc::make_cap_rw(ds->obj_cap());

    // register pages for SPMM operations.
    for (l4_addr_t i = acc_window_start;
         i < acc_window_end;
         i += L4_PAGESIZE)
    {
      manager->register_page(this, i);
      manager->inc_pages_unshared(this);
    }

    printf("handing out dataspace [addr: 0x%08lX, size: %ld bytes]\n",
           acc_window_start, mem_size);
    printf("associated volatile page pool [addr: 0x%08lX, size: %ld bytes]\n",
           vol_pool_start, mem_size);

    return L4_EOK;
  };

  page_t allocate_page(AllocatorFlags flags, l4_addr_t hint) override
  {
    page_t page;

    if (flags.imm())
    {
      page = _page_pool.allocate_page();
      manager->inc_pages_shared(this);
    }
    else //if (flags.vol())
    {
      page = _retrieve_client_page(hint);
      manager->register_page(this, hint);
      manager->inc_pages_unshared(this);
    }

    return page;
  }

  void free_page(AllocatorFlags flags, page_t page) override
  {
    if (flags.imm())
    {
      _page_pool.free_page(page);
      manager->dec_pages_shared(this);
    }
    else //if (flags.vol())
    {
      _free_client_page(page);
      manager->unregister_page(this, page);
      manager->dec_pages_unshared(this);
    }
  }

};

} //Spmm
