# Yarg Test

Yarg tests present in yarg/test exercise the complete Yarg stack. Currently these are run as an automated suite on host.

## hostyarg runtests

To run the test suite, first build the repo, and then:

```
$ ./bin/hostyarg runtests -tests "yarg/test" -interpreter "bin/cyarg"
Interpreter: bin/cyarg
Tests: yarg/test
Total tests: 1065, passed: 1065
```

This walks every folder in the supplied directory (yarg/test), using bin/cyarg to run every .ya file found in each directory. Currently a folder named 'benchmark' is skipped.

If some tests fail, their output is provided, and the return code from hostyarg will be the count of failing tests (ie, non-zero), eg:

```
% ./bin/hostyarg runtests -tests yarg/test -interpreter bin/cyarg
test: bool
tests supplied: 2
tests passed: 1
--- (70)
Undefined variable (OP_GET_GLOBAL) 'tru'.
[line 1] in script
---
Interpreter: bin/cyarg
Tests: yarg/test
Total tests: 1065, passed: 1064
% echo $?
1
```

The failing test can be located with `find`, and then run directly with `cyarg` to inspect the failure:

```
$ find . -name bool.ya
./yarg/test/call/bool.ya
$ ./bin/cyarg yarg/test/call/bool.ya
Undefined variable (OP_GET_GLOBAL) 'tru'.
[line 1] in script
```

## Test Script

An individual test script is written in Yarg, with comments that will be parsed (via regex) by the hostyarg harness, eg if `test.ya` contains:

```
print -0.0;    // expect: -0.00000
```

hostyarg executes cyarg, and captures its output and exit code, akin to:

```
$ cyarg test.ya
-0.00000
$ echo $?
0
```

The regex scan of comments is unware of Yarg, so will not usefully honour if statements, loops, etc, so this an example of how to test a loop:

```
while (a < 3) {
  print a;
  a = a + 1;
}
// expect: 0
// expect: 1
// expect: 2
```

Tests can be:

  * Output Tests
  * Compile Error Tests
  * Runtime Error Tests

A script consists of one or more output tests followed by an optional Error test. If included, only one Error test can be present in a script.

## Output Tests

Most tests expect successful exuction and a line of output, eg:

```
print 1; // expect: 1
```
If the statement creates multiple lines of output these can be added, and will be accounted as separate tests:

```
print test_sync();  // expect: Waiting for interrupts to be simulated - handler
// expect: done
// expect: Type:any[]:[]
```

A test script can contain any number of Output Tests.

## Compile Error Tests

if the test is validating the compiler, it will check for 'Error' appearing, with an optional line number if that is material, eg:

```
// [line 2] Error at ';': Expect expression.
print;
```

```
var a = "a";
!a = "value"; // Error at '=': Invalid assignment target.
```
The exit code from the interpreter is compared with COMPILE_ERROR (65), so a script can test at most one compile error.

## Runtime Error Tests

If a test expects a runtime error, 'expect runtime error:' will prefix the expected message:

```
var test = new(any[10]);
print test[10];  // expect runtime error: Array index 10 out of bounds.
```

The error message must match exactly for a test pass, along with the RUNTIME_ERROR (70) exitcode. Only one per script can be tested.

