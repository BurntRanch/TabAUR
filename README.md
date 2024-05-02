![arch](https://img.shields.io/badge/Arch_Linux-10A0CC?style=for-the-badge&logo=arch-linux&logoColor=white)
[![tabaur](https://img.shields.io/aur/version/tabaur?color=1793d1&label=tabaur&logo=arch-linux&style=for-the-badge)](https://aur.archlinux.org/packages/tabaur/)
[![customize](https://img.shields.io/badge/customize-your_colors-blue?color=5544ff&style=for-the-badge)](https://github.com/BurntRanch/TabAUR/tree/libalpm-test/predefined-themes)\
[![forthebadge](https://forthebadge.com/images/badges/works-on-my-machine.svg)](https://forthebadge.com)

# TabAUR
An AUR Helper that has support for downloading AUR Repositories with tarballs or git. Also it's fast as pacman

> [!NOTE]
> #### TabAUR has decided to become public in **April 27 2024** since we agreeed that is almost in a good beta shape.
> #### Expect bugs and some problems to happen, though we believe that there won't be so so many because it's great for both installing and upgrading

## Some features:
- Customizable colors
- Fast with near/same perfomances as pacman
- Doesn't leak memory when running -h, unlike 2 popular AUR helpers lol
- Support for using AUR repos' either tarballs or git repos
- Currently less than 2mb without -O2
- Upcoming useful features

# Installation
### Release
```bash
git clone https://aur.archlinux.org/tabaur-bin.git && cd tabaur-bin
makepkg -si
```
### Development
```bash
git clone https://aur.archlinux.org/tabaur-git.git && cd tabaur-git
makepkg -si
```

# Dependencies
TabAUR includes its own distribution of libcpr, but if that's not good enough for you, you can install it from the [AUR](https://aur.archlinux.org/packages/cpr), it will automatically detect it!

You can compile cpr this way:
```bash
# note that it won't work if cpr is already installed
make cpr
```

# Compilation
Just run `make`.

If the compilation time is too slow, use `make -j$(nproc)` and it will use 100% of your CPU, making the compilation way faster.

## Pull requests
If you want to make a PR that fixes/adds functionality, even fix some comments typos, feel free to do so! :)\
**Be free to test and try out TabAUR**

## TODO

- More insight for the user to decide whether they trust this package or not.
- Work on putting the whole "diff viewing" code in a function instead of repeating myself
