package hostyarg

import (
	"fmt"
	"io/fs"
	"os"
	"os/exec"
	"path/filepath"
	"strings"

	"github.com/yarg-lang/yarg-lang/hostyarg/internal/devicerunner"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/hostrunner"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/runbinary"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/testrunner"
	"go.bug.st/serial/enumerator"
)

func IsDevicePath(path string) bool {
	return strings.HasPrefix(path, "device:")
}

func DevicePath(path string) string {
	return strings.TrimPrefix(path, "device:")
}

type YargRunner interface {
	Run(source string) error
	REPL() error
}

type DeviceRunner struct {
	Port devicerunner.PicoPort
}

func (d *DeviceRunner) Run(source string) error {

	if !IsDevicePath(source) {
		return fmt.Errorf("source must be a device path starting with 'device:'")
	}
	source = DevicePath(source)

	serialComplete := make(chan error)
	output := make([]string, 0)
	go func() {
		output = devicerunner.GetSerialOutput(d.Port.Name(), source, serialComplete)
	}()

	error := <-serialComplete
	if error != nil {
		return error
	}
	fmt.Println(d.Port)
	for _, line := range output {
		fmt.Println(line)
	}

	return error
}

func (d *DeviceRunner) REPL() error {
	return devicerunner.StreamSerialIO(d.Port.Name())
}

type HostRunner struct {
	Interpreter string
	LibPath     string
}

func (h *HostRunner) Run(source string) error {

	if IsDevicePath(source) {
		return fmt.Errorf("cannot run device path locally")
	}

	runner := exec.Command(h.Interpreter, "--lib", h.LibPath, source)

	output, errors, _, ok := runbinary.RunCommand(runner)
	fmt.Println(h.Interpreter)
	if ok {
		for _, line := range output {
			fmt.Println(line)
		}
		for _, line := range errors {
			fmt.Println(line)
		}
	}
	return nil
}

func (h *HostRunner) REPL() error {
	runner := exec.Command(h.Interpreter, "--lib", h.LibPath)

	runner.Stdin = os.Stdin
	runner.Stdout = os.Stdout
	runner.Stderr = os.Stderr

	return runner.Run()
}

func DefaultYargRunner(suppliedPort, suppliedInterpreter, suppliedLib string) (runner YargRunner, err error) {

	if suppliedPort == "" && suppliedInterpreter == "" {
		port, ok := devicerunner.DefaultPort()
		if !ok {
			return nil, fmt.Errorf("failed to find default device port")
		}
		runner = &DeviceRunner{Port: port}
		return runner, nil
	} else if suppliedPort != "" && suppliedInterpreter != "" {
		return nil, fmt.Errorf("cannot specify both device port and local interpreter")
	} else if suppliedInterpreter != "" {
		runner = &HostRunner{Interpreter: suppliedInterpreter, LibPath: suppliedLib}
		return runner, nil
	} else if suppliedPort != "" {
		port, ok := devicerunner.PicoPortFor(suppliedPort)
		if !ok {
			return nil, fmt.Errorf("failed to find specified device port")
		}
		runner = &DeviceRunner{Port: port}
		return runner, nil
	}
	return nil, fmt.Errorf("invalid configuration")
}

func CmdListDevices(interpreter, lib string) bool {

	if interpreter != "" && lib != "" {
		if _, err := os.Stat(interpreter); err != nil {
			fmt.Fprintln(os.Stderr, "could not stat interpreter at "+interpreter)
			return false
		}
		if _, err := os.Stat(lib); err != nil {
			fmt.Fprintln(os.Stderr, "could not stat library at "+lib)
			return false
		}
		fmt.Printf("Host device interpreter %s and library %s\n", interpreter, lib)
	} else if interpreter != "" || lib != "" {
		fmt.Fprintln(os.Stderr, "cannot specify only one of interpreter and lib for devices command")
		return false
	}

	detailedports, err := enumerator.GetDetailedPortsList()
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		return false
	}
	if len(detailedports) == 0 {
		fmt.Fprintln(os.Stderr, "No device serial ports found!")
		return false
	}
	for _, port := range detailedports {
		if port.IsUSB && port.VID == devicerunner.RaspberryPiVID {
			switch port.PID {
			case devicerunner.DebugProbePID:
				fmt.Printf("Connected Debug Probe %s, Serial=%s, VID:PID=%s:%s\n", port.Name, port.SerialNumber, port.VID, port.PID)
			case devicerunner.PicoPID:
				fmt.Printf("Connected Pico %s, Serial=%s, VID:PID=%s:%s\n", port.Name, port.SerialNumber, port.VID, port.PID)
			default:
				fmt.Printf("Connected Raspberry Pi Device %s, Serial=%s, VID:PID=%s:%s\n", port.Name, port.SerialNumber, port.VID, port.PID)
			}
		} else if port.IsUSB {
			fmt.Printf("Connected USB Serial %s, VID:PID=%s:%s\n", port.Name, port.VID, port.PID)
		} else {
			fmt.Printf("Connected Serial%s\n", port.Name)
		}
	}
	return true
}

func CmdRunTests(interpreter, lib, tests string) (err error, failedtests int) {

	tests = filepath.Clean(tests)
	interpreter = filepath.Clean(interpreter)
	lib = filepath.Clean(lib)

	info, err := os.Stat(tests)
	if err != nil {
		return
	}

	_, err = os.Stat(interpreter)
	if err != nil {
		return fmt.Errorf("Could not stat %v, no interpreter to run", interpreter), 0
	}

	_, err = os.Stat(lib)
	if err != nil {
		return fmt.Errorf("Could not stat %v, no library to include in test runs", lib), 0
	}

	runner := hostrunner.HostRunner{Interpreter: interpreter, YargLib: lib}

	var grandtotal, grandpass int

	if info.IsDir() {
		fileSystem := os.DirFS(tests)

		err = fs.WalkDir(fileSystem, ".", func(path string, d fs.DirEntry, walkerr error) error {
			if walkerr != nil {
				return walkerr
			}

			if !d.IsDir() {
				testfile := filepath.Join(tests, path)
				total, pass := testrunner.RunTestFile(runner, testfile, path)
				grandtotal += total
				grandpass += pass
			}

			return nil
		})
	} else {
		logname := filepath.Base(tests)

		grandtotal, grandpass = testrunner.RunTestFile(runner, tests, logname)
	}

	fmt.Printf("%s tests: %v, passed: %v\n", tests, grandtotal, grandpass)
	return err, grandtotal - grandpass
}
