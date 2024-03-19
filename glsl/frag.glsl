#version 330
precision mediump float;

in vec3  iNormal;
in vec3  vertPos;
in vec4  iColor;
in float visibility;

uniform vec3 cameraPos;
uniform vec3 skyColor;

uniform vec3 sunDir;
const vec3 lightColor = vec3(0.6, 0.6, 0.5);
const float screenGamma = 2.2; // Assume the monitor is calibrated to the sRGB color space

void main() {
	vec3 ambient = lightColor * 0.1;
	vec3 normal = normalize(iNormal);

	//vec3 sunDir = lightPos - vertPos;
	vec3 camDir = cameraPos - vertPos;

	// dist from camera
	float dist = length(camDir);

	//sunDir = normalize(sunDir);
	camDir = normalize(camDir);

	float lambrt = max(dot(sunDir, normal), 0.0);
	float camLambrt = max(dot(camDir, normal), 0.0) * exp2(-dist * 0.125);

	vec3 diff = lambrt * lightColor;
	vec3 camDiff = camLambrt * lightColor;

	diff += camDiff;

	vec3 viewDir = normalize(cameraPos - vertPos);
	vec3 reflectDir = reflect(-sunDir, normal);  

	float spec = pow(max(dot(viewDir, reflectDir), 0.0), 2);
	vec3 specular = 0.5 * spec * lightColor;

	vec3 colorLinear = (ambient + diff + specular) * iColor.rgb;
	vec3 colorCorrect = pow(colorLinear, vec3(1.0 / screenGamma));
	vec3 outColor = mix(skyColor, colorCorrect, visibility);
	gl_FragColor = vec4(outColor, iColor.a);
}
