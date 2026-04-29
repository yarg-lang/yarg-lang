package deviceutil

import (
	"fmt"
	"log"
	"strings"

	"go.bug.st/serial/enumerator"
)

func IsDevicePath(path string) bool {
	return strings.HasPrefix(path, "device:")
}

func DevicePath(path string) string {
	return strings.TrimPrefix(path, "device:")
}

type PicoPort struct {
	Name         string
	SerialNumber string
	PID          string
	VID          string
}

const (
	RaspberryPiVID = "2E8A" // https://github.com/raspberrypi/usb-pid
	PicoPID        = "000A" // Raspberry Pi Pico SDK CDC UART (RP2040)
	DebugProbePID  = "000C" // Raspberry Pi RP2040 CMSIS-DAP debug adapter
)

func (p PicoPort) String() string {
	if p.VID == RaspberryPiVID && p.PID == DebugProbePID {
		return fmt.Sprintf("%s, Serial=%s, VID:PID=%s:%s (Debug Probe)", p.Name, p.SerialNumber, p.VID, p.PID)
	} else if p.VID == RaspberryPiVID && p.PID == PicoPID {
		return fmt.Sprintf("%s, Serial=%s, VID:PID=%s:%s (Pico)", p.Name, p.SerialNumber, p.VID, p.PID)
	} else {
		return fmt.Sprintf("%s, Serial=%s, VID:PID=%s:%s", p.Name, p.SerialNumber, p.VID, p.PID)
	}
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
				Name:         port.Name,
				SerialNumber: port.SerialNumber,
				PID:          port.PID,
				VID:          port.VID,
			}
			return picoPort, true
		}
	}

	return p, false
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
				Name:         port.Name,
				SerialNumber: port.SerialNumber,
				PID:          port.PID,
				VID:          port.VID,
			}
			picoPorts = append(picoPorts, debugprobe)
		}
	}

	for _, port := range detailedports {
		if port.IsUSB && port.VID == RaspberryPiVID && port.PID == PicoPID {
			log.Printf("Found Pico port: %s\n", port.Name)
			pico := PicoPort{
				Name:         port.Name,
				SerialNumber: port.SerialNumber,
				PID:          port.PID,
				VID:          port.VID,
			}
			picoPorts = append(picoPorts, pico)
		}
	}

	if len(picoPorts) == 0 {
		return p, false
	}

	return picoPorts[0], true
}
