// ChromaticAberration.hlsl

Texture2D gSceneTexture : register(t0); // The input scene texture
Texture2D gLutTexture : register(t1); // The input scene texture
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


float3 ApplyLUT_1024x32(float3 originalColor, Texture2D lutTexture, SamplerState lutSampler)
{
    // Размерность одной стороны куба LUT
    const float LUT_DIMENSION = 32.0f; // Для текстуры 1024x32 -> куб 32x32x32

    // Убедимся, что исходный цвет находится в диапазоне [0, 1]
    float3 inColor = saturate(originalColor.rgb);

    // 1. Определяем два "среза" (slice) и фактор смешивания по синему каналу (inColor.b)
    // Масштабируем B-компонент до диапазона [0, LUT_DIMENSION - 1], т.е. [0, 31]
    float blueScaled = inColor.b * (LUT_DIMENSION - 1.0f);

    float sliceIndex0_f = floor(blueScaled);
    // sliceIndex1_f не должен выходить за пределы (LUT_DIMENSION - 1.0f)
    float sliceIndex1_f = min(sliceIndex0_f + 1.0f, LUT_DIMENSION - 1.0f);

    float blendFactor = blueScaled - sliceIndex0_f; // Эквивалентно frac(blueScaled)

    // 2. Вычисляем UV-координаты для выборки из этих двух срезов.
    // Используем формулу для центрирования на текселе:
    // (нормализованное_значение * (РазмерностьСреза - 1.0) + 0.5) / ОбщаяРазмерностьВПикселях

    float2 uv0, uv1;

    // Координата X (горизонталь):
    // Нормализованное значение: inColor.r (Красный)
    // Размерность среза по горизонтали: LUT_DIMENSION (32 пикселя)
    // Смещение до нужного среза: sliceIndex_f * LUT_DIMENSION
    // Общая ширина текстуры в пикселях: LUT_DIMENSION * LUT_DIMENSION (1024 пикселя)
    float commonXNumerator = inColor.r * (LUT_DIMENSION - 1.0f) + 0.5f;
    uv0.x = (commonXNumerator + sliceIndex0_f * LUT_DIMENSION) / (LUT_DIMENSION * LUT_DIMENSION);
    uv1.x = (commonXNumerator + sliceIndex1_f * LUT_DIMENSION) / (LUT_DIMENSION * LUT_DIMENSION);

    // Координата Y (вертикаль):
    // Нормализованное значение: inColor.g (Зеленый)
    // Размерность среза по вертикали: LUT_DIMENSION (32 пикселя)
    // Общая высота текстуры в пикселях: LUT_DIMENSION (32 пикселя)
    float commonYNumerator = inColor.g * (LUT_DIMENSION - 1.0f) + 0.5f;
    uv0.y = commonYNumerator / LUT_DIMENSION;
    uv1.y = commonYNumerator / LUT_DIMENSION; // Y координата одинакова для обоих срезов

    // 3. Делаем две выборки из 2D LUT-текстуры
    float3 colorSlice0 = lutTexture.Sample(lutSampler, uv0).rgb;
    float3 colorSlice1 = lutTexture.Sample(lutSampler, uv1).rgb;

    // 4. Линейно интерполируем между результатами двух выборок, используя blendFactor
    float3 correctedColor = lerp(colorSlice0, colorSlice1, blendFactor);

    return correctedColor;
}







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
    float4 color = float4(r, g, b, a);
    float3 red = float3(0, 0, 1);
    color.rgb = ApplyLUT_1024x32(color.rgb, gLutTexture,gsamLinearClamp);
    return color;
}