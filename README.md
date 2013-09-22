# Tny: A simple data serializer in C

Tny is a simple library to serialize data in C. It can be seen as
a kind of binary JSON but unlike BSON it also supports arrays as root elements.

It was designed to be simple and easy to use. If you want to know how to use it, take a look
at src/tny/tny.h or the tests located at src/tests.c

## Documentation

The documentation can be found in [doc/](https://github.com/BobMarlon/Tny/tree/master/doc),
or read it [online](http://bobmarlon.github.io/Tny/pages/).

### Binary Format

If you want to now how Tny serializes data, take a look at the Documentation at the [tny.h File Reference](http://bobmarlon.github.io/Tny/pages/tny_8h.html) "Detailed Description" section.
There you find an ABNF specification of the binary format.

## System Requirements

Tny should run on every plattform with a compatible C99 compiler.
No additional libraries are required.

## License

This software is distributed under [MIT license](http://www.opensource.org/licenses/mit-license.php),
so feel free to use it for everything you want.
