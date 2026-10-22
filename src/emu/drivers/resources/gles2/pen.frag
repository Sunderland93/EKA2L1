// GLES 2.0 port of the desktop pen shader. See pen.vert: there is no flat
// interpolation or integer bitwise support in GLSL ES 1.00.
//
// The vertex shader stores twice the NDC segment distance in v_dist, matching
// the desktop shader's (current - start) * u_viewport length.
precision highp float;

uniform vec4 u_color;
uniform float u_pattern;

varying float v_dist;

void main() {
    float bit_index = mod(floor(v_dist + 0.5), 16.0);

    // (u_pattern >> bit_index) & 1, using only arithmetic.
    float bit_mask = exp2(bit_index);
    float bit_value = mod(floor(u_pattern / bit_mask), 2.0);

    if (bit_value < 0.5) {
        discard;
    }

    gl_FragColor = u_color / 255.0;
}
