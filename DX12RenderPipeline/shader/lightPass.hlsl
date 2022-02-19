#include "Lighting.hlsl"
#define PREFILTER_MIP_LEVEL 5
struct VSInput
{
	float2 Position : POSITION;
	float2 Texcoord  : TEXCOORD0;
};

struct PSInput
{
	float4 PositionH : SV_POSITION;
	float2 Texcoord  : TEXCOORD0;
};

cbuffer cbLights : register(b0) {
	float4 gEyePos;
	DirLight gDirLights[MAX_DIR];
	PointLight gPointLights[MAX_POINT];
}

Texture2D gAlbedoRoughness								: register(t0);
Texture2D gNormalMetallic								: register(t1);
Texture2D gPointOcclusion								: register(t2);

TextureCube gRadianceDiffuse							: register(t3);  //Prefiltered ambient diffuse irradiance
Texture2D gLUT											: register(t4);
TextureCube gPrefilteredMap[PREFILTER_MIP_LEVEL]		: register(t5);

SamplerState gsamPointWrap        : register(s0);
SamplerState gsamPointClamp       : register(s1);
SamplerState gsamLinearWrap       : register(s2);
SamplerState gsamLinearClamp      : register(s3);
SamplerState gsamAnisotropicWrap  : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);


PSInput VSMain(VSInput input) {

	PSInput output;
	output.PositionH = float4(input.Position, 0.5f, 1.0f);
	output.Texcoord = input.Texcoord;
	return output;
}

float3 PrefilteredColor(float3 viewDir, float3 normal, float roughness)
{
	float roughnessLevel = roughness * PREFILTER_MIP_LEVEL;
	int fl = floor(roughnessLevel);
	int cl = ceil(roughnessLevel);
	float3 R = reflect(-viewDir, normal);
	float3 flSample = gPrefilteredMap[fl].Sample(gsamLinearWrap, R).rgb;
	float3 clSample = gPrefilteredMap[cl].Sample(gsamLinearWrap, R).rgb;
	float3 prefilterColor = lerp(flSample, clSample, (roughnessLevel - fl));
	return prefilterColor;
}

float2 BrdfLUT(float3 normal, float3 viewDir, float roughness)
{
	float NoV = dot(normal, viewDir);
	NoV = max(NoV, 0.0f);
	float2 uv = float2(NoV, roughness);
	return gLUT.Sample(gsamLinearWrap, uv).xy;
}

float4 PSMain(PSInput input) : SV_TARGET
{
	float3 albedo = gAlbedoRoughness.Sample(gsamLinearWrap,input.Texcoord).xyz;
	float roughness = gAlbedoRoughness.Sample(gsamLinearWrap, input.Texcoord).w;
	float3 normal = normalize(gNormalMetallic.Sample(gsamLinearWrap, input.Texcoord).xyz);
	float metallic = gNormalMetallic.Sample(gsamLinearWrap, input.Texcoord).w;
	float3 position = gPointOcclusion.Sample(gsamLinearWrap, input.Texcoord).xyz;
	float  occlusion = gPointOcclusion.Sample(gsamLinearWrap, input.Texcoord).w;
	float3 viewDir = normalize(gEyePos.xyz - position);

	float3 F0 = float3(0.04, 0.04, 0.04);
	Surface surface;
	surface.DiffuseAlbedo = albedo;
	surface.Metallic = metallic;
	surface.Roughness = roughness;
	surface.FresnelR0 = lerp(F0, albedo, metallic);

	float3 dirLight = ComputeLighting(gDirLights, gPointLights, surface, position,
		normal, viewDir);
	
	float3 irradianceDiffuse = gRadianceDiffuse.Sample(gsamLinearWrap, normal).rgb;
	float3 prefilteredColor = PrefilteredColor(viewDir, normal, roughness);
	float2 brdf = BrdfLUT(normal, viewDir, roughness);
	float3 envLight = ComputeEnvLighting(irradianceDiffuse, prefilteredColor, brdf, surface, normal, viewDir);
	
	float3 result = HDRToneMapping(envLight + dirLight);
	return float4(result, 1.0f);

}