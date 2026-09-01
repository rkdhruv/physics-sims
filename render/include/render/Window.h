#pragma once

#include <string>

#include <glm/vec2.hpp>

struct GLFWwindow;

namespace render {

// A GLFW window with an OpenGL 4.1 core context, plus the input state a scene
// needs to drive a camera.
//
// 4.1 is the newest core profile macOS supports; the version hints in the
// constructor are the only thing to change for a higher version elsewhere.
//
// Owns the window handle, so copying is disabled.
class Window {
 public:
  Window(int width, int height, const std::string& title);
  ~Window();

  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;

  bool shouldClose() const;
  void close();

  // Poll input, update the mouse/scroll deltas, and clear the framebuffer.
  void beginFrame();
  // Present the frame.
  void endFrame();

  void setTitle(const std::string& title);

  glm::ivec2 framebufferSize() const;
  float aspect() const;

  // --- input ---------------------------------------------------------------
  // Held down right now.
  bool keyDown(int key) const;
  // True only on the frame the key went down. For toggles, where keyDown()
  // would flip the state every frame the key is held.
  bool keyPressed(int key);

  bool mouseDown(int button) const;
  glm::vec2 mouseDelta() const { return mouse_delta_; }
  float scrollDelta() const { return scroll_delta_; }

  double time() const;

  // Write the framebuffer to a binary PPM. Chosen over PNG so the renderer
  // needs no image library; convert with Pillow (see BUILDING.md).
  bool saveScreenshot(const std::string& path) const;

 private:
  GLFWwindow* window_ = nullptr;

  glm::vec2 mouse_position_{0.0f};
  glm::vec2 mouse_delta_{0.0f};
  float scroll_delta_ = 0.0f;
  bool first_mouse_ = true;

  // Previous-frame key state, for keyPressed()'s edge detection.
  mutable bool prev_keys_[512] = {};

  static void scrollCallback(GLFWwindow* w, double xoff, double yoff);
};

}  // namespace render
