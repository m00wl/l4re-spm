#pragma once

#include <l4/re/error_helper>
#include <l4/util/util.h>

#include <cstring>
#include <list>

#include "worker.h"

using L4Re::chksys;

namespace Spmm
{

// simple worker class that continuously scans X pages and then sleeps for Y
// seconds (both X and Y are configurable).
// it performs only primitive bookkeeping of merged pages and merge candidates
// in the form of two lists.
class SimpleWorker : public Worker
{
  // this workers collection of already encountered volatile pages.
  // gets reset after every pass.
  typedef std::list<page_t> unstable_list_t;

  // this workers collection of merged immutable pages.
  // persists across passes.
  typedef std::list<std::list<page_t>> stable_list_t;
private:
  unstable_list_t  _unstable_pages;
  stable_list_t    _stable_pages;
  l4_uint64_t     _pages_to_scan;
  l4_uint64_t     _sleep_duration;

  bool _pages_match(page_t page1, page_t page2)
  {
    void const *ptr1 = reinterpret_cast<void const *>(page1);
    void const *ptr2 = reinterpret_cast<void const *>(page2);

    manager->lock_page(this, page1);
    manager->lock_page(this, page2);

    int result = memcmp(ptr1, ptr2, L4_PAGESIZE);

    manager->unlock_page(this, page2);
    manager->unlock_page(this, page1);

    return (result == 0);
  }

  bool _try_stable_list(page_t page)
  {
    bool const successful = true;
    for (stable_list_t::value_type &list : _stable_pages)
    {
      page_t candidate = list.front();
      if (_pages_match(page, candidate))
      {
        // match found, proceed to merge.
        long error;
        MemoryFlags flags = Spmm::Memory::F::MERGE_IMMUTABLE;
        error = manager->merge_pages(this, candidate, page, flags);
        if (error != L4_EOK)
          return !successful;

        // merge was successful, update stable list.
        list.push_back(page);

        return successful;
      }
    }
    // fallthrough.
    return !successful;
  }

  bool _try_unstable_list(page_t page)
  {
    bool const successful = true;
    for (unstable_list_t::value_type &candidate : _unstable_pages)
    {
      if (_pages_match(page, candidate))
      {
        // match_found, proceed to merge.
        long error;
        MemoryFlags flags = Spmm::Memory::F::MERGE_VOLATILE;
        error = manager->merge_pages(this, candidate, page, flags);
        if (error != L4_EOK)
          return !successful;

        // merge was successful, update stable and unstable lists.
        _unstable_pages.remove(candidate);
        _stable_pages.push_back({page, candidate});

        return successful;
      }
    }
    // fallthrough.
    return !successful;
  }

public:
  SimpleWorker(l4_uint64_t pages_to_scan, l4_uint64_t sleep_duration)
    : _pages_to_scan(pages_to_scan), _sleep_duration(sleep_duration) {}

  void run(void) override
  {
    l4_sleep(120000);
    _stable_pages.clear();
    _unstable_pages.clear();

    while(1)
    {
      //pass.
      printf("worker scan...\n");
      for (unsigned int i = 0; i < _pages_to_scan; i++)
      {
        // obtain next page from queue.
        page_t page = manager->get_next_page(this);

        // sanitize.
        if (!page)
        {
          printf("No more pages left to scan.\n");
          break;
        }

        // make sure not to operate on the same page.
        _unstable_pages.remove(page);

        // first try stable list.
        bool successful;
        successful = _try_stable_list(page);
        if (successful)
          continue; // with next page.

        // then try unstable list.
        successful = _try_unstable_list(page);
        if (successful)
          continue; // with next page.

        // else add page (back) to unstable list.
        _unstable_pages.push_back(page);

        // and continue with next page.
      }

      //sleep.
      printf("worker sleep...\n");
      l4_sleep(_sleep_duration);
      _unstable_pages.clear();
    }
  }

  bool page_unmerge_notification(page_t page) override
  {
    stable_list_t::value_type::iterator candidate;

    // iterate over every list in stable list.
    for (stable_list_t::value_type &list : _stable_pages)
    {
      // iterate over every page in list.
      candidate = list.begin();
      while (candidate != list.end())
      {
        // and search for page.
        if (page == *candidate)
          break;
        candidate++;
      }

      // if page is not in this list.
      if (candidate == list.end())
        continue; // with next list.

      // else page was found.
      // proceed to delete.
      list.erase(candidate);

      // check whether there are no other pages merged with this page.
      // in this case, the underlying physical memory page can be freed.
      bool freeable = list.empty();
      if (freeable)
        _stable_pages.remove(list);
      return freeable;
    }
    // fallthrough.
    // in case the page was not known to this worker, do not recommend to free.
    return false;
  }
};

} //Spmm
