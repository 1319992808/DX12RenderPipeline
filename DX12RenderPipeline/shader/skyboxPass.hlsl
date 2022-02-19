
struct VSInput{
    float3 Position : POSITION;
};

struct PSInput{
    float4 PositionH : SV_POSITION;
    float3 Texcoord  : POSITION;
};

SamplerState gsamPointWrap        : register(s0);
SamplerState gsamPointClamp       : register(s1);
SamplerState gsamLinearWrap       : register(s2);
SamplerState gsamLinearClamp      : register(s3);
SamplerState gsamAnisotropicWrap  : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

TextureCube gCubeMap : register(t0);

cbuffer cbTransform : register(b0) {
    float4 gEyePosW;
    float4x4 gViewProj;
}


PSInput VSMain(VSInput input)
{
    PSInput output;
    float3 posW = input.Position + gEyePosW.xyz;
    output.Texcoord = float3(input.Position);
    output.PositionH = mul(float4(posW, 1.0f), gViewProj).xyww;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return gCubeMap.Sample(gsamLinearWrap, input.Texcoord);
}
