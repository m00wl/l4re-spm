#pragma once

#include <l4/sys/cxx/types>

#include "manager.h"

namespace Spmm
{

/**
 * Interface for virtual address space management functionality.
 * 
 * Memory components handle the manipulation of virtual page mappings in the
 * SPMM client memory.
 */
class Memory : public Component
{
public:
  virtual ~Memory() {};

  /**
   * Merge two memory pages to point to the same (physical) page.
   *
   * @param page1       The first page that should be merged.
   * @param page2       The second page that should be merged.
   * @param flags       Merge flags, see Spmm::Flags.
   *
   * @retval L4_EOK     Success.
   * @retval -L4_EINVAL Invalid arguments such as merging a page with itself,
   *                    passing in invalid pages or requesting an immutable
   *                    merge for a non-immutable page.
   * @retval -L4_EFAULT The content of both pages does not match.
   *
   * On success, it is guaranteed that both pages refer to the same physical
   * page and are mapped read-only to their respective addresses. It is not
   * guaranteed however, that one of the two passed-in pages is going to be
   * reused for that. On failure, pages and page mappings will not have been
   * modified.
   */
  virtual long merge_pages(page_t page1, page_t page2, Flags flags) = 0;

  /**
   * Unmerge a memory page to assign it to an individual (physical) page.
   *
   * @param page        The page that should be unmerged.
   *
   * @retval L4_EOK     Success.
   * @retval -L4_EINVAL Invalid arguments such as passing in an invalid page.
   * @retval -L4_EFAULT The page was not previously merged.
   *
   * On success, it is guaranteed that the specified page is relocated to an 
   * individual physical page and it is mapped back to its original address
   * with full access rights. On failure, page and page mapping will not have
   * been modified. 
   */
  virtual long unmerge_page(page_t page) = 0;
};

} //Spmm
