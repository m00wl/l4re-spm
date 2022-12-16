#pragma once

#include "manager.h"

namespace Spmm
{

/**
 * Interface for statistics about SPMM runtime and operational efficiency.
 * 
 * Statistics components provide means to measure the efficiency of SPMM
 * operations during runtime. To achieve this with minimal overhead, a
 * statistics component must implement 4 simple counters:
 *
 * pages_shared   - how many immutable pages are being used.
 * pages_sharing  - how many pages are sharing the immutable pages.
 * pages_unshared - how many pages are volatile but repeatedly checked for
 *                  merging.
 * full_scans     - how many times all mergable areas have been scanned.
 *
 * More sophisticated evaluation variables might be derived from those.
 */
class Statistics : public Component
{
public:
  virtual ~Statistics() {};

  /**
   * Increase the pages_shared counter by one.
   */
  virtual void inc_pages_shared(void) = 0;

  /**
   * Decrease the pages_shared counter by one.
   */
  virtual void dec_pages_shared(void) = 0;

  /**
   * Increase the pages_sharing counter by one.
   */
  virtual void inc_pages_sharing(void) = 0;

  /**
   * Decrease the pages_sharing counter by one.
   */
  virtual void dec_pages_sharing(void) = 0;

  /**
   * Increase the pages_unshared counter by one.
   */
  virtual void inc_pages_unshared(void) = 0;

  /**
   * Decrease the pages_unshared counter by one.
   */
  virtual void dec_pages_unshared(void) = 0;

  /**
   * Increase the full_scans counter by one.
   */
  virtual void inc_full_scans(void) = 0;
};

} //Spmm
