
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

## What's still limited:
- Function call arguments: The parser now skips argument expressions instead of erroring, but the codegen doesn't emit proper argument setup 
- Function-like macros: Not expanded (leaked as raw identifiers with parenthesized args, but the parser skips them)
- Pointer arithmetic: Arrays are parsed ([]) but binary + with pointers to emit offsets is not implemented
- Code gen test: The global main addition from the emit_entry fix changed output: pre-existing difference
