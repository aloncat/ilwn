//∙MDPN
#include "pch.h"
#include "largemempages.h"

#include <core/array.h>
#include <core/winapi.h>

#include <ntsecapi.h>
#include <string.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Вспомогательные функции
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
static void InitLsaString(LSA_UNICODE_STRING& lsaString, LPWSTR pStr)
{
	lsaString.Buffer = nullptr;
	lsaString.Length = 0;
	lsaString.MaximumLength = 0;

	if (pStr)
	{
		const size_t len = wcslen(pStr);

		lsaString.Buffer = pStr;
		lsaString.Length = static_cast<USHORT>(len * sizeof(WCHAR));
		lsaString.MaximumLength = static_cast<USHORT>((len + 1) * sizeof(WCHAR));
	}
}

//----------------------------------------------------------------------------------------------------------------------
static NTSTATUS OpenPolicy(LPWSTR pServerName, DWORD desiredAccess, LSA_HANDLE& policyHandle)
{
	LSA_UNICODE_STRING serverString;
	InitLsaString(serverString, pServerName);
	LSA_UNICODE_STRING* pServer = pServerName ? &serverString : nullptr;

	LSA_OBJECT_ATTRIBUTES attributes;
	memset(&attributes, 0, sizeof(attributes));
	return ::LsaOpenPolicy(pServer,	&attributes, desiredAccess, &policyHandle);
}

//----------------------------------------------------------------------------------------------------------------------
static NTSTATUS SetPrivilegeOnAccount(LSA_HANDLE policyHandle, PSID accountSid, LPWSTR pPrivilegeName, bool enable)
{
	LSA_UNICODE_STRING privilegeString;
	InitLsaString(privilegeString, pPrivilegeName);

	if (enable)
	{
		return LsaAddAccountRights(policyHandle, accountSid, &privilegeString, 1);
	} else
	{
		return LsaRemoveAccountRights(policyHandle, accountSid, FALSE, &privilegeString, 1);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   LargeMemPages
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool LargeMemPages::s_IsEnabled = false;

//----------------------------------------------------------------------------------------------------------------------
void LargeMemPages::Init()
{
	static bool isInitialized = ([]() {
		if (GetLargePageSize())
		{
			AdjustPrivileges();
			s_IsEnabled = CheckAlloc();
		}
	}(), true);
}

//----------------------------------------------------------------------------------------------------------------------
bool LargeMemPages::IsEnabled()
{
	Init();
	return s_IsEnabled;
}

//----------------------------------------------------------------------------------------------------------------------
size_t LargeMemPages::GetLargePageSize()
{
	return ::GetLargePageMinimum();
}

//----------------------------------------------------------------------------------------------------------------------
void LargeMemPages::AdjustPrivileges()
{
	HANDLE queryToken = nullptr;
	if (::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &queryToken))
	{
		DWORD bufferSize = 0;
		if ((::GetTokenInformation(queryToken, TokenUser, nullptr, 0, &bufferSize) ||
			::GetLastError() == ERROR_INSUFFICIENT_BUFFER) && bufferSize)
		{
			util::DynamicArray<char> buffer(bufferSize);
			auto pTokenUser = reinterpret_cast<PTOKEN_USER>(&buffer[0]);
			if (::GetTokenInformation(queryToken, TokenUser, pTokenUser, bufferSize, &bufferSize))
			{
				LSA_HANDLE policyHandle = nullptr;
				if (OpenPolicy(nullptr, POLICY_CREATE_ACCOUNT | POLICY_LOOKUP_NAMES, policyHandle))
				{
					wchar_t seLockMemory[] = SE_LOCK_MEMORY_NAME;
					SetPrivilegeOnAccount(policyHandle, pTokenUser->User.Sid, seLockMemory, true);

					HANDLE processToken = nullptr;
					if (::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, &processToken))
					{
						TOKEN_PRIVILEGES tp;
						tp.PrivilegeCount = 1;
						tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

						if (::LookupPrivilegeValue(nullptr, SE_LOCK_MEMORY_NAME, &tp.Privileges[0].Luid))
							::AdjustTokenPrivileges(processToken, FALSE, &tp, 0, nullptr, nullptr);

						::CloseHandle(processToken);
					}
				}
			}
		}
		::CloseHandle(queryToken);
	}
}

//----------------------------------------------------------------------------------------------------------------------
bool LargeMemPages::CheckAlloc()
{
	const size_t pageSize = GetLargePageSize();
	if (void* p = ::VirtualAlloc(nullptr, pageSize, MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES, PAGE_READWRITE))
	{
		::VirtualFree(p, 0, MEM_RELEASE);
		return true;
	}
	return false;
}
