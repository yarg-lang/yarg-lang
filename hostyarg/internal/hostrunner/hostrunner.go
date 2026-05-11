package hostrunner

import (
	"fmt"
	"io/fs"
	"os"
	"os/exec"
	"path/filepath"

	"github.com/yarg-lang/yarg-lang/hostyarg/internal/deviceutil"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/expect_tests"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/runbinary"
)

type Compiler struct {
	Interpreter string
}

type HostRunner struct {
	Compiler
	LibPath string
}

func (h *HostRunner) RunInteractively(source string) error {

	if deviceutil.IsDevicePath(source) {
		return fmt.Errorf("cannot run device path locally")
	}

	runner := exec.Command(h.Interpreter, "--lib", h.LibPath, source)

	runner.Stdin = os.Stdin
	runner.Stdout = os.Stdout
	runner.Stderr = os.Stderr

	return runner.Run()
}

func (h *HostRunner) REPL() error {
	runner := exec.Command(h.Interpreter, "--lib", h.LibPath)

	runner.Stdin = os.Stdin
	runner.Stdout = os.Stdout
	runner.Stderr = os.Stderr

	return runner.Run()
}

func (h *HostRunner) CmdExpectTest(hostsource string) (err error, failed int) {

	tests := filepath.Clean(hostsource)
	h.Interpreter = filepath.Clean(h.Interpreter)
	h.LibPath = filepath.Clean(h.LibPath)

	info, err := os.Stat(tests)
	if err != nil {
		return
	}

	_, err = os.Stat(h.Interpreter)
	if err != nil {
		return fmt.Errorf("Could not stat %v, no interpreter to run", h.Interpreter), 0
	}

	_, err = os.Stat(h.LibPath)
	if err != nil {
		return fmt.Errorf("Could not stat %v, no library to include in test runs", h.LibPath), 0
	}

	var grandtotal, grandpass int

	if info.IsDir() {
		fileSystem := os.DirFS(tests)

		err = fs.WalkDir(fileSystem, ".", func(path string, d fs.DirEntry, walkerr error) error {
			if walkerr != nil {
				return walkerr
			}

			if !d.IsDir() {
				testfile := filepath.Join(tests, path)
				total, pass := h.runTestFile(testfile, path)
				grandtotal += total
				grandpass += pass
			}

			return nil
		})
	} else {
		logname := filepath.Base(tests)

		grandtotal, grandpass = h.runTestFile(tests, logname)
	}

	fmt.Printf("%s tests: %v, passed: %v\n", tests, grandtotal, grandpass)
	return err, grandtotal - grandpass
}

func (h *HostRunner) runTestFile(testfile, fsname string) (total, pass int) {

	test, total := expect_tests.CreateExpectationTest(testfile, fsname)

	output, errors, code, ok := h.RunBatch(testfile)
	if ok {
		pass = expect_tests.CmdReportTestResults(test, output, errors, code)
	}

	return total, pass
}

func (h *HostRunner) RunBatch(source string) (output []string, errors []string, returncode int, ok bool) {
	runner := exec.Command(h.Interpreter, "--lib", h.LibPath, source)

	output, errors, returncode, ok = runbinary.RunCommand(runner)
	return
}

func (c *Compiler) CmdCompile(source string, output string) error {

	source = filepath.Clean(source)
	c.Interpreter = filepath.Clean(c.Interpreter)

	_, err := os.Stat(source)
	if err != nil {
		return err
	}

	_, err = os.Stat(c.Interpreter)
	if err != nil {
		return fmt.Errorf("Could not stat %v, no interpreter to run", c.Interpreter)
	}

	return compileFile(c.Interpreter, source, output)
}

func compileFile(interpreter, source, output string) error {
	args := []string{"--compile", source}
	if output != "" {
		args = append(args, output)
	}
	runner := exec.Command(interpreter, args...)

	runner.Stdin = os.Stdin
	runner.Stdout = os.Stdout
	runner.Stderr = os.Stderr

	return runner.Run()
}
