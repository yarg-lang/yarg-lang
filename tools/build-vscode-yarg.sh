#!/bin/bash

VSCODEEXTENSION_VERSION=0.3.0
BUILD_ERROR=0

pushd vscode-yarg
vsce package || BUILD_ERROR=1
mv yarg-lang-$VSCODEEXTENSION_VERSION.vsix ../build/
popd

exit $BUILD_ERROR