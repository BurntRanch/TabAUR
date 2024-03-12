# TabAUR

TabAUR is an AUR Helper that actually makes releases and it's written in C++, It supports downloading AUR Repositories with tarballs or git

### Compilation
Just run `make`. 

If the compilation time is too slow, use `make -j$(nproc)` and it will use 100% of your CPU, making the compilation way faster.

### Cleaning
To clean, run `make clean`.

## Dependencies
you need to install libgit2 and [cpr](https://github.com/libcpr/cpr).

For `libgit2` can be installed by this command
```
sudo pacman -S libgit2
```
For cpr,
TabAUR offers the choice to either use the [AUR version](https://aur.archlinux.org/packages/cpr) (recommended)

Or by compiling and installing from source with the following commands:
```bash
git submodule init
git submodule update --init --recursive
make cpr
make
```
**NOTE:** using the compiled from source version can lead to bugs or errors.
