#pragma once

#include <l4/re/error_helper>
#include <l4/util/util.h>

#include <cstring>
#include <list>
#include <map>

#include "worker.h"

using L4Re::chksys;

namespace Spmm
{

// simple worker class that continuously scans X pages and then sleeps for Y
// seconds (both X and Y are configurable).
// it performs only primitive bookkeeping of merged pages and merge candidates.
class SimpleWorker : public Worker
{
  // primitive checksum of a page (sum of its content).
  // note that this is not a hash and therefore ambiguous!
  typedef l4_uint64_t checksum_t;

  // this workers collection of already encountered volatile pages and their
  // then checksums.
  // gets reset after every pass.
  typedef std::map<page_t, checksum_t> volatile_pages_t;

  // this workers collection of merged immutable pages.
  // persists across passes.
  typedef std::list<std::list<page_t>> immutable_pages_t;
private:
  volatile_pages_t  _volatile_pages;
  immutable_pages_t _immutable_pages;
  l4_uint64_t       _pages_to_scan;
  l4_uint64_t       _sleep_duration;

  checksum_t _calculate_checksum(page_t page)
  {
    // interpret page as array of l4_uint64_t.
    l4_uint64_t const *array = reinterpret_cast<l4_uint64_t const *>(page);
    l4_size_t array_size = L4_PAGESIZE / sizeof(l4_uint64_t);

    // checksum is sum over all of them.
    checksum_t result = 0;
    for (l4_size_t i = 0; i < array_size; i++)
      result += array[i];

    return result;
  }

  bool _page_contents_match(page_t page1, page_t page2)
  {
    void const *ptr1 = reinterpret_cast<void const *>(page1);
    void const *ptr2 = reinterpret_cast<void const *>(page2);
    bool match = (memcmp(ptr1, ptr2, L4_PAGESIZE) == 0);
    return match;
  }

  bool _try_immutable_pages(page_t page)
  {
    bool const successful = true;
    for (immutable_pages_t::value_type &list : _immutable_pages)
    {
      // all pages in the list have the same content.
      // pick first as representative.
      page_t candidate = list.front();

      //if (page == candidate)
      //  continue;

      if (_page_contents_match(page, candidate))
      {
        // match found, proceed to merge.
        MemoryFlags flags = Spmm::Memory::F::MERGE_IMMUTABLE;
        long error = manager->merge_pages(this, candidate, page, flags);
        if (error != L4_EOK)
          return !successful;

        // merge was successful, update page collections.
        _volatile_pages.erase(page);
        list.push_back(page);

        return successful;
      }
    }
    // fallthrough.
    return !successful;
  }

  bool _try_volatile_pages(page_t page)
  {
    bool const successful = true;
    for (volatile_pages_t::value_type &pair : _volatile_pages)
    {
      page_t candidate = pair.first;
      if (candidate == page)
        continue;

      if (_page_contents_match(page, candidate))
      {
        // match_found, proceed to merge.
        MemoryFlags flags = Spmm::Memory::F::MERGE_VOLATILE;
        long error = manager->merge_pages(this, candidate, page, flags);
        if (error != L4_EOK)
          return !successful;

        // merge was successful, update immutable and volatile lists.
        _volatile_pages.erase(candidate);
        _volatile_pages.erase(page);
        _immutable_pages.push_back({page, candidate});

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
    l4_sleep(100000);
    _immutable_pages.clear();
    _volatile_pages.clear();

    while(1)
    {
      //pass.
      printf("worker scan\n");
      for (unsigned int i = 0; i < _pages_to_scan; i++)
      {
        // obtain next page from queue.
        page_t page = manager->get_next_page(this);

        // sanitize.
        if (!page)
          break;

        manager->lock_page(this, page);

        // first try immutable pages.
        bool successful;
        successful = _try_immutable_pages(page);
        if (successful)
        {
          manager->unlock_page(this, page);
          continue; // with next page.
        }

        // calculate checksum
        checksum_t checksum = _calculate_checksum(page);

        // primitive thrashing protection.
        checksum_t old_checksum = _volatile_pages[page];
        bool is_zero_page_or_new = (old_checksum == 0);
        bool checksums_match = (old_checksum == checksum);
        bool is_stable = is_zero_page_or_new || checksums_match;
        if (!is_stable)
        {
          // update checksum.
          _volatile_pages[page] = checksum;
          manager->unlock_page(this, page);
          continue; // with next page.
        }

        // then try volatile pages.
        successful = _try_volatile_pages(page);
        if (successful)
        {
          manager->unlock_page(this, page);
          continue; // with next page.
        }

        // else remember page and then checksum for later.
        _volatile_pages[page] = checksum;
        manager->unlock_page(this, page);
        // and continue with next page.
      }

      //sleep.
      printf("worker sleep\n");
      l4_sleep(_sleep_duration);
      _volatile_pages.clear();
    }
  }

  bool page_unmerge_notification(page_t page) override
  {
    immutable_pages_t::value_type::iterator candidate;

    // iterate over every list in immutable pages.
    for (immutable_pages_t::value_type &list : _immutable_pages)
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
        _immutable_pages.remove(list);
      return freeable;
    }
    // fallthrough.
    // in case the page was not known to this worker, do not recommend to free.
    //chksys(-L4_EINVAL, "page not known to this worker")
    return false;
  }
};

} //Spmm
