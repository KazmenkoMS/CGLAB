#include "TexColumnsApp.h"

// shadow root signature 
void TexColumnsApp::BuildShadowPassRootSignature()
{
	CD3DX12_ROOT_PARAMETER slotRootParameter[2];

	slotRootParameter[0].InitAsConstantBufferView(0); // ObjectConstants (b0)
	slotRootParameter[1].InitAsConstantBufferView(1); // ShadowPassConstants (b1 - gLightViewProj)
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc;
	rootSigDesc.Init(
		_countof(slotRootParameter), slotRootParameter,
		0, nullptr, // No static samplers needed for basic shadow map generation
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
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
		IID_PPV_ARGS(&mShadowPassRootSignature)));
}

// dsv and rtv's
void TexColumnsApp::BuildShadowMapViews()
{
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 2; // For one shadow map
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&mShadowDsvHeap)));
	int i = 0;
	for (auto& light : mLights)
	{

		if (light.type == 2 || light.type == 3)
		{
			// Define shadow map properties (can be members of the class or taken from a specific light)
			mShadowViewport = { 0.0f, 0.0f, (float)SHADOW_MAP_WIDTH, (float)SHADOW_MAP_HEIGHT, 0.0f, 1.0f };
			mShadowScissorRect = { 0, 0, (int)SHADOW_MAP_WIDTH, (int)SHADOW_MAP_HEIGHT };

			// Create the shadow map texture
			D3D12_RESOURCE_DESC texDesc;
			ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
			texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			texDesc.Alignment = 0;
			texDesc.Width = SHADOW_MAP_WIDTH;
			texDesc.Height = SHADOW_MAP_HEIGHT;
			texDesc.DepthOrArraySize = 1;
			texDesc.MipLevels = 1;
			texDesc.Format = SHADOW_MAP_FORMAT;
			texDesc.SampleDesc.Count = 1;
			texDesc.SampleDesc.Quality = 0;
			texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

			D3D12_CLEAR_VALUE clearValue;
			clearValue.Format = SHADOW_MAP_DSV_FORMAT;
			clearValue.DepthStencil.Depth = 1.0f;
			clearValue.DepthStencil.Stencil = 0;

			ThrowIfFailed(md3dDevice->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&texDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ, // Start in generic read, will transition to DEPTH_WRITE
				&clearValue,
				IID_PPV_ARGS(&light.ShadowMap)));
			// Create DSV for the shadow map.
			// We need a DSV heap. Let's create one specifically for shadow maps for clarity,
			// or you can extend your existing mDsvHeap if it's not solely for the main depth buffer.

			D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
			dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			dsvDesc.Format = SHADOW_MAP_DSV_FORMAT;
			dsvDesc.Texture2D.MipSlice = 0;
			light.ShadowMapDsvHandle = mShadowDsvHeap->GetCPUDescriptorHandleForHeapStart();
			light.ShadowMapDsvHandle.Offset(i, mDsvDescriptorSize); // Use the stored index
			md3dDevice->CreateDepthStencilView(light.ShadowMap.Get(), &dsvDesc, light.ShadowMapDsvHandle);

			light.ShadowMapSrvHeapIndex = mTextures.size() + 3 + i;
			i++;
		}
	}
}
