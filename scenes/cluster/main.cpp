// Self-gravitating star cluster, with the solver switchable at runtime.
//
// Controls:
//   left drag      orbit the camera        scroll    zoom
//   right drag     pan                     space     pause
//   1 / 2          direct / Barnes-Hut     - / =     theta down / up
//   [ / ]          fewer / more bodies     , / .     point size
//   C              restart
//   R              reload shaders          P         screenshot
//   esc            quit

#include <algorithm>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include "core/BarnesHut.h"
#include "core/Diagnostics.h"
#include "core/ForceModel.h"
#include "core/Integrator.h"
#include "core/Scenarios.h"
#include "core/System.h"
#include "render/Camera.h"
#include "render/Shader.h"
#include "render/Window.h"

namespace {

constexpr std::size_t kMinBodies = 256;
constexpr std::size_t kMaxBodies = 32768;
constexpr std::size_t kDefaultBodies = 4096;

// A cluster crossing takes O(1) time in these units, so this is ~50 steps per
// crossing -- large, but measured rather than guessed. With the tree at
// theta=0.7 the force approximation puts a floor of ~3e-4 under the energy
// drift, and the drift is identical at dt=0.002 and dt=0.04: below that floor
// a smaller timestep buys nothing. The exact solver does scale as dt^2, so
// pressing 1 makes the step size matter again.
constexpr double kDt = 2.0e-2;

constexpr glm::vec4 kBodyColor{0.65f, 0.78f, 1.0f, 0.55f};

// Streams a position buffer to the GPU once per frame and draws it in a single
// call. The other scenes issue one draw per body, which is fine for two and
// hopeless for thousands.
class PointCloud {
 public:
  PointCloud() {
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, kMaxBodies * sizeof(glm::vec3), nullptr,
                 GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
    glBindVertexArray(0);
  }

  ~PointCloud() {
    glDeleteBuffers(1, &vbo_);
    glDeleteVertexArrays(1, &vao_);
  }

  PointCloud(const PointCloud&) = delete;
  PointCloud& operator=(const PointCloud&) = delete;

  void upload(const std::vector<glm::dvec3>& positions) {
    scratch_.resize(positions.size());
    for (std::size_t i = 0; i < positions.size(); ++i) {
      scratch_[i] = glm::vec3(positions[i]);
    }
    count_ = static_cast<int>(scratch_.size());

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    // Orphan the old storage first: the driver may still be drawing from it,
    // and this lets it hand back fresh memory instead of stalling until the
    // previous frame finishes.
    glBufferData(GL_ARRAY_BUFFER, kMaxBodies * sizeof(glm::vec3), nullptr,
                 GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    static_cast<long>(scratch_.size() * sizeof(glm::vec3)),
                    scratch_.data());
  }

  void draw() const {
    if (count_ == 0) return;
    glBindVertexArray(vao_);
    glDrawArrays(GL_POINTS, 0, count_);
    glBindVertexArray(0);
  }

 private:
  unsigned int vao_ = 0, vbo_ = 0;
  int count_ = 0;
  std::vector<glm::vec3> scratch_;
};

}  // namespace

int main() {
  try {
    render::Window window(1280, 900, "physics-sims | cluster");
    render::Shader shader("orbit.vert", "orbit.frag");
    render::Camera camera;
    camera.setDistance(8.0f);

    glEnable(GL_PROGRAM_POINT_SIZE);

    std::size_t body_count = kDefaultBodies;
    double theta = 0.7;
    bool use_tree = true;
    float point_size = 3.0f;

    core::System system = core::scenarios::cluster(body_count);
    PointCloud cloud;

    bool paused = false;
    bool screenshot_requested = false;
    double sim_time = 0.0;
    double step_ms = 0.0;
    double last_title = 0.0;
    double last_energy_check = -1.0;
    double energy_0 = 0.0;
    double energy_drift = 0.0;

    const double softening = core::scenarios::kClusterSoftening;

    auto makeModel = [&]() -> std::unique_ptr<core::ForceModel> {
      if (use_tree) {
        return std::make_unique<core::BarnesHutGravity>(1.0, theta, softening);
      }
      return std::make_unique<core::NBodyGravity>(1.0, softening);
    };
    std::unique_ptr<core::ForceModel> forces = makeModel();

    auto restart = [&]() {
      system = core::scenarios::cluster(body_count);
      forces = makeModel();
      sim_time = 0.0;
      last_energy_check = -1.0;
      energy_0 = core::totalEnergy(system, *forces);
      energy_drift = 0.0;
    };
    restart();

    while (!window.shouldClose()) {
      window.beginFrame();

      if (window.keyDown(GLFW_KEY_ESCAPE)) window.close();
      if (window.keyPressed(GLFW_KEY_SPACE)) paused = !paused;
      if (window.keyPressed(GLFW_KEY_R)) {
        std::printf(shader.reload() ? "[shader] reloaded\n" : "[shader] reload failed\n");
      }
      if (window.keyPressed(GLFW_KEY_P)) screenshot_requested = true;
      if (window.keyPressed(GLFW_KEY_C)) restart();

      if (window.keyPressed(GLFW_KEY_1) && use_tree) {
        use_tree = false;
        restart();
      }
      if (window.keyPressed(GLFW_KEY_2) && !use_tree) {
        use_tree = true;
        restart();
      }
      if (window.keyPressed(GLFW_KEY_LEFT_BRACKET)) {
        body_count = std::max(kMinBodies, body_count / 2);
        restart();
      }
      if (window.keyPressed(GLFW_KEY_RIGHT_BRACKET)) {
        body_count = std::min(kMaxBodies, body_count * 2);
        restart();
      }
      if (window.keyPressed(GLFW_KEY_COMMA)) {
        point_size = std::max(1.0f, point_size - 1.0f);
      }
      if (window.keyPressed(GLFW_KEY_PERIOD)) {
        point_size = std::min(16.0f, point_size + 1.0f);
      }
      if (window.keyPressed(GLFW_KEY_MINUS)) {
        theta = std::max(0.0, theta - 0.1);
        restart();
      }
      if (window.keyPressed(GLFW_KEY_EQUAL)) {
        theta = std::min(2.0, theta + 0.1);
        restart();
      }

      if (window.mouseDown(GLFW_MOUSE_BUTTON_LEFT)) {
        camera.orbit(window.mouseDelta().x, window.mouseDelta().y);
      }
      if (window.mouseDown(GLFW_MOUSE_BUTTON_RIGHT)) {
        camera.pan(window.mouseDelta().x, window.mouseDelta().y);
      }
      camera.zoom(window.scrollDelta());

      // --- simulate --------------------------------------------------------
      if (!paused) {
        const double before = window.time();
        core::step(system, kDt, core::Method::Verlet, *forces);
        step_ms = (window.time() - before) * 1e3;
        sim_time += kDt;

        // Energy is an exact O(n^2) sum for both models, so it costs as much
        // as a direct force evaluation. Sampled once a second rather than
        // every frame, or the diagnostic would dominate the frame time.
        if (window.time() - last_energy_check > 1.0) {
          last_energy_check = window.time();
          const double energy = core::totalEnergy(system, *forces);
          energy_drift = std::abs((energy - energy_0) / energy_0);
        }
      }

      // --- draw ------------------------------------------------------------
      glClearColor(0.035f, 0.038f, 0.047f, 1.0f);

      // Additive blending: overlapping particles accumulate rather than
      // occlude, so the dense core reads as bright instead of flat.
      glBlendFunc(GL_SRC_ALPHA, GL_ONE);
      glDepthMask(GL_FALSE);

      shader.use();
      shader.set("uModel", glm::mat4(1.0f));
      shader.set("uView", camera.viewMatrix());
      shader.set("uProjection", camera.projectionMatrix(window.aspect()));
      shader.set("uRound", true);
      shader.set("uColor", kBodyColor);
      shader.set("uPointSize", point_size);

      cloud.upload(system.positions);
      cloud.draw();

      glDepthMask(GL_TRUE);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

      if (window.time() - last_title > 0.25) {
        last_title = window.time();
        char buffer[256];
        std::snprintf(buffer, sizeof(buffer),
                      "physics-sims | cluster | %zu bodies | %s%s | "
                      "%.1f ms/step | t = %.1f | dE/E = %.2e%s",
                      body_count,
                      use_tree ? "Barnes-Hut theta=" : "direct summation",
                      use_tree ? std::to_string(theta).substr(0, 3).c_str() : "",
                      step_ms, sim_time, energy_drift,
                      paused ? " | PAUSED" : "");
        window.setTitle(buffer);
      }

      if (screenshot_requested) {
        screenshot_requested = false;
        const std::string path =
            "cluster-" + std::to_string(static_cast<long>(window.time() * 1000)) +
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
