#version 450

// Fullscreen triangle: no vertex buffer, positions come from gl_VertexIndex.
// Vertices 0,1,2 -> (-1,-1), (3,-1), (-1,3). The triangle covers the whole
// [-1,1] square (the rest is clipped), and the UVs (0,0), (2,0), (0,2)
// interpolate to exactly 0..1 across the visible screen.

layout(location = 0) out vec2 uv;

void main()
{
    uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}
