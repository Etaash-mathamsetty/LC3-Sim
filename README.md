# LC3-Sim

A single file LC-3 emulator and debugger that supports all the operations that the offical emulator does (and has a more accurate implementation of putsp trap).

Compile with `gcc lc3sim.c -o lc3sim`

By default the emulator runs in execute mode, there is no way to debug anything or influence code execution in any way.

Input parameters:

`./lc3sim <parameters here>`

`--debug`: Enables the built-in debugger  
`--randomize`: Randomizes the registers  
`--input=$''`: Input string for `GETC` and `IN`  
`--dump=0xdead,0xeceb`: Dump memory addresses upon termination of emulator  
`--silent`: Don't print anything (except memory addresses if specified). Useful for test automation  
`--memory=0x4001,0x34`: in pairs. First parameter is address and second parameter is the value the address is set to.

Any parameter without a `--` will be interpreted as a file and the last file is the program that will be executed. 

Example usage: `./lc3sim --debug --randomize --dump=0xdead --memory=0x6767,0x32 --input=$'.@Etaash\n' font_data.obj lab13.obj`  

In this example `font_data.obj` is loaded into memory and PC is set to the `.ORIG` of `lab13.obj` and code execution begins there once the emulator leaves supervisor mode.  
  
You can also load a custom OS this way if you wish. 
