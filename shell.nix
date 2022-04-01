{ pkgs ? import <nixpkgs> {} }:

with pkgs;

let
  freetype-minimal = freetype.overrideAttrs (o: {
    propagatedBuildInputs = [];
    configureFlags = o.configureFlags ++ ["--with-zlib=no"
                                          "--with-bzip2=no"
                                          "--with-png=no"
                                          "--with-harfbuzz=no" ];
  });

  ftarm = (freetype-minimal.overrideAttrs (o: {
    CFLAGS = "--specs=nosys.specs -mthumb-interwork";
  })).override {
    stdenv = pkgsCross.arm-embedded.stdenv;
  };


in

pkgs.mkShell {
  buildInputs = [
    autoconf
    automake
    cmake
    #ftarm
    gcc-arm-embedded
    gdb
    libftdi
    libtool
    libusb
    picotool
    pkgconfig
    texinfo
    ];
}
