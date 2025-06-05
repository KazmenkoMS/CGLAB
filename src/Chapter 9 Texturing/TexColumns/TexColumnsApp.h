#pragma once
#include "../../Common/Camera.h"
#include "../../Common/d3dApp.h"
#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"
#include "../../Common/GeometryGenerator.h"
#include "ParticleSystem.h"
#include "FrameResource.h"
#include <filesystem>
#include <iostream>

#include "imgui_impl_dx12.h"
#include "imgui_impl_win32.h"
#include "imgui.h"

static Camera cam;
static int imguiID = 0;
static int emitRate = 5;
using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")


struct ParticleVertex
{
	DirectX::XMFLOAT3 Pos;
};
// Lightweight structure stores parameters to draw a shape.  This will
// vary from app-to-app.
struct RenderItem
{
	RenderItem() = default;
	RenderItem(const RenderItem& rhs) = delete;

	// World matrix of the shape that describes the object's local space
	// relative to the world space, which defines the position, orientation,
	// and scale of the object in the world.
	XMFLOAT4X4 World = MathHelper::Identity4x4();
	XMMATRIX ScaleM = XMMatrixIdentity();
	XMMATRIX RotationM = XMMatrixIdentity();
	XMMATRIX TranslationM = XMMatrixIdentity();
	XMFLOAT3 Position = { 0.0f, 0.0f, 2.0f };
	XMFLOAT3 RotationAngle = { 0.0f, .0f, 0.0f };
	XMFLOAT3 Scale = { 1.0f, 1.0f, 1.0f };
	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify obect data we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT ObjCBIndex = -1;

	Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;

	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// DrawIndexedInstanced parameters.
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;
	std::string Name;
};

class TexColumnsApp : public D3DApp
{
public:
	TexColumnsApp(HINSTANCE hInstance);
	TexColumnsApp(const TexColumnsApp& rhs) = delete;
	TexColumnsApp& operator=(const TexColumnsApp& rhs) = delete;
	~TexColumnsApp();

	virtual bool Initialize()override;



private:
	virtual void OnResize()override;
	virtual void Update(const GameTimer& gt)override;
	virtual void DeferredDraw(const GameTimer& gt)override;
	virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y)override;
	virtual void MoveBackFwd(float step)override;
	virtual void MoveLeftRight(float step)override;
	virtual void MoveUpDown(float step)override;
	void OnKeyPressed(const GameTimer& gt, WPARAM key) override;
	void OnKeyReleased(const GameTimer& gt, WPARAM key) override;
	std::wstring GetCamSpeed() override;
	void UpdateCamera(const GameTimer& gt);
	void BuildShadowMapViews();
	void AnimateMaterials(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateLightCBs(const GameTimer& gt);
	void UpdateMaterialCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	void CreateGBuffer() override;
	void CreateSceneTexture();
	void LoadAllTextures();
	void LoadTexture(const std::string& name);
	void BuildRootSignature();
	void BuildLightingRootSignature();
	void BuildShadowPassRootSignature();
	void BuildPostProcessRootSignature();
	void BuildLights();
	void SetLightShapes();
	void BuildDescriptorHeaps();
	void BuildShadersAndInputLayout();
	void BuildShapeGeometry();
	void BuildPSOs();
	void BuildFrameResources();
	void CreateMaterial(std::string _name, int _CBIndex, int _SRVDiffIndex, int _SRVNMapIndex, XMFLOAT4 _DiffuseAlbedo, XMFLOAT3 _FresnelR0, float _Roughness);
	void BuildMaterials();
	void RenderCustomMesh(std::string unique_name, std::string meshname, std::string materialName, XMFLOAT3 Scale, XMFLOAT3 Rotation, XMFLOAT3 Position);
	void BuildCustomMeshGeometry(std::string name, UINT& meshVertexOffset, UINT& meshIndexOffset, UINT& prevVertSize, UINT& prevIndSize, std::vector<Vertex>& vertices, std::vector<std::uint16_t>& indices, MeshGeometry* Geo);
	void BuildRenderItems();
	void DrawSceneToShadowMap();
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> GetStaticSamplers();
	void CreateSpotLight(XMFLOAT3 pos, XMFLOAT3 rot, XMFLOAT3 color, float faloff_start, float faloff_end, float strength, float spotpower);
	void CreatePointLight(XMFLOAT3 pos, XMFLOAT3 color, float faloff_start, float faloff_end, float strength);

	// Add new methods for particles
	void BuildParticleGeometry();
	void BuildParticleBuffers();
	void BuildParticleRootSignature();
	void BuildParticlePSO();
	void UpdateParticles(const GameTimer& gt);
	void DrawParticles(ID3D12GraphicsCommandList* cmdList);

	void InitImGUI();

private:
	std::unordered_map<std::string, unsigned int>ObjectsMeshCount;
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;
	//
	std::unordered_map<std::string, int>TexOffsets;
	//
	UINT mCbvSrvDescriptorSize = 0;

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	ComPtr<ID3D12RootSignature> mLightingRootSignature = nullptr;
	ComPtr<ID3D12RootSignature> mShadowPassRootSignature = nullptr;

	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;
	ComPtr<ID3D12DescriptorHeap> m_ImGuiSrvDescriptorHeap; // Member variable

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;
	std::vector<Light>mLights;
	// Render items divided by PSO.
	std::vector<RenderItem*> mOpaqueRitems;

	PassConstants mMainPassCB;
	XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	float mTheta = 1.5f * XM_PI;
	float mPhi = 0.2f * XM_PI;
	float mRadius = 15.0f;

	POINT mLastMousePos;

	// G-Buffer ресурсы
	ComPtr<ID3D12Resource> mGBufferPosition;
	ComPtr<ID3D12Resource> mGBufferNormal;
	ComPtr<ID3D12Resource> mGBufferAlbedo;
	ComPtr<ID3D12Resource> mGBufferDepthStencil;
	ComPtr<ID3D12DescriptorHeap> mGBufferSrvHeap = nullptr;

	// Дескрипторы для G-Buffer
	CD3DX12_CPU_DESCRIPTOR_HANDLE mGBufferRTVs[3]; // 0:Position, 1:Normal, 2:Albedo
	CD3DX12_CPU_DESCRIPTOR_HANDLE mGBufferDSV;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mGBufferSRVs[3]; // SRV для шейдеров

	UINT mGBufferRTVDescriptorSize;
	UINT mGBufferDSVDescriptorSize;

	// shadow resources 
	const UINT SHADOW_MAP_WIDTH = 2048;
	const UINT SHADOW_MAP_HEIGHT = 2048;
	const DXGI_FORMAT SHADOW_MAP_FORMAT = DXGI_FORMAT_R24G8_TYPELESS; // Resource format
	const DXGI_FORMAT SHADOW_MAP_DSV_FORMAT = DXGI_FORMAT_D24_UNORM_S8_UINT; // DSV format
	const DXGI_FORMAT SHADOW_MAP_SRV_FORMAT = DXGI_FORMAT_R24_UNORM_X8_TYPELESS; // SRV format
	Microsoft::WRL::ComPtr<ID3D12Resource> mShadowMap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mShadowDsvHeap; // A separate heap for shadow map DSVs
	D3D12_VIEWPORT mShadowViewport;
	D3D12_RECT mShadowScissorRect;

	// Размеры как у окна
	UINT width = mClientWidth;
	UINT height = mClientHeight;

	// Форматы:
	const DXGI_FORMAT positionFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
	const DXGI_FORMAT normalFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	const DXGI_FORMAT albedoFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	// post-process resources
	ComPtr<ID3D12Resource> mSceneTexture;        // Texture to hold the lit scene
	CD3DX12_CPU_DESCRIPTOR_HANDLE mSceneRtvHandle;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mSceneSrvHandle; // GPU handle for the SRV
	UINT mSceneSrvHeapIndex = -1; // Index in your main SRV heap if you combine them
	ComPtr<ID3D12RootSignature> mPostProcessRootSignature = nullptr;
	std::unique_ptr<UploadBuffer<float>> mChromaticAberrationCB = nullptr; // Constant buffer for offset


	// Particle System Members
	std::unique_ptr<ParticleSystem> mParticleSystem;
	DirectX::XMFLOAT3 mParticleGravity = { 0.0f, -9.81f, 0.0f }; // Example gravity
	const int MAX_PARTICLES = 100000; // Max particles for the system

	ComPtr<ID3D12Resource> mParticleVertexBuffer; // Dummy VB for a single point
	ComPtr<ID3D12Resource> mParticleIndexBuffer;  // Dummy IB for a single point
	ComPtr<ID3D12Resource> mParticleUploadVB;     // For uploading dummy VB
	ComPtr<ID3D12Resource> mParticleUploadIB;     // For uploading dummy IB

	// These are our two main structured buffers as per the requirement
	ComPtr<ID3D12Resource> mGpuParticleAppendBuffer; // CPU writes live particle data here (conceptual "append")
	ComPtr<ID3D12Resource> mGpuParticleConsumeBuffer; // Copied from AppendBuffer, rendering shader "consumes" from here (SRV)

	ComPtr<ID3D12Resource> mParticleDataUploadBuffer; // Intermediate upload buffer for particle instance data

	ComPtr<ID3D12RootSignature> mParticleRootSignature;
	ComPtr<ID3D12PipelineState> mParticlePSO;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mParticleInputLayout;
	UINT mParticleSrvHeapIndex = 0;

	// Somewhere accessible for ImGui or update logic
	float gChromaticAberrationOffset = 0.000f;
};