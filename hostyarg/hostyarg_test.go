package hostyarg

import (
	"io"
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

func TestLs(t *testing.T) {

	fname := "testdata/yarg-lang-pico-ls.uf2"

	_, err := copyFile("testdata/yarg-lang-pico-0.3.0.uf2", fname)
	if err != nil {
		t.Fatalf("copyFile failed: %v", err)
	}
	defer os.Remove(fname)

	deviceimage.CmdLs(fname, ".", false)
	deviceimage.CmdLs(fname, ".", true)
}

func TestCopyFile(t *testing.T) {

	os.Chdir("testdata")
	defer os.Chdir("../")

	fname := "yarg-lang-pico-copy.uf2"
	_, err := copyFile("yarg-lang-pico-0.3.0.uf2", fname)
	if err != nil {
		t.Fatalf("copyFile failed: %v", err)
	}
	defer os.Remove(fname)

	err = deviceimage.CmdCp(fname, "fresh_cheese.ya", "copy_of_fresh_cheese.ya")
	if err != nil {
		t.Fatalf("deviceimage.CmdCp unexpected error: %v", err)
	}
	err = deviceimage.CmdLs(fname, ".", false)
	if err != nil {
		t.Fatalf("deviceimage.CmdLs unexpected error: %v", err)
	}
	err = deviceimage.CmdCp(fname, "stale_cheese.ya", "copy_of_fresh_cheese.ya")
	if err != nil {
		t.Fatalf("deviceimage.CmdCp unexpected error: %v", err)
	}
	err = deviceimage.CmdLs(fname, ".", true)
	if err != nil {
		t.Fatalf("deviceimage.CmdLs unexpected error: %v", err)
	}
}

func TestMkdir(t *testing.T) {

	os.Chdir("testdata")
	defer os.Chdir("../")

	fname := "yarg-lang-pico-md.uf2"
	_, err := copyFile("yarg-lang-pico-0.3.0.uf2", fname)
	if err != nil {
		t.Fatalf("copyFile failed: %v", err)
	}
	defer os.Remove(fname)

	deviceimage.CmdMkdir(fname, "cheese")
	deviceimage.CmdLs(fname, ".", true)
}

func TestFormat(t *testing.T) {

	fname := "testdata/yarg-lang-pico-format.uf2"
	_, err := copyFile("testdata/yarg-lang-pico-0.3.0.uf2", fname)
	if err != nil {
		t.Fatalf("copyFile failed: %v", err)
	}

	e := deviceimage.Cmdformat(fname, false)
	if e != nil {
		os.Remove(fname)
		t.Fatalf("unexpected error: %v", e)
	}

	os.Remove(fname)

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

	fname := "yarg-lang-pico-sequence.uf2"
	_, err := copyFile("testdata/yarg-lang-pico-0.3.0.uf2", "testdata/"+fname)
	if err != nil {
		t.Fatalf("copyFile failed: %v", err)
	}
	defer os.Remove("testdata/" + fname)

	deviceimage.Cmdformat("testdata/"+fname, false)
	os.Chdir("testdata")
	deviceimage.CmdCp(fname, "fresh_cheese.ya", "fresh_cheese.ya")
	os.Chdir("../")

	deviceimage.CmdLs("testdata/"+fname, ".", false)
}
