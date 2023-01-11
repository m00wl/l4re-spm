# L4Re same-page merging

`spmm` is the same-page merging monitor for the L4Re operating system.
It provides an interface to supply memory to other L4Re components.
The memory that it administers is periodically scanned and memory pages with matching contents are merged together.

## Structure

```bash
.
├── conf    # module configurations and ned launcher scripts for same-page merging examples.
├── l4re    # location of the replacable L4Re-snapshot.
├── patch   # collection of patches for the L4Re-snapshot.
└── spmm    # L4Re-package that contains the same-page merging monitor.
```

## Installation/Update

1. Obtain the latest [L4Re-snapshot](https://l4re.org/download/snapshots), unpack it and place it into `l4re/`.

2. Apply the following patches:

   Mandatory: These patches are required for same-page merging (increase kernel memory, tweak `uvmm`, etc.).

   ```bash
   git apply patch/req.patch
   ```

   Optional: These patches tweak the snapshot for usage under NixOS: 
   
   ```bash
   git apply patch/nixos.patch
   ```

3. Follow the build instructions from the official [L4Re-website](https://l4re.org/build.html).

   Hint: On NixOS, the prefix of the cross compilation toolchain is: `aarch64-unknown-linux-gnu-`.
   This is different from the one assumed in the snapshot.
   The flake provided in this repository automatically sets the `CROSS_COMPILE` environment variable correctly.
   In addition to that, be sure to configure the correct prefix when asked during `make setup`.

   Important: Don't forget to build the external packages for same-page merging as well.

4. Create a `Makeconf.boot` for your local system by following the instructions in `l4re/src/l4/conf/Makeconf.boot.example`. 

   Important: In order for L4Re to find the same-page merging examples provided in this repository, append the following configuration to the end of your `Makeconf.boot`:
  
   ```make
   # SPM configuration
   SPM_CONFIG_DIR = <absolute/path/to/your/l4re-spm/repo>/conf
   MODULE_SEARCH_PATH += $(SPM_CONFIG_DIR) 
   MODULES_LIST += $(SPM_CONFIG_DIR)/spm_modules.list
   ```

## License

Detailed licensing information can be found in the [LICENSE](LICENSE.md) file.
