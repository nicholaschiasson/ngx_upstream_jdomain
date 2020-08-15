#!/usr/bin/env bash

set -ex

find src -name *.c | while read f
do
	echo ${f}
	diff ${f} <(clang-format ${f})
done
