// Gunman's Proof Recomp: CRT shader, pass 4 of 4 - the display.
// Sharp pixel scaling, brightness-dependent scanline beam between the two
// nearest source lines, optional glow, aperture-grille mask, display gamma.
// Every look-defining value is a parameter the in-game CRT menu drives.
// Original work for this project (GPL-3.0).

#pragma parameter SCAN "Scanline strength" 0.70 0.0 1.0 0.05
#pragma parameter BEAM_MIN "Beam width, dark pixels" 0.27 0.10 0.60 0.01
#pragma parameter BEAM_MAX "Beam width, bright pixels" 0.42 0.10 0.80 0.01
#pragma parameter BLOOM "Glow amount" 0.03 0.0 0.5 0.01
#pragma parameter MASK "Phosphor mask strength" 0.25 0.0 1.0 0.05
#pragma parameter GAIN "Brightness" 1.10 0.5 1.6 0.01
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
uniform sampler2D Texture;          // pass 3: glow
uniform sampler2D Pass1Texture;     // pass 1: linear-light picture
uniform vec2 Pass1InputSize;
uniform vec2 OutputSize;
uniform float SCAN;
uniform float BEAM_MIN;
uniform float BEAM_MAX;
uniform float BLOOM;
uniform float MASK;
uniform float GAIN;
uniform float OUT_GAMMA;
in vec2 vTexCoord;
out vec4 FragColor;

// Sharp-bilinear horizontal coordinate: pixels stay crisp, only the seam
// between two source pixels is antialiased over about one output pixel.
float sharp_x(float u, float kx) {
    float i = floor(u);
    float d = (u - i) - 0.5;
    float range = 0.5 - 0.5 / max(kx, 1.0);
    return (i + (d - clamp(d, -range, range)) * max(kx, 1.0) + 0.5) / Pass1InputSize.x;
}

void main() {
    vec2 src = Pass1InputSize;
    float kx_scale = OutputSize.x / src.x;
    float ky_scale = OutputSize.y / src.y;
    float x = sharp_x(vTexCoord.x * src.x, kx_scale);

    float ypos = vTexCoord.y * src.y - 0.5;
    float y0 = floor(ypos);
    float f = ypos - y0;                               // 0 on line y0, 1 on y0+1
    vec3 c0 = texture(Pass1Texture, vec2(x, (y0 + 0.5) / src.y)).rgb;
    vec3 c1 = texture(Pass1Texture, vec2(x, (y0 + 1.5) / src.y)).rgb;

    // Scanline beam: brighter pixels fatten the beam so highlights fill the
    // gap between lines, like a real tube.
    vec3 s0 = mix(vec3(BEAM_MIN), vec3(BEAM_MAX), clamp(c0, 0.0, 1.0));
    vec3 s1 = mix(vec3(BEAM_MIN), vec3(BEAM_MAX), clamp(c1, 0.0, 1.0));
    vec3 w0 = exp(-(f * f) / (2.0 * s0 * s0));
    vec3 w1 = exp(-((1.0 - f) * (1.0 - f)) / (2.0 * s1 * s1));
    vec3 beam = c0 * w0 + c1 * w1;

    // Without the beam: plain hard-edged lines (no vertical smearing).
    float aa = 0.5 / max(ky_scale, 1.0);
    vec3 plain = mix(c0, c1, smoothstep(0.5 - aa, 0.5 + aa, f));

    // Scanline gaps only read cleanly with >= ~3 output pixels per line.
    float k = clamp((ky_scale - 1.6) / 1.6, 0.0, 1.0) * SCAN;
    vec3 col = mix(plain, beam * 1.15, k) * GAIN;

    // Aperture grille, energy-preserving, faded out when too coarse.
    float km = clamp((kx_scale - 1.8) / 2.0, 0.0, 1.0) * MASK;
    float ph = mod(floor(gl_FragCoord.x), 3.0);
    vec3 m = vec3(1.0 - km);
    m[int(ph)] = 1.0;
    m /= (1.0 - km) * 2.0 / 3.0 + 1.0 / 3.0;
    col *= m;

    // Glow: light scattered in the glass, not affected by the mask.
    col += texture(Texture, vTexCoord).rgb * BLOOM;

    FragColor = vec4(pow(clamp(col, 0.0, 1.0), vec3(1.0 / OUT_GAMMA)), 1.0);
}
#endif
