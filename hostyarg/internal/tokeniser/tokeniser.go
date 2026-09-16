package tokeniser

import (
	"bufio"
	"fmt"
	"io"
	"os"
	"strconv"
	"strings"
)

type Token int

const (
	TokenPath Token = iota
	TokenComment
	TokenNode
	TokenFile
	TokenTextFile
	TokenIndexFile
	TokenBootfile
	TokenNewLine
	TokenIdentifier
	TokenEOF
	TokenError
)

func (t Token) String() string {
	switch t {
	case TokenPath:
		return "TokenPath"
	case TokenComment:
		return "TokenComment"
	case TokenNode:
		return "TokenNode"
	case TokenFile:
		return "TokenFile"
	case TokenTextFile:
		return "TokenTextFile"
	case TokenIndexFile:
		return "TokenIndexFile"
	case TokenBootfile:
		return "TokenBootfile"
	case TokenNewLine:
		return "TokenNewLine"
	case TokenIdentifier:
		return "TokenIdentifier"
	case TokenEOF:
		return "TokenEOF"
	case TokenError:
		return "TokenError"
	default:
		return "Unknown"
	}
}

type TokenInfo struct {
	Type  Token
	Value string
}

func (T TokenInfo) String() string {
	return fmt.Sprintf("Type: %v, Value: %s", T.Type, strconv.Quote(T.Value))
}

type State int

const (
	ReadNext State = iota
	ReadNewLine
	ReadNewLine2
	ReadRuneIdentifier
	AddRuneToIdentifier
	ReadComment
	AddRuneToComment
	ReadString
	AddRuneToString
	ReadEscape
	AddEscapeToString
	DispatchNewLine
	DispatchRune
	DispatchToken
	Error
	LastLine
	End
)

func (s State) String() string {
	switch s {
	case ReadNext:
		return "ReadNext"
	case ReadNewLine:
		return "ReadNewLine"
	case ReadNewLine2:
		return "ReadNewLine2"
	case ReadRuneIdentifier:
		return "ReadRuneIdentifier"
	case AddRuneToIdentifier:
		return "AddRuneToIdentifier"
	case DispatchNewLine:
		return "DispatchNewLine"
	case DispatchRune:
		return "DispatchRune"
	case DispatchToken:
		return "DispatchToken"
	case Error:
		return "Error"
	case LastLine:
		return "LastLine"
	case End:
		return "End"
	default:
		return "Unknown"
	}
}

func isNewLineComponent(r rune) bool {
	switch r {
	case '\u000A', '\u000D', '\u000C', '\u000B', '\u0085', '\u2028', '\u2029':
		return true
	default:
		return false
	}
}

func isWhitespace(r rune) bool {
	switch r {
	case ' ', '\t':
		return true
	default:
		return false
	}
}

func isCommentStart(r rune) bool {
	switch r {
	case '#':
		return true
	default:
		return false
	}
}

func isStringStart(r rune) bool {
	switch r {
	case '"':
		return true
	default:
		return false
	}
}

func isStringEnd(r rune) bool {
	switch r {
	case '"':
		return true
	default:
		return false
	}
}

func isEscape(r rune) bool {
	switch r {
	case '\\':
		return true
	default:
		return false
	}
}

// a tokeniser for utf-8 line-oriented command scripts.
// supports "strings" (with escape sequences) and #comments
func tokenise(rd io.Reader) []TokenInfo {

	scanner := bufio.NewReader(rd)

	cursor := 0
	current := ReadNext
	var r rune
	var lastRuneSize int

	readRune := func(next State, eof State) {
		rn, size, e := scanner.ReadRune()
		r = rn
		switch {
		case e != nil && e == io.EOF:
			current = eof
		case e != nil:
			current = Error
		case r == '\ufffd' && size == 1:
			current = Error
		default:
			cursor += size
			lastRuneSize = size
			current = next
		}
	}

	var token TokenInfo
	tokens := []TokenInfo{}
	line, column := 1, 1

	for {
		switch current {
		case ReadNext:
			readRune(DispatchRune, End)
		case DispatchRune:
			column += 1
			token.Value = string(r)
			switch {
			case isNewLineComponent(r):
				token.Type = TokenNewLine
				current = ReadNewLine
			case isWhitespace(r):
				current = ReadNext
			case isCommentStart(r):
				token.Type = TokenComment
				current = ReadComment
			case isStringStart(r):
				token.Type = TokenIdentifier
				token.Value = ""
				current = ReadString
			default:
				token.Type = TokenIdentifier
				current = ReadRuneIdentifier
			}
		case ReadNewLine:
			readRune(ReadNewLine2, End)
		case ReadNewLine2:
			switch {
			case r == '\r' && token.Value == string('\n'),
				r == '\n' && token.Value == string('\r'):
				token.Value += string(r)
				current = DispatchNewLine
			default:
				scanner.UnreadRune()
				cursor -= lastRuneSize
				current = DispatchNewLine
			}
		case ReadRuneIdentifier:
			readRune(AddRuneToIdentifier, DispatchToken)
		case AddRuneToIdentifier:
			switch {
			case isNewLineComponent(r), isWhitespace(r), isCommentStart(r), isStringStart(r):
				scanner.UnreadRune()
				cursor -= lastRuneSize
				current = DispatchToken
			default:
				column += 1
				token.Value += string(r)
				current = ReadRuneIdentifier
			}
		case ReadComment:
			readRune(AddRuneToComment, DispatchToken)
		case AddRuneToComment:
			switch {
			case isNewLineComponent(r):
				scanner.UnreadRune()
				cursor -= lastRuneSize
				current = DispatchToken
			default:
				column += 1
				token.Value += string(r)
				current = ReadComment
			}
		case ReadString:
			readRune(AddRuneToString, DispatchToken)
		case AddRuneToString:
			switch {
			case isStringEnd(r):
				cursor -= lastRuneSize
				current = DispatchToken
			case isEscape(r):
				column += 1
				current = ReadEscape
			default:
				column += 1
				token.Value += string(r)
				current = ReadString
			}
		case ReadEscape:
			readRune(AddEscapeToString, DispatchToken)
		case AddEscapeToString:
			column += 1
			switch {
			case r == '"':
				token.Value += string(r)
				current = ReadString
			default:
				current = Error
			}
		case DispatchNewLine:
			column = 1
			line += 1
			current = DispatchToken
		case DispatchToken:
			tokens = append(tokens, token)
			token = TokenInfo{}
			current = ReadNext
		case Error:
			token.Type = TokenError
			token.Value = fmt.Sprintf("cursor: %d, line: %d, column: %d", cursor, line, column)
			tokens = append(tokens, token)
			return tokens
		case End:
			tokens = append(tokens, TokenInfo{Type: TokenEOF, Value: ""})
			return tokens
		}
	}
}

func TokeniseFile(filePath string) ([]TokenInfo, error) {
	file, e := os.Open(filePath)
	if e != nil {
		return nil, e
	}
	defer file.Close()

	return tokenise(file), nil
}

func TokeniseString(input string) ([]TokenInfo, error) {
	reader := strings.NewReader(input)

	return tokenise(reader), nil
}
