#pragma once
#include "stdafx.h"
#include <string>
#include "RHITypes.h"
#include "HRException.h"
#include <stdexcept>

class ShaderCompiler
{
public:
    static ShaderBytecode CompileFromFile(
        const std::wstring& path,
        const std::string& entryPoint,
        const std::string& target)
    {
        UINT compileFlag = 0;

#ifdef _DEBUG
        compileFlag = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

        Microsoft::WRL::ComPtr<ID3DBlob> shader;
        Microsoft::WRL::ComPtr<ID3DBlob> error;

        HRESULT hr = D3DCompileFromFile(
            path.c_str(),
            nullptr,
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            entryPoint.c_str(),
            target.c_str(),
            compileFlag,
            0,
            &shader,
            &error
        );

        wchar_t msg[1024];
        swprintf_s(msg, L"hr = 0x%08X, shader = %p\n", hr, shader.Get());
        OutputDebugStringW(msg);

        if (FAILED(hr))
        {
            if (error)
                OutputDebugStringA((char*)error->GetBufferPointer());

            throw std::runtime_error("D3DCompileFromFile failed");
        }

        if (!shader)
        {
            throw std::runtime_error("D3DCompileFromFile succeeded but shader blob is null");
        }

        std::shared_ptr<ID3DBlob> owned(
            shader.Detach(),
            [](ID3DBlob* p) { if (p) p->Release(); }
        );

        return ShaderBytecode{ owned->GetBufferPointer(), owned->GetBufferSize(), owned };
    }
};