package runbinary

import (
	"bufio"
	"bytes"
	"fmt"
	"os/exec"
)

func streamToLines(stream bytes.Buffer) (lines []string) {
	scanner := bufio.NewScanner(&stream)
	for scanner.Scan() {
		lines = append(lines, scanner.Text())
	}
	return lines
}

func RunCommand(runner *exec.Cmd) (output, errors []string, exitcode int, ok bool) {
	var cstdout, cstderr bytes.Buffer
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
