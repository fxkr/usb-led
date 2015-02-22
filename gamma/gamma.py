"""
Calculates a gamma correction table with the following properties.

- Adjustable number of bits of input/output.
- Only one entry maps to 0x00.
- Uses the full intensity range.

Having even just one more bit for inputs than for outputs increases
the output value coverage significantly, for example from 128 to 252
for 8/9 bit inputs and 8 bit outputs.
"""

import itertools
import math
import sys


# Gamma exponent. 2.5 is a good choice.
GAMMA = 2.5

# Number of input values. Should be a power of two.
STEPS = 2**9

# Highest output value. Should be one less than a power of two.
MAXIMUM = 2**8 - 1

# Maps fractions to output values.
# Start with highest desired output value.
# If the last entry in the generated table is less than the highest output value,
# try increasing it.
MULTIPLIER = 256


# Gamma correction
gamma = lambda step, steps: ((step/float(steps)) ** GAMMA) * MULTIPLIER

# Determine best range (elide all but one initial 0x00 outputs)
for null_steps in itertools.count():
    steps = STEPS - 1 + null_steps
    if int(round(gamma(null_steps, steps))) > 0:
        break
else:
    assert False

# Build table
float_table = [0.0]
table = [0]
for step in range(null_steps, steps):
    value = gamma(step, steps)
    float_table.append(value)
    table.append(int(round(value)))

# Convert to C code
print("uint8_t gamma[%i] = {" % len(table))
line_width = 8
num_width = int(math.ceil(math.log(table[-1], 2**4)))
line_format = " " * 2 + ", ".join(["0x%%0%ix" % num_width] * line_width) + "," + " // 0x%03x"
for line_begin in range(0, len(table), line_width):
    line = tuple(table[line_begin:line_begin+line_width]) + (line_begin,)
    print(line_format % line)
print("}")

# Check correctness
last_i = -1
for i in table:
    assert i >= last_i
    last_i = i
assert table[1] > 0

# Show statistics
sys.stderr.write("Elided null outputs: %i\n" % (null_steps-1))
sys.stderr.write("Maximum output: %i\n" % table[-1])
sys.stderr.write("Unique output: %i\n" % len(set(table)))

