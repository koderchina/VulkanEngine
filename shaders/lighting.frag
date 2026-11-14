#version 450

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 outColor;

struct PointLight
{
  vec4 position;
  vec4 color;
};

// Must match GlobalUniformBufferObject exactly (same as simple_shader.frag).
layout(set = 0, binding = 0) uniform GlobalUniformBufferObject
{
  mat4 projectionMatrix;
  mat4 viewMatrix;
  mat4 inverseViewMatrix;
  vec4 ambientLightColor; // w is intensity
  PointLight pointLights[10];
  int numLights;
} ubo;

// G-buffer written by gbuffer.frag
layout(set = 1, binding = 0) uniform sampler2D gAlbedo;
layout(set = 1, binding = 1) uniform sampler2D gNormal;
layout(set = 1, binding = 2) uniform sampler2D gPosition;

void main()
{
  vec4 albedoSample = texture(gAlbedo, uv);
  vec4 normalSample = texture(gNormal, uv);

  // Normal alpha is 0 where no geometry was drawn: show the cleared background.
  if (normalSample.a == 0.0)
  {
    outColor = vec4(albedoSample.rgb, 1.0);
    return;
  }

  vec3 albedo = albedoSample.rgb;
  vec3 surfaceNormal = normalize(normalSample.xyz);
  vec3 positionWorld = texture(gPosition, uv).xyz;

  vec3 diffuseLight = ubo.ambientLightColor.xyz * ubo.ambientLightColor.w;
  vec3 specularLight = vec3(0.0);

  vec3 cameraPosWorld = ubo.inverseViewMatrix[3].xyz;
  vec3 viewDirection = normalize(cameraPosWorld - positionWorld);

  for (int i = 0; i < ubo.numLights; i++)
  {
    PointLight pointLight = ubo.pointLights[i];
    vec3 directionToLight = pointLight.position.xyz - positionWorld;
    float attenuation = 1.0 / dot(directionToLight, directionToLight); // distance squared
    directionToLight = normalize(directionToLight);

    float cosAngIncidence = max(dot(surfaceNormal, directionToLight), 0.0);
    vec3 intensity = pointLight.color.xyz * pointLight.color.w * attenuation;

    diffuseLight += intensity * cosAngIncidence;

    vec3 halfAngle = normalize(directionToLight + viewDirection);
    float blinnTerm = clamp(dot(surfaceNormal, halfAngle), 0.0, 1.0);
    blinnTerm = pow(blinnTerm, 32.0);
    specularLight += intensity * blinnTerm;
  }

  // Vertex color and texture are already folded into albedo.
  outColor = vec4(albedo * (diffuseLight + specularLight), 1.0);
}
