# Run a Taint Analysis

This example shows how to run the IFDS taint analysis tool on a small C program
and how to customize sources and sinks.

## Files

- `taint.c` - Source program with a tainted flow and a sanitized flow

## Build the bitcode

```bash
clang -emit-llvm -c -g taint.c -o taint.bc
```

## Run the default taint analysis

```bash
./build/bin/lotus-taint -verbose taint.bc
```

## Run with custom sources and sinks

```bash
./build/bin/lotus-taint \
  -sources="scanf,gets,read" \
  -sinks="system,exec,popen" \
  taint.bc
```

## Expected output (abridged)

```text
[Taint Flow Detected]
Source: scanf (line 24)
Sink: system (line 7) via execute_command
Path: user_input -> execute_command(user_input) -> system(cmd)
```
