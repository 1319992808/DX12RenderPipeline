#include<Resource/Texture.h>

Texture::Texture(Device* device, std::string name) : Resource(device), Name(name) {

}

void Texture::LoadTexture(ID3D12GraphicsCommandList* commandList, std::wstring filePath) {
	
	this->Filename = filePath;
	HRESULT hr = DirectX::CreateDDSTextureFromFile12(device->DxDevice(),
		commandList, filePath.c_str(),
		resource, UploadHeap);

	if (FAILED(hr)) {
		assert(0);
	}
}

D3D12_SHADER_RESOURCE_VIEW_DESC Texture::GetSRVDesc() {

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = resource->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = resource->GetDesc().MipLevels;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	return srvDesc;

}

D3D12_SHADER_RESOURCE_VIEW_DESC Texture::GetCubeMapSRVDesc() {

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = resource->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = resource->GetDesc().MipLevels;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	return srvDesc;

}
//void Texture::LoadHDR(ID3D12GraphicsCommandList* commandList, std::string filePath) {
//	
//	this->Filename.FilenameA = filePath;
//	int width, height, nrComponents;
//	unsigned int hdrTexture;
//	if (data)
//	{
//		
//		ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(device->DxDevice(),
//			commandList, filePath.c_str(),
//			resource, UploadHeap));
//		stbi_image_free(data);
//	}
//	else
//	{
//		OutputDebugStringA("Failed to load HDR image.\n");
//	}
//}
