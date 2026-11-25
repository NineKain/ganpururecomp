// Gunman's Proof Recomp: CRT shader, pass 3 of 4 - glow, vertical.
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
    float step_y = 1.0 / InputSize.y;
    vec3 acc = vec3(0.0);
    float wsum = 0.0;
    for (int i = -3; i <= 3; i++) {
        float w = exp(-float(i * i) / 2.9);
        acc += texture(Texture, vTexCoord + vec2(0.0, step_y * float(i))).rgb * w;
        wsum += w;
    }
    FragColor = vec4(acc / wsum, 1.0);
}
#endif
