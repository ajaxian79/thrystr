# Value mappers

Value mappers run before bytes are wrapped into the final scalar range.

The stack supports add, subtract, multiply, and divide operations. After the
custom stack runs, values are cast to unsigned, reduced modulo 256, and mapped
onto the display range.
