#include <Resource/Mesh.h>
Mesh::Mesh(
	Device* device,
	std::span<rtti::Struct const*> vbStructs,
	uint64 vertexCount,
	uint64 indexCount)
	: Resource(device),
	  vertexCount(vertexCount),
	  indexCount(indexCount),
	  vertexStructs(vbStructs) {
	vertexBuffers.reserve(vertexStructs.size());
	if (indexCount>0) {
		indexBuffer = std::make_unique<DefaultBuffer>(device, indexCount * sizeof(uint));
	}
	uint slotCount = 0;
	for (auto&& i : vertexStructs) {
		vertexBuffers.emplace_back(device, i->structSize * vertexCount);
		i->GetMeshLayout(slotCount, layout);
		++slotCount;
	}
}

void Mesh::GetVertexBufferView(std::vector<D3D12_VERTEX_BUFFER_VIEW>& result) const {
	result.clear();
	result.resize(vertexBuffers.size());
	for (size_t i = 0; i < vertexBuffers.size(); ++i) {
		auto& r = result[i];
		auto& v = vertexBuffers[i];
		r.BufferLocation = v.GetAddress();
		r.SizeInBytes = v.GetByteSize();
		r.StrideInBytes = r.SizeInBytes / vertexCount;
	}
}

D3D12_INDEX_BUFFER_VIEW Mesh::GetIndexBufferView() const {
	D3D12_INDEX_BUFFER_VIEW result;
	result.BufferLocation = indexBuffer->GetAddress();
	result.Format = DXGI_FORMAT_R32_UINT;
	result.SizeInBytes = indexCount * sizeof(uint);
	return result;
}