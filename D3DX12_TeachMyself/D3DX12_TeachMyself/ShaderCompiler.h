#pragma once
#include "stdafx.h"
#include <string>
#include "RHITypes.h"
#include "HRException.h"
#include <filesystem>

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
    static ComPtr<ID3DBlob> Compile(
        const std::wstring& path,
        const std::string& entryPoint,
        const std::string& target)
    {
        UINT flags = 0;
#ifdef _DEBUG
        flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
        ComPtr<ID3DBlob> shader, error;
        HRESULT hr = D3DCompileFromFile(
            path.c_str(), nullptr, nullptr,
            entryPoint.c_str(), target.c_str(),
            flags, 0, &shader, &error);
        if (error)
            OutputDebugStringA((const char*)error->GetBufferPointer());
        HR_CHECK(hr);
        return shader;
    }

    inline static std::vector<Shader> m_shaders;
};