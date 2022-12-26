#pragma once

#include "manager.h"

namespace Spmm
{

/**
 * Interface for functionality to identify mergable memory areas.
 *
 * Worker components periodically scan memory pages (obtained from queue
 * components) of SPMM client memory. When a worker identifies matching pages,
 * it initiates a merge operation through the memory component.
 */
class Worker : public Component
{
public:
  virtual ~Worker() {};

  /**
   * Main working loop of the worker.
   *
   * This function is going to be started in a new thread by the workers manager
   * during SPMM initialisation.
   */
  virtual void run(void) = 0;

  /**
   * Helper function for internal bookkeeping.
   *
   * @param page  The page that was unmerged.
   *
   * @returns     True if there are no more memory pages that were merged with
   *              page, and the old mapping (the shared immutable physical page)
   *              can be freed.
   *
   * Other components can call this function in order to notify the worker of an
   * unmerging event for a page that it previously requested a merge for. The
   * worker may use this information for internal bookkeeping
   * (implementation-specific). It returns whether it is safe to free the
   * underlying physical memory page or not (i.e. whether there are other pages
   * that are still mapped to the underlying physical memory page).
   */
  virtual bool page_unmerge_notification(page_t page) = 0;
};

} //Spmm
