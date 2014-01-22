#ifndef __SETTINGS_H
#define __SETTINGS_H

#include "clist.h"

#define SERVICE_NAME TEXT("Usb Boot Watcher Service")
#define SETTINGS_FILE TEXT("UsbBootWatcher.conf")

enum ConfigEntryType
{
	CONFIG_ENTRY_TYPE_REG_DWORD,
	CONFIG_ENTRY_TYPE_REG_SZ,
	CONFIG_ENTRY_TYPE_REG_EXPAND_SZ
};

struct ConfigEntry
{
	ListHead list;
	const char *name;
	ConfigEntryType type;
	union 
	{
		int numberValue;
		const char *stringValue;
	};
};

struct ConfigSection
{
	ListHead list;
	const char *name;
	ListHead entries;
};

class Settings
{
public:
	static void SettingsReadConfiguration();
	static void DumpConfiguration();
	static void EnumSection(const char *sectionName, bool (*enumerator)(ConfigSection *, ConfigEntry *));
	static void EnumSections(bool (*enumerator)(ConfigSection *));
	static int GetSectionsCount();
	static const char * GetSectionName(int offset);
};

#endif
