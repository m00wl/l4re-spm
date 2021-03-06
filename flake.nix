{
  description = "l4re-spm flake";

  inputs = {
    #nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
    nixpkgs.url = "github:nixos/nixpkgs/release-21.11";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
  flake-utils.lib.eachDefaultSystem (system:
  let
    pkgs = import nixpkgs {
      inherit system;
    };
    pkgsArm64 = pkgs.pkgsCross.aarch64-multiplatform;
  in
  {
    #devShell = pkgsArm64.mkShell.override { stdenv = pkgsArm64.gcc10Stdenv; } {
    devShell = pkgsArm64.mkShell {
      CROSS_COMPILE = "aarch64-unknown-linux-gnu-";
      nativeBuildInputs = [
        # build
        pkgs.gcc
        pkgs.dialog
        pkgs.gawk
        pkgs.binutils
        pkgs.pkg-config
        pkgs.flex
        pkgs.bison
        pkgs.dtc
        pkgs.ubootTools

        # run
        pkgs.qemu
      ];
      shellHook = ''
        export L4RE_SPM_ROOT=$(pwd)
        alias ms="make -C $L4RE_SPM_ROOT/spmm";
        alias mq="make -C $L4RE_SPM_ROOT/l4re/obj/l4/arm64/ qemu PLATFORM_TYPE=arm_virt";
      '';
    };
  }
  );

  nixConfig = {
    bash-prompt-suffix = "dev: ";
  };
}
