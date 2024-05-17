#!/bin/env bash

# Update compile_flags.txt once in a while

version="$(awk -F '= ' '/^VERSION/ {print $2}' Makefile | sed 's/\"//g')"
branch="$(awk -F '= ' '/^BRANCH/ {print $2}' Makefile | sed 's/\"//g')"

printf -- "-I./include\n" > compile_flags.txt
printf -- "-pedantic\n" >> compile_flags.txt
printf -- "-Wall\n" >> compile_flags.txt
printf -- "-std=c++20\n" >> compile_flags.txt
printf -- "-DENABLE_NLS=1\n" >> compile_flags.txt
printf -- "-DVERSION=\"%s\"\n" $version >> compile_flags.txt
printf -- "-DBRANCH=\"%s\"\n" $branch >> compile_flags.txt
printf -- "-DLOCALEDIR=\"/usr/share/locale\"\n" >> compile_flags.txt
