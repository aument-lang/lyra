project('lyra', 'c')

prog_python = import('python').find_installation('python3')

sources = run_command(prog_python, [
    './build-scripts/meson_files.py'
]).stdout().split('\n')

executable('lyra-test', sources)
