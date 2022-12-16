#pragma once

#include <l4/re/env>
#include <l4/re/error_helper>
#include <l4/re/util/dataspace_svr>
#include <l4/sys/cxx/ipc_epiface>

#include "manager.h"

namespace Spmm
{

/**
 * Interface for memory regions which are suitable for same-page merging.
 */
class Dataspace : public Component,
                  public L4Re::Util::Dataspace_svr,
                  public L4::Epiface_t<Spmm::Dataspace, L4Re::Dataspace>
{
public:
  Dataspace(l4_addr_t mem_start,
            l4_size_t mem_size,
            L4Re::Dataspace::Flags mem_flags,
            Spmm::Manager *manager);

  /**
   * See L4Re::Util::Dataspace_svr::map_hook
   */
  int map_hook(L4Re::Dataspace::Offset offs,
               L4Re::Dataspace::Flags flags,
               L4Re::Dataspace::Map_addr min,
               L4Re::Dataspace::Map_addr max) override;
};

} //Spmm
