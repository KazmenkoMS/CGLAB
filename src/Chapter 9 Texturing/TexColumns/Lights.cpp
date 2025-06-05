#include "TexColumnsApp.h"

void TexColumnsApp::BuildLights()
{
	// directional
	Light dir;
	dir.LightCBIndex = mLights.size();
	dir.Position = { 0,300,0 };
	dir.Direction = { 0, -1, 0 };
	dir.Color = { 1,1,1 };
	dir.Strength = 0.8;
	dir.type = 2;
	dir.LightUp = XMVectorSet(0, 0, -1, 0);
	auto& world = XMMatrixScaling(1000, 1000, 1000);
	XMStoreFloat4x4(&dir.gWorld, XMMatrixTranspose(world));
	mLights.push_back(dir);

	Light ambient;
	ambient.LightCBIndex = mLights.size();
	ambient.Position = { 3.0f, 0.0f, 3.0f };
	ambient.Color = { 0,0,0 }; // need only x
	ambient.Strength = 0.4; // need only x
	ambient.type = 0;
	XMStoreFloat4x4(&ambient.gWorld, XMMatrixTranspose(XMMatrixTranslation(0, 0, 0) * XMMatrixScaling(1000, 1000, 1000)));
	mLights.push_back(ambient);

	/*CreatePointLight({ -3,3,0 }, { 4,0,0 }, 1, 5,1);
	CreatePointLight({ 3,3,0 }, { 0,0,4 }, 1, 5,1);*/

	CreateSpotLight({ -5,3,30 }, { 0,0,-90 }, { 1,1,1 }, 1, 30, 6, 1);
}

void TexColumnsApp::SetLightShapes()
{
	for (auto& light : mLights)
	{

		switch (light.type)
		{
		case 1:
			light.ShapeGeo = mGeometries["shapeGeo"]->DrawArgs["sphere"];
			break;
		case 3:
			light.ShapeGeo = mGeometries["shapeGeo"]->DrawArgs["box"];
			break;
		}
	}
	mLights;
}

// build lighting root signature 
void TexColumnsApp::BuildLightingRootSignature()
{


	CD3DX12_DESCRIPTOR_RANGE gPosition;
	gPosition.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0
	CD3DX12_DESCRIPTOR_RANGE gNormal;
	gNormal.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1); // t1
	CD3DX12_DESCRIPTOR_RANGE gAlbedo;
	gAlbedo.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2); // t2
	CD3DX12_DESCRIPTOR_RANGE shadowMapRange;
	shadowMapRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3); // Shadow map at register t3

	CD3DX12_ROOT_PARAMETER rootParams[7];
	rootParams[0].InitAsDescriptorTable(1, &gPosition, D3D12_SHADER_VISIBILITY_ALL);
	rootParams[1].InitAsDescriptorTable(1, &gNormal, D3D12_SHADER_VISIBILITY_ALL);
	rootParams[2].InitAsDescriptorTable(1, &gAlbedo, D3D12_SHADER_VISIBILITY_ALL);
	rootParams[3].InitAsConstantBufferView(0); // b0 
	rootParams[4].InitAsConstantBufferView(1); // b1
	rootParams[5].InitAsConstantBufferView(2); // b2
	rootParams[6].InitAsDescriptorTable(1, &shadowMapRange, D3D12_SHADER_VISIBILITY_PIXEL); // PIXEL visibility

	auto staticSamplers = GetStaticSamplers();

	CD3DX12_ROOT_SIGNATURE_DESC rsDesc;
	rsDesc.Init(_countof(rootParams), rootParams,
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);


	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mLightingRootSignature.GetAddressOf())));
}
void TexColumnsApp::CreatePointLight(XMFLOAT3 pos, XMFLOAT3 color, float faloff_start, float faloff_end, float strength)
{
	Light light;
	light.LightCBIndex = mLights.size();

	light.Position = pos;
	light.Color = color;
	light.FalloffStart = faloff_start;
	light.FalloffEnd = faloff_end;
	light.type = 1;
	auto& world = XMMatrixScaling(faloff_end * 2, faloff_end * 2, faloff_end * 2) * XMMatrixTranslation(pos.x, pos.y, pos.z);
	XMStoreFloat4x4(&light.gWorld, XMMatrixTranspose(world));
	mLights.push_back(light);
}
void TexColumnsApp::CreateSpotLight(XMFLOAT3 pos, XMFLOAT3 rot, XMFLOAT3 color, float faloff_start, float faloff_end, float strength, float spotpower)
{
	Light light;
	light.LightCBIndex = mLights.size();

	light.Position = pos;
	light.Color = color;
	light.FalloffStart = faloff_start;
	light.FalloffEnd = faloff_end;
	light.Rotation = rot;
	light.LightUp = XMVectorSet(0, 1, 0, 0);
	light.type = 3;
	light.Strength = strength;
	light.SpotPower = spotpower;
	mLights.push_back(light);
}

