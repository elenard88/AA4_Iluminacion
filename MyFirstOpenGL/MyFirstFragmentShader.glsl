#version 440 core

uniform vec2 windowSize;
uniform sampler2D textureSampler;

uniform vec3 sunPosition;
uniform vec3 moonPosition;

uniform vec3 sunColor;
uniform vec3 moonColor;
uniform vec3 ambientColor;

uniform float sunEnabled;
uniform float moonEnabled;
uniform float minimumDiffuseLight;

uniform int renderLightObject;
uniform vec3 objectColor;

in vec2 uvsFragmentShader;
in vec3 normalsFragmentShader;
in vec4 primitivePosition;

out vec4 fragColor;

void main() {

	if (renderLightObject == 1) {
		fragColor = vec4(objectColor, 1.0);
		return;
	}

	vec2 adjustedTexCoord = vec2(uvsFragmentShader.x, 1.0 - uvsFragmentShader.y);
	vec4 baseColor = texture(textureSampler, adjustedTexCoord);

	vec3 normalDirection = normalize(normalsFragmentShader);

	vec3 sunDirection = normalize(sunPosition - primitivePosition.xyz);
	vec3 moonDirection = normalize(moonPosition - primitivePosition.xyz);

	float sunDiffuse = max(dot(normalDirection, sunDirection), 0.0) * sunEnabled;
	float moonDiffuse = max(dot(normalDirection, moonDirection), 0.0) * moonEnabled;

	vec3 directLight = (sunColor * sunDiffuse) + (moonColor * moonDiffuse);
	vec3 finalLight = ambientColor + directLight;

	finalLight = max(finalLight, vec3(minimumDiffuseLight));

	fragColor = vec4(baseColor.rgb * finalLight, baseColor.a);
}