#!/usr/bin/env python
with open('ospdatatypes.txt', 'r') as f:
    for line in f:
        s = line.strip()
        if s == '':
            continue
        print('.value("%s", OSPDataType::%s)' % (s, s))