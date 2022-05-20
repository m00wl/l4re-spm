# L4Re same-page merging

`<WIP>`

## Structure

- `l4re/`: directory for the replacable L4Re-snapshot.

- `spmm/`: L4Re-package that contains the same-page merging monitor.

## Installation/Update

1. Obtain the latest [L4Re-snapshot](https://l4re.org/download/snapshots), unpack and place it into `l4re/`.

   Optional: Apply the following patch to tweak the snapshot for NixOS.
   
   ```diff
   diff --git a/l4re/bin/setup.d/04-setup b/l4re/bin/setup.d/04-setup
   index dce4482d..6deb312a 100755
   --- a/l4re/bin/setup.d/04-setup
   +++ b/l4re/bin/setup.d/04-setup
   @@ -1,4 +1,4 @@
   -#! /bin/bash
   +#! /usr/bin/env bash
    
    # This scripts sets up a snapshot on the target build machine.
    # TODO: Add question for compiler location :(
   diff --git a/l4re/src/l4/pkg/bootstrap/server/src/Make.rules b/l4re/src/l4/pkg/bootstrap/server/src/Make.rules
   index 6f3b496e..ecd53eec 100644
   --- a/l4re/src/l4/pkg/bootstrap/server/src/Make.rules
   +++ b/l4re/src/l4/pkg/bootstrap/server/src/Make.rules
   @@ -331,7 +331,7 @@ ifneq ($(RAM_SIZE_MB),)
    CPPFLAGS	+= -DRAM_SIZE_MB=$(RAM_SIZE_MB)
    endif
    
   -CXXFLAGS += -fno-rtti -fno-exceptions
   +CXXFLAGS += -fno-rtti -fno-exceptions -Wno-format-security
    CXXFLAGS += $(call checkcxx,-fno-threadsafe-statics)
    
    ifneq ($(BUILD_MOD_CMD),)
   ```

2. Follow the build instructions from the official [L4Re-website](https://l4re.org/built.html).

## License

Detailed licensing information can be found in the [LICENSE](LICENSE.md) file.
