package expect_tests

import (
	"bufio"
	"fmt"
	"log"
	"os"
	"reflect"
	"regexp"
	"strconv"
)

const (
	RUNTIME_ERROR = 70
	COMPILE_ERROR = 65
)

type expectationTest struct {
	fsname                   string
	expectations             int
	expectedExitCode         int
	expectedRuntimeErrorLine int
	expectedOutput           []string
	expectedError            []string
}

func CreateExpectationTest(testfile, fsname string) (test *expectationTest, total int) {

	file, err := os.Open(testfile)
	if err != nil {
		log.Fatal("could not open test")
	}
	defer file.Close()

	test = &expectationTest{}
	scanner := bufio.NewScanner(file)
	var lineNo int
	for scanner.Scan() {
		lineNo++
		test.parseLine(lineNo, scanner.Text())
	}

	if len(test.expectedOutput) == 0 && len(test.expectedError) == 0 {
		test.expectations++
	}

	test.fsname = fsname
	return test, test.expectations
}

func CmdReportTestResults(test *expectationTest, output, errors []string, code int) (pass int) {

	if test.validateCode(code) {

		switch code {
		case RUNTIME_ERROR:
			test.accountRuntimeErrorExpectations(errors, &pass)
		case COMPILE_ERROR:
			test.accountCompileErrorExpectations(errors, &pass)
		}

		test.accountEmptyTestExpectations(output, errors, &pass)
	}

	test.accountOutputExpectations(output, &pass)

	if pass != test.expectations {
		fmt.Printf("%v tests %v, passed %v", test.fsname, test.expectations, pass)
		if !test.validateCode(code) {
			fmt.Printf(", exitcode %v (expected: %v)", code, test.expectedExitCode)
		}
		fmt.Println()
		for l := range output {
			fmt.Println(output[l])
		}
		for l := range errors {
			fmt.Println(errors[l])
		}
	}

	return pass
}

func (test *expectationTest) validateCode(code int) bool {
	return code == test.expectedExitCode
}

func (test *expectationTest) accountEmptyTestExpectations(output, errors []string, pass *int) {

	if len(test.expectedError) == 0 && len(test.expectedOutput) == 0 && test.expectations == 1 {
		if len(output) == 0 && len(errors) == 0 {
			*pass += 1
		}
	}
}

func (test *expectationTest) accountCompileErrorExpectations(errors []string, pass *int) {

	if len(test.expectedError) > 0 &&
		reflect.DeepEqual(test.expectedError, errors) {
		*pass += len(errors)
		return
	}
}

func (test *expectationTest) accountRuntimeErrorExpectations(errors []string, pass *int) {
	if len(test.expectedError) > 0 {
		if errors[0] == test.expectedError[0] {
			*pass++
		}

		// examine the first line of the stack dump.
		r := regexp.MustCompile(`\[line (\d+)\]`)
		match := r.FindStringSubmatch(errors[1])
		candidate := strconv.Itoa(test.expectedRuntimeErrorLine)
		if match != nil && match[1] == candidate {
			*pass++
		}
	}
}

func (test *expectationTest) accountOutputExpectations(output []string, pass *int) {
	if reflect.DeepEqual(test.expectedOutput[0:len(output)], output) {
		*pass += len(output)
	}
}

func (test *expectationTest) parseLine(lineNo int, line string) {
	r := regexp.MustCompile(`// expect: ?(.*)`)
	match := r.FindStringSubmatch(line)
	if match != nil {
		test.expectedOutput = append(test.expectedOutput, match[1])
		test.expectations++
	}

	r = regexp.MustCompile(`// (Error.*)`)
	match = r.FindStringSubmatch(line)
	if match != nil {
		test.expectedExitCode = COMPILE_ERROR
		expected := fmt.Sprintf("[line %v] %v", lineNo, match[1])
		test.expectedError = append(test.expectedError, expected)
		test.expectations++
	}

	r = regexp.MustCompile(`// \[line (\d+)\] (Error.*)`)
	match = r.FindStringSubmatch(line)
	if match != nil {
		test.expectedExitCode = COMPILE_ERROR
		test.expectedError = append(test.expectedError, fmt.Sprintf("[line %v] %v", match[1], match[2]))
		test.expectations++
	}

	r = regexp.MustCompile(`// expect runtime error: (.+)`)
	match = r.FindStringSubmatch(line)
	if match != nil {
		test.expectedExitCode = RUNTIME_ERROR
		test.expectedError = append(test.expectedError, match[1])
		test.expectations++
		test.expectedRuntimeErrorLine = lineNo
		test.expectations++
	}
}
