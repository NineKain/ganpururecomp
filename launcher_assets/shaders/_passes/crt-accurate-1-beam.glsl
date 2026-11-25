// Gunman's Proof Recomp: original "CRT Accurate" shader, pass 1 of 4.
// Linearise the picture and apply the horizontal beam softness at source
// resolution. Original work for this project (GPL-3.0); the structure
// (linear light -> beam -> halation -> mask) follows the general approach of
// high-end CRT shaders, but none of their code is used here.

#pragma parameter BEAM_H "Horizontal beam softness (source px)" 0.55 0.05 1.5 0.05
#pragma parameter IN_GAMMA "CRT input gamma" 2.4 1.8 3.0 0.05

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
uniform float BEAM_H;
uniform float IN_GAMMA;
in vec2 vTexCoord;
out vec4 FragColor;

void main() {
    vec2 px = vec2(1.0 / InputSize.x, 0.0);
    float inv = 1.0 / (2.0 * BEAM_H * BEAM_H);
    vec3 acc = vec3(0.0);
    float wsum = 0.0;
    for (int i = -2; i <= 2; i++) {
        float w = exp(-float(i * i) * inv);
        acc += pow(texture(Texture, vTexCoord + px * float(i)).rgb, vec3(IN_GAMMA)) * w;
        wsum += w;
    }
    FragColor = vec4(acc / wsum, 1.0);
}
#endif
