#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragPosWorldSpace;
layout(location = 2) in vec3 fragNormalWorldSpace;
layout(location = 3) in vec2 texCoord;

layout(location = 0) out vec4 albedo;
layout(location = 1) out vec4 normal;
layout(location = 2) out vec4 position;

struct PointLight
{
  vec4 position;
  vec4 color;
};

layout(set = 0, binding = 0) uniform GlobalUniformBufferObject
{
  mat4 projectionMatrix;
  mat4 viewMatrix;
  mat4 inverseViewMatrix;
  vec4 ambientLightColor; // w is intensity
  PointLight pointLights[10];
  int numLights;
} ubo;

layout(set = 1, binding = 0) uniform sampler2D texSampler;

layout(push_constant) uniform Push 
{
  mat4 modelMatrix; // projection * view * model
  mat4 normalMatrix;
} push;

void main()
{
    albedo = vec4(texture(texSampler, texCoord).rgb * fragColor, 1.0);
    normal = vec4(normalize(fragNormalWorldSpace), 1.0);
    position = vec4(fragPosWorldSpace, 1.0);
}