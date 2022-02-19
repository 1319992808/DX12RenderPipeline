#pragma once
#include<DirectXMath.h>
#include <Utility/ReflactableStruct.h>
#include<Resource/MathHelper.h>
#include<Resource/UploadBuffer.h>
#include<DXSampleHelper.h>

struct CommonVertex : public rtti::Struct {
	rtti::Var<DirectX::XMFLOAT3> position = "POSITION";
	rtti::Var<DirectX::XMFLOAT3> normal = "NORMAL";
	rtti::Var<DirectX::XMFLOAT2> texcoord = "TEXCOORD";
};

struct QuadVertex : public rtti::Struct {
	rtti::Var<DirectX::XMFLOAT2> position = "POSITION";
	rtti::Var<DirectX::XMFLOAT2> texcoord = "TEXCOORD";
};

struct MiniVertex : public rtti::Struct {
	rtti::Var<DirectX::XMFLOAT4> position = "POSITION";
};

struct DirLight {
	DirectX::XMFLOAT3 Strength;
	float padding0;
	DirectX::XMFLOAT3 Direction;
	float padding1;
};
struct PointLight {
	DirectX::XMFLOAT3 Strength;
	float padding0;
	DirectX::XMFLOAT3 Position;
	float padding1;
};
struct GeometryPassConstants {

	DirectX::XMFLOAT4X4 View = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvView = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
	float cbPerObjectPad1 = 0.0f;
	DirectX::XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
	DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };
	float NearZ = 0.0f;
	float FarZ = 0.0f;
	float TotalTime = 0.0f;
	float DeltaTime = 0.0f;
};
struct LightPassConstants {

	DirectX::XMFLOAT4 EyePosW = { 0.0f, 0.0f, 0.0f, 0.0f };

	DirLight DirLights[4];
	PointLight PointLights[4];

};

struct ObjConstants {

	DirectX::XMFLOAT4X4 model = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
	UINT     MaterialIndex;
	UINT     ObjPad0;
	UINT     ObjPad1;
	UINT     ObjPad2;
};

struct TransformConstants {

	DirectX::XMFLOAT4 EyePosW = { 0.0f, 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
};

struct MaterialData {

	DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	float Metallic = 0.0f;
	float Roughness = 64.0f;

	// Used in texture mapping.
	DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();

	UINT DiffuseMapIndex = 0;
	UINT MaterialPad0;
	UINT MaterialPad1;
	UINT MaterialPad2;
	UINT MaterialPad3;
	UINT MaterialPad4;
};

class FrameResource{
    
public:

        FrameResource(Device* device, uint32_t passCount, uint32_t objectCount, uint32_t materialCount);
        FrameResource(const FrameResource& rhs) = delete;
        FrameResource& operator=(const FrameResource& rhs) = delete;
        ~FrameResource();

        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;
		
		std::unique_ptr<UploadBuffer> GeometryPassConstantBuffer = nullptr;
		std::unique_ptr<UploadBuffer> LightPassConstantBuffer = nullptr;
		std::unique_ptr<UploadBuffer> ObjConstantBuffer = nullptr;
		std::unique_ptr<UploadBuffer> MaterialDataResource = nullptr;
		std::unique_ptr<UploadBuffer> SkyboxConstantBuffer = nullptr;

        UINT64 Fence = 0;
};