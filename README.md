# JavaSharp

**JavaSharp** is a command-line source-to-source converter that translates Java source files (`.java`) into C# source files (`.cs`). It is implemented in C++17 and includes prebuilt command-line binaries for several 64-bit platforms.

- **Version:** 1.0.0
- **Language:** C++17
- **License:** Apache License 2.0

## Features

JavaSharp parses a Java compilation unit, performs lightweight type and declaration analysis, and emits corresponding C# source code.

The parser recognizes a range of Java language constructs, including:

- Classes, interfaces, enums, annotation declarations, and records.
- Fields, methods, constructors, nested types, and local classes.
- Inheritance, implemented interfaces, generic type parameters, and arrays.
- Control-flow statements such as `if`, `for`, enhanced `for`, `while`, `do-while`, and `switch`.
- `try`/`catch`/`finally`, try-with-resources, `throw`, `synchronized`, and `assert`.
- Lambdas, method references, anonymous classes, conditional expressions, and `instanceof` patterns.
- Switch expressions and arrow-style switch labels.
- Java comments and selected annotations.

It also includes mappings for common Java types and APIs to .NET equivalents. Examples include:

| Java | Generated C# equivalent |
| --- | --- |
| `String` | `string` |
| `Object` | `object` |
| `boolean` | `bool` |
| `System.out` | `Console.Out` |
| `ArrayList` / `List` | `List` |
| `HashMap` / `Map` | `Dictionary` |
| `HashSet` / `Set` | `HashSet` |
| `BigDecimal` | `decimal` |
| `BigInteger` | `BigInteger` |
| `LocalDate` | `DateOnly` |
| `LocalTime` | `TimeOnly` |
| `LocalDateTime` | `DateTime` |

Some Java library types are translated to .NET types, and the converter can emit small helper implementations for selected Java APIs when needed.

> **Compatibility note:** JavaSharp is a source converter, not a complete Java compiler or a full implementation of the Java standard library. It writes C# source files; it does not create a `.csproj`, resolve project dependencies, compile, or run the generated code. Java and .NET APIs do not always have identical behavior. Review generated code, resolve any project-specific dependencies, and compile and test the result in your target .NET environment.

## Quick start

### Use a prebuilt binary

Prebuilt executables are provided in the `build/` directory:

| Platform | Executable |
| --- | --- |
| Windows x64 | `build/javasharp-windows-x64.exe` |
| Linux x64 | `build/javasharp-linux-x64` |
| macOS x64 | `build/javasharp-macos-x64` |
| macOS ARM64 | `build/javasharp-macos-arm64` |
| Android ARM64 | `build/javasharp-android-arm64` |

For example, on Linux:

```bash
chmod +x build/javasharp-linux-x64
./build/javasharp-linux-x64 --version
```

On Windows, run the executable from PowerShell:

```powershell
.\build\javasharp-windows-x64.exe --version
```

### Convert a Java file

```bash
./build/javasharp-linux-x64 --convert HelloWorld.java to HelloWorld.cs
```

The output path's parent directories are created when necessary. If the output file already exists, it is overwritten.

## Command-line usage

```text
javasharp --convert <input.java> to <output.cs> [--convert <input.java> to <output.cs> ...]
```

### Options

| Option | Description |
| --- | --- |
| `--convert <in.java> to <out.cs>` | Convert one Java source file into one C# source file. This option can be repeated. |
| `--version`, `-v` | Print the JavaSharp version. |
| `--help`, `-h` | Print command-line help. |

### Convert multiple files

Pass an additional `--convert` pair for each file:

```bash
javasharp \
  --convert src/Main.java to out/Main.cs \
  --convert src/Utils.java to out/Utils.cs
```

Each conversion is processed as an individual source unit. JavaSharp does not build a project-wide compilation model across multiple input files.

## Example

Given the following `HelloWorld.java`:

```java
package demo;

public class HelloWorld {
    public static void main(String[] args) {
        System.out.println("Hello, World!");
    }
}
```

Run:

```bash
javasharp --convert HelloWorld.java to HelloWorld.cs
```

JavaSharp produces:

```csharp
using System;
using System.Collections.Generic;

namespace demo
{
    public class HelloWorld
    {
        public static void Main(string[] args)
        {
            Console.WriteLine("Hello, World!");
        }
    }
}
```

The converter maps the Java package to a C# namespace, changes the Java entry-point method name from `main` to `Main`, maps `String` to `string`, and translates `System.out.println` to `Console.WriteLine`.

## Build from source

### Requirements

- A C++17-compatible compiler.
- The C++ standard library and filesystem support provided by that compiler.
- For the macOS cross-build commands below: Python and the `ziglang` package.
- For the Android ARM64 build: Android NDK.

### Windows and Linux

From the `src/` directory, build with GCC or MinGW-w64:

```bash
g++ -std=c++17 -O2 -s -Wall -Wextra -o javasharp main.cpp
```

On Windows, run the resulting executable as `javasharp.exe` (or the filename produced by your compiler).

### macOS cross-compilation

Install Zig:

```bash
python -m pip install ziglang
```

From the `src/` directory, build for macOS x64:

```bash
python -m ziglang c++ -target x86_64-macos.10.15 -std=c++17 -O2 -s -o javasharp-macos-x64 main.cpp
```

Build for macOS ARM64:

```bash
python -m ziglang c++ -target aarch64-macos.11.0 -std=c++17 -O2 -s -o javasharp-macos-arm64 main.cpp
```

### Android ARM64 cross-compilation

The supplied build command uses Android NDK `26.1.10909125`. From the `src/` directory, run it in Linux or WSL:

```bash
$HOME/android-sdk/ndk/26.1.10909125/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android28-clang++ \
  -std=c++17 -O2 -DNDEBUG -ffunction-sections -fdata-sections \
  -static-libstdc++ -Wl,--gc-sections -s \
  main.cpp -o javasharp-android-arm64
```

If your NDK is installed at a different version or location, update the compiler path accordingly.

### Build metadata

The included `build/versions.txt` records the toolchains used for the supplied binaries:

- Windows: MinGW-w64 GCC 15.2.0 (MSYS2).
- Linux: GCC 13.3.0 from Ubuntu 24.04.
- macOS x64 and ARM64: Zig 0.17.1.
- Android ARM64: Android NDK 26.1.10909125.

## Project structure

```text
javasharp/
├── README.md
├── LICENSE
├── updates.txt
├── build/
│   ├── how-to-build.txt
│   ├── versions.txt
│   ├── javasharp-windows-x64.exe
│   ├── javasharp-linux-x64
│   ├── javasharp-macos-x64
│   ├── javasharp-macos-arm64
│   └── javasharp-android-arm64
└── src/
    ├── main.cpp
    ├── js_base.hpp
    ├── js_parser.hpp
    ├── js_semantic.hpp
    ├── js_emit.hpp
    └── onefile/
        └── js.cpp
```

### Source files

- `src/main.cpp` — command-line entry point, file handling, and conversion orchestration.
- `src/js_base.hpp` — shared data structures, lexer, language/type mappings, and common utilities.
- `src/js_parser.hpp` — Java parser and syntax-tree construction.
- `src/js_semantic.hpp` — declaration collection, type classification, and semantic transformations.
- `src/js_emit.hpp` — C# code generation.
- `src/onefile/js.cpp` — amalgamated single-translation-unit version of the implementation.

## Diagnostics and exit codes

JavaSharp prints conversion progress to standard output and reports parsing or file errors to standard error.

| Exit code | Meaning |
| --- | --- |
| `0` | Conversion succeeded without reported syntax errors. |
| `1` | C# output was written, but syntax errors were reported for the Java input. Review the diagnostics and generated file. |
| `2` | Invalid command-line usage, or an input/output file error. |

Syntax diagnostics include the source line number. A conversion that returns `1` may still produce an output file, so the generated code should be reviewed before use.

## License

JavaSharp is distributed under the [Apache License, Version 2.0](LICENSE).

Copyright 2026 JafStudio.
