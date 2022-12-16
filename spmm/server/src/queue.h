#pragma once

#include "manager.h"

namespace Spmm
{

/**
 * Interface for management of memory regions that are being subjected to SPMM
 * operations.
 *
 * Queue components provide a registration interface which other components can
 * use to subscribe single pages or entire memory regions to the same-page
 * merging service. In addition to that, a queue provides iterator-style
 * page-granular access to memory regions under its administration.
 */
class Queue : public Component
{
public:
  virtual ~Queue() {};

  /**
   * Subscribe a page to SPMM operations.
   *
   * @param page  The page which should be registered.
   */
  virtual void register_page(page_t page) = 0;

  /**
   * Unsubscribe a page from SPMM operations.
   *
   * @param page  The page which should be unregistered.
   */
  virtual void unregister_page(page_t page) = 0;

  /**
   * Retrieve a page from the memory regions of this queue, according to an
   * implementation-choosen strategy.
   *
   * @returns     The page.
   */
  virtual page_t get_next_page(void) = 0;
};

} //Spmm
