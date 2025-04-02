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
    
    float gTessFactorMin; // ����������� ������ ���������� �����
    float gTessFactorMax; // ������������ ������ ���������� �����
    int gTessLevel; // ������ ���������� ������ ����� (����� ���� ������� ������������)
    float gMaxTessDistance; // ����������, �� ������� ����������� ���. ����������
    float gDisplacementScale; // ������� ��������
    int fixTessLevel;
    float DecalRadius; // 4 ����� (����� 16 ����)
    float DecalFalloffRadius; // 4 ����� (���������� � 16 ����)
    float DecalPadding; // 4 ����� (���������� � 20 ���� - ����� ���������?)
    float3 decalPosition;
    float4x4 decalViewProj;
    float4x4 decalTranslation;
    float DecalPadding1; // 4 ����� (���������� � 20 ���� - ����� ���������?)
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
    float3 PosW : POSITION; // ������� � ����
    float3 NormalW : NORMAL;
    float2 TexC : TEXCOORD0;
    float3 TanW : TANGENT;
};

// HS -> DS (Control Point Data) - ����� ��������� � VertexOutHSIn
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
    float EdgeTess[3] : SV_TessFactor; // ��� ����������� ������
    float InsideTess : SV_InsideTessFactor; // ��� ����������� ������
    // ����� �������� ������ ������, ������������ ��� ����� �����
};

// DS -> PS
struct DSOutPSIn
{
    float4 PosH : SV_POSITION; // ������� � Clip Space (!!!)
    float3 PosW : POSITION; // ������� � ���� (��� ���������)
    float3 NormalW : NORMAL; // ������� � ���� (����� ��������!)
    float2 TexC : TEXCOORD0; // ���������� ����������
    float2 decalUV : TEXCOORD1; // ���������� ����������
    float3 TanW : TANGENT; // ����������� � ���� (��� normal mapping)
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

    // �������������� �������, �������, ����������� � ������� ����������
    vout.PosW = mul(float4(vin.PosL, 1.0f), gWorld).xyz;
    // ��� �������/����������� ���������� gWorld (����������� uniform scale).
    // ���� ���� non-uniform scale, ����� ��������-����������������� ������� ���� (����� (float3x3)gInvWorld).
    // �� ��� �������� ���� ���������� gWorld.
    vout.NormalW = normalize(mul(vin.NormalL, (float3x3) gWorld));
    vout.TanW = normalize(mul(vin.Tan, (float3x3) gWorld));

    // �������������� ���������� ���������� (� ������ ������������� ������� � ���������)
    float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
    vout.TexC = mul(texC, gMatTransform).xy;

    return vout;
}

float exponential_interpolation_exp(float a, float b, float alpha)
{
    float exponent = 3.0; // ����� ��������� ��� ��������� ��������
    return a + (b - a) * (exp(alpha * exponent) - 1.0) / (exp(exponent) - 1.0);
}
// ������� ��� ���������� �������� ���������� �� ������ ����������
PatchTess CalcTessFactors(float3 p0, float3 p1, float3 p2)
{
    PatchTess pt;

    // ��������� ����� �����
    float3 patchCenterW = (p0 + p1 + p2) / 3.0f;
    float distToDecal = distance(patchCenterW, decalPosition);
    float decalInfluence = smoothstep(DecalFalloffRadius, DecalRadius, distToDecal);
    // ������������� �� ������������ � ������������� ������� �� ������ ������� ������
    float decalTessFactor = lerp(gTessFactorMin, gTessFactorMax, decalInfluence);

    // ������� ������������� ������ ���������� ����� min � max � ����������� �� ����������
    float tessFactor = decalTessFactor;
    tessFactor = max(1.0f, tessFactor);
    if (fixTessLevel==1)
    {
        tessFactor = gTessLevel;
    }
    // ������������� ������� ��� ����� � ���������� �����
    // ����� ��������� ������������� ��� ������� ����� ��� ������������
    pt.EdgeTess[0] = tessFactor;
    pt.EdgeTess[1] = tessFactor;
    pt.EdgeTess[2] = tessFactor;
    pt.InsideTess = tessFactor; // ��� ������������ gTessInsideFactor

    return pt;
}












// HS Constant Function

PatchTess HSConst(InputPatch<VertexOutHSIn, 3> patch) // 3 ����������� ����� ��� ������������
{
    // ��������� ������� ���������� ��� ����� �����
    return CalcTessFactors(patch[0].PosW, patch[1].PosW, patch[2].PosW);
}

// HS Patch Function
[domain("tri")] // ����� - �����������
[partitioning("pow2")] // ����� ��������� (��� "integer", "fractional_even", "pow2")
[outputtopology("triangle_cw")] // �������� ��������� - ������������ �� ������� �������
[outputcontrolpoints(3)] // 3 ����������� ����� �� ������
[patchconstantfunc("HSConst")] // ��������� ����������� �������
[maxtessfactor(64.0)]

HSOutDSIn HSMain(InputPatch<VertexOutHSIn, 3> patch, uint i : SV_OutputControlPointID)
{
    HSOutDSIn hout;

    // ������ �������� ������ ����������� �����
    hout.PosW = patch[i].PosW;
    hout.NormalW = patch[i].NormalW;
    hout.TexC = patch[i].TexC;
    hout.TanW = patch[i].TanW;

    return hout;
}

// --- Domain Shader (DS) ---
// �������: ������ ������������� Displacement Map � �������� �������

[domain("tri")]
DSOutPSIn DSMain(PatchTess patchTessConstants,
                 float3 domainLoc : SV_DomainLocation, // ���������������� ���������� (u, v, w)
                 const OutputPatch<HSOutDSIn, 3> patch)
{
    DSOutPSIn dout;

    // 1. ������������ ��������� ����������� �����
    dout.PosW = domainLoc.x * patch[0].PosW + domainLoc.y * patch[1].PosW + domainLoc.z * patch[2].PosW;
    dout.NormalW = domainLoc.x * patch[0].NormalW + domainLoc.y * patch[1].NormalW + domainLoc.z * patch[2].NormalW;
    dout.TexC = domainLoc.x * patch[0].TexC + domainLoc.y * patch[1].TexC + domainLoc.z * patch[2].TexC;
    dout.TanW = domainLoc.x * patch[0].TanW + domainLoc.y * patch[1].TanW + domainLoc.z * patch[2].TanW;
    // --- 2. ��������� ������� ������ �� ��� ������� ---
    float distToDecalCenter = distance(dout.PosW, decalPosition);
    float decalInfluence = smoothstep(DecalFalloffRadius, DecalRadius, distToDecalCenter);
     // --- 3. ��������� ��������, ���� ���� ������� ������ ---
    float finalDisplacementOffset = 0.0f; // �� ��������� �������� ���
    
    
    if (decalInfluence > 0.0f) // ��������� �������� ������ ��� �������� ������
    {
        // --- 3a. ��������� UV ���������� ��� ������ ---
        // �������������� ������� ������� � ������������ �������� ������
        float4 decalClipPos = mul(float4(dout.PosW, 1.0f), decalViewProj);

        // ������������� ������� (���� ������� ������������)
        // ��������� small epsilon ��� ��������� ������� �� ����
        decalClipPos.xyz /= (decalClipPos.w + 1e-6f);

        // ����������� Clip Space [-1, 1] � UV [0, 1]
        // (Y �������������, �.�. � UV ������ 0 ������)
        float2 decalUV = float2(decalClipPos.x * 0.5f + 0.5f, decalClipPos.y * -0.5f + 0.5f);

        // --- 3b. ���������� ����� �������� ������ (������ ���� UV � �������� [0,1]) ---
        // Clamp UVs ��� ���������� saturate, ����� �������� ������ �� ������� ��������
        decalUV = saturate(decalUV); // ������������ UV ���������� [0, 1]

        // �������������� ��������: �������� ����������� ������ ���� �� ������ �������� ������ �� XY
        // � ���� ������� (decalClipPos.z) ��������� � ���������� ��������� (��������, [0, 1] ��� ���������������)
        // ��� �������� �� ����������� ������, �.�. decalInfluence ��� ������������ �� �������,
        // �� ����� ������ �������� ���������� �� �������� ��������.
        // if(all(decalUV >= 0) && all(decalUV <= 1) && decalClipPos.z >= 0 && decalClipPos.z <= 1) { ... }
        float2 texC = mul(float4(dout.TexC, 0.0f, 1.0f), decalTranslation).xy;
        dout.decalUV = texC;
        float decalDispValue = gDecalDispMap.SampleLevel(gsamLinearWrap, texC, 0.0f).r;
        // --- 3c. ������������ ��������� �������� ---
        // (decalDispValue - 0.5f) ���� 0.5 - ��� ��������
        // �������� �� ���� �������� ������ � �� ������� ������� (������� ����)
        finalDisplacementOffset = (decalDispValue - 0.5f) * gDisplacementScale * decalInfluence;

        // ��������� �������� � ������� ����� �������
        dout.PosW += finalDisplacementOffset * dout.NormalW;
        dout.isInDecal = true;
    }
    else
    {
        dout.isInDecal = false;
    }
    
    // 4. �������� �������/����������� (�� ���������, ��� ��� �������� ���)
    // ����������� ����������������� ������� (����� ��� �������� � �����������)
    dout.NormalW = normalize(dout.NormalW);
    dout.TanW = normalize(dout.TanW);
  
    // 5. ������������� ����������������� ������� ������� � Clip Space
    dout.PosH = mul(float4(dout.PosW, 1.0f), gViewProj);

    // ���������� ��������� ��� ����������� �������
    return dout;
}




// �������� ��������� �������
float4 PS(DSOutPSIn pin) : SV_Target
{
    float4 diffuseAlbedo;
    float3 normalSample;
    if (pin.isInDecal)
    {
        diffuseAlbedo = gDecalDispMap.Sample(gsamAnisotropicWrap, pin.decalUV) * gDiffuseAlbedo;
        normalSample = gDecalDispMap.Sample(gsamAnisotropicWrap, pin.decalUV).rgb; // ��������� ����� �������
    }
    else
    {
        diffuseAlbedo = gDiffuseMap.Sample(gsamAnisotropicWrap, pin.TexC) * gDiffuseAlbedo;
    normalSample = gNormalMap.Sample(gsamAnisotropicWrap, pin.TexC).rgb; // ��������� ����� �������
        
    }
    if (diffuseAlbedo.r == 1 && diffuseAlbedo.g == 1 && diffuseAlbedo.b == 1)
    {
        diffuseAlbedo = gDiffuseMap.Sample(gsamAnisotropicWrap, pin.TexC) * gDiffuseAlbedo;
    }
    // ���������� �������� � ������� ��� ������

    // ���������� normal map, ���� ����
    // ������� NormalSampleToWorldSpace ������ ������������ pin.NormalW � pin.TanW �� DS
 
    float3 bumpedNormalW = NormalSampleToWorldSpace(normalSample, pin.NormalW, pin.TanW); // ��������� ��������� �������

    // ������� ��� ������ ���� ������������� � DS, �� �� ������ ������:
    bumpedNormalW = normalize(bumpedNormalW); // ���������� bumpedNormalW ��� ���������

    // ������ � ������
    float3 toEyeW = normalize(gEyePosW - pin.PosW);

    // ������ ��������� (��������� bumpedNormalW � pin.PosW)
    float4 ambient = gAmbientLight * diffuseAlbedo;

    const float shininess = 1.0f - gRoughness;
    Material mat = { diffuseAlbedo, gFresnelR0, shininess }; // �������� ����������� diffuseAlbedo

    // ��������� ������ ��������� ��� ���� ���������� �����
    float3 directLight = ComputeLighting(gLights,mat,pin.PosW, bumpedNormalW, toEyeW, 1.f); // �������� bumpedNormalW
    float4 litColor = ambient + float4(directLight, 0.0f);

    // ��������� �����, ���� ����� (fog logic...)

    // �����-��������� � �.�.
    litColor.a = diffuseAlbedo.a; // ��������� ����� �� ��������

    return litColor;
}


