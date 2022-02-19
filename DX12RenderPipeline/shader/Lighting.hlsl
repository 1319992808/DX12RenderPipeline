#define DIRECT_COUNT 3
#define POINT_COUNT 1
#define SPOT_COUNT 0
#define MAX_DIR 4
#define MAX_POINT 4
#define PI 3.1415926575
#define GAMMA 2.2
#define INV_GAMMA 0.454545455
struct DirLight{

    float3 Strength;
    float padding0;
    float3 Direction;
    float padding1;
};

struct PointLight {

    float3 Strength;
    float padding0;
    float3 Position;
    float padding1;
};

struct SpotLight{

};

struct Surface {

    float3 DiffuseAlbedo;
    float3 FresnelR0;
    float Metallic; //Take metallic into consideration
    float Roughness;
};

//To be implemented
float CalcAttenuation(float d, float falloffStart, float falloffEnd){
    // Linear falloff. No fall off for now
    return saturate((falloffEnd - d) / (falloffEnd - falloffStart));
}

float3 FresnelSchlick(float HoV, float3 F0){

    return F0 + (1.0 - F0) * pow(1.0 - HoV, 5.0);
}
float3 FresnelRoughness(float NoV, float3 F0, float Roughness) {

    float3 delta = max(float3(1.0 - Roughness, 1.0 - Roughness, 1.0 - Roughness), F0);
    return F0 + (delta - F0) * pow((1.0 - NoV), 5.0);
}
//Integration of projected area == 1
float NDF_GGXTR(float NoH, float roughness){
    float a = roughness;
    float a2 = a * a;
    float NoH2 = NoH * NoH;
    float numerator = a2;
    float denominator = (NoH2 * (a2 - 1.0) + 1.0);
    denominator = PI * denominator * denominator;
    return numerator / denominator;
}

float Schlick_GGX(float NoV, float roughness){
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float denominator = NoV * (1.0 - k) + k;
    return NoV / denominator;
}

//light->surface surface->light occlusion twice
float Smith_G(float NoV, float NoL, float roughness) {
    float ggx1 = Schlick_GGX(NoV, roughness);
    float ggx2 = Schlick_GGX(NoL, roughness);
    return ggx1 * ggx2;
}

float3 ComputeEnvLighting(float3 irradiance, float3 prefilteredColor, float2 brdf, Surface surface, float3 normal, float3 viewDir) {
   
    float NoV = max(dot(viewDir, normal), 0.0f);
    float3 F = FresnelRoughness(NoV, surface.FresnelR0, surface.Roughness);
    float3 diffuse = (float3(1.0f, 1.0f, 1.0f) - F) * (1.0f - surface.Metallic) * surface.DiffuseAlbedo * irradiance;
    float3 specular = prefilteredColor * (F * brdf.x + float3(brdf.y, brdf.y, brdf.y));
    return diffuse + specular;
}

float3 ComputeDirectionalLight(DirLight L, Surface surface, float3 normal, float3 viewDir){

    float3 lightDir = -L.Direction;
    float3 halfVec = normalize(lightDir + viewDir);

    float NoL = max(dot(lightDir, normal), 0.0f);
    float NoV = max(dot(viewDir, normal), 0.0f);
    float NoH = max(dot(halfVec, normal), 0.0f);
    float HoV = max(dot(halfVec, viewDir), 0.0f);

    float  N = NDF_GGXTR(NoH, surface.Roughness);
    float  G = Smith_G(NoV, NoL, surface.Roughness);
    float3 F = FresnelSchlick(HoV, surface.FresnelR0);

    float3 numerator = N * G * F;
    float  denominator = max(4.0 * NoV * NoL, 0.001);
    float3 specularTerm = numerator / denominator;
    float3 diffuseTerm = (float3(1.0, 1.0, 1.0) - F) * (1.0 - surface.Metallic) * surface.DiffuseAlbedo / PI;
    float3 radianceProj = L.Strength * NoL;
    
    return (diffuseTerm + specularTerm)* radianceProj;

}

float3 HDRToneMapping(float3 hdrColor) {
    float3 mappedColor = hdrColor / (hdrColor + float3(1.0f, 1.0f, 1.0f));
    mappedColor = pow(mappedColor, INV_GAMMA);
    return mappedColor;
}

float3 ComputePointLight(PointLight L, Surface surface, float3 pos, float3 normal, float3 viewDir){

    DirLight light;
    light.Strength = L.Strength;
    light.Direction = normalize(pos - L.Position);
    return ComputeDirectionalLight(light, surface, normal, viewDir);
}

float3 ComputeLighting(DirLight dirLights[MAX_DIR], PointLight pointLights[MAX_POINT], Surface surface,
    float3 pos, float3 normal, float3 viewDir){

    float3 result = 0.0f;

    int i = 0;

#if (DIRECT_COUNT > 0)
    for (i = 0; i < DIRECT_COUNT; ++i)
    {
        result += ComputeDirectionalLight(dirLights[i], surface, normal, viewDir);
    }
#endif

#if (POINT_COUNT > 0)
    for (i = 0; i < POINT_COUNT; ++i)
    {
        result += ComputePointLight(pointLights[i], surface, pos, normal, viewDir);
    }
#endif

    return result;
}

