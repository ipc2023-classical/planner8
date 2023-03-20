# Clingo: A grounder and solver for logic programs

Clingo is part of the [Potassco](https://potassco.org) project for *Answer Set
Programming* (ASP).  ASP offers a simple and powerful modeling language to
describe combinatorial problems as *logic programs*.  The *clingo* system then
takes such a logic program and computes *answer sets* representing solutions to
the given problem.  To get an idea, check our [Getting
Started](https://potassco.org/doc/start/) page and the [online
version](https://potassco.org/clingo/run/) of clingo.

Please consult the following resources for further information:

  - [**Downloading source and binary releases**](https://github.com/potassco/clingo/releases)
  - [Changes between releases](CHANGES.md)
  - [Documentation](https://github.com/potassco/guide/releases)
  - [Potassco clingo page](https://potassco.org/clingo/)

Clingo is distributed under the [MIT License](LICENSE.md).

## Contents of Linux Binary Release

The `clingo.exe` and `gringo.exe` binaries are compiled with Lua 5.3 but
without Python support.

- `clingo.exe`: solver for non-ground programs
- `gringo.exe`: grounder
- `clasp.exe`: solver for ground programs
- `reify.exe`: reifier for ground programs
- `lpconvert.exe`: translator for ground formats
