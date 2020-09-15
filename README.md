OCTASOX
-------

This tool will ingest Octatrack slices from `.ot` files and generate
[sox](http://sox.sourceforge.net/) command line arguments that
chop the original sample accordingly.

The `.ot` file loading code is based on [OctaChainer](https://github.com/KaiDrange/OctaChainer).

To compile, any C++ 11 compliant compiler should work.

```shell
mkdir build
cd build
cmake ..
make
```

Test this by running
```shell
./octasox path/to/otfile.ot
```

The tool will print lines that look like this:

```shell
AmnKC.wav AmnKC00.wav trim 126s =8872s
```

Passing this as arguments to `sox` will load the 
sample `AmnKC.wav`, discard everything outside the range `126`...`8872` 
(measured in samples, not seconds), and store the result in `AmnKC00.wav`.
Similarly for the other lines.

To go ahead with chopping the samples, you can use `xarg` to build
command lines:

```shell
./octasox path/to/otfile.ot | xargs -n5 sox
```

This will pipe all of the output into `xargs`, which takes chunks
of five elements and passes them to the `sox` command.




