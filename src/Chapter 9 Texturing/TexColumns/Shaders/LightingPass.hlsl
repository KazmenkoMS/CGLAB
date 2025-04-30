// LightingPass.hlsl

// Defaults for number of lights.
#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 1
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 1
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 0
#endif
#include "LightingUtil.hlsl"
Texture2D gPositionMap : register(t2);
Texture2D gNormalMap : register(t1);
Texture2D gAlbedoMap : register(t0);

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

cbuffer cbPass : register(b0)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
    float4 gAmbientLight;

    // Indices [0, NUM_DIR_LIGHTS) are directional lights;
    // indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
    // indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
    // are spot lights for a maximum of MaxLights per object.
    Light gLights[MaxLights];
};

// Вершинный шейдер для полноэкранного треугольника
struct VSOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

VSOut VS(uint vid : SV_VertexID)
{
    VSOut output;
    
    // Координаты вершин полноэкранного треугольника
    float2 positions[3] =
    {
        float2(-1, -1),
        float2(3, -1),
        float2(-1, 3)
    };
    
    output.PosH = float4(positions[vid], 0, 1);
    output.TexC = positions[vid] * float2(0.5, -0.5) + 0.5;
    
    return output;
}

// Пиксельный шейдер освещения
float4 PS(VSOut pin) : SV_TARGET
{
    // Вычитываем G-Buffer
    float4 albedo = gAlbedoMap.Sample(gsamAnisotropicWrap, pin.TexC);
    float3 normalW = normalize(gNormalMap.Sample(gsamAnisotropicWrap, pin.TexC).rgb);
   
    float3 posW = gPositionMap.Sample(gsamAnisotropicWrap, pin.TexC).rgb;

    float3 toEyeW = normalize(gEyePosW - posW);

    // Light terms.
    float4 ambient = gAmbientLight * albedo;

    Material mat =
    {
       albedo, float3(0.05, 0.05, 0.05), 0.7
    };
    float3 shadowFactor = 1.0f;
    float4 directLight = ComputeLighting(gLights, mat, posW,
        normalW, toEyeW, shadowFactor)/3;
    float4 litColor = directLight;
    // Common convention to take alpha from diffuse albedo.
    litColor.a = albedo.a;

    return litColor;
}
