// Scintilla source code edit control
/** @file LexErrorList.cxx
 ** Lexer for error lists. Used for the output pane in SciTE.
 **/
// Copyright 1998-2001 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may
// be distributed.

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
#include "ExtraLexers.h"

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

bool strstart(const char* haystack, const char* needle) noexcept
{
    return strncmp(haystack, needle, strlen(needle)) == 0;
}

constexpr bool Is0To9(char ch) noexcept { return (ch >= '0') && (ch <= '9'); }

constexpr bool Is1To9(char ch) noexcept { return (ch >= '1') && (ch <= '9'); }

bool AtEOL(Accessor& styler, Sci_Position i)
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

#define CSI "\033["

constexpr bool SequenceEnd(int ch) noexcept { return (ch == 0) || ((ch >= '@') && (ch <= '~')); }

int StyleFromSequence(const char* seq) noexcept
{
    int bold = 0;
    int colour = 0;
    while (!SequenceEnd(*seq)) {
        if (Is0To9(*seq)) {
            int base = *seq - '0';
            if (Is0To9(seq[1])) {
                base = base * 10;
                base += seq[1] - '0';
                seq++;
            }
            if (base == 0) {
                colour = 0;
                bold = 0;
            } else if (base == 1) {
                bold = 1;
            } else if (base >= 30 && base <= 37) {
                colour = base - 30;
            }
        }
        seq++;
    }
    return wxSTC_TERMINAL_ES_BLACK + bold * 8 + colour;
}

void ColouriseErrorListLine(const std::string& lineBuffer, Sci_PositionU endPos, Accessor& styler, bool valueSeparate,
                            bool escapeSequences)
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
                styler.ColourTo(startPortion + (startSeq - linePortion), portionStyle);
            }
            const char* endSeq = startSeq + 2;
            while (!SequenceEnd(*endSeq))
                endSeq++;
            const Sci_Position endSeqPosition = startPortion + (endSeq - linePortion) + 1;
            switch (*endSeq) {
            case 0:
                styler.ColourTo(endPos, wxSTC_TERMINAL_ESCSEQ_UNKNOWN);
                return;
            case 'm': // Colour command
                styler.ColourTo(endSeqPosition, wxSTC_TERMINAL_ESCSEQ);
                portionStyle = StyleFromSequence(startSeq + 2);
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

void ColouriseTerminalDoc(Sci_PositionU startPos, Sci_Position length, int, WordList*[], Accessor& styler)
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
