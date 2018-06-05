PerfEvent: A header-only C++ wrapper for Linux' perf event API

Usage:

```
#include "PerfEvent.hpp"
...
PerfEvent e;
e.startCounters();
for (int i=0; i<n; i++) // this code will be measured
  ...
e.stopCounters();
e.printReport(std::cout, n); // use n as scale factor
std::cout << std::endl;
```

This prints something like this:
```
10.97 cycles, 28.01 instructions, 0.22 L1-misses, 0.00 LLC-misses, 0.00 branch-misses, 3.89 task-clock, 10000000 scale, 2.55 IPC, 1.00 CPUs, 2.82 GHz
```

Usage of convenience wrapper:
```
#include "PerfEvent.hpp"
...
// Define some global params
BenchmarkParameters params;
params.setParam("name","Dummy Benchmark");
params.setParam("dataSize","100 GB");

for (int threads=1;threads<maxThreads;++threads) {
  // Change local variables like num threads
  params.setParam("threads",numThreads);
  // Only print the header for the first iteration
  bool printHeader=numThreads==1;
  PerfEventBlock e(n,params,printHeader);
  // Coutner are started in constructor

  yourBenchmark();

  // Benchmark counters are automatically printed on destructor of e
}
```

This prints something like this:
```
           name,  dataSize, threads,  time sec,       cycles,  instructions,  L1-misses,  LLC-misses,  branch-misses,  task-clock,    scale,       IPC,      CPUs,       GHz
Dummy Benchmark,    100 GB,       1,  1.400645,  1075.520519,   1931.465504,   8.888315,    0.070063,       0.121389,  280.115649,  5000000,  1.795843,  0.999952,  3.839559
Dummy Benchmark,    100 GB,       2,  1.133364,  2386.772941,   2062.313141,  32.095011,    0.043248,       0.918986,  650.737357,  5000000,  0.864059,  2.870823,  3.667798
...
```
