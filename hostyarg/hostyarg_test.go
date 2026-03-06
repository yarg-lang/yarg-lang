package hostyarg

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

	deviceimage.CmdLs("testdata/yarg-lang-pico.uf2", ".", false)
	deviceimage.CmdLs("testdata/yarg-lang-pico.uf2", ".", true)
}

func TestCopyFile(t *testing.T) {

	os.Chdir("testdata")
	err := deviceimage.CmdCp("yarg-lang-pico.uf2", "fresh_cheese.ya", "copy_of_fresh_cheese.ya")
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	err = deviceimage.CmdLs("yarg-lang-pico.uf2", ".", false)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	err = deviceimage.CmdCp("yarg-lang-pico.uf2", "stale_cheese.ya", "copy_of_fresh_cheese.ya")
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	err = deviceimage.CmdLs("yarg-lang-pico.uf2", ".", true)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	os.Chdir("../")
}

func TestMkdir(t *testing.T) {

	os.Chdir("testdata")
	deviceimage.CmdMkdir("yarg-lang-pico.uf2", "cheese")
	deviceimage.CmdLs("yarg-lang-pico.uf2", ".", true)
	os.Chdir("../")
}

func TestFormat(t *testing.T) {

	e := deviceimage.Cmdformat("testdata/yarg-lang-pico.uf2", false)
	if e != nil {
		t.Fatalf("unexpected error: %v", e)
	}

	e = deviceimage.Cmdformat("testdata/nonexistent.uf2", false)
	if e == nil {
		t.Fatalf("expected error, got nil")
	}
	if !os.IsNotExist(e) {
		t.Fatalf("expected not exist error, got %v", e)
	}

	e = deviceimage.Cmdformat("testdata/nonexistent.uf2", true)
	if e != nil {
		t.Fatalf("unexpected error: %v", e)
	}
	os.Remove("testdata/nonexistent.uf2")
}

var interpreter = flag.String("interpreter", "../bin/cyarg", "interpreter to use")

func TestRunTests(t *testing.T) {

	err, failedtests := testrunner.CmdRunTests(*interpreter, "testdata/tests")
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if failedtests != 0 {
		t.Fatalf("expected 0 failed tests, got %v", failedtests)
	}
}

func TestFileSequence(t *testing.T) {

	deviceimage.Cmdformat("testdata/yarg-lang-pico.uf2", false)
	os.Chdir("testdata")
	deviceimage.CmdCp("yarg-lang-pico.uf2", "fresh_cheese.ya", "fresh_cheese.ya")
	os.Chdir("../")

	deviceimage.CmdLs("testdata/yarg-lang-pico.uf2", ".", false)
}
