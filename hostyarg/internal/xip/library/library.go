package library

type XIPLibrary struct {
	CommandPath  string
	IndexedFiles []Command
	NamedFiles   []Command
}

type Command interface {
	Execute(*XIPLibrary) error
}

func (lib *XIPLibrary) NodeCount() int {
	count := 1
	if len(lib.IndexedFiles) > 0 {
		count += len(lib.IndexedFiles)
	}
	if len(lib.NamedFiles) > 0 {
		count += 1
		count += len(lib.NamedFiles) * 2
	}
	return count
}

func (lib *XIPLibrary) IndexedFileCount() int {
	return len(lib.IndexedFiles)
}

func (lib *XIPLibrary) NamedFileCount() int {
	return len(lib.NamedFiles)
}

func (lib *XIPLibrary) TotalFileCount() int {
	return len(lib.IndexedFiles) + len(lib.NamedFiles)
}

func (lib *XIPLibrary) NamedFileNodes() int {
	if len(lib.NamedFiles) == 0 {
		return 0
	}
	return 1 + len(lib.NamedFiles)*2
}
