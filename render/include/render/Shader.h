#pragma once

#include <string>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace render {

// A compiled vertex+fragment program, loaded from files on disk rather than
// baked into the binary, so reload() can pick up edits without a rebuild.
class Shader {
 public:
  // Paths are relative to render/shaders/ (see SHADER_DIR in CMakeLists.txt).
  Shader(const std::string& vertex_file, const std::string& fragment_file);
  ~Shader();

  Shader(const Shader&) = delete;
  Shader& operator=(const Shader&) = delete;

  void use() const;

  // Recompile from disk. On failure the previous working program is kept and
  // the error printed, so a typo doesn't take the window down mid-session.
  bool reload();

  void set(const std::string& name, bool value) const;
  void set(const std::string& name, int value) const;
  void set(const std::string& name, float value) const;
  void set(const std::string& name, const glm::vec3& value) const;
  void set(const std::string& name, const glm::vec4& value) const;
  void set(const std::string& name, const glm::mat4& value) const;

  unsigned int id() const { return program_; }

 private:
  unsigned int program_ = 0;
  std::string vertex_path_;
  std::string fragment_path_;

  int uniformLocation(const std::string& name) const;
  static unsigned int compile(const std::string& path, unsigned int type);
  static unsigned int link(unsigned int vert, unsigned int frag);
};

}  // namespace render
