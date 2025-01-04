// Scintilla source code edit control
/** @file LexErrorList.cxx
 ** Lexer for error lists. Used for the output pane in SciTE.
 **/
// Copyright 1998-2001 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may
// be distributed.

#include "ExtraLexers.h"

#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <string>

// clang-format off
#include "ILexer.h"
#include "Scintilla.h"

#include "InList.h"
#include "WordList.h"
#include "LexAccessor.h"
#include "Accessor.h"
#include "StyleContext.h"
#include "LexCharacterSet.h"
#include "LexerModule.h"
#include "PropSetSimple.h"
#include "LexerBase.h"
#include "LexerSimple.h"

// clang-format on

using namespace Lexilla;

namespace
{

class NativeAccessor : public AccessorInterface
{
public:
    NativeAccessor(Accessor& accessor)
        : m_accessor(accessor)
    {
    }
    const char operator[](size_t index) const override { return m_accessor[index]; }
    char SafeGetCharAt(size_t index, char chDefault = ' ') const override
    {
        return m_accessor.SafeGetCharAt(index, chDefault);
    }
    void ColourTo(size_t pos, int style) override { m_accessor.ColourTo(pos, style); }
    void StartAt(size_t start) override { m_accessor.StartAt(start); }
    void StartSegment(size_t pos) override { m_accessor.StartSegment(pos); }
    int GetPropertyInt(const std::string& name, int defaultVal = 0) const override
    {
        return m_accessor.GetPropertyInt(name, defaultVal);
    };

private:
    Accessor& m_accessor;
};

bool strstart(const char* haystack, const char* needle) noexcept
{
    return strncmp(haystack, needle, strlen(needle)) == 0;
}

constexpr bool Is0To9(char ch) noexcept { return (ch >= '0') && (ch <= '9'); }

constexpr bool Is1To9(char ch) noexcept { return (ch >= '1') && (ch <= '9'); }

bool AtEOL(AccessorInterface& styler, Sci_Position i)
{
    return (styler[i] == '\n') || ((styler[i] == '\r') && (styler.SafeGetCharAt(i + 1) != '\n'));
}

bool IsGccExcerpt(const char* s) noexcept
{
    while (*s) {
        if (s[0] == ' ' && s[1] == '|' && (s[2] == ' ' || s[2] == '+')) {
            return true;
        }
        if (!(s[0] == ' ' || s[0] == '+' || Is0To9(s[0]))) {
            return false;
        }
        s++;
    }
    return true;
}

const std::string bashDiagnosticMark = ": line ";
bool IsBashDiagnostic(std::string const& sv)
{
    const size_t mark = sv.find(bashDiagnosticMark);
    if (mark == std::string::npos) {
        return false;
    }
    std::string rest = sv.substr(mark + bashDiagnosticMark.length());
    if (rest.empty() || !Is0To9(rest.front())) {
        return false;
    }
    while (!rest.empty() && Is0To9(rest.front())) {
        rest = rest.substr(1);
    }
    return !rest.empty() && (rest.front() == ':');
}

int RecogniseErrorListLine(const char* lineBuffer, Sci_PositionU lengthLine, Sci_Position& startValue)
{
    if (lineBuffer[0] == '>') {
        // Command or return status
        return wxSTC_TERMINAL_CMD;
    } else if (lineBuffer[0] == '<') {
        // Diff removal.
        return wxSTC_TERMINAL_DIFF_DELETION;
    } else if (lineBuffer[0] == '!') {
        return wxSTC_TERMINAL_DIFF_CHANGED;
    } else if (lineBuffer[0] == '+') {
        if (strstart(lineBuffer, "+++ ")) {
            return wxSTC_TERMINAL_DIFF_MESSAGE;
        } else {
            return wxSTC_TERMINAL_DIFF_ADDITION;
        }
    } else if (lineBuffer[0] == '-' && !(strstart(lineBuffer, "-rw") || strstart(lineBuffer, "-r-"))) {
        if (strstart(lineBuffer, "--- ")) {
            return wxSTC_TERMINAL_DIFF_MESSAGE;
        } else if (strstart(lineBuffer, "-- ")) {
            // Probably a CMake status message
            return wxSTC_TERMINAL_DEFAULT;
        } else {
            return wxSTC_TERMINAL_DIFF_DELETION;
        }
    } else if (strstart(lineBuffer, "cf90-")) {
        // Absoft Pro Fortran 90/95 v8.2 error and/or warning message
        return wxSTC_TERMINAL_ABSF;
    } else if (strstart(lineBuffer, "fortcom:")) {
        // Intel Fortran Compiler v8.0 error/warning message
        return wxSTC_TERMINAL_IFORT;
    } else if (strstr(lineBuffer, "File \"") && strstr(lineBuffer, ", line ")) {
        return wxSTC_TERMINAL_PYTHON;
    } else if (strstr(lineBuffer, " in ") && strstr(lineBuffer, " on line ")) {
        return wxSTC_TERMINAL_PHP;
    } else if ((strstart(lineBuffer, "Error ") || strstart(lineBuffer, "Warning ")) && strstr(lineBuffer, " at (") &&
               strstr(lineBuffer, ") : ") && (strstr(lineBuffer, " at (") < strstr(lineBuffer, ") : "))) {
        // Intel Fortran Compiler error/warning message
        return wxSTC_TERMINAL_IFC;
    } else if (strstart(lineBuffer, "Error ")) {
        // Borland error message
        return wxSTC_TERMINAL_BORLAND;
    } else if (strstart(lineBuffer, "Warning ")) {
        // Borland warning message
        return wxSTC_TERMINAL_BORLAND;
    } else if (strstr(lineBuffer, "at line ") && (strstr(lineBuffer, "at line ") < (lineBuffer + lengthLine)) &&
               strstr(lineBuffer, "file ") && (strstr(lineBuffer, "file ") < (lineBuffer + lengthLine))) {
        // Lua 4 error message
        return wxSTC_TERMINAL_LUA;
    } else if (strstr(lineBuffer, " at ") && (strstr(lineBuffer, " at ") < (lineBuffer + lengthLine)) &&
               strstr(lineBuffer, " line ") && (strstr(lineBuffer, " line ") < (lineBuffer + lengthLine)) &&
               (strstr(lineBuffer, " at ") + 4 < (strstr(lineBuffer, " line ")))) {
        // perl error message:
        // <message> at <file> line <line>
        return wxSTC_TERMINAL_PERL;
    } else if ((lengthLine >= 6) && (memcmp(lineBuffer, "   at ", 6) == 0) && strstr(lineBuffer, ":line ")) {
        // A .NET traceback
        return wxSTC_TERMINAL_NET;
    } else if (strstart(lineBuffer, "Line ") && strstr(lineBuffer, ", file ")) {
        // Essential Lahey Fortran error message
        return wxSTC_TERMINAL_ELF;
    } else if (strstart(lineBuffer, "line ") && strstr(lineBuffer, " column ")) {
        // HTML tidy style: line 42 column 1
        return wxSTC_TERMINAL_TIDY;
    } else if (strstart(lineBuffer, "\tat ") && strchr(lineBuffer, '(') && strstr(lineBuffer, ".java:")) {
        // Java stack back trace
        return wxSTC_TERMINAL_JAVA_STACK;
    } else if (strstart(lineBuffer, "In file included from ") || strstart(lineBuffer, "                 from ")) {
        // GCC showing include path to following error
        return wxSTC_TERMINAL_GCC_INCLUDED_FROM;
    } else if (strstart(lineBuffer, "NMAKE : fatal error")) {
        // Microsoft nmake fatal error:
        // NMAKE : fatal error <code>: <program> : return code <return>
        return wxSTC_TERMINAL_MS;
    } else if (strstr(lineBuffer, "warning LNK") || strstr(lineBuffer, "error LNK")) {
        // Microsoft linker warning:
        // {<object> : } (warning|error) LNK9999
        return wxSTC_TERMINAL_MS;
    } else if (IsBashDiagnostic(lineBuffer)) {
        // Bash diagnostic
        // <filename>: line <line>:<message>
        return wxSTC_TERMINAL_BASH;
    } else if (IsGccExcerpt(lineBuffer)) {
        // GCC code excerpt and pointer to issue
        //    73 |   GTimeVal last_popdown;
        //       |            ^~~~~~~~~~~~
        return wxSTC_TERMINAL_GCC_EXCERPT;
    } else {
        // Look for one of the following formats:
        // GCC: <filename>:<line>:<message>
        // Microsoft: <filename>(<line>) :<message>
        // Common: <filename>(<line>): warning|error|note|remark|catastrophic|fatal
        // Common: <filename>(<line>) warning|error|note|remark|catastrophic|fatal
        // Microsoft: <filename>(<line>,<column>)<message>
        // CTags: <identifier>\t<filename>\t<message>
        // Lua 5 traceback: \t<filename>:<line>:<message>
        // Lua 5.1: <exe>: <filename>:<line>:<message>
        const bool initialTab = (lineBuffer[0] == '\t');
        bool initialColonPart = false;
        bool canBeCtags = !initialTab; // For ctags must have an identifier with no
                                       // spaces then a tab
        enum {
            stInitial,
            stGccStart,
            stGccDigit,
            stGccColumn,
            stGcc,
            stMsStart,
            stMsDigit,
            stMsBracket,
            stMsVc,
            stMsDigitComma,
            stMsDotNet,
            stCtagsStart,
            stCtagsFile,
            stCtagsStartString,
            stCtagsStringDollar,
            stCtags,
            stUnrecognized
        } state = stInitial;
        for (Sci_PositionU i = 0; i < lengthLine; i++) {
            const char ch = lineBuffer[i];
            char chNext = ' ';
            if ((i + 1) < lengthLine)
                chNext = lineBuffer[i + 1];
            if (state == stInitial) {
                if (ch == ':') {
                    // May be GCC, or might be Lua 5 (Lua traceback same but with tab
                    // prefix)
                    if ((chNext != '\\') && (chNext != '/') && (chNext != ' ')) {
                        // This check is not completely accurate as may be on
                        // GTK+ with a file name that includes ':'.
                        state = stGccStart;
                    } else if (chNext == ' ') { // indicates a Lua 5.1 error message
                        initialColonPart = true;
                    }
                } else if ((ch == '(') && Is1To9(chNext) && (!initialTab)) {
                    // May be Microsoft
                    // Check against '0' often removes phone numbers
                    state = stMsStart;
                } else if ((ch == '\t') && canBeCtags) {
                    // May be CTags
                    state = stCtagsStart;
                } else if (ch == ' ') {
                    canBeCtags = false;
                }
            } else if (state == stGccStart) { // <filename>:
                state = ((ch == '-') || Is0To9(ch)) ? stGccDigit : stUnrecognized;
            } else if (state == stGccDigit) { // <filename>:<line>
                if (ch == ':') {
                    state = stGccColumn; // :9.*: is GCC
                    startValue = i + 1;
                } else if (!Is0To9(ch)) {
                    state = stUnrecognized;
                }
            } else if (state == stGccColumn) { // <filename>:<line>:<column>
                if (!Is0To9(ch)) {
                    state = stGcc;
                    if (ch == ':')
                        startValue = i + 1;
                    break;
                }
            } else if (state == stMsStart) { // <filename>(
                state = Is0To9(ch) ? stMsDigit : stUnrecognized;
            } else if (state == stMsDigit) { // <filename>(<line>
                if (ch == ',') {
                    state = stMsDigitComma;
                } else if (ch == ')') {
                    state = stMsBracket;
                } else if ((ch != ' ') && !Is0To9(ch)) {
                    state = stUnrecognized;
                }
            } else if (state == stMsBracket) { // <filename>(<line>)
                if ((ch == ' ') && (chNext == ':')) {
                    state = stMsVc;
                } else if ((ch == ':' && chNext == ' ') || (ch == ' ')) {
                    // Possibly Delphi.. don't test against chNext as it's one of the
                    // strings below.
                    char word[512];
                    unsigned numstep = 0;
                    if (ch == ' ')
                        numstep = 1; // ch was ' ', handle as if it's a delphi errorline,
                                     // only add 1 to i.
                    else
                        numstep = 2; // otherwise add 2.
                    Sci_PositionU chPos = 0;
                    for (Sci_PositionU j = i + numstep;
                         j < lengthLine && IsUpperOrLowerCase(lineBuffer[j]) && chPos < sizeof(word) - 1; j++)
                        word[chPos++] = lineBuffer[j];
                    word[chPos] = 0;
                    if (InListCaseInsensitive(word,
                                              { "error", "warning", "fatal", "catastrophic", "note", "remark" })) {
                        state = stMsVc;
                    } else {
                        state = stUnrecognized;
                    }
                } else {
                    state = stUnrecognized;
                }
            } else if (state == stMsDigitComma) { // <filename>(<line>,
                if (ch == ')') {
                    state = stMsDotNet;
                    break;
                } else if ((ch != ' ') && !Is0To9(ch)) {
                    state = stUnrecognized;
                }
            } else if (state == stCtagsStart) {
                if (ch == '\t') {
                    state = stCtagsFile;
                }
            } else if (state == stCtagsFile) {
                if ((lineBuffer[i - 1] == '\t') && ((ch == '/' && chNext == '^') || Is0To9(ch))) {
                    state = stCtags;
                    break;
                } else if ((ch == '/') && (chNext == '^')) {
                    state = stCtagsStartString;
                }
            } else if ((state == stCtagsStartString) && ((lineBuffer[i] == '$') && (lineBuffer[i + 1] == '/'))) {
                state = stCtagsStringDollar;
                break;
            }
        }
        if (state == stGcc) {
            if (initialColonPart) {
                return wxSTC_TERMINAL_LUA;
            } else {
                if (strstr(lineBuffer, "warning:")) {
                    return wxSTC_TERMINAL_GCC_WARNING;
                } else if (strstr(lineBuffer, "note:")) {
                    return wxSTC_TERMINAL_GCC_NOTE;
                } else {
                    return wxSTC_TERMINAL_GCC;
                }
            }
        } else if ((state == stMsVc) || (state == stMsDotNet)) {
            return wxSTC_TERMINAL_MS;
        } else if ((state == stCtagsStringDollar) || (state == stCtags)) {
            return wxSTC_TERMINAL_CTAG;
        } else if (initialColonPart && strstr(lineBuffer, ": warning C")) {
            // Microsoft warning without line number
            // <filename>: warning C9999
            return wxSTC_TERMINAL_MS;
        } else {
            return wxSTC_TERMINAL_DEFAULT;
        }
    }
}

#define R(c) (((c) >> 16) & 0xff)
#define G(c) (((c) >> 8) & 0xff)
#define B(c) ((c) & 0xff)

uint32_t base_colours[16] = {
    0x000000, 0xcd0000, 0x00cd00, 0xcdcd00, 0x0000ee, 0xcd00cd, 0x00cdcd, 0xe5e5e5,
    0x7f7f7f, 0xff0000, 0x00ff00, 0xffff00, 0x5c5cff, 0xff00ff, 0x00ffff, 0xffffff,
};

int base_colour_to_style[16] = { wxSTC_TERMINAL_ES_BLACK,        wxSTC_TERMINAL_ES_RED,
                                 wxSTC_TERMINAL_ES_GREEN,        wxSTC_TERMINAL_ES_BROWN,
                                 wxSTC_TERMINAL_ES_BLUE,         wxSTC_TERMINAL_ES_MAGENTA,
                                 wxSTC_TERMINAL_ES_CYAN,         wxSTC_TERMINAL_ES_GRAY,
                                 wxSTC_TERMINAL_ES_DARK_GRAY,    wxSTC_TERMINAL_ES_BRIGHT_RED,
                                 wxSTC_TERMINAL_ES_BRIGHT_GREEN, wxSTC_TERMINAL_ES_YELLOW,
                                 wxSTC_TERMINAL_ES_BRIGHT_BLUE,  wxSTC_TERMINAL_ES_BRIGHT_MAGENTA,
                                 wxSTC_TERMINAL_ES_BRIGHT_CYAN,  wxSTC_TERMINAL_ES_WHITE };

// clang-format off
/// Returns sRGB colour corresponding to the index in the 256-colour ANSI palette
uint32_t rgb_from_ansi256(uint8_t index) {
    static uint32_t colours[256] = {
        /* The 16 system colours as used by default by xterm.  Taken
           from XTerm-col.ad distributed with xterm source code. */
        0x000000, 0xcd0000, 0x00cd00, 0xcdcd00,
        0x0000ee, 0xcd00cd, 0x00cdcd, 0xe5e5e5,
        0x7f7f7f, 0xff0000, 0x00ff00, 0xffff00,
        0x5c5cff, 0xff00ff, 0x00ffff, 0xffffff,

        /* 6×6×6 cube.  One each axis, the six indices map to [0, 95,
           135, 175, 215, 255] RGB component values. */
        0x000000, 0x00005f, 0x000087, 0x0000af,
        0x0000d7, 0x0000ff, 0x005f00, 0x005f5f,
        0x005f87, 0x005faf, 0x005fd7, 0x005fff,
        0x008700, 0x00875f, 0x008787, 0x0087af,
        0x0087d7, 0x0087ff, 0x00af00, 0x00af5f,
        0x00af87, 0x00afaf, 0x00afd7, 0x00afff,
        0x00d700, 0x00d75f, 0x00d787, 0x00d7af,
        0x00d7d7, 0x00d7ff, 0x00ff00, 0x00ff5f,
        0x00ff87, 0x00ffaf, 0x00ffd7, 0x00ffff,
        0x5f0000, 0x5f005f, 0x5f0087, 0x5f00af,
        0x5f00d7, 0x5f00ff, 0x5f5f00, 0x5f5f5f,
        0x5f5f87, 0x5f5faf, 0x5f5fd7, 0x5f5fff,
        0x5f8700, 0x5f875f, 0x5f8787, 0x5f87af,
        0x5f87d7, 0x5f87ff, 0x5faf00, 0x5faf5f,
        0x5faf87, 0x5fafaf, 0x5fafd7, 0x5fafff,
        0x5fd700, 0x5fd75f, 0x5fd787, 0x5fd7af,
        0x5fd7d7, 0x5fd7ff, 0x5fff00, 0x5fff5f,
        0x5fff87, 0x5fffaf, 0x5fffd7, 0x5fffff,
        0x870000, 0x87005f, 0x870087, 0x8700af,
        0x8700d7, 0x8700ff, 0x875f00, 0x875f5f,
        0x875f87, 0x875faf, 0x875fd7, 0x875fff,
        0x878700, 0x87875f, 0x878787, 0x8787af,
        0x8787d7, 0x8787ff, 0x87af00, 0x87af5f,
        0x87af87, 0x87afaf, 0x87afd7, 0x87afff,
        0x87d700, 0x87d75f, 0x87d787, 0x87d7af,
        0x87d7d7, 0x87d7ff, 0x87ff00, 0x87ff5f,
        0x87ff87, 0x87ffaf, 0x87ffd7, 0x87ffff,
        0xaf0000, 0xaf005f, 0xaf0087, 0xaf00af,
        0xaf00d7, 0xaf00ff, 0xaf5f00, 0xaf5f5f,
        0xaf5f87, 0xaf5faf, 0xaf5fd7, 0xaf5fff,
        0xaf8700, 0xaf875f, 0xaf8787, 0xaf87af,
        0xaf87d7, 0xaf87ff, 0xafaf00, 0xafaf5f,
        0xafaf87, 0xafafaf, 0xafafd7, 0xafafff,
        0xafd700, 0xafd75f, 0xafd787, 0xafd7af,
        0xafd7d7, 0xafd7ff, 0xafff00, 0xafff5f,
        0xafff87, 0xafffaf, 0xafffd7, 0xafffff,
        0xd70000, 0xd7005f, 0xd70087, 0xd700af,
        0xd700d7, 0xd700ff, 0xd75f00, 0xd75f5f,
        0xd75f87, 0xd75faf, 0xd75fd7, 0xd75fff,
        0xd78700, 0xd7875f, 0xd78787, 0xd787af,
        0xd787d7, 0xd787ff, 0xd7af00, 0xd7af5f,
        0xd7af87, 0xd7afaf, 0xd7afd7, 0xd7afff,
        0xd7d700, 0xd7d75f, 0xd7d787, 0xd7d7af,
        0xd7d7d7, 0xd7d7ff, 0xd7ff00, 0xd7ff5f,
        0xd7ff87, 0xd7ffaf, 0xd7ffd7, 0xd7ffff,
        0xff0000, 0xff005f, 0xff0087, 0xff00af,
        0xff00d7, 0xff00ff, 0xff5f00, 0xff5f5f,
        0xff5f87, 0xff5faf, 0xff5fd7, 0xff5fff,
        0xff8700, 0xff875f, 0xff8787, 0xff87af,
        0xff87d7, 0xff87ff, 0xffaf00, 0xffaf5f,
        0xffaf87, 0xffafaf, 0xffafd7, 0xffafff,
        0xffd700, 0xffd75f, 0xffd787, 0xffd7af,
        0xffd7d7, 0xffd7ff, 0xffff00, 0xffff5f,
        0xffff87, 0xffffaf, 0xffffd7, 0xffffff,

        /* Greyscale ramp.  This is calculated as (index - 232) * 10 + 8
           repeated for each RGB component. */
        0x080808, 0x121212, 0x1c1c1c, 0x262626,
        0x303030, 0x3a3a3a, 0x444444, 0x4e4e4e,
        0x585858, 0x626262, 0x6c6c6c, 0x767676,
        0x808080, 0x8a8a8a, 0x949494, 0x9e9e9e,
        0xa8a8a8, 0xb2b2b2, 0xbcbcbc, 0xc6c6c6,
        0xd0d0d0, 0xdadada, 0xe4e4e4, 0xeeeeee,
    };

    return colours[index];
}

// clang-format on

/// Calculates distance between two colours.  Tries to balance speed and
/// perceptual correctness.  It’s not a proper metric but two properties this
/// function provides are: d(x, x) = 0 and d(x, y) < d(x, z) implies x being
/// closer to y than to z.
uint32_t distance(uint32_t x, uint32_t y)
{
    /* See <https://www.compuphase.com/cmetric.htm> though we’re doing a few
       things to avoid some of the calculations.  We can do that since we
       only care about some properties of the metric. */
    int32_t r_sum = R(x) + R(y);
    int32_t r = (int32_t)R(x) - (int32_t)R(y);
    int32_t g = (int32_t)G(x) - (int32_t)G(y);
    int32_t b = (int32_t)B(x) - (int32_t)B(y);
    return (1024 + r_sum) * r * r + 2048 * g * g + (1534 - r_sum) * b * b;
}

/// Convert 256 colour index to style
int style_from_colour_number(uint8_t number)
{
    if (number >= 30 && number <= 37) {
        return base_colour_to_style[number - 30]; // normal colours are starting from 0
    } else if (number >= 90 && number <= 97) {
        return base_colour_to_style[number - 90 + 8]; // bright colours are starting from pos 8
    }

    // else
    auto encoded = rgb_from_ansi256(number);
    uint32_t dist = (uint32_t)-1;
    size_t index = 0;
    for (size_t i = 0; i < 16; ++i) {
        auto base_colour = base_colours[i];
        auto curdist = distance(encoded, base_colour);
        if (curdist < dist) {
            dist = curdist;
            index = i;
        }
    }
    return base_colour_to_style[index];
}

#define CSI "\033["
#define CSI_LEN 2

#define ESC "\033"
#define ESC_LEN 1

constexpr bool SequenceEnd(int ch) noexcept { return (ch == 0) || ((ch >= '@') && (ch <= '~')); }
constexpr bool IsSeparator(int ch) noexcept { return (ch == ';' || ch == ':'); }

enum class TokenType {
    Eof,
    Number,
    Separator,
};

static TokenType ReadNext(const char* seq, size_t& number, size_t& consumed)
{
    enum StateInternal {
        Start,
        Digit,
    };

    number = 0;
    StateInternal state = StateInternal::Start;
    bool cont = true;
    const char* start = seq;
    while (cont && !SequenceEnd(*seq)) {
        switch (state) {
        case StateInternal::Start:
            if (Is0To9(*seq)) {
                state = StateInternal::Digit;
                ++seq;
            } else if (IsSeparator(*seq)) {
                consumed = 1;
                return TokenType::Separator;
            } else {
                consumed = 1;
                return TokenType::Eof;
            }
            break;
        case StateInternal::Digit:
            if (Is0To9(*seq)) {
                ++seq;
            } else {
                cont = false;
            }
            break;
        }
    }

    if (state == StateInternal::Digit && start != seq) {
        std::string_view sv(start, seq - start);
        number = ::atoi(sv.data());
        consumed = sv.length();
        return TokenType::Number;
    }

    consumed = seq - start;
    return TokenType::Eof;
}

#define CHECK_EQ_OR_RETURN(a, b, retval) \
    if (a != b)                          \
        return retval;

int StyleFromSequence(const char* seq) noexcept
{
    const char* p = seq;
    size_t num, consumed;
    // Common
    TokenType t = ReadNext(p, num, consumed);
    if (t == TokenType::Number && (num >= 0 && num <= 9)) {
        p += consumed;
        // Read the next token - it should be a separator
        t = ReadNext(p, num, consumed);
        CHECK_EQ_OR_RETURN(t, TokenType::Separator, wxSTC_TERMINAL_DEFAULT);
        p += consumed;
        t = ReadNext(p, num, consumed);
    }

    if (t == TokenType::Number && num == 38) {
        // foreground colour in the format of: 38;5;<number>
        p += consumed;
        t = ReadNext(p, num, consumed);
        CHECK_EQ_OR_RETURN(t, TokenType::Separator, wxSTC_TERMINAL_DEFAULT);

        p += consumed;
        t = ReadNext(p, num, consumed);
        CHECK_EQ_OR_RETURN(t, TokenType::Number, wxSTC_TERMINAL_DEFAULT);
        CHECK_EQ_OR_RETURN(num, 5, wxSTC_TERMINAL_DEFAULT);

        p += consumed;
        t = ReadNext(p, num, consumed);
        CHECK_EQ_OR_RETURN(t, TokenType::Separator, wxSTC_TERMINAL_DEFAULT);

        p += consumed;
        t = ReadNext(p, num, consumed);
        CHECK_EQ_OR_RETURN(t, TokenType::Number, wxSTC_TERMINAL_DEFAULT);
        return style_from_colour_number((uint8_t)num);

    } else if (t == TokenType::Number && num == 48) {
        // background colour
        return wxSTC_TERMINAL_DEFAULT;
    } else if (t == TokenType::Number && num < 256) {
        // find the style from the colour table
        return style_from_colour_number((uint8_t)num);
    }
    return wxSTC_TERMINAL_DEFAULT;
}

#define NOT_FOUND std::string_view::npos
bool findOtherEscape(std::string_view sv, size_t& pos, size_t& len)
{
    auto where = sv.find(ESC);
    if (where == NOT_FOUND) {
        return false;
    }

    std::string_view sub = sv.substr(where + ESC_LEN); // skip the escape
    if (sub.find("(B") != NOT_FOUND || sub.find("(0") != NOT_FOUND || sub.find("(U") != NOT_FOUND ||
        sub.find("(K") != NOT_FOUND) {
        // found ESC(<B|0|U|K>
        pos += where;
        len = ESC_LEN + 2;
        return true;
    }
    return false;
}

void ColouriseErrorListLine(const std::string& lineBuffer, Sci_PositionU endPos, AccessorInterface& styler,
                            bool valueSeparate, bool escapeSequences)
{
    Sci_Position startValue = -1;
    const Sci_PositionU lengthLine = lineBuffer.length();
    const int style = RecogniseErrorListLine(lineBuffer.c_str(), lengthLine, startValue);
    if (escapeSequences && strstr(lineBuffer.c_str(), CSI)) {
        const Sci_Position startPos = endPos - lengthLine;
        const char* linePortion = lineBuffer.c_str();
        Sci_Position startPortion = startPos;
        int portionStyle = style;
        while (const char* startSeq = strstr(linePortion, CSI)) {
            if (startSeq > linePortion) {
                std::string_view prefix{ linePortion, static_cast<size_t>(startSeq - linePortion) };
                size_t pos = startPortion;
                while (!prefix.empty()) {
                    size_t otherEscPos = 0;
                    size_t otherEscLen = 0;
                    if (findOtherEscape(prefix, otherEscPos, otherEscLen)) {
                        if (otherEscPos != 0) {
                            styler.ColourTo(pos + otherEscPos, portionStyle);
                            pos += otherEscPos;
                        }
                        styler.ColourTo(pos + otherEscLen, wxSTC_TERMINAL_ESCSEQ_UNKNOWN);
                        pos += otherEscLen;
                        prefix = prefix.substr(otherEscPos + otherEscLen);
                    } else {
                        styler.ColourTo(pos + (startSeq - linePortion), portionStyle);
                        break;
                    }
                }
            }
            const char* endSeq = startSeq + CSI_LEN;
            while (!SequenceEnd(*endSeq))
                endSeq++;
            const Sci_Position endSeqPosition = startPortion + (endSeq - linePortion) + 1;
            switch (*endSeq) {
            case 0:
                styler.ColourTo(endPos, wxSTC_TERMINAL_ESCSEQ_UNKNOWN);
                return;
            case 'm': // Colour command
                styler.ColourTo(endSeqPosition, wxSTC_TERMINAL_ESCSEQ);
                portionStyle = StyleFromSequence(startSeq + CSI_LEN);
                break;
            case 'K': // Erase to end of line -> ignore
                styler.ColourTo(endSeqPosition, wxSTC_TERMINAL_ESCSEQ);
                break;
            default:
                styler.ColourTo(endSeqPosition, wxSTC_TERMINAL_ESCSEQ_UNKNOWN);
                portionStyle = style;
            }
            startPortion = endSeqPosition;
            linePortion = endSeq + 1;
        }
        styler.ColourTo(endPos, portionStyle);
    } else {
        if (valueSeparate && (startValue >= 0)) {
            styler.ColourTo(endPos - (lengthLine - startValue), style);
            styler.ColourTo(endPos, wxSTC_TERMINAL_VALUE);
        } else {
            styler.ColourTo(endPos, style);
        }
    }
}

void ColouriseTerminalDocInternal(Sci_PositionU startPos, Sci_Position length, int, WordList*[],
                                  AccessorInterface& styler)
{
    std::string lineBuffer;
    styler.StartAt(startPos);
    styler.StartSegment(startPos);

    // property lexer.errorlist.value.separate
    //	For lines in the output pane that are matches from Find in Files or
    // GCC-style 	diagnostics, style the path and line number separately from the
    // rest of the 	line with style 21 used for the rest of the line. 	This allows
    // matched text to be more easily distinguished from its location.
    const bool valueSeparate = styler.GetPropertyInt("lexer.terminal.value.separate", 0) != 0;

    // property lexer.errorlist.escape.sequences
    //	Set to 1 to interpret escape sequences.
    const bool escapeSequences = styler.GetPropertyInt("lexer.terminal.escape.sequences") != 0;

    for (Sci_PositionU i = startPos; i < startPos + length; i++) {
        lineBuffer.push_back(styler[i]);
        if (AtEOL(styler, i)) {
            // End of line met, colourise it
            ColouriseErrorListLine(lineBuffer, i, styler, valueSeparate, escapeSequences);
            lineBuffer.clear();
        }
    }
    if (!lineBuffer.empty()) { // Last line does not have ending characters
        ColouriseErrorListLine(lineBuffer, startPos + length - 1, styler, valueSeparate, escapeSequences);
    }
}

void ColouriseTerminalDoc(Sci_PositionU startPos, Sci_Position length, int, WordList*[], Accessor& styler)
{
    NativeAccessor accessor(styler);
    ColouriseTerminalDocInternal(startPos, length, 0, nullptr, accessor);
}

const char* const emptyWordListDesc[] = { nullptr };

} // namespace

// Our API for exporting the lexer
void* CreateExtraLexerTerminal()
{
    static LexerModule module(wxSTC_LEX_TERMINAL, ColouriseTerminalDoc, "terminal", nullptr, emptyWordListDesc);
    return (void*)new LexerSimple(&module);
}

void FreeExtraLexer(void* p)
{
    if (p) {
        delete reinterpret_cast<LexerSimple*>(p);
    }
}

/// Accessor API
void LexerTerminalStyle(size_t startPos, size_t length, AccessorInterface& styler)
{
    ColouriseTerminalDocInternal(startPos, length, 0, nullptr, styler);
}
