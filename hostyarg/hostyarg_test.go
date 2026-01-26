package main

import (
	"flag"
	"io"
	"log"
	"os"
	"testing"

	"github.com/yarg-lang/yarg-lang/hostyarg/internal/deviceimage"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/testrunner"
)

func copyFile(src, dst string) {
	input, err := os.ReadFile(src)
	if err != nil {
		panic(err)
	}
	err = os.WriteFile(dst, input, 0644)
	if err != nil {
		panic(err)
	}
}

func TestMain(m *testing.M) {
	log.SetOutput(io.Discard)

	copyFile("testdata/yarg-lang-pico-0.3.0.uf2", "testdata/yarg-lang-pico.uf2")
	code := m.Run()
	os.Remove("testdata/yarg-lang-pico.uf2")
	os.Exit(code)
}

func TestLs(t *testing.T) {

	deviceimage.CmdLs("testdata/yarg-lang-pico.uf2", "/")
}

func TestAddFile(t *testing.T) {

	os.Chdir("testdata")

	deviceimage.CmdAddFile("yarg-lang-pico.uf2", "fresh_cheese.ya")
	os.Chdir("../")
}

func TestFormat(t *testing.T) {

	deviceimage.Cmdformat("testdata/yarg-lang-pico.uf2")
}

var interpreter = flag.String("interpreter", "../bin/cyarg", "interpreter to use")

func TestRunTests(t *testing.T) {

	exit_code := testrunner.CmdRunTests(*interpreter, "testdata/tests")
	if exit_code != 0 {
		t.Fatalf("expected exit code 0, got %v", exit_code)
	}
}

func TestFileSequence(t *testing.T) {

	deviceimage.Cmdformat("testdata/yarg-lang-pico.uf2")
	os.Chdir("testdata")
	deviceimage.CmdAddFile("yarg-lang-pico.uf2", "fresh_cheese.ya")
	os.Chdir("../")

	deviceimage.CmdLs("testdata/yarg-lang-pico.uf2", "/")
}
