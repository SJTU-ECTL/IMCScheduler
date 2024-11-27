# IMCScheduler
SIMD IMC scheduler target at minimizing memory footprint

## Dependencies
In order to use the scheduler, you will need a Windows machine with the following three tools:

Mockturtle(https://github.com/lsils/mockturtle)

Gurobi(https://www.gurobi.com/)

Z3(https://github.com/Z3Prover/z3)

Please change the file path in utils.h according to your setting and add the include directories of the tools into your project.

## Run
To appy the scheduler on an XMG netlist (.v / .bliff / .aig file), please put the name of the file in benchmarks.txt (you can optionally add the bound for the netlist).

An example of int2float benchmark can be found in the files.
