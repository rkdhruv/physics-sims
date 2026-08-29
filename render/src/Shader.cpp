#include "render/Shader.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>

namespace render {
namespace {

std::string resolve(const std::string& filename) {
  return std::string(SHADER_DIR) + "/" + filename;
}

std::string readFile(const std::string& path) {
  std::ifstream file(path);
  if (!file) throw std::runtime_error("cannot open shader file: " + path);
  std::ostringstream contents;
  contents << file.rdbuf();
  return contents.str();
}

}  // namespace

Shader::Shader(const std::string& vertex_file, const std::string& fragment_file)
    : vertex_path_(resolve(vertex_file)), fragment_path_(resolve(fragment_file)) {
  if (!reload()) throw std::runtime_error("shader failed to compile on startup");
}

Shader::~Shader() {
  if (program_) glDeleteProgram(program_);
}

unsigned int Shader::compile(const std::string& path, unsigned int type) {
  const std::string source = readFile(path);
  const char* source_ptr = source.c_str();

  const unsigned int shader = glCreateShader(type);
  glShaderSource(shader, 1, &source_ptr, nullptr);
  glCompileShader(shader);

  int ok = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    int length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    std::vector<char> log(length > 0 ? length : 1);
    glGetShaderInfoLog(shader, length, nullptr, log.data());
    glDeleteShader(shader);
    // The log carries line numbers into the file, hence printing the path too.
    std::fprintf(stderr, "[shader] %s failed to compile:\n%s\n", path.c_str(), log.data());
    return 0;
  }
  return shader;
}

unsigned int Shader::link(unsigned int vert, unsigned int frag) {
  const unsigned int program = glCreateProgram();
  glAttachShader(program, vert);
  glAttachShader(program, frag);
  glLinkProgram(program);

  int ok = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &ok);
  if (!ok) {
    int length = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    std::vector<char> log(length > 0 ? length : 1);
    glGetProgramInfoLog(program, length, nullptr, log.data());
    glDeleteProgram(program);
    std::fprintf(stderr, "[shader] link failed:\n%s\n", log.data());
    return 0;
  }
  return program;
}

bool Shader::reload() {
  unsigned int vert = 0, frag = 0;
  try {
    vert = compile(vertex_path_, GL_VERTEX_SHADER);
    frag = compile(fragment_path_, GL_FRAGMENT_SHADER);
  } catch (const std::exception& e) {
    std::fprintf(stderr, "[shader] %s\n", e.what());
  }

  if (!vert || !frag) {
    if (vert) glDeleteShader(vert);
    if (frag) glDeleteShader(frag);
    return false;  // keep whatever program_ already is
  }

  const unsigned int program = link(vert, frag);
  glDeleteShader(vert);
  glDeleteShader(frag);
  if (!program) return false;

  if (program_) glDeleteProgram(program_);
  program_ = program;
  return true;
}

void Shader::use() const { glUseProgram(program_); }

int Shader::uniformLocation(const std::string& name) const {
  // Returns -1 for a name the linker dropped (declared but unused). Setting
  // location -1 is a defined no-op, so no guard is needed -- but it also means
  // a misspelled uniform name fails silently.
  return glGetUniformLocation(program_, name.c_str());
}

void Shader::set(const std::string& n, bool v) const {
  glUniform1i(uniformLocation(n), static_cast<int>(v));
}
void Shader::set(const std::string& n, int v) const {
  glUniform1i(uniformLocation(n), v);
}
void Shader::set(const std::string& n, float v) const {
  glUniform1f(uniformLocation(n), v);
}
void Shader::set(const std::string& n, const glm::vec3& v) const {
  glUniform3fv(uniformLocation(n), 1, glm::value_ptr(v));
}
void Shader::set(const std::string& n, const glm::vec4& v) const {
  glUniform4fv(uniformLocation(n), 1, glm::value_ptr(v));
}
void Shader::set(const std::string& n, const glm::mat4& v) const {
  glUniformMatrix4fv(uniformLocation(n), 1, GL_FALSE, glm::value_ptr(v));
}

}  // namespace render
