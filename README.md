# fswatch

This is a small program using the events a directory.
When an event about any change to that directory is received, the specified
shell command is executed by `/bin/bash`.

[fswatch](https://github.com/alandipert/fswatch) of notifytools use version.

## Build

required cmake.

```
% mkdir build && cd build
% cmake ..
% make
% make install
```

## Usage

```
% fswatch /some/dir "echo changed"
```

This would monitor `/some/dir` for any change, and run `echo changed`
when a modification event is received.

In the case you want to watch multiple directories, just separate them
with colons like:

```
% fswatch /some/dir:/some/otherdir "echo changed"
```
