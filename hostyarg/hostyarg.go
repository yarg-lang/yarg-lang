package hostyarg

import (
	"fmt"
	"os/exec"
	"strings"

	"github.com/yarg-lang/yarg-lang/hostyarg/internal/devicerunner"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/runbinary"
)

func IsDevicePath(path string) bool {
	return strings.HasPrefix(path, "device:")
}

func DevicePath(path string) string {
	return strings.TrimPrefix(path, "device:")
}

type YargRunner interface {
	Run(source string) error
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

type HostRunner struct {
	Interpreter string
}

func (h *HostRunner) Run(source string) error {

	if IsDevicePath(source) {
		return fmt.Errorf("cannot run device path locally")
	}

	runner := exec.Command(h.Interpreter, source)

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

func DefaultYargRunner() (runner YargRunner) {
	port, ok := devicerunner.DefaultPort()
	if ok {
		runner = &DeviceRunner{Port: port}
	}
	return
}
