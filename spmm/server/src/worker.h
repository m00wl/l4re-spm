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
   * Notifies the worker of an unmerging event for a page that it previously
   * requested a merge for.
   *
   * @param page  The page that was unmerged.
   */
  virtual void page_unmerge_notification(page_t page) = 0;
};

} //Spmm
