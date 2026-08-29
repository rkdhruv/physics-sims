#include "render/Window.h"

#include <cstdio>
#include <stdexcept>

#include <glad/gl.h>
// glad must come before glfw3.h, or GLFW pulls in the system GL headers and the
// declarations collide. Don't let a formatter reorder these.
#include <GLFW/glfw3.h>

namespace render {
namespace {

int g_window_count = 0;

void errorCallback(int code, const char* description) {
  std::fprintf(stderr, "[glfw] error %d: %s\n", code, description);
}

}  // namespace

Window::Window(int width, int height, const std::string& title) {
  if (g_window_count == 0) {
    glfwSetErrorCallback(errorCallback);
    if (!glfwInit()) throw std::runtime_error("glfwInit failed");
  }

  // The forward-compat hint is required on macOS; without it the context comes
  // back as 2.1 legacy and modern GL calls silently do nothing.
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
  glfwWindowHint(GLFW_SAMPLES, 4);  // 4x MSAA -- orbit lines are thin

  window_ = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
  if (!window_) {
    if (g_window_count == 0) glfwTerminate();
    throw std::runtime_error("failed to create window (is a display attached?)");
  }
  ++g_window_count;

  glfwMakeContextCurrent(window_);
  glfwSwapInterval(1);  // vsync

  if (!gladLoadGL(glfwGetProcAddress)) {
    throw std::runtime_error("failed to load OpenGL functions");
  }

  glfwSetWindowUserPointer(window_, this);
  glfwSetScrollCallback(window_, scrollCallback);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_MULTISAMPLE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  std::printf("OpenGL %s | %s\n", glGetString(GL_VERSION), glGetString(GL_RENDERER));
  std::fflush(stdout);  // so this survives if the process is killed mid-run
}

Window::~Window() {
  if (window_) {
    glfwDestroyWindow(window_);
    if (--g_window_count == 0) glfwTerminate();
  }
}

bool Window::shouldClose() const { return glfwWindowShouldClose(window_); }

void Window::close() { glfwSetWindowShouldClose(window_, GLFW_TRUE); }

void Window::beginFrame() {
  glfwPollEvents();

  double x = 0.0, y = 0.0;
  glfwGetCursorPos(window_, &x, &y);
  const glm::vec2 position(static_cast<float>(x), static_cast<float>(y));

  // Guarded, or the first frame reports a delta measured from (0,0) and snaps
  // the camera when the cursor enters the window.
  mouse_delta_ = first_mouse_ ? glm::vec2(0.0f) : position - mouse_position_;
  mouse_position_ = position;
  first_mouse_ = false;

  const glm::ivec2 size = framebufferSize();
  glViewport(0, 0, size.x, size.y);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Window::endFrame() {
  glfwSwapBuffers(window_);
  scroll_delta_ = 0.0f;  // consumed; the callback refills it
}

void Window::setTitle(const std::string& title) {
  glfwSetWindowTitle(window_, title.c_str());
}

glm::ivec2 Window::framebufferSize() const {
  int w = 0, h = 0;
  glfwGetFramebufferSize(window_, &w, &h);
  return {w, h};
}

float Window::aspect() const {
  const glm::ivec2 size = framebufferSize();
  return size.y == 0 ? 1.0f : static_cast<float>(size.x) / static_cast<float>(size.y);
}

bool Window::keyDown(int key) const {
  return glfwGetKey(window_, key) == GLFW_PRESS;
}

bool Window::keyPressed(int key) {
  if (key < 0 || key >= 512) return false;
  const bool down = keyDown(key);
  const bool edge = down && !prev_keys_[key];
  prev_keys_[key] = down;
  return edge;
}

bool Window::mouseDown(int button) const {
  return glfwGetMouseButton(window_, button) == GLFW_PRESS;
}

double Window::time() const { return glfwGetTime(); }

void Window::scrollCallback(GLFWwindow* w, double, double yoff) {
  auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
  if (self) self->scroll_delta_ += static_cast<float>(yoff);
}

}  // namespace render
