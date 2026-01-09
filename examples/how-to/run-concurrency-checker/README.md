# Run a Concurrency Checker

This example shows how to detect data races in a small multithreaded program.

## Files

- `concurrent.c` - Uses two threads, one of which updates shared state without a lock

## Build the bitcode

```bash
clang -emit-llvm -c -g -pthread concurrent.c -o concurrent.bc
```

## Run the concurrency checker

```bash
./build/bin/lotus-concur concurrent.bc
```

## Run specific checks (optional)

```bash
./build/bin/lotus-concur --check-data-races concurrent.bc
./build/bin/lotus-concur --check-deadlocks --check-atomicity concurrent.bc
```

## Emit a machine-readable report (optional)

```bash
./build/bin/lotus-concur --report-json=report.json concurrent.bc
```

## Expected output (abridged)

```text
[Data Race Detected]
Variable: shared_counter
Thread 1: increment_thread (line 10) - PROTECTED by lock
Thread 2: buggy_thread (line 17) - UNPROTECTED
```
