# L4Re same-page merging

`<WIP>`

## Structure

- `l4re/`: directory for the replacable L4Re-snapshot.

- `spmm/`: L4Re-package that contains the same-page merging monitor.

- `conf/`: module configurations and `ned` launcher scripts for SPM examples.

## Installation/Update

1. Obtain the latest [L4Re-snapshot](https://l4re.org/download/snapshots), unpack it and place it into `l4re/`.

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

   Hint: On NixOS, the prefix of the cross compilation toolchain slightly differs from the one assumed in the snapshot.
   The flake provided in this repository automatically sets the `CROSS_COMPILE` environment variable correctly.
   In addition to that, be sure to configure the correct prefix when asked during `make setup`.

3. Create a `Makeconf.boot` for your local system by following the instructions in `l4re/src/l4/conf/Makeconf.boot.example`. 

   Important: In order for L4Re to find the SPM examples provided in this repository, append the following configuration to the end of your `Makeconf.boot`:
  
   ```make
   # SPM configuration
   SPM_CONFIG_DIR = <absolute/path/to/your/l4re-spm/repo>/conf
   MODULE_SEARCH_PATH += $(SPM_CONFIG_DIR) 
   MODULES_LIST += $(SPM_CONFIG_DIR)/spm_modules.list
   ```

## License

Detailed licensing information can be found in the [LICENSE](LICENSE.md) file.
