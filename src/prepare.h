#ifndef __PREPARE_H
#define __PREPARE_H

class Prepare
{
public:
	static void Make(TCHAR* path);
private:
	static int currentControlSet;
	static void CopyFiles(TCHAR* path);
	static bool SectionsEnumerator(ConfigSection *section);
	static void LoadHive(TCHAR* path);
	static void UnloadHive();
	static void AdjustPrivileges();
	static void UsbBootService();
};

#endif


