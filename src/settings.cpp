#include <windows.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include "settings.h"

enum Token
{
	TOKEN_EOF,
	TOKEN_NEW_LINE,
	TOKEN_COLON,
	TOKEN_SEMICOLON,
	TOKEN_STRING,
	TOKEN_NAME,
	TOKEN_NUM,
	TOKEN_ADD,
	TOKEN_SUB,
	TOKEN_MUL,
	TOKEN_DIV,
	TOKEN_EQU,
	TOKEN_OPEN_BRACKET,
	TOKEN_CLOSE_BRACKET,
	TOKEN_AT
};

const char *TokenStrings[] = {"EOF", "New line", ":", ";", "String", "Name", "Number", "+", "-", "*", "/", "=", "[", "]", "@"};

bool IsSpace(char c)
{
	switch (c)
	{
	case ' ':
	case '\n':
	case '\r':
		return true;
	default:
		return false;
	}
}

bool IsNameChar(char c)
{
	return (isalnum(c) || c == '_' || c == '-');
}

class Lexer
{
private:
	char *_input;
	int _pos;
	size_t _len;
	Token _currentToken;
	char _ch;
	int _currentNumber;
	char *_currentString;

public:
	Lexer(const char *input)
	{
		_pos = 0;
		_len = strlen(input);
		_input = new char[_len+1];
		strcpy(_input, input);
		_input[_len] = (char)0xff;
		_currentString = NULL;
		Input();
	}

	~Lexer()
	{
		if (_input)
			delete[] _input;
		if (_currentString)
			delete[] _currentString;
	}

	Token GetCurrentToken()
	{
		return _currentToken;
	}

	char GetCurrentChar()
	{
		return _ch;
	}

	int GetCurrentNumber()
	{
		return _currentNumber;
	}

	const char *GetCurrentString()
	{
		return _currentString;
	}

	const char *GetCurrentName()
	{
		return _currentString;
	}

	void Input()
	{
		_ch = _input[_pos++];
	}

	char Peek()
	{
		int pos = _pos;
		while (IsSpace(_input[pos]))
			pos++;
		return _input[pos];
	}

	void Expect(Token t)
	{
		if (_currentToken != t)
		{
			throw "Syntax error"; 
		}
	}

	void Next()
	{
		while (IsSpace(_ch))
			Input();
		switch (_ch)
		{
		case '\xff':
			_currentToken = TOKEN_EOF;
			return;
		case '=':
			Input();
			_currentToken = TOKEN_EQU;
			return;
		case ':':
			Input();
			_currentToken = TOKEN_COLON;
			return;
		case ';':
			Input();
			_currentToken = TOKEN_SEMICOLON;
			return;
		case '[':
			Input();
			_currentToken = TOKEN_OPEN_BRACKET;
			return;
		case ']':
			Input();
			_currentToken = TOKEN_CLOSE_BRACKET;
			return;
		case '+':
			Input();
			_currentToken = TOKEN_ADD;
			return;
		case '-':
			Input();
			_currentToken = TOKEN_SUB;
			return;
		case '*':
			Input();
			_currentToken = TOKEN_MUL;
			return;
		case '/':
			Input();
			_currentToken = TOKEN_DIV;
			return;
		case '@':
			Input();
			_currentToken = TOKEN_AT;
			return;
		}
		if (isdigit(_ch))
		{
			int pos = _pos - 1;
			int digits = 0;
			int num = 0;
			int digit = 0;
			while (isdigit(_input[pos]))
			{
				digits++;
				pos++;
			}
			pos = _pos - 1;
			for (int i = digits - 1; i >= 0; i--)
			{
				digit = _input[pos] - 0x30;
				num += digit * (int)pow(10.0, (double)i);
				pos++;
			}
			_currentNumber = num;
			_pos = pos;
			_currentToken = TOKEN_NUM;
			Input();
			return;
		}
		else if (_ch == '"')
		{
			int pos = _pos;
			int len = 0;
			while (_input[pos] != '"')
			{
				len++;
				pos++;
				if (_input[pos] == '\xff')
				{
					// syntax error
					throw "Syntax error";
					return;
				}
			}
			if (_currentString)
				delete[] _currentString;

			_currentString = new char[len + 1];
			pos = _pos;
			char *p = _currentString;
			
			while (_input[pos] != '"')
				*p++ = _input[pos++];
			*p = 0;

			_currentToken = TOKEN_STRING;
			_pos = pos + 1;
			Input();
			return;
		}
		else if (IsNameChar(_ch))
		{
			int pos = _pos - 1;
			int len = 0;
			while (IsNameChar(_input[pos]))
			{
				len++;
				pos++;
			}
			if (_currentString)
				delete[] _currentString;

			_currentString = new char[len + 1];
			pos = _pos - 1;
			char *p = _currentString;

			while (IsNameChar(_input[pos]))
				*p++ = _input[pos++];
			*p = 0;

			_currentToken = TOKEN_NAME;
			_pos = pos;
			Input();
			return;
		}
	}
};

int Factor(Lexer *lex)
{
	Token t = lex->GetCurrentToken();

	if (t == TOKEN_NUM)
	{
		int num = lex->GetCurrentNumber();
		lex->Next();
		return num;
	}
	else
	{
		return 0;
	}
}

int Term(Lexer *lex)
{
	int lhs = Factor(lex);
	Token t = lex->GetCurrentToken();

	while (t == TOKEN_MUL || t == TOKEN_DIV)
	{
		if (t == TOKEN_MUL)
		{
			lex->Next();
			int rhs = Factor(lex);
			lhs *= rhs;
		}
		else if (t == TOKEN_DIV)
		{
			lex->Next();
			int rhs = Factor(lex);
			lhs /= rhs;
		}

		t = lex->GetCurrentToken();
	}

	return lhs;
}

int Expression(Lexer *lex)
{
	int lhs = Term(lex);
	Token t = lex->GetCurrentToken();

	while (t == TOKEN_ADD || t == TOKEN_SUB)
	{
		if (t == TOKEN_ADD)
		{
			lex->Next();
			int rhs = Term(lex);
			lhs += rhs;
		}
		else if (t == TOKEN_SUB)
		{
			lex->Next();
			int rhs = Term(lex);
			lhs -= rhs;
		}
		else
		{
			break;
		}

		t = lex->GetCurrentToken();
	}

	return lhs;
}

class ConfigParser
{
private:
	Lexer _lex;
	ListHead _sections;

	void ParseSectionEntries(ConfigSection *section)
	{
		InitListHead(&section->entries);
		for (;;)
		{
			_lex.Next();
			if (_lex.GetCurrentToken() == TOKEN_OPEN_BRACKET ||
				_lex.GetCurrentToken() == TOKEN_EOF)
				break;

			_lex.Expect(TOKEN_NAME);

			ConfigEntry *entry = new ConfigEntry;
			entry->name = _strdup(_lex.GetCurrentString());
			
			_lex.Next();
			_lex.Expect(TOKEN_AT);

			_lex.Next();
			_lex.Expect(TOKEN_NAME);
			const char *name = _lex.GetCurrentString();

			if (!_stricmp(name, "REG_DWORD"))
			{
				_lex.Next();
				_lex.Expect(TOKEN_EQU);
				_lex.Next();
				//entry->numberValue = Expression(&_lex);
				_lex.Expect(TOKEN_NUM);
				entry->numberValue = _lex.GetCurrentNumber();
				entry->type = CONFIG_ENTRY_TYPE_REG_DWORD;
			}
			else if (!_stricmp(name, "REG_SZ"))
			{
				_lex.Next();
				_lex.Expect(TOKEN_EQU);
				_lex.Next();
				_lex.Expect(TOKEN_STRING);
				entry->stringValue = _strdup(_lex.GetCurrentString());
				entry->type = CONFIG_ENTRY_TYPE_REG_SZ;
			}
			else if (!_stricmp(name, "REG_EXPAND_SZ"))
			{
				_lex.Next();
				_lex.Expect(TOKEN_EQU);
				_lex.Next();
				_lex.Expect(TOKEN_STRING);
				entry->stringValue = _strdup(_lex.GetCurrentString());
				entry->type = CONFIG_ENTRY_TYPE_REG_EXPAND_SZ;
			}
			else
			{
				throw "Unknown type";
			}

			//_lex.Next();
			//_lex.Expect(TOKEN_SEMICOLON);

			// OK - entry parsed :D

			ListAdd(&entry->list, &section->entries);
		}
	}

	void ParseSection()
	{
		_lex.Expect(TOKEN_OPEN_BRACKET);
		_lex.Next();
		_lex.Expect(TOKEN_NAME);

		ConfigSection *section = new ConfigSection;
		section->name = _strdup(_lex.GetCurrentName());

		_lex.Next();
		_lex.Expect(TOKEN_CLOSE_BRACKET);

		ParseSectionEntries(section);

		ListAdd(&section->list, &_sections);
	}

public:
	ConfigParser(const char *input)
		: _lex(input)
	{
		InitListHead(&_sections);
	}

	~ConfigParser()
	{
		ListHead *p;
		ConfigSection *section;

		p = _sections.Prev;
		while (p != &_sections)
		{
			section = LIST_ENTRY(p, ConfigSection, list);
			
			free((void *)section->name);

			ListHead *p2 = section->entries.Prev;
			while (p2 != &section->entries)
			{
				ConfigEntry *entry = LIST_ENTRY(p2, ConfigEntry, list);

				free((void *)entry->name);
			
				switch (entry->type)
				{
				case CONFIG_ENTRY_TYPE_REG_SZ:
				case CONFIG_ENTRY_TYPE_REG_EXPAND_SZ:
					free((void *)entry->stringValue);
					break;
				}

				p2 = p2->Prev;

				free((void *)entry);
			}

			p = p->Prev;

			free((void *)section);
		}
	}

	void Parse()
	{
		_lex.Next();
		for (;;)
		{
			if (_lex.GetCurrentToken() == TOKEN_EOF)
				break;
			ParseSection();
		}
	}

	ConfigSection *GetSection(const char *sectionName)
	{
		ListHead *p;
		ConfigSection *section;

		p = _sections.Prev;
		while (p != &_sections)
		{
			section = LIST_ENTRY(p, ConfigSection, list);

			if (!strcmp(section->name, sectionName))
				return section;

			p = p->Prev;
		}

		return NULL;
	}

	int SectionsCount()
	{
		int z = 0;
		ListHead *p;
		ConfigSection *section;

		p = _sections.Prev;
		while (p != &_sections)
		{
			section = LIST_ENTRY(p, ConfigSection, list);
			z++;
			p = p->Prev;
		}
		return z;
	}

	bool SectionExists(const char *sectionName)
	{
		ConfigSection *section = GetSection(sectionName);

		return section != NULL;
	}

	ConfigEntry *GetEntry(const char *sectionName, const char *entryName)
	{
		ConfigSection *section = GetSection(sectionName);
		if (!section)
			return NULL;

		ListHead *p = section->entries.Prev;
		ConfigEntry *entry;

		while (p != &section->entries)
		{
			entry = LIST_ENTRY(p, ConfigEntry, list);

			if (!strcmp(entry->name, entryName))
				return entry;
			
			p = p->Prev;
		}

		return NULL;
	}

	bool EntryExists(const char *sectionName, const char *entryName)
	{
		ConfigEntry *entry = GetEntry(sectionName, entryName);

		return entry != NULL;
	}

	void EnumSection(const char *sectionName, bool (*enumerator)(ConfigSection *, ConfigEntry *))
	{
		ConfigSection *section = GetSection(sectionName);
		if (!section)
			return;

		ListHead *p = section->entries.Prev;
		ConfigEntry *entry;

		while (p != &section->entries)
		{
			entry = LIST_ENTRY(p, ConfigEntry, list);

			if (!enumerator(section, entry))
				return;

			p = p->Prev;
		}
	}

	void EnumSections(bool (*enumerator)(ConfigSection *))
	{
		ListHead *p;
		ConfigSection *section;

		p = _sections.Prev;
		while (p != &_sections)
		{
			section = LIST_ENTRY(p, ConfigSection, list);

			if (!enumerator(section))
				return;

			p = p->Prev;
		}
	}

	int GetInt(const char *sectionName, const char *entryName)
	{
		ConfigEntry *entry = GetEntry(sectionName, entryName);
		if (entry)
			return entry->numberValue;

		return 0;
	}

	const char *GetSectionName(int offset)
	{
		int curr = 0;
		ListHead *p;
		ConfigSection *section;

		p = _sections.Prev;
		while (p != &_sections)
		{
			section = LIST_ENTRY(p, ConfigSection, list);

			if (curr == offset)
				return section->name;

			curr++;
			p = p->Prev;
		}

		return NULL;
	}
	
	const char *GetString(const char *sectionName, const char *entryName)
	{
		ConfigEntry *entry = GetEntry(sectionName, entryName);
		if (entry)
			return entry->stringValue;

		return NULL;
	}

	void ShowAll()
	{
		ListHead *p;
		ConfigSection *section;

		p = _sections.Prev;
		while (p != &_sections)
		{
			section = LIST_ENTRY(p, ConfigSection, list);
			printf("Section: %s\n", section->name);

			ListHead *p2 = section->entries.Prev;
			while (p2 != &section->entries)
			{
				ConfigEntry *entry = LIST_ENTRY(p2, ConfigEntry, list);
				printf("\t%s = ", entry->name);
				switch (entry->type)
				{
				case CONFIG_ENTRY_TYPE_REG_DWORD:
					printf("REG_DWORD %d\n", entry->numberValue);
					break;
				case CONFIG_ENTRY_TYPE_REG_SZ:
					printf("REG_SZ \"%s\"\n", entry->stringValue);
					break;
				case CONFIG_ENTRY_TYPE_REG_EXPAND_SZ:
					printf("REG_EXPAND_SZ \"%s\"\n", entry->stringValue);
					break;
				}
				p2 = p2->Prev;
			}

			p = p->Prev;
		}
	}
};

ConfigParser * settingsData;

void Settings::SettingsReadConfiguration()
{
	TCHAR strConfigFilePath[_MAX_PATH];
	ZeroMemory(strConfigFilePath, _MAX_PATH * 2);
	GetModuleFileName(GetModuleHandle(NULL), strConfigFilePath, _MAX_PATH);	
	strConfigFilePath[lstrlenW(strConfigFilePath) - lstrlenW(wcsrchr(strConfigFilePath, '\\'))] = '\0';
	lstrcatW(strConfigFilePath, TEXT("\\"));
	lstrcatW(strConfigFilePath, SETTINGS_FILE);

	HANDLE hFile = NULL;
	CHAR buffer[4086];
	BOOL result;
	DWORD count;

	hFile = CreateFileW(strConfigFilePath, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (INVALID_HANDLE_VALUE == hFile) 
		throw "Cannot open settings file";

	do
	{
		result = ReadFile(hFile, buffer, 4086 - 1, &count, NULL);

		if (result && count > 0)
		{
			buffer[count] = '\0';
			break;
		}
	}
	while (result);
	CloseHandle(hFile);
	
	settingsData = new ConfigParser(buffer);
	settingsData->Parse();
}

void Settings::EnumSection(const char *sectionName, bool (*enumerator)(ConfigSection *, ConfigEntry *))
{
	settingsData->EnumSection(sectionName, enumerator);
}

void Settings::EnumSections(bool (*enumerator)(ConfigSection *))
{
	settingsData->EnumSections(enumerator);
}

void Settings::DumpConfiguration()
{
	settingsData->ShowAll();
}

int Settings::GetSectionsCount()
{
	return settingsData->SectionsCount();
}

const char * Settings::GetSectionName(int offset)
{
	return settingsData->GetSectionName(offset);
}