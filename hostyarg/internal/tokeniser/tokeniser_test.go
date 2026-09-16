package tokeniser

import "testing"

type TokenTestCase struct {
	input    string
	expected []TokenInfo
}

var tokenTestCases = []TokenTestCase{
	{
		input: string([]byte{0xDD, 0xdd}),
		expected: []TokenInfo{
			{Type: TokenError, Value: "cursor: 0, line: 1, column: 1"},
		},
	},
	{
		input: "t",
		expected: []TokenInfo{
			{Type: TokenIdentifier, Value: "t"},
			{Type: TokenEOF, Value: ""},
		},
	},
	{
		input: "",
		expected: []TokenInfo{
			{Type: TokenEOF, Value: ""},
		},
	},
	{
		input: "t\r",
		expected: []TokenInfo{
			{Type: TokenIdentifier, Value: "t"},
			{Type: TokenEOF, Value: ""},
		},
	},
	{
		input: "t\n\n",
		expected: []TokenInfo{
			{Type: TokenIdentifier, Value: "t"},
			{Type: TokenNewLine, Value: "\n"},
			{Type: TokenEOF, Value: ""},
		},
	},
	{
		input: "test input string\n\rtest\r\n\u2028test\n",
		expected: []TokenInfo{
			{Type: TokenIdentifier, Value: "test"},
			{Type: TokenIdentifier, Value: "input"},
			{Type: TokenIdentifier, Value: "string"},
			{Type: TokenNewLine, Value: "\n\r"},
			{Type: TokenIdentifier, Value: "test"},
			{Type: TokenNewLine, Value: "\r\n"},
			{Type: TokenNewLine, Value: "\u2028"},
			{Type: TokenIdentifier, Value: "test"},
			{Type: TokenEOF, Value: ""},
		},
	},
	{
		input: "single line without newline",
		expected: []TokenInfo{
			{Type: TokenIdentifier, Value: "single"},
			{Type: TokenIdentifier, Value: "line"},
			{Type: TokenIdentifier, Value: "without"},
			{Type: TokenIdentifier, Value: "newline"},
			{Type: TokenEOF, Value: ""},
		},
	},
	{
		input: "three line\nwith several\nnewline\n",
		expected: []TokenInfo{
			{Type: TokenIdentifier, Value: "three"},
			{Type: TokenIdentifier, Value: "line"},
			{Type: TokenNewLine, Value: "\n"},
			{Type: TokenIdentifier, Value: "with"},
			{Type: TokenIdentifier, Value: "several"},
			{Type: TokenNewLine, Value: "\n"},
			{Type: TokenIdentifier, Value: "newline"},
			{Type: TokenEOF, Value: ""},
		},
	},
	{
		input: "three line\nwith several\nnew" + string([]byte{0xa0}) + "line\n",
		expected: []TokenInfo{
			{Type: TokenIdentifier, Value: "three"},
			{Type: TokenIdentifier, Value: "line"},
			{Type: TokenNewLine, Value: "\n"},
			{Type: TokenIdentifier, Value: "with"},
			{Type: TokenIdentifier, Value: "several"},
			{Type: TokenNewLine, Value: "\n"},
			{Type: TokenError, Value: "cursor: 27, line: 3, column: 4"},
		},
	},
	{
		input: "\n\n\r\r\n",
		expected: []TokenInfo{
			{Type: TokenNewLine, Value: "\n"},
			{Type: TokenNewLine, Value: "\n\r"},
			{Type: TokenNewLine, Value: "\r\n"},
			{Type: TokenEOF, Value: ""},
		},
	},
	{
		input: "\u000A\u000C\u000D\u000B\u0085\u2028\u2029test",
		expected: []TokenInfo{
			{Type: TokenNewLine, Value: "\n"},
			{Type: TokenNewLine, Value: "\u000C"},
			{Type: TokenNewLine, Value: "\r"},
			{Type: TokenNewLine, Value: "\v"},
			{Type: TokenNewLine, Value: "\u0085"},
			{Type: TokenNewLine, Value: "\u2028"},
			{Type: TokenNewLine, Value: "\u2029"},
			{Type: TokenIdentifier, Value: "test"},
			{Type: TokenEOF, Value: ""},
		},
	},
	{
		input: string([]byte{'h', 'e', 'l', 'l', 'o', '\r', '\n', 'w', 'o', 'r', 'l', 'd', 0xa0, 'y', 'a', 'r', 'g'}),
		expected: []TokenInfo{
			{Type: TokenIdentifier, Value: "hello"},
			{Type: TokenNewLine, Value: "\r\n"},
			{Type: TokenError, Value: "cursor: 12, line: 2, column: 6"},
		},
	},
	{
		input: "# this is a comment\nidentifier",
		expected: []TokenInfo{
			{Type: TokenComment, Value: "# this is a comment"},
			{Type: TokenNewLine, Value: "\n"},
			{Type: TokenIdentifier, Value: "identifier"},
			{Type: TokenEOF, Value: ""},
		},
	},
	{
		input: "\n#comment",
		expected: []TokenInfo{
			{Type: TokenNewLine, Value: "\n"},
			{Type: TokenComment, Value: "#comment"},
			{Type: TokenEOF, Value: ""},
		},
	},
	{
		input: "\nid#comment",
		expected: []TokenInfo{
			{Type: TokenNewLine, Value: "\n"},
			{Type: TokenIdentifier, Value: "id"},
			{Type: TokenComment, Value: "#comment"},
			{Type: TokenEOF, Value: ""},
		},
	},
	{
		input: "\"string\"",
		expected: []TokenInfo{
			{Type: TokenIdentifier, Value: "string"},
			{Type: TokenEOF, Value: ""},
		},
	},
	{
		input: "hello\n\"hello world\" yarg",
		expected: []TokenInfo{
			{Type: TokenIdentifier, Value: "hello"},
			{Type: TokenNewLine, Value: "\n"},
			{Type: TokenIdentifier, Value: "hello world"},
			{Type: TokenIdentifier, Value: "yarg"},
			{Type: TokenEOF, Value: ""},
		},
	},
	{
		input: "hello\nprint\"hello\nworld\"yarg",
		expected: []TokenInfo{
			{Type: TokenIdentifier, Value: "hello"},
			{Type: TokenNewLine, Value: "\n"},
			{Type: TokenIdentifier, Value: "print"},
			{Type: TokenIdentifier, Value: "hello\nworld"},
			{Type: TokenIdentifier, Value: "yarg"},
			{Type: TokenEOF, Value: ""},
		},
	},
	{
		input: "\"string\\\"symbol\"",
		expected: []TokenInfo{
			{Type: TokenIdentifier, Value: "string\"symbol"},
			{Type: TokenEOF, Value: ""},
		},
	},
}

func executeTestCase(t *testing.T, i int) {
	tokens, err := TokeniseString(tokenTestCases[i].input)
	if err != nil {
		t.Errorf("test case %d: tokeniseString failed: %v", i, err)
	}
	if len(tokens) != len(tokenTestCases[i].expected) {
		t.Errorf("test case %d: expected %d tokens, got %d", i, len(tokenTestCases[i].expected), len(tokens))
	}
	for j, token := range tokens {
		if token != tokenTestCases[i].expected[j] {
			t.Errorf("test case %d: expected token %v at index %d, got %v", i, tokenTestCases[i].expected[j], j, token)
		}
	}

}

func TestTokeniseString(t *testing.T) {
	for i := range tokenTestCases {
		executeTestCase(t, i)
	}
}

func TestACase(t *testing.T) {
	executeTestCase(t, 5)
}
