#include "TexColumnsApp.h"

// particles
void TexColumnsApp::BuildParticleGeometry()
{
	// Create a single point vertex. Position is irrelevant as it will be overridden by instance data.
	ParticleVertex particleVertex = { DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f) };
	std::uint16_t particleIndex = 0;

	const UINT vbByteSize = sizeof(ParticleVertex);
	const UINT ibByteSize = sizeof(std::uint16_t);



	mParticleVertexBuffer = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), &particleVertex, vbByteSize, mParticleUploadVB);

	mParticleIndexBuffer = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), &particleIndex, ibByteSize, mParticleUploadIB);
}

void TexColumnsApp::BuildParticleBuffers()
{
	// Size for the maximum number of particles
	UINT particleInstanceDataSize = sizeof(ParticleInstanceData);
	UINT bufferSize = MAX_PARTICLES * particleInstanceDataSize;

	// 1. mGpuParticleAppendBuffer: CPU will upload live particle data here.
	//    It's a default heap resource. We'll copy to it.
	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(bufferSize), // Structured buffer
		D3D12_RESOURCE_STATE_COPY_DEST, // Initial state for receiving data from upload buffer
		nullptr,
		IID_PPV_ARGS(&mGpuParticleAppendBuffer)));
	mGpuParticleAppendBuffer->SetName(L"GpuParticleAppendBuffer");

	// 2. mGpuParticleConsumeBuffer: Data is copied here from AppendBuffer.
	//    Rendering shader reads from this as an SRV.
	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(bufferSize), // Structured buffer
		D3D12_RESOURCE_STATE_COMMON, // Initial state, will be transitioned to COPY_DEST then PIXEL_SHADER_RESOURCE
		nullptr,
		IID_PPV_ARGS(&mGpuParticleConsumeBuffer)));
	mGpuParticleConsumeBuffer->SetName(L"GpuParticleConsumeBuffer");

	// 3. mParticleDataUploadBuffer: Intermediate upload heap buffer for CPU to write to.
	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&mParticleDataUploadBuffer)));
	mParticleDataUploadBuffer->SetName(L"ParticleDataUploadBuffer");
}

void TexColumnsApp::BuildParticleRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE srvTable;
	// Particle instance data will be in t0
	srvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_ROOT_PARAMETER slotRootParameter[2];

	slotRootParameter[0].InitAsDescriptorTable(1, &srvTable, D3D12_SHADER_VISIBILITY_VERTEX); // SRV for particle data visible to VS
	slotRootParameter[1].InitAsConstantBufferView(0); // b0 for PassConstants (ViewProj matrix etc.)

	auto staticSamplers = GetStaticSamplers(); // You can reuse existing samplers if needed

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		2, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(), // Or 0, nullptr if no samplers needed for basic points
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
		IID_PPV_ARGS(&mParticleRootSignature)));
	mParticleRootSignature->SetName(L"ParticleRootSignature");
}

void TexColumnsApp::BuildParticlePSO()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC particlePsoDesc;
	ZeroMemory(&particlePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	particlePsoDesc.InputLayout = { mParticleInputLayout.data(), (UINT)mParticleInputLayout.size() };
	particlePsoDesc.pRootSignature = mParticleRootSignature.Get();
	particlePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["particleVS"]->GetBufferPointer()),
		mShaders["particleVS"]->GetBufferSize()
	};
	particlePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["particlePS"]->GetBufferPointer()),
		mShaders["particlePS"]->GetBufferSize()
	};
	particlePsoDesc.GS =
	{
		reinterpret_cast<BYTE*>(mShaders["particleGS"]->GetBufferPointer()),
		mShaders["particleGS"]->GetBufferSize()
	};

	particlePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	particlePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	// particlePsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // Points are usually not culled

	// Blend state for transparent particles (additive or alpha blend)
	D3D12_RENDER_TARGET_BLEND_DESC transparencyBlendDesc;
	transparencyBlendDesc.BlendEnable = FALSE;
	transparencyBlendDesc.LogicOpEnable = FALSE;
	transparencyBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA; // Common for alpha blending
	transparencyBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	transparencyBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	transparencyBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	transparencyBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	transparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	particlePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // Start with default
	particlePsoDesc.BlendState.RenderTarget[0] = transparencyBlendDesc; // Apply blend to RT0

	particlePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	particlePsoDesc.DepthStencilState.DepthEnable = TRUE;
	// For particles, you might want to disable depth writes but keep depth tests
	// particlePsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	// particlePsoDesc.DepthStencilState.DepthEnable = TRUE;


	particlePsoDesc.SampleMask = UINT_MAX;
	particlePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT; // Render as points
	// If you decide to make particles quads (via geometry shader or expanding in VS),
	// this would be D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE. And your dummy geometry
	// would be a quad, and DrawIndexedInstanced would use 6 indices for a quad.

	particlePsoDesc.NumRenderTargets = 1;
	particlePsoDesc.RTVFormats[0] = mBackBufferFormat; // Render to the scene texture or back buffer
	particlePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	particlePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	particlePsoDesc.DSVFormat = mDepthStencilFormat;

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&particlePsoDesc, IID_PPV_ARGS(&mPSOs["particles"])));
}

void TexColumnsApp::UpdateParticles(const GameTimer& gt)
{
	if (!mParticleSystem) return;
	ImGui::Begin("Particle Settings");

	ImGui::Text("Emit Settings");
	ImGui::DragInt("Emit Rate", &emitRate, 1, 0, 1000);



	ImGui::Text("Spread");
	ImGui::DragFloat("Spread", &mParticleSystem->spread, 0.01f, 0, 10);

	ImGui::Text("Strength");
	ImGui::DragFloat("Min Strength", &mParticleSystem->min_strength, 0.1f, 0, mParticleSystem->max_strength);

	ImGui::DragFloat("Max Strength", &mParticleSystem->max_strength, 0.1f, mParticleSystem->min_strength, 100);

	ImGui::Text("Size");
	ImGui::DragFloat("Min Size", &mParticleSystem->min_size, 0.01f, 0, mParticleSystem->max_size);

	ImGui::DragFloat("Max Size", &mParticleSystem->max_size, 0.01f, mParticleSystem->min_size, 1);

	ImGui::Text("Lifespan");
	ImGui::DragFloat("Min Lifespan", &mParticleSystem->min_lifespan, 0.01f, 0, mParticleSystem->max_lifespan);

	ImGui::DragFloat("Max Lifespan", &mParticleSystem->max_lifespan, 0.01f, mParticleSystem->min_lifespan, 3);

	ImGui::Text("Weight");
	ImGui::DragFloat("Min Weight", &mParticleSystem->min_weight, 0.01f, 0, mParticleSystem->max_weight);

	ImGui::DragFloat("Max Weight", &mParticleSystem->max_weight, 0.01f, mParticleSystem->min_weight, 30);



	ImGui::End();

	// Emit some particles (e.g., every few frames or based on time)
	// This is just an example, control emission as you like
	static float timeToEmit = 0.0f;
	timeToEmit += gt.DeltaTime();
	if (timeToEmit > 0.016f) // Roughly 60 FPS emission rate
	{
		mParticleSystem->Emit(emitRate); // Emit 5 new particles
		timeToEmit = 0.0f;
	}
	if (GetAsyncKeyState('P') & 0x8000) // Hold P to emit
	{
		mParticleSystem->Emit(50);
	}




	mParticleSystem->Update(gt.DeltaTime(), mParticleGravity);

	// Get render data from the particle system
	const auto& particleRenderData = mParticleSystem->GetParticleRenderData();
	int numLiveParticles = mParticleSystem->GetAliveParticleCount();
	if (numLiveParticles > 0)
	{
		// Upload particle data to mGpuParticleAppendBuffer via mParticleDataUploadBuffer
		UINT bufferSize = numLiveParticles * sizeof(ParticleInstanceData);
		if (bufferSize == 0) return; // No particles to upload

		// Map the upload buffer
		void* pMappedData = nullptr;
		ThrowIfFailed(mParticleDataUploadBuffer->Map(0, nullptr, &pMappedData));
		memcpy(pMappedData, particleRenderData.data(), bufferSize);
		mParticleDataUploadBuffer->Unmap(0, nullptr);

		// Get the current command list (assuming it's reset and ready for Update related copies)
		// This part is tricky as Update() usually doesn't record commands.
		// A common pattern is to do these uploads just before DeferredDraw() starts,
		// or in a dedicated resource update phase that uses a command list.

		// For now, let's assume we do this copy in DeferredDraw right before using the data.
		// So, this UpdateParticles just prepares the CPU-side data.
		// The actual upload will be handled in DeferredDraw.
	}
}

void TexColumnsApp::DrawParticles(ID3D12GraphicsCommandList* cmdList)
{
	if (!mParticleSystem || mParticleSystem->GetAliveParticleCount() == 0)
	{
		return;
	}

	int numLiveParticles = mParticleSystem->GetAliveParticleCount();
	const auto& particleRenderData = mParticleSystem->GetParticleRenderData();
	UINT dataSize = numLiveParticles * sizeof(ParticleInstanceData);

	if (dataSize == 0) return;

	// 1. Upload data from CPU to mParticleDataUploadBuffer, then copy to mGpuParticleAppendBuffer
	// This part is sensitive to when the command list is open and ready.
	// Assuming cmdList is the main command list used for DeferredDraw.
	{
		void* pMappedData = nullptr;
		ThrowIfFailed(mParticleDataUploadBuffer->Map(0, nullptr, &pMappedData));
		memcpy(pMappedData, particleRenderData.data(), dataSize);
		mParticleDataUploadBuffer->Unmap(0, nullptr);

		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mGpuParticleAppendBuffer.Get(),
			D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));

		cmdList->CopyBufferRegion(
			mGpuParticleAppendBuffer.Get(), // Dest
			0,                             // DestOffset
			mParticleDataUploadBuffer.Get(),// Src
			0,                             // SrcOffset
			dataSize);                     // NumBytes

		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mGpuParticleAppendBuffer.Get(),
			D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE)); // Ready for next copy
	}

	// 2. Copy from mGpuParticleAppendBuffer to mGpuParticleConsumeBuffer
	{
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mGpuParticleConsumeBuffer.Get(),
			D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));

		cmdList->CopyResource(mGpuParticleConsumeBuffer.Get(), mGpuParticleAppendBuffer.Get());

		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mGpuParticleConsumeBuffer.Get(),
			D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)); // Ready for shader to read
	}

	// 3. Set PSO and Root Signature
	cmdList->SetPipelineState(mPSOs["particles"].Get());
	cmdList->SetGraphicsRootSignature(mParticleRootSignature.Get());


	// Corrected setting VB/IB:
	D3D12_VERTEX_BUFFER_VIEW vbv;
	vbv.BufferLocation = mParticleVertexBuffer->GetGPUVirtualAddress();
	vbv.StrideInBytes = sizeof(ParticleVertex);
	vbv.SizeInBytes = sizeof(ParticleVertex);
	cmdList->IASetVertexBuffers(0, 1, &vbv);

	D3D12_INDEX_BUFFER_VIEW ibv;
	ibv.BufferLocation = mParticleIndexBuffer->GetGPUVirtualAddress();
	ibv.Format = DXGI_FORMAT_R16_UINT;
	ibv.SizeInBytes = sizeof(std::uint16_t);
	cmdList->IASetIndexBuffer(&ibv);

	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST); // Set to point list

	// 5. Bind PassConstants (assuming b0 for particles)
	auto passCB = mCurrFrameResource->PassCB->Resource(); // Use the main pass CB
	cmdList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress()); // Slot 1 for PassCB in particle root sig

	// 6. Bind Particle SRV (mGpuParticleConsumeBuffer)
	// You need the GPU descriptor handle for the SRV of mGpuParticleConsumeBuffer.
	// Assuming you stored mParticleSrvHeapIndex:
	CD3DX12_GPU_DESCRIPTOR_HANDLE particleSrvHandle(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	if (mParticleSrvHeapIndex == -1) { /* Error or not initialized */ return; } // Add mParticleSrvHeapIndex member
	particleSrvHandle.Offset(mParticleSrvHeapIndex, mCbvSrvDescriptorSize); // Use the member index

	cmdList->SetGraphicsRootDescriptorTable(0, particleSrvHandle); // Slot 0 for ParticleData SRV

	// 7. Draw
	cmdList->DrawIndexedInstanced(
		1,                  // IndexCountPerInstance (1 for our single point)
		numLiveParticles,   // InstanceCount
		0,                  // StartIndexLocation
		0,                  // BaseVertexLocation
		0);                 // StartInstanceLocation

	// Transition mGpuParticleConsumeBuffer back to common if needed for next frame's copy
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mGpuParticleConsumeBuffer.Get(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON));
	// Transition mGpuParticleAppendBuffer back to common if needed for next frame's copy
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mGpuParticleAppendBuffer.Get(),
		D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON));

}