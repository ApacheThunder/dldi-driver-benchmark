# nds-sd-benchmark

Simple filesystem I/O benchmark for NDS homebrew.

# Changes from original build

* Compiled with nds329 for 32KB DLDI support.
* Better font for console text.
* Top screen initialized so it doesn't remain white.
* nitroFS as source for pad.bin temporarily disabled. pad.bin is expected to be on root of flashcart's file system now. This is due to bug in write code that ends up overwriting the NDS file!
* BlocksDS makefile hasn't been updated to support the new font file. Someone else should look into this as I'm unfamiliar with that make system right now.
* Random Write function appears to be broken. (hangs on first 0.5MiB write test). This bug existed in original build...I do not know why it's happening right now so remains unfixed.
* ITCM_CODE flag used for read/write functions...may improve performance a bit?

## Building

    $ ./generate_padding.sh
    $ make -f Makefile.blocks # or Makefile.dkp

## License

MIT
