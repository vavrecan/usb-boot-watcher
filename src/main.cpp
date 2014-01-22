//
// Usb Boot Watcher - Marek Vavrecan, 2009
// Parser class for UsbBootWatcher - Klaus Daniel Terhorst, 2009
//

#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include "settings.h"
#include "prepare.h"

class Service
{
public:
	static SERVICE_STATUS serviceStatus;
	static SERVICE_STATUS_HANDLE serviceStatusHandle;
	static HANDLE stopServiceEvent;

	static void WINAPI ServiceControlHandler(DWORD controlCode)
	{
		switch (controlCode)
		{
		case SERVICE_CONTROL_INTERROGATE:
			break;

		case SERVICE_CONTROL_SHUTDOWN:
		case SERVICE_CONTROL_STOP:

			CheckForChange();

			serviceStatus.dwCurrentState = SERVICE_STOP_PENDING;
			SetServiceStatus(serviceStatusHandle, &serviceStatus);

			SetEvent(stopServiceEvent);
			return;

		case SERVICE_CONTROL_PAUSE:
			break;

		case SERVICE_CONTROL_CONTINUE:
			break;
		}

		SetServiceStatus(serviceStatusHandle, &serviceStatus);
	}

	static bool SectionsEnumerator(ConfigSection *section)
	{
		char strRegistryKey[_MAX_PATH];
		char strValue[_MAX_PATH];
		wsprintfA(strRegistryKey, "SYSTEM\\CurrentControlSet\\Services\\%s", section->name);
		//wsprintfA(strRegistryKey, "USBBOOT\\ControlSet001\\Services\\%s", section->name);

		//printf("Checking %s...\n", strRegistryKey);

		HKEY hKey;
		DWORD dwValue = 0;
		DWORD dwType = 0;
		DWORD dwBufLen = 0;

		if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, strRegistryKey, NULL, KEY_QUERY_VALUE | KEY_SET_VALUE, &hKey) != ERROR_SUCCESS)
			return true;

		ListHead *p2 = section->entries.Prev;
		while (p2 != &section->entries)
		{
			ConfigEntry *entry = LIST_ENTRY(p2, ConfigEntry, list);
			ZeroMemory(strValue, _MAX_PATH);

			switch (entry->type)
			{
				case CONFIG_ENTRY_TYPE_REG_DWORD:
				{
					dwBufLen = sizeof(DWORD);
					RegQueryValueExA(hKey, entry->name, NULL, &dwType, (LPBYTE)&dwValue, &dwBufLen);

					if (dwValue != entry->numberValue)
						RegSetValueExA(hKey, entry->name, 0, REG_DWORD, (LPBYTE)&entry->numberValue, sizeof(DWORD));
				}				
				break;
				case CONFIG_ENTRY_TYPE_REG_SZ:
				{
					dwBufLen = _MAX_PATH;
					RegQueryValueExA(hKey, entry->name, NULL, &dwType, (LPBYTE)strValue, &dwBufLen);

					if (lstrcmpiA(entry->stringValue, strValue) != 0)
						RegSetValueExA(hKey, entry->name, 0, REG_SZ, (LPBYTE)entry->stringValue, lstrlenA(entry->stringValue));
				}	
				break;
				case CONFIG_ENTRY_TYPE_REG_EXPAND_SZ:
				{
					dwBufLen = _MAX_PATH;
					RegQueryValueExA(hKey, entry->name, NULL, &dwType, (LPBYTE)strValue, &dwBufLen);

					if (lstrcmpiA(entry->stringValue, strValue) != 0)
						RegSetValueExA(hKey, entry->name, 0, REG_EXPAND_SZ, (LPBYTE)entry->stringValue, lstrlenA(entry->stringValue));
				}			
				break;
			}

			p2 = p2->Prev;
		}

		RegCloseKey(hKey);
		return true;
	}

	static void CheckForChange()
	{
		Settings::EnumSections(SectionsEnumerator);
	}

	static bool WaitOnChange()
	{
		int eventsCount = Settings::GetSectionsCount();
		char szRegPath[_MAX_PATH];
		HANDLE * events = new HANDLE[eventsCount];
		HKEY hKey;

		if (eventsCount < 1)
			return false;

		for (int i = 0; i < eventsCount; i++)
		{
			events[i] = CreateEvent(NULL, TRUE, FALSE, NULL);

			wsprintfA(szRegPath, "SYSTEM\\CurrentControlSet\\Services\\%s", Settings::GetSectionName(i));
			RegOpenKeyExA(HKEY_LOCAL_MACHINE, szRegPath, 0, KEY_NOTIFY, &hKey);

			//printf("Event %s\n", szRegPath);

			RegNotifyChangeKeyValue(hKey, TRUE, REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_ATTRIBUTES | 
												REG_NOTIFY_CHANGE_LAST_SET, events[i], TRUE);
		}

		while (WaitForMultipleObjects(eventsCount, events, FALSE, 10000) == WAIT_TIMEOUT);
		delete [] events;

		for (int i = 0; i < eventsCount; i++)
			CloseHandle(events[i]);

		//printf("Change detected... updating...\n");

		RegCloseKey(hKey);
		CheckForChange();		
		return true;
	}

	static DWORD WINAPI WaitForChangesThread(LPVOID lpvParam)
	{
		try
		{
			while (WaitOnChange());
		}
		catch (...)
		{
		}

		return 0;
	}

	static void WINAPI ServiceMain(DWORD argc, TCHAR* argv[])
	{
		serviceStatusHandle = 0;
		stopServiceEvent = 0;

		// initialize service status
		serviceStatus.dwServiceType = SERVICE_WIN32;
		serviceStatus.dwCurrentState = SERVICE_STOPPED;
		serviceStatus.dwControlsAccepted = 0;
		serviceStatus.dwWin32ExitCode = NO_ERROR;
		serviceStatus.dwServiceSpecificExitCode = NO_ERROR;
		serviceStatus.dwCheckPoint = 0;
		serviceStatus.dwWaitHint = 0;

		serviceStatusHandle = RegisterServiceCtrlHandler(SERVICE_NAME, ServiceControlHandler);

		if (serviceStatusHandle)
		{
			// service is starting
			serviceStatus.dwCurrentState = SERVICE_START_PENDING;
			SetServiceStatus(serviceStatusHandle, &serviceStatus);

			// do initialization here
			stopServiceEvent = CreateEvent(0, FALSE, FALSE, 0);

			// running
			serviceStatus.dwControlsAccepted |= (SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN);
			serviceStatus.dwCurrentState = SERVICE_RUNNING;
			SetServiceStatus(serviceStatusHandle, &serviceStatus);

			// checking
			try
			{
				Settings::SettingsReadConfiguration();
				CheckForChange();

				DWORD dwThreadID;
				HANDLE threadHandle = CreateThread(NULL, 0, WaitForChangesThread, (LPVOID)0, 0, &dwThreadID);

				while (WaitForSingleObject(stopServiceEvent, 10000) == WAIT_TIMEOUT);

				TerminateThread(threadHandle, 0);
			}
			catch (...)
			{
				// exception
			}
			
			// service was stopped
			serviceStatus.dwCurrentState = SERVICE_STOP_PENDING;
			SetServiceStatus(serviceStatusHandle, &serviceStatus);

			// do cleanup here
			CloseHandle(stopServiceEvent);
			stopServiceEvent = 0;

			// service is now stopped
			serviceStatus.dwControlsAccepted &= ~(SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN);
			serviceStatus.dwCurrentState = SERVICE_STOPPED;
			SetServiceStatus(serviceStatusHandle, &serviceStatus);
		}
	}

	static void InstallService()
	{
		SC_HANDLE serviceControlManager = OpenSCManager(0, 0, SC_MANAGER_CREATE_SERVICE);

		if (serviceControlManager)
		{
			TCHAR path[_MAX_PATH + 1];
			if (GetModuleFileName(0, path, sizeof(path)/sizeof(path[0])) > 0)
			{
				SC_HANDLE service = CreateService(
					serviceControlManager,
					SERVICE_NAME, 
					SERVICE_NAME,
					SERVICE_ALL_ACCESS, 
					SERVICE_WIN32_OWN_PROCESS,
					SERVICE_AUTO_START, 
					SERVICE_ERROR_IGNORE, 
					path,
					0, 0, 0, 0, 0);

				if (service)
					CloseServiceHandle(service);
			}

			CloseServiceHandle(serviceControlManager);
		}

		ServiceStart();
	}

	static void UninstallService()
	{
		ServiceStop();

		SC_HANDLE serviceControlManager = OpenSCManager(0, 0, SC_MANAGER_CONNECT);

		if (serviceControlManager)
		{
			SC_HANDLE service = OpenService(serviceControlManager, SERVICE_NAME, SERVICE_QUERY_STATUS | DELETE);

			if (service)
			{
				SERVICE_STATUS serviceStatus;
				if (QueryServiceStatus(service, &serviceStatus))
				{
					if (serviceStatus.dwCurrentState == SERVICE_STOPPED)
						DeleteService(service);
				}

				CloseServiceHandle(service);
			}

			CloseServiceHandle(serviceControlManager);
		}
	}

	static void ServiceStart()
	{
		SC_HANDLE serviceControlManager = OpenSCManager(0, 0, SC_MANAGER_CONNECT);

		if(serviceControlManager) 
		{
			SC_HANDLE service = OpenService(serviceControlManager, SERVICE_NAME, SERVICE_QUERY_STATUS | SERVICE_START);

			if(service) 
			{
				SERVICE_STATUS serviceStatus;
				if(QueryServiceStatus(service, &serviceStatus)) 
				{
					if(serviceStatus.dwCurrentState == SERVICE_STOPPED) 
					{
						StartService(service, 0, NULL);	
					} 
				}

				CloseServiceHandle(service);
			}

			CloseServiceHandle(serviceControlManager);
		}
	}

	static void ServiceStop()
	{
		SC_HANDLE serviceControlManager = OpenSCManager(0, 0, SC_MANAGER_CONNECT);

		if(serviceControlManager) 
		{
			SC_HANDLE service = OpenService(serviceControlManager, SERVICE_NAME, SERVICE_QUERY_STATUS | SERVICE_STOP);

			if(service) 
			{
				SERVICE_STATUS serviceStatus;
				if(QueryServiceStatus(service, &serviceStatus)) 
				{
					if(serviceStatus.dwCurrentState == SERVICE_RUNNING) 
					{
						ControlService(service, SERVICE_CONTROL_STOP, &serviceStatus);
					} 
				}

				CloseServiceHandle(service);
			}

			CloseServiceHandle(serviceControlManager);
		}
	}
};

SERVICE_STATUS Service::serviceStatus;
SERVICE_STATUS_HANDLE Service::serviceStatusHandle;
HANDLE Service::stopServiceEvent;

int _tmain(int argc, TCHAR* argv[])
{
	SERVICE_TABLE_ENTRY serviceTable[] =
	{
		{ SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)Service::ServiceMain },
		{ 0, 0 }
	};

	if (!(argc > 1))
	{
		StartServiceCtrlDispatcher(serviceTable);
		return 0;
	}

	if (argc > 1 && lstrcmpi(argv[1], TEXT("/install")) == 0)
	{
		Service::InstallService();
	}
	else if (argc > 1 && lstrcmpi(argv[1], TEXT("/uninstall")) == 0)
	{
		Service::UninstallService();
	}
	else if (argc > 2 && lstrcmpi(argv[1], TEXT("/prepare")) == 0)
	{
		Prepare::Make(argv[2]);
	}
	else if (argc > 1 && lstrcmpi(argv[1], TEXT("/test")) == 0)
	{
		Settings::SettingsReadConfiguration();
		Service::CheckForChange();
		while (Service::WaitOnChange());
	}
	else if (argc > 1 && lstrcmpi(argv[1], TEXT("/?")) == 0)
	{
		printf("USB Boot preparation by MarV\n\n");
		printf("Install service on current system:\n  /install\n");
		printf("Uninstall service from current system:\n  /uninstall\n");
		printf("Prepare USB boot:\n  /prepare [system32 path]\n\n");
		printf("Active configuration:\n\n");

		try
		{
			Settings::SettingsReadConfiguration();
			Settings::DumpConfiguration();
		}
		catch (const char *str)
		{
			printf("Exception: %s\n", str);
		}
	}

	return 0;
}