package hostrunner

import (
	"fmt"
	"io/fs"
	"os"
	"os/exec"
	"path/filepath"

	"github.com/yarg-lang/yarg-lang/hostyarg/internal/runbinary"
)

type HostRunner struct {
	Interpreter string
	YargLib     string
}

func (hr HostRunner) RunLocally(source string) (output []string, errors []string, returncode int, ok bool) {
	runner := exec.Command(hr.Interpreter, "--lib", hr.YargLib, source)

	output, errors, returncode, ok = runbinary.RunCommand(runner)
	return
}

func CmdCompile(source string, interpreter string) bool {

	source = filepath.Clean(source)
	interpreter = filepath.Clean(interpreter)

	info, err := os.Stat(source)
	if err != nil {
		return false
	}

	_, err = os.Stat(interpreter)
	if err != nil {
		return false
	}

	if info.IsDir() {
		fileSystem := os.DirFS(source)

		err = fs.WalkDir(fileSystem, ".", func(path string, d fs.DirEntry, walkerr error) error {
			if walkerr != nil {
				return walkerr
			}

			if !d.IsDir() {
				testfile := filepath.Join(source, path)
				compileFile(interpreter, testfile, path)
			}

			return nil
		})
	} else {
		logname := filepath.Base(source)

		compileFile(interpreter, source, logname)
	}

	return true
}

func compileFile(interpreter string, source string, logname string) {
	runner := exec.Command(interpreter, "--compile", source)

	output, errors, _, ok := runbinary.RunCommand(runner)
	if ok {
		for _, line := range output {
			fmt.Println(logname, ":", line)
		}
		for _, line := range errors {
			fmt.Println(logname, ":", line)
		}
	}
}
