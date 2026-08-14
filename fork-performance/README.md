# Node.js fork/exec performance test

This benchmark compares the end-to-end latency of two ways to launch a fresh
Node.js process:

- `child_process.fork()` starts a Node.js child with an IPC channel.
- `child_process.exec()` starts the same child through the platform's shell,
  without an IPC channel.

Both operations run the same minimal `child.js` program, so the measured
difference is not affected by different child workloads.

Run it with:

```sh
node benchmark.js
```

The number of measured and warm-up runs can be changed from the command line:

```sh
node benchmark.js --iterations=100 --warmup=10
```

Each child is launched only after the preceding child exits. This avoids
measuring contention between concurrently starting processes. The reported
duration includes process creation, Node.js startup, and process teardown.
