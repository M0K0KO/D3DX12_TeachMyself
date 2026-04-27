#pragma once
#include <d3dcompiler.h>
#include <wrl.h>
#include <string>
#include <vector>
#include <filesystem>
#include <dxcapi.h>
#pragma comment(lib, "dxcompiler.lib")

#include "RHITypes.h"
#include "HRException.h"

using namespace Microsoft::WRL;

struct Shader
{
	std::wstring path;
	std::string entryPoint;
	std::string target;
	std::filesystem::file_time_type lastWriteTime;

	ComPtr<ID3DBlob> bytecode;

	bool dirty = false;
};

struct ShaderHandle { uint32_t id; };

class ShaderCompiler
{
public:
    static ShaderHandle CompileFromFile(
        const std::wstring& path,
        const std::string& entryPoint,
        const std::string& target)
    {
        auto absolutePath = std::filesystem::absolute(path).wstring();

        ComPtr<ID3DBlob> bytecode = Compile(absolutePath, entryPoint, target);

        Shader shader;
        shader.path = absolutePath;
        shader.entryPoint = entryPoint;
        shader.target = target;
        shader.lastWriteTime = std::filesystem::last_write_time(path);
        shader.bytecode = bytecode;

        uint32_t id = (uint32_t)m_shaders.size();
        m_shaders.push_back(std::move(shader));
        return ShaderHandle{ id };
    }

    static void CheckForChanges()
    {
        for (auto& shader : m_shaders)
        {
            auto time = std::filesystem::last_write_time(shader.path);
            if (time != shader.lastWriteTime)
            {
                shader.lastWriteTime = time;
                shader.bytecode = Compile(
                    shader.path, shader.entryPoint, shader.target);
                shader.dirty = true;
            }
        }
    }

    static bool IsDirty(ShaderHandle handle)
    {
        return m_shaders[handle.id].dirty;
    }

    static void ClearDirty(ShaderHandle handle)
    {
        m_shaders[handle.id].dirty = false;
    }

    static void Reserve(size_t count)
    {
        m_shaders.reserve(count);
    }

    static ShaderBytecode GetBytecode(ShaderHandle handle)
    {
        ComPtr<ID3DBlob> blob = m_shaders[handle.id].bytecode;
        ID3DBlob* raw = blob.Get();
        raw->AddRef();
        std::shared_ptr<ID3DBlob> owned(raw, [](ID3DBlob* p) { if (p) p->Release(); });
        return ShaderBytecode{
            raw->GetBufferPointer(),
            raw->GetBufferSize(),
            owned
        };
    }

private:
    inline static ComPtr<IDxcUtils>          s_utils;
    inline static ComPtr<IDxcCompiler3>      s_compiler;
    inline static ComPtr<IDxcIncludeHandler> s_includeHandler;

    static void InitDxc()
    {
        if (s_compiler) return; 

        DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&s_utils));
        DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&s_compiler));
        s_utils->CreateDefaultIncludeHandler(&s_includeHandler);
    }

private:
    static ComPtr<ID3DBlob> Compile(
        const std::wstring& path,
        const std::string& entryPoint,
        const std::string& target)
    {
        InitDxc();

        ComPtr<IDxcBlobEncoding> sourceBlob;
        HR_CHECK(s_utils->LoadFile(path.c_str(), nullptr, &sourceBlob));

        DxcBuffer source = {
            .Ptr = sourceBlob->GetBufferPointer(),
            .Size = sourceBlob->GetBufferSize(),
            .Encoding = DXC_CP_UTF8,
        };

        std::wstring wEntry(entryPoint.begin(), entryPoint.end());
        std::wstring wTarget(target.begin(), target.end());

        std::vector<LPCWSTR> args = {
            L"-E", wEntry.c_str(),
            L"-T", wTarget.c_str(),
            L"-HV", L"2021",
        };

#ifdef _DEBUG
        args.push_back(L"-Zi");
        args.push_back(L"-Qembed_debug");
        args.push_back(L"-Od");
#else
        args.push_back(L"-03");
#endif

        ComPtr<IDxcResult> result;
        HR_CHECK(s_compiler->Compile(&source, args.data(), (UINT32)args.size(), s_includeHandler.Get(), IID_PPV_ARGS(&result)));

        ComPtr<IDxcBlobUtf8> errors;
        result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
        if (errors && errors->GetStringLength() > 0)
            OutputDebugStringA(errors->GetStringPointer());

        HRESULT status;
        result->GetStatus(&status);
        HR_CHECK(status);

        ComPtr<IDxcBlob> shaderBlob;
        HR_CHECK(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr));

        ComPtr<ID3DBlob> ret;
        HR_CHECK(shaderBlob.As(&ret));
        return ret;
    }

    inline static std::vector<Shader> m_shaders;
};