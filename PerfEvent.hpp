/*

Copyright (c) 2018 Viktor Leis

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 */

#pragma once

#if defined(__linux__)

#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <linux/perf_event.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <unistd.h>

struct PerfEvent {
   struct event {
      struct read_format {
         uint64_t value;
         uint64_t time_enabled;
         uint64_t time_running;
         uint64_t id;
      };

      std::string name;
      perf_event_attr pe;
      int fd;
      read_format prev;
      read_format data;

      double readCounter() {
         double multiplexingCorrection = static_cast<double>(data.time_enabled - prev.time_enabled) / (data.time_running - prev.time_running);
         return (data.value - prev.value) * multiplexingCorrection;
      }
   };

   std::vector<event> events;
   std::chrono::time_point<std::chrono::steady_clock> startTime;
   std::chrono::time_point<std::chrono::steady_clock> stopTime;
   std::vector<std::string> extraParamsKeys;
   std::vector<std::string> extraParamsValues;
   bool printHeader;

   PerfEvent() : printHeader(true) {
      // additional counters can be found in linux/perf_event.h
      registerCounter("cycle", PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES);
      registerCounter("kcycle", PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES, true); // cycles in kernel
      registerCounter("instr", PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS);
      registerCounter("L1-miss", PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_L1D|(PERF_COUNT_HW_CACHE_OP_READ<<8)|(PERF_COUNT_HW_CACHE_RESULT_MISS<<16));
      //registerCounter("c-miss", PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES); // L2 misses on most recent CPUs
      registerCounter("br-miss", PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_MISSES);
      registerCounter("task", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_TASK_CLOCK);

      for (auto& event: events)
         event.fd = syscall(__NR_perf_event_open, &event.pe, 0, -1, -1, 0);

      // AMD uncore counters: https://git.zx2c4.com/linux-rng/commit/tools?id=5b2ca349c313a0e03162e353d898c4f7046c7898
      std::vector<int> type = readFile("/sys/bus/event_source/devices/amd_l3/type");
      if (type.size()==1) {
         for (int cpuMask : readFile("/sys/bus/event_source/devices/amd_l3/cpumask")) {
            uint64_t eventID = 0x104; // L3 cache miss on AMD Zen
            registerCounter("L3-miss" + std::to_string(cpuMask), type[0], eventID);
            auto& event = events.back();
            event.fd = syscall(__NR_perf_event_open, &event.pe, -1, cpuMask, -1, 0);
         }
      }

      // Check for errors
      for (auto& event: events) {
         if (event.fd < 0) {
            std::cerr << "Error opening counter " << event.name << std::endl;
            for (auto& event: events)
               if (event.fd >= 0)
                  close(event.fd);
            events.resize(0);
            return;
         }
      }
   }

   static std::vector<int> readFile(const std::string& filename) {
      std::vector<int> result;
      std::ifstream file(filename);
      if (!file.is_open())
         return result;

      std::string line;
      if (std::getline(file, line)) {
         std::stringstream ss(line);
         std::string token;
         while (std::getline(ss, token, ',')) {
            try {
               result.push_back(std::stoi(token));
            } catch (...) {}
         }
      }
      return result;
   }

   void registerCounter(const std::string& name, uint64_t type, uint64_t eventID, bool exclude_user=false) {
      events.emplace_back();
      auto& event = events.back();
      event.fd = -1;
      event.name = name;
      auto& pe = event.pe;
      memset(&pe, 0, sizeof(struct perf_event_attr));
      pe.type = type;
      pe.size = sizeof(struct perf_event_attr);
      pe.config = eventID;
      pe.disabled = true;
      pe.inherit = true;
      pe.inherit_stat = false;
      pe.exclude_user = exclude_user;
      pe.exclude_kernel = false;
      pe.exclude_hv = false;
      pe.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING | PERF_FORMAT_ID;
   }

   void startCounters() {
      for (auto& event: events) {
         ioctl(event.fd, PERF_EVENT_IOC_RESET, 0);
         ioctl(event.fd, PERF_EVENT_IOC_ENABLE, 0);
         if (read(event.fd, &event.prev, sizeof(PerfEvent::event::read_format)) != sizeof(PerfEvent::event::read_format))
            std::cerr << "Error reading counter " << event.name << std::endl;
      }
      startTime = std::chrono::steady_clock::now();
   }

   ~PerfEvent() {
      for (auto& event : events) {
         close(event.fd);
      }
   }

   void stopCounters() {
      stopTime = std::chrono::steady_clock::now();
      for (auto& event: events) {
         if (read(event.fd, &event.data, sizeof(PerfEvent::event::read_format)) != sizeof(PerfEvent::event::read_format))
            std::cerr << "Error reading counter " << event.name << std::endl;
         ioctl(event.fd, PERF_EVENT_IOC_DISABLE, 0);
      }
   }

   double getDuration() {
      return std::chrono::duration<double>(stopTime - startTime).count();
   }

   double getIPC() {
      return getCounter("instr") / getCounter("cycle");
   }

   double getCPUs() {
      return getCounter("task") / (getDuration() * 1e9);
   }

   double getGHz() {
      return getCounter("cycle") / getCounter("task");
   }

   double getCounter(const std::string& name) {
      for (auto& event: events)
         if (event.name==name)
            return event.readCounter();
      return -1;
   }

   static void printCounter(std::ostream& headerOut, std::ostream& dataOut, std::string name, std::string counterValue, bool addComma=true) {
      unsigned width = name.length();
      if (counterValue.length() > width)
         width = counterValue.length();
      headerOut << std::setw(width) << name << (addComma ? "," : "") << " ";
      dataOut << std::setw(width) << counterValue << (addComma ? "," : "") << " ";
   }

   static void printCounter(std::ostream& headerOut, std::ostream& dataOut, std::string name, double counterValue, bool addComma=true) {
      std::stringstream stream;
      stream << std::fixed << std::setprecision(counterValue >= 100 ? 0 : 2) << counterValue;
      PerfEvent::printCounter(headerOut,dataOut,name,stream.str(),addComma);
   }

   void printReport(std::ostream& out, uint64_t scale) {
      std::stringstream header;
      std::stringstream data;
      printReport(header,data,scale);
      out << header.str() << std::endl;
      out << data.str() << std::endl;
   }

   void printReport(std::ostream& headerOut, std::ostream& dataOut, uint64_t scale) {
      if (events.empty())
         return;

      // print all metrics
      for (auto& event: events)
         if (event.name != "task") // getCPUs() is enough
            printCounter(headerOut, dataOut, event.name, event.readCounter()/scale);

      printCounter(headerOut, dataOut, "scale", scale);

      // derived metrics
      printCounter(headerOut, dataOut, "IPC", getIPC());
      printCounter(headerOut, dataOut, "CPU", getCPUs());
      printCounter(headerOut, dataOut, "GHz", getGHz(), false);
   }

   void setParam(const std::string& name, const std::string& value) {
      for (unsigned i=0; i<extraParamsKeys.size(); i++) {
         if (extraParamsKeys[i] == name) {
            extraParamsValues[i] = value;
            return;
         }
      }
      extraParamsKeys.push_back(name);
      extraParamsValues.push_back(value);
   }

   void setParam(const std::string& name, const char* value) {
      setParam(name, std::string(value));
   }

   template <typename T>
   void setParam(const std::string& name, T value) {
      setParam(name,std::to_string(value));
   }

   void printParams(std::ostream& header, std::ostream& data) {
      for (unsigned i=0; i<extraParamsKeys.size(); i++) {
         printCounter(header, data, extraParamsKeys[i], extraParamsValues[i]);
      }
   }
};

struct PerfEventBlock {
   PerfEvent& e;
   uint64_t scale;

   PerfEventBlock(PerfEvent& e, uint64_t scale = 1) : e(e), scale(scale) {
      e.startCounters();
   }

   ~PerfEventBlock() {
      e.stopCounters();
      std::stringstream header;
      std::stringstream data;
      e.printParams(header,data);
      PerfEvent::printCounter(header,data,"time",e.getDuration());
      e.printReport(header, data, scale);
      if (e.printHeader) {
         std::cout << header.str() << std::endl;
         e.printHeader = false;
      }
      std::cout << data.str() << std::endl;
   }
};

#else
#include <ostream>
#include <cstdint>

struct PerfEvent {
   void startCounters() {}
   void stopCounters() {}
   void printReport(std::ostream&, uint64_t) {}
};

struct PerfEventBlock {
   PerfEventBlock(PerfEvent& e, uint64_t scale = 1) {}
};
#endif
