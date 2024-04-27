# TabAUR

TabAUR is an AUR Helper that actually makes releases and it's written in C++20, It supports downloading AUR Repositories with tarballs or git

> [!CAUTION]
> #### TabAUR has decided to become public in **April 27 2024** since we agreeed that is almost in a good beta shape.\
> #### Expect bugs and some problems to happen (we believe that there won't be so so many though, it's great both installing and upgrading)\
> #### **Do not TabAUR for removing packages (taur -R)**

> [!NOTE]
> #### Be free to test and try out TabAUR.\
> #### Open issues for bug reports or feature requests.\
> #### Open Pull Requests for fixes and features addons.\
> #### Thanks for using/trying out TabAUR

## Compilation
Just run `make`.

If the compilation time is too slow, use `make -j$(nproc)` and it will use 100% of your CPU, making the compilation way faster.

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
