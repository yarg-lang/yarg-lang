package hostrunner

import (
	"flag"
	"io"
	"log"
	"os"
	"testing"
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

var testdataDir = flag.String("testdata", "../../testdata", "directory containing test data")
var interpreter = flag.String("interpreter", "../../../bin/cyarg", "interpreter to use")
var libPath = flag.String("lib", "../../../yarg/specimen", "library to include in test runs")

func TestMain(m *testing.M) {
	log.SetOutput(io.Discard)

	copyFile(*testdataDir+"/yarg-lang-pico-0.3.0.uf2", *testdataDir+"/yarg-lang-pico.uf2")
	code := m.Run()
	os.Remove(*testdataDir + "/yarg-lang-pico.uf2")
	os.Exit(code)
}

func TestRunTests(t *testing.T) {

	drun := &HostRunner{}
	drun.Interpreter = *interpreter
	drun.LibPath = *libPath

	err, failedtests := drun.CmdExpectTest(*testdataDir + "/tests")
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if failedtests != 0 {
		t.Fatalf("expected 0 failed tests, got %v", failedtests)
	}
}
