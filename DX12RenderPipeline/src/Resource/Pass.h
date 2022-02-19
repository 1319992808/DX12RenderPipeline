#pragma once
#include <stdafx.h>
#include <DXRuntime/Device.h>
#include <Resource/Mesh.h>
#include<DXSampleHelper.h>
using Microsoft::WRL::ComPtr;

class Pass {

public:
	Pass(const std::wstring& shaderPath);
	Pass(std::wstring&& shaderPath);
	
	void CompileSahder();
	void Set(); //PSO, root signature, shader, mesh
	void DrawMesh(); //cmdlist, mesh

private:

	std::wstring m_shaderPath;
	ComPtr<ID3DBlob> m_vertexShader = nullptr;
	ComPtr<ID3DBlob> m_pixelShader = nullptr;
	ComPtr<ID3D12PipelineState> m_pipelineState = nullptr;
	ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;

public:
	ID3DBlob* GetVertexShader();
	ID3DBlob* GetPixelShader();
};