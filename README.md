# TabAUR

TabAUR is an AUR Helper that actually makes releases and it's written in C++, It supports downloading AUR Repositories with tarballs or git

### Compilation
Just run `make`.

If the compilation time is too slow, use `make -j$(nproc)` and it will use 100% of your CPU, making the compilation way faster.

### Cleaning
To clean, run `make clean`.

## Dependencies
TabAUR includes its own distribution of libcpr, but if that's not good enough for you, you can install it from the [AUR](https://aur.archlinux.org/packages/cpr), it will automatically detect it!

You can compile & install cpr this way:
```bash
make cpr && make
```

## Pull requests
If you want to make a PR that fixes/adds functionality, feel free to do so! we aren't picky :)

## TODO

- More insight for the user to decide whether they trust this package or not.
- Work on putting the whole "diff viewing" code in a function instead of repeating myself
