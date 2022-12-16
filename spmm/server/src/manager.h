#pragma once

#include <l4/sys/types.h>

#include <flags.h>

namespace Spmm
{

/**
 * Intra-package uniform type to refer to a memory page.
 * Every page is characterized by the address of its first byte in memory.
 */
typedef l4_addr_t page_t;

/**
 * Abstract "client" class of the mediator pattern.
 *
 * Every interactable component in this package inherits from this base class. 
 */
class Component;

/**
 * Abstract "mediator" class of the mediator pattern.
 *
 * The different *-manager implementations depict arbitrary relations and the
 * communication between at least one or more instantiations of every component.
 * This is the reason why every manager needs to implement *all* interaction
 * functions of *every* component. Component-specific documentation of functions
 * and members can be found in the respective component files.
 */
class Manager
{
public:
  virtual ~Manager() {};

  // memory:
  virtual long
  merge_pages(Component *caller,
              page_t page1,
              page_t page2,
              Flags flags) const = 0;

  virtual long
  unmerge_page(Component *caller, page_t page) const = 0;

  // lock:
  virtual void
  lock_page(Component *caller, page_t page) const = 0;

  virtual void
  unlock_page(Component *caller, page_t page) const = 0;

  // allocator:
  virtual page_t
  allocate_page(Component *caller, l4_addr_t hint = 0) const = 0;

  virtual void
  free_page(Component *caller, page_t page) const = 0;

  // queue:
  virtual void
  register_page(Component *caller, page_t page) const = 0;

  virtual void
  unregister_page(Component *caller, page_t page) const = 0;

  virtual page_t
  get_next_page(Component *caller) const = 0;

  // worker:
  virtual void
  run(Component *caller) const = 0;

  virtual void
  page_unmerge_notification(Component *caller, page_t page) const = 0;

  // statistics:
  virtual void
  inc_pages_shared(Component *caller) const = 0;

  virtual void
  dec_pages_shared(Component *caller) const = 0;

  virtual void
  inc_pages_sharing(Component *caller) const = 0;

  virtual void
  dec_pages_sharing(Component *caller) const = 0;

  virtual void
  inc_pages_unshared(Component *caller) const = 0;

  virtual void
  dec_pages_unshared(Component *caller) const = 0;

  virtual void
  inc_full_scans(Component *caller) const = 0;
};

class Component
{
protected:
  Manager *manager;
  Component(Manager *manager = nullptr) : manager(manager) {}
  virtual ~Component() {};

public:
  void set_manager(Manager *manager) { this->manager = manager; }
};

} //Spmm
