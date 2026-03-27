#pragma once
#include <exception>
#include <string>
#include <source_location>
#include <comdef.h>
#include <d3d12.h>

class HRException : public std::exception
{
public:
	HRException(
		HRESULT hr,
		ID3D12Device* device = nullptr,
		std::source_location loc = std::source_location::current())
		:
		m_hr(hr),
		m_location(loc)
	{
		_com_error err(hr);
		std::wstring wideMsg = err.ErrorMessage();
		std::string hrMsg(wideMsg.begin(), wideMsg.end());

		std::string removedReason;
		if (hr == DXGI_ERROR_DEVICE_REMOVED && device)
		{
			HRESULT reason = device->GetDeviceRemovedReason();
			_com_error reasonErr(reason);
			std::wstring wideReason = reasonErr.ErrorMessage();
			removedReason = " | DeviceRemovedReason: " + std::string(wideReason.begin(), wideReason.end());
		}

		m_message =
			std::string(loc.function_name())
			+ " (" + loc.file_name() + ":" + std::to_string(loc.line()) + ")"
			+ "\n  HRESULT 0x" + ToHexString(hr)
			+ " — " + hrMsg
			+ removedReason;
	}

	const char* what() const noexcept override { return m_message.c_str(); }
	HRESULT     GetHR() const noexcept { return m_hr; }

private:
	static std::string ToHexString(HRESULT hr)
	{
		char buf[16];
		snprintf(buf, sizeof(buf), "%08X", static_cast<unsigned int>(hr));
		return buf;
	}

	HRESULT m_hr;
	std::source_location m_location;
	std::string m_message;
};

#define HR_CHECK(hr)\
	do {\
		HRESULT _hr_ = (hr);\
		if (FAILED(_hr_))\
			throw HRException(_hr_);\
	} while(false)

#define HR_CHECK_DEVICE(hr, device)\
    do {\
        HRESULT _hr_ = (hr);\
        if (FAILED(_hr_))\
            throw HRException(_hr_, (device));\
    } while (false)