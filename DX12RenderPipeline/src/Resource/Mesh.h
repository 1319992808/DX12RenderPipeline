#pragma once
#include <Resource/Resource.h>
#include <Resource/DefaultBuffer.h>
#include <Utility/ReflactableStruct.h>
class Mesh : public Resource {
	
	std::vector<DefaultBuffer> vertexBuffers;
	std::unique_ptr<DefaultBuffer> indexBuffer;

	std::span<rtti::Struct const*> vertexStructs;
	std::vector<D3D12_INPUT_ELEMENT_DESC> layout;
	uint64 vertexCount;
	uint64 indexCount;

public:
	std::span<DefaultBuffer const> VertexBuffers() const { return vertexBuffers; }
	DefaultBuffer* IndexBuffer() const { return indexBuffer.get(); }
	uint64 IndexCount() const { return indexCount; }


	std::span<D3D12_INPUT_ELEMENT_DESC const> Layout() const { return layout; }
	Mesh(
		Device* device,
		std::span<rtti::Struct const*> vbStructs,
		uint64 vertexCount,
		uint64 indexCount = 0);

	void GetVertexBufferView(std::vector<D3D12_VERTEX_BUFFER_VIEW>& result) const;
	D3D12_INDEX_BUFFER_VIEW Mesh::GetIndexBufferView() const;

};