# psdisctool - a PS1/PS2 Disc Filesystem tool

# Overview

Provides automation-friendly PS1/PS2 disc filesystem tools:
 - identify disc media types (iso/bin)
 - list filesystem directories
 - extract files
 - _(coming soon)_ modify files
 - _(coming soon)_ advanced media type optimizations

# Usage Samples

Latest usage info is usually best obtained from the tool itself:

```
$ ./psdisctool --help
```

_(todo: add usage samples)_

# Developer Section (clone, build, etc)

## Clone and Build

### Dependencies

Git repository dependencies for this project are:

 - libpsdisc
 - icystdlib

Dependencies can be cloned out as either git submodules or as external individual clones, depending
on what best suits your workflow. For basic development, git submodules are recommended:

```
$ git submodule update --init
```

#### advanced dependency cloning (clogen)

Probably you don' want to use this. It's something I make use of as a developer of `icyStdLib` but for the most part
git submodules should always be preferred. Individual clones can be initialized by running the `clogen-` scripts:

```
# default [path-to-clone] is ../projname, eg. ../icystdlib and ../libpsdisc

 $ ./clogen-libpsdisc.sh [path-to-clone]
 $ ./clogen-icystdlib.sh [path-to-clone]
```

These scripts generate configuration files in the `msbuild/clogen/` dir, which tell the `vcxproj` where to find the
files and includes for these dependencies. BWhen the clogen files do not exist, the system assumes git submodule
mode. If submodules have not been initialized, you will get build errors for missing content.

_(todo: add a message to msbuild indicating actions to resolve missing dependencies)_
