#pragma once

#include <array>
#include <cstddef>
#include <vector>

#include <glm/vec3.hpp>

#include "core/ForceModel.h"

namespace core {

// An octree cell. Children are indices into the tree's flat node array rather
// than pointers, so the whole tree is a couple of allocations instead of one
// per node.
struct OctreeNode {
  glm::dvec3 center{0.0};     // geometric centre of the cell
  double half_width = 0.0;    // half the cell's edge length

  glm::dvec3 com{0.0};        // centre of mass of everything below this node
  double mass = 0.0;          // total mass below this node

  std::array<int, 8> children{-1, -1, -1, -1, -1, -1, -1, -1};
  int body = -1;              // body index if this is a leaf holding one, else -1

  bool isLeaf() const { return children[0] < 0 && children[1] < 0 &&
                               children[2] < 0 && children[3] < 0 &&
                               children[4] < 0 && children[5] < 0 &&
                               children[6] < 0 && children[7] < 0; }
};

// A Barnes-Hut octree over a set of point masses. Rebuilt each step rather
// than repaired: bodies move far enough per step that rebuilding is cheaper.
class Octree {
 public:
  // Discard any previous tree and build one over these bodies.
  void build(const std::vector<glm::dvec3>& positions,
             const std::vector<double>& masses);

  // Acceleration at `position` from every body in the tree.
  //
  // `exclude` skips one body so it doesn't attract itself; -1 includes all.
  // `theta` is the opening angle: a cell counts as a single point mass when
  // its width over the distance to it falls below theta. theta = 0 never
  // approximates and reduces to exact pairwise summation.
  glm::dvec3 acceleration(const glm::dvec3& position, int exclude,
                          double theta, double G, double softening) const;

  const std::vector<OctreeNode>& nodes() const { return nodes_; }
  std::size_t nodeCount() const { return nodes_.size(); }

  // Depth of the deepest leaf, as a health check on the distribution.
  int depth() const;

 private:
  std::vector<OctreeNode> nodes_;
  std::vector<glm::dvec3> positions_;
  std::vector<double> masses_;
};

// Gravitation via a Barnes-Hut tree. Reproduces NBodyGravity exactly at
// theta = 0 and approximates more aggressively as theta grows; 0.5 is the
// conventional default.
class BarnesHutGravity : public ForceModel {
 public:
  BarnesHutGravity(double G, double theta = 0.5, double softening = 0.0);

  using ForceModel::accelerations;

  void accelerations(const std::vector<glm::dvec3>& positions,
                     const std::vector<double>& masses,
                     std::vector<glm::dvec3>& out) const override;

  double potentialEnergy(const std::vector<glm::dvec3>& positions,
                         const std::vector<double>& masses) const override;

  double theta() const { return theta_; }
  const Octree& tree() const { return tree_; }

 private:
  double G_;
  double theta_;
  double softening_;

  // Mutable because the tree is a cache rebuilt per call, not part of the
  // model's state.
  mutable Octree tree_;
};

}  // namespace core
