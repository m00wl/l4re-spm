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
    };
  }
  );

  nixConfig = {
    bash-prompt-suffix = "dev: ";
  };
}
