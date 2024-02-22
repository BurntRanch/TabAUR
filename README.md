# TabAUR

TabAUR is an AUR Helper written with C++, It supports downloading AUR Repositories only through git (for now)

## Compilation
Just run `make`, to clean, run `make clean`.

## Dependencies
This software relies on 3 open-source dependencies, which are:
```
rapidjson
libcpr
libgit2
```
rapidjson is pre-included in the source files, you need to download libcpr and libgit2 or compile [libcpr from source](https://github.com/libcpr/cpr) and [libgit2 from source](https://github.com/libgit2/libgit2).
