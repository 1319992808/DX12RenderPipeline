#pragma once

#include<Resource/Resource.h>
#include<Resource/DDSTextureLoader.h>
#include<DXSampleHelper.h>
//Later include WIC module

class Texture : public Resource
{

public:
	Texture(Device* device, std::string name);
	virtual ~Texture() = default;
	Texture(Texture&&) = default;
	void LoadTexture(ID3D12GraphicsCommandList* commandList, std::wstring filePath);
	//void LoadHDR(ID3D12GraphicsCommandList* commandList, std::string filePath);
	inline ID3D12Resource* GetResource() const override{ return resource.Get(); }
	D3D12_SHADER_RESOURCE_VIEW_DESC GetSRVDesc();
	D3D12_SHADER_RESOURCE_VIEW_DESC GetCubeMapSRVDesc();
	void SetViewOffset(uint32_t offset) { viewOffset = offset; };
	inline uint32_t GetViewOffset() const { return viewOffset; };
private:
	std::string Name; //For look up

	std::wstring Filename;
	uint32_t viewOffset;
	ComPtr<ID3D12Resource> resource = nullptr;
	ComPtr<ID3D12Resource> UploadHeap = nullptr;
};
