#pragma once

#include <list>

#include "queue.h"

namespace Spmm
{

// simple wrapper implementing the queue interface around a standard library
// list.
class SimpleQueue : public Queue
{
  typedef std::list<page_t> list_t;
private:
  list_t _list;
  list_t::iterator _next_page;

  void _increment_next_page(void)
  {
    // increment internal iterator.
    _next_page++;
    // wrap internal iterator and update statistics, if needed.
    if (_next_page == _list.end())
    {
      _next_page = _list.begin();
      manager->inc_full_scans(this);
    }
  }

public:
  SimpleQueue() { _next_page = _list.begin(); }

  void register_page(page_t page) override
  {
    // check if page is already in the list.
    //for (page_t &p : _list)
    //  if (page == p) return;

    // check if page is already merged.
    //if (manager->is_merged_page(this, page))
    //  return;

    // insert page.
    _list.push_back(page);

    // update internal iterator, if necessary.
    if (_list.size() == 1)
      _next_page = _list.begin();
  }

  void unregister_page(page_t page) override
  {
    // empty list is never accessed.
    if (_list.empty())
      return;

    // if currently merged then unmerge page
    //if (manager->is_merged_page(this, page))
    //{
        // TODO: unmerge logic.
    //}

    // else check if we need to move internal iterator.
    if (*_next_page == page)
      _increment_next_page();

    // remove page.
    _list.remove(page);
  }

  page_t get_next_page(void) override
  {
    // empty list is never accessed.
    if (_list.empty())
      return 0;

    page_t page = *_next_page;
    _increment_next_page();
    return page;
  }
};

} //Spmm
