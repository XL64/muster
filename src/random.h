#ifndef CLUSTER_RANDOM_H
#define CLUSTER_RANDOM_H
///
/// @file random.h
/// @brief Helper functions for taking random samples and seeding 
///        RNGs from the system clock.
///

#include <sys/time.h>

namespace cluster {

  ///
  /// This is Knuth's algorithm R for taking a sample of indices from
  /// 0 to numElements.  We sample size indices from this (the superset)
  /// and put them in the subset's mapping.
  ///
  /// @param numElements    total number of elements to select from
  /// @param sample_size    number of elements to select
  /// @param out            destination for selected elements 
  /// @param random         model of STL Random Number Generator.
  ///                       must be callable as random(N), returning a random number in [0,N).
  ///
  template <class OutputIterator, class Random>
  void random_subset(size_t numElements, size_t sample_size, OutputIterator out, Random& random) {
    size_t first = 0;
    size_t remaining = numElements;
    size_t m = sample_size;
  
    while (m > 0) {
      if ((size_t)random(remaining) < m) {
        *out = first;
        ++out;
        --m;
      }
      --remaining;
      ++first;
    }
  }


  ///
  /// Returns a reasonably distributed seed for random number generators.
  /// Based on the product of the seconds and usec in gettimeofday().
  ///
  inline long get_time_seed() {
    struct timeval time;
    gettimeofday(&time, 0);
    return time.tv_sec * time.tv_usec;
  }

} // namespace cluster

#endif // CLUSTER_RANDOM_H
