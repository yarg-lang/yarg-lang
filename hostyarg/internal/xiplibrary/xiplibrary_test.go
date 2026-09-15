package xiplibrary

import (
	"os"
	"testing"
)

func DoTestHeaderPacking(t *testing.T) (name string, err error) {
	buffer, err := os.CreateTemp(t.TempDir(), "header")
	if err != nil {
		t.Fatal(err)
	}
	defer buffer.Close()

	name = buffer.Name()

	err = writeLibraryImageHeader(buffer, 30)
	if err != nil {
		t.Fatalf("writeLibraryImageHeader failed: %v", err)
	}

	data := make([]byte, 30)
	buffer.ReadAt(data, 0)

	header, err := readLibraryImageHeader(data)
	if err != nil {
		t.Fatalf("readLibraryImageHeader failed: %v", err)
	}

	if header.Magic != packageMagic {
		t.Fatalf("expected magic %v, got %v", packageMagic, header.Magic)
	}

	if header.Version != packageVersion {
		t.Fatalf("expected version %v, got %v", packageVersion, header.Version)
	}
	if header.Length != 30 {
		t.Fatalf("expected length 30, got %v", header.Length)
	}
	if header.NodeZeroOffset != 16 {
		t.Fatalf("expected NodeZeroOffset 16, got %v", header.NodeZeroOffset)
	}
	return name, nil
}
func TestHeaderPacking(t *testing.T) {

	name, err := DoTestHeaderPacking(t)
	if err != nil {
		t.Fatalf("DoTestHeaderPacking failed: %v", err)
	}
	err = os.Remove(name)
	if err != nil {
		t.Fatalf("Failed to remove temporary file: %v", err)
	}

}
