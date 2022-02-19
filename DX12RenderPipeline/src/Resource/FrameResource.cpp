#include<Resource/FrameResource.h>
FrameResource::FrameResource(Device* device, uint32_t passCount, uint32_t objectCount, uint32_t materialCount)
    
{
    ThrowIfFailed(device->DxDevice()->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(CmdListAlloc.GetAddressOf())));

    GeometryPassConstantBuffer = std::make_unique<UploadBuffer>(device, GetConstantSize(sizeof(GeometryPassConstants) * passCount));
    LightPassConstantBuffer = std::make_unique<UploadBuffer>(device, GetConstantSize(sizeof(LightPassConstants) * passCount));
    ObjConstantBuffer = std::make_unique<UploadBuffer>(device, GetConstantSize(sizeof(ObjConstants) * objectCount));
    SkyboxConstantBuffer = std::make_unique<UploadBuffer>(device, GetConstantSize(sizeof(TransformConstants)));
    MaterialDataResource = std::make_unique<UploadBuffer>(device, sizeof(MaterialData) * materialCount);

}

FrameResource::~FrameResource()
{

}