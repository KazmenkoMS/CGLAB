//***************************************************************************************
// Default.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

// Defaults for number of lights.
#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 0
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 1
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 0
#endif

// Include structures and functions for lighting.
#include "LightingUtil.hlsl"

Texture2D    gDiffuseMap : register(t0);
Texture2D    gNormalMap : register(t1);
Texture2D    gDispMap : register(t2);
Texture2D gDecalDispMap : register(t3);

SamplerState gsamPointWrap        : register(s0);
SamplerState gsamPointClamp       : register(s1);
SamplerState gsamLinearWrap       : register(s2);
SamplerState gsamLinearClamp      : register(s3);
SamplerState gsamAnisotropicWrap  : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

// Constant data that varies per frame.
cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gInvWorld;
	float4x4 gTexTransform;
};

// Constant data that varies per material.
cbuffer cbPass : register(b1)
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
    Light gLights[MaxLights];
    
    float gTessFactorMin; // Минимальный фактор тесселяции ребер
    float gTessFactorMax; // Максимальный фактор тесселяции ребер
    int gTessLevel; // Фактор тесселяции внутри патча (можно тоже сделать динамическим)
    float gMaxTessDistance; // Расстояние, на котором достигается мин. тесселяция
    float gDisplacementScale; // Масштаб смещения
    int fixTessLevel;
    float DecalRadius; // 4 байта (Итого 16 байт)
    float DecalFalloffRadius; // 4 байта (Начинается с 16 байт)
    float DecalPadding; // 4 байта (Начинается с 20 байт - чтобы выровнять?)
    float3 decalPosition;
    float4x4 decalViewProj;
    float4x4 decalTranslation;
    float DecalPadding1; // 4 байта (Начинается с 20 байт - чтобы выровнять?)
    // Indices [0, NUM_DIR_LIGHTS) are directional lights;
    // indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
    // indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
    // are spot lights for a maximum of MaxLights per object.
};

cbuffer cbMaterial : register(b2)
{
	float4   gDiffuseAlbedo;
    float3   gFresnelR0;
    float    gRoughness;
	float4x4 gMatTransform;
};

struct VertexIn
{
	float3 PosL    : POSITION;
    float3 NormalL : NORMAL;
	float2 TexC    : TEXCOORD;
    float3 Tan : TANGENT;
};

struct VertexOut
{
	float4 PosH    : SV_POSITION;
    float3 PosW    : POSITION;
    float3 NormalW : NORMAL;
	float2 TexC    : TEXCOORD;
    float3 Tan : TANGENT;
};

// VS -> HS
struct VertexOutHSIn
{
    float3 PosW : POSITION; // Позиция в мире
    float3 NormalW : NORMAL;
    float2 TexC : TEXCOORD0;
    float3 TanW : TANGENT;
};

// HS -> DS (Control Point Data) - часто совпадает с VertexOutHSIn
struct HSOutDSIn
{
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
    float2 TexC : TEXCOORD0;
    float3 TanW : TANGENT;
};

// HS Constant Function Output
struct PatchTess
{
    float EdgeTess[3] : SV_TessFactor; // Для треугольных патчей
    float InsideTess : SV_InsideTessFactor; // Для треугольных патчей
    // Можно добавить другие данные, передаваемые для всего патча
};

// DS -> PS
struct DSOutPSIn
{
    float4 PosH : SV_POSITION; // Позиция в Clip Space (!!!)
    float3 PosW : POSITION; // Позиция в мире (для освещения)
    float3 NormalW : NORMAL; // Нормаль в мире (после смещения!)
    float2 TexC : TEXCOORD0; // Текстурные координаты
    float2 decalUV : TEXCOORD1; // Текстурные координаты
    float3 TanW : TANGENT; // Касательная в мире (для normal mapping)
    bool isInDecal : ISINDECAL;
};


float3 NormalSampleToWorldSpace(float3 normalMapSample, float3 unitNormalW, float3 tangentW)
{
	// Uncompress each component from [0,1] to [-1,1].
    float3 normalT = 2.0f * normalMapSample - 1.0f;

	// Build orthonormal basis.
    float3 N = unitNormalW;
    float3 T = normalize(tangentW - dot(tangentW, N) * N);
    float3 B = cross(N, T);

    float3x3 TBN = float3x3(T, B, N);

	// Transform from tangent space to world space.
    float3 bumpedNormalW = mul(normalT, TBN);

    return bumpedNormalW;
}
VertexOutHSIn VS(VertexIn vin)
{
    VertexOutHSIn vout;

    // Трансформируем позицию, нормаль, касательную в мировые координаты
    vout.PosW = mul(float4(vin.PosL, 1.0f), gWorld).xyz;
    // Для нормали/касательной используем gWorld (предполагая uniform scale).
    // Если есть non-uniform scale, нужна инверсно-транспонированная матрица мира (часто (float3x3)gInvWorld).
    // Но для простоты пока используем gWorld.
    vout.NormalW = normalize(mul(vin.NormalL, (float3x3) gWorld));
    vout.TanW = normalize(mul(vin.Tan, (float3x3) gWorld));

    // Трансформируем текстурные координаты (с учетом трансформаций объекта и материала)
    float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
    vout.TexC = mul(texC, gMatTransform).xy;

    return vout;
}

float exponential_interpolation_exp(float a, float b, float alpha)
{
    float exponent = 3.0; // Можно настроить для изменения скорости
    return a + (b - a) * (exp(alpha * exponent) - 1.0) / (exp(exponent) - 1.0);
}
// Функция для вычисления факторов тесселяции на основе расстояния
PatchTess CalcTessFactors(float3 p0, float3 p1, float3 p2)
{
    PatchTess pt;

    // Вычисляем центр патча
    float3 patchCenterW = (p0 + p1 + p2) / 3.0f;
    float distToDecal = distance(patchCenterW, decalPosition);
    float decalInfluence = smoothstep(DecalFalloffRadius, DecalRadius, distToDecal);
    // Интерполируем от минимального к максимальному фактору на основе влияния декали
    float decalTessFactor = lerp(gTessFactorMin, gTessFactorMax, decalInfluence);

    // Линейно интерполируем фактор тесселяции между min и max в зависимости от расстояния
    float tessFactor = decalTessFactor;
    tessFactor = max(1.0f, tessFactor);
    if (fixTessLevel==1)
    {
        tessFactor = gTessLevel;
    }
    // Устанавливаем факторы для ребер и внутренней части
    // Можно вычислять индивидуально для каждого ребра для адаптивности
    pt.EdgeTess[0] = tessFactor;
    pt.EdgeTess[1] = tessFactor;
    pt.EdgeTess[2] = tessFactor;
    pt.InsideTess = tessFactor; // Или использовать gTessInsideFactor

    return pt;
}












// HS Constant Function

PatchTess HSConst(InputPatch<VertexOutHSIn, 3> patch) // 3 контрольные точки для треугольника
{
    // Вычисляем факторы тесселяции для этого патча
    return CalcTessFactors(patch[0].PosW, patch[1].PosW, patch[2].PosW);
}

// HS Patch Function
[domain("tri")] // Домен - треугольник
[partitioning("pow2")] // Схема разбиения (или "integer", "fractional_even", "pow2")
[outputtopology("triangle_cw")] // Выходные примитивы - треугольники по часовой стрелке
[outputcontrolpoints(3)] // 3 контрольные точки на выходе
[patchconstantfunc("HSConst")] // Указываем константную функцию
[maxtessfactor(64.0)]

HSOutDSIn HSMain(InputPatch<VertexOutHSIn, 3> patch, uint i : SV_OutputControlPointID)
{
    HSOutDSIn hout;

    // Просто передаем данные контрольной точки
    hout.PosW = patch[i].PosW;
    hout.NormalW = patch[i].NormalW;
    hout.TexC = patch[i].TexC;
    hout.TanW = patch[i].TanW;

    return hout;
}

// --- Domain Shader (DS) ---
// ИЗМЕНЕН: Убрано сэмплирование Displacement Map и смещение позиции

[domain("tri")]
DSOutPSIn DSMain(PatchTess patchTessConstants,
                 float3 domainLoc : SV_DomainLocation, // Барицентрические координаты (u, v, w)
                 const OutputPatch<HSOutDSIn, 3> patch)
{
    DSOutPSIn dout;

    // 1. Интерполяция атрибутов контрольных точек
    dout.PosW = domainLoc.x * patch[0].PosW + domainLoc.y * patch[1].PosW + domainLoc.z * patch[2].PosW;
    dout.NormalW = domainLoc.x * patch[0].NormalW + domainLoc.y * patch[1].NormalW + domainLoc.z * patch[2].NormalW;
    dout.TexC = domainLoc.x * patch[0].TexC + domainLoc.y * patch[1].TexC + domainLoc.z * patch[2].TexC;
    dout.TanW = domainLoc.x * patch[0].TanW + domainLoc.y * patch[1].TanW + domainLoc.z * patch[2].TanW;
    // --- 2. Вычисляем влияние декали НА ЭТУ ВЕРШИНУ ---
    float distToDecalCenter = distance(dout.PosW, decalPosition);
    float decalInfluence = smoothstep(DecalFalloffRadius, DecalRadius, distToDecalCenter);
     // --- 3. Применяем смещение, если есть влияние декали ---
    float finalDisplacementOffset = 0.0f; // По умолчанию смещения нет
    
    
    if (decalInfluence > 0.0f) // Применяем смещение только под влиянием декали
    {
        // --- 3a. Вычисляем UV координаты для декали ---
        // Трансформируем мировую позицию в пространство проекции декали
        float4 decalClipPos = mul(float4(dout.PosW, 1.0f), decalViewProj);

        // Перспективное деление (если матрица проекционная)
        // Добавляем small epsilon для избежания деления на ноль
        decalClipPos.xyz /= (decalClipPos.w + 1e-6f);

        // Преобразуем Clip Space [-1, 1] в UV [0, 1]
        // (Y инвертируется, т.к. в UV обычно 0 сверху)
        float2 decalUV = float2(decalClipPos.x * 0.5f + 0.5f, decalClipPos.y * -0.5f + 0.5f);

        // --- 3b. Сэмплируем карту смещения декали (только если UV в пределах [0,1]) ---
        // Clamp UVs или используем saturate, чтобы избежать выхода за пределы текстуры
        decalUV = saturate(decalUV); // Ограничиваем UV диапазоном [0, 1]

        // Дополнительная проверка: смещение применяется только если мы внутри проекции декали по XY
        // и если глубина (decalClipPos.z) находится в допустимом диапазоне (например, [0, 1] для ортографической)
        // Эта проверка не обязательна строго, т.к. decalInfluence уже ограничивает по радиусу,
        // но может помочь избежать артефактов на границах проекции.
        // if(all(decalUV >= 0) && all(decalUV <= 1) && decalClipPos.z >= 0 && decalClipPos.z <= 1) { ... }
        float2 texC = mul(float4(dout.TexC, 0.0f, 1.0f), decalTranslation).xy;
        dout.decalUV = texC;
        float decalDispValue = gDecalDispMap.SampleLevel(gsamLinearWrap, texC, 0.0f).r;
        // --- 3c. Рассчитываем финальное смещение ---
        // (decalDispValue - 0.5f) если 0.5 - нет смещения
        // Умножаем на силу смещения декали и на текущее влияние (плавный спад)
        finalDisplacementOffset = (decalDispValue - 0.5f) * gDisplacementScale * decalInfluence;

        // Добавляем смещение к позиции вдоль нормали
        dout.PosW += finalDisplacementOffset * dout.NormalW;
        dout.isInDecal = true;
    }
    else
    {
        dout.isInDecal = false;
    }
    
    // 4. Пересчет нормали/касательной (НЕ ТРЕБУЕТСЯ, так как смещения нет)
    // Нормализуем интерполированные векторы (важно для нормалей и касательных)
    dout.NormalW = normalize(dout.NormalW);
    dout.TanW = normalize(dout.TanW);
  
    // 5. Трансформация ИНТЕРПОЛИРОВАННОЙ мировой позиции в Clip Space
    dout.PosH = mul(float4(dout.PosW, 1.0f), gViewProj);

    // Возвращаем структуру для Пиксельного Шейдера
    return dout;
}




// Изменить сигнатуру функции
float4 PS(DSOutPSIn pin) : SV_Target
{
    float4 diffuseAlbedo;
    float3 normalSample;
    if (pin.isInDecal)
    {
        diffuseAlbedo = gDecalDispMap.Sample(gsamAnisotropicWrap, pin.decalUV) * gDiffuseAlbedo;
        normalSample = gDecalDispMap.Sample(gsamAnisotropicWrap, pin.decalUV).rgb; // Загружаем сэмпл нормали
    }
    else
    {
        diffuseAlbedo = gDiffuseMap.Sample(gsamAnisotropicWrap, pin.TexC) * gDiffuseAlbedo;
    normalSample = gNormalMap.Sample(gsamAnisotropicWrap, pin.TexC).rgb; // Загружаем сэмпл нормали
        
    }
    if (diffuseAlbedo.r == 1 && diffuseAlbedo.g == 1 && diffuseAlbedo.b == 1)
    {
        diffuseAlbedo = gDiffuseMap.Sample(gsamAnisotropicWrap, pin.TexC) * gDiffuseAlbedo;
    }
    // Используем текстуру и сэмплер как раньше

    // Используем normal map, если есть
    // Функция NormalSampleToWorldSpace должна использовать pin.NormalW и pin.TanW из DS
 
    float3 bumpedNormalW = NormalSampleToWorldSpace(normalSample, pin.NormalW, pin.TanW); // Вычисляем смещенную нормаль

    // Нормаль уже должна быть нормализована в DS, но на всякий случай:
    bumpedNormalW = normalize(bumpedNormalW); // Используем bumpedNormalW для освещения

    // Вектор к камере
    float3 toEyeW = normalize(gEyePosW - pin.PosW);

    // Расчет освещения (используя bumpedNormalW и pin.PosW)
    float4 ambient = gAmbientLight * diffuseAlbedo;

    const float shininess = 1.0f - gRoughness;
    Material mat = { diffuseAlbedo, gFresnelR0, shininess }; // Передаем обновленный diffuseAlbedo

    // Вычисляем прямое освещение для всех источников света
    float3 directLight = ComputeLighting(gLights,mat,pin.PosW, bumpedNormalW, toEyeW, 1.f); // Передаем bumpedNormalW
    float4 litColor = ambient + float4(directLight, 0.0f);

    // Добавляем туман, если нужно (fog logic...)

    // Альфа-коррекция и т.д.
    litColor.a = diffuseAlbedo.a; // Сохраняем альфу из текстуры

    return litColor;
}


