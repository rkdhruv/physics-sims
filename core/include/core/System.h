#pragma once

#include <cstddef>
#include <vector>

#include <glm/vec3.hpp>

namespace core {

// Complete state of an N-body system.
//
// Struct-of-arrays rather than a vector<Body>: the force loop reads only
// positions, so this keeps them contiguous in cache, and it lets positions go
// to a GPU buffer as one memcpy.
//
// dvec3 (double) rather than vec3 -- float's ~7 significant digits aren't
// enough over millions of steps. The renderer casts to float at upload time.
struct System {
  std::vector<glm::dvec3> positions;   // AU
  std::vector<glm::dvec3> velocities;  // AU/yr
  std::vector<double> masses;          // solar masses

  std::size_t size() const { return masses.size(); }

  void resize(std::size_t n) {
    positions.resize(n);
    velocities.resize(n);
    masses.resize(n);
  }
};

}  // namespace core
