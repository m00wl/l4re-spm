#pragma once

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
   * Flags for merge operations.
   */
  struct F
  {
    enum Flags
    {
      /// Treat page1 and page2 as volatile.
      MERGE_VOLATILE  = 0x0,
      /// Treat page1 as immutable (with an already existing shared memory page
      /// mapped to it) and only page2 as volatile.
      MERGE_IMMUTABLE = 0x1,
    };

    L4_TYPES_FLAGS_OPS_DEF(Flags);
  };

  /**
   * Merge two memory pages to point to the same (physical) page.
   *
   * @param page1       The first page that should be merged.
   * @param page2       The second page that should be merged.
   * @param flags       Merge flags, see Spmm::Memory::F::Flags.
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
  virtual long merge_pages(page_t page1, page_t page2, MemoryFlags flags) = 0;

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

  /**
   * Check if a page is currently merged.
   *
   * @param page        The page that should be checked.
   *
   * @returns           True if the page is currently merged.
   */
  virtual bool is_merged_page(page_t page) = 0;
};

struct MemoryFlags : L4::Types::Flags_ops_t<MemoryFlags>
{
  unsigned long raw;

  MemoryFlags() = default;
  constexpr MemoryFlags(Memory::F::Flags f) : raw(f) {}

  constexpr bool vol() const
  { return raw | Memory::F::Flags::MERGE_VOLATILE; }
  constexpr bool imm() const
  { return raw & Memory::F::Flags::MERGE_IMMUTABLE; }
};

} //Spmm
