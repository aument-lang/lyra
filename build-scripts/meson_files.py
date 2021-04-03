# This source file is part of the Aument language
# Copyright (c) 2021 the aument contributors
#
# Licensed under Apache License v2.0 with Runtime Library Exception
# See LICENSE.txt for license information

import glob
import os
all_files = []
for root, _, files in os.walk("src/"):
    for file in files:
        if file.endswith(".h") or file.endswith(".c"):
            path = os.path.join(root, file)
            all_files.append(path.replace("\\","/"))
all_files.sort()
print('\n'.join(all_files), end='')
