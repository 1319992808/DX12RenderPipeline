#pragma once
#include<Resource/GeometryGenerator.h>
#include<Resource/Mesh.h>
#include<Resource/Material.h>
#include<stdafx.h>
#include<Resource/MathHelper.h>
#include<DirectXMath.h>

class RenderItem {
	
public:
    RenderItem() = default;
    RenderItem(const RenderItem & rhs) = delete;

    DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
    int NumFramesDirty = 3; //Hard Code, number of frame resources
    
    uint32_t objCBIndex = -1;

    GeometryGenerator::MeshData meshData;

    Material* Mat = nullptr;

    // DrawIndexedInstanced parameters.
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;
    
};