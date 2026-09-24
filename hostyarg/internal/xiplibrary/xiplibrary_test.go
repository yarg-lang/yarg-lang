package xiplibrary

import (
	"os"
	"path/filepath"
	"testing"
)

func DoTestHeaderPacking(t *testing.T) (name string, err error) {
	buffer, err := os.CreateTemp(t.TempDir(), "header")
	if err != nil {
		t.Fatal(err)
	}
	defer buffer.Close()

	name = buffer.Name()

	err = writeLibraryImageHeader(buffer, 30, 0)
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
	if header.Endianness != endiannessMarker {
		t.Fatalf("expected endianness %v, got %v", endiannessMarker, header.Endianness)
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

type TestFile struct {
	FileName     string
	ExpectedInfo FsInfo
}

var filetests = []TestFile{
	{
		FileName: "test.yarglib",
		ExpectedInfo: FsInfo{
			Version:          packageVersion,
			Size:             2645,
			NodeCount:        12,
			UsefulLength:     2623,
			DirectoryEntries: 4,
			IndexedEntries:   2,
		},
	},
	{
		FileName: "no-startup.yarglib",
		ExpectedInfo: FsInfo{
			Version:          packageVersion,
			Size:             94,
			NodeCount:        4,
			UsefulLength:     78,
			DirectoryEntries: 1,
			IndexedEntries:   0,
		},
	},
	{
		FileName: "empty.yarglib",
		ExpectedInfo: FsInfo{
			Version:          packageVersion,
			Size:             24,
			NodeCount:        1,
			UsefulLength:     8,
			DirectoryEntries: 0,
			IndexedEntries:   0,
		},
	},
	{
		FileName: "startup-test.yarglib",
		ExpectedInfo: FsInfo{
			Version:          packageVersion,
			Size:             110,
			NodeCount:        2,
			UsefulLength:     94,
			DirectoryEntries: 0,
			IndexedEntries:   1,
		},
	},
	{
		FileName: "startup-yarg-test.yarglib",
		ExpectedInfo: FsInfo{
			Version:          packageVersion,
			Size:             205,
			NodeCount:        3,
			UsefulLength:     187,
			DirectoryEntries: 0,
			IndexedEntries:   2,
		},
	},
}

func testAFile(t *testing.T, output *os.File, input string, expected FsInfo) {
	CmdBuildWithContents(input, output.Name())

	output.Seek(0, 0)
	output.Sync()

	info, e := os.Stat(output.Name())
	if e != nil {
		t.Fail()
	}

	data, _ := os.ReadFile(output.Name())

	xipinfo, e := readFsInfo(data)
	if e != nil {
		t.Fail()
	}

	if expected != xipinfo {
		t.Logf("expected: %+v, got: %+v", expected, xipinfo)
		t.Fail()
	}

	if xipinfo.Size != uint32(info.Size()) {
		t.Fail()
	}
}

func reallyTestAFile(t *testing.T, test TestFile) {
	input := filepath.Join("../../testdata/", test.FileName)

	t.Logf("test filename: %v", input)

	output, err := os.CreateTemp(t.TempDir(), "xipcmd")
	if err != nil {
		t.Fatal(err)
	}
	defer output.Close()

	name := output.Name()
	defer os.Remove(name)

	testAFile(t, output, input, test.ExpectedInfo)

}

func TestCmdBuildWithContents(t *testing.T) {

	for _, test := range filetests {
		t.Logf("Test: %v", test.FileName)
		reallyTestAFile(t, test)
	}
}
