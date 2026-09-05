#include "core/Parallel.h"

#include <algorithm>
#include <thread>
#include <vector>

namespace core {

unsigned hardwareThreads() {
  const unsigned n = std::thread::hardware_concurrency();
  return n > 0 ? n : 1;
}

void parallelFor(std::size_t count, unsigned threads,
                 const std::function<void(std::size_t, std::size_t)>& work) {
  if (count == 0) return;

  if (threads == 0) threads = hardwareThreads();
  threads = std::min<std::size_t>(threads, count);

  if (threads <= 1 || count < kParallelThreshold) {
    work(0, count);
    return;
  }

  const std::size_t chunk = count / threads;
  const std::size_t remainder = count % threads;

  // One fewer worker than chunks: the calling thread takes the last one rather
  // than idling in join(), which saves a spawn and a context switch.
  std::vector<std::thread> workers;
  workers.reserve(threads - 1);

  std::size_t begin = 0;
  for (unsigned t = 0; t < threads; ++t) {
    // Spread the remainder over the first chunks rather than piling it onto
    // the last one, which would leave one worker running long after the rest.
    const std::size_t end = begin + chunk + (t < remainder ? 1 : 0);
    if (t + 1 < threads) {
      workers.emplace_back(work, begin, end);
    } else {
      work(begin, end);
    }
    begin = end;
  }

  for (std::thread& worker : workers) worker.join();
}

}  // namespace core
