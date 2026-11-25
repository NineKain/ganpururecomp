// Gunman's Proof Recomp: "CRT Accurate" pass 2 of 4 - halation, horizontal.
// Original work for this project (GPL-3.0).

#ifdef VERTEX
in vec2 VertexCoord;
in vec2 TexCoord;
out vec2 vTexCoord;
void main() {
    vTexCoord = TexCoord;
    gl_Position = vec4(VertexCoord * 2.0 - 1.0, 0.0, 1.0);
}
#endif

#ifdef FRAGMENT
uniform sampler2D Texture;
uniform vec2 InputSize;
in vec2 vTexCoord;
out vec4 FragColor;

void main() {
    // This pass renders at half resolution; step 2 source pixels per tap.
    float step_x = 2.0 / InputSize.x;
    vec3 acc = vec3(0.0);
    float wsum = 0.0;
    for (int i = -4; i <= 4; i++) {
        float w = exp(-float(i * i) / 8.0);          // sigma = 2 taps
        acc += texture(Texture, vTexCoord + vec2(step_x * float(i), 0.0)).rgb * w;
        wsum += w;
    }
    FragColor = vec4(acc / wsum, 1.0);
}
#endif
