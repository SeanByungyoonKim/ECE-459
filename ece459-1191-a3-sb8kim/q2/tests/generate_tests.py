import os.path
import random

PATH = os.path.dirname(__file__) + '/q2/tests/'
FILE_NAME = 'test-'
PARTICLE_COUNT = [500, 5000]
ELECTRON_PERCENT = 0.2
MIN_POINT = -10
MAX_POINT = 10
EXP = 'e-09'


def format_float(val):
    if val < 0:
        return f'{val:.5f}{EXP}'
    else:
        return f'{val:.5f}{EXP}'


def get_random_point():
    return random.uniform(MIN_POINT, MAX_POINT)


def gen_particle(is_proton):
    x = get_random_point()
    y = get_random_point()
    z = get_random_point()
    particle = 'p' if is_proton else 'e'
    return f'{particle},{format_float(x)},{format_float(y)},{format_float(z)}'


def write_file(particles):
    full_path = f'{PATH}{FILE_NAME}{particles}.in'
    with open(full_path, 'w') as f:
        electron_count = int(particles * ELECTRON_PERCENT)
        proton_count = particles - electron_count
        for _ in range(0, proton_count):
            f.write(f'{gen_particle(True)}\n')
        for _ in range(0, electron_count):
            f.write(f'{gen_particle(False)}\n')

if __name__ == '__main__':
    for i in PARTICLE_COUNT:
        write_file(i)