package main

import (
	"bufio"
	"bytes"
	"fmt"
	"io/fs"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"reflect"
	"regexp"
	"strconv"
)

const (
	RUNTIME_ERROR = 70
	COMPILE_ERROR = 65
)

type Test struct {
	Expectations             int
	Name                     string
	FileName                 string
	ExpectedExitCode         int
	ExpectedRuntimeErrorLine int
	ExpectedOutput           []string
	ExpectedError            []string
}

func testsAvailable(test string) (isDir, ok bool) {
	info, err := os.Stat(test)
	if err == nil {
		return info.IsDir(), true
	}
	return isDir, false
}

func interpreterAvailable(interpreter string) (ok bool) {
	_, err := os.Stat(interpreter)
	ok = err == nil
	return ok
}

func cmdRunTests(interpreter string, test string) (exitcode int) {

	test = filepath.Clean(test)
	interpreter = filepath.Clean(interpreter)

	isDir, ok := testsAvailable(test)
	if !ok {
		fmt.Printf("Could not stat %v, nothing to test\n", test)
		return
	}
	ok = interpreterAvailable(interpreter)
	if !ok {
		fmt.Printf("Could not stat %v, no interpreter to run\n", interpreter)
		return
	}

	var grandtotal, grandpass int

	if isDir {
		fileSystem := os.DirFS(test)

		fs.WalkDir(fileSystem, ".", func(path string, d fs.DirEntry, err error) error {
			if err != nil {
				log.Fatal(err)
			}

			if path == "benchmark" {
				return fs.SkipDir
			}

			if !d.IsDir() {
				target := filepath.Join(test, path)
				total, pass := runTestFile(interpreter, target)
				grandtotal += total
				grandpass += pass
			}
			return nil
		})
	} else {
		total, pass := runTestFile(interpreter, test)
		grandtotal += total
		grandpass += pass
	}

	fmt.Printf("Interpreter: %v\n", interpreter)
	fmt.Printf("Tests: %v\n", test)
	fmt.Printf("Total tests: %v, passed: %v\n", grandtotal, grandpass)

	return grandtotal - grandpass
}

func testFriendlyName(testfile string) string {
	test_name := filepath.Base(testfile)
	extension := filepath.Ext(testfile)

	return test_name[:len(test_name)-len(extension)]
}

func runTestFile(interpreter string, testfile string) (total, pass int) {

	var test Test
	test.FileName = testfile
	test.Name = testFriendlyName(test.FileName)

	test.parseTestSource()
	total = test.Expectations

	output, errors, code, ok := runInterpreter(interpreter, test.FileName)
	if ok {

		if test.validateCode(code) {

			if code == RUNTIME_ERROR {
				test.accountRuntimeErrorExpectations(errors, &pass)
			} else if code == COMPILE_ERROR {
				test.accountCompileErrorExpectations(errors, &pass)
			}

			test.accountEmptyTestExpectations(output, errors, &pass)
		}

		test.accountOutputExpectations(output, &pass)
	}

	if pass != total {
		fmt.Printf("test: %v\n", test.Name)
		fmt.Printf("tests supplied: %v\n", total)
		fmt.Printf("tests passed: %v\n", pass)
		fmt.Printf("--- (%v)\n", code)
		for l := range output {
			fmt.Println(output[l])
		}
		for l := range errors {
			fmt.Println(errors[l])
		}
		fmt.Println("---")
	}

	return total, pass
}

func (test *Test) validateCode(code int) bool {
	return code == test.ExpectedExitCode
}

func (test *Test) accountEmptyTestExpectations(output, errors []string, pass *int) {

	if len(test.ExpectedError) == 0 && len(test.ExpectedOutput) == 0 && test.Expectations == 1 {
		if len(output) == 0 && len(errors) == 0 {
			*pass += 1
		}
	}
}

func (test *Test) accountCompileErrorExpectations(errors []string, pass *int) bool {

	if len(test.ExpectedError) > 0 &&
		reflect.DeepEqual(test.ExpectedError, errors) {
		*pass += len(errors)
		return true
	}
	return false
}

func (test *Test) accountRuntimeErrorExpectations(errors []string, pass *int) {
	if len(test.ExpectedError) > 0 {
		if errors[0] == test.ExpectedError[0] {
			*pass++
		}

		// examine the first line of the stack dump.
		r := regexp.MustCompile(`\[line (\d+)\]`)
		match := r.FindStringSubmatch(errors[1])
		candidate := strconv.Itoa(test.ExpectedRuntimeErrorLine)
		if match != nil && match[1] == candidate {
			*pass++
		}
	}
}

func (test *Test) accountOutputExpectations(output []string, pass *int) {
	if reflect.DeepEqual(test.ExpectedOutput[0:len(output)], output) {
		*pass += len(output)
	}
}

func (test *Test) parseTestSource() {
	file, err := os.Open(test.FileName)
	if err != nil {
		log.Fatal("could not open test")
	}
	defer file.Close()

	scanner := bufio.NewScanner(file)
	var lineNo int
	for scanner.Scan() {
		lineNo++
		test.parseLine(lineNo, scanner.Text())
	}

	if len(test.ExpectedOutput) == 0 && len(test.ExpectedError) == 0 {
		test.Expectations++
	}
}

func (test *Test) parseLine(lineNo int, line string) {
	r := regexp.MustCompile(`// expect: ?(.*)`)
	match := r.FindStringSubmatch(line)
	if match != nil {
		test.ExpectedOutput = append(test.ExpectedOutput, match[1])
		test.Expectations++
	}

	r = regexp.MustCompile(`// (Error.*)`)
	match = r.FindStringSubmatch(line)
	if match != nil {
		test.ExpectedExitCode = COMPILE_ERROR
		expected := fmt.Sprintf("[line %v] %v", lineNo, match[1])
		test.ExpectedError = append(test.ExpectedError, expected)
		test.Expectations++
	}

	r = regexp.MustCompile(`// \[line (\d+)\] (Error.*)`)
	match = r.FindStringSubmatch(line)
	if match != nil {
		test.ExpectedExitCode = COMPILE_ERROR
		test.ExpectedError = append(test.ExpectedError, fmt.Sprintf("[line %v] %v", match[1], match[2]))
		test.Expectations++
	}

	r = regexp.MustCompile(`// expect runtime error: (.+)`)
	match = r.FindStringSubmatch(line)
	if match != nil {
		test.ExpectedExitCode = RUNTIME_ERROR
		test.ExpectedError = append(test.ExpectedError, match[1])
		test.Expectations++
		test.ExpectedRuntimeErrorLine = lineNo
		test.Expectations++
	}
}

func streamToLines(stream bytes.Buffer) (lines []string) {
	scanner := bufio.NewScanner(&stream)
	for scanner.Scan() {
		lines = append(lines, scanner.Text())
	}
	return lines
}

func runInterpreter(interpreter, argument string) (output, errors []string, exitcode int, ok bool) {
	var cstdout, cstderr bytes.Buffer
	runner := exec.Command(interpreter, argument)
	runner.Stdout = &cstdout
	runner.Stderr = &cstderr

	err := runner.Run()
	ok = err == nil

	if err != nil {
		switch e := err.(type) {
		case *exec.ExitError:
			ok = true
			exitcode = e.ExitCode()
		default:
			fmt.Println("failed executing:", err)
		}
	}

	if ok {
		output = streamToLines(cstdout)
		errors = streamToLines(cstderr)
	}

	return output, errors, exitcode, ok
}
