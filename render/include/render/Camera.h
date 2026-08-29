#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace render {

// An orbit (turntable) camera: always looks at a target point and moves on a
// sphere around it, described by two angles and a radius.
//
// Chosen over a free-flying FPS camera because the target can't drift
// off-screen, which is the usual failure mode in a mostly-empty scene.
class Camera {
 public:
  // Drag with the left mouse button. Pixel deltas in, rotation out.
  void orbit(float dx_pixels, float dy_pixels);
  // Scroll wheel. Multiplicative so zoom feels the same at every distance.
  void zoom(float scroll_ticks);
  // Drag with the middle/right button to slide the target point.
  void pan(float dx_pixels, float dy_pixels);

  glm::mat4 viewMatrix() const;
  glm::mat4 projectionMatrix(float aspect) const;

  glm::vec3 position() const;
  glm::vec3 target() const { return target_; }
  float distance() const { return distance_; }

  void setTarget(const glm::vec3& t) { target_ = t; }
  void setDistance(float d);

 private:
  glm::vec3 target_{0.0f};
  float distance_ = 3.0f;   // AU
  float yaw_ = -0.6f;       // radians, around +y
  float pitch_ = 0.5f;      // radians, from the xz-plane

  float fov_ = 45.0f;       // degrees
  float near_ = 0.01f;
  float far_ = 1000.0f;

  static constexpr float kOrbitSpeed = 0.006f;
  static constexpr float kPanSpeed = 0.0015f;
  static constexpr float kZoomSpeed = 0.12f;
};

}  // namespace render
