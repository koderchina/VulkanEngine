#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragPosWorldSpace;
layout(location = 2) in vec3 fragNormalWorldSpace;
layout(location = 3) in vec2 texCoord;

layout(location = 0) out vec4 outColor;

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

/*
The fragment shader runs on per-fragment basis. Exact 
fragments it will affect are determined in the 
rasterization stage.
*/
void main() {
  // r g b a
  vec3 diffuseLight = ubo.ambientLightColor.xyz * ubo.ambientLightColor.w;
  vec3 specularLight = vec3(0.0);
  vec3 surfaceNormal = normalize(fragNormalWorldSpace);

  vec3 cameraPosWorld = ubo.inverseViewMatrix[3].xyz;
  vec3 viewDirection = normalize(cameraPosWorld - fragPosWorldSpace);

  for(int i = 0; i < ubo.numLights; i++)
  {
    PointLight pointLight = ubo.pointLights[i];
    vec3 directionToLight = pointLight.position.xyz - fragPosWorldSpace;
    float attenuation = 1.0 / dot(directionToLight, directionToLight); // distance squared
    directionToLight = normalize(directionToLight);

    float cosAngIncidence = max(dot(surfaceNormal, directionToLight), 0);
    vec3 intensity = pointLight.color.xyz * pointLight.color.w * attenuation;

    diffuseLight += intensity * cosAngIncidence;

    vec3 halfAngle = normalize(directionToLight + viewDirection);
    float blinnTerm = dot(surfaceNormal, halfAngle);
    blinnTerm = clamp(blinnTerm, 0, 1);
    blinnTerm = pow(blinnTerm, 32.0); // higher values -> higher lighting
    specularLight += intensity * blinnTerm;
  }

  outColor = texture(texSampler, texCoord) * vec4(diffuseLight * fragColor + specularLight * fragColor, 1.0);
}