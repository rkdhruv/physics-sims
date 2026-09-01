// Orbital mechanics scene -- the first thing that puts the engine on screen.
//
// Controls:
//   left drag      orbit the camera        scroll    zoom
//   right drag     pan                     space     pause
//   1 / 2 / 3      Euler / Verlet / RK4    C         clear trails
//   [ / ]          slow down / speed up    R         reload shaders
//   P              screenshot              esc       quit

#include <cstdio>
#include <deque>
#include <stdexcept>
#include <string>
#include <vector>

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include "core/Diagnostics.h"
#include "core/ForceModel.h"
#include "core/Integrator.h"
#include "core/Scenarios.h"
#include "core/System.h"
#include "render/Camera.h"
#include "render/Shader.h"
#include "render/Window.h"

namespace {

constexpr int kMaxTrailPoints = 40000;
const core::NBodyGravity kGravity(core::solar::kG);
constexpr double kDt = 1.0 / 365.25;  // one day, in years

// Matching the palette the Python figures use.
constexpr glm::vec4 kSunColor{0.929f, 0.631f, 0.0f, 1.0f};
constexpr glm::vec4 kBodyColor{0.165f, 0.471f, 0.839f, 1.0f};
constexpr glm::vec4 kTrailColor{0.290f, 0.580f, 0.900f, 0.90f};

// A GPU buffer that a std::vector of points gets streamed into every frame.
class PointBuffer {
 public:
  PointBuffer() {
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    // DYNAMIC_DRAW: the contents are rewritten every frame.
    glBufferData(GL_ARRAY_BUFFER, kMaxTrailPoints * sizeof(glm::vec3), nullptr,
                 GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
    glBindVertexArray(0);
  }

  ~PointBuffer() {
    glDeleteBuffers(1, &vbo_);
    glDeleteVertexArrays(1, &vao_);
  }

  PointBuffer(const PointBuffer&) = delete;
  PointBuffer& operator=(const PointBuffer&) = delete;

  void upload(const std::vector<glm::vec3>& points) {
    count_ = static_cast<int>(points.size());
    if (count_ == 0) return;
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    static_cast<long>(points.size() * sizeof(glm::vec3)),
                    points.data());
  }

  void draw(unsigned int mode) const {
    if (count_ == 0) return;
    glBindVertexArray(vao_);
    glDrawArrays(mode, 0, count_);
    glBindVertexArray(0);
  }

 private:
  unsigned int vao_ = 0, vbo_ = 0;
  int count_ = 0;
};

}  // namespace

int main() {
  try {
    render::Window window(1280, 800, "physics-sims | orbital");
    render::Shader shader("orbit.vert", "orbit.frag");
    render::Camera camera;
    camera.setDistance(3.0f);

    glEnable(GL_PROGRAM_POINT_SIZE);

    core::System system = core::scenarios::sunEarth();
    core::Method method = core::Method::Euler;

    std::vector<std::deque<glm::vec3>> trails(system.size());
    std::vector<PointBuffer> trail_buffers(system.size());
    PointBuffer body_buffer;

    bool paused = false;
    bool screenshot_requested = false;
    int steps_per_frame = 4;
    double sim_time = 0.0;
    double last_title_update = 0.0;

    while (!window.shouldClose()) {
      window.beginFrame();

      // --- input ---------------------------------------------------------
      if (window.keyDown(GLFW_KEY_ESCAPE)) window.close();
      if (window.keyPressed(GLFW_KEY_SPACE)) paused = !paused;
      if (window.keyPressed(GLFW_KEY_R)) {
        std::printf(shader.reload() ? "[shader] reloaded\n" : "[shader] reload failed\n");
      }
      if (window.keyPressed(GLFW_KEY_P)) screenshot_requested = true;
      if (window.keyPressed(GLFW_KEY_LEFT_BRACKET)) {
        steps_per_frame = std::max(1, steps_per_frame / 2);
      }
      if (window.keyPressed(GLFW_KEY_RIGHT_BRACKET)) {
        steps_per_frame = std::min(4096, steps_per_frame * 2);
      }

      auto switch_method = [&](core::Method m) {
        system = core::scenarios::sunEarth();
        method = m;
        sim_time = 0.0;
        for (auto& trail : trails) trail.clear();
      };
      if (window.keyPressed(GLFW_KEY_1)) switch_method(core::Method::Euler);
      if (window.keyPressed(GLFW_KEY_2)) switch_method(core::Method::Verlet);
      if (window.keyPressed(GLFW_KEY_3)) switch_method(core::Method::RK4);

      if (window.keyPressed(GLFW_KEY_C)) {
        for (auto& trail : trails) trail.clear();
      }

      if (window.mouseDown(GLFW_MOUSE_BUTTON_LEFT)) {
        camera.orbit(window.mouseDelta().x, window.mouseDelta().y);
      }
      if (window.mouseDown(GLFW_MOUSE_BUTTON_RIGHT)) {
        camera.pan(window.mouseDelta().x, window.mouseDelta().y);
      }
      camera.zoom(window.scrollDelta());

      // --- simulate ------------------------------------------------------
      if (!paused) {
        try {
          for (int i = 0; i < steps_per_frame; ++i) {
            core::step(system, kDt, method, kGravity);
            sim_time += kDt;
          }
        } catch (const std::logic_error& e) {
          // Fall back rather than throwing the same exception every frame.
          std::printf("[sim] %s -- falling back to Euler\n", e.what());
          switch_method(core::Method::Euler);
        }

        for (std::size_t i = 0; i < system.size(); ++i) {
          trails[i].push_back(glm::vec3(system.positions[i]));
          if (trails[i].size() > kMaxTrailPoints) trails[i].pop_front();
        }
      }

      // --- draw ----------------------------------------------------------
      glClearColor(0.043f, 0.047f, 0.055f, 1.0f);

      shader.use();
      shader.set("uModel", glm::mat4(1.0f));
      shader.set("uView", camera.viewMatrix());
      shader.set("uProjection", camera.projectionMatrix(window.aspect()));

      // Trails, skipping body 0 -- the Sun barely moves.
      shader.set("uRound", false);
      shader.set("uColor", kTrailColor);
      shader.set("uPointSize", 1.0f);
      for (std::size_t i = 1; i < system.size(); ++i) {
        const std::vector<glm::vec3> points(trails[i].begin(), trails[i].end());
        trail_buffers[i].upload(points);
        trail_buffers[i].draw(GL_LINE_STRIP);
      }

      // Bodies, drawn as round point sprites.
      shader.set("uRound", true);
      for (std::size_t i = 0; i < system.size(); ++i) {
        const std::vector<glm::vec3> point{glm::vec3(system.positions[i])};
        body_buffer.upload(point);
        shader.set("uColor", i == 0 ? kSunColor : kBodyColor);
        shader.set("uPointSize", i == 0 ? 18.0f : 9.0f);
        body_buffer.draw(GL_POINTS);
      }

      // --- title bar readout ---------------------------------------------
      if (window.time() - last_title_update > 0.25) {
        last_title_update = window.time();
        std::string energy = "energy n/a";
        try {
          char buffer[64];
          std::snprintf(buffer, sizeof(buffer), "E = %.9e", core::totalEnergy(system, kGravity));
          energy = buffer;
        } catch (const std::logic_error&) {
        }
        window.setTitle("physics-sims | orbital | " + std::string(core::methodName(method)) +
                        " | t = " + std::to_string(static_cast<int>(sim_time)) + " yr | " +
                        energy + (paused ? " | PAUSED" : ""));
      }

      // Captured here rather than in the input block: beginFrame() clears the
      // framebuffer, so reading it before the scene is drawn returns nothing
      // but the clear colour. glReadPixels also has to come before the swap,
      // since the back buffer's contents are undefined afterwards.
      if (screenshot_requested) {
        screenshot_requested = false;
        const std::string path = std::string("orbital-") +
                                 std::to_string(static_cast<long>(window.time() * 1000)) +
                                 ".ppm";
        std::printf(window.saveScreenshot(path) ? "[shot] wrote %s\n"
                                                : "[shot] failed: %s\n",
                    path.c_str());
      }

      window.endFrame();
    }
  } catch (const std::exception& e) {
    std::fprintf(stderr, "fatal: %s\n", e.what());
    return 1;
  }
  return 0;
}
