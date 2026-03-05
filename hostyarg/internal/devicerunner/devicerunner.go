package devicerunner

// devicerunner provides functions to manipulate Pico boards via OpenOCD and a Raspberry Pi Pico Debug Probe.
//
// Captures and displays serial output from the Pico UART connected to the DebugProbe UART port.

import (
	"fmt"
	"log"
	"os/exec"
	"strings"

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

type picoPort struct {
	name         string
	serialNumber string
	pid          string
	vid          string
}

const (
	raspberryPiVID = "2E8A" // https://github.com/raspberrypi/usb-pid
	picoPID        = "000A" // Raspberry Pi Pico SDK CDC UART (RP2040)
	debugProbePID  = "000C" // Raspberry Pi RP2040 CMSIS-DAP debug adapter
)

func (p picoPort) String() string {
	if p.vid == raspberryPiVID && p.pid == debugProbePID {
		return fmt.Sprintf("%s, Serial=%s, VID:PID=%s:%s (Debug Probe)", p.name, p.serialNumber, p.vid, p.pid)
	} else if p.vid == raspberryPiVID && p.pid == picoPID {
		return fmt.Sprintf("%s, Serial=%s, VID:PID=%s:%s (Pico)", p.name, p.serialNumber, p.vid, p.pid)
	} else {
		return fmt.Sprintf("%s, Serial=%s, VID:PID=%s:%s", p.name, p.serialNumber, p.vid, p.pid)
	}
}

func getSerialOutput(portName string, sourcePath string, done chan bool) {

	defer func() { done <- true }()

	serial, err := serial.Open(portName, &serial.Mode{BaudRate: 115200})
	if err != nil {
		fmt.Println("Failed to open serial port!")
		return
	}
	defer serial.Close()

	serial.Write([]byte(fmt.Sprintf("exec(read_source(\"%s\"));\n", sourcePath)))

	buff := make([]byte, 100)
	for {
		n, err := serial.Read(buff)
		if err != nil {
			log.Fatal(err)
		}
		if n == 0 {
			fmt.Println("No data read from serial port, exiting.")
			break
		}

		input := string(buff[:n])
		fmt.Print(input)

		if strings.HasSuffix(input, "> ") {
			fmt.Print("\n")
			break
		}
		if strings.Contains(input, "\n") {
			fmt.Print("\n")
			continue
		}

	}
}

func DefaultPort() (string, bool) {

	detailedports, err := enumerator.GetDetailedPortsList()
	if err != nil {
		log.Println(err)
		return "", false
	}
	if len(detailedports) == 0 {
		log.Println("No serial ports found!")
		return "", false
	}

	picoPorts := []picoPort{}

	// prefer debug probe connections, since they have a more stable serial connection.
	// scan twice, looking for debug probe first, and then direct Pico connections.

	for _, port := range detailedports {
		if port.IsUSB && port.VID == raspberryPiVID && port.PID == debugProbePID {
			log.Printf("Found Debug Probe port: %s\n", port.Name)
			debugprobe := picoPort{
				name:         port.Name,
				serialNumber: port.SerialNumber,
				pid:          port.PID,
				vid:          port.VID,
			}
			picoPorts = append(picoPorts, debugprobe)
		}
	}

	for _, port := range detailedports {
		if port.IsUSB && port.VID == raspberryPiVID && port.PID == picoPID {
			log.Printf("Found Pico port: %s\n", port.Name)
			pico := picoPort{
				name:         port.Name,
				serialNumber: port.SerialNumber,
				pid:          port.PID,
				vid:          port.VID,
			}
			picoPorts = append(picoPorts, pico)
		}
	}

	if len(picoPorts) == 0 {
		return "", false
	}

	fmt.Printf("%s\n", picoPorts[0])
	return picoPorts[0].name, true
}

func CmdRunOnDevice(sourcePath string, port string) error {

	serialComplete := make(chan bool)
	go getSerialOutput(port, sourcePath, serialComplete)

	<-serialComplete
	return nil
}

func CmdListDevices() bool {

	detailedports, err := enumerator.GetDetailedPortsList()
	if err != nil {
		log.Println(err)
		return false
	}
	if len(detailedports) == 0 {
		log.Println("No device serial ports found!")
		return false
	}
	for _, port := range detailedports {
		if port.IsUSB && port.VID == raspberryPiVID {
			switch port.PID {
			case debugProbePID:
				fmt.Printf("%s, Serial=%s, VID:PID=%s:%s (Debug Probe)\n", port.Name, port.SerialNumber, port.VID, port.PID)
			case picoPID:
				fmt.Printf("%s, Serial=%s, VID:PID=%s:%s (Pico)\n", port.Name, port.SerialNumber, port.VID, port.PID)
			default:
				fmt.Printf("%s, Serial=%s, VID:PID=%s:%s (Raspberry Pi Device)\n", port.Name, port.SerialNumber, port.VID, port.PID)
			}
		} else {
			if port.IsUSB {
				log.Printf("%s, VID:PID=%s:%s\n", port.Name, port.VID, port.PID)
			} else {
				log.Printf("%s\n", port.Name)
			}
		}
	}
	return true
}
