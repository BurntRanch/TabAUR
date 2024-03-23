# TabAUR

TabAUR is an AUR Helper that actually makes releases and it's written in C++, It supports downloading AUR Repositories with tarballs or git

### Compilation
Just run `make`.

If the compilation time is too slow, use `make -j$(nproc)` and it will use 100% of your CPU, making the compilation way faster.

### Cleaning
To clean, run `make clean`.

## Dependencies
you need to install [cpr](https://github.com/libcpr/cpr)
TabAUR offers the choice to either use the [AUR version](https://aur.archlinux.org/packages/cpr) (recommended)

Or by compiling and installing from here with the following command:
```bash
make cpr && make
```
