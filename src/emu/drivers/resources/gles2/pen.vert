// GLES 2.0 port of the desktop pen shader. GLSL ES 1.00 has no flat
// interpolation and no integer bitwise operators, so the segment distance is
// carried per-vertex by the CPU (in_dist) and the stipple bit is extracted
// arithmetically in the fragment shader.
precision highp float;

attribute vec2 in_position;
attribute float in_dist;

uniform mat4 u_proj;
uniform mat4 u_model;
uniform float u_pointSize;

varying float v_dist;

void main() {
    gl_Position = u_proj * u_model * vec4(in_position, 0.0, 1.0);
    gl_Position.y = -gl_Position.y;
    gl_PointSize = u_pointSize;

    v_dist = in_dist;
}
