# PerfEvent
### A header-only C++ wrapper for Linux' perf event API

### Configuration

You may need to run `sudo sysctl -w kernel.perf_event_paranoid=-1`. Add `kernel.perf_event_paranoid = -1` to `/etc/sysctl.conf` to make the setting survive a reboot. To get L3 cache misses on recent AMD CPUs, load the amd_uncore module.

### Basic Usage:

```c++
#include "PerfEvent.hpp"
...
PerfEvent e;
e.startCounters();
yourBenchmark();
e.stopCounters();
e.printReport(std::cout, n); // use n as scale factor
```

This prints something like this:
```
cycles, instructions, L1-misses, LLC-misses, branch-misses, task-clock,    scale, IPC, CPUs,  GHz
 10.97,     28.01,      0.22,       0.00,          0.00,       3.89, 10000000, 2.55, 1.00, 2.82
```
### Usage of PerfEventBlock (convenience wrapper):

```c++
#include "PerfEvent.hpp"

PerfEvent e;

int main() {
  for (int threads=1;threads<maxThreads;++threads) {
    // Add number of threads to output
    e.setParam("threads", threads);
    // Only print the header for the first iteration
    e.printHeader = (threads==1);

    PerfEventBlock b(e, n); // Counters are started in constructor
    yourBenchmark(threads);
    // Benchmark counters are automatically stopped and printed on destruction of e
  }
}
```

This prints something like this:
```csv
threads, time sec,      cycles, instructions, L1-misses, LLC-misses, branch-misses, task-clock,   scale,      IPC,     CPUs,      GHz
      1, 1.400645, 1075.520519,  1931.465504,  8.888315,   0.070063,      0.121389, 280.115649, 5000000, 1.795843, 0.999952, 3.839559
      2, 1.133364, 2386.772941,  2062.313141, 32.095011,   0.043248,      0.918986, 650.737357, 5000000, 0.864059, 1.870823, 3.667798
...
```
