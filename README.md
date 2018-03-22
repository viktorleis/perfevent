PerfEvent: A header-only C++ wrapper for Linux' perf event API

Usage:

```
#include "PerfEvent.hpp"
#include <iostream>
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
