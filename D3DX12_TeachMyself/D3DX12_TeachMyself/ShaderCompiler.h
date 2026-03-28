#pragma once
#include "stdafx.h"
#include <string>
#include "RHITypes.h"
#include "HRException.h"

class ShaderCompiler
{
public:
	static ShaderBytecode CompileFromFile(const std::wstring& path, const std::string& entryPoint, const std::string& target)
	{
		UINT compileFlag = 0;

#ifdef _DEBUG
		compileFlag = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

		Microsoft::WRL::ComPtr<ID3DBlob> shader;
		Microsoft::WRL::ComPtr<ID3DBlob> error;
		HRESULT hr = D3DCompileFromFile(path.c_str(), nullptr, nullptr, entryPoint.c_str(), target.c_str(), compileFlag, 0, &shader, &error);
		if (error)
			OutputDebugStringA(static_cast<const char*>(error->GetBufferPointer()));
		HR_CHECK(hr);


		std::shared_ptr<ID3DBlob> owned(shader.Detach(), [](ID3DBlob* p) {if (p) p->Release(); });

		return ShaderBytecode{ owned->GetBufferPointer(), owned->GetBufferSize(), owned };
	}
};