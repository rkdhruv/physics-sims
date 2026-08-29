#version 410 core

out vec4 FragColor;

uniform vec4 uColor;
uniform bool uRound;   // draw GL_POINTS as discs instead of squares

void main() {
    if (uRound) {
        // gl_PointCoord runs 0..1 across the sprite; discarding outside the
        // inscribed circle turns the default square point into a disc.
        vec2 offset = gl_PointCoord - vec2(0.5);
        float r = length(offset);
        if (r > 0.5) discard;

        // Feathered edge -- MSAA doesn't antialias a discard.
        float alpha = 1.0 - smoothstep(0.45, 0.5, r);
        FragColor = vec4(uColor.rgb, uColor.a * alpha);
        return;
    }

    FragColor = uColor;
}
