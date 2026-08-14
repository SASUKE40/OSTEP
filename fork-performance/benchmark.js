#!/usr/bin/env node

'use strict';

const { exec, fork } = require('node:child_process');
const path = require('node:path');
const { hrtime } = require('node:process');

const CHILD_FILE = path.join(__dirname, 'child.js');

function quoteShellArgument(value) {
  if (process.platform === 'win32') {
    return `"${value.replaceAll('"', '""')}"`;
  }

  return `'${value.replaceAll("'", "'\\''")}'`;
}

const EXEC_COMMAND = [process.execPath, CHILD_FILE]
  .map(quoteShellArgument)
  .join(' ');

function readPositiveInteger(value, name) {
  const parsed = Number(value);

  if (!Number.isSafeInteger(parsed) || parsed <= 0) {
    throw new Error(`${name} must be a positive integer; received "${value}"`);
  }

  return parsed;
}

function readOptions(argv) {
  const options = {
    iterations: 50,
    warmup: 5,
  };

  for (const argument of argv) {
    if (argument.startsWith('--iterations=')) {
      options.iterations = readPositiveInteger(
        argument.slice('--iterations='.length),
        'iterations',
      );
    } else if (argument.startsWith('--warmup=')) {
      options.warmup = readPositiveInteger(
        argument.slice('--warmup='.length),
        'warmup',
      );
    } else if (argument === '--help' || argument === '-h') {
      console.log(`Usage: node benchmark.js [options]

Compare the end-to-end cost of child_process.fork() and exec().

Options:
  --iterations=N  measured runs per operation (default: 50)
  --warmup=N       unmeasured warm-up runs per operation (default: 5)
  -h, --help       show this help`);
      process.exit(0);
    } else {
      throw new Error(`unknown option: ${argument}`);
    }
  }

  return options;
}

function elapsedMilliseconds(startedAt) {
  return Number(hrtime.bigint() - startedAt) / 1_000_000;
}

function waitForChild(startChild) {
  return new Promise((resolve, reject) => {
    const startedAt = hrtime.bigint();
    const child = startChild();

    child.once('error', reject);
    child.once('close', (code, signal) => {
      if (code !== 0) {
        reject(
          new Error(
            `child failed (${signal ? `signal ${signal}` : `exit code ${code}`})`,
          ),
        );
        return;
      }

      resolve(elapsedMilliseconds(startedAt));
    });
  });
}

function runFork() {
  return waitForChild(() =>
    fork(CHILD_FILE, [], {
      stdio: ['ignore', 'ignore', 'ignore', 'ipc'],
    }),
  );
}

function runExec() {
  return waitForChild(() =>
    exec(EXEC_COMMAND, {
      windowsHide: true,
    }),
  );
}

function percentile(sortedValues, percentage) {
  const index = Math.ceil((percentage / 100) * sortedValues.length) - 1;
  return sortedValues[Math.max(0, index)];
}

function summarize(samples) {
  const sorted = [...samples].sort((left, right) => left - right);
  const total = samples.reduce((sum, sample) => sum + sample, 0);

  return {
    mean: total / samples.length,
    median: percentile(sorted, 50),
    p95: percentile(sorted, 95),
    min: sorted[0],
    max: sorted[sorted.length - 1],
  };
}

function formatRow(name, summary) {
  const values = [
    name.padEnd(9),
    summary.mean.toFixed(2).padStart(10),
    summary.median.toFixed(2).padStart(10),
    summary.p95.toFixed(2).padStart(10),
    summary.min.toFixed(2).padStart(10),
    summary.max.toFixed(2).padStart(10),
  ];

  return values.join(' ');
}

async function main() {
  const { iterations, warmup } = readOptions(process.argv.slice(2));
  const benchmarks = [
    ['fork', runFork],
    ['exec', runExec],
  ];

  for (let run = 0; run < warmup; run += 1) {
    for (const [, benchmark] of benchmarks) {
      await benchmark();
    }
  }

  const samples = new Map(benchmarks.map(([name]) => [name, []]));

  // Alternate the order to reduce bias from CPU frequency and system load.
  for (let run = 0; run < iterations; run += 1) {
    const orderedBenchmarks = run % 2 === 0 ? benchmarks : [...benchmarks].reverse();

    for (const [name, benchmark] of orderedBenchmarks) {
      samples.get(name).push(await benchmark());
    }
  }

  console.log(`Node ${process.version} on ${process.platform}/${process.arch}`);
  console.log(`${iterations} measured runs after ${warmup} warm-up runs`);
  console.log('');
  console.log('operation    mean ms  median ms     p95 ms     min ms     max ms');
  console.log('--------- ---------- ---------- ---------- ---------- ----------');

  for (const [name] of benchmarks) {
    console.log(formatRow(name, summarize(samples.get(name))));
  }
}

main().catch((error) => {
  console.error(`benchmark failed: ${error.message}`);
  process.exitCode = 1;
});
