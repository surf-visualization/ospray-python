#!/usr/bin/env python
import sys, getopt, os
scriptdir = os.path.split(__file__)[0]
sys.path.insert(0, os.path.join(scriptdir, '..'))

import ospray

ospray.init([])

print('Compile-time version:', ospray.VERSION)
print('Run-time version:', ospray.version())
