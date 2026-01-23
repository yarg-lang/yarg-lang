package main

import (
	"flag"
	"fmt"
	"io"
	"log"
	"os"

	"github.com/yarg-lang/yarg-lang/hostyarg/internal/deviceimage"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/testrunner"
)

func main() {

	log.SetOutput(io.Discard)

	formatFSCmd := flag.NewFlagSet("format", flag.ExitOnError)
	formatFSFS := formatFSCmd.String("fs", "test.uf2", "format fs on this image")

	addFileCmd := flag.NewFlagSet("addfile", flag.ExitOnError)
	addFileFS := addFileCmd.String("fs", "test.uf2", "add file to this filesystem")
	addFileName := addFileCmd.String("add", "", "filename to add")

	lsDirCmd := flag.NewFlagSet("ls", flag.ExitOnError)
	lsDirFS := lsDirCmd.String("fs", "test.uf2", "filesystem to mount")
	lsDirEntry := lsDirCmd.String("dir", "/", "directory to ls")

	testRunCmd := flag.NewFlagSet("runtests", flag.ExitOnError)
	testRunInterpreter := testRunCmd.String("interpreter", "cyarg", "default interpreter")
	testRunTests := testRunCmd.String("tests", "", "default tests")

	if len(os.Args) < 2 {
		fmt.Println("expected command")
		os.Exit(64)
	}

	switch os.Args[1] {
	case "format":
		formatFSCmd.Parse(os.Args[2:])
		deviceimage.Cmdformat(*formatFSFS)
	case "addfile":
		addFileCmd.Parse(os.Args[2:])
		if *addFileName == "" {
			fmt.Println("expect filename to add")
			os.Exit(64)
		}
		deviceimage.CmdAddFile(*addFileFS, *addFileName)
	case "ls":
		lsDirCmd.Parse(os.Args[2:])
		deviceimage.CmdLs(*lsDirFS, *lsDirEntry)
	case "runtests":
		testRunCmd.Parse(os.Args[2:])
		exit_code := testrunner.CmdRunTests(*testRunInterpreter, *testRunTests)
		os.Exit(exit_code)
	default:
		fmt.Println("unknown command")
		os.Exit(64)
	}
}
