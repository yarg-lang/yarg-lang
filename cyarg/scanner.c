#include <stdio.h>
#include <string.h>

#include "common.h"
#include "scanner.h"

typedef struct {
    const char* start;
    const char* current;
    int line;
} Scanner;

Scanner scanner;

void initScanner(const char* source) {
    scanner.start = source;
    scanner.current = source;
    scanner.line = 1;
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
        case 'd': return true;
        case 'D': return true;
        default: return false;
    }
}

static int radixType(char c) {
    switch (c) {
        case 'x': return 16;
        case 'X': return 16;
        case 'b': return 2;
        case 'B': return 2;
        case 'd': return 10;
        case 'D': return 10;
        default: return 10;
    }
}

static bool isAtEnd() {
    return *scanner.current == '\0';
}

static char advance() {
    scanner.current++;
    return scanner.current[-1];
}

static char peek() {
    return *scanner.current;
}

static char peekNext() {
    if (isAtEnd()) return '\0';
    return scanner.current[1];
}

static bool match(char expected) {
    if (isAtEnd()) return false;
    if (*scanner.current != expected) return false;
    scanner.current++;
    return true;
}

static Token makeToken(TokenType type) {
    Token token;
    token.type = type;
    token.start = scanner.start;
    token.length = (int)(scanner.current - scanner.start);
    token.line = scanner.line;
    return token;
}

static Token errorToken(const char* message) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = scanner.line;
    return token;
}

static void skipWhitespace() {
    for (;;) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance();
                break;
            case '\n':
                scanner.line++;
                advance();
                break;
            case '/':
                if (peekNext() == '/') {
                    // A comment goes until the end of the line.
                    while (peek() != '\n' && !isAtEnd()) advance();
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

static TokenType checkKeyword(int start, int length, const char* rest, TokenType type) {
    if (scanner.current - scanner.start == start + length &&
        memcmp(scanner.start + start, rest, length) == 0) {
        
        return type;
    }

    return TOKEN_IDENTIFIER;
}

static TokenType identifierType() {
    switch (scanner.start[0]) {
        case 'a': 
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'n': 
                        if (scanner.current - scanner.start > 2) {
                                switch (scanner.start[2]) {
                                case 'd': return TOKEN_AND;                                
                                case 'y': return TOKEN_ANY;
                            }
                        }
                        break;
                }
            }
            break; 
        case 'b':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'o': return checkKeyword(2, 2, "ol", TOKEN_BOOL);
                }
            }
            break;
        case 'c':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'l': return checkKeyword(2, 3, "ass", TOKEN_CLASS);
                    case 'o': return checkKeyword(2, 3, "nst", TOKEN_CONST);
                    case 'p': return checkKeyword(2, 3, "eek", TOKEN_CPEEK);
                }
            }
            break;
        case 'e': return checkKeyword(1, 3, "lse", TOKEN_ELSE);
        case 'f':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'a': return checkKeyword(2, 3, "lse", TOKEN_FALSE);
                    case 'o': return checkKeyword(2, 1, "r", TOKEN_FOR);
                    case 'u': return checkKeyword(2, 1, "n", TOKEN_FUN);
                }
            }
            break;
        case 'i':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'f': return TOKEN_IF;
                    case 'm': return checkKeyword(2, 4, "port", TOKEN_IMPORT);
                    case 'n': return checkKeyword(2, 5, "teger", TOKEN_INTEGER);
                }
            }
            break;
        case 'l': return checkKeyword(1, 2, "en", TOKEN_LEN);
    case 'm':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'a': {
                        if (   scanner.current - scanner.start > 5
                            && memcmp(scanner.start + 2, "ke_", 3) == 0) {
                            switch (scanner.start[5]) {
                                case 'c': return checkKeyword(6, 6, "hannel", TOKEN_MAKE_CHANNEL);
                                case 'r': return checkKeyword(6, 6, "outine", TOKEN_MAKE_ROUTINE);
                            }
                        }
                        break;
                    }
                    case 'u': return checkKeyword(2, 5, "int32", TOKEN_MACHINE_UINT32);
                    case 'f': return checkKeyword(2, 6, "loat64", TOKEN_MACHINE_FLOAT64);
                }
            }
            break;
        case 'n': 
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'e': return checkKeyword(2, 1, "w", TOKEN_NEW);
                    case 'i': return checkKeyword(2, 1, "l", TOKEN_NIL);
                }
            }
            break;       
        case 'o': return checkKeyword(1, 1, "r", TOKEN_OR);
        case 'p': 
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'e': return checkKeyword(2, 2, "ek", TOKEN_PEEK);
                    case 'i': return checkKeyword(2, 1, "n", TOKEN_PIN);
                    case 'l': return checkKeyword(2, 3, "ace", TOKEN_PLACE);
                    case 'o': return checkKeyword(2, 2, "ke", TOKEN_POKE);
                    case 'r': return checkKeyword(2, 3, "int", TOKEN_PRINT); 
                }
            }
            break;
        case 'r': 
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'e': 
                        if (scanner.current - scanner.start > 2) {
                            switch (scanner.start[2]) {
                                case 'c': return checkKeyword(3, 4, "eive", TOKEN_RECEIVE);                                
                                case 's': return checkKeyword(3, 3, "ume", TOKEN_RESUME);
                                case 't': return checkKeyword(3, 3, "urn", TOKEN_RETURN);
                            }
                        }
                        break;
                }
            }
            break;
        case 's':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'e': 
                        if (scanner.current - scanner.start > 2) {
                            switch (scanner.start[2]) {
                                case 'n': return checkKeyword(3, 1, "d", TOKEN_SEND);
                            }
                        }
                        break;
                    case 'h': return checkKeyword(2, 3, "are", TOKEN_SHARE);
                    case 't':
                        if (scanner.current - scanner.start > 2) {
                            switch (scanner.start[2]) {
                                case 'a': return checkKeyword(3, 2, "rt", TOKEN_START);                                
                                case 'r': 
                                    if (scanner.current - scanner.start > 3) {
                                        switch (scanner.start[3]) {
                                            case 'i': return checkKeyword(4, 2, "ng", TOKEN_TYPE_STRING);
                                            case 'u': return checkKeyword(4, 2, "ct", TOKEN_STRUCT);
                                        }
                                    }
                                    break;
                            }
                        }
                        break;
                    case 'u': return checkKeyword(2, 3, "per", TOKEN_SUPER);
                }
            }
            break;
        case 't':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'h': return checkKeyword(2, 2, "is", TOKEN_THIS);
                    case 'r': return checkKeyword(2, 2, "ue", TOKEN_TRUE);
                }
            }
            break;
        case 'v': return checkKeyword(1, 2, "ar", TOKEN_VAR);
        case 'w': return checkKeyword(1, 4, "hile", TOKEN_WHILE);
        case 'y': return checkKeyword(1, 4, "ield", TOKEN_YIELD);
    }

    return TOKEN_IDENTIFIER;
}

static Token identifier() {
    while (isAlpha(peek()) || isDigit(peek())) advance();
    return makeToken(identifierType());
}

static Token number() {
    int radix = 10;
    if (isRadix(peek())) {
        radix = radixType(peek());
        advance();

        if (!isRadixDigit(peek(), radix)) {
            return errorToken("Expected digit after radix specifier.");
        }
    }

    while (isRadixDigit(peek(), radix)) advance();

    // Look for a fractional part.
    if (radix == 10 && peek() == '.' && isDigit(peekNext())) {
        // Consume the ".".
        advance();
        
        while (isRadixDigit(peek(), radix)) advance();
    }

    return makeToken(TOKEN_NUMBER);
}

static Token address() {
    advance(); // the 'x' or 'X'
    while (isRadixDigit(peek(), 16)) advance();

    return makeToken(TOKEN_ADDRESS); 
}

static Token string() {
    while (peek() != '"' && !isAtEnd()) {
        if (peek() == '\n') scanner.line++;
        advance();
    }

    if (isAtEnd()) return errorToken("Unterminated string.");

    // The closing quote.
    advance();
    return makeToken(TOKEN_STRING);
}

Token scanToken() {
    skipWhitespace();
    scanner.start = scanner.current;

    if (isAtEnd()) return makeToken(TOKEN_EOF);

    char c = advance();
    if (isAlpha(c)) return identifier();
    if (isDigit(c)) return number();

    switch (c) {
        case '(': return makeToken(TOKEN_LEFT_PAREN);
        case ')': return makeToken(TOKEN_RIGHT_PAREN);
        case '{': return makeToken(TOKEN_LEFT_BRACE);
        case '}': return makeToken(TOKEN_RIGHT_BRACE);
        case '[': return makeToken(TOKEN_LEFT_SQUARE_BRACKET);
        case ']': return makeToken(TOKEN_RIGHT_SQUARE_BRACKET);
        case ';': return makeToken(TOKEN_SEMICOLON);
        case ',': return makeToken(TOKEN_COMMA);
        case '.': return makeToken(TOKEN_DOT);
        case '-': return makeToken(TOKEN_MINUS);
        case '+': return makeToken(TOKEN_PLUS);
        case '/': return makeToken(TOKEN_SLASH);
        case '*': return makeToken(TOKEN_STAR);
        case '|': return makeToken(TOKEN_BAR);
        case '&': return makeToken(TOKEN_AMP);
        case '^': return makeToken(TOKEN_CARET);
        case '%': return makeToken(TOKEN_PERCENT);
        case '@': {
            if (peek() == 'x' || peek() == 'X') {
                return address();
            } else {
                return makeToken(TOKEN_AT);
            }
        }
        case '!':
            return makeToken(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '=':
            return makeToken(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '<': {
            switch (peek()) {
                case '<': advance(); return makeToken(TOKEN_LEFT_SHIFT);
                case '=': advance(); return makeToken(TOKEN_LESS_EQUAL);
                default: return makeToken(TOKEN_LESS);
            }
        }
        case '>':
        switch (peek()) {
            case '>': advance(); return makeToken(TOKEN_RIGHT_SHIFT);
            case '=': advance(); return makeToken(TOKEN_GREATER_EQUAL);
            default: return makeToken(TOKEN_GREATER);
        }
        case '"': return string();
    }

    return errorToken("Unexpected character.");
}