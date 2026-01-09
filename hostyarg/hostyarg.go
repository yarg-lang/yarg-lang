package main

import (
	"flag"
	"fmt"
	"io"
	"log"
	"os"
)

func add_file(lfs LittleFs, fileToAdd string) {

	r, err := os.Open(fileToAdd)
	if err != nil {
		log.Fatal("nothing to open")
	}
	defer r.Close()
	data, err := io.ReadAll(r)
	if err != nil {
		log.Fatal("nothing read")
	}

	file, _ := lfs.OpenFile(fileToAdd)
	defer file.Close()

	file.Write(data)
}

func list_files(fs LittleFs, dirEntry string) {

	dir, err := fs.OpenDir(dirEntry)
	if err != nil {
		log.Fatal(err.Error())
	}
	defer dir.Close()

	for more, info, err := dir.Read(); more; more, info, err = dir.Read() {
		if err != nil {
			log.Fatal(err)
		}
		switch info.Type {
		case EntryTypeDir:
			fmt.Printf("'%v' (dir)\n", info.Name)
		case EntryTypeReg:
			fmt.Printf("'%v' (%v)\n", info.Name, info.Size)
		default:
			log.Fatal("unexpected entry type.")
		}
	}

}

func (bd BlockDevice) readFromUF2File(filename string) (e error) {
	f, e := os.Open(filename)
	if e == nil {
		defer f.Close()
		bd.ReadFromUF2(f)
	}
	return e
}

func (bd BlockDevice) writeToUF2File(filename string) error {
	f, err := os.OpenFile(filename, os.O_RDWR|os.O_CREATE, 0666)
	if err != nil {
		log.Fatal(err)
	}
	defer f.Close()

	bd.WriteAsUF2(f)

	return nil
}

func formatCmd(fs BdFS, fsFilename string) {
	fs.flash_fs.device.readFromUF2File(fsFilename)

	fs.cfg.Format()

	fs.flash_fs.device.writeToUF2File(fsFilename)
}

func cmdAddFile(fs BdFS, fsFilename, fileToAdd string) {
	fs.flash_fs.device.readFromUF2File(fsFilename)

	lfs, _ := fs.cfg.Mount()
	defer lfs.Close()

	add_file(lfs, fileToAdd)

	fs.flash_fs.device.writeToUF2File(fsFilename)
}

func cmdLs(fs BdFS, fsFilename, dirEntry string) {
	fs.flash_fs.device.readFromUF2File(fsFilename)

	lfs, _ := fs.cfg.Mount()
	defer lfs.Close()

	list_files(lfs, dirEntry)
}

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

	device := newBlockDevice()
	defer device.Close()

	fs := newBdFS(device, FLASHFS_BASE_ADDR, FLASHFS_BLOCK_COUNT)
	defer fs.Close()

	switch os.Args[1] {
	case "format":
		formatFSCmd.Parse(os.Args[2:])
		formatCmd(*fs, *formatFSFS)
	case "addfile":
		addFileCmd.Parse(os.Args[2:])
		if *addFileName == "" {
			fmt.Println("expect filename to add")
			os.Exit(64)
		}
		cmdAddFile(*fs, *addFileFS, *addFileName)
	case "ls":
		lsDirCmd.Parse(os.Args[2:])
		cmdLs(*fs, *lsDirFS, *lsDirEntry)
	case "runtests":
		testRunCmd.Parse(os.Args[2:])
		exit_code := cmdRunTests(*testRunInterpreter, *testRunTests)
		os.Exit(exit_code)
	default:
		fmt.Println("unknown command")
		os.Exit(64)
	}
}
