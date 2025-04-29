// LightingPass.hlsl
#include "LightingUtil.hlsl"
Texture2D gAlbedoMap : register(t0);
Texture2D gNormalMap : register(t1);
Texture2D gPositionMap : register(t2);
SamplerState gsamLinear : register(s0);

cbuffer cbPass : register(b1)
{
    float4x4 gInvViewProj;
    float3 gEyePosW;
    float pad;
    float4 gAmbientLight;
    Light gLights[MaxLights]; // или прописать явно количество, например Light gLights[3];
};

// Вершинный шейдер для полноэкранного треугольника
struct VSOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

VSOut VS(uint VertexID : SV_VertexID)
{
    VSOut outt;
    // Координаты вершин полноэкранного треугольника
    float2 verts[3] = { float2(-1, -1), float2(3, -1), float2(-1, 3) };
    outt.PosH = float4(verts[VertexID], 0, 1);
    outt.TexC = (verts[VertexID] + 1) * 0.5; // преобразуем из [-1,1] в [0,1]
    return outt;
}

// Пиксельный шейдер освещения
float4 PS(VSOut pin) : SV_TARGET
{
    // Вычитываем G-Buffer
    float3 albedo = gAlbedoMap.Sample(gsamLinear, pin.TexC).rgb;
    float3 normalW = normalize(gNormalMap.Sample(gsamLinear, pin.TexC).rgb);
    float3 posW = gPositionMap.Sample(gsamLinear, pin.TexC).rgb;

    float3 viewDir = normalize(gEyePosW - posW);

    // Начальный цвет от фонового света
    float3 color = albedo * gAmbientLight.rgb;

    // Применяем все источники света
    for (int i = 0; i < MaxLights; ++i)
    {
        // Предположим, что gLights[i] – это направленный свет с полем Direction и Strength
        float3 L = normalize(-gLights[i].Direction); // направление от точки к свету (для направленного света)
        float NdotL = max(dot(normalW, L), 0.0f);
        float3 diffuse = albedo * gLights[i].Strength * NdotL;
        // (Простая ламбертова модель без зеркального блика)
        color += diffuse;
    }

    return float4(color, 1.0f);
}
