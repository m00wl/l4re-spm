/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2015-2020 Kernkonzept GmbH.
 * Author(s): Sarah Hoffmann <sarah.hoffmann@kernkonzept.com>
 *            Alexander Warg <alexander.warg@kernkonzept.com>
 *
 */
#include <l4/cxx/unique_ptr>
#include <l4/cxx/ref_ptr>
#include <l4/re/error_helper>
#include <l4/vbus/vbus>

#ifdef SUPPORT_GZIP_IMAGES
#include <l4/sys/factory>
#include <zlib.h>
#endif

#include <memory>

#include "binary_loader.h"
#include "device_factory.h"
#include "guest.h"
#include "guest_subarch.h"
#include "irq.h"
#include "irq_dt.h"
#include "sys_reg.h"
#include "virt_bus.h"

#include "vm_print.h"

static cxx::unique_ptr<Vmm::Guest> guest;

__thread unsigned vmm_current_cpu_id;

typedef void (*Entry)(Vmm::Vcpu_ptr vcpu);

namespace Vmm {

namespace {

using namespace Arm;

struct DCCSR : Sys_reg_ro
{
  l4_uint32_t flip = 0;

  l4_uint64_t read(Vmm::Vcpu_ptr, Key) override
  {
    // printascii in Linux is doing busyuart which wants to see a
    // busy flag to quit its loop while waituart does not want to
    // see a busy flag; this little trick makes it work
    flip ^= 1 << 29;
    return flip;
  }
};

struct DBGDTRxX : Sys_reg_const<0>
{
  void write(Vmm::Vcpu_ptr, Key, l4_uint64_t v) override
  {
    putchar(v);
  }
};

// Helper for logging read/write accesses to groups of known system registers
// where the 'n' value is encoded by the 'CRm'.
// Write accesses are not performed. Read accesses return 0.
struct Sys_reg_log_n : Sys_reg
{
  Sys_reg_log_n(char const *name)
  : name(name)
  {}

  void write(Vmm::Vcpu_ptr vcpu, Key k, l4_uint64_t v) override
  {
    Dbg(Dbg::Core, Dbg::Info)
      .printf("%08lx: msr %s%d_EL1 = %08llx (ignored)\n",
              vcpu->r.ip, name, (unsigned)k.crm(), v);
  }

  l4_uint64_t read(Vmm::Vcpu_ptr vcpu, Key k) override
  {
    Dbg(Dbg::Core, Dbg::Info)
      .printf("%08lx: mrs %s%d_EL1 (read 0)\n",
              vcpu->r.ip, name, (unsigned)k.crm());
    return 0;
  }

  char const *name;
};

// Helper for logging read/write accesses to dedicated known system registers.
// Write accesses are not performed. Read accesses return 0.
struct Sys_reg_log : Sys_reg
{
  Sys_reg_log(char const *name)
  : name(name)
  {}

  void write(Vmm::Vcpu_ptr vcpu, Key, l4_uint64_t v) override
  {
    Dbg(Dbg::Core, Dbg::Info)
      .printf("%08lx: msr %s = %08llx (ignored)\n", vcpu->r.ip, name, v);
  }

  l4_uint64_t read(Vmm::Vcpu_ptr vcpu, Key) override
  {
    Dbg(Dbg::Core, Dbg::Info)
      .printf("%08lx: mrs %s (read 0)\n", vcpu->r.ip, name);
    return 0;
  }

  char const *name;
};

}

Guest::Guest()
: _gic(Gic::Dist_if::create_dist(l4_vcpu_e_info(*Cpu_dev::main_vcpu())->gic_version,
                                 16))
{
  register_vm_handler(Hvc, Vdev::make_device<Vm_print_device>());
  cxx::Ref_ptr<Sys_reg> r = cxx::make_ref_obj<DCCSR>();
  add_sys_reg_aarch32(14, 0, 0, 1, 0, r); // DBGDSCRint
  add_sys_reg_aarch64( 2, 3, 0, 1, 0, r);
  // MDSCR_EL1 (we can map this to DBGSCRint as long as we only implement bit 29..30
  add_sys_reg_aarch64( 2, 0, 0, 2, 2, r);

  // DBGIDR
  add_sys_reg_aarch32(14, 0, 0, 0, 0, cxx::make_ref_obj<Sys_reg_const<0>>());

  r = cxx::make_ref_obj<DBGDTRxX>();
  add_sys_reg_aarch32(14, 0, 0, 5, 0, r);
  add_sys_reg_aarch64( 2, 3, 0, 5, 0, r);

  // Log miscellaneous debug / non-debug registers
  r = cxx::make_ref_obj<Sys_reg_log_n>("DBGBVR");
  for (unsigned i = 0; i < 16; ++i)
    add_sys_reg_aarch64(2, 0, 0, i, 4, r);

  r = cxx::make_ref_obj<Sys_reg_log_n>("DBGBCR");
  for (unsigned i = 0; i < 16; ++i)
    add_sys_reg_aarch64(2, 0, 0, i, 5, r);

  r = cxx::make_ref_obj<Sys_reg_log_n>("DBGWVR");
  for (unsigned i = 0; i < 16; ++i)
    add_sys_reg_aarch64(2, 0, 0, i, 6, r);

  r = cxx::make_ref_obj<Sys_reg_log_n>("DBGWCR");
  for (unsigned i = 0; i < 16; ++i)
    add_sys_reg_aarch64(2, 0, 0, i, 7, r);

  r = cxx::make_ref_obj<Sys_reg_log>("OSLAR_EL1");
  add_sys_reg_aarch64(2, 0, 1, 0, 4, r);

  r = cxx::make_ref_obj<Sys_reg_log>("OSDLR_EL1");
  add_sys_reg_aarch64(2, 0, 1, 3, 4, r);

  r = cxx::make_ref_obj<Sys_reg_log>("PMUSERENR_EL0");
  add_sys_reg_aarch64(3, 3, 9, 14, 0, r);
}

Guest *
Guest::create_instance()
{
  guest.reset(new Guest());
  return guest.get();
}

Guest *
Guest::instance()
{
  return guest.get();
}

namespace {

using namespace Vdev;

struct F : Factory
{
  cxx::Ref_ptr<Vdev::Device> create(Device_lookup *devs,
                                    Vdev::Dt_node const &node) override
  {
    auto gic = devs->vmm()->gic();
    return gic->setup_gic(devs, node);
  }
};

static F f;
static Vdev::Device_type t1 = { "arm,cortex-a9-gic", nullptr, &f };
static Vdev::Device_type t2 = { "arm,cortex-a15-gic", nullptr, &f };
static Vdev::Device_type t3 = { "arm,cortex-a7-gic", nullptr, &f };
static Vdev::Device_type t4 = { "arm,gic-400", nullptr, &f };
static Vdev::Device_type t5 = { "arm,gic-v3", nullptr, &f };

struct F_timer : Factory
{
  cxx::Ref_ptr<Vdev::Device> create(Device_lookup *devs,
                                    Vdev::Dt_node const &node) override
  {
    Vdev::Irq_dt_iterator it(devs, node);

    // skip the first two interrupts
    for (int i = 0; i < 3; ++i)
      L4Re::chksys(it.next(devs), "Parsing timer interrupt");

    if (!it.ic_is_virt())
      L4Re::chksys(-L4_EINVAL, "Timer not connected to virtual interrupt controller");

    auto timer = Vdev::make_device<Vdev::Core_timer>(it.ic(), it.irq(), node);

    devs->vmm()->set_timer(timer);
    return timer;
  }
};

static F_timer ftimer;
static Vdev::Device_type tt1 = { "arm,armv7-timer", nullptr, &ftimer };
static Vdev::Device_type tt2 = { "arm,armv8-timer", nullptr, &ftimer };

/**
 * Mmio access handler that maps the GICC page.
 *
 * This handler maps the page during the eager-mapping stage before the
 * guest is started. It is also able to respond to page faults in the region
 * and will map the page again. Note, however, that this should normally
 * not happen because the page is pinned in the VM task during its life time.
 * Therefore a warning is printed when the access() function is called.
 */
class Gicc_region_mapper : public Vmm::Mmio_device
{
public:
  Gicc_region_mapper(l4_addr_t base)
  : _fp(l4_fpage(base, L4_PAGESHIFT, L4_FPAGE_RW))
  {}

  int access(l4_addr_t, l4_addr_t, Vcpu_ptr,
             L4::Cap<L4::Vm> vm, l4_addr_t, l4_addr_t) override
  {
    Dbg(Dbg::Core, Dbg::Warn)
      .printf("Access to GICC page trapped into guest handler. Restoring mapping.\n");

    remap_page(vm);

    return Retry;
  }

  void map_eager(L4::Cap<L4::Vm> vm, Vmm::Guest_addr, Vmm::Guest_addr) override
  { remap_page(vm); }

  static void
  update_reg_entry(l4_uint64_t base, l4_uint64_t size, bool strip,
                   Vdev::Dt_node const &node)
  {
    l4_uint64_t gicd_base, gicd_size;
    int res;

    Dbg(Dbg::Irq, Dbg::Info, "GIC")
      .printf("GICC virtualization only supports sizes up to 0x1000,"
              " adjusting device tree node\n");
    if (size > L4_PAGESIZE)
      {
        Dbg(Dbg::Irq, Dbg::Info, "GIC")
          .printf("GIC %s.reg update: Adjusting GICC size from %llx to %lx\n",
                  node.get_name(), size, L4_PAGESIZE);
      }
    if (strip)
      Dbg(Dbg::Irq, Dbg::Info, "GIC")
        .printf("GIC %s.reg update: Stripping superfluous entries\n",
                node.get_name());

    // Get GICD entry
    if ((res = node.get_reg_val(0, &gicd_base, &gicd_size)) < 0)
      {
        Err().printf("Failed to read 'reg[0]' from node %s: %s\n",
                     node.get_name(), node.strerror(res));
        throw L4::Runtime_error(-L4_EINVAL,
                                "Reading device tree entry for GIC");
      }

    // rewrite reg_entry
    size = size < L4_PAGESIZE ? size : L4_PAGESIZE;
    node.set_reg_val(gicd_base, gicd_size);
    node.append_reg_val(base, size);
  }

  static l4_uint64_t
  verify_node(Vdev::Dt_node const &node)
  {
    l4_uint64_t base, size, dummy;
    int res = node.get_reg_val(1, &base, &size);
    if (res < 0)
      {
        Err().printf("Failed to read 'reg[1]' from node %s: %s\n",
                     node.get_name(), node.strerror(res));
        throw L4::Runtime_error(-L4_EINVAL,
                                "Reading device tree entry for GIC");
      }

    // Check the alignment of the GICC page
    if (base & (L4_PAGESIZE - 1))
      {
        Err().printf("%s:The GICC page is not page aligned: <%llx, %llx>.\n",
                     node.get_name(), base, size);
        L4Re::chksys(-L4_EINVAL, "Setting up GICC page");
      }

    // Do we have to adapt the device tree?
    bool strip = node.get_reg_val(2, &dummy, &dummy) >= 0;
    if ((size > L4_PAGESIZE) || strip)
      update_reg_entry(base, size, strip, node);

    return base;
  }

private:
  void remap_page(L4::Cap<L4::Vm> vm) const
  {
    Dbg(Dbg::Mmio, Dbg::Info, "mmio")
      .printf("\tMapping [GICC] -> [%lx - %lx]\n", l4_fpage_memaddr(_fp),
              l4_fpage_memaddr(_fp) + L4_PAGESIZE - 1);
    L4Re::chksys(vm->vgicc_map(_fp), "Mapping VGICC area into guest task");
  }

  l4_fpage_t _fp;
};



} // namespace

void
Guest::map_gicc(Device_lookup *devs, Vdev::Dt_node const &node) const
{
  l4_uint64_t base = Gicc_region_mapper::verify_node(node);
  auto gerr = Vdev::make_device<Gicc_region_mapper>(base);
  devs->vmm()->register_mmio_device(cxx::move(gerr), Region_type::Kernel,
                                    node, 1);
}

void
Guest::check_guest_constraints(l4_addr_t base) const
{
  Dbg warn(Dbg::Mmio, Dbg::Warn, "ram");

  if (guest_64bit)
    {
      if (base & ((1UL << 21) - 1))
        warn.printf(
          "\033[01;31mWARNING: Guest memory not 2MB aligned!\033[m\n"
          "       If you run a 64bit-Linux as a guest,\n"
          "       Linux will likely fail to boot as it expects\n"
          "       a 2MB alignment of its memory.\n"
          "       Current guest RAM alignment is only 0x%x\n",
          1 << __builtin_ctz(base));

      return;
    }

  if (base & ((1UL << 27) - 1))
    warn.printf(
      "\033[01;31mWARNING: Guest memory not 128MB aligned!\033[m\n"
      "       If you run a 32bit-Linux as a guest,\n"
      "       Linux will likely fail to boot as it assumes\n"
      "       a 128MB alignment of its memory.\n"
      "       Current guest RAM alignment is only 0x%x\n",
      1 << __builtin_ctz(base));

  if (base & ~0xf0000000)
    warn.printf(
      "WARNING: Guest memory not 256MB aligned!\n"
      "       If you run a 32bit-Linux as a guest, you might hit a bug\n"
      "       in the arch/arm/boot/compressed/head.S code\n"
      "       that misses an ISB after code has been relocated.\n"
      "       According to the internet a fix for this issue\n"
      "       is floating around.\n");
}

l4_addr_t
Guest::load_linux_kernel(Vm_ram *ram, char const *kernel, Ram_free_list *free_list)
{
  Guest_addr ram_base = free_list->first_free_address();

  l4_addr_t entry = ~0ul;
  std::unique_ptr<Boot::Binary_ds> image(new Boot::Binary_ds(kernel));

  if (image->is_elf_binary())
    {
      entry = image->load_as_elf(ram, free_list);
      guest_64bit = image->is_elf64();
      if (!Guest_64bit_supported && guest_64bit)
        L4Re::chksys(-L4_EINVAL, "Running a 64bit guest on a 32bit host is "
                                 "not possible.");
    }
  else
    {
      auto *h = static_cast<unsigned char const *>(image->get_header());
#ifdef SUPPORT_GZIP_IMAGES
      if (h[0] == 0x1f && h[1] == 0x8b && h[2] == 0x08)
        {
          const L4Re::Env *e = L4Re::Env::env();
          L4Re::Rm::Unique_region<Bytef *> imager_src;
          L4Re::Rm::Unique_region<Byte *> imager_dst;
          size_t compr_sz = image->size();

          L4Re::chksys(e->rm()->attach(&imager_src, compr_sz,
                                       L4Re::Rm::F::Search_addr | L4Re::Rm::F::R,
                                       L4::Ipc::make_cap_rw(image->ds())),
                       "Attach compressed file.");

          uint32_t uncompr_sz = *(uint32_t *)&imager_src.get()[compr_sz - 4];

          info().printf("Detected gzip compressed image: uncompressing (%zd -> %u)\n",
                        compr_sz, uncompr_sz);

          L4::Cap<L4Re::Dataspace> f =
            L4Re::chkcap(L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>(),
                         "Allocate DS cap for uncompressed memory.");

          L4Re::chksys(e->mem_alloc()->alloc(uncompr_sz, f),
                       "Allocate memory in dataspace.");

          L4Re::chksys(e->rm()->attach(&imager_dst, uncompr_sz,
                                       L4Re::Rm::F::Search_addr | L4Re::Rm::F::RW,
                                       L4::Ipc::make_cap_rw(f)),
                       "Attach DS for uncompressed data.");

            z_stream strm = {};
            strm.next_in  = imager_src.get();
            strm.avail_in = compr_sz;

            int err = inflateInit2(&strm, 47);
            if (err == Z_OK)
              {
                strm.next_out = imager_dst.get();
                strm.avail_out = uncompr_sz;
                err = inflate(&strm, Z_NO_FLUSH);

                // Should finish in one round
                if (err != Z_STREAM_END)
                  {
                    Err().printf("zlib decompression error: %s\n", strm.msg);
                    L4Re::throw_error(-L4_EINVAL);
                  }
                else
                  image.reset(new Boot::Binary_ds(f));
              }
            else
              {
                Err().printf("zlib init error: %d\n", err);
                L4Re::throw_error(-L4_EINVAL);
              }
        }
#endif // SUPPORT_GZIP_IMAGES

      // Reload the header in case the image changed
      h = static_cast<unsigned char const *>(image->get_header());
      // 32bit-Linux:
      //  With-EFI:
      //   00000000   4D 5A 10 13  4D 5A 10 13  4D 5A 10 13  4D 5A 10 13  MZ..MZ..MZ..MZ..
      //   00000010   4D 5A 10 13  4D 5A 10 13  4D 5A 10 13  00 00 A0 E1  MZ..MZ..MZ......
      //   00000020   03 00 00 EA  18 28 6F 01  00 00 00 00  18 02 70 00  .....(o.......p.
      //   00000030   01 02 03 04  71 00 00 EA  00 00 00 00  40 00 00 00  ....q.......@...
      //   00000040   50 45 00 00  C2 01 02 00  00 00 00 00  00 00 00 00  PE..............
      //  Without-EFI:
      //   00000000   00 00 A0 E1  00 00 A0 E1  00 00 A0 E1  00 00 A0 E1  ................
      //   00000010   00 00 A0 E1  00 00 A0 E1  00 00 A0 E1  00 00 A0 E1  ................
      //   00000020   03 00 00 EA  18 28 6F 01  00 00 00 00  E0 A4 63 00  .....(o.......c.
      //   00000030   01 02 03 04  00 90 0F E1  B8 0D 00 EB  01 70 A0 E1  .............p..
      //   00000040   02 80 A0 E1  00 20 0F E1  03 00 12 E3  01 00 00 1A  ..... ..........

      // 64bit-Linux:
      //  With-EFI:
      //   00000000   4D 5A 00 91  FF FF 30 14  00 00 08 00  00 00 00 00  MZ....0.........
      //   00000010   00 50 E9 00  00 00 00 00  0A 00 00 00  00 00 00 00  .P..............
      //   00000020   00 00 00 00  00 00 00 00  00 00 00 00  00 00 00 00  ................
      //   00000030   00 00 00 00  00 00 00 00  41 52 4D 64  40 00 00 00  ........ARMd@...
      //   00000040   50 45 00 00  64 AA 02 00  00 00 00 00  00 00 00 00  PE..d...........
      //  Without-EFI:
      //   00000000   00 00 32 14  00 00 00 00  00 00 08 00  00 00 00 00  ..2.............
      //   00000010   00 90 ED 00  00 00 00 00  0A 00 00 00  00 00 00 00  ................
      //   00000020   00 00 00 00  00 00 00 00  00 00 00 00  00 00 00 00  ................
      //   00000030   00 00 00 00  00 00 00 00  41 52 4D 64  00 00 00 00  ........ARMd....
      //   00000040   00 00 00 00  00 00 00 00  00 00 00 00  00 00 00 00  ................

      enum
      {
        IMAGE_FILE_EXECUTABLE_IMAGE = 0x0002,
      };

      struct COFF_header
      {
        l4_uint16_t machine;
        l4_uint16_t nr_sections;
        l4_uint32_t time;
        l4_uint32_t offset_to_symtab;
        l4_uint32_t nr_of_symbols;
        l4_uint16_t size_of_optional_header;
        l4_uint16_t characteristics;

      };

      struct PE_optional_header
      {
        l4_uint16_t magic;
        l4_uint8_t  major_linker_version;
        l4_uint8_t  minor_linker_version;
        l4_uint32_t size_of_code;
        l4_uint32_t size_of_initialized_data;
        l4_uint32_t size_of_uninitialized_data;
        l4_uint32_t address_of_entry_point;
        l4_uint32_t base_of_code;
        l4_uint32_t base_of_data;
      };

      struct PE
      {
        char               magic[4];
        COFF_header        coff;
        PE_optional_header opt;
      };

      if (Guest_64bit_supported
          && h[0x38] == 0x41 && h[0x39] == 0x52
          && h[0x3A] == 0x4d && h[0x3B] == 0x64) // Linux header ARM\x64
        {
          l4_uint64_t l = *reinterpret_cast<l4_uint64_t const *>(&h[8]);
          // Bytes 0xc-0xf have the size
          entry = image->load_as_raw(ram, ram_base + l, free_list);
          this->guest_64bit = true;
        }
      else if (   h[0x24] == 0x18 && h[0x25] == 0x28
               && h[0x26] == 0x6f && h[0x27] == 0x01) // Linux magic
        {
          l4_uint32_t l = *reinterpret_cast<l4_uint32_t const *>(&h[0x28]);
          // Bytes 0x2c-0x2f have the zImage size
          entry = image->load_as_raw(ram, ram_base + l, free_list);
        }
      else if (h[0] == 0x1f && h[1] == 0x8b && h[2] == 0x08)
        // Gzip compressed kernel images are not self-decompressing on ARM
        L4Re::throw_error(-L4_EINVAL,
           "Cannot boot compressed images! Unzip first or enable uvmm gzip support.");
      else if (h[0] == 'M' && h[1] == 'Z')
        {
          l4_uint32_t pe_offset = *reinterpret_cast<l4_uint32_t const *>(&h[0x3c]);

          PE const *pe = reinterpret_cast<PE const *>(&h[pe_offset]);

          if (   pe->magic[0] == 'P'  && pe->magic[1] == 'E'
              && pe->magic[2] == '\0' && pe->magic[3] == '\0')
            {
              if (!(pe->coff.characteristics & IMAGE_FILE_EXECUTABLE_IMAGE))
                L4Re::chksys(-L4_EINVAL, "Non-executable PE file");

              if (   pe->coff.machine == 0xaa64)  // AArch64
                {
                  entry = pe->opt.address_of_entry_point;
                  guest_64bit = true;
                }
              else if (pe->coff.machine == 0x01c2) // ARM or ARM-Thumb
                entry = pe->opt.address_of_entry_point;
              else
                {
                  Err().printf("Unsupported PE architecture (%4x)\n",
                               pe->coff.machine);
                  L4Re::chksys(-L4_EINVAL);
                }
            }
        }

      if (entry == ~0ul)
        {
          enum { Default_entry =  0x208000 };
          entry = image->load_as_raw(ram, ram_base + Default_entry, free_list);
        }
    }

  check_guest_constraints(ram_base.get());

  return entry;
}

  /*
   * Prepare a clean vcpu register state before entering the VM
   *
   * Initializes the VCPU register state according to the mode the
   * VCPU is supposed to run in. Registers related to virtualization
   * (control registers, vcpu state registers) are initialized in the
   * context of the thread handling this virtual CPU.
   * We assume that this state is not changed by invoking
   * vcpu_control_ext().
   */
void
Guest::prepare_vcpu_startup(Vcpu_ptr vcpu, l4_addr_t entry) const
{
  if (Guest_64bit_supported && guest_64bit)
    vcpu->r.flags = Cpu_dev::Flags_default_64;
  else
    {
      vcpu->r.flags = Cpu_dev::Flags_default_32;
      if (entry & 1)
        {
          // set thumb mode, remove thumb bit from address
          vcpu->r.flags |= 1 << 5;
          entry &= ~1;
        }
    }

  vcpu->r.sp    = 0;
  vcpu->r.ip    = entry;
}

void
Guest::prepare_linux_run(Vcpu_ptr vcpu, l4_addr_t entry,
                         Vm_ram * /* ram */, char const * /* kernel */,
                         char const * /* cmd_line */, l4_addr_t dt_boot_addr)
{
  prepare_vcpu_startup(vcpu, entry);

  // Set up the VCPU state as expected by Linux entry
  if (Guest_64bit_supported && guest_64bit)
    {
      vcpu->r.r[0]  = dt_boot_addr;
      vcpu->r.r[1]  = 0;
      vcpu->r.r[2]  = 0;
    }
  else
    {
      vcpu->r.r[0]  = 0;
      vcpu->r.r[1]  = ~0UL;
      vcpu->r.r[2]  = dt_boot_addr;
    }
  vcpu->r.r[3]  = 0;
}

void
Guest::run(cxx::Ref_ptr<Cpu_dev_array> cpus)
{
  if (!_timer)
    warn().printf("WARNING: No timer found. Your guest will likely not work properly!\n");

  for (auto cpu: *cpus.get())
    {
      if (!cpu)
        continue;

      auto vcpu = cpu->vcpu();

      vcpu->user_task = _task.cap();
      cpu->powerup_cpu();
      info().printf("Powered up cpu%d [%p]\n", vcpu.get_vcpu_id(),
                    cpu.get());

      _gic->setup_cpu(vcpu);
    }
  cpus->cpu(0)->mark_on_pending();
  cpus->cpu(0)->startup();
}

void
Guest::cpu_online(Cpu_dev *cpu)
{
  _gic->cpu_online(cpu->vcpu());
}

void
Guest::cpu_offline(Cpu_dev *cpu)
{
  _gic->cpu_offline(cpu->vcpu());
}

void Guest::stop_cpus()
{
  // Exit all vCPU threads into the vmm and stop the vCPUs.
  for (auto cpu: *_cpus.get())
    {
      if (   cpu && cpu->online()
          && cpu->vcpu().get_vcpu_id() != vmm_current_cpu_id)
        cpu->thread_cap()->ex_regs(~0, ~0, L4_THREAD_EX_REGS_TRIGGER_EXCEPTION);
    }
}

l4_msgtag_t
Guest::handle_entry(Vcpu_ptr vcpu)
{
  auto *utcb = l4_utcb();

  vcpu.process_pending_ipc(utcb);
  _gic->schedule_irqs(vmm_current_cpu_id);

  L4::Cap<L4::Thread> myself;
  return myself->vcpu_resume_start(utcb);
}

static void dispatch_vm_call(Vcpu_ptr vcpu)
{
  guest->handle_smccc_call<Guest::Hvc>(vcpu);
}

static void dispatch_smc(Vcpu_ptr vcpu)
{
  guest->handle_smccc_call<Guest::Smc>(vcpu);
}

static void
guest_unknown_fault(Vcpu_ptr vcpu)
{
  Err().printf("unknown trap: err=%lx ec=0x%x ip=%lx lr=%lx\n",
               vcpu->r.err, (int)vcpu.hsr().ec(), vcpu->r.ip, vcpu.get_lr());
  if (!guest->inject_undef(vcpu))
    guest->halt_vm(vcpu);
}

static void
guest_memory_fault(Vcpu_ptr vcpu)
{
  switch (guest->handle_mmio(vcpu->r.pfa, vcpu))
    {
    case Retry: break;
    case Jump_instr: vcpu.jump_instruction(); break;
    default:
      Err().printf("cannot handle VM memory access @ %lx ip=%lx lr=%lx\n",
                   vcpu->r.pfa, vcpu->r.ip, vcpu.get_lr());
      guest->halt_vm(vcpu);
      break;
    }
}

bool
Vmm::Guest::inject_abort(l4_addr_t addr, Vcpu_ptr vcpu)
{
  // Inject an instruction abort?
  bool inst = vcpu.hsr().ec() == Hsr::Ec_iabt_low;
  return inject_abort(vcpu, inst, addr);
}

void
Vmm::Guest::wait_for_timer_or_irq(Vcpu_ptr vcpu)
{
  if (_gic->schedule_irqs(vmm_current_cpu_id))
    return;

  l4_timeout_t to = L4_IPC_NEVER;

  auto *utcb = l4_utcb();
  if (_timer
      && (l4_vcpu_e_read_32(*vcpu, L4_VCPU_E_CNTVCTL) & 3) == 1) // timer enabled and not masked
    {
      // calculate the timeout based on the VTIMER values !
      auto cnt = vcpu.cntvct();
      auto cmp = vcpu.cntv_cval();

      if (cmp <= cnt)
        return;

      l4_uint64_t diff = _timer->get_micro_seconds(cmp - cnt);
      if (0)
        printf("diff=%lld\n", diff);
      l4_rcv_timeout(l4_timeout_abs_u(l4_kip_clock(l4re_kip()) + diff, 8, utcb), &to);
    }

  vcpu.wait_for_ipc(utcb, to);
}

void
Vmm::Guest::handle_wfx(Vcpu_ptr vcpu)
{
  vcpu->r.ip += 2 << vcpu.hsr().il();
  if (vcpu.hsr().wfe_trapped()) // WFE
    return;

  wait_for_timer_or_irq(vcpu);
}

static void
guest_wfx(Vcpu_ptr vcpu)
{ guest->handle_wfx(vcpu); }


void
Vmm::Guest::handle_ppi(Vcpu_ptr vcpu)
{
  switch (vcpu.hsr().svc_imm())
    {
    case 0: // VGIC IRQ
      _gic->handle_maintenance_irq(vmm_current_cpu_id);
      break;
    case 1: // VTMR IRQ
      if (_timer)
        _timer->inject();
      break;
    default:
      Err().printf("unknown virtual PPI: %d\n", (int)vcpu.hsr().svc_imm());
      break;
    }
}

static void
guest_ppi(Vcpu_ptr vcpu)
{ guest->handle_ppi(vcpu); }

static void guest_irq(Vcpu_ptr vcpu)
{
  vcpu.handle_ipc(vcpu->i.tag, vcpu->i.label, l4_utcb());
}

template<unsigned CP>
static void guest_mcrr_access_cp(Vcpu_ptr vcpu)
{
  using Vmm::Arm::Sys_reg;
  using Key = Sys_reg::Key;
  auto hsr = vcpu.hsr();

  try
    {
      Key k = Key::cp_r_64(CP, hsr.mcrr_opc1(), hsr.mcr_crm());

      auto r = guest->sys_reg(k);
      if (hsr.mcr_read())
        {
          l4_uint64_t v = r->read(vcpu, k);
          vcpu.set_gpr(hsr.mcr_rt(), v & 0xffffffff);
          vcpu.set_gpr(hsr.mcrr_rt2(), v >> 32);
        }
      else
        {
          l4_uint64_t v = (vcpu.get_gpr(hsr.mcr_rt()) & 0xffffffff)
                          | (((l4_uint64_t)vcpu.get_gpr(hsr.mcrr_rt2())) << 32);

          r->write(vcpu, k, v);
        }

      vcpu.jump_instruction();
    }
  catch (...)
    {
      printf("%08lx: %s p%u, %d, r%d, c%d, c%d, %d (hsr=%08lx)\n",
             vcpu->r.ip, hsr.mcr_read() ? "MRC" : "MCR", CP,
             (unsigned)hsr.mcr_opc1(),
             (unsigned)hsr.mcr_rt(),
             (unsigned)hsr.mcr_crn(),
             (unsigned)hsr.mcr_crm(),
             (unsigned)hsr.mcr_opc2(),
             (l4_umword_t)hsr.raw());
      vcpu.jump_instruction();
    }
}
template<unsigned CP>
static void guest_mcr_access_cp(Vcpu_ptr vcpu)
{
  using Vmm::Arm::Sys_reg;
  using Key = Sys_reg::Key;
  auto hsr = vcpu.hsr();

  try
    {
      Key k = Key::cp_r(CP, hsr.mcr_opc1(),
                        hsr.mcr_crn(),
                        hsr.mcr_crm(),
                        hsr.mcr_opc2());

      auto r = guest->sys_reg(k);
      if (hsr.mcr_read())
        vcpu.set_gpr(hsr.mcr_rt(), r->read(vcpu, k));
      else
        r->write(vcpu, k, vcpu.get_gpr(hsr.mcr_rt()));

      vcpu.jump_instruction();
    }
  catch (...)
    {
      printf("%08lx: %s p%u, %d, r%d, c%d, c%d, %d (hsr=%08lx)\n",
             vcpu->r.ip, hsr.mcr_read() ? "MRC" : "MCR", CP,
             (unsigned)hsr.mcr_opc1(),
             (unsigned)hsr.mcr_rt(),
             (unsigned)hsr.mcr_crn(),
             (unsigned)hsr.mcr_crm(),
             (unsigned)hsr.mcr_opc2(),
             (l4_umword_t)hsr.raw());
      vcpu.jump_instruction();
    }
}

static void guest_msr_access(Vcpu_ptr vcpu)
{
  using Vmm::Arm::Sys_reg;
  using Key = Sys_reg::Key;
  auto hsr = vcpu.hsr();

  try
    {
      Key k = Key::sr(hsr.msr_op0(),
                      hsr.msr_op1(),
                      hsr.msr_crn(),
                      hsr.msr_crm(),
                      hsr.msr_op2());

      auto r = guest->sys_reg(k);
      if (hsr.msr_read())
        vcpu.set_gpr(hsr.msr_rt(), r->read(vcpu, k));
      else
        r->write(vcpu, k, vcpu.get_gpr(hsr.msr_rt()));

      vcpu.jump_instruction();
    }
  catch (...)
    {
      if (hsr.msr_read())
        printf("%08lx: mrs r%u, S%u_%u_C%u_C%u_%u (hsr=%08lx)\n",
               vcpu->r.ip, (unsigned)hsr.msr_rt(),
               (unsigned)hsr.msr_op0(),
               (unsigned)hsr.msr_op1(),
               (unsigned)hsr.msr_crn(),
               (unsigned)hsr.msr_crm(),
               (unsigned)hsr.msr_op2(),
               (l4_umword_t)hsr.raw());
      else
        printf("%08lx: msr S%u_%u_C%u_C%u_%u = %08lx (hsr=%08lx)\n",
               vcpu->r.ip,
               (unsigned)hsr.msr_op0(),
               (unsigned)hsr.msr_op1(),
               (unsigned)hsr.msr_crn(),
               (unsigned)hsr.msr_crm(),
               (unsigned)hsr.msr_op2(),
               vcpu.get_gpr(hsr.msr_rt()),
               (l4_umword_t)hsr.raw());

      vcpu.jump_instruction();
    }
}

void Vmm::Guest::handle_ex_regs_exception(Vcpu_ptr vcpu)
{
  // stop this vcpu
  _cpus->cpu(vcpu.get_vcpu_id())->stop();
}

static void ex_regs_exception(Vcpu_ptr vcpu)
{
  guest->handle_ex_regs_exception(vcpu);
}

extern "C" l4_msgtag_t prepare_guest_entry(Vcpu_ptr vcpu);
l4_msgtag_t prepare_guest_entry(Vcpu_ptr vcpu)
{ return guest->handle_entry(vcpu); }

} // namespace

using namespace Vmm;
Entry vcpu_entries[64] =
{
  [0x00] = guest_unknown_fault,
  [0x01] = guest_wfx,
  [0x02] = guest_unknown_fault,
  [0x03] = guest_mcr_access_cp<15>,
  [0x04] = guest_mcrr_access_cp<15>,
  [0x05] = guest_mcr_access_cp<14>,
  [0x06] = guest_unknown_fault,
  [0x07] = guest_unknown_fault,
  [0x08] = guest_unknown_fault,
  [0x09] = guest_unknown_fault,
  [0x0a] = guest_unknown_fault,
  [0x0b] = guest_unknown_fault,
  [0x0c] = guest_mcrr_access_cp<14>,
  [0x0d] = guest_unknown_fault,
  [0x0e] = guest_unknown_fault,
  [0x0f] = guest_unknown_fault,
  [0x10] = guest_unknown_fault,
  [0x11] = guest_unknown_fault,
  [0x12] = dispatch_vm_call,
  [0x13] = dispatch_smc,
  [0x14] = guest_unknown_fault,
  [0x15] = guest_unknown_fault,
  [0x16] = dispatch_vm_call,
  [0x17] = dispatch_smc,
  [0x18] = guest_msr_access,
  [0x19] = guest_unknown_fault,
  [0x1a] = guest_unknown_fault,
  [0x1b] = guest_unknown_fault,
  [0x1c] = guest_unknown_fault,
  [0x1d] = guest_unknown_fault,
  [0x1e] = guest_unknown_fault,
  [0x1f] = guest_unknown_fault,
  [0x20] = guest_memory_fault,
  [0x21] = guest_unknown_fault,
  [0x22] = guest_unknown_fault,
  [0x23] = guest_unknown_fault,
  [0x24] = guest_memory_fault,
  [0x25] = guest_unknown_fault,
  [0x26] = guest_unknown_fault,
  [0x27] = guest_unknown_fault,
  [0x28] = guest_unknown_fault,
  [0x29] = guest_unknown_fault,
  [0x2a] = guest_unknown_fault,
  [0x2b] = guest_unknown_fault,
  [0x2c] = guest_unknown_fault,
  [0x2d] = guest_unknown_fault,
  [0x2e] = guest_unknown_fault,
  [0x2f] = guest_unknown_fault,
  [0x30] = guest_unknown_fault,
  [0x31] = guest_unknown_fault,
  [0x32] = guest_unknown_fault,
  [0x33] = guest_unknown_fault,
  [0x34] = guest_unknown_fault,
  [0x35] = guest_unknown_fault,
  [0x36] = guest_unknown_fault,
  [0x37] = guest_unknown_fault,
  [0x38] = guest_unknown_fault,
  [0x39] = guest_unknown_fault,
  [0x3a] = guest_unknown_fault,
  [0x3b] = guest_unknown_fault,
  [0x3c] = guest_unknown_fault,
  [0x3d] = guest_ppi,
  [0x3e] = ex_regs_exception,
  [0x3f] = guest_irq
};
