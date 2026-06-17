#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 1) in vec2 coord;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 size;
    vec4 radii;
    vec4 u_transform;
    float u_imgScale;
};

layout(binding = 1) uniform sampler2D source;

float roundedBoxSDF(vec2 centerPos, vec2 halfSize, float r) {
    return length(max(abs(centerPos) - halfSize + r, 0.0)) - r;
}

void main() {
    vec2 uv = qt_TexCoord0;

    // Apply fill mode transform
    uv = uv * u_transform.xy + u_transform.zw;

    // Apply scale
    uv = (uv - 0.5) / u_imgScale + 0.5;

    vec4 texColor = texture(source, uv);

    // Compute distance to rounded rect
    vec2 halfSize = size * 0.5;
    vec2 centerPos = coord - halfSize;

    float tl = radii.x;
    float tr = radii.y;
    float bl = radii.z;
    float br = radii.w;

    // Select radius for current quadrant
    float r;
    if (centerPos.x < 0.0) {
        r = centerPos.y < 0.0 ? bl : tl;
    } else {
        r = centerPos.y < 0.0 ? br : tr;
    }

    float dist = roundedBoxSDF(centerPos, halfSize, r);
    float aa = fwidth(dist);
    float alpha = 1.0 - smoothstep(-aa, aa, dist);

    fragColor = texColor * alpha * qt_Opacity;
}
