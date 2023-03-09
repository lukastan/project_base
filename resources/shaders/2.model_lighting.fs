#version 330 core
out vec4 FragColor;
out vec4 BrightColor;

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
} fs_in;

float near = 0.1;
float far = 100.0;
float LinearizeDepth(float depth) {
    float z = depth * 2.0 - 1.0;
    return (2.0 * near * far) / (far + near - z * (far - near));
}

struct PointLight {
    vec3 position;

    vec3 specular;
    vec3 diffuse;
    vec3 ambient;

    float constant;
    float linear;
    float quadratic;
};

struct Material {
    sampler2D texture_diffuse1;
    sampler2D texture_specular1;

    float shininess;
};

vec3 gridSamplingDisk[20] = vec3[]
(
   vec3(1, 1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1, 1,  1),
   vec3(1, 1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1, 1, -1),
   vec3(1, 1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1, 1,  0),
   vec3(1, 0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1, 0, -1),
   vec3(0, 1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0, 1, -1)
);
uniform samplerCube depthMap;
uniform float far_plane;
uniform bool shadows;

uniform PointLight pointLight;
uniform Material material;
uniform bool shouldDiscard;
uniform vec3 viewPosition;
uniform bool blinn;

float ShadowCalculation(vec3 fragPos)
{
    vec3 fragToLight = fragPos - pointLight.position;
    float currentDepth = length(fragToLight);
    float shadow = 0.0;
    float bias = 0.15;
    int samples = 20;
    float viewDistance = length(viewPosition - fragPos);
    float diskRadius = (1.0 + (viewDistance / far_plane)) / 25.0;
    for(int i = 0; i < samples; ++i)
    {
        float closestDepth = texture(depthMap, fragToLight + gridSamplingDisk[i] * diskRadius).r;
        closestDepth *= far_plane;   // undo mapping [0;1]
        if(currentDepth - bias > closestDepth)
            shadow += 1.0;
    }
    shadow /= float(samples);
    return shadow;
}

void main()
{
    vec4 color = texture(material.texture_diffuse1, fs_in.TexCoords);
    // depth
    float depth = LinearizeDepth(gl_FragCoord.z) / far;
    //ambient
    vec3 ambient = pointLight.ambient * color.rgb;
    // diffuse
    vec3 normal = normalize(fs_in.Normal);
    vec3 lightDir = normalize(pointLight.position - fs_in.FragPos);
    float diff = max(dot(lightDir, normal), 0.0);
    vec3 diffuse = diff * color.rgb;
    //specular
    vec3 viewDir = normalize(viewPosition - fs_in.FragPos);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = 0.0;
    //blinn set and check
    vec3 halfwayDir = normalize(lightDir + viewDir);
    if(blinn)
        spec = pow(max(dot(normal, halfwayDir), 0.0), material.shininess);
    else
        spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);

    vec3 specular = vec3(0.3) * spec;
    //attenuation
    float distance = length(pointLight.position - fs_in.FragPos);
    float attenuation = 1 / (pointLight.constant + pointLight.linear * distance + pointLight.quadratic * (distance * distance));
    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;
    // calculate shadow
    float shadow = shadows ? ShadowCalculation(fs_in.FragPos) : 0.0;

    if(shouldDiscard)
        discard;
    if(color.a < 0.1)
        discard;

    // check whether result is higher than some threshold, if so, output as bloom threshold color
    float brightness = dot(ambient + (diffuse + specular), vec3(0.2126, 0.7152, 0.0722));
    if(brightness > 1.0)
        BrightColor = vec4(ambient + (diffuse + specular), 1.0);
    else
        BrightColor = vec4(0.0, 0.0, 0.0, 1.0);
    // to get a weird effect, put depth before the closing bracket on the left
    FragColor = vec4(ambient + (1.0 - shadow) * (diffuse + specular) + depth, 1.0);
}