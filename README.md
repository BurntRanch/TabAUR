# TabAUR

TabAUR is an AUR Helper that actually makes releases and it's written with C++, It supports downloading AUR Repositories only through git (for now)

## Compilation
Just run `make`. 

If the compilation time is too slow, use `make -j$(nproc)` and it will use 100% of your CPU, making the compilation way faster.
### Cleaning
To clean, run `make clean`.

## Dependencies
you need to install some library dependencies, which you can by this command
```
sudo pacman -S libgit2 libcpr 
```

rapidjson is pre-included in the source files and doesn't need to be installed 
