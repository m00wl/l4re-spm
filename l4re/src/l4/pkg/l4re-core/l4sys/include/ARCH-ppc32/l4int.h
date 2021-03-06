/**
 * \file
 * \brief   Fixed sized integer types, ppc32 version
 * \ingroup l4_basic_types
 */
/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 *
 * As a special exception, you may use this file as part of a free software
 * library without restriction.  Specifically, if other files instantiate
 * templates or use macros or inline functions from this file, or you compile
 * this file and link it with other files to produce an executable, this
 * file does not by itself cause the resulting executable to be covered by
 * the GNU General Public License.  This exception does not however
 * invalidate any other reasons why the executable file might be covered by
 * the GNU General Public License.
 */
#pragma once

#include_next <l4/sys/l4int.h>

/**
 * \addtogroup l4_basic_types
 */
/**@{*/

#define L4_MWORD_BITS           32            /**< Size of machine words in bits */

typedef unsigned int            l4_size_t;    /**< \brief Unsigned size type */
typedef signed int              l4_ssize_t;   /**< \brief Signed size type */
/**@}*/

