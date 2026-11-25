// All GLSL shader sources (GLSL 330 core).
#pragma once

static const char* LIT_VS = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;
layout(location=3) in vec4 aColor;
uniform mat4 uMVP;
uniform mat4 uModel;
uniform mat3 uNormalMat;
uniform mat4 uShadowVP;
uniform float uTexScale;
out vec3 vWorld;
out vec3 vNormal;
out vec2 vUV;
out vec4 vColor;
out vec4 vShadow;
void main(){
    vec4 w = uModel * vec4(aPos,1.0);
    vWorld = w.xyz;
    vNormal = normalize(uNormalMat * aNormal);
    vUV = aUV * uTexScale;
    vColor = aColor;
    vShadow = uShadowVP * w;
    gl_Position = uMVP * vec4(aPos,1.0);
}
)";

static const char* LIT_FS = R"(
#version 330 core
in vec3 vWorld;
in vec3 vNormal;
in vec2 vUV;
in vec4 vColor;
in vec4 vShadow;
uniform sampler2D uTex;
uniform sampler2D uShadowMap;
uniform vec3 uSunDir;
uniform vec3 uSunColor;
uniform vec3 uCamPos;
uniform vec3 uAmbientTop;
uniform vec3 uAmbientBottom;
uniform vec3 uFogColor;
uniform float uFogDensity;
uniform float uSpecular;
uniform float uShininess;
uniform float uAO;
uniform float uMetal;
uniform float uEmissive;
uniform float uUseTex;
uniform vec3 uTint;
out vec4 fragColor;

float shadowPCF(vec4 sc){
    vec3 p = sc.xyz / sc.w * 0.5 + 0.5;
    if(p.z > 1.0 || p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0) return 1.0;
    float bias = max(0.0018, 0.004 * (1.0 - abs(vNormal.y)));
    float sum = 0.0;
    vec2 ts = vec2(1.0/2048.0);
    for(int x=-1;x<=1;x++)
    for(int y=-1;y<=1;y++){
        float d = texture(uShadowMap, p.xy + vec2(x,y)*ts).r;
        sum += (p.z - bias) > d ? 0.0 : 1.0;
    }
    return sum / 9.0;
}

void main(){
    vec3 base = vColor.rgb * uTint;
    if(uUseTex > 0.5) base *= texture(uTex, vUV).rgb;
    vec3 N = normalize(vNormal);
    vec3 L = normalize(uSunDir);
    vec3 V = normalize(uCamPos - vWorld);
    float ndl = max(dot(N,L), 0.0);
    float sh = shadowPCF(vShadow);
    // hemisphere ambient
    vec3 amb = mix(uAmbientBottom, uAmbientTop, N.y*0.5+0.5);
    vec3 H = normalize(L+V);
    float spec = pow(max(dot(N,H),0.0), uShininess) * uSpecular * ndl * sh;
    // metallic sky sheen
    vec3 R = reflect(-V, N);
    float fres = pow(1.0 - max(dot(N,V),0.0), 3.0);
    vec3 env = mix(vec3(0.55,0.62,0.70), vec3(0.85,0.90,1.0), R.y*0.5+0.5);
    vec3 col = base * (amb * uAO * vColor.a + uSunColor * ndl * sh * (0.55 + 0.45 * vColor.a))
             + uSunColor * spec
             + env * fres * uMetal * (0.35 + 0.65*sh)
             + base * uEmissive;
    float dist = length(uCamPos - vWorld);
    float fog = 1.0 - exp(-uFogDensity * uFogDensity * dist * dist);
    col = mix(col, uFogColor, fog);
    fragColor = vec4(col, 1.0);
}
)";

static const char* DEPTH_VS = R"(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 uMVP;
void main(){ gl_Position = uMVP * vec4(aPos,1.0); }
)";

static const char* DEPTH_FS = R"(
#version 330 core
void main(){}
)";

// Procedural sky: gradient, sun disc + halo, drifting cloud bands, haze at horizon.
static const char* SKY_VS = R"(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 uMVP;
out vec3 vDir;
void main(){
    vDir = aPos;
    vec4 p = uMVP * vec4(aPos,1.0);
    gl_Position = p.xyww; // depth = far
}
)";

static const char* SKY_FS = R"(
#version 330 core
in vec3 vDir;
uniform vec3 uSunDir;
uniform float uTime;
uniform float uDusk;
out vec4 fragColor;
float hash(vec2 p){ return fract(sin(dot(p, vec2(127.1,311.7)))*43758.5453); }
float noise(vec2 p){
    vec2 i=floor(p), f=fract(p);
    f=f*f*(3.0-2.0*f);
    return mix(mix(hash(i),hash(i+vec2(1,0)),f.x),
               mix(hash(i+vec2(0,1)),hash(i+vec2(1,1)),f.x),f.y);
}
float fbm(vec2 p){
    float v=0.0,a=0.5;
    for(int i=0;i<5;i++){ v+=a*noise(p); p*=2.03; a*=0.5; }
    return v;
}
void main(){
    vec3 d = normalize(vDir);
    float h = d.y;
    // sky gradient: bright hazy horizon -> deep blue zenith
    vec3 zen = mix(vec3(0.16,0.38,0.78), vec3(0.10,0.10,0.30), uDusk);
    vec3 hor = mix(vec3(0.75,0.85,0.95), vec3(0.95,0.45,0.22), uDusk);
    vec3 col = mix(hor, zen, pow(clamp(h*1.4+0.15,0.0,1.0),0.7));
    // sun disc + glare halo
    float sd = dot(d, normalize(uSunDir));
    col += mix(vec3(1.0,0.95,0.80), vec3(1.0,0.55,0.25), uDusk) * pow(max(sd,0.0), 1200.0) * 8.0;
    col += mix(vec3(1.0,0.92,0.70), vec3(1.0,0.50,0.22), uDusk) * pow(max(sd,0.0), 180.0) * (0.9 + 1.4*uDusk);
    col += mix(vec3(1.0,0.90,0.65), vec3(0.9,0.45,0.25), uDusk) * pow(max(sd,0.0), 8.0) * (0.22 + 0.4*uDusk);
    // clouds: only above horizon
    if(h > 0.01){
        vec2 cuv = d.xz / (h + 0.18);
        float cl = fbm(cuv*1.6 + vec2(uTime*0.006, uTime*0.002));
        float cm = smoothstep(0.55, 0.8, cl) * smoothstep(0.0, 0.25, h);
        col = mix(col, mix(vec3(1.0), vec3(1.0,0.55,0.40), uDusk), cm*(0.75 - 0.2*uDusk));
        float wisp = fbm(cuv*4.0 - vec2(uTime*0.01, 0.0));
        col += mix(vec3(1.0), vec3(0.9,0.5,0.35), uDusk) * smoothstep(0.7,0.95,wisp) * 0.06;
    }
    // horizon warm tint opposite sun
    col = mix(col, mix(vec3(0.85,0.78,0.70), vec3(0.65,0.30,0.28), uDusk), pow(1.0-abs(h),8.0)*0.35);
    fragColor = vec4(col,1.0);
}
)";

// Fullscreen triangle post shaders.
static const char* FSQ_VS = R"(
#version 330 core
layout(location=0) in vec2 aPos;
out vec2 vUV;
void main(){ vUV = aPos*0.5+0.5; gl_Position = vec4(aPos,0.0,1.0); }
)";

static const char* BRIGHT_FS = R"(
#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
out vec4 fragColor;
void main(){
    vec3 c = texture(uTex, vUV).rgb;
    float l = dot(c, vec3(0.299,0.587,0.114));
    float k = smoothstep(1.05, 1.6, l);
    fragColor = vec4(c*k, 1.0);
}
)";

static const char* BLUR_FS = R"(
#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
uniform vec2 uDir; // (1/w,0) or (0,1/h)
out vec4 fragColor;
void main(){
    vec3 c = texture(uTex,vUV).rgb * 0.227027;
    c += (texture(uTex,vUV+uDir*1.3846).rgb + texture(uTex,vUV-uDir*1.3846).rgb)*0.316216;
    c += (texture(uTex,vUV+uDir*3.2308).rgb + texture(uTex,vUV-uDir*3.2308).rgb)*0.070270;
    fragColor = vec4(c,1.0);
}
)";

static const char* COMPOSITE_FS = R"(
#version 330 core
in vec2 vUV;
uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform float uBloomAmt;
uniform float uDistort;   // barrel distortion strength (fisheye feel)
uniform float uVignette;
uniform float uExposure;
uniform float uFlash;     // bail/impact white flash
uniform float uTime;
out vec4 fragColor;
void main(){
    vec2 uv = vUV*2.0-1.0;
    float r2 = dot(uv,uv);
    uv *= 1.0 + uDistort*r2 + uDistort*0.4*r2*r2;
    uv = uv*0.5+0.5;
    vec3 col;
    if(uv.x<0.0||uv.x>1.0||uv.y<0.0||uv.y>1.0){ col = vec3(0.0); }
    else {
        // FXAA: edge-directed smoothing — the aliasing crawl on rails/ledges was
        // the single biggest cheapness tell, and MSAA costs 4x fill in software GL
        vec2 px = 1.0 / vec2(textureSize(uScene, 0));
        vec3 rgbNW = texture(uScene, uv + vec2(-1.0,-1.0)*px).rgb;
        vec3 rgbNE = texture(uScene, uv + vec2( 1.0,-1.0)*px).rgb;
        vec3 rgbSW = texture(uScene, uv + vec2(-1.0, 1.0)*px).rgb;
        vec3 rgbSE = texture(uScene, uv + vec2( 1.0, 1.0)*px).rgb;
        vec3 rgbM  = texture(uScene, uv).rgb;
        vec3 luma = vec3(0.299, 0.587, 0.114);
        float lNW = dot(rgbNW, luma), lNE = dot(rgbNE, luma);
        float lSW = dot(rgbSW, luma), lSE = dot(rgbSE, luma);
        float lM = dot(rgbM, luma);
        float lMin = min(lM, min(min(lNW, lNE), min(lSW, lSE)));
        float lMax = max(lM, max(max(lNW, lNE), max(lSW, lSE)));
        vec2 dir = vec2(-((lNW + lNE) - (lSW + lSE)), ((lNW + lSW) - (lNE + lSE)));
        float dirReduce = max((lNW + lNE + lSW + lSE) * 0.03125, 0.0078125);
        float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
        dir = clamp(dir * rcpDirMin, -8.0, 8.0) * px;
        vec3 rgbA = 0.5 * (texture(uScene, uv + dir * (1.0/3.0 - 0.5)).rgb +
                           texture(uScene, uv + dir * (2.0/3.0 - 0.5)).rgb);
        vec3 rgbB = rgbA * 0.5 + 0.25 * (texture(uScene, uv + dir * -0.5).rgb +
                                         texture(uScene, uv + dir *  0.5).rgb);
        float lB = dot(rgbB, luma);
        col = (lB < lMin || lB > lMax) ? rgbA : rgbB;
        // slight chromatic aberration toward the frame edge (lens feel)
        vec2 ca = (uv - 0.5) * (0.0005 + r2 * 0.0016);
        col.r = mix(col.r, texture(uScene, uv + ca).r, 0.6);
        col.b = mix(col.b, texture(uScene, uv - ca).b, 0.6);
        col += texture(uBloom, uv).rgb * uBloomAmt;
    }
    // filmic-ish tonemap
    col = vec3(1.0) - exp(-col * uExposure);
    col = pow(col, vec3(0.85));
    // skate-video grade: contrast curve, saturation, slight warmth
    float lum = dot(col, vec3(0.299, 0.587, 0.114));
    col = mix(vec3(lum), col, 1.18);
    col = mix(col, col * col * (3.0 - 2.0 * col), 0.35);
    col *= vec3(1.02, 1.0, 0.97);
    // vignette
    float vg = 1.0 - uVignette*smoothstep(0.3, 1.6, r2);
    col *= vg;
    // subtle film grain (skate-video feel)
    float gn = fract(sin(dot(uv * vec2(1187.3, 993.1) + mod(uTime, 100.0) * 77.7,
                             vec2(12.9898, 78.233))) * 43758.5453);
    col += (gn - 0.5) * 0.028;
    col += vec3(uFlash);
    fragColor = vec4(col,1.0);
}
)";

static const char* LINE_VS = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec4 aColor;
uniform mat4 uMVP;
out vec4 vColor;
void main(){ vColor = aColor; gl_Position = uMVP * vec4(aPos,1.0); }
)";

static const char* LINE_FS = R"(
#version 330 core
in vec4 vColor;
out vec4 fragColor;
void main(){ fragColor = vColor; }
)";

static const char* POINT_VS = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec4 aColor;
layout(location=2) in float aSize;
uniform mat4 uVP;
out vec4 vColor;
void main(){
    vec4 c = uVP * vec4(aPos,1.0);
    gl_Position = c;
    gl_PointSize = clamp(aSize * 320.0 / max(c.w, 0.1), 1.0, 96.0);
    vColor = aColor;
}
)";

static const char* POINT_FS = R"(
#version 330 core
in vec4 vColor;
out vec4 fragColor;
void main(){
    vec2 d = gl_PointCoord*2.0-1.0;
    float r2 = dot(d,d);
    if(r2 > 1.0) discard;
    float a = (1.0-r2) * vColor.a;
    fragColor = vec4(vColor.rgb, a);
}
)";
