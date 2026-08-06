
## Requirements

- [Meson](https://mesonbuild.com/)
- [C11 Compiler](https://en.wikipedia.org/wiki/C11_(C_standard_revision))

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

```sh
./build/c <source_file>
```
