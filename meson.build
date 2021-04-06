project(
    'lyra', 'c',
    license: 'Apache-2.0',
    default_options: [
        'warning_level=2',
        'werror=true',
    ]
)

prog_python = import('python').find_installation('python3')

sources = run_command(prog_python, [
    './build-scripts/meson_files.py'
]).stdout().split('\n')

liblyra = static_library('lyra', sources,
    include_directories: include_directories('./lyra'))
