#version 450
/*
This main function will be executed for each vertex. It will
get each px from the input assembler stage, perform transforms
and output the pixel coordinates.
*/

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 uv;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragPosWorldSpace;
layout(location = 2) out vec3 fragNormalWorldSpace;
layout(location = 3) out vec2 texCoord;

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

layout(push_constant) uniform Push 
{
  mat4 modelMatrix; // projection * view * model
  mat4 normalMatrix;
} push;

void main()
{
  // order of matrix multiplication matters
  vec4 positionWorld = push.modelMatrix * vec4(position, 1.0);
  gl_Position = ubo.projectionMatrix  * ubo.viewMatrix * positionWorld;

  fragNormalWorldSpace = normalize(mat3(push.normalMatrix) * normal);
  fragPosWorldSpace = positionWorld.xyz;
  fragColor = color;
  texCoord = uv;

}