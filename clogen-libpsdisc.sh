#!/bin/bash

# this script does not support dirs with spaces.
# if your working dirs have spaces, shame on you.

mydir=$(dirname $(cygpath $(readlink -f ${BASH_SOURCE})))

target_dir=${1:-$mydir/../libpsdisc}

if [[ ! -e $target_dir ]]; then
    git clone https://github.com/piepacker/libpsdisc $target_dir || exit
fi

mswpath=$(cygpath -w $target_dir)
propsgendir=$mydir/msbuild/clogen

echo "Generating $propsgendir/libpsdisc.props"

mkdir -p $propsgendir
> $propsgendir/libpsdisc.props echo \
'<?xml version="1.0" encoding="utf-8"?>
<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup>
    <PATH_TO_LIBPSDISC Condition="'$(PATH_TO_LIBPSDISC)' == ''">'"$mswpath"'</PATH_TO_LIBPSDISC>
  </PropertyGroup>
</Project>
'