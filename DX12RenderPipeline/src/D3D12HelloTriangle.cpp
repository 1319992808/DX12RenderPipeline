#include "Metalib.h"
#include "D3D12HelloTriangle.h"

D3D12HelloTriangle::D3D12HelloTriangle(uint32_t width, uint32_t height, std::wstring name)
	: DXSample(width, height, name),
	  m_frameIndex(0),
	  m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
	  m_scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height))
	{ }

void D3D12HelloTriangle::OnInit() {
	LoadPipeline();
	LoadAssets();
	PrepareDeferResource();
	Precompute();
}

void D3D12HelloTriangle::LoadPipeline() {

	//Create the device;
	device = std::make_unique<Device>();
	//No support fullscreen transitions.
	ThrowIfFailed(device->DxgiFactory()->MakeWindowAssociation(Win32Application::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));
	
	//Create the command queue and command allocator
	{
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		ThrowIfFailed(device->DxDevice()->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));
		ThrowIfFailed(device->DxDevice()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
	}

	//Create the swap chain
	{
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
		swapChainDesc.BufferCount = FrameCount;
		swapChainDesc.Width = m_width;
		swapChainDesc.Height = m_height;
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.SampleDesc.Count = 1;
		ComPtr<IDXGISwapChain1> swapChain;
		ThrowIfFailed(device->DxgiFactory()->CreateSwapChainForHwnd(
			m_commandQueue.Get(),// Swap chain needs the queue so that it can force a flush on it.
			Win32Application::GetHwnd(),
			&swapChainDesc,
			nullptr,
			nullptr,
			&swapChain));
		ThrowIfFailed(swapChain.As(&m_swapChain)); //Query interface, which is use as version 3
	}
	//Current frame index
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	//Descriptor heaps.
	m_srvHeap = std::make_unique<DescriptorHeap>(device.get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 13, true);
	m_rtvHeap = std::make_unique<DescriptorHeap>(device.get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 11, false);
	m_dsvHeap = std::make_unique<DescriptorHeap>(device.get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);

	// Get back buffer resources and populate rtv heap
	for (uint32_t i = 0; i < FrameCount; ++i) {
		ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])));
		m_rtvHeap->CreateRTV(m_renderTargets[i].Get(), nullptr, i);
	}
	
	//Create ds resource and populate dsv heap 
	{
		D3D12_RESOURCE_DESC depthStencilDesc;
		depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		depthStencilDesc.Alignment = 0;
		depthStencilDesc.Width = m_width;
		depthStencilDesc.Height = m_height;
		depthStencilDesc.DepthOrArraySize = 1;
		depthStencilDesc.MipLevels = 1;
		depthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS; //May act as uav later
		depthStencilDesc.SampleDesc.Count = 1;
		depthStencilDesc.SampleDesc.Quality = 0;
		depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_CLEAR_VALUE optClear;
		optClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		optClear.DepthStencil.Depth = 1.0f;
		optClear.DepthStencil.Stencil = 0;
		ThrowIfFailed(device->DxDevice()->CreateCommittedResource(
			get_rvalue_ptr(CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT)),
			D3D12_HEAP_FLAG_NONE,
			&depthStencilDesc,
			D3D12_RESOURCE_STATE_COMMON,
			&optClear,
			IID_PPV_ARGS(&m_depthStencilBuffer)));

		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsvDesc.Texture2D.MipSlice = 0;
		device->DxDevice()->CreateDepthStencilView(m_depthStencilBuffer.Get(), &dsvDesc, m_dsvHeap->hCPU());
	}
}

void D3D12HelloTriangle::LoadAssets() {

	ThrowIfFailed(device->DxDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList)));
	ThrowIfFailed(m_commandList->Close());
	ThrowIfFailed(m_commandAllocator->Reset());
	ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), nullptr));
	
	//Define a camera
	m_camera.LookAt(XMFLOAT3(0., 0., -5.), XMFLOAT3(0., 0., 0.), XMFLOAT3(0., 1., 0.));
	m_camera.SetLens(0.25f * MathHelper::Pi, m_width/static_cast<float>(m_height), 1.0f, 1000.0f);
	m_camera.UpdateViewMatrix();
	
	//Timer
	m_timer.Reset();
	m_timer.Start();
	
	//Load textures
	{
		auto pureColorTexture = std::make_unique<Texture>(device.get(), "Brown");
		pureColorTexture->LoadTexture(m_commandList.Get(), L"res/pureBrown.dds"); 
		pureColorTexture->SetViewOffset(0);
		m_srvHeap->CreateSRV(pureColorTexture->GetResource(), get_rvalue_ptr(pureColorTexture->GetSRVDesc()), 3);
		m_textures["Brown"] = std::move(pureColorTexture);
		
		auto bricksTexture = std::make_unique<Texture>(device.get(), "Bricks");
		bricksTexture->LoadTexture(m_commandList.Get(), L"res/bricks.dds");
		bricksTexture->SetViewOffset(1);
		m_srvHeap->CreateSRV(bricksTexture->GetResource(), get_rvalue_ptr(bricksTexture->GetSRVDesc()), 4);
		m_textures["Bricks"] = std::move(bricksTexture);

		
		auto skyboxTexture = std::make_unique<Texture>(device.get(), "Skybox");
		skyboxTexture->LoadTexture(m_commandList.Get(), L"res/Cubemap/sunsetcube1024.dds");
		skyboxTexture->SetViewOffset(2);
		m_srvHeap->CreateSRV(skyboxTexture->GetResource(), get_rvalue_ptr(skyboxTexture->GetCubeMapSRVDesc()), 5);
		m_textures["Skybox"] = std::move(skyboxTexture);

		auto LUT = std::make_unique<Texture>(device.get(), "LUT");
		LUT->LoadTexture(m_commandList.Get(), L"res/UE_LUT.dds");
		LUT->SetViewOffset(7);
		m_srvHeap->CreateSRV(LUT->GetResource(), get_rvalue_ptr(LUT->GetSRVDesc()), 7);
		m_textures["LUT"] = std::move(LUT);
	}
	
	//Build materials
	{
		auto material = std::make_unique<Material>();
		material->DiffuseAlbedo = { 1., 1., 1., 1. };
		material->DiffuseSrvHeapIndex = 0;
		material->Metallic = 1.;
		material->Roughness = 0.;
		material->MatCBIndex = 0;
		material->Name = "MaterialName0";
		material->NormalSrvHeapIndex = -1;

		m_materials["MaterialName0"] = std::move(material);
	}

	//Build render items (Mesh + Material + CB index) 需要重构
	{
		auto boxRitem = std::make_unique<RenderItem>();
		boxRitem->meshData = GeometryGenerator::CreateSphere(3.,100,100); //Data generated, not final commited data
		//May use weak pointer later
		boxRitem->Mat = m_materials["MaterialName0"].get();
		boxRitem->objCBIndex = 0;
		m_renderItems.push_back(std::move(boxRitem));
	}

	//Build Mesh
	
		//This should put outside, or resource already released when gpu command list closed
		std::unique_ptr<UploadBuffer> verticesUploadBuffer;
		std::unique_ptr<UploadBuffer> indicesUploadBuffer;
		{
			GeometryGenerator::MeshData tmpMesh = m_renderItems[0]->meshData;

			static CommonVertex commonVertex; //Vertex committed

			std::vector<vbyte> commonVertexData(commonVertex.structSize * tmpMesh.Vertices.size());
			std::vector<vbyte> indexData(tmpMesh.Indices32.size() * sizeof(uint));

			vbyte* indexDataPtr = indexData.data();
			for (auto& index : tmpMesh.Indices32) {
				size_t ptr = reinterpret_cast<size_t>(indexDataPtr);
				*reinterpret_cast<uint*>(ptr) = index;
				indexDataPtr += sizeof(uint);
			}

			vbyte* commonVertexDataPtr = commonVertexData.data();
			for (auto& vertex : tmpMesh.Vertices) {
				commonVertex.position.Get(commonVertexDataPtr) = vertex.Position;
				commonVertex.normal.Get(commonVertexDataPtr) = vertex.Normal;
				commonVertex.texcoord.Get(commonVertexDataPtr) = vertex.TexC;
				commonVertexDataPtr += commonVertex.structSize;
			}

			verticesUploadBuffer = std::make_unique<UploadBuffer>(
				device.get(),
				commonVertexData.size());

			indicesUploadBuffer = std::make_unique<UploadBuffer>(
				device.get(),
				indexData.size());

			verticesUploadBuffer->CopyData(0, commonVertexData);
			indicesUploadBuffer->CopyData(0, indexData);

			std::vector<rtti::Struct const*> structs;
			structs.emplace_back(&commonVertex);

			m_staticOpaqueMesh = std::make_unique<Mesh>(
				device.get(),
				structs,
				tmpMesh.Vertices.size(),
				tmpMesh.Indices32.size());

			// Copy uploadBuffer to mesh, 将upload拷贝至default,作为vertex buffers
			m_commandList->CopyBufferRegion(
				m_staticOpaqueMesh->VertexBuffers()[0].GetResource(),
				0,
				verticesUploadBuffer->GetResource(),
				0,
				commonVertexData.size());

			m_commandList->CopyBufferRegion(
				m_staticOpaqueMesh->IndexBuffer()->GetResource(),
				0,
				indicesUploadBuffer->GetResource(),
				0,
				indexData.size());
		}
		std::unique_ptr<UploadBuffer> quadUploadBuffer;
		{
			static QuadVertex quadVertex;
			std::vector<vbyte> quadVertexData(quadVertex.structSize * 6);
			vbyte* quadVertexDataPtr = quadVertexData.data();
			quadVertex.position.Get(quadVertexDataPtr) = { -1.0f, 1.0f };
			quadVertex.texcoord.Get(quadVertexDataPtr) = { 0.0f, 0.0f };
			quadVertexDataPtr += quadVertex.structSize;
			quadVertex.position.Get(quadVertexDataPtr) = { -1.0f, -1.0f };
			quadVertex.texcoord.Get(quadVertexDataPtr) = { 0.0f,  1.0f };
			quadVertexDataPtr += quadVertex.structSize;
			quadVertex.position.Get(quadVertexDataPtr) = { 1.0f, 1.0f };
			quadVertex.texcoord.Get(quadVertexDataPtr) = { 1.0f, 0.0f };
			quadVertexDataPtr += quadVertex.structSize;
			quadVertex.position.Get(quadVertexDataPtr) = { -1.0f, -1.0f };
			quadVertex.texcoord.Get(quadVertexDataPtr) = { 0.0f,  1.0f };
			quadVertexDataPtr += quadVertex.structSize;
			quadVertex.position.Get(quadVertexDataPtr) = { 1.0f, -1.0f };
			quadVertex.texcoord.Get(quadVertexDataPtr) = { 1.0f,  1.0f };
			quadVertexDataPtr += quadVertex.structSize;
			quadVertex.position.Get(quadVertexDataPtr) = { 1.0f, 1.0f };
			quadVertex.texcoord.Get(quadVertexDataPtr) = { 1.0f, 0.0f };

			quadUploadBuffer = std::make_unique<UploadBuffer>(
				device.get(),
				quadVertexData.size());
			quadUploadBuffer->CopyData(0, quadVertexData);
			std::vector<rtti::Struct const*> structs;
			structs.emplace_back(&quadVertex);
			m_screenQuad = std::make_unique<Mesh>(device.get(), structs, 6);
			m_commandList->CopyBufferRegion(
				m_screenQuad->VertexBuffers()[0].GetResource(),
				0,
				quadUploadBuffer->GetResource(),
				0,
				quadVertexData.size());

		}

		std::unique_ptr<UploadBuffer> boxUploadBuffer;
		std::unique_ptr<UploadBuffer> boxIndicesUploadBuffer;
		{
			static MiniVertex boxVertex;
			GeometryGenerator::MeshData tmpMesh = GeometryGenerator::CreateBox(2.0f, 2.0f, 2.0f, 0);
			std::vector<vbyte> boxVertexData(boxVertex.structSize * tmpMesh.Vertices.size());
			std::vector<vbyte> indexData(tmpMesh.Indices32.size() * sizeof(uint));
			vbyte* indexDataPtr = indexData.data();
			for (auto& index : tmpMesh.Indices32) {
				size_t ptr = reinterpret_cast<size_t>(indexDataPtr);
				*reinterpret_cast<uint*>(ptr) = index;
				indexDataPtr += sizeof(uint);
			}

			vbyte* boxVertexDataPtr = boxVertexData.data();
			for (auto& vertex : tmpMesh.Vertices) {
				boxVertex.position.Get(boxVertexDataPtr) = XMFLOAT4(vertex.Position.x, vertex.Position.y, vertex.Position.z, 1.0f);
				boxVertexDataPtr += boxVertex.structSize;
			}

			boxUploadBuffer = std::make_unique<UploadBuffer>(device.get(), boxVertexData.size());
			boxIndicesUploadBuffer = std::make_unique<UploadBuffer>(device.get(), indexData.size());

			boxUploadBuffer->CopyData(0, boxVertexData);
			boxIndicesUploadBuffer->CopyData(0, indexData);

			std::vector<rtti::Struct const*> structs;
			structs.emplace_back(&boxVertex);
			m_boxMesh = std::make_unique<Mesh>(device.get(), structs, tmpMesh.Vertices.size(), tmpMesh.Indices32.size());
			m_commandList->CopyBufferRegion(
				m_boxMesh->VertexBuffers()[0].GetResource(),
				0,
				boxUploadBuffer->GetResource(),
				0,
				boxVertexData.size());

			m_commandList->CopyBufferRegion(
				m_boxMesh->IndexBuffer()->GetResource(),
				0,
				boxIndicesUploadBuffer->GetResource(),
				0,
				indexData.size());
		}
	
	
	//Build frame resource
	for (int i = 0; i < FrameResourceCount; i++)
	{
		m_frameResources.push_back(std::make_unique<FrameResource>(device.get(), 1, (UINT)m_renderItems.size(), 1));
	}

	m_commandList->ResourceBarrier(1, get_rvalue_ptr(CD3DX12_RESOURCE_BARRIER::Transition(m_depthStencilBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE)));

	ThrowIfFailed(m_commandList->Close());
	ID3D12CommandList* ppCommandLists[] = {m_commandList.Get()};
	m_commandQueue->ExecuteCommandLists(array_count(ppCommandLists), ppCommandLists);
	
	
	//Root signature
	{
		//Common root signature
		{
			CD3DX12_ROOT_PARAMETER slotRootParameter[4];
			slotRootParameter[0].InitAsConstantBufferView(0);		//per object
			slotRootParameter[1].InitAsConstantBufferView(1);		//per pass
			slotRootParameter[2].InitAsShaderResourceView(0, 1);	//material
			CD3DX12_DESCRIPTOR_RANGE descRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0);//2 2D textures
			slotRootParameter[3].InitAsDescriptorTable(1, &descRange, D3D12_SHADER_VISIBILITY_PIXEL);
			auto staticSamplers = GetStaticSamplers();

			// A root signature is an array of root parameters.
			CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter,
				(UINT)staticSamplers.size(), staticSamplers.data(),
				D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

			ComPtr<ID3DBlob> signature;
			ComPtr<ID3DBlob> error;
			ThrowIfFailed(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
			ThrowIfFailed(device->DxDevice()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
		}
		//Screen quad root signature
		{
			CD3DX12_ROOT_PARAMETER slotQuadRootParameter[2];
			slotQuadRootParameter[0].InitAsConstantBufferView(0);		//lights
			CD3DX12_DESCRIPTOR_RANGE descRange[2];
			descRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0, 0, 0);//3 2D geo buffer. param:num base space offset
			descRange[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 7, 3, 0, 6);//1 LUT 1 2D filtered env light, 5 2D filtered spec light
			slotQuadRootParameter[1].InitAsDescriptorTable(2, descRange, D3D12_SHADER_VISIBILITY_PIXEL);
			auto staticSamplers = GetStaticSamplers();
			// A root signature is an array of root parameters.
			CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotQuadRootParameter,
				(UINT)staticSamplers.size(), staticSamplers.data(),
				D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

			ComPtr<ID3DBlob> signature;
			ComPtr<ID3DBlob> error;
			HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
			if (error != nullptr)
			{
				OutputDebugStringA((char*)error->GetBufferPointer());

				ThrowIfFailed(hr);
			}
			ThrowIfFailed(device->DxDevice()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_quadRootSignature)));
		}

		//sky box root signature
		{
			CD3DX12_ROOT_PARAMETER slotRootParameter[2];
			slotRootParameter[0].InitAsConstantBufferView(0);		//Transform
			CD3DX12_DESCRIPTOR_RANGE descRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);//1 Cube map texture
			slotRootParameter[1].InitAsDescriptorTable(1, &descRange, D3D12_SHADER_VISIBILITY_PIXEL);
			auto staticSamplers = GetStaticSamplers();
			CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameter,
				(UINT)staticSamplers.size(), staticSamplers.data(),
				D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

			ComPtr<ID3DBlob> signature;
			ComPtr<ID3DBlob> error;
			ThrowIfFailed(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
			ThrowIfFailed(device->DxDevice()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_skyboxSignature)));
		}

	}

	// Create the pipeline state, which includes compiling and loading shaders.
	{
		m_Passes["GeometryPass"] = std::make_unique<Pass>(GetAssetFullPath(L"shader/geometryPass.hlsl"));
		m_Passes["LightPass"] = std::make_unique<Pass>(GetAssetFullPath(L"shader/lightPass.hlsl"));
		m_Passes["SkyboxPass"] = std::make_unique<Pass>(GetAssetFullPath(L"shader/skyboxPass.hlsl"));
		m_Passes["PrefilterPass"] = std::make_unique<Pass>(GetAssetFullPath(L"shader/convoluteRadiance.hlsl"));
		m_Passes["PrefilterMap"] = std::make_unique<Pass>(GetAssetFullPath(L"shader/prefilterMap.hlsl"));
		
		{
			//Create the geometry pass, support multi-rts
			D3D12_GRAPHICS_PIPELINE_STATE_DESC geoPassDesc = {};
			auto meshLayout = m_staticOpaqueMesh->Layout();
			geoPassDesc.InputLayout = { meshLayout.data(), uint(meshLayout.size()) };
			geoPassDesc.pRootSignature = m_rootSignature.Get(); //Same structure is ok
			geoPassDesc.VS = CD3DX12_SHADER_BYTECODE(m_Passes["GeometryPass"]->GetVertexShader());
			geoPassDesc.PS = CD3DX12_SHADER_BYTECODE(m_Passes["GeometryPass"]->GetPixelShader());
			geoPassDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			geoPassDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			geoPassDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			geoPassDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
			geoPassDesc.SampleMask = UINT_MAX;
			geoPassDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			geoPassDesc.NumRenderTargets = 3;
			geoPassDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
			geoPassDesc.RTVFormats[1] = DXGI_FORMAT_R16G16B16A16_FLOAT;
			geoPassDesc.RTVFormats[2] = DXGI_FORMAT_R16G16B16A16_FLOAT;
			geoPassDesc.SampleDesc.Count = 1;
			ThrowIfFailed(device->DxDevice()->CreateGraphicsPipelineState(&geoPassDesc, IID_PPV_ARGS(&m_geometryPass)));

			//Create the geometry pass, support multi-rts
			D3D12_GRAPHICS_PIPELINE_STATE_DESC lightPassDesc = {};
			meshLayout = m_screenQuad->Layout();
			lightPassDesc.InputLayout = { meshLayout.data(), uint(meshLayout.size()) };
			lightPassDesc.pRootSignature = m_quadRootSignature.Get();
			lightPassDesc.VS = CD3DX12_SHADER_BYTECODE(m_Passes["LightPass"]->GetVertexShader());
			lightPassDesc.PS = CD3DX12_SHADER_BYTECODE(m_Passes["LightPass"]->GetPixelShader());
			lightPassDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			lightPassDesc.RasterizerState.DepthClipEnable = FALSE;
			lightPassDesc.RasterizerState.FrontCounterClockwise = true;
			lightPassDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			lightPassDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			lightPassDesc.DepthStencilState.DepthEnable = FALSE;
			lightPassDesc.DepthStencilState.StencilEnable = false;
			lightPassDesc.SampleMask = UINT_MAX;
			lightPassDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			lightPassDesc.NumRenderTargets = 1;
			lightPassDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
			lightPassDesc.SampleDesc.Count = 1;
			ThrowIfFailed(device->DxDevice()->CreateGraphicsPipelineState(&lightPassDesc, IID_PPV_ARGS(&m_lightPass)));

			//Draw last with the depth buffer
			{
				D3D12_GRAPHICS_PIPELINE_STATE_DESC skyboxPassDesc = {};
				meshLayout = m_boxMesh->Layout();
				skyboxPassDesc.InputLayout = { meshLayout.data(), uint(meshLayout.size()) };
				skyboxPassDesc.pRootSignature = m_skyboxSignature.Get(); //Same as skybox signature
				skyboxPassDesc.VS = CD3DX12_SHADER_BYTECODE(m_Passes["SkyboxPass"]->GetVertexShader());
				skyboxPassDesc.PS = CD3DX12_SHADER_BYTECODE(m_Passes["SkyboxPass"]->GetPixelShader());
				skyboxPassDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				skyboxPassDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
				skyboxPassDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				skyboxPassDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
				skyboxPassDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; //Equal only will cause z fighting!
				skyboxPassDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
				skyboxPassDesc.SampleMask = UINT_MAX;
				skyboxPassDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				skyboxPassDesc.NumRenderTargets = 1;
				skyboxPassDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
				skyboxPassDesc.SampleDesc.Count = 1;
				ThrowIfFailed(device->DxDevice()->CreateGraphicsPipelineState(&skyboxPassDesc, IID_PPV_ARGS(&m_skyboxPass)));
			}
			{
				D3D12_GRAPHICS_PIPELINE_STATE_DESC prefilterPassDesc = {};
				meshLayout = m_boxMesh->Layout();
				prefilterPassDesc.InputLayout = { meshLayout.data(), uint(meshLayout.size()) };
				prefilterPassDesc.pRootSignature = m_skyboxSignature.Get();
				prefilterPassDesc.VS = CD3DX12_SHADER_BYTECODE(m_Passes["PrefilterPass"]->GetVertexShader());
				prefilterPassDesc.PS = CD3DX12_SHADER_BYTECODE(m_Passes["PrefilterPass"]->GetPixelShader());
				prefilterPassDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				prefilterPassDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
				prefilterPassDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				prefilterPassDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
				prefilterPassDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; //Equal only will cause z fighting!
				prefilterPassDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
				prefilterPassDesc.SampleMask = UINT_MAX;
				prefilterPassDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				prefilterPassDesc.NumRenderTargets = 1;
				prefilterPassDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				prefilterPassDesc.SampleDesc.Count = 1;
				ThrowIfFailed(device->DxDevice()->CreateGraphicsPipelineState(&prefilterPassDesc, IID_PPV_ARGS(&m_prefilterPass)));			
			}
			{
				D3D12_GRAPHICS_PIPELINE_STATE_DESC prefilterMapDesc = {};
				meshLayout = m_boxMesh->Layout();
				prefilterMapDesc.InputLayout = { meshLayout.data(), uint(meshLayout.size())};
				prefilterMapDesc.pRootSignature = m_skyboxSignature.Get();
				prefilterMapDesc.VS = CD3DX12_SHADER_BYTECODE(m_Passes["PrefilterMap"]->GetVertexShader());
				prefilterMapDesc.PS = CD3DX12_SHADER_BYTECODE(m_Passes["PrefilterMap"]->GetPixelShader());
				prefilterMapDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				prefilterMapDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
				prefilterMapDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				prefilterMapDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
				prefilterMapDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; //Equal only will cause z fighting!
				prefilterMapDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
				prefilterMapDesc.SampleMask = UINT_MAX;
				prefilterMapDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				prefilterMapDesc.NumRenderTargets = 1;
				prefilterMapDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				prefilterMapDesc.SampleDesc.Count = 1;
				ThrowIfFailed(device->DxDevice()->CreateGraphicsPipelineState(&prefilterMapDesc, IID_PPV_ARGS(&m_prefilterMapPass)));
			}
		}
	}

	// Create synchronization objects and wait until assets have been uploaded to the GPU.
	{
		ThrowIfFailed(device->DxDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
		m_fenceValue = 1;

		// Create an event handle to use for frame synchronization.
		m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (m_fenceEvent == nullptr) {
			ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
		}
		WaitForPreviousFrame();
	}
}

void D3D12HelloTriangle::OnMouseDown(WPARAM btnState, int x, int y) {
	m_lastMousePos.x = x;
	m_lastMousePos.y = y;
	SetFocus(Win32Application::GetHwnd());
}

void D3D12HelloTriangle::OnMouseUp(WPARAM btnState, int x, int y) {
	SetFocus(Win32Application::GetHwnd());

}

void D3D12HelloTriangle::OnMouseMove(WPARAM btnState, int x, int y) {

	if ((btnState & MK_LBUTTON) != 0)
	{
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.5f * static_cast<float>(x - m_lastMousePos.x));
		float dy = XMConvertToRadians(0.5f * static_cast<float>(y - m_lastMousePos.y));

		m_camera.Pitch(dy);
		m_camera.RotateY(dx);
	}

	m_lastMousePos.x = x;
	m_lastMousePos.y = y;
}

// Update frame-based values.
void D3D12HelloTriangle::OnUpdate() {

	m_timer.Tick();
	OnKeyboardInput();
	
	m_currFrameResourceIndex = (m_currFrameResourceIndex + 1) % FrameResourceCount;
	m_currFrameResource = m_frameResources[m_currFrameResourceIndex].get();

	if (m_currFrameResource->Fence != 0 //First iteration should not wait? Don't get it here
		&& m_fence->GetCompletedValue() < m_currFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(m_fence->SetEventOnCompletion(m_currFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	UpdateMaterialBuffer();
	UpdateObjectConstant();
	UpdatePassConstant();
	
}

// Render the scene.
void D3D12HelloTriangle::OnRender() {

	ThrowIfFailed(m_currFrameResource->CmdListAlloc->Reset()); //Reset when alloc commands finished executing
		
	DeferRender();
	
	ThrowIfFailed(m_swapChain->Present(1, 0));
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
	m_currFrameResource->Fence = ++m_fenceValue;
	m_commandQueue->Signal(m_fence.Get(), m_fenceValue);
}

void D3D12HelloTriangle::OnDestroy() {
	// Ensure that the GPU is no longer referencing resources that are about to be
	// cleaned up by the destructor.
	WaitForPreviousFrame();

	CloseHandle(m_fenceEvent);
}



void D3D12HelloTriangle::DeferRender() {

	//Geometry Pass
	ThrowIfFailed(m_commandList->Reset(m_currFrameResource->CmdListAlloc.Get(), m_geometryPass.Get())); //Reset after commiting to command queue
	m_commandList->RSSetViewports(1, &m_viewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);
	m_commandList->ResourceBarrier(1, get_rvalue_ptr(CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET)));

	m_commandList->OMSetRenderTargets(3, get_rvalue_ptr(m_rtvHeap->hCPU(2)), TRUE, get_rvalue_ptr(m_dsvHeap->hCPU(0)));
	const float clearValue[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	m_commandList->ClearRenderTargetView(m_rtvHeap->hCPU(2), clearValue, 0, nullptr);
	m_commandList->ClearRenderTargetView(m_rtvHeap->hCPU(3), clearValue, 0, nullptr);
	m_commandList->ClearRenderTargetView(m_rtvHeap->hCPU(4), clearValue, 0, nullptr);
	m_commandList->ClearDepthStencilView(m_dsvHeap->hCPU(0), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	ID3D12DescriptorHeap* srvHeaps[] = { m_srvHeap->GetHeap()};
	m_commandList->SetDescriptorHeaps(_countof(srvHeaps), srvHeaps);
	m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
	auto passCB = m_currFrameResource->GeometryPassConstantBuffer->GetResource();
	m_commandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());
	auto matBuffer = m_currFrameResource->MaterialDataResource->GetResource();
	m_commandList->SetGraphicsRootShaderResourceView(2, matBuffer->GetGPUVirtualAddress());
	m_commandList->SetGraphicsRootDescriptorTable(3, m_srvHeap->hGPU(4));
	DrawRenderItems();
	
	//Light Pass, render screem quad
	
	m_commandList->SetPipelineState(m_lightPass.Get());
	m_commandList->ResourceBarrier(1, get_rvalue_ptr(CD3DX12_RESOURCE_BARRIER::Transition(m_geoBuffer[0].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ)));
	m_commandList->ResourceBarrier(1, get_rvalue_ptr(CD3DX12_RESOURCE_BARRIER::Transition(m_geoBuffer[1].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ)));
	m_commandList->ResourceBarrier(1, get_rvalue_ptr(CD3DX12_RESOURCE_BARRIER::Transition(m_geoBuffer[2].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ)));
	m_commandList->RSSetViewports(1, &m_viewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);
	m_commandList->OMSetRenderTargets(1, get_rvalue_ptr(m_rtvHeap->hCPU(m_frameIndex)), FALSE, nullptr);
	const float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	m_commandList->ClearRenderTargetView(m_rtvHeap->hCPU(m_frameIndex), clearColor, 0, nullptr);

	m_commandList->SetGraphicsRootSignature(m_quadRootSignature.Get());
	m_commandList->SetGraphicsRootConstantBufferView(0, m_currFrameResource->LightPassConstantBuffer->GetResource()->GetGPUVirtualAddress());
	m_commandList->SetGraphicsRootDescriptorTable(1, m_srvHeap->hGPU(0));
	DrawLights();

	m_commandList->ResourceBarrier(1, get_rvalue_ptr(CD3DX12_RESOURCE_BARRIER::Transition(m_geoBuffer[0].Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET)));
	m_commandList->ResourceBarrier(1, get_rvalue_ptr(CD3DX12_RESOURCE_BARRIER::Transition(m_geoBuffer[1].Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET)));
	m_commandList->ResourceBarrier(1, get_rvalue_ptr(CD3DX12_RESOURCE_BARRIER::Transition(m_geoBuffer[2].Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET)));
		
	
	//Skybox
	m_commandList->SetPipelineState(m_skyboxPass.Get());
	m_commandList->RSSetViewports(1, &m_viewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);
	m_commandList->OMSetRenderTargets(1, get_rvalue_ptr(m_rtvHeap->hCPU(m_frameIndex)), FALSE, get_rvalue_ptr(m_dsvHeap->hCPU(0)));
	m_commandList->SetGraphicsRootSignature(m_skyboxSignature.Get());
	m_commandList->SetGraphicsRootConstantBufferView(0, m_currFrameResource->SkyboxConstantBuffer->GetResource()->GetGPUVirtualAddress());
	m_commandList->SetGraphicsRootDescriptorTable(1, m_srvHeap->hGPU(5));
	DrawSkybox();
	m_commandList->ResourceBarrier(1, get_rvalue_ptr(CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT)));
	m_commandList->Close();

	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(array_count(ppCommandLists), ppCommandLists);
	
}

void D3D12HelloTriangle::DrawLights() {

	std::vector<D3D12_VERTEX_BUFFER_VIEW> vertexBufferView;
	m_screenQuad->GetVertexBufferView(vertexBufferView);
	m_commandList->IASetVertexBuffers(0, vertexBufferView.size(), vertexBufferView.data());
	m_commandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_commandList->DrawInstanced(6, 1, 0, 0);
}

void D3D12HelloTriangle::DrawSkybox() {

	std::vector<D3D12_VERTEX_BUFFER_VIEW> vertexBufferView;
	D3D12_INDEX_BUFFER_VIEW indexBufferView = m_boxMesh->GetIndexBufferView();
	m_boxMesh->GetVertexBufferView(vertexBufferView);
	m_commandList->IASetVertexBuffers(0, vertexBufferView.size(), vertexBufferView.data());
	m_commandList->IASetIndexBuffer(&indexBufferView);
	m_commandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_commandList->DrawIndexedInstanced(m_boxMesh->IndexCount(), 1, 0, 0, 0);
}

void D3D12HelloTriangle::DrawRenderItems() {
	
	auto objectCB = m_currFrameResource->ObjConstantBuffer->GetResource();
	auto indexView = m_staticOpaqueMesh->GetIndexBufferView();
	std::vector<D3D12_VERTEX_BUFFER_VIEW> vertexBufferView;
	m_staticOpaqueMesh->GetVertexBufferView(vertexBufferView);
	m_commandList->IASetVertexBuffers(0, vertexBufferView.size(), vertexBufferView.data());
	m_commandList->IASetIndexBuffer(&indexView);
	m_commandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	for (size_t i = 0; i < m_renderItems.size(); ++i)
	{
		auto ri = m_renderItems[i].get();
		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->objCBIndex * GetConstantSize(sizeof(ObjConstants));
		m_commandList->SetGraphicsRootConstantBufferView(0, objCBAddress);
		m_commandList->DrawIndexedInstanced(ri->meshData.Indices32.size(), 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}

void D3D12HelloTriangle::UpdateMaterialBuffer() {

	auto currMaterialBuffer = m_currFrameResource->MaterialDataResource.get();
	
	for (auto& e : m_materials)
	{
		Material* mat = e.second.get();
		if (mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialData matData;
			matData.DiffuseAlbedo = mat->DiffuseAlbedo;
			matData.Metallic = mat->Metallic;
			matData.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matData.MatTransform, XMMatrixTranspose(matTransform));
			matData.DiffuseMapIndex = mat->DiffuseSrvHeapIndex;
			std::vector<vbyte> data(sizeof(matData));
			*reinterpret_cast<MaterialData*>(data.data()) = matData;
			currMaterialBuffer->CopyData(mat->MatCBIndex, data);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}
}

void D3D12HelloTriangle::UpdateObjectConstant() {

	auto currObjectCB = m_currFrameResource->ObjConstantBuffer.get();
	for (auto& e : m_renderItems)
	{
		if (e->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

			ObjConstants objConstants;
			XMStoreFloat4x4(&objConstants.model, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));
			objConstants.MaterialIndex = e->Mat->MatCBIndex;
			std::vector<vbyte> data(sizeof(ObjConstants));
			*reinterpret_cast<ObjConstants*>(data.data()) = objConstants;
			currObjectCB->CopyData(e->objCBIndex, data);

			e->NumFramesDirty--;
		}
	}
}
void D3D12HelloTriangle::PrepareDeferResource(){

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Alignment = 0;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.MipLevels = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.Width = m_width;
	resourceDesc.Height = m_height;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	resourceDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	CD3DX12_HEAP_PROPERTIES heapProperty(D3D12_HEAP_TYPE_DEFAULT);
	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = resourceDesc.Format;
	clearValue.Color[0] = 0.0f;
	clearValue.Color[1] = 0.0f;
	clearValue.Color[2] = 0.0f;
	clearValue.Color[3] = 0.0f;
	//Create resource
	for (uint32_t i = 0; i < 3; ++i) {
		ThrowIfFailed(device->DxDevice()->CreateCommittedResource(&heapProperty, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, &clearValue, IID_PPV_ARGS(&m_geoBuffer[i])));
	}

	//Create render target view
	uint32_t sizeRTV = device->DxDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	for (uint32_t n = 0; n < 3; ++n) {
		m_rtvHeap->CreateRTV(m_geoBuffer[n].Get(), nullptr, 2 + n);
	}

	//Create shader resource view
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Texture2D.MipLevels = resourceDesc.MipLevels;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Format = resourceDesc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	uint32_t sizeSRV = device->DxDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	for (int i = 0; i < 3; i++) {
		m_srvHeap->CreateSRV(m_geoBuffer[i].Get(), &srvDesc, i);
	}
}

void D3D12HelloTriangle::UpdatePassConstant()
{
	XMMATRIX view = m_camera.GetView();
	XMMATRIX proj = m_camera.GetProj();
	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(get_rvalue_ptr(XMMatrixDeterminant(view)), view);
	XMMATRIX invProj = XMMatrixInverse(get_rvalue_ptr(XMMatrixDeterminant(proj)), proj);
	XMMATRIX invViewProj = XMMatrixInverse(get_rvalue_ptr(XMMatrixDeterminant(viewProj)), viewProj);

	GeometryPassConstants geo;
	XMStoreFloat4x4(&geo.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&geo.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&geo.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&geo.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&geo.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&geo.InvViewProj, XMMatrixTranspose(invViewProj));
	geo.EyePosW = m_camera.GetPosition3f();
	geo.RenderTargetSize = XMFLOAT2((float)m_width, (float)m_height);
	geo.InvRenderTargetSize = XMFLOAT2(1.0f / m_width, 1.0f / m_height);
	geo.NearZ = 1.0f;
	geo.FarZ = 1000.0f;
	geo.TotalTime = m_timer.TotalTime();
	geo.DeltaTime = m_timer.DeltaTime();
	std::vector<vbyte> geodata(GetConstantSize(sizeof(GeometryPassConstants)));
	*reinterpret_cast<GeometryPassConstants*>(geodata.data()) = geo;
	m_currFrameResource->GeometryPassConstantBuffer->CopyData(0, geodata);

	LightPassConstants light;
	light.EyePosW = { m_camera.GetPosition3f().x,m_camera.GetPosition3f().y,m_camera.GetPosition3f().z,1.0f };
	light.DirLights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	light.DirLights[0].Strength = { 2.f, 2.f, 2.f };

	light.DirLights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	light.DirLights[1].Strength = { 0.4f, 0.4f, 0.4f };

	light.DirLights[2].Direction = { 0.0f, -0.707f, -0.707f };
	light.DirLights[2].Strength = { 0.3f, 0.2f, 0.0f };

	light.DirLights[3].Direction = { 0.0f, -0.707f, -0.707f };
	light.DirLights[3].Strength = { 0.0f, 0.0f, 0.0f };
	
	light.PointLights[0].Position = { 0.f, 4.f, -4.f };
	light.PointLights[0].Strength = { 0.f, 0.7f, 0.8f };
	
	std::vector<vbyte> lightdata(sizeof(LightPassConstants));
	*reinterpret_cast<LightPassConstants*>(lightdata.data()) = light;
	m_currFrameResource->LightPassConstantBuffer->CopyData(0, lightdata);
	
	TransformConstants skyboxTransform;
	skyboxTransform.EyePosW = { m_camera.GetPosition3f().x,m_camera.GetPosition3f().y,m_camera.GetPosition3f().z, 1.0f};
	XMStoreFloat4x4(&skyboxTransform.ViewProj, XMMatrixTranspose(viewProj));
	std::vector<vbyte> skyboxData(sizeof(TransformConstants));
	*reinterpret_cast<TransformConstants*>(skyboxData.data()) = skyboxTransform;
	m_currFrameResource->SkyboxConstantBuffer->CopyData(0, skyboxData);

}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> D3D12HelloTriangle::GetStaticSamplers()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy
	std::array<CD3DX12_STATIC_SAMPLER_DESC, 6> result;

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp };
}

void D3D12HelloTriangle::WaitForPreviousFrame() {

	const uint64_t fence = m_fenceValue;
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fence));
	m_fenceValue++;

	// Wait until the previous frame is finished.
	if (m_fence->GetCompletedValue() < fence) {
		ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
		WaitForSingleObject(m_fenceEvent, INFINITE);
	}
}

void D3D12HelloTriangle::OnKeyboardInput()
{
	const float dt = m_timer.DeltaTime();

	if (GetAsyncKeyState('W') & 0x8000)
		m_camera.Walk(10.0f * dt);

	if (GetAsyncKeyState('S') & 0x8000)
		m_camera.Walk(-10.0f * dt);

	if (GetAsyncKeyState('A') & 0x8000)
		m_camera.Strafe(-10.0f * dt);

	if (GetAsyncKeyState('D') & 0x8000)
		m_camera.Strafe(10.0f * dt);

	m_camera.UpdateViewMatrix();
}

void D3D12HelloTriangle::Precompute() {

	Camera faceCamera;
	XMMATRIX viewProj[6];
	faceCamera.SetLens(0.5f * MathHelper::Pi, 1.0f, 0.1f, 10.0f);

	faceCamera.LookAt(XMFLOAT3(0., 0., 0.), XMFLOAT3(1., 0., 0.), XMFLOAT3(0., 1., 0.));
	faceCamera.UpdateViewMatrix();
	viewProj[0] = XMMatrixMultiply(faceCamera.GetView(), faceCamera.GetProj());
	faceCamera.LookAt(XMFLOAT3(0., 0., 0.), XMFLOAT3(-1., 0., 0.), XMFLOAT3(0., 1., 0.));
	faceCamera.UpdateViewMatrix();
	viewProj[1] = XMMatrixMultiply(faceCamera.GetView(), faceCamera.GetProj());
	faceCamera.LookAt(XMFLOAT3(0., 0., 0.), XMFLOAT3(0., 1., 0.), XMFLOAT3(0., 0., -1.));
	faceCamera.UpdateViewMatrix();
	viewProj[2] = XMMatrixMultiply(faceCamera.GetView(), faceCamera.GetProj());
	faceCamera.LookAt(XMFLOAT3(0., 0., 0.), XMFLOAT3(0., -1., 0.), XMFLOAT3(0., 0., 1.));
	faceCamera.UpdateViewMatrix();
	viewProj[3] = XMMatrixMultiply(faceCamera.GetView(), faceCamera.GetProj());
	faceCamera.LookAt(XMFLOAT3(0., 0., 0.), XMFLOAT3(0., 0., 1.), XMFLOAT3(0., 1., 0.));
	faceCamera.UpdateViewMatrix();
	viewProj[4] = XMMatrixMultiply(faceCamera.GetView(), faceCamera.GetProj());
	faceCamera.LookAt(XMFLOAT3(0., 0., 0.), XMFLOAT3(0., 0., -1.), XMFLOAT3(0., 1., 0.));
	faceCamera.UpdateViewMatrix();
	viewProj[5] = XMMatrixMultiply(faceCamera.GetView(), faceCamera.GetProj());
	
	//Convolute irradiance
	{
		m_prefilterMap = std::make_unique<CubeRenderTarget>(device->DxDevice(), 512, 512, DXGI_FORMAT_R16G16B16A16_FLOAT);
		CD3DX12_CPU_DESCRIPTOR_HANDLE cubeRtvHandles[6];
		for (int i = 0; i < 6; ++i) {
			cubeRtvHandles[i] = m_rtvHeap->hCPU(5 + i);
		}
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCPU(m_srvHeap->hCPU(6));
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGPU(m_srvHeap->hGPU(6));
		m_prefilterMap->BuildDescriptors(hCPU, hGPU, cubeRtvHandles); //Set the output srv and intermediate rtv
		//Filter the skybox cubemap
		m_commandList->Reset(m_commandAllocator.Get(), m_prefilterPass.Get());
		m_commandList->RSSetViewports(1, get_rvalue_ptr(m_prefilterMap->Viewport()));
		m_commandList->RSSetScissorRects(1, get_rvalue_ptr(m_prefilterMap->ScissorRect()));
		m_commandList->ResourceBarrier(1, get_rvalue_ptr(CD3DX12_RESOURCE_BARRIER::Transition(m_prefilterMap->Resource(),
			D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET)));

		float clearColor[4]{ 0.f, 0.f, 0.f, 0.f };
		

		std::vector<std::unique_ptr<UploadBuffer>> buffers(6); int aindex = 0;
		for (auto& buffer : buffers) {
			buffer = std::make_unique<UploadBuffer>(device.get(), GetConstantSize(sizeof(XMMATRIX)));
			std::vector<vbyte> data(GetConstantSize(sizeof(XMMATRIX)));
			*reinterpret_cast<XMMATRIX*>(data.data()) = XMMatrixTranspose(viewProj[aindex]); ++aindex;
			buffer->CopyData(0, data);
		}

		for (int i = 0; i < 6; ++i)
		{
			m_commandList->ClearDepthStencilView(m_dsvHeap->hCPU(0), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
			m_commandList->OMSetRenderTargets(1, get_rvalue_ptr(m_prefilterMap->Rtv(i)), true, get_rvalue_ptr(m_dsvHeap->hCPU(0)));

			m_commandList->SetGraphicsRootSignature(m_skyboxSignature.Get());

			m_commandList->SetGraphicsRootConstantBufferView(0, buffers[i]->GetAddress());
			ID3D12DescriptorHeap* srvHeaps[] = { m_srvHeap->GetHeap() };
			m_commandList->SetDescriptorHeaps(1, srvHeaps);
			m_commandList->SetGraphicsRootDescriptorTable(1, m_srvHeap->hGPU(5));
			//Draw face
			std::vector<D3D12_VERTEX_BUFFER_VIEW> vertexBufferView;
			D3D12_INDEX_BUFFER_VIEW indexBufferView = m_boxMesh->GetIndexBufferView();
			m_boxMesh->GetVertexBufferView(vertexBufferView);
			m_commandList->IASetVertexBuffers(0, vertexBufferView.size(), vertexBufferView.data());
			m_commandList->IASetIndexBuffer(&indexBufferView);
			m_commandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			m_commandList->DrawIndexedInstanced(m_boxMesh->IndexCount(), 1, 0, 0, 0);
		}
		m_commandList->ResourceBarrier(1, get_rvalue_ptr(CD3DX12_RESOURCE_BARRIER::Transition(m_prefilterMap->Resource(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ)));
		ThrowIfFailed(m_commandList->Close());
		ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
		m_commandQueue->ExecuteCommandLists(array_count(ppCommandLists), ppCommandLists);
		WaitForPreviousFrame();
	}

	//Prefilter map
	{
		struct PrefilterMapConstant {
			XMMATRIX viewProj;
			float roughness;
			float padd0 = 0.;
			float padd1 = 0.;
			float padd2 = 0.;
		};

		int numMipLevel = 5;
		std::vector<PrefilterMapConstant> constantData(6 * numMipLevel);
		for (int i = 0; i < numMipLevel; i++) {
			for (int j = 0; j < 6; j++) {
				constantData[6 * i + j] = { XMMatrixTranspose(viewProj[j]), 0.2f * i };
			}
		}

		std::vector<std::unique_ptr<UploadBuffer>> specBuffers(6 * numMipLevel); int index = 0;
		for (auto& buffer : specBuffers) {
			buffer = std::make_unique<UploadBuffer>(device.get(), GetConstantSize(sizeof(PrefilterMapConstant)));
			std::vector<vbyte> data(GetConstantSize(sizeof(PrefilterMapConstant)));
			*reinterpret_cast<PrefilterMapConstant*>(data.data()) = constantData[index]; ++index;
			buffer->CopyData(0, data);
		}

		auto tempRTVHeap = std::make_unique<DescriptorHeap>(device.get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 6 * numMipLevel, false);
		m_commandList->Reset(m_commandAllocator.Get(), m_prefilterMapPass.Get());
		m_commandList->SetGraphicsRootSignature(m_skyboxSignature.Get());
		m_commandList->SetDescriptorHeaps(1, get_rvalue_ptr(m_srvHeap->GetHeap()));
		m_commandList->SetGraphicsRootDescriptorTable(1, m_srvHeap->hGPU(5));
		std::vector<D3D12_VERTEX_BUFFER_VIEW> vertexBufferView;
		D3D12_INDEX_BUFFER_VIEW indexBufferView = m_boxMesh->GetIndexBufferView();
		m_boxMesh->GetVertexBufferView(vertexBufferView);
		m_commandList->IASetVertexBuffers(0, vertexBufferView.size(), vertexBufferView.data());
		m_commandList->IASetIndexBuffer(&indexBufferView);
		m_commandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		for (int i = 0; i < numMipLevel; ++i) {
			m_prefilterSpecMap[i] = std::make_unique<CubeRenderTarget>(device->DxDevice(), 512, 512, DXGI_FORMAT_R16G16B16A16_FLOAT);
			CD3DX12_CPU_DESCRIPTOR_HANDLE cubeRtvHandles[6];
			for (int j = 0; j < 6; j++) {
				cubeRtvHandles[j] = tempRTVHeap->hCPU(6 * i + j);
			}
			CD3DX12_CPU_DESCRIPTOR_HANDLE hCPU(m_srvHeap->hCPU(8 + i));
			CD3DX12_GPU_DESCRIPTOR_HANDLE hGPU(m_srvHeap->hGPU(8 + i));
			m_prefilterSpecMap[i]->BuildDescriptors(hCPU, hGPU, cubeRtvHandles);
			m_commandList->RSSetViewports(1, get_rvalue_ptr(m_prefilterSpecMap[i]->Viewport()));
			m_commandList->RSSetScissorRects(1, get_rvalue_ptr(m_prefilterSpecMap[i]->ScissorRect()));
			m_commandList->ResourceBarrier(1, get_rvalue_ptr(CD3DX12_RESOURCE_BARRIER::Transition(m_prefilterSpecMap[i]->Resource(),
				D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET)));

			for (int j = 0; j < 6; j++) {
				m_commandList->SetGraphicsRootConstantBufferView(0, specBuffers[i * 6 + j]->GetResource()->GetGPUVirtualAddress());
				m_commandList->OMSetRenderTargets(1, get_rvalue_ptr(m_prefilterSpecMap[i]->Rtv(j)), true, get_rvalue_ptr(m_dsvHeap->hCPU(0)));
				m_commandList->DrawIndexedInstanced(m_boxMesh->IndexCount(), 1, 0, 0, 0);
			}
			m_commandList->ResourceBarrier(1, get_rvalue_ptr(CD3DX12_RESOURCE_BARRIER::Transition(m_prefilterSpecMap[i]->Resource(),
				D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ)));
		}
		ThrowIfFailed(m_commandList->Close());
		ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
		m_commandQueue->ExecuteCommandLists(array_count(ppCommandLists), ppCommandLists);
		WaitForPreviousFrame();
	}
}