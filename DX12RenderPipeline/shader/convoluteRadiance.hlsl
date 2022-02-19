#define PI 3.14159265359

struct VSInput {
    float3 Position : POSITION;
};

struct PSInput {
    float4 PositionH : SV_POSITION;
    float3 Texcoord  : POSITION;
};

TextureCube gCubeMap : register(t0);
SamplerState gsamPointWrap        : register(s0);
SamplerState gsamPointClamp       : register(s1);
SamplerState gsamLinearWrap       : register(s2);
SamplerState gsamLinearClamp      : register(s3);
SamplerState gsamAnisotropicWrap  : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

cbuffer cbTransform : register(b0) {
    float4x4 gViewProj;
}

PSInput VSMain(VSInput input)
{
    PSInput output;
    output.Texcoord = float3(input.Position);
    output.PositionH = mul(float4(input.Position, 1.0f), gViewProj).xyww;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float3 irradiance = float3(0.0f, 0.0f, 0.0f);

    float3 normal = normalize(input.Texcoord);
    float3 up = float3(0.0, 1.0, 0.0);
    float3 right = normalize(cross(up, normal)); //这里是有必要normalize的，叉乘两单位向量可不一定是单位向量.....
    up = normalize(cross(normal, right));
    
    float sampleDelta = 0.025f;
    float numSamples = 0.0f;
    for (float phi = 0.0f; phi < 2.0f * PI; phi += sampleDelta)
    {
        for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
        {
            // spherical to cartesian (in tangent space)
            float3 tangentSample = float3(sin(theta) * cos(phi),  sin(theta) * sin(phi), cos(theta));
            // tangent space to world
            float3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;

            irradiance += gCubeMap.Sample(gsamLinearWrap, sampleVec).rgb * cos(theta) * sin(theta);

            numSamples++;
        }
    }
    irradiance = PI * irradiance * (1.0f / numSamples); //根据蒙特卡罗方法 此处应为pi2, 但与分母PI约掉一个
    return float4(irradiance, 1.0f);
}