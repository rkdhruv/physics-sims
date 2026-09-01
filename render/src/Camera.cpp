#include "render/Camera.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

namespace render {
namespace {

constexpr float kPi = 3.14159265358979323846f;

}  // namespace

void Camera::orbit(float dx_pixels, float dy_pixels) {
  yaw_ += dx_pixels * kOrbitSpeed;
  pitch_ += dy_pixels * kOrbitSpeed;

  // Clamp just short of straight up/down: at exactly +/-90 degrees the forward
  // vector is parallel to world up, lookAt's cross product degenerates, and the
  // view matrix fills with NaN.
  const float limit = kPi * 0.5f - 0.01f;
  pitch_ = std::clamp(pitch_, -limit, limit);
}

void Camera::zoom(float scroll_ticks) {
  // Multiplicative so zoom feels the same at every distance. Additive would
  // crawl when far out and slam into the target up close.
  distance_ *= std::exp(-scroll_ticks * kZoomSpeed);
  setDistance(distance_);
}

void Camera::pan(float dx_pixels, float dy_pixels) {
  const glm::vec3 forward = glm::normalize(target_ - position());
  const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
  const glm::vec3 up = glm::cross(right, forward);

  // Scaled by distance so a drag moves the same amount of content at any zoom.
  const float scale = distance_ * kPanSpeed;
  target_ += (-right * dx_pixels + up * dy_pixels) * scale;
}

void Camera::setDistance(float d) {
  distance_ = std::clamp(d, min_distance_, max_distance_);
}

void Camera::setDistanceRange(float min_distance, float max_distance) {
  min_distance_ = min_distance;
  max_distance_ = max_distance;
  setDistance(distance_);
}

void Camera::setClipPlanes(float near_plane, float far_plane) {
  near_ = near_plane;
  far_ = far_plane;
}

glm::vec3 Camera::position() const {
  const float cos_pitch = std::cos(pitch_);
  return target_ + distance_ * glm::vec3(cos_pitch * std::sin(yaw_),
                                         std::sin(pitch_),
                                         cos_pitch * std::cos(yaw_));
}

glm::mat4 Camera::viewMatrix() const {
  return glm::lookAt(position(), target_, glm::vec3(0, 1, 0));
}

glm::mat4 Camera::projectionMatrix(float aspect) const {
  return glm::perspective(glm::radians(fov_), aspect, near_, far_);
}

}  // namespace render
