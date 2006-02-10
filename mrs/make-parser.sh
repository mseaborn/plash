#!/bin/sh

set -e

(echo '#include "region.h"';
 echo '#include "shell.h"';
 echo '#include "shell-variants.h"';
 ~/projects/parsing/c_compile_grammar --extra-arg r 'region_t r' shell.gram
) >shell-parse.c
