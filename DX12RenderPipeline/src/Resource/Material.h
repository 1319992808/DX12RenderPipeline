#pragma once
#include<string>
#include<DirectXMath.h>
#include<Resource/MathHelper.h>

class Material
{
public:

	// Unique material name for lookup.
	std::string Name;

	int MatCBIndex = -1;
	int DiffuseSrvHeapIndex = -1;
	int NormalSrvHeapIndex = -1;
	int NumFramesDirty = 3; //Hard Code, num frame resources

	// Material constant buffer data used for shading.
	DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	float Metallic = 0.f;
	float Roughness = 4.f;
	DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();
};