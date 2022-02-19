#pragma once
#include "stdafx.h"
#include "DXSample.h"

#include<Resource/FrameResource.h>
#include<Resource/Texture.h>
#include<Resource/Material.h>
#include<Resource/Camera.h>
#include<Resource/GameTimer.h>
#include <Resource/Mesh.h>
#include<Resource/GeometryGenerator.h>
#include <DXRuntime/Device.h>
#include <Resource/UploadBuffer.h>
#include <Resource/RenderItem.h>
#include<Resource/Pass.h>
#include<Resource/Light.h>
#include<Resource/CubeRenderTarget.h>
#include<Resource/DescriptorHeap.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;
class D3D12HelloTriangle : public DXSample {
public:
	D3D12HelloTriangle(uint32_t width, uint32_t height, std::wstring name);
	void OnInit() override;
	void OnUpdate() override;
	void OnRender() override;
	void OnDestroy() override;

	void OnKeyboardInput()override;
	void OnMouseDown(WPARAM btnState, int x, int y) override;
	void OnMouseUp(WPARAM btnState, int x, int y) override;
	void OnMouseMove(WPARAM btnState, int x, int y) override;
	
private:
	static const uint32_t FrameCount = 2;
	static const uint32_t FrameResourceCount = 3;
	static const uint32_t GeoBufferCount = 3;

	std::unique_ptr<Device> device;
	// Pipeline objects.
	CD3DX12_VIEWPORT m_viewport;
	CD3DX12_RECT m_scissorRect;
	ComPtr<IDXGISwapChain3> m_swapChain;
	ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
	ComPtr<ID3D12Resource> m_geoBuffer[GeoBufferCount];
	ComPtr<ID3D12Resource> m_depthStencilBuffer;

	std::vector<std::unique_ptr<FrameResource>> m_frameResources;
	FrameResource* m_currFrameResource = nullptr;
	int m_currFrameResourceIndex = 0;

	std::vector<std::unique_ptr<RenderItem>> m_renderItems;
	std::unordered_map<std::string, std::unique_ptr<Texture>> m_textures;
	std::unordered_map<std::string, std::unique_ptr<Material>> m_materials;
	std::unordered_map < std::string, std::unique_ptr<Pass>> m_Passes;
	
	std::unique_ptr<CubeRenderTarget> m_prefilterMap = nullptr;
	std::array<std::unique_ptr< CubeRenderTarget>, 5> m_prefilterSpecMap;
	ComPtr<ID3D12CommandAllocator> m_commandAllocator;
	ComPtr<ID3D12CommandQueue> m_commandQueue;

	ComPtr<ID3D12RootSignature> m_rootSignature;
	ComPtr<ID3D12RootSignature> m_quadRootSignature;
	ComPtr<ID3D12RootSignature> m_skyboxSignature;

	std::unique_ptr<DescriptorHeap> m_srvHeap;  //3 geo buffer, 2 albedo textures, 1 skybox cubemap, 1 prefiltered ambient env, 1 UE_LUT, 5 prefilter spec maps
	std::unique_ptr<DescriptorHeap> m_rtvHeap;  //2 swapchian back buffer, 3 geo pass buffer, 6 prefiltered ambient env face
	std::unique_ptr<DescriptorHeap> m_dsvHeap;  //1		


	//index 2:skybox cube map, index 3: filtered cube map

	ComPtr<ID3D12PipelineState> m_pipelineState;
	ComPtr<ID3D12PipelineState> m_prefilterPass;
	ComPtr<ID3D12PipelineState> m_geometryPass;
	ComPtr<ID3D12PipelineState> m_lightPass;
	ComPtr<ID3D12PipelineState> m_skyboxPass;
	ComPtr<ID3D12PipelineState> m_prefilterMapPass;

	ComPtr<ID3D12GraphicsCommandList> m_commandList;

	// App resources.
	std::unique_ptr<Mesh> m_staticOpaqueMesh;
	std::unique_ptr<Mesh> m_screenQuad;
	std::unique_ptr<Mesh> m_boxMesh;


	// Synchronization objects.
	uint32_t m_frameIndex;
	HANDLE m_fenceEvent;
	ComPtr<ID3D12Fence> m_fence;
	uint64_t m_fenceValue;
	Camera m_camera;
	GameTimer m_timer;

	POINT m_lastMousePos;

	void LoadPipeline();
	void LoadAssets();
	void Precompute();
	void UpdateMaterialBuffer();
	void WaitForPreviousFrame();
	void DrawRenderItems();
	void DeferRender();
	void DrawLights();
	void DrawSkybox();
	void UpdatePassConstant();
	void UpdateObjectConstant();
	void PrepareDeferResource();
	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

};
