
struct ParticleInstanceData
{
    float3 Position : POSITION;
    float4 Color : COLOR;
    float Size : SIZE;
};

// SRV for particle instance data
StructuredBuffer<ParticleInstanceData> gParticleData : register(t0);

// Per-pass constants (view-projection matrix, etc.)
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
};

// Структура для вывода из вершинного шейдера и ввода в геометрический шейдер
struct VSOutput
{
    float3 WorldPos : WORLDPOS; // Мировая позиция частицы
    float4 Color : COLOR;
    float Size : SIZE;
    // SV_Position здесь не нужен, так как GS будет генерировать вершины в clip space
};

// Вершинный шейдер
VSOutput VS(uint vertId : SV_VertexID, uint instId : SV_InstanceID)
{
    VSOutput vout;
    ParticleInstanceData pIn = gParticleData[instId];

    vout.WorldPos = pIn.Position; // Просто передаем мировую позицию
    vout.Color = pIn.Color;
    vout.Size = pIn.Size; // Передаем размер

    return vout;
}

// Структура для вывода из геометрического шейдера и ввода в пиксельный шейдер
struct GSOutput
{
    float4 PosH : SV_Position; // Позиция в пространстве отсечения (clip space)
    float4 Color : COLOR;
    float2 UV : TEXCOORD0; // UV-координаты для отсечения в PS (НОВОЕ ПОЛЕ)
};

// Геометрический шейдер (с добавлением UV)
[maxvertexcount(4)]
void GS(point VSOutput input[1],
        inout TriangleStream<GSOutput> triStream)
{
    float3 particleWorldPos = input[0].WorldPos;
    float4 particleColor = input[0].Color;
    float particleSize = input[0].Size; // Используйте актуальный размер частицы

    // Логика для viewpoint-oriented билбордов (остается как в предыдущем шаге)
    float3 particleToEyeDir = normalize(gEyePosW - particleWorldPos);
    float3 worldUpAxis = float3(0.0f, 1.0f, 0.0f);

    if (abs(dot(particleToEyeDir, worldUpAxis)) > 0.999f)
    {
        worldUpAxis = float3(1.0f, 0.0f, 0.0f);
    }

    float3 billboardRight = normalize(cross(worldUpAxis, particleToEyeDir));
    float3 billboardUp = normalize(cross(particleToEyeDir, billboardRight));

    // halfWidth и halfHeight определяют размер квадрата, в который будет вписан круг
    // Убедитесь, что particleSize правильно масштабируется для получения желаемого радиуса в мировых единицах
    float halfWidth = particleSize * 0.5f; // Например, если particleSize - это диаметр
    float halfHeight = particleSize * 0.5f;

    GSOutput v[4];

    // Порядок вершин и UV-координат:
    // UV (0,0) - Верхний левый угол
    // UV (0,1) - Нижний левый угол
    // UV (1,0) - Верхний правый угол
    // UV (1,1) - Нижний правый угол

    // Верхняя левая вершина
    v[0].PosH = mul(float4(particleWorldPos + billboardUp * halfHeight - billboardRight * halfWidth, 1.0f), gViewProj);
    v[0].Color = particleColor;
    v[0].UV = float2(0.0f, 0.0f); // UV для верхнего левого угла

    // Нижняя левая вершина
    v[1].PosH = mul(float4(particleWorldPos - billboardUp * halfHeight - billboardRight * halfWidth, 1.0f), gViewProj);
    v[1].Color = particleColor;
    v[1].UV = float2(0.0f, 1.0f); // UV для нижнего левого угла

    // Верхняя правая вершина
    v[2].PosH = mul(float4(particleWorldPos + billboardUp * halfHeight + billboardRight * halfWidth, 1.0f), gViewProj);
    v[2].Color = particleColor;
    v[2].UV = float2(1.0f, 0.0f); // UV для верхнего правого угла

    // Нижняя правая вершина
    v[3].PosH = mul(float4(particleWorldPos - billboardUp * halfHeight + billboardRight * halfWidth, 1.0f), gViewProj);
    v[3].Color = particleColor;
    v[3].UV = float2(1.0f, 1.0f); // UV для нижнего правого угла

    // Отправляем вершины в виде Triangle Strip
    triStream.Append(v[0]); // TopLeft
    triStream.Append(v[1]); // BottomLeft
    triStream.Append(v[2]); // TopRight
    triStream.Append(v[3]); // BottomRight
    triStream.RestartStrip();
}


// Пиксельный шейдер (для создания круглых частиц)
float4 PS(GSOutput pin) : SV_Target
{
    // Преобразуем UV из диапазона [0,1] в диапазон [-1,1]
    // Центр квадрата (0.5, 0.5) в UV [0,1] станет (0,0) в UV [-1,1]
    float2 centeredUV = pin.UV * 2.0f - 1.0f;

    // Вычисляем квадрат расстояния от центра (0,0) до текущего пикселя в пространстве [-1,1]
    // dot(v,v) == v.x*v.x + v.y*v.y
    float distSq = dot(centeredUV, centeredUV);

    // Если квадрат расстояния больше 1.0 (т.е. расстояние больше радиуса 1),
    // то пиксель находится вне единичного круга, отбрасываем его.
    if (distSq > 1.0f)
    {
        discard; // discard полностью отменяет отрисовку текущего пикселя
    }

    // Если пиксель внутри круга, возвращаем его цвет.
    // Альфа-канал цвета частицы (pin.Color.a) будет использован, если включено смешивание.
    return pin.Color;

}