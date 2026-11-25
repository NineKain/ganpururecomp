// Gunman's Proof Recomp: original "CRT Fast" shader. Single pass, cheap.
// Soft horizontal beam, scanlines, a faint halo and an aperture-grille mask.
// Original work for this project (GPL-3.0); written from general CRT
// techniques, no code copied from other shader packs.

#pragma parameter SCAN "Scanline depth" 0.80 0.0 1.0 0.05
#pragma parameter HALO "Glow amount" 0.10 0.0 0.5 0.01
#pragma parameter MASK "Phosphor mask strength" 0.22 0.0 1.0 0.05

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
uniform vec2 OutputSize;
uniform float SCAN;
uniform float HALO;
uniform float MASK;
in vec2 vTexCoord;
out vec4 FragColor;

vec3 lin(vec3 c) { return c * c; }                 // ~gamma 2.0, cheap
vec3 enc(vec3 c) { return sqrt(max(c, 0.0)); }

vec3 beam_line(float lineIdx, float x) {
    // 3 bilinear taps across the line = a little horizontal softness
    float y = (lineIdx + 0.5) / InputSize.y;
    float dx = 0.5 / InputSize.x;
    return lin(texture(Texture, vec2(x - dx, y)).rgb) * 0.25
         + lin(texture(Texture, vec2(x,      y)).rgb) * 0.50
         + lin(texture(Texture, vec2(x + dx, y)).rgb) * 0.25;
}

void main() {
    float ypos = vTexCoord.y * InputSize.y - 0.5;
    float y0 = floor(ypos);
    float f = ypos - y0;
    vec3 c0 = beam_line(y0, vTexCoord.x);
    vec3 c1 = beam_line(y0 + 1.0, vTexCoord.x);

    // Brighter pixels get a fatter beam, like a real tube.
    vec3 s0 = mix(vec3(0.27), vec3(0.42), c0);
    vec3 s1 = mix(vec3(0.27), vec3(0.42), c1);
    vec3 w0 = exp(-(f * f) / (2.0 * s0 * s0));
    vec3 w1 = exp(-((1.0 - f) * (1.0 - f)) / (2.0 * s1 * s1));
    vec3 beam = c0 * w0 + c1 * w1;
    vec3 plain = mix(c0, c1, f);

    // Scanlines only when each source line has enough output pixels.
    float ky = clamp((OutputSize.y / InputSize.y - 1.6) / 1.6, 0.0, 1.0) * SCAN;
    vec3 col = mix(plain, beam * 1.18, ky);

    // Faint halo from the neighbours (very cheap bloom).
    vec2 px = 1.0 / InputSize;
    vec3 h = lin(texture(Texture, vTexCoord + vec2( 1.5,  1.5) * px).rgb)
           + lin(texture(Texture, vTexCoord + vec2(-1.5,  1.5) * px).rgb)
           + lin(texture(Texture, vTexCoord + vec2( 1.5, -1.5) * px).rgb)
           + lin(texture(Texture, vTexCoord + vec2(-1.5, -1.5) * px).rgb);
    col += h * 0.25 * HALO;

    // Aperture grille on the output pixels, faded out when too coarse.
    float kx = clamp((OutputSize.x / InputSize.x - 1.8) / 2.0, 0.0, 1.0) * MASK;
    float ph = mod(floor(gl_FragCoord.x), 3.0);
    vec3 m = vec3(1.0 - kx);
    m[int(ph)] = 1.0;
    m /= (1.0 - kx) * 2.0 / 3.0 + 1.0 / 3.0;
    col *= m;

    FragColor = vec4(enc(col), 1.0);
}
#endif
