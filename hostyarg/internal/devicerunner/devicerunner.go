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

	"github.com/yarg-lang/yarg-lang/hostyarg/internal/runbinary"

	"go.bug.st/serial/enumerator"

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

type PicoPort struct {
	name         string
	serialNumber string
	pid          string
	vid          string
}

const (
	RaspberryPiVID = "2E8A" // https://github.com/raspberrypi/usb-pid
	PicoPID        = "000A" // Raspberry Pi Pico SDK CDC UART (RP2040)
	DebugProbePID  = "000C" // Raspberry Pi RP2040 CMSIS-DAP debug adapter
)

func (p PicoPort) String() string {
	if p.vid == RaspberryPiVID && p.pid == DebugProbePID {
		return fmt.Sprintf("%s, Serial=%s, VID:PID=%s:%s (Debug Probe)", p.name, p.serialNumber, p.vid, p.pid)
	} else if p.vid == RaspberryPiVID && p.pid == PicoPID {
		return fmt.Sprintf("%s, Serial=%s, VID:PID=%s:%s (Pico)", p.name, p.serialNumber, p.vid, p.pid)
	} else {
		return fmt.Sprintf("%s, Serial=%s, VID:PID=%s:%s", p.name, p.serialNumber, p.vid, p.pid)
	}
}

func (p PicoPort) Name() string {
	return p.name
}

func GetSerialOutput(portName string, sourcePath string, done chan error) (output []string) {

	var result_error error

	defer func() { done <- result_error }()

	serial, result_error := serial.Open(portName, &serial.Mode{BaudRate: 115200})
	if result_error != nil {
		return nil
	}
	defer serial.Close()

	fmt.Fprintf(serial, "exec(read_source(\"%s\"));\n", sourcePath)

	buff := make([]byte, 100)
	for {
		n, err := serial.Read(buff)
		if err != nil {
			result_error = err
			return nil
		}
		if n == 0 {
			fmt.Println("No data read from serial port, exiting.")
			break
		}

		input := string(buff[:n])
		lines := strings.SplitSeq(input, "\n")
		for line := range lines {
			if strings.HasSuffix(line, "> ") {
				return output
			}
			output = append(output, line)
		}
	}
	return output
}

func StreamSerialIO(portName string) error {
	serial, err := serial.Open(portName, &serial.Mode{BaudRate: 115200})
	if err != nil {
		log.Println("Failed to open serial port:", err)
		return err
	}
	defer serial.Close()

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

func DefaultPort() (p PicoPort, ok bool) {

	detailedports, err := enumerator.GetDetailedPortsList()
	if err != nil {
		log.Println(err)
		return p, false
	}
	if len(detailedports) == 0 {
		log.Println("No serial ports found!")
		return p, false
	}

	picoPorts := []PicoPort{}

	// prefer debug probe connections, since they have a more stable serial connection.
	// scan twice, looking for debug probe first, and then direct Pico connections.

	for _, port := range detailedports {
		if port.IsUSB && port.VID == RaspberryPiVID && port.PID == DebugProbePID {
			log.Printf("Found Debug Probe port: %s\n", port.Name)
			debugprobe := PicoPort{
				name:         port.Name,
				serialNumber: port.SerialNumber,
				pid:          port.PID,
				vid:          port.VID,
			}
			picoPorts = append(picoPorts, debugprobe)
		}
	}

	for _, port := range detailedports {
		if port.IsUSB && port.VID == RaspberryPiVID && port.PID == PicoPID {
			log.Printf("Found Pico port: %s\n", port.Name)
			pico := PicoPort{
				name:         port.Name,
				serialNumber: port.SerialNumber,
				pid:          port.PID,
				vid:          port.VID,
			}
			picoPorts = append(picoPorts, pico)
		}
	}

	if len(picoPorts) == 0 {
		return p, false
	}

	return picoPorts[0], true
}

func PicoPortFor(name string) (p PicoPort, ok bool) {

	detailedports, err := enumerator.GetDetailedPortsList()
	if err != nil {
		log.Println(err)
		return p, false
	}
	if len(detailedports) == 0 {
		log.Println("No serial ports found!")
		return p, false
	}

	for _, port := range detailedports {
		if port.Name == name && port.IsUSB {
			log.Printf("Found port: %s\n", port.Name)
			picoPort := PicoPort{
				name:         port.Name,
				serialNumber: port.SerialNumber,
				pid:          port.PID,
				vid:          port.VID,
			}
			return picoPort, true
		}
	}

	return p, false
}
