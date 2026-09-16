package command

import (
	"fmt"
	"os"
	"path/filepath"
	"strconv"

	"github.com/yarg-lang/yarg-lang/hostyarg/internal/tokeniser"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/xip/library"
)

type FileCommand struct {
	SourcePath string
	TargetPath string
	Length     int64
	Alignment  int
}

func (c *FileCommand) String() string {
	return fmt.Sprintf("file \"%s\" \"%s\" (%d|%d)", c.SourcePath, c.TargetPath, c.Length, c.Alignment)
}

func CanonicalSource(lib *library.XIPLibrary, sourcePath string) string {
	dir := filepath.Dir(lib.CommandPath)
	target := filepath.Join(dir, sourcePath)
	return filepath.Clean(target)
}

func DefaultAlignment(sourcePath string) int {
	if filepath.Ext(sourcePath) == ".yb" {
		return 8
	}
	return 1
}

func (c *FileCommand) Execute(lib *library.XIPLibrary) error {
	c.SourcePath = CanonicalSource(lib, c.SourcePath)
	if c.TargetPath == "" {
		c.TargetPath = filepath.Base(c.SourcePath)
	}

	info, err := os.Stat(c.SourcePath)
	if err != nil {
		return err
	}
	c.Length = info.Size()
	c.Alignment = DefaultAlignment(c.SourcePath)

	lib.NamedFiles = append(lib.NamedFiles, c)

	return nil
}

type TextFileCommand struct {
	FileCommand
}

func (c *TextFileCommand) String() string {
	return fmt.Sprintf("txtfile \"%s\" -> \"%s\" (%d|%d)", c.SourcePath, c.TargetPath, c.Length, c.Alignment)
}

func (c *TextFileCommand) Execute(lib *library.XIPLibrary) error {
	return c.FileCommand.Execute(lib)
}

type IndexFileCommand struct {
	SourcePath string
	Alignment  int
	Length     int64
	Index      uint16
}

func (c *IndexFileCommand) Execute(lib *library.XIPLibrary) error {
	c.SourcePath = CanonicalSource(lib, c.SourcePath)
	c.Alignment = DefaultAlignment(c.SourcePath)
	info, err := os.Stat(c.SourcePath)
	if err != nil {
		return err
	}
	c.Length = info.Size()

	lib.IndexedFiles = append(lib.IndexedFiles, c)
	return nil
}

func (c *IndexFileCommand) String() string {
	return fmt.Sprintf("indexfile \"%s\" %d (%d|%d)", c.SourcePath, c.Index, c.Length, c.Alignment)
}

type ErrorCommand struct {
	Message string
}

func (c *ErrorCommand) Execute(lib *library.XIPLibrary) error {
	return fmt.Errorf("%s", c.Message)
}

func (c *ErrorCommand) String() string {
	return fmt.Sprintf("error \"%s\"", c.Message)
}

func parseACommand(commandTokens []tokeniser.TokenInfo) library.Command {
	switch commandTokens[0].Type {
	case tokeniser.TokenIdentifier:
		switch commandTokens[0].Value {
		case "file", "txtfile":
			if len(commandTokens) < 2 {
				// Handle error: not enough arguments for file command
				return &ErrorCommand{Message: "not enough arguments for file command"}
			}
			fileSource := commandTokens[1].Value
			targetPath := ""
			if len(commandTokens) == 3 {
				targetPath = commandTokens[2].Value
			}
			fmt.Printf("file command: source=%s, target=%s\n", fileSource, targetPath)
			if commandTokens[0].Value == "txtfile" {
				return &TextFileCommand{FileCommand: FileCommand{SourcePath: fileSource, TargetPath: targetPath}}
			}
			return &FileCommand{SourcePath: fileSource, TargetPath: targetPath}
		case "bootfile":
			if len(commandTokens) < 2 {
				// Handle error: not enough arguments for bootfile command
				return &ErrorCommand{Message: "not enough arguments for bootfile command"}
			}
			bootSource := commandTokens[1].Value
			fmt.Printf("bootfile command: source=%s\n", bootSource)
			return &IndexFileCommand{SourcePath: bootSource, Index: 1}
		case "indexfile":
			if len(commandTokens) < 3 {
				// Handle error: not enough arguments for indexfile command
				return &ErrorCommand{Message: "not enough arguments for indexfile command"}
			}
			indexSource := commandTokens[1].Value
			indexValue, err := strconv.Atoi(commandTokens[2].Value)
			if err != nil {
				return &ErrorCommand{Message: "invalid index value for indexfile command"}
			}
			if indexValue < 0 || indexValue > 65535 {
				return &ErrorCommand{Message: "index value out of range for indexfile command"}
			}
			fmt.Printf("indexfile command: source=%s, index=%d\n", indexSource, indexValue)
			return &IndexFileCommand{SourcePath: indexSource, Index: uint16(indexValue)}
		default:
			return &ErrorCommand{Message: fmt.Sprintf("unknown command: %s", commandTokens[0].Value)}
		}
	}
	return &ErrorCommand{Message: "invalid command"}
}

func parseCommand(token tokeniser.TokenInfo, tokens []tokeniser.TokenInfo) (library.Command, int) {
	consumed := 0
	commandTokens := []tokeniser.TokenInfo{}

	for {
		switch token.Type {
		case tokeniser.TokenComment:
			// skip
		case tokeniser.TokenNewLine:
			if len(commandTokens) > 0 {
				return parseACommand(commandTokens), consumed
			}
		case tokeniser.TokenEOF:
			if len(commandTokens) > 0 {
				return parseACommand(commandTokens), consumed - 1
			}
		default:
			commandTokens = append(commandTokens, token)
		}
		consumed++
		if len(tokens) == 0 {
			break
		}
		token = tokens[0]
		tokens = tokens[1:]
	}
	return &ErrorCommand{Message: "unexpected end of input"}, consumed
}

func Parse(tokens []tokeniser.TokenInfo) ([]library.Command, error) {

	commands := []library.Command{}

	for {
		token := tokens[0]
		tokens = tokens[1:]
		switch {
		case token.Type == tokeniser.TokenComment:
			// Skip comments
			continue
		case token.Type == tokeniser.TokenEOF:
			return commands, nil
		case token.Type == tokeniser.TokenIdentifier:
			command, consumed := parseCommand(token, tokens)
			if command != nil {
				commands = append(commands, command)
			}
			tokens = tokens[consumed:]
		}
	}
}
