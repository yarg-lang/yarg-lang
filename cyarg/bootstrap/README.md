The files *.yb; *.ylib and *.c in this directory are synthesised and committed
to the repo to break the circular dependency on cyarg as a compiler that
must compile it's own bootstrap.

The tool ./tools/build-cyarg-bootstrap.sh can be used to refresh them.
