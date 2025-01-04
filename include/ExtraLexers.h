#pragma once

#include <cstddef>
#include <string>
#define wxSTC_LEX_TERMINAL 200

#define wxSTC_TERMINAL_DEFAULT 0
#define wxSTC_TERMINAL_PYTHON 1
#define wxSTC_TERMINAL_GCC 2
#define wxSTC_TERMINAL_MS 3
#define wxSTC_TERMINAL_CMD 4
#define wxSTC_TERMINAL_BORLAND 5
#define wxSTC_TERMINAL_PERL 6
#define wxSTC_TERMINAL_NET 7
#define wxSTC_TERMINAL_LUA 8
#define wxSTC_TERMINAL_CTAG 9
#define wxSTC_TERMINAL_DIFF_CHANGED 10
#define wxSTC_TERMINAL_DIFF_ADDITION 11
#define wxSTC_TERMINAL_DIFF_DELETION 12
#define wxSTC_TERMINAL_DIFF_MESSAGE 13
#define wxSTC_TERMINAL_PHP 14
#define wxSTC_TERMINAL_ELF 15
#define wxSTC_TERMINAL_IFC 16
#define wxSTC_TERMINAL_IFORT 17
#define wxSTC_TERMINAL_ABSF 18
#define wxSTC_TERMINAL_TIDY 19
#define wxSTC_TERMINAL_JAVA_STACK 20
#define wxSTC_TERMINAL_VALUE 21
#define wxSTC_TERMINAL_GCC_INCLUDED_FROM 22
#define wxSTC_TERMINAL_ESCSEQ 23
#define wxSTC_TERMINAL_ESCSEQ_UNKNOWN 24
#define wxSTC_TERMINAL_GCC_EXCERPT 25
#define wxSTC_TERMINAL_BASH 26
#define wxSTC_TERMINAL_ES_BLACK 40
#define wxSTC_TERMINAL_ES_RED 41
#define wxSTC_TERMINAL_ES_GREEN 42
#define wxSTC_TERMINAL_ES_BROWN 43
#define wxSTC_TERMINAL_ES_BLUE 44
#define wxSTC_TERMINAL_ES_MAGENTA 45
#define wxSTC_TERMINAL_ES_CYAN 46
#define wxSTC_TERMINAL_ES_GRAY 47
#define wxSTC_TERMINAL_ES_DARK_GRAY 48
#define wxSTC_TERMINAL_ES_BRIGHT_RED 49
#define wxSTC_TERMINAL_ES_BRIGHT_GREEN 50
#define wxSTC_TERMINAL_ES_YELLOW 51
#define wxSTC_TERMINAL_ES_BRIGHT_BLUE 52
#define wxSTC_TERMINAL_ES_BRIGHT_MAGENTA 53
#define wxSTC_TERMINAL_ES_BRIGHT_CYAN 54
#define wxSTC_TERMINAL_ES_WHITE 55
#define wxSTC_TERMINAL_GCC_WARNING 56
#define wxSTC_TERMINAL_GCC_NOTE 57

class AccessorInterface
{
public:
    virtual const char operator[](size_t index) const = 0;
    virtual char SafeGetCharAt(size_t index, char chDefault = ' ') const = 0;
    virtual void ColourTo(size_t pos, int style) = 0;
    virtual void StartAt(size_t start) = 0;
    virtual void StartSegment(size_t pos) = 0;
    virtual int GetPropertyInt(const std::string& name, int defaultVal = 0) const = 0;
};

// API
void* CreateExtraLexerTerminal();
void FreeExtraLexer(void* lexer);
void LexerTerminalStyle(size_t startPos, size_t length, AccessorInterface& styler);
