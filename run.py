#!/usr/bin/env python3
import os
import sys
import pyperclip

base_dir = sys.path[0]
if sys.platform.startswith('win'):
    is_win = True
    dir_char = '\\'
else:
    is_win = False
    dir_char = '/'
base_dir += dir_char

config = {
    'compile_tool': ('gcc -std=c11', ''),
    'compile_filename': base_dir + 'main.c',
    'executable_filename': base_dir + 'cmake-build-debug' + dir_char + 'OS',
    'input_file': base_dir + 'cmake-build-debug' + dir_char + 'input.txt'
}


def run(use_txt=False, executable_file=config['executable_filename']):
    cmd = executable_file + ' '
    if argv:
        cmd += ' '.join(argv)
    cmd += (' < ' + input_file if use_txt else '')
    os.system(cmd)


def red_col(string):
    if is_win:
        return string
    return '\033[1;31m' + string + '\033[0m'


def blue_col(string):
    if is_win:
        return string
    return '\033[1;34m' + string + '\033[0m'


if __name__ == '__main__':
    cmd_ls = ['-b', '-r', '-br', '-h', '-f', '-if', '-paste']
    to_build = '-b' in sys.argv or '-br' in sys.argv
    to_run = '-r' in sys.argv or '-b' not in sys.argv
    filename = config['compile_filename']
    flag = False
    input_file = config['input_file']
    if '-h' in sys.argv:
        print(blue_col('usage: run.py:\n') +
              '  * ' + blue_col('build or run:\n') +
              '    # ' + red_col('[ -b ]') + ' : ' + blue_col('build\n') +
              '    # ' + red_col('[ -r ]') + ' : ' + blue_col('run\n') +
              '    # ' + red_col('[ -br]') + ' : ' + blue_col('build and run\n') +
              blue_col('    (it will run if neither of commands in "build or run")\n') +
              '  * ' + red_col('[ -i ]') + ' : ' + blue_col('use input.txt as input\n') +
              '  * ' + red_col('[ -if  *.* ]') + ' : ' + blue_col('set input file(*.*) as input\n') +
              '  * ' + red_col('[-if -paste]') + ' : ' + blue_col('use Clipboard content as input\n') +
              '  * ' + red_col('[ -f  *.cpp]') + ' : ' + blue_col('set build file as *.cpp\n') +
              '  * ' + red_col('[ -h ]') + ' : ' + blue_col('help\n') +
              '  * ' + red_col('[ *  ]') + ' : ' + blue_col('add parameters for program\n') +
              '  * ' + blue_col('Modify config to adjust default configuration'))
        exit(0)
    if '-f' in sys.argv:
        index = sys.argv.index('-f')
        if index == len(sys.argv) - 1:
            print(red_col('ERROR: No file with -f'))
            exit(-1)
        filename = sys.argv[index + 1]
        if not os.path.exists(filename):
            print(red_col('ERROR: No such file:%s' % filename))
            exit(-1)
        if not filename.endswith('.cpp') and not filename.endswith('.c'):
            print(red_col("ERROR: %s is not an C/CPP file" % filename))
            exit(-1)
        flag = True
    if '-if' in sys.argv:
        index = sys.argv.index('-if')
        if index == len(sys.argv) - 1:
            print(red_col('ERROR: No file with -if'))
        tmp_file = sys.argv[index + 1]
        if tmp_file == '-paste':
            with open(base_dir + 'cmake-build-debug' + dir_char + 'input.txt', 'w') as f:
                f.write(pyperclip.paste())
        else:
            input_file = tmp_file
            if not os.path.exists(input_file):
                print(red_col('ERROR: No such file:%s' % input_file))
                exit(-1)
    o_file = config['executable_filename']
    if to_build:
        if flag:
            o_file = base_dir + filename.split(dir_char)[-1].split('.')[0]
        os.system(config['compile_tool'][0] + ' ' + filename + ' -o ' + o_file + ' ' + config['compile_tool'][1])
    if to_run:
        argv = []
        add_flag = True
        for i in enumerate(sys.argv, 1):
            if i[1] not in cmd_ls and add_flag:
                argv.append(i[1])
            if not add_flag:
                add_flag = True
            if i[1] == '-f' or i[1] == '-if':
                add_flag = False
        run('-i' in sys.argv or '-if' in sys.argv, o_file)
    if flag:
        os.remove(o_file)