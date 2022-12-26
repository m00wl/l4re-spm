#pragma once

#include <map>
#include <mutex>

#include "memory.h"

namespace Spmm
{

class SimpleMemory : public Memory
{
  typedef std::map<page_t , page_t> map_t;
private:
  map_t _page_map;
  std::mutex _mutex;

  void _unmap_page_from_others(page_t page)
  {
    L4::Cap<L4::Task> const task = L4Re::Env::env()->task();
    l4_fpage_t flexpage;

    flexpage = l4_fpage(page, L4_LOG2_PAGESIZE, L4_FPAGE_RWX);
    chksys(task->unmap(flexpage, L4_FP_OTHER_SPACES), "unmap page from others");
  }

  bool _page_contents_match(page_t page1, page_t page2)
  {
    void const *page1_ptr = reinterpret_cast<void const *>(page1);
    void const *page2_ptr = reinterpret_cast<void const *>(page2);
    bool matching = (memcmp(page1_ptr, page2_ptr, L4_PAGESIZE) == 0);
    return matching;
  }

  void _copy_page_contents(page_t from_page, page_t to_page)
  {
    void *to_ptr = reinterpret_cast<void *>(to_page);
    void const *from_ptr = reinterpret_cast<void const *>(from_page);
    memcpy(to_ptr, from_ptr, L4_PAGESIZE);
  }

  bool _is_merged_page(page_t page)
  {
    // search for page in page mapping relation.
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
    std::lock_guard<std::mutex> const lock(_mutex);
    manager->lock_page(this, page1);
    manager->lock_page(this, page2);
    l4_fpage_t flexpage;

    // sanitize.
    //long error = L4_EOK;
    //if (page1 != l4_trunc_page(page1))
    //  error = -L4_EINVAL;
    //else if (page2 != l4_trunc_page(page2))
    //  error = -L4_EINVAL;
    //else if (page1 == page2)
    //  error = -L4_EINVAL;
    //else if (flags.vol() && _is_merged_page(page1))
    //  error = -L4_EFAULT;
    //else if (flags.imm() && !_is_merged_page(page1))
    //  error = -L4_EFAULT;
    //else if (_is_merged_page(page2))
    //  error = -L4_EFAULT;

    //if (error != L4_EOK)
    //{
    //  manager->unlock_page(this, page2);
    //  manager->unlock_page(this, page1);
    //  return error;
    //}

    // unmap pages, if necessary.
    _unmap_page_from_others(page2);
    if (flags.vol())
      _unmap_page_from_others(page1);

    // make sure page contents still match.
    if (!_page_contents_match(page1, page2))
    {
      // no match -> release locks and abort.
      manager->unlock_page(this, page2);
      manager->unlock_page(this, page1);
      return -L4_EFAULT;
    }

    // match -> proceed with merge.
    if (flags.imm())
    {
      // free page2, then map page1 to page2.
      AllocatorFlags vol_flags = Spmm::Allocator::F::VOLATILE;
      manager->free_page(this, vol_flags, page2);
      flexpage = l4_fpage(page1, L4_LOG2_PAGESIZE, L4_FPAGE_RO);
      chksys(L4Re::Env::env()->task()->map(L4Re::This_task, flexpage, page2),
             "map page2 to page1");

      // bookkeeping.
      _page_map[page2] = page1;
      manager->inc_pages_sharing(this);

      printf("merging 0x%08lX [0x%08lX --> 0x%08lX]\n", page2, page2, page1);
    }
    else //if (flags.vol())
    {
      page_t imm_page;

      // match -> allocate new imm_page and copy contents.
      AllocatorFlags imm_flags = Spmm::Allocator::F::IMMUTABLE;
      imm_page = manager->allocate_page(this, imm_flags, /* no hint: */ 0);
      _copy_page_contents(page1, imm_page);

      flexpage = l4_fpage(imm_page, L4_LOG2_PAGESIZE, L4_FPAGE_RO);
      AllocatorFlags vol_flags = Spmm::Allocator::F::VOLATILE;

      // free page1, then map imm_page to page1.
      manager->free_page(this, vol_flags, page1);
      chksys(L4Re::Env::env()->task()->map(L4Re::This_task, flexpage, page1),
             "map imm_page to page1");

      // free page2, then map imm_page to page2.
      manager->free_page(this, vol_flags, page2);
      chksys(L4Re::Env::env()->task()->map(L4Re::This_task, flexpage, page2),
             "map imm_page to page2");

      // bookkeeping.
      _page_map[page1] = imm_page;
      _page_map[page2] = imm_page;
      manager->inc_pages_sharing(this);
      manager->inc_pages_sharing(this);

      printf("merging 0x%08lX [0x%08lX --> 0x%08lX]\n", page1, page1, imm_page);
      printf("merging 0x%08lX [0x%08lX --> 0x%08lX]\n", page2, page2, imm_page);
    }

    manager->unlock_page(this, page2);
    manager->unlock_page(this, page1);

    return L4_EOK;
  }

  long unmerge_page(page_t page) override
  {
    std::lock_guard<std::mutex> const lock(_mutex);
    manager->lock_page(this, page);

    // sanitize.
    long error = L4_EOK;
    if (page != l4_trunc_page(page))
      error = -L4_EINVAL;
    else if (!_is_merged_page(page))
      error = -L4_EFAULT;

    if (error != L4_EOK)
    {
      manager->unlock_page(this, page);
      return error;
    }

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

    printf("unmerging 0x%08lX [0x%08lX --> 0x%08lX (internal: 0x%08lX)]\n",
           page, _page_map[page], page, vol_page);

    // bookkeeping.
    _page_map.erase(page);
    manager->dec_pages_sharing(this);

    manager->unlock_page(this, page);

    return L4_EOK;
  }

  bool is_merged_page(page_t page) override
  {
    std::lock_guard<std::mutex> const lock(_mutex);
    return _is_merged_page(page);
  }
};

} //Spmm
