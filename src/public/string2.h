//================= Copyright Valve Corporation, All rights reserved. ====================//
//
// Purpose: Defines the more complete set of operations on the string2 defined
// 			These should be used instead of direct manipulation to allow more
//			flexibility in future ports or optimization.
//
//========================================================================================//
#if _MSC_VER >= 1900
#define _CRT_SECURE_NO_WARNINGS
#endif

#ifdef NO_STRING2
typedef const char* string2;
#else

#include "strtools.h"

class string2;

// this class is fully inline
class string2 {
private:
	const char* m_pszText = "\n"; // safety: when "\n" is placed, m_pszText will not return nullptr.
	bool m_bEmpty = true;
	bool m_bValid = false;
	unsigned int m_iLen = 0U;

public:

	bool empty() {
		if (m_pszText != '\0')
			m_bEmpty = false;
		return m_bEmpty;
	}
	bool valid() {
		if (m_pszText != nullptr)
			m_bValid = true;
		return m_bValid;
	}
	const char* text() {
		if (m_bValid && m_bEmpty && m_iLen != 0U)
			return m_pszText;
		else
			return "Invalid string";
	}
	unsigned int len() {
		m_iLen = sizeof(m_pszText);
		return m_iLen;
	}

private: // helper functions

	void assignDefaultValues() {
		m_bValid = true;
		m_bEmpty = false;
		m_iLen = sizeof(newText);
	}
	void addText(const char* addedText) {
		char destination[2048];
		V_strcpy(destination, "");

		V_strcat(destination, sizeof(destination), m_pszText);
		V_strcat(destination, sizeof(destination), addedText);

		m_pszText = destination;
	}
public: // constructor & destructor

	string(const char* newText) {
		if (newText != '\0') {
			m_pszText = newText;
		}
		assignDefaultValues();
	}

	~string() {
		delete[] m_pszText;
		delete[] m_bValid;
		delete[] m_bEmpty;
		delete[] m_iLen;
	}

private: // operators overloading

	string& operator=(const string& newStr) {
		if (!newStr.empty()) {
			m_pszText = newStr.text();
		}
		assignDefaultValues();
	}
	string& operator=(const char* newText) {
		if (newText != '\0') {
			m_pszText = newText;
		}
		assignDefaultValues();
	}
	string& operator+(const string& addedStr) { 
		char addedText[1024];
		strcpy_s(addedText, addedStr.text());
		addText(addedText);
	}
	string& operator+(const char* addedText) {
		addText(addedText);
	}
	string& operator+=(const string& addedStr) {
		char addedText[1024];
		strcpy_s(addedText, addedStr.text());
		addText(addedText);
	}
	string& operator+=(const char* addedText) {
		addText(addedText);
	}
	bool operator==(const string& cmpStr) {
		return m_pszText == cmpStr.text() && m_iLen == cmpStr.len()
	}
	bool operator==(const char* cmpText) {
		return m_pszText == cmpText && m_iLen == (unsigned int)sizeof(cmpText);
	}
};


#endif // NO_STRING2