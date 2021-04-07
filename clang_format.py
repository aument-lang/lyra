import os
import subprocess

files = list(os.walk("lyra"))
for root, _, files in files:
    for file in files:
        if file.endswith(".h") or file.endswith(".c"):
            subprocess.run([
                "clang-format",
                "-style=file",
                "-i",
                os.path.join(root,file)
            ])
