#!/bin/bash

# this script does not support dirs with spaces.
# if your working dirs have spaces, shame on you.

mydir=$(dirname $(cygpath $(readlink -f ${BASH_SOURCE})))

target_dir=${1:-$mydir/../icystdlib}

if [[ ! -e $target_dir ]]; then
    git clone https://github.com/jstine35/icystdlib $target_dir || exit
fi

mswpath=$(cygpath -w $target_dir)
propsgendir=$mydir/msbuild/clogen

echo "Generating $propsgendir/icystdlib.props"

mkdir -p $propsgendir
> $propsgendir/icystdlib.props echo \
'<?xml version="1.0" encoding="utf-8"?>
<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup>
    <PathToIcyStdlib>'"$mswpath"'</PathToIcyStdlib>
  </PropertyGroup>
</Project>
'