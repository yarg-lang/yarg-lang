package hostyarg

import (
	"fmt"
	"os"
	"path/filepath"

	"github.com/yarg-lang/yarg-lang/hostyarg/internal/deviceimage"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/deviceutil"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/xiplibrary"
	"go.bug.st/serial/enumerator"
)

type YargCompiler interface {
	CmdCompile(source, output string) error
}

type YargRunner interface {
	RunInteractively(source string) error
	REPL() error
	RunBatch(source string) (output []string, errors []string, returncode int, ok bool)
	CmdExpectTest(source string) (err error, failed int)
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
		if port.IsUSB && port.VID == deviceutil.RaspberryPiVID {
			switch port.PID {
			case deviceutil.DebugProbePID:
				fmt.Printf("Connected Debug Probe %s, Serial=%s, VID:PID=%s:%s\n", port.Name, port.SerialNumber, port.VID, port.PID)
			case deviceutil.PicoPID:
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

func CmdLs(fsFilename string, dirEntry string, long bool) (e error) {
	ext := filepath.Ext(fsFilename)
	switch ext {
	case ".ylib":
		return xiplibrary.CmdLs(fsFilename, dirEntry, long)
	case ".uf2":
		return deviceimage.CmdLs(fsFilename, dirEntry, long)
	default:
		return fmt.Errorf("unsupported file extension %s", ext)
	}
}

func CmdFsInfo(fsFilename string) (e error) {
	ext := filepath.Ext(fsFilename)
	switch ext {
	case ".ylib":
		return xiplibrary.CmdFsInfo(fsFilename)
	case ".uf2":
		return deviceimage.CmdFsInfo(fsFilename)
	default:
		return fmt.Errorf("unsupported file extension %s", ext)
	}
}
