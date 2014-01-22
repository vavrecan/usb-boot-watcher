#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "settings.h"
#include "prepare.h"

#define myheapalloc(x) (HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, x))
#define myheapfree(x)  (HeapFree(GetProcessHeap(), 0, x))

typedef BOOL (WINAPI *SetSecurityDescriptorControlFnPtr)(
	IN PSECURITY_DESCRIPTOR pSecurityDescriptor,
	IN SECURITY_DESCRIPTOR_CONTROL ControlBitsOfInterest,
	IN SECURITY_DESCRIPTOR_CONTROL ControlBitsToSet);

BOOL AddAccessRights(TCHAR *lpszFileName, TCHAR *lpszAccountName, 
					 DWORD dwAccessMask) 
{
	// SID variables.
	SID_NAME_USE   snuType;
	TCHAR *        szDomain       = NULL;
	DWORD          cbDomain       = 0;
	LPVOID         pUserSID       = NULL;
	DWORD          cbUserSID      = 0;

	// File SD variables.
	PSECURITY_DESCRIPTOR pFileSD  = NULL;
	DWORD          cbFileSD       = 0;

	// New SD variables.
	SECURITY_DESCRIPTOR  newSD;

	// ACL variables.
	PACL           pACL           = NULL;
	BOOL           fDaclPresent;
	BOOL           fDaclDefaulted;
	ACL_SIZE_INFORMATION AclInfo;

	// New ACL variables.
	PACL           pNewACL        = NULL;
	DWORD          cbNewACL       = 0;

	// Temporary ACE.
	LPVOID         pTempAce       = NULL;
	UINT           CurrentAceIndex = 0;

	UINT           newAceIndex = 0;

	// Assume function will fail.
	BOOL           fResult        = FALSE;
	BOOL           fAPISuccess;

	SECURITY_INFORMATION secInfo = DACL_SECURITY_INFORMATION;

	// New APIs available only in Windows 2000 and above for setting 
	// SD control
	SetSecurityDescriptorControlFnPtr _SetSecurityDescriptorControl = NULL;

	__try {

	 // 
	 // STEP 1: Get SID of the account name specified.
	 // 
	 fAPISuccess = LookupAccountName(NULL, lpszAccountName,
		 pUserSID, &cbUserSID, szDomain, &cbDomain, &snuType);

	 // API should have failed with insufficient buffer.
	 if (fAPISuccess)
		 __leave;
	 else if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
		 printf("LookupAccountName() failed. Error %d\n", 
			 GetLastError());
		 __leave;
	 }

	 pUserSID = myheapalloc(cbUserSID);
	 if (!pUserSID) {
		 printf("HeapAlloc() failed. Error %d\n", GetLastError());
		 __leave;
	 }

	 szDomain = (TCHAR *) myheapalloc(cbDomain * sizeof(TCHAR));
	 if (!szDomain) {
		 printf("HeapAlloc() failed. Error %d\n", GetLastError());
		 __leave;
	 }

	 fAPISuccess = LookupAccountName(NULL, lpszAccountName,
		 pUserSID, &cbUserSID, szDomain, &cbDomain, &snuType);
	 if (!fAPISuccess) {
		 printf("LookupAccountName() failed. Error %d\n", 
			 GetLastError());
		 __leave;
	 }

	 // 
	 // STEP 2: Get security descriptor (SD) of the file specified.
	 // 
	 fAPISuccess = GetFileSecurity(lpszFileName, 
		 secInfo, pFileSD, 0, &cbFileSD);

	 // API should have failed with insufficient buffer.
	 if (fAPISuccess)
		 __leave;
	 else if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
		 printf("GetFileSecurity() failed. Error %d\n", 
			 GetLastError());
		 __leave;
	 }

	 pFileSD = myheapalloc(cbFileSD);
	 if (!pFileSD) {
		 printf("HeapAlloc() failed. Error %d\n", GetLastError());
		 __leave;
	 }

	 fAPISuccess = GetFileSecurity(lpszFileName, 
		 secInfo, pFileSD, cbFileSD, &cbFileSD);
	 if (!fAPISuccess) {
		 printf("GetFileSecurity() failed. Error %d\n", 
			 GetLastError());
		 __leave;
	 }

	 // 
	 // STEP 3: Initialize new SD.
	 // 
	 if (!InitializeSecurityDescriptor(&newSD, 
		 SECURITY_DESCRIPTOR_REVISION)) {
			 printf("InitializeSecurityDescriptor() failed. Error %d\n", GetLastError());
			 __leave;
	 }

	 // 
	 // STEP 4: Get DACL from the old SD.
	 // 
	 if (!GetSecurityDescriptorDacl(pFileSD, &fDaclPresent, &pACL,
		 &fDaclDefaulted)) {
			 printf("GetSecurityDescriptorDacl() failed. Error %d\n",
				 GetLastError());
			 __leave;
	 }

	 // 
	 // STEP 5: Get size information for DACL.
	 // 
	 AclInfo.AceCount = 0; // Assume NULL DACL.
	 AclInfo.AclBytesFree = 0;
	 AclInfo.AclBytesInUse = sizeof(ACL);

	 if (pACL == NULL)
		 fDaclPresent = FALSE;

	 // If not NULL DACL, gather size information from DACL.
	 if (fDaclPresent) {    

		 if (!GetAclInformation(pACL, &AclInfo, 
			 sizeof(ACL_SIZE_INFORMATION), AclSizeInformation)) {
				 printf("GetAclInformation() failed. Error %d\n",
					 GetLastError());
				 __leave;
		 }
	 }

	 // 
	 // STEP 6: Compute size needed for the new ACL.
	 // 
	 cbNewACL = AclInfo.AclBytesInUse + sizeof(ACCESS_ALLOWED_ACE) 
		 + GetLengthSid(pUserSID) - sizeof(DWORD);

	 // 
	 // STEP 7: Allocate memory for new ACL.
	 // 
	 pNewACL = (PACL) myheapalloc(cbNewACL);
	 if (!pNewACL) {
		 printf("HeapAlloc() failed. Error %d\n", GetLastError());
		 __leave;
	 }

	 // 
	 // STEP 8: Initialize the new ACL.
	 // 
	 if (!InitializeAcl(pNewACL, cbNewACL, ACL_REVISION2)) {
		 printf("InitializeAcl() failed. Error %d\n", 
			 GetLastError());
		 __leave;
	 }

	 // 
	 // STEP 9 If DACL is present, copy all the ACEs from the old DACL
	 // to the new DACL.
	 // 
	 // The following code assumes that the old DACL is
	 // already in Windows 2000 preferred order.  To conform 
	 // to the new Windows 2000 preferred order, first we will 
	 // copy all non-inherited ACEs from the old DACL to the 
	 // new DACL, irrespective of the ACE type.
	 // 

	 newAceIndex = 0;

	 if (fDaclPresent && AclInfo.AceCount) {

		 for (CurrentAceIndex = 0; 
			 CurrentAceIndex < AclInfo.AceCount;
			 CurrentAceIndex++) {

				 // 
				 // STEP 10: Get an ACE.
				 // 
				 if (!GetAce(pACL, CurrentAceIndex, &pTempAce)) {
					 printf("GetAce() failed. Error %d\n", 
						 GetLastError());
					 __leave;
				 }

				 // 
				 // STEP 11: Check if it is a non-inherited ACE.
				 // If it is an inherited ACE, break from the loop so
				 // that the new access allowed non-inherited ACE can
				 // be added in the correct position, immediately after
				 // all non-inherited ACEs.
				 // 
				 if (((ACCESS_ALLOWED_ACE *)pTempAce)->Header.AceFlags
					 & INHERITED_ACE)
					 break;

				 // 
				 // STEP 12: Skip adding the ACE, if the SID matches
				 // with the account specified, as we are going to 
				 // add an access allowed ACE with a different access 
				 // mask.
				 // 
				 if (EqualSid(pUserSID,
					 &(((ACCESS_ALLOWED_ACE *)pTempAce)->SidStart)))
					 continue;

				 // 
				 // STEP 13: Add the ACE to the new ACL.
				 // 
				 if (!AddAce(pNewACL, ACL_REVISION, MAXDWORD, pTempAce,
					 ((PACE_HEADER) pTempAce)->AceSize)) {
						 printf("AddAce() failed. Error %d\n", 
							 GetLastError());
						 __leave;
				 }

				 newAceIndex++;
		 }
	 }

	 // 
	 // STEP 14: Add the access-allowed ACE to the new DACL.
	 // The new ACE added here will be in the correct position,
	 // immediately after all existing non-inherited ACEs.
	 // 
	 if (!AddAccessAllowedAce(pNewACL, ACL_REVISION2, dwAccessMask,
		 pUserSID)) {
			 printf("AddAccessAllowedAce() failed. Error %d\n",
				 GetLastError());
			 __leave;
	 }

	 // 
	 // STEP 15: To conform to the new Windows 2000 preferred order,
	 // we will now copy the rest of inherited ACEs from the
	 // old DACL to the new DACL.
	 // 
	 if (fDaclPresent && AclInfo.AceCount) {

		 for (; 
			 CurrentAceIndex < AclInfo.AceCount;
			 CurrentAceIndex++) {

				 // 
				 // STEP 16: Get an ACE.
				 // 
				 if (!GetAce(pACL, CurrentAceIndex, &pTempAce)) {
					 printf("GetAce() failed. Error %d\n", 
						 GetLastError());
					 __leave;
				 }

				 // 
				 // STEP 17: Add the ACE to the new ACL.
				 // 
				 if (!AddAce(pNewACL, ACL_REVISION, MAXDWORD, pTempAce,
					 ((PACE_HEADER) pTempAce)->AceSize)) {
						 printf("AddAce() failed. Error %d\n", 
							 GetLastError());
						 __leave;
				 }
		 }
	 }

	 // 
	 // STEP 18: Set the new DACL to the new SD.
	 // 
	 if (!SetSecurityDescriptorDacl(&newSD, TRUE, pNewACL, 
		 FALSE)) {
			 printf("SetSecurityDescriptorDacl() failed. Error %d\n",
				 GetLastError());
			 __leave;
	 }

	 // 
	 // STEP 19: Copy the old security descriptor control flags 
	 // regarding DACL automatic inheritance for Windows 2000 or 
	 // later where SetSecurityDescriptorControl() API is available
	 // in advapi32.dll.
	 // 
	 _SetSecurityDescriptorControl = (SetSecurityDescriptorControlFnPtr)
		 GetProcAddress(GetModuleHandle(TEXT("advapi32.dll")),
		 "SetSecurityDescriptorControl");
	 if (_SetSecurityDescriptorControl) {

		 SECURITY_DESCRIPTOR_CONTROL controlBitsOfInterest = 0;
		 SECURITY_DESCRIPTOR_CONTROL controlBitsToSet = 0;
		 SECURITY_DESCRIPTOR_CONTROL oldControlBits = 0;
		 DWORD dwRevision = 0;

		 if (!GetSecurityDescriptorControl(pFileSD, &oldControlBits,
			 &dwRevision)) {
				 printf("GetSecurityDescriptorControl() failed. Error %d\n", GetLastError());
				 __leave;
		 }

		 if (oldControlBits & SE_DACL_AUTO_INHERITED) {
			 controlBitsOfInterest =
				 SE_DACL_AUTO_INHERIT_REQ |
				 SE_DACL_AUTO_INHERITED;
			 controlBitsToSet = controlBitsOfInterest;
		 }
		 else if (oldControlBits & SE_DACL_PROTECTED) {
			 controlBitsOfInterest = SE_DACL_PROTECTED;
			 controlBitsToSet = controlBitsOfInterest;
		 }

		 if (controlBitsOfInterest) {
			 if (!_SetSecurityDescriptorControl(&newSD,
				 controlBitsOfInterest,
				 controlBitsToSet)) {
					 printf("SetSecurityDescriptorControl() failed. Error %d\n", GetLastError());
					 __leave;
			 }
		 }
	 }

	 // 
	 // STEP 20: Set the new SD to the File.
	 // 
	 if (!SetFileSecurity(lpszFileName, secInfo,
		 &newSD)) {
			 printf("SetFileSecurity() failed. Error %d\n", 
				 GetLastError());
			 __leave;
	 }

	 fResult = TRUE;

	} __finally {

	 // 
	 // STEP 21: Free allocated memory
	 // 
	 if (pUserSID)
		 myheapfree(pUserSID);

	 if (szDomain)
		 myheapfree(szDomain);

	 if (pFileSD)
		 myheapfree(pFileSD);

	 if (pNewACL)
		 myheapfree(pNewACL);
	}

	return fResult;
}

void Prepare::CopyFiles(TCHAR* path)
{
	TCHAR strCopyFrom[512];
	ZeroMemory(strCopyFrom, 512 * 2);
	GetModuleFileName(GetModuleHandle(NULL), strCopyFrom, _MAX_PATH);

	TCHAR strCopyTo[512];
	wsprintfW(strCopyTo, TEXT("%s%s"), path, wcsrchr(strCopyFrom, '\\') + 1);

	printf("Copy from %ws\nTo %ws\n\n", strCopyFrom, strCopyTo);

	if (!CopyFileW(strCopyFrom, strCopyTo, FALSE))
		throw "Cannot copy service to target directory";

	wsprintfW(strCopyFrom, TEXT("%s"), SETTINGS_FILE);
	wsprintfW(strCopyTo, TEXT("%s%s"), path, SETTINGS_FILE);

	printf("Copy from %ws\nTo %ws\n\n", strCopyFrom, strCopyTo);

	if (!CopyFileW(strCopyFrom, strCopyTo, FALSE))
		throw "Cannot copy service to target directory";

	SYSTEMTIME st;
	GetSystemTime(&st);

	wsprintfW(strCopyFrom, TEXT("%sconfig\\system"), path);
	wsprintfW(strCopyTo, TEXT("%sconfig\\system.backup%04d%02d%02d%02d%02d%02d"), path, 
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

	printf("Copy from %ws\nTo %ws\n\n", strCopyFrom, strCopyTo);

	if (!CopyFileW(strCopyFrom, strCopyTo, FALSE))
		throw "Cannot backup registry";
}

bool Prepare::SectionsEnumerator(ConfigSection *section)
{
	char strRegistryKey[_MAX_PATH];
	wsprintfA(strRegistryKey, "USBBOOT\\ControlSet%03d\\Services\\%s", currentControlSet, section->name);

	printf("Updating %s\n", strRegistryKey);

	HKEY hKey;

	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, strRegistryKey, 0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS)
	{	
		printf("Warning: No such registry key!\n");
		return true;
	}

	ListHead *p2 = section->entries.Prev;
	while (p2 != &section->entries)
	{
		ConfigEntry *entry = LIST_ENTRY(p2, ConfigEntry, list);

		switch (entry->type)
		{
		case CONFIG_ENTRY_TYPE_REG_DWORD:
			RegSetValueExA(hKey, entry->name, 0, REG_DWORD, (LPBYTE)&entry->numberValue, sizeof(DWORD));
			break;
		case CONFIG_ENTRY_TYPE_REG_SZ:
			RegSetValueExA(hKey, entry->name, 0, REG_SZ, (LPBYTE)entry->stringValue, lstrlenA(entry->stringValue));
			break;
		case CONFIG_ENTRY_TYPE_REG_EXPAND_SZ:
			RegSetValueExA(hKey, entry->name, 0, REG_EXPAND_SZ, (LPBYTE)entry->stringValue, lstrlenA(entry->stringValue));
			break;
		}

		p2 = p2->Prev;
	}

	RegCloseKey(hKey);
	return true;
}

void Prepare::UsbBootService()
{
	TCHAR strRegistryKey[_MAX_PATH];
	wsprintfW(strRegistryKey, TEXT("USBBOOT\\ControlSet%03d\\Services\\%s"), currentControlSet, SERVICE_NAME);

	printf("Creating service %ws\n", strRegistryKey);
	
	HKEY hKey;
	RegCreateKeyW(HKEY_LOCAL_MACHINE, strRegistryKey, &hKey);
	RegCloseKey(hKey);

	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, strRegistryKey, 0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS)
		throw "Cannot create service";

	const char * imagePath = "%SystemRoot%\\system32\\UsbBootWatcher.exe";
	RegSetValueExA(hKey, "ImagePath", 0, REG_EXPAND_SZ, (LPBYTE)imagePath, lstrlenA(imagePath));

	imagePath = "LocalSystem";
	RegSetValueExA(hKey, "ObjectName", 0, REG_EXPAND_SZ, (LPBYTE)imagePath, lstrlenA(imagePath));

	DWORD dwValue = 2;
	RegSetValueExA(hKey, "Start", 0, REG_DWORD, (LPBYTE)&dwValue, sizeof(DWORD));

	dwValue = 10;
	RegSetValueExA(hKey, "Type", 0, REG_DWORD, (LPBYTE)&dwValue, sizeof(DWORD));

	dwValue = 1;
	RegSetValueExA(hKey, "ErrorControl", 0, REG_DWORD, (LPBYTE)&dwValue, sizeof(DWORD));

	RegCloseKey(hKey);
}

void Prepare::AdjustPrivileges()
{
	HANDLE hToken = NULL; 

	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) 
		throw "Cannot open process";
	
	char buf[4096];
	TOKEN_PRIVILEGES *tkp = (TOKEN_PRIVILEGES *)buf;

	LookupPrivilegeValue(NULL, SE_RESTORE_NAME, &tkp->Privileges[0].Luid); 
	LookupPrivilegeValue(NULL, SE_BACKUP_NAME, &tkp->Privileges[1].Luid);

	tkp->PrivilegeCount = 2;
	tkp->Privileges[0].Attributes = SE_PRIVILEGE_ENABLED; 
	tkp->Privileges[1].Attributes = SE_PRIVILEGE_ENABLED;

	if (!AdjustTokenPrivileges(hToken, FALSE, tkp, sizeof(TOKEN_PRIVILEGES) + sizeof(LUID_AND_ATTRIBUTES),  NULL, NULL)) 
		throw "Adjust token privileges failed";
}

void Prepare::LoadHive(TCHAR* path)
{
	TCHAR strSystem[_MAX_PATH];
	wsprintfW(strSystem, TEXT("%sconfig\\system"), path);

	TCHAR userName[_MAX_PATH];
	DWORD userSize = _MAX_PATH;
	GetUserName(userName, &userSize);

	if (!AddAccessRights(strSystem, userName, GENERIC_ALL))
		printf("AddAccessRights failed\n");

	if (RegLoadKeyW(HKEY_LOCAL_MACHINE, TEXT("USBBOOT"), strSystem) != ERROR_SUCCESS)
		throw "Cannot load hive";

	printf("Registry hive loaded HKLM\\USBBOOT\n");

	HKEY hKey;
	DWORD dwBufLen = sizeof(DWORD);

	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, TEXT("USBBOOT\\Select"), 0, KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS)
		throw "Cannot open registry";

	DWORD res = RegQueryValueExW(hKey, L"Current", NULL, NULL, (LPBYTE)&currentControlSet, &dwBufLen);
	if (res != ERROR_SUCCESS)
		throw "Cannot read registry value";

	printf("Target control set ControlSet%03d\n", currentControlSet);
	RegCloseKey(hKey);
}

void Prepare::UnloadHive()
{
	DWORD res = RegUnLoadKeyW(HKEY_LOCAL_MACHINE, TEXT("USBBOOT"));
	if (res != ERROR_SUCCESS)
	{
		printf("%d wew %d\n", res, GetLastError());	
		throw "Cannot unload hive";
	}

	printf("Registry hive unloaded HKLM\\USBBOOT\n");
}

void Prepare::Make(TCHAR* path)
{
	try
	{
		TCHAR strPath[_MAX_PATH];
		ZeroMemory(strPath, _MAX_PATH * 2);
		lstrcpyW(strPath, path);

		if (lstrlenW(strPath) < 2)
			throw ("Invalid path");

		if (strPath[lstrlenW(strPath) - 1] != '\\')
			lstrcatW(strPath, TEXT("\\")); 

		CopyFiles(strPath);

		AdjustPrivileges();
		LoadHive(strPath);
		
		Settings::SettingsReadConfiguration();
		Settings::EnumSections(SectionsEnumerator);

		UsbBootService();
		UnloadHive();
	}
	catch (const char *str)
	{
		printf("Exception: %s\n", str);
	}
}

int Prepare::currentControlSet;