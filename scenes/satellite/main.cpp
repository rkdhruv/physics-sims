// Earth-orbit scene: satellite propagation with the J2 oblateness
// perturbation, and a ground track panel along the bottom.
//
// Controls:
//   left drag      orbit the camera        scroll    zoom
//   right drag     pan                     space     pause
//   1 / 2 / 3      ISS / sun-sync / Molniya
//   J              toggle the J2 perturbation
//   [ / ]          slow down / speed up    C         clear trails
//   R              reload shaders          P         screenshot
//   esc            quit

#include <cstdio>
#include <deque>
#include <stdexcept>
#include <string>
#include <vector>

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include "core/Elements.h"
#include "core/ForceModel.h"
#include "core/Integrator.h"
#include "core/Scenarios.h"
#include "core/System.h"
#include "core/Units.h"
#include "render/Camera.h"
#include "render/Shader.h"
#include "render/Window.h"

namespace {

// History is capped in orbits, not samples: a fixed point count would be a
// few orbits for the ISS and less than one for Molniya, whose period is eight
// times longer. Six orbits keeps the plot legible -- long enough to show the
// track drifting, short enough that it doesn't saturate into a solid mesh.
constexpr double kHistoryOrbits = 6.0;
constexpr int kMaxTrackPoints = 30000;
constexpr double kDt = 1.0;          // seconds
constexpr double kSampleInterval = 30.0;  // simulated seconds between samples

// The track is drawn as segment pairs, so it needs twice its point count.
constexpr int kBufferCapacity = 2 * kMaxTrackPoints + 16;

constexpr glm::vec4 kEarthColor{0.20f, 0.35f, 0.55f, 0.85f};
constexpr glm::vec4 kEquatorColor{0.55f, 0.62f, 0.72f, 0.9f};
constexpr glm::vec4 kSatColor{0.925f, 0.408f, 0.204f, 1.0f};
constexpr glm::vec4 kTrailColor{0.960f, 0.470f, 0.260f, 0.85f};
constexpr glm::vec4 kTrackColor{0.180f, 0.800f, 0.560f, 0.95f};
constexpr glm::vec4 kGridColor{0.25f, 0.27f, 0.31f, 1.0f};

class LineBuffer {
 public:
  LineBuffer() {
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, kBufferCapacity * sizeof(glm::vec3), nullptr,
                 GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
    glBindVertexArray(0);
  }
  ~LineBuffer() {
    glDeleteBuffers(1, &vbo_);
    glDeleteVertexArrays(1, &vao_);
  }
  LineBuffer(const LineBuffer&) = delete;
  LineBuffer& operator=(const LineBuffer&) = delete;

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

// Latitude/longitude wireframe, as disconnected line segments so the whole
// globe is one draw call.
std::vector<glm::vec3> sphereWireframe(float radius, int parallels, int meridians,
                                       int segments = 64) {
  std::vector<glm::vec3> lines;
  const float pi = static_cast<float>(core::kPi);

  for (int p = 1; p < parallels; ++p) {
    const float lat = pi * (static_cast<float>(p) / parallels - 0.5f);
    const float y = radius * std::sin(lat);
    const float r = radius * std::cos(lat);
    for (int s = 0; s < segments; ++s) {
      const float a0 = 2.0f * pi * s / segments;
      const float a1 = 2.0f * pi * (s + 1) / segments;
      lines.push_back({r * std::cos(a0), y, r * std::sin(a0)});
      lines.push_back({r * std::cos(a1), y, r * std::sin(a1)});
    }
  }

  for (int m = 0; m < meridians; ++m) {
    const float lon = pi * m / meridians;
    for (int s = 0; s < segments; ++s) {
      const float a0 = 2.0f * pi * s / segments;
      const float a1 = 2.0f * pi * (s + 1) / segments;
      lines.push_back({radius * std::cos(a0) * std::cos(lon),
                       radius * std::sin(a0),
                       radius * std::cos(a0) * std::sin(lon)});
      lines.push_back({radius * std::cos(a1) * std::cos(lon),
                       radius * std::sin(a1),
                       radius * std::cos(a1) * std::sin(lon)});
    }
  }
  return lines;
}

std::vector<glm::vec3> circle(float radius, int segments = 128) {
  std::vector<glm::vec3> points;
  for (int s = 0; s <= segments; ++s) {
    const float a = 2.0f * static_cast<float>(core::kPi) * s / segments;
    points.push_back({radius * std::cos(a), 0.0f, radius * std::sin(a)});
  }
  return points;
}

// Grid lines for the ground-track panel, in (longitude, latitude) degrees.
std::vector<glm::vec3> mapGrid() {
  std::vector<glm::vec3> lines;
  for (int lon = -180; lon <= 180; lon += 30) {
    lines.push_back({static_cast<float>(lon), -90.0f, 0.0f});
    lines.push_back({static_cast<float>(lon), 90.0f, 0.0f});
  }
  for (int lat = -90; lat <= 90; lat += 30) {
    lines.push_back({-180.0f, static_cast<float>(lat), 0.0f});
    lines.push_back({180.0f, static_cast<float>(lat), 0.0f});
  }
  return lines;
}

const char* scenarioName(int index) {
  switch (index) {
    case 0: return "ISS (420 km, 51.6 deg)";
    case 1: return "Sun-synchronous (700 km)";
    case 2: return "Molniya (critical inclination)";
  }
  return "";
}

core::System makeScenario(int index) {
  switch (index) {
    case 1: return core::scenarios::sunSynchronous();
    case 2: return core::scenarios::molniya();
  }
  return core::scenarios::iss();
}

}  // namespace

int main() {
  try {
    render::Window window(1280, 900, "physics-sims | satellite");
    render::Shader shader("orbit.vert", "orbit.frag");
    render::Camera camera;

    glEnable(GL_PROGRAM_POINT_SIZE);

    // Kilometres, not AU: the camera's default distance range and clip planes
    // are three orders of magnitude too small for this scene.
    const float re = static_cast<float>(core::earth::kRadius);
    camera.setDistanceRange(re * 1.05f, re * 40.0f);
    camera.setClipPlanes(re * 0.01f, re * 80.0f);
    camera.setDistance(re * 3.5f);

    int scenario_index = 0;
    core::System system = makeScenario(scenario_index);

    bool j2_enabled = true;
    bool j2_available = true;
    core::EarthOrbit forces({/*j2=*/true});

    LineBuffer globe_buffer, equator_buffer, trail_buffer, sat_buffer;
    LineBuffer track_buffer, grid_buffer;

    globe_buffer.upload(sphereWireframe(static_cast<float>(core::earth::kRadius), 6, 6));
    equator_buffer.upload(circle(static_cast<float>(core::earth::kRadius)));
    grid_buffer.upload(mapGrid());

    std::deque<glm::vec3> trail;
    std::deque<glm::vec3> track;

    bool paused = false;
    bool screenshot_requested = false;
    // ~400 simulated seconds per frame, so a day passes in about four seconds
    // of wall clock. J2's drift is roughly a degree a day; at real time it
    // would be invisible.
    int steps_per_frame = 400;
    double sim_time = 0.0;
    double last_title = 0.0;
    double last_sample = 0.0;
    double raan_at_start = 0.0;
    std::size_t history_limit = 2000;

    auto reset = [&](int index) {
      scenario_index = index;
      system = makeScenario(index);
      sim_time = 0.0;
      last_sample = 0.0;
      trail.clear();
      track.clear();
      try {
        const core::OrbitalElements el =
            core::toElements(system.positions[0], system.velocities[0]);
        raan_at_start = el.raan;
        const double period = core::orbitalPeriod(el.semi_major_axis);
        history_limit = static_cast<std::size_t>(
            std::min(kHistoryOrbits * period / kSampleInterval,
                     static_cast<double>(kMaxTrackPoints)));
      } catch (const std::logic_error&) {
        raan_at_start = 0.0;
      }
    };
    reset(scenario_index);

    while (!window.shouldClose()) {
      window.beginFrame();

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
        steps_per_frame = std::min(8192, steps_per_frame * 2);
      }
      if (window.keyPressed(GLFW_KEY_1)) reset(0);
      if (window.keyPressed(GLFW_KEY_2)) reset(1);
      if (window.keyPressed(GLFW_KEY_3)) reset(2);
      if (window.keyPressed(GLFW_KEY_C)) { trail.clear(); track.clear(); }
      if (window.keyPressed(GLFW_KEY_J) && j2_available) {
        j2_enabled = !j2_enabled;
        forces = core::EarthOrbit({j2_enabled});
        reset(scenario_index);
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
            core::step(system, kDt, core::Method::RK4, forces);
            sim_time += kDt;

            // Sampled on simulated time rather than once per frame, so the
            // history spans weeks instead of seconds. J2's signature is a drift
            // of about a degree a day -- at one sample per frame the buffer
            // holds a few minutes and there is simply nothing to see.
            if (sim_time - last_sample < kSampleInterval) continue;
            last_sample = sim_time;

            // Positions are thousands of km, so the cast to float costs well
            // under a metre.
            trail.push_back(glm::vec3(system.positions[0]));
            if (trail.size() > history_limit) trail.pop_front();

            const core::GroundPoint g =
                core::toGroundPoint(system.positions[0], sim_time);
            track.push_back({static_cast<float>(g.longitude),
                             static_cast<float>(g.latitude), 0.0f});
            if (track.size() > history_limit) track.pop_front();
          }
        } catch (const std::logic_error& e) {
          std::printf("[sim] %s -- disabling J2\n", e.what());
          j2_available = false;
          j2_enabled = false;
          forces = core::EarthOrbit({/*j2=*/false});
          reset(scenario_index);
        }
      }

      // --- draw 3D view ---------------------------------------------------
      const glm::ivec2 size = window.framebufferSize();
      const int panel_height = size.y / 4;

      glClearColor(0.043f, 0.047f, 0.055f, 1.0f);
      glViewport(0, panel_height, size.x, size.y - panel_height);

      shader.use();
      shader.set("uModel", glm::mat4(1.0f));
      shader.set("uView", camera.viewMatrix());
      shader.set("uProjection", camera.projectionMatrix(
                                    static_cast<float>(size.x) /
                                    static_cast<float>(size.y - panel_height)));
      shader.set("uRound", false);
      shader.set("uPointSize", 1.0f);

      shader.set("uColor", kEarthColor);
      globe_buffer.draw(GL_LINES);

      shader.set("uColor", kEquatorColor);
      equator_buffer.draw(GL_LINE_STRIP);

      shader.set("uColor", kTrailColor);
      trail_buffer.upload(std::vector<glm::vec3>(trail.begin(), trail.end()));
      trail_buffer.draw(GL_LINE_STRIP);

      shader.set("uRound", true);
      shader.set("uColor", kSatColor);
      shader.set("uPointSize", 10.0f);
      sat_buffer.upload({glm::vec3(system.positions[0])});
      sat_buffer.draw(GL_POINTS);

      // --- draw ground track panel ----------------------------------------
      glViewport(0, 0, size.x, panel_height);
      glDisable(GL_DEPTH_TEST);

      // Orthographic straight onto the lon/lat plane -- an equirectangular
      // projection, which is what makes a ground track the familiar sine wave.
      shader.set("uView", glm::mat4(1.0f));
      shader.set("uProjection",
                 glm::ortho(-185.0f, 185.0f, -95.0f, 95.0f, -1.0f, 1.0f));
      shader.set("uRound", false);

      shader.set("uColor", kGridColor);
      grid_buffer.draw(GL_LINES);

      // Drawn as separate segments rather than one strip: a pass wraps from
      // +180 to -180, and a strip would stripe straight back across the map.
      // Dropping just the wrapping segment keeps every previous pass on screen,
      // which is what makes the track's drift visible at all.
      shader.set("uColor", kTrackColor);
      std::vector<glm::vec3> segments;
      segments.reserve(track.size() * 2);
      for (std::size_t i = 1; i < track.size(); ++i) {
        if (std::abs(track[i].x - track[i - 1].x) > 180.0f) continue;
        segments.push_back(track[i - 1]);
        segments.push_back(track[i]);
      }
      track_buffer.upload(segments);
      track_buffer.draw(GL_LINES);

      glEnable(GL_DEPTH_TEST);

      // --- title readout ---------------------------------------------------
      if (window.time() - last_title > 0.25) {
        last_title = window.time();

        std::string elements = "elements n/a";
        try {
          const core::OrbitalElements el =
              core::toElements(system.positions[0], system.velocities[0]);

          // RAAN drift since the run started is the readout that makes J2
          // legible: with it off this stays pinned at zero, with it on it
          // walks at about a degree a day.
          double drift = core::degrees(el.raan - raan_at_start);
          if (drift > 180.0) drift -= 360.0;
          if (drift < -180.0) drift += 360.0;
          const double days = sim_time / 86400.0;

          char buffer[224];
          std::snprintf(buffer, sizeof(buffer),
                        "a=%.1f km  e=%.4f  i=%.2f  dRAAN=%+.3f deg (%+.3f deg/day)",
                        el.semi_major_axis, el.eccentricity,
                        core::degrees(el.inclination), drift,
                        days > 0.05 ? drift / days : 0.0);
          elements = buffer;
        } catch (const std::logic_error&) {
        }

        window.setTitle(
            "physics-sims | " + std::string(scenarioName(scenario_index)) +
            " | J2 " + (j2_enabled ? "on" : "off") +
            " | t = " + std::to_string(static_cast<int>(sim_time / 3600.0)) + " h | " +
            elements + (paused ? " | PAUSED" : ""));
      }

      // Captured here rather than in the input block: beginFrame() clears the
      // framebuffer, so reading it before the scene is drawn returns nothing
      // but the clear colour. glReadPixels also has to come before the swap,
      // since the back buffer's contents are undefined afterwards.
      if (screenshot_requested) {
        screenshot_requested = false;
        const std::string path = std::string("satellite-") +
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
