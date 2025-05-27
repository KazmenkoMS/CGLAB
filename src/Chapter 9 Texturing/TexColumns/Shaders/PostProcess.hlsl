// ChromaticAberration.hlsl

Texture2D gSceneTexture : register(t0); // The input scene texture
SamplerState gsamLinearClamp : register(s0); // Sampler

cbuffer cbPostProcess : register(b0)
{
    float gChromaticAberrationOffset;
    // Add other parameters if needed, e.g., float2 gScreenDimensions;
};

struct VSOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

// Vertex shader for a full-screen triangle (re-use or adapt from LightingPass.hlsl)
VSOut VS(uint vid : SV_VertexID)
{
    VSOut output;
    // Full-screen triangle vertices
    float2 positions[3] =
    {
        float2(-1.0f, -1.0f),
        float2(-1.0f, 3.0f),
        float2(3.0f, -1.0f)
    };
    output.PosH = float4(positions[vid], 0.0f, 1.0f);
    // Generate texture coordinates to cover the full screen
    output.TexC = float2((output.PosH.x + 1.0f) * 0.5f, (1.0f - output.PosH.y) * 0.5f);
    return output;
}

float4 PS(VSOut pin) : SV_TARGET
{
    float2 texCoord = pin.TexC;
    float2 texC_H = float2(pin.TexC.x * 2 - 1, pin.TexC.y * 2 - 1);
    // Calculate offsets for R and B channels
    // The gChromaticAberrationOffset could be a small value like 0.001 to 0.01
    float alpha = (abs(texC_H.x) + abs(texC_H.y)) / 2;
    float2 offsetR = float2(lerp(0, gChromaticAberrationOffset, alpha), 0.0f);
    float2 offsetB = float2(-lerp(0, gChromaticAberrationOffset, alpha), 0.0f);
    // You can also make offsets vertical or in other directions:
    // float2 offsetR = float2(gChromaticAberrationOffset, gChromaticAberrationOffset);
    // float2 offsetB = float2(-gChromaticAberrationOffset, -gChromaticAberrationOffset);


    float r = gSceneTexture.Sample(gsamLinearClamp, texCoord + offsetR).r;
    float g = gSceneTexture.Sample(gsamLinearClamp, texCoord).g;
    float b = gSceneTexture.Sample(gsamLinearClamp, texCoord + offsetB).b;
    float a = gSceneTexture.Sample(gsamLinearClamp, texCoord).a; // Or albedo.a from your gbuffer if more appropriate

    return float4(r, g, b, a);
}