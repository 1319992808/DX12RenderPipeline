#include<Resource/Pass.h>

Pass::Pass(const std::wstring& shaderPath) : m_shaderPath(shaderPath){
	CompileSahder();
}

Pass::Pass(std::wstring&& shaderPath) : m_shaderPath(shaderPath) {
	CompileSahder();
}

ID3DBlob*  Pass::GetVertexShader() {
	return m_vertexShader.Get();
}

ID3DBlob* Pass::GetPixelShader() {
	return m_pixelShader.Get();
}
void Pass::CompileSahder() {

#if defined(_DEBUG)
	// Enable better shader debugging with the graphics debugging tools.
	uint32_t compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	uint32_t compileFlags = 0;
#endif
	ComPtr<ID3DBlob> errors;
	HRESULT hr = D3DCompileFromFile(m_shaderPath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_1", compileFlags, 0, &m_vertexShader, &errors);
	if (errors != nullptr)
	{
		OutputDebugStringA((char*)errors->GetBufferPointer());
		OutputDebugStringW(m_shaderPath.c_str());
		ThrowIfFailed(hr);
	}
	hr = D3DCompileFromFile(m_shaderPath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_1", compileFlags, 0, &m_pixelShader, &errors);
	if (errors != nullptr)
	{
		OutputDebugStringA((char*)errors->GetBufferPointer());
		OutputDebugStringW(m_shaderPath.c_str());
		ThrowIfFailed(hr);
	}
}

void Pass::Set() {
	//TO Be Implemented
}

void Pass::DrawMesh() {
	//To Be Implemented
}