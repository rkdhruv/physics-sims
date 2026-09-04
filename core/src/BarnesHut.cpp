#include "core/BarnesHut.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <glm/geometric.hpp>

namespace core {

// Which octant of `node` contains `p`. Bit 0 is +x, 1 is +y, 2 is +z, so the
// result indexes straight into OctreeNode::children.
int octantFor(const OctreeNode& node, const glm::dvec3& p) {
  return (p.x > node.center.x ? 1 : 0) |
         (p.y > node.center.y ? 2 : 0) |
         (p.z > node.center.z ? 4 : 0);
}

// Centre and half-width of one octant of a parent cell.
void childBounds(const OctreeNode& parent, int octant,
                 glm::dvec3& center, double& half_width) {
  half_width = parent.half_width * 0.5;
  center = parent.center + glm::dvec3((octant & 1) ? half_width : -half_width,
                                      (octant & 2) ? half_width : -half_width,
                                      (octant & 4) ? half_width : -half_width);
}

void Octree::build(const std::vector<glm::dvec3>& positions,
                   const std::vector<double>& masses) {
  
  positions_ = positions;
  masses_ = masses;
  nodes_.clear();

  const std::size_t n = masses_.size();
  if (n == 0) return;

  glm::dvec3 lo = positions_[0];
  glm::dvec3 hi = positions_[0];
  for (std::size_t i = 1; i < n; ++i) {
    lo.x = std::min(lo.x, positions_[i].x);
    lo.y = std::min(lo.y, positions_[i].y);
    lo.z = std::min(lo.z, positions_[i].z);
    hi.x = std::max(hi.x, positions_[i].x);
    hi.y = std::max(hi.y, positions_[i].y);
    hi.z = std::max(hi.z, positions_[i].z);
  }

  const glm::dvec3 extent = hi - lo;
  double half_width = 0.5 * std::max({extent.x, extent.y, extent.z, 1e-12});
  half_width *= 1.0 + 1e-9;

  nodes_.reserve(2 * n);
  OctreeNode root;
  root.center = 0.5 * (lo + hi);
  root.half_width = half_width;
  nodes_.push_back(root);

  auto makeChild = [this](int parent, int octant) {
    glm::dvec3 c;
    double hw;
    childBounds(nodes_[parent], octant, c, hw);

    OctreeNode child;
    child.center = c;
    child.half_width = hw;
    nodes_.push_back(child);

    const int index = static_cast<int>(nodes_.size()) - 1;
    nodes_[parent].children[octant] = index;
    return index;
  };

  // Insert bodies, splitting a leaf when a second body lands in it.
  constexpr int kMaxDepth = 64;

  for (std::size_t b = 0; b < n; ++b) {
    const int body = static_cast<int>(b);
    int node = 0;
    int depth = 0;

    while (true) {
      if (nodes_[node].isLeaf()) {
        if (nodes_[node].body < 0) {
          nodes_[node].body = body;
          break;
        }
        if (depth >= kMaxDepth) {
          throw std::runtime_error("Octree::build: coincident bodies");
        }
        const int other = nodes_[node].body;
        nodes_[node].body = -1;

        const int oct = octantFor(nodes_[node], positions_[other]);
        const int child = makeChild(node, oct);
        nodes_[child].body = other;
      }

      const int oct = octantFor(nodes_[node], positions_[body]);
      int child = nodes_[node].children[oct];
      if (child < 0) child = makeChild(node, oct);
      node = child;
      ++depth;
    }
  }

  // Mass and centre of mass, bottom-up. A child is always pushed after its
  // parent, so its index is always higher -- iterating backwards visits every
  // child before the parent that needs it, with no recursion.
  for (int i = static_cast<int>(nodes_.size()) - 1; i >= 0; --i) {
    OctreeNode& node = nodes_[i];

    if (node.isLeaf()) {
      if (node.body >= 0) {
        node.mass = masses_[node.body];
        node.com = positions_[node.body];
      } else {
        node.mass = 0.0;
        node.com = node.center;
      }
      continue;
    }
    double m = 0.0;
    glm::dvec3 weighted(0.0);
    for (int c : node.children) {
      if (c < 0) continue;
      m += nodes_[c].mass;
      weighted += nodes_[c].mass * nodes_[c].com;
    }
  node.mass = m;
  node.com = (m > 0.0) ? weighted / m : node.center;
  }
}

glm::dvec3 Octree::acceleration(const glm::dvec3& position, int exclude,
                                double theta, double G, double softening) const {
  glm::dvec3 acc(0.0);
  if (nodes_.empty()) return acc;

  const double eps2 = softening * softening;

  auto addPoint = [&](const glm::dvec3& com, double m) {
    const glm::dvec3 d = com - position;
    const double dist2 = glm::dot(d, d) + eps2;
    if (dist2 <= 0.0) return;
    const double dist = std::sqrt(dist2);
    acc += G * m * d / (dist2 * dist);
  };

  std::vector<int> stack{0};
  while (!stack.empty()) {
    const int index = stack.back();
    stack.pop_back();
    const OctreeNode& node = nodes_[index];

    if (node.mass <= 0.0) continue;

    if (node.isLeaf()) {
      if (node.body >= 0 && node.body != exclude) {
        addPoint(positions_[node.body], masses_[node.body]);
      }
      continue;
    }

    const glm::dvec3 d = node.com - position;
    const double dist = std::sqrt(glm::dot(d, d) + eps2);
    const double width = 2.0 * node.half_width;

    if (dist > 0.0 && width / dist < theta) {
      addPoint(node.com, node.mass);
    } else {
      for (int c : node.children) {
        if (c >= 0) stack.push_back(c);
      }
    }
  }

  return acc;
}

int Octree::depth() const {
  if (nodes_.empty()) return 0;

  int deepest = 0;
  // Explicit stack rather than recursion: a degenerate distribution can nest
  // deep enough to overflow the call stack.
  std::vector<std::pair<int, int>> stack{{0, 1}};
  while (!stack.empty()) {
    const auto [index, d] = stack.back();
    stack.pop_back();
    deepest = std::max(deepest, d);
    for (int child : nodes_[index].children) {
      if (child >= 0) stack.push_back({child, d + 1});
    }
  }
  return deepest;
}

// ---------------------------------------------------------------------------

BarnesHutGravity::BarnesHutGravity(double G, double theta, double softening)
    : G_(G), theta_(theta), softening_(softening) {}

void BarnesHutGravity::accelerations(const std::vector<glm::dvec3>& positions,
                                     const std::vector<double>& masses,
                                     std::vector<glm::dvec3>& out) const {
  const std::size_t n = masses.size();
  out.assign(n, glm::dvec3(0.0));
  if (n == 0) return;

  tree_.build(positions, masses);

  for (std::size_t i = 0; i < n; ++i) {
    out[i] = tree_.acceleration(positions[i], static_cast<int>(i), theta_, G_,
                                softening_);
  }
}

double BarnesHutGravity::potentialEnergy(const std::vector<glm::dvec3>& positions,
                                         const std::vector<double>& masses) const {
  // Exact pairwise sum: this is a diagnostic evaluated occasionally rather
  // than every step, so O(n^2) is affordable and being exact makes it a
  // trustworthy check.
  const std::size_t n = masses.size();
  const double eps2 = softening_ * softening_;

  double pe = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    for (std::size_t j = i + 1; j < n; ++j) {
      const glm::dvec3 d = positions[i] - positions[j];
      pe -= G_ * masses[i] * masses[j] / std::sqrt(glm::dot(d, d) + eps2);
    }
  }
  return pe;
}

}  // namespace core
