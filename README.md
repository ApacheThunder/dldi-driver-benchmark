# dldi-driver-benchmark

Simple filesystem I/O benchmark for NDS homebrew.

The benchmark expects an 8 MB file "benchmark_pad.bin" in the root of the CF/SD card.
This file will be created automatically if it doesn't exist, but you might want to create it manually
for read-only storage mediums.

# Changes from original build

* Compiled with nds329 for 32KB DLDI support.
* My build system doesn't like make file having dkp so currently there is a makefile with no extension provided. It is currently identical to the dkp file right now.
* Better font for console text. Associeted changes to make file to make this work. May need further tweeks if one wishes to use the "make -f Makefile.dkp" command.
* Top screen initialized so it doesn't remain white.
* BlocksDS makefile hasn't been updated to support the new font file. Someone else should look into this as I'm unfamiliar with that make system right now.

## Building

    $ make -f Makefile.blocks # or Makefile.dkp

## License

MIT
