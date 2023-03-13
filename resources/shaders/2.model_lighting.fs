#version 330 core
layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec4 BrightColor;

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

uniform int NR_LIGHTS;
uniform PointLight pointLights[5];
uniform Material material;
uniform bool shouldDiscard;
uniform vec3 viewPosition;
uniform bool blinn;

float ShadowCalculation(vec3 fragPos)
{
    vec3 fragToLight = fragPos - pointLights[0].position;
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
    vec3 lighting = vec3(0.0);
    // color
    vec4 color = texture(material.texture_diffuse1, fs_in.TexCoords);
    // calculate shadow
    float shadow = shadows ? ShadowCalculation(fs_in.FragPos) : 0.0;
    // depth
    float depth = LinearizeDepth(gl_FragCoord.z) / far;
    for(int i=0; i<NR_LIGHTS; i++) {
        //ambient
        vec3 ambient = pointLights[i].ambient * color.rgb;
        // diffuse
        vec3 normal = normalize(fs_in.Normal);
        vec3 lightDir = normalize(pointLights[i].position - fs_in.FragPos);
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
        float distance = length(pointLights[i].position - fs_in.FragPos);
        float attenuation = 1 / (pointLights[i].constant + pointLights[i].linear * distance + pointLights[i].quadratic * (distance * distance));
        ambient *= attenuation;
        diffuse *= attenuation;
        specular *= attenuation;

        if(i == 0)
            lighting += ambient + (1.0 - shadow) * (diffuse + specular);
        else
            lighting += 1.5 * ambient + (diffuse + specular);
    }

    // check whether result is higher than some threshold, if so, output as bloom threshold color
    float brightness = dot(lighting, vec3(0.2126, 0.7152, 0.0722));
    if(brightness > 1.0)
        BrightColor = vec4(lighting, 1.0);
    else
        BrightColor = vec4(0.0, 0.0, 0.0, 1.0);

    if(color.a < 0.1 || shouldDiscard)
        discard;
    // to get a weird effect, put depth before the closing bracket on the left
    FragColor = vec4(lighting + depth, 1.0);
}