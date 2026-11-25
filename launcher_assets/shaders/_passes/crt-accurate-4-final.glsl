// Gunman's Proof Recomp: "CRT Accurate" pass 4 of 4 - the display.
// Brightness-dependent scanline beam between the two nearest source lines,
// halation glow, aperture-grille phosphor mask, display gamma.
// Original work for this project (GPL-3.0).

#pragma parameter BEAM_MIN "Beam width, dark pixels" 0.27 0.10 0.60 0.01
#pragma parameter BEAM_MAX "Beam width, bright pixels" 0.42 0.10 0.80 0.01
#pragma parameter BLOOM "Glow (halation) amount" 0.14 0.0 0.6 0.01
#pragma parameter MASK "Phosphor mask strength" 0.22 0.0 1.0 0.05
#pragma parameter GAIN "Brightness compensation" 1.18 0.5 2.0 0.01
#pragma parameter OUT_GAMMA "Display gamma" 2.2 1.8 3.0 0.05

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
uniform sampler2D Texture;          // pass 3: blurred glow
uniform sampler2D Pass1Texture;     // pass 1: beam-softened linear picture
uniform vec2 Pass1InputSize;
uniform vec2 OutputSize;
uniform float BEAM_MIN;
uniform float BEAM_MAX;
uniform float BLOOM;
uniform float MASK;
uniform float GAIN;
uniform float OUT_GAMMA;
in vec2 vTexCoord;
out vec4 FragColor;

void main() {
    vec2 src = Pass1InputSize;
    float ypos = vTexCoord.y * src.y - 0.5;
    float y0 = floor(ypos);
    float f = ypos - y0;                              // 0 on line y0, 1 on y0+1
    vec3 c0 = texture(Pass1Texture, vec2(vTexCoord.x, (y0 + 0.5) / src.y)).rgb;
    vec3 c1 = texture(Pass1Texture, vec2(vTexCoord.x, (y0 + 1.5) / src.y)).rgb;

    // Gaussian beam per colour channel; brighter pixels fatten the beam so
    // highlights spill into the gap between lines, like a real tube.
    vec3 s0 = mix(vec3(BEAM_MIN), vec3(BEAM_MAX), clamp(c0, 0.0, 1.0));
    vec3 s1 = mix(vec3(BEAM_MIN), vec3(BEAM_MAX), clamp(c1, 0.0, 1.0));
    vec3 w0 = exp(-(f * f) / (2.0 * s0 * s0));
    vec3 w1 = exp(-((1.0 - f) * (1.0 - f)) / (2.0 * s1 * s1));
    vec3 beam = (c0 * w0 + c1 * w1) * GAIN;
    vec3 plain = mix(c0, c1, f);

    // Scanline gaps only read cleanly with >= ~3 output pixels per line.
    float ky = clamp((OutputSize.y / src.y - 1.6) / 1.6, 0.0, 1.0);
    vec3 col = mix(plain, beam, ky);

    // Aperture grille, energy-preserving, faded out when too coarse.
    float kx = clamp((OutputSize.x / src.x - 1.8) / 2.0, 0.0, 1.0) * MASK;
    float ph = mod(floor(gl_FragCoord.x), 3.0);
    vec3 m = vec3(1.0 - kx);
    m[int(ph)] = 1.0;
    m /= (1.0 - kx) * 2.0 / 3.0 + 1.0 / 3.0;
    col *= m;

    // Halation: light scattered in the glass, not affected by the mask.
    col += texture(Texture, vTexCoord).rgb * BLOOM;

    FragColor = vec4(pow(clamp(col, 0.0, 1.0), vec3(1.0 / OUT_GAMMA)), 1.0);
}
#endif
