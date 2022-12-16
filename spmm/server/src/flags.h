#pragma once

#include <l4/sys/cxx/types>

namespace Spmm
{

/**
 * Flags for SPMM operations.
 */
struct F
{
  enum Flags
  {
    /// Request regular merge of two volatile pages.
    MERGE_VOLATILE  = 0x0,
    /// Treat the first page as immutable (already merged) and only the second
    /// page as volatile.
    MERGE_IMMUTABLE = 0x1,
  };

  L4_TYPES_FLAGS_OPS_DEF(Flags);
};

struct Flags : L4::Types::Flags_opt_t<Flags>
{
  unsigned long raw;
  Flags() = default;
  constexpr Flags(F::Flags f) : raw(f) {}
  constexpr bool vol() const { return raw | F::Flags::MERGE_VOLATILE };
  constexpr bool imm() const { return raw & F::Flags::MERGE_IMMUTABLE };
}

} //Spmm
