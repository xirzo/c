
## Build

```sh
meson setup build --buildtype=debug
```

```sh
meson compile -C build
```

## Run tests

```sh
ninja -C build test
```

## Run compiler

For now there is no support for reading external source files.

```sh
./build/c 
```

```sh
nasm -f elf64 c.asm -o c.o
```

```sh
ld c.o -o c
```

```
./c
```
