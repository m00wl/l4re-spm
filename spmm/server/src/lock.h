#pragma once

#include "manager.h"

namespace Spmm
{

/**
 * Interface for memory access synchronisation functionality.
 *
 * Lock components ensure deadlock- and race-free access to the volatile memory
 * regions that the SPMM provides to its clients. Other components can request
 * page-granular exclusive memory access from a lock in order to protect
 * critical sections of their execution.
 *
 * Every page can only be locked by one thread at every point in time.
 * Further, a single thread might accquire and hold locks to multiple pages
 * simultaneously.
 */
class Lock : public Component
{
public:
  virtual ~Lock() {};

  /**
   * Claim exclusive access to a page. 
   *
   * @param page  The page to which exclusive access should be obtained.
   *
   * In case of contention, this function is expected to block the calling
   * thread until exclusive access has been obtained.
   */
  virtual void lock_page(page_t page) = 0;

  /**
   * Release exclusive access to a page.
   *
   * @param page  The page to which exclusive access should be resigned.
   */
  virtual void unlock_page(page_t page) = 0;
};

} //Spmm
