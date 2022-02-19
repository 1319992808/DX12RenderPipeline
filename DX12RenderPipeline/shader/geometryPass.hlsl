//Geometry pass
//No lighting

struct VSInput
{
    float3 PositionL : POSITION;
    float3 NormalL   : NORMAL;
    float2 Texcoord  : TEXCOORD0;
};

struct PSInput
{
    float4 PositionH : SV_POSITION;
    float3 PositionW : POSITION;
    float3 NormalW   : NORMAL;
    float2 Texcoord  : TEXCOORD0;
};

struct PSOutput
{
    float4 AlbedoRoughness   : SV_TARGET0;
    float4 NormalMetallic   : SV_TARGET1;
    float4 PositionOcclusion : SV_TARGET2;
};

struct MaterialData
{
    float4   DiffuseAlbedo;
    float    Metallic;
    float    Roughness;
    float4x4 MatTransform;
    uint     DiffuseMapIndex;
    uint     MatPad0;
    uint     MatPad1;
    uint     MatPad2;
    uint     MatPad3;
    uint     MatPad4;
};

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gTexTransform;
    uint gMaterialIndex;
    uint gObjPad0;
    uint gObjPad1;
    uint gObjPad2;
};

cbuffer cbPass : register(b1) //Some values are not used, don't mind for now.
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float3   gEyePosW;
    float    cbPerObjectPad1;
    float2   gRenderTargetSize;
    float2   gInvRenderTargetSize;
    float    gNearZ;
    float    gFarZ;
    float    gTotalTime;
    float    gDeltaTime;
}

Texture2D gDiffuseMap[2] : register(t0);
StructuredBuffer<MaterialData> gMaterialData : register(t0, space1);

SamplerState gsamPointWrap        : register(s0);
SamplerState gsamPointClamp       : register(s1);
SamplerState gsamLinearWrap       : register(s2);
SamplerState gsamLinearClamp      : register(s3);
SamplerState gsamAnisotropicWrap  : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);


PSInput VSMain(VSInput input)
{
    PSInput output;
    MaterialData matData = gMaterialData[gMaterialIndex];
    float4 posW = mul(float4(input.PositionL, 1.0), gWorld);
    output.PositionW = posW.xyz;
    output.PositionH = mul(posW, gViewProj);
    output.NormalW = mul(input.NormalL, (float3x3)gWorld);  //Uniform scale
    output.Texcoord = input.Texcoord;
    float4 texC = mul(float4(input.Texcoord, 0.0f, 1.0f), gTexTransform);
    output.Texcoord = mul(texC, matData.MatTransform).xy;
    return output;
}

//Multiple render targets
PSOutput PSMain(PSInput input)
{
    PSOutput output;

    MaterialData matData = gMaterialData[gMaterialIndex];
    
    float3 diffuseAlbedo = matData.DiffuseAlbedo.rgb;
    uint diffuseTexIndex = matData.DiffuseMapIndex;
    float4 texDiffuseAlbedo = gDiffuseMap[diffuseTexIndex].Sample(gsamLinearWrap, input.Texcoord);
    diffuseAlbedo *= texDiffuseAlbedo.xyz;
    float3 normal = normalize(input.NormalW);
    float metallic = matData.Metallic; //No need to record three channel for fresnel
    float  roughness = matData.Roughness;

    output.AlbedoRoughness = float4(diffuseAlbedo, roughness);
    output.NormalMetallic = float4(normal, metallic);
    output.PositionOcclusion = float4(input.PositionW, 1.0f); //Later implement occlusion.
    return output;
}
