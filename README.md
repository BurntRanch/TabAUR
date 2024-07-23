[![tabaur](https://img.shields.io/aur/version/tabaur?color=1793d1&label=tabaur&logo=arch-linux&style=for-the-badge)](https://aur.archlinux.org/packages/tabaur/)
[![tabaur](https://img.shields.io/aur/version/tabaur-git?color=1793d1&label=tabaur-git&logo=arch-linux&style=for-the-badge)](https://aur.archlinux.org/packages/tabaur-git/)
[![tabaur](https://img.shields.io/aur/version/tabaur-bin?color=1793d1&label=tabaur-bin&logo=arch-linux&style=for-the-badge)](https://aur.archlinux.org/packages/tabaur-bin/)
[![customize](https://img.shields.io/badge/customize-your_colors-blue?color=5544ff&style=for-the-badge)](https://github.com/BurntRanch/TabAUR/tree/main/predefined-themes)\
[![forthebadge](https://forthebadge.com/images/badges/works-on-my-machine.svg)](https://forthebadge.com)

An AUR Helper with support for downloading AUR packages with either tarballs or git.\
TabAUR aims to be a fast, lightweight, capable, and safe front-end for pacman, that supports and even *helps* you with AUR stuff!

> [!NOTE]
> #### TabAUR has decided to become public in **April 27 2024** since we agreed that is almost in a good beta shape.
> #### Expect bugs and some problems to happen, though we believe that there won't be too many bugs since it was well tested for installing, upgrading and querying

## Some features:
- [Customizable colors](https://github.com/BurntRanch/TabAUR/tree/main/predefined-themes)
- Fast with near/same perfomances as pacman (for searching and querying)
- Doesn't leak memory when running -h, unlike 2 popular AUR helpers lol
- Support for using AUR packages' either tarballs or git repos
- Currently less than 2mb without -O2
- Upcoming useful features

# Installation
### Release (recommended)
```bash
git clone https://aur.archlinux.org/tabaur-bin.git && cd tabaur-bin
makepkg -si
```
### Release (compile from source)
```bash
git clone https://aur.archlinux.org/tabaur.git && cd tabaur
makepkg -si
```
### Development
```bash
git clone https://aur.archlinux.org/tabaur-git.git && cd tabaur-git
makepkg -si
```
### Development (manually)
```bash
git clone --depth=1 https://github.com/BurntRanch/TabAUR && cd TabAUR
make
sudo make install
```
Can your average C++/Rust/(any non-interpreted language) project compile that easy in many ways? I don't think so :P

# Dependencies
- pacman

"that's it?" yes. just pacman. and it's dependencies ofc, which includes curl

## Optional Dependencies
- sudo: privilege escalation
- opendoas: privilege escalation
- git: for using AUR packages git repos
- tar: for using AUR packages tarballs (.tar.gz)

# Pull requests
If you want to make a PR that fixes/adds functionality, even fix some comments typos, feel free to do so! :)\
**Be free to test and try out TabAUR**

## TODO

- Complete translations for ar_SA and (up to toni) it_IT
