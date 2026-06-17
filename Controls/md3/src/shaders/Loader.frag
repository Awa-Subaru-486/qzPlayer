#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;

    float lobes1;
    float lobes2;
    float mixFactor;
    vec4  uColor;
    float uAspectRatio;
};

void main() {
    vec2 uv = qt_TexCoord0 - 0.5;

    float aspect = (uAspectRatio > 0.0) ? uAspectRatio : 1.0;
    uv.x *= aspect;

    float len = length(uv);
    float angle = atan(uv.y, uv.x) + 3.14159265359;

    float baseR = 0.4;
    float varR  = 0.07;

    float r1 = baseR + varR * sin(lobes1 * angle);
    float r2 = baseR + varR * sin(lobes2 * angle);
    float r  = mix(r1, r2, mixFactor);

    float d = len - r;
    float aa_width = fwidth(len);
    float alpha = 1.0 - smoothstep(-aa_width, aa_width, d);

    fragColor = uColor * alpha * qt_Opacity;
}
