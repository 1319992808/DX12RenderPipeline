#pragma once
#include <Resource/Resource.h>
#include <Resource/BufferView.h>

inline uint64 GetConstantSize(size_t size) {
	return (size + 255) & ~255;
}

class Buffer : public Resource{
public:
	Buffer(Device* device);
	virtual D3D12_GPU_VIRTUAL_ADDRESS GetAddress() const = 0;
	virtual uint64 GetByteSize() const = 0;
	virtual ~Buffer();
	Buffer(Buffer&&) = default;
};
