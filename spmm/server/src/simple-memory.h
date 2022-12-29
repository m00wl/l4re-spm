#pragma once

#include <map>

#include "memory.h"

namespace Spmm
{

class SimpleMemory : public Memory
{
  typedef std::map<page_t , page_t> map_t;
private:
  map_t _page_map;

  void _unmap_page_from_others(page_t page)
  {
    L4::Cap<L4::Task> const task = L4Re::Env::env()->task();
    l4_fpage_t flexpage = l4_fpage(page, L4_LOG2_PAGESIZE, L4_FPAGE_RWX);
    chksys(task->unmap(flexpage, L4_FP_OTHER_SPACES), "unmap page from others");
  }

  bool _page_contents_match(page_t page1, page_t page2)
  {
    void const *ptr1 = reinterpret_cast<void const *>(page1);
    void const *ptr2 = reinterpret_cast<void const *>(page2);
    bool match = (memcmp(ptr1, ptr2, L4_PAGESIZE) == 0);
    return match;
  }

  void _copy_page_contents(page_t from_page, page_t to_page)
  {
    void *to_ptr = reinterpret_cast<void *>(to_page);
    void const *from_ptr = reinterpret_cast<void const *>(from_page);
    memcpy(to_ptr, from_ptr, L4_PAGESIZE);
  }

  void _map_imm_page(page_t imm_page, page_t page)
  {
    // free page that is going to be overmapped.
    AllocatorFlags vol = Spmm::Allocator::F::VOLATILE;
    manager->free_page(this, vol, page);

    // map imm_page to page.
    L4::Cap<L4::Task> const task = L4Re::Env::env()->task();
    l4_fpage_t flexpage = l4_fpage(imm_page, L4_LOG2_PAGESIZE, L4_FPAGE_RO);
    chksys(task->map(L4Re::This_task, flexpage, page), "map imm_page to page");

    // bookkeeping.
    _page_map[page] = imm_page;
    manager->inc_pages_sharing(this);
    //printf("merging 0x%08lX [0x%08lX --> 0x%08lX]\n", page, page, imm_page);
  }

  bool _is_merged_page(page_t page)
  {
    // search the page mapping relation.
    map_t::iterator it = _page_map.begin();
    while (it != _page_map.end())
    {
      if (page == it->first)
        return true;
      it++;
    }
    // fallthrough.
    return false;
  }

public:

  long merge_pages(page_t page1, page_t page2, MemoryFlags flags) override
  {
    // sanitize.
    //bool page1_valid = (page1 == l4_trunc_page(page1));
    //bool page2_valid = (page2 == l4_trunc_page(page2));
    //bool pages_same = (page1 == page2);
    //bool page1_merged = _is_merged_page(page1);
    //bool page2_merged = _is_merged_page(page2);
    //if (!page1_valid || !page2_valid || pages_same)
    //  return -L4_EINVAL;
    //else if (page2_merged || (flags.vol() == page1_merged))
    //  return -L4_EFAULT;

    // unmap pages, if necessary.
    if (flags.vol())
      _unmap_page_from_others(page1);
    _unmap_page_from_others(page2);

    // make sure page contents still match.
    if (!_page_contents_match(page1, page2))
      return -L4_EFAULT;

    // check flags for case distinction (volatile/immutable).
    if (flags.imm())
    {
      // retrieve the actual immutable page here because we don't do transitive
      // mappings.
      page_t imm_page = _page_map[page1];

      // do the map.
      _map_imm_page(imm_page, page2);
    }
    else //if (flags.vol())
    {
      // prepare new immutable page because we don't have one yet.
      AllocatorFlags imm_flags = Spmm::Allocator::F::IMMUTABLE;
      page_t imm_page = manager->allocate_page(this, imm_flags, /* hint: */ 0);
      _copy_page_contents(page1, imm_page);

      // do the maps.
      _map_imm_page(imm_page, page1);
      _map_imm_page(imm_page, page2);
    }

    return L4_EOK;
  }

  long unmerge_page(page_t page) override
  {
    // sanitize.
    bool page_valid = (page == l4_trunc_page(page));
    bool page_merged = _is_merged_page(page);
    if (!page_valid)
      return -L4_EINVAL;
    else if (!page_merged)
      return -L4_EFAULT;

    // allocate new volatile page and copy contents.
    AllocatorFlags vol_flags = Spmm::Allocator::F::VOLATILE;
    page_t vol_page = manager->allocate_page(this, vol_flags, /* hint: */ page);
    _copy_page_contents(page, vol_page);

    // map volatile page to page.
    l4_fpage_t flexpage = l4_fpage(vol_page, L4_LOG2_PAGESIZE, L4_FPAGE_RWX);
    chksys(L4Re::Env::env()->task()->map(L4Re::This_task, flexpage, page),
           "unmerge: map volatile page to page");

    // notify worker of unmerge operation.
    bool should_free = manager->page_unmerge_notification(this, page);
    if (should_free)
    {
      // free immutable page if requested by worker.
      page_t imm_page = _page_map[page];
      AllocatorFlags imm_flags = Spmm::Allocator::F::IMMUTABLE;
      manager->free_page(this, imm_flags, imm_page);
    }

    //printf("unmerging 0x%08lX [0x%08lX --> 0x%08lX (internal: 0x%08lX)]\n",
    //       page, _page_map[page], page, vol_page);

    // bookkeeping.
    _page_map.erase(page);
    manager->dec_pages_sharing(this);

    return L4_EOK;
  }

  bool is_merged_page(page_t page) override
  { return _is_merged_page(page); }
};

} //Spmm
