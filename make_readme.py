#!/bin/env python3

import subprocess
import sys

src = sys.argv[1]
tgt = sys.argv[2]

exes = sys.argv[3:]

def get_help(exe):
    result = subprocess.run([exe, '-h'], capture_output=True)
    assert result.returncode == 0
    return result.stderr.decode(sys.stdout.encoding)


data = {
    'BAR': get_help(exes[0]),
    'DMENU': get_help(exes[1]),
    'GRID_CLIENT': get_help(exes[2]),
    'GRID_SERVER': get_help(exes[3])
}

with open(src) as readme_in, open(tgt, 'w') as readme:
    for line in readme_in:
        for k,v in data.items():
            placeholder = 'HELP_OUTPUT_FOR_' + k
            if placeholder in line:
                print(placeholder)
            line = line.replace(placeholder, v)
        readme.write(line)
