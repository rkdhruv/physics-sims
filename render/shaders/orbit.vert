#version 410 core

// Position in world space (AU). Trails and bodies both go through this.
layout (location = 0) in vec3 aPosition;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform float uPointSize;

void main() {
    // Applied right-to-left: model, then view, then projection.
    gl_Position = uProjection * uView * uModel * vec4(aPosition, 1.0);

    // Only GL_POINTS reads this, and only if GL_PROGRAM_POINT_SIZE is enabled
    // on the CPU side.
    gl_PointSize = uPointSize;
}
