#version 330 core
out vec4 FragColor;

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

uniform PointLight pointLight;
uniform Material material;
uniform bool shouldDiscard;
uniform vec3 viewPosition;
uniform bool blinn;

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

    vec3 halfwayDir = normalize(lightDir + viewDir);
    // two options just to check if blinn does indeed work
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

    if(shouldDiscard)
        discard;
    if(color.a < 0.1)
        discard;

    FragColor = vec4(ambient + diffuse + specular + depth/3, 1.0);
}

// // calculates the color when using a point light.
// vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir)
// {
//     vec3 lightDir = normalize(light.position - fragPos);
//     // diffuse shading
//     float diff = max(dot(normal, lightDir), 0.0);
//     // specular shading
//     vec3 reflectDir = reflect(-lightDir, normal);
//     float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
//     // attenuation
//     float distance = length(light.position - fragPos);
//     float attenuation = 0.333 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));
//     // combine results
//     vec3 ambient = light.ambient * vec3(texture(material.texture_diffuse1, TexCoords));
//     vec3 diffuse = light.diffuse * diff * vec3(texture(material.texture_diffuse1, TexCoords));
//     vec3 specular = light.specular * spec * vec3(texture(material.texture_specular1, TexCoords).xxx);
//     ambient *= attenuation;
//     diffuse *= attenuation;
//     specular *= attenuation;
//     return (ambient + diffuse + specular);
// }