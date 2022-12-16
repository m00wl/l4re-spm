#pragma once

#include <l4/sys/cxx/ipc_epiface>
#include <l4/sys/factory>
#include <l4/re/util/br_manager>
#include <l4/re/util/object_registry>

#include "dataspace.h"
#include "manager.h"

extern L4Re::Util::Registry_server<L4Re::Util::Br_manager_hooks> server;

namespace Spmm
{

/**
 * Interface for memory allocation and deallocation functionality.
 *
 * Allocator components administer the memory that the SPMM operates on.
 * An allocator supplies clients of the SPMM with memory and registers their
 * pages for the same-page merging service at a queue component. In addition to
 * that, it provides management for singular memory pages that other components
 * can request from and return to it.
 */
class Allocator : public Component
{
public:
  virtual ~Allocator() {};

  /**
   * Allocate a page from this allocator.
   *
   * @param hint  An optional hint for the allocator which denotes the address
   *              of where the new page is going to be mapped to. This might
   *              enable allocation strategy-specific optimizations, but
   *              allocators should not rely on this information.
   *              Should be 0 otherwise.
   *
   * @returns     The newly allocated page.
   */
  virtual page_t allocate_page(l4_addr_t hint = 0) = 0;

  /**
   * Return a page to this allocator.
   *
   * @param page  The page that should be returned.
   */
  virtual void free_page(page_t page) = 0;
};

/**
 * Interface for memory allocation and deallocation functionality in the
 * L4Re operating system.
 *
 * In L4, memory is provided through dataspaces. Allocators of this kind supply
 * memory towards clients in the form of dataspaces as well.
 */
class L4ReAllocator : public Allocator,
                      public L4::Epiface_t<L4ReAllocator, L4::Factory>
{
public:
  virtual ~L4ReAllocator() {};

  /**
   * Implementation of the factory create IPC call that clients in L4Re use to
   * request memory from the SPMM.
   */
  virtual int op_create(L4::Factory::Rights,
                        L4::Ipc::Cap<void> &res,
                        l4_umword_t type,
                        L4::Ipc::Varg_list<> &&args) = 0;
};

} //Spmm
