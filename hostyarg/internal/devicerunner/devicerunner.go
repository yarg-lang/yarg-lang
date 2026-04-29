package devicerunner

// devicerunner provides functions to manipulate Pico boards via OpenOCD and a Raspberry Pi Pico Debug Probe.
//
// Captures and displays serial output from the Pico UART connected to the DebugProbe UART port.

import (
	"fmt"
	"io"
	"log"
	"os"
	"os/exec"
	"strings"
	"sync"

	"github.com/yarg-lang/yarg-lang/hostyarg/internal/deviceutil"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/runbinary"

	"go.bug.st/serial"
)

const openOCDPath = "/Users/jhm/.pico-sdk/openocd/0.12.0+dev"

func openOCDCommand(task string) *exec.Cmd {
	command := exec.Command(openOCDPath+"/openocd",
		"-s", openOCDPath+"/scripts",
		"-f", "interface/cmsis-dap.cfg",
		"-f", "target/rp2040.cfg",
		"-c", "adapter speed 5000",
		"-c", task)
	command.Dir = openOCDPath
	return command
}

func runOpenOCDCommand(task string, c chan int) {
	openOCDCommand := openOCDCommand(task)

	output, errors, code, ok := runbinary.RunCommand(openOCDCommand)
	if ok {
		for _, line := range output {
			log.Println("OpenOCD:", line)
		}
		if code != 0 {
			for _, line := range errors {
				log.Println("OpenOCD stderr:", line)
			}
		}
		c <- code
	} else {
		log.Println("Failed to run OpenOCD")
		c <- -1
	}
}

func CmdResetDevice() (ok bool) {
	c := make(chan int)
	go runOpenOCDCommand("init; reset; exit", c)
	result := <-c
	return result == 0
}

func CmdFlashBinary(binaryPath string) (ok bool) {
	c := make(chan int)
	go runOpenOCDCommand(fmt.Sprintf("program \"%s\" verify reset; exit", binaryPath), c)
	result := <-c
	return result == 0
}

func GetSerialOutput(portName string, sourcePath string) (err error, output []string) {

	serial, err := serial.Open(portName, &serial.Mode{BaudRate: 115200})
	if err != nil {
		log.Println("Failed to open serial port:", err)
		return err, nil
	}
	defer serial.Close()
	serial.ResetInputBuffer()
	serial.ResetOutputBuffer()

	fmt.Fprintf(serial, "exec(read_source(\"%s\"));\n", sourcePath)
	output = make([]string, 0)

	output_error := make(chan error)

	go func() {

		linebuffer := ""
		buff := make([]byte, 1)
		for {
			n, err := serial.Read(buff)
			if err == io.EOF {
				output_error <- nil
				return
			}
			if err != nil {
				output_error <- err
				return
			}
			linebuffer += string(buff[:n])
			if strings.HasSuffix(linebuffer, "\r\n") {
				linebuffer = strings.TrimSuffix(linebuffer, "\r\n")
				output = append(output, linebuffer)
				linebuffer = ""
			}
			if strings.HasSuffix(linebuffer, "yarg> ") {
				output_error <- nil
				return
			}
		}
	}()

	err = <-output_error
	return err, output
}

func streamSerialOutput(portName string, sourcePath string, outputstream io.Writer) (err error) {

	serial, err := serial.Open(portName, &serial.Mode{BaudRate: 115200})
	if err != nil {
		log.Println("Failed to open serial port:", err)
		return err
	}
	defer serial.Close()
	serial.ResetInputBuffer()
	serial.ResetOutputBuffer()

	fmt.Fprintf(serial, "exec(read_source(\"%s\"));\n", sourcePath)

	output_error := make(chan error)

	go func() {

		linebuffer := ""
		buff := make([]byte, 1)
		for {
			n, err := serial.Read(buff)
			if err == io.EOF {
				output_error <- nil
				return
			}
			if err != nil {
				output_error <- err
				return
			}
			linebuffer += string(buff[:n])
			if strings.HasSuffix(linebuffer, "\r\n") {
				linebuffer = strings.TrimSuffix(linebuffer, "\r\n")
				outputstream.Write([]byte(linebuffer + "\n"))
				linebuffer = ""
			}
			if strings.HasSuffix(linebuffer, "yarg> ") {
				output_error <- nil
				return
			}
		}
	}()

	err = <-output_error
	return err
}

func StreamSerialIO(portName string) error {
	serial, err := serial.Open(portName, &serial.Mode{BaudRate: 115200})
	if err != nil {
		log.Println("Failed to open serial port:", err)
		return err
	}
	defer serial.Close()
	serial.ResetInputBuffer()
	serial.ResetOutputBuffer()

	fmt.Fprint(serial, "\r\n")

	input_error := make(chan error)

	var wg sync.WaitGroup
	wg.Go(func() {
		defer wg.Done()
		buff := make([]byte, 1)
		for {
			n, err := os.Stdin.Read(buff)
			if err != nil {
				input_error <- err
				return
			}
			if buff[:n][0] == '\n' {
				fmt.Fprint(serial, "\r\n")
				continue
			}
			serial.Write(buff[:n])
		}
	})

	output_error := make(chan error)

	wg.Go(func() {
		defer wg.Done()

		buff := make([]byte, 1)
		for {
			n, err := serial.Read(buff)
			if err == io.EOF {
				output_error <- nil
				return
			}
			if err != nil {
				output_error <- err
				return
			}
			fmt.Print(string(buff[:n]))
		}
	})

	wg.Wait()
	ine := <-input_error
	oute := <-output_error
	if ine != nil {
		return ine
	} else if oute != nil {
		return oute
	} else {
		return nil
	}
}

type DeviceRunner struct {
	Port deviceutil.PicoPort
}

func (d *DeviceRunner) RunInteractively(source string) (err error) {

	if !deviceutil.IsDevicePath(source) {
		return fmt.Errorf("source must be a device path starting with 'device:'")
	}
	source = deviceutil.DevicePath(source)

	fmt.Println(d.Port)
	err = streamSerialOutput(d.Port.Name, source, os.Stdout)
	return err
}

func (d *DeviceRunner) REPL() error {
	return StreamSerialIO(d.Port.Name)
}

func (d *DeviceRunner) CmdExpectTest(hostsource string) (error, int) {
	return nil, 0
}

func (d *DeviceRunner) RunBatch(source string) (output []string, errors []string, returncode int, ok bool) {
	if !deviceutil.IsDevicePath(source) {
		return nil, []string{fmt.Errorf("source must be a device path starting with 'device:'").Error()}, -1, false
	}
	source = deviceutil.DevicePath(source)

	err, output := GetSerialOutput(d.Port.Name, source)

	if err != nil {
		return output, []string{err.Error()}, -1, false
	}

	return output, nil, 0, true
}
