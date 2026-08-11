package hostyarg

import (
	"io"
	"log"
	"os"
	"testing"

	"github.com/yarg-lang/yarg-lang/hostyarg/internal/deviceimage"
)

func copyFile(src, dst string) (int64, error) {
	sourceFile, err := os.Open(src)
	if err != nil {
		return 0, err
	}
	defer sourceFile.Close()

	destinationFile, err := os.Create(dst)
	if err != nil {
		return 0, err
	}
	defer destinationFile.Close()

	return io.Copy(destinationFile, sourceFile)
}

func TestMain(m *testing.M) {
	log.SetOutput(io.Discard)

	_, err := copyFile("testdata/yarg-lang-pico-0.3.0.uf2", "testdata/yarg-lang-pico.uf2")
	if err != nil {
		log.Fatalf("copyFile failed: %v", err)
	}
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
		t.Fatalf("deviceimage.CmdCp unexpected error: %v", err)
	}
	err = deviceimage.CmdLs("yarg-lang-pico.uf2", ".", false)
	if err != nil {
		t.Fatalf("deviceimage.CmdLs unexpected error: %v", err)
	}
	err = deviceimage.CmdCp("yarg-lang-pico.uf2", "stale_cheese.ya", "copy_of_fresh_cheese.ya")
	if err != nil {
		t.Fatalf("deviceimage.CmdCp unexpected error: %v", err)
	}
	err = deviceimage.CmdLs("yarg-lang-pico.uf2", ".", true)
	if err != nil {
		t.Fatalf("deviceimage.CmdLs unexpected error: %v", err)
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

func TestFileSequence(t *testing.T) {

	deviceimage.Cmdformat("testdata/yarg-lang-pico.uf2", false)
	os.Chdir("testdata")
	deviceimage.CmdCp("yarg-lang-pico.uf2", "fresh_cheese.ya", "fresh_cheese.ya")
	os.Chdir("../")

	deviceimage.CmdLs("testdata/yarg-lang-pico.uf2", ".", false)
}
