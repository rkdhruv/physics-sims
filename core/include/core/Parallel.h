#pragma once

#include <cstddef>
#include <functional>

namespace core {

// Number of workers `threads = 0` resolves to.
unsigned hardwareThreads();

// Splits [0, count) into contiguous chunks and runs `work(begin, end)` on each,
// in parallel.
//
// threads = 0 uses hardwareThreads(), 1 runs inline. Below kParallelThreshold
// items it runs inline regardless: spawning a thread costs tens of
// microseconds, which is longer than a small range takes to process.
//
// Chunks are contiguous and disjoint, so callers writing to `out[i]` for i in
// their own range need no synchronisation.
void parallelFor(std::size_t count, unsigned threads,
                 const std::function<void(std::size_t begin, std::size_t end)>& work);

constexpr std::size_t kParallelThreshold = 512;

}  // namespace core
