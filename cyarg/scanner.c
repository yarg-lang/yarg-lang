#include "scanner.h"

#include <string.h>

#include "common.h"
#include "object.h"
#include "memory.h"
#include "yargobject.h"

typedef struct ObjScanner {
    Obj obj;
    ObjString* source;
    const char* start;
    const char* current;
    int line;
} ObjScanner;

ObjScanner* newScanner(ObjString* source) {
    ObjScanner* scanner = ALLOCATE_OBJ(ObjScanner, OBJ_SCANNER);
    scanner->source = source;

    scanner->start = source->chars;
    scanner->current = scanner->start;
    scanner->line = 1;

    return scanner;
}

void markScanner(ObjScanner* scanner) {
    markObject((Obj*)scanner->source);
}

static bool isAlpha(char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
            c == '_';
}

static bool isBinDigit(char c) {
    return c >= '0' && c <= '1';
}

static bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

static bool isHexDigit(char c) {
    if (c >= 'a' && c <= 'f') {
        return true;
    } 
    else if (c >= 'A' && c <= 'F') {
        return true;
    }
    else {
        return isDigit(c);
    }
}

static bool isRadixDigit(char c, int radix) {
    switch (radix) {
        case 16: return isHexDigit(c);
        case 10: return isDigit(c);
        case 2: return isBinDigit(c);
        default: return false;
    }
}

static bool isRadix(char c) {
    switch (c) {
        case 'x': return true;
        case 'X': return true;
        case 'b': return true;
        case 'B': return true;
        default: return false;
    }
}

static int radixType(char c) {
    switch (c) {
        case 'x': return 16;
        case 'X': return 16;
        case 'b': return 2;
        case 'B': return 2;
        default: return 10;
    }
}

static bool isAtEnd(ObjScanner* scanner) {
    return *scanner->current == '\0';
}

static char advance(ObjScanner* scanner) {
    scanner->current++;
    return scanner->current[-1];
}

static char peek(ObjScanner* scanner) {
    return *scanner->current;
}

static char peekNext(ObjScanner* scanner) {
    if (isAtEnd(scanner)) return '\0';
    return scanner->current[1];
}

static bool match(ObjScanner* scanner, char expected) {
    if (isAtEnd(scanner)) return false;
    if (*scanner->current != expected) return false;
    scanner->current++;
    return true;
}

static Token makeToken(ObjScanner* scanner, TokenType type) {
    Token token;
    token.type = type;
    token.start = scanner->start;
    token.length = (int)(scanner->current - scanner->start);
    token.line = scanner->line;
    return token;
}

static Token errorToken(ObjScanner* scanner, const char* message) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = scanner->line;
    return token;
}

static void skipWhitespace(ObjScanner* scanner) {
    for (;;) {
        char c = peek(scanner);
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance(scanner);
                break;
            case '\n':
                scanner->line++;
                advance(scanner);
                break;
            case '/':
                if (peekNext(scanner) == '/') {
                    // A comment goes until the end of the line.
                    while (peek(scanner) != '\n' && !isAtEnd(scanner)) advance(scanner);
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

static TokenType checkKeyword(ObjScanner* scanner, int start, int length, const char* rest, TokenType type) {
    if (scanner->current - scanner->start == start + length &&
        memcmp(scanner->start + start, rest, length) == 0) {
        
        return type;
    }

    return TOKEN_IDENTIFIER;
}

static TokenType identifierType(ObjScanner* scanner) {
    switch (scanner->start[0]) {
        case 'a': 
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'n': 
                        if (scanner->current - scanner->start > 2) {
                                switch (scanner->start[2]) {
                                case 'd': return checkKeyword(scanner, 3, 0, "", TOKEN_AND);                                
                                case 'y': return checkKeyword(scanner, 3, 0, "", TOKEN_ANY);
                            }
                        }
                        break;
                }
            }
            break; 
        case 'b':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'o': return checkKeyword(scanner, 2, 2, "ol", TOKEN_BOOL);
                    case 'r': return checkKeyword(scanner, 2, 3, "eak", TOKEN_BREAK);
                }
            }
            break;
        case 'c':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'l': return checkKeyword(scanner, 2, 3, "ass", TOKEN_CLASS);
                    case 'o': 
                        if (scanner->current - scanner->start > 2) {
                            switch (scanner->start[2]) {
                                case 'm': return checkKeyword(scanner, 3, 4, "pile", TOKEN_COMPILE);
                                case 'n':
                                    if (scanner->current - scanner->start > 3) {
                                        switch (scanner->start[3]) {
                                            case 's': return checkKeyword(scanner, 4, 1, "t", TOKEN_CONST);
                                            case 't': return checkKeyword(scanner, 4, 4, "inue", TOKEN_CONTINUE);
                                        }
                                    }
                                    break;
                            }
                        }
                    case 'p': return checkKeyword(scanner, 2, 3, "eek", TOKEN_CPEEK);
                }
            }
            break;
        case 'e': 
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'l': return checkKeyword(scanner, 2, 2, "se", TOKEN_ELSE);
                }
            }
            break;
        case 'f':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'a': return checkKeyword(scanner, 2, 3, "lse", TOKEN_FALSE);
                    case 'o': return checkKeyword(scanner, 2, 1, "r", TOKEN_FOR);
                    case 'u': return checkKeyword(scanner, 2, 1, "n", TOKEN_FUN);
                }
            }
            break;
        case 'i':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'f': return checkKeyword(scanner, 2, 0, "", TOKEN_IF);
                    case 'n':
                        if (scanner->current - scanner->start > 2) {
                            switch (scanner->start[2]) {
                            case 't':
                                if (scanner->current - scanner->start > 3) {
                                    switch (scanner->start[3]) {
                                    case '1': return checkKeyword(scanner, 4, 1, "6", TOKEN_INT16);
                                    case '3': return checkKeyword(scanner, 4, 1, "2", TOKEN_INT32);
                                    case '6': return checkKeyword(scanner, 4, 1, "4", TOKEN_INT64);
                                    case '8': return checkKeyword(scanner, 4, 0, "", TOKEN_INT8);
                                    }
                                }
                                else
                                {
                                    return checkKeyword(scanner, 3, 0, "", TOKEN_INT);
                                }
                                break;
                            }
                        }
                        break;
                    }
                }
            break;
        case 'l':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                case 'e': return checkKeyword(scanner, 2, 1, "n", TOKEN_LEN);
                case 'o': return checkKeyword(scanner, 2, 2, "ad", TOKEN_LOAD);
                }
            }
            break;
        case 'm':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'a': {
                        if (   scanner->current - scanner->start > 5
                            && memcmp(scanner->start + 2, "ke_", 3) == 0) {
                            switch (scanner->start[5]) {
                                case 'c': return checkKeyword(scanner, 6, 6, "hannel", TOKEN_MAKE_CHANNEL);
                                case 'r': return checkKeyword(scanner, 6, 6, "outine", TOKEN_MAKE_ROUTINE);
                                case 's': return checkKeyword(scanner, 6, 9, "ync_group", TOKEN_MAKE_SYNCGROUP);
                            }
                        }
                        break;
                    }
                    case 'f': return checkKeyword(scanner, 2, 6, "loat64", TOKEN_MACHINE_FLOAT64);
                }
            }
            break;
        case 'n': 
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'e': return checkKeyword(scanner, 2, 1, "w", TOKEN_NEW);
                    case 'i': return checkKeyword(scanner, 2, 1, "l", TOKEN_NIL);
                }
            }
            break;       
        case 'o': return checkKeyword(scanner, 1, 1, "r", TOKEN_OR);
        case 'p': 
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'e': return checkKeyword(scanner, 2, 2, "ek", TOKEN_PEEK);
                    case 'i': return checkKeyword(scanner, 2, 1, "n", TOKEN_PIN);
                    case 'l': return checkKeyword(scanner, 2, 3, "ace", TOKEN_PLACE);
                    case 'o': return checkKeyword(scanner, 2, 2, "ke", TOKEN_POKE);
                    case 'r': return checkKeyword(scanner, 2, 3, "int", TOKEN_PRINT); 
                }
            }
            break;
        case 'r': 
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'e': 
                        if (scanner->current - scanner->start > 2) {
                            switch (scanner->start[2]) {
                                case 'a': return checkKeyword(scanner, 3, 13, "d_yarg_source", TOKEN_READ_YARG_SOURCE);
                                case 'c': return checkKeyword(scanner, 3, 4, "eive", TOKEN_RECEIVE);                                
                                case 's': return checkKeyword(scanner, 3, 3, "ume", TOKEN_RESUME);
                                case 't': return checkKeyword(scanner, 3, 3, "urn", TOKEN_RETURN);
                            }
                        }
                        break;
                }
            }
            break;
        case 's':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'e': 
                        if (scanner->current - scanner->start > 2) {
                            switch (scanner->start[2]) {
                                case 'n': return checkKeyword(scanner, 3, 1, "d", TOKEN_SEND);
                            }
                        }
                        break;
                    case 'h': return checkKeyword(scanner, 2, 3, "are", TOKEN_SHARE);
                    case 't':
                        if (scanner->current - scanner->start > 2) {
                            switch (scanner->start[2]) {
                                case 'a': return checkKeyword(scanner, 3, 2, "rt", TOKEN_START);                                
                                case 'r': 
                                    if (scanner->current - scanner->start > 3) {
                                        switch (scanner->start[3]) {
                                            case 'i': return checkKeyword(scanner, 4, 2, "ng", TOKEN_TYPE_STRING);
                                            case 'u': return checkKeyword(scanner, 4, 2, "ct", TOKEN_STRUCT);
                                        }
                                    }
                                    break;
                            }
                        }
                        break;
                    case 'u': return checkKeyword(scanner, 2, 3, "per", TOKEN_SUPER);
                }
            }
            break;
        case 't':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'e':
                        if (scanner->current - scanner->start > 7
                            && memcmp(scanner->start + 2, "st_", 3) == 0) {
                            switch (scanner->start[5]) {
                                case 'i': return checkKeyword(scanner, 6, 8, "nterrupt", TOKEN_TS_INTERRUPT);
                                case 'r': return checkKeyword(scanner, 6, 3, "ead", TOKEN_TS_READ);
                                case 's':
                                    switch (scanner->start[6]) {
                                        case 'e': return checkKeyword(scanner, 7, 1, "t", TOKEN_TS_SET);
                                        case 'y': return checkKeyword(scanner, 7, 2, "nc", TOKEN_TS_SYNC);
                                    }
                                    break;
                                case 'w': return checkKeyword(scanner, 6, 4, "rite", TOKEN_TS_WRITE);
                            }
                        }
                    case 'h': return checkKeyword(scanner, 2, 2, "is", TOKEN_THIS);
                    case 'r': return checkKeyword(scanner, 2, 2, "ue", TOKEN_TRUE);
                }
            }
            break;
        case 'u': 
            if (   scanner->current - scanner->start > 4
                && memcmp(scanner->start + 1, "int", 3) == 0) {
                switch (scanner->start[4]) {
                    case '1': return checkKeyword(scanner, 5, 1, "6", TOKEN_UINT16);
                    case '3': return checkKeyword(scanner, 5, 1, "2", TOKEN_UINT32);
                    case '6': return checkKeyword(scanner, 5, 1, "4", TOKEN_UINT64);
                    case '8': return checkKeyword(scanner, 5, 0, "", TOKEN_UINT8);
                }
            }
            break;
        case 'v': 
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'a': return checkKeyword(scanner, 2, 1, "r", TOKEN_VAR);
                    case 'm': return checkKeyword(scanner, 2, 14, "_read_xip_file", TOKEN_VM_READ_XIP_FILE);
                }
            }
            break;
        case 'w': return checkKeyword(scanner, 1, 4, "hile", TOKEN_WHILE);
        case 'y': return checkKeyword(scanner, 1, 4, "ield", TOKEN_YIELD);
    }

    return TOKEN_IDENTIFIER;
}

static Token identifier(ObjScanner* scanner) {
    while (isAlpha(peek(scanner)) || isDigit(peek(scanner))) advance(scanner);
    return makeToken(scanner, identifierType(scanner));
}

static Token number(ObjScanner* scanner) {
    int radix = 10;
    if (isRadix(peek(scanner))) {
        radix = radixType(peek(scanner));
        advance(scanner);

        if (!isRadixDigit(peek(scanner), radix)) {
            return errorToken(scanner, "Expected digit after radix specifier.");
        }
    }

    while (isRadixDigit(peek(scanner), radix)) advance(scanner);

    // Look for a fractional part.
    if (radix == 10 && peek(scanner) == '.' && isDigit(peekNext(scanner))) {
        // Consume the ".".
        advance(scanner);
    }

    while (isRadixDigit(peek(scanner), radix)) advance(scanner);

    // Look for an exponent.
    if (radix == 10 &&
        (peek(scanner) == 'e' || peek(scanner) == 'E') && (peekNext(scanner) == '-' || isDigit(peekNext(scanner)))) {
        // Consume the "e".
        advance(scanner);

        if (peek(scanner) == '-') {
            advance(scanner);
        }
    }

    while (isRadixDigit(peek(scanner), radix)) advance(scanner);

    return makeToken(scanner, TOKEN_NUMBER);
}

static Token address(ObjScanner* scanner) {
    advance(scanner); // the 'x' or 'X'
    while (isRadixDigit(peek(scanner), 16)) advance(scanner);

    return makeToken(scanner, TOKEN_ADDRESS); 
}

static Token string(ObjScanner* scanner) {
    while (peek(scanner) != '"' && !isAtEnd(scanner)) {
        if (peek(scanner) == '\\')
        {
            advance(scanner);
        }
        if (peek(scanner) == '\n') scanner->line++;
        advance(scanner);
    }

    if (isAtEnd(scanner)) return errorToken(scanner, "Unterminated string.");

    // The closing quote.
    advance(scanner);
    return makeToken(scanner, TOKEN_STRING);
}

Token scanToken(ObjScanner* scanner) {
    skipWhitespace(scanner);
    scanner->start = scanner->current;

    if (isAtEnd(scanner)) return makeToken(scanner, TOKEN_EOF);

    char c = advance(scanner);
    if (isAlpha(c)) return identifier(scanner);
    if (isDigit(c)) return number(scanner);

    switch (c) {
        case '(': return makeToken(scanner, TOKEN_LEFT_PAREN);
        case ')': return makeToken(scanner, TOKEN_RIGHT_PAREN);
        case '{': return makeToken(scanner, TOKEN_LEFT_BRACE);
        case '}': return makeToken(scanner, TOKEN_RIGHT_BRACE);
        case '[': return makeToken(scanner, TOKEN_LEFT_SQUARE_BRACKET);
        case ']': return makeToken(scanner, TOKEN_RIGHT_SQUARE_BRACKET);
        case ';': return makeToken(scanner, TOKEN_SEMICOLON);
        case ',': return makeToken(scanner, TOKEN_COMMA);
        case '.': return makeToken(scanner, TOKEN_DOT);
        case '-': return makeToken(scanner, TOKEN_MINUS);
        case '+': return makeToken(scanner, TOKEN_PLUS);
        case '/': return makeToken(scanner, TOKEN_SLASH);
        case '*': return makeToken(scanner, TOKEN_STAR);
        case '|': return makeToken(scanner, TOKEN_BAR);
        case '&': return makeToken(scanner, TOKEN_AMP);
        case '^': return makeToken(scanner, TOKEN_CARET);
        case '%': return makeToken(scanner, TOKEN_PERCENT);
        case ':': return makeToken(scanner, TOKEN_COLON);
        case '@': {
            if (peek(scanner) == 'x' || peek(scanner) == 'X') {
                return address(scanner);
            } else {
                return makeToken(scanner, TOKEN_AT);
            }
        }
        case '!':
            return makeToken(scanner, match(scanner, '=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '=':
            return makeToken(scanner, match(scanner, '=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '<': {
            switch (peek(scanner)) {
                case '<': advance(scanner); return makeToken(scanner, TOKEN_LEFT_SHIFT);
                case '=': advance(scanner); return makeToken(scanner, TOKEN_LESS_EQUAL);
                default: return makeToken(scanner, TOKEN_LESS);
            }
        }
        case '>':
        switch (peek(scanner)) {
            case '>': advance(scanner); return makeToken(scanner, TOKEN_RIGHT_SHIFT);
            case '=': advance(scanner); return makeToken(scanner, TOKEN_GREATER_EQUAL);
            default: return makeToken(scanner, TOKEN_GREATER);
        }
        case '"': return string(scanner);
    }

    return errorToken(scanner, "Unexpected character.");
}
