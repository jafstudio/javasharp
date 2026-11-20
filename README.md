# JavaSharp

**JavaSharp** — a console source-to-source transpiler: it translates programs from Java to C#. Written in C++17, it builds into a single executable with no external dependencies. The current version is **1.0.0**.

## How it works

The converter goes through four stages implemented in pluggable headers:

1. **Lexer** — tokenizes the `.java` file and collects comments;
2. **Parser** (`js_parser.hpp`) — builds the syntax tree of a compilation unit (classes, interfaces, enums, records, methods, expressions);
3. **Semantic analysis** (`js_semantic.hpp`) — infers types, resolves method overrides, finds functional interfaces (SAM) for lambdas, and collects information about records and anonymous classes;
4. **C# generation** (`js_emit.hpp`) — prints the finished `.cs` file with correct indentation, `using` directives, and type casts.

## Features

- **Java standard library to .NET mapping**: collections (`ArrayList`/`Vector` → `List<T>`, `HashMap` → `Dictionary<K,V>`, `HashSet` → `HashSet<T>`, `TreeMap` → `SortedDictionary<K,V>`, `Queue`/`Deque` → `LinkedList<T>`, `Iterator`/`Iterable`/`Stream` → `IEnumerable`/`IEnumerator`), I/O (`java.io`, `java.nio.file` → `System.IO`), date and time (`java.time`), networking (`java.net`), math (`java.math` → `System.Numerics`).
- **Type and exception mapping**: `boolean` → `bool`, wrapper types (`Integer` → `int?`, etc.), `String` → `string`, `Object` → `object`; `IndexOutOfBoundsException` → `IndexOutOfRangeException`, `NumberFormatException` → `FormatException`, `NullPointerException` → `NullReferenceException`, and dozens more.
- **Member and method renaming**: `System.out.println` → `Console.WriteLine`, `size()` → `Count`, `super` → `base`, the `main` method → `Main`, record components are converted to `PascalCase`.
- **Lambdas and anonymous classes**: functional interfaces (`Supplier`, `Function`, `Consumer`, etc.) become `Func`/`Action` delegates; anonymous classes are extracted into generated nested classes (`__Anon0`, `__Anon1`, …).
- **Language constructs**: enums, interfaces, switch, foreach, records, the `instanceof` operator, `var`, and insertion of explicit numeric type casts by rank.
- **Batch conversion**: a single run can convert several files by repeating the `--convert` pairs.
- **Error resilience**: if the input file contains syntax errors, conversion continues, and the exit code reports a partial result.

## Usage

```
javasharp --convert <input.java> to <output.cs> [--convert <input.java> to <output.cs> ...]
```

| Option | Description |
| --- | --- |
| `--convert <in.java> to <out.cs>` | Convert a Java file to C# (the pair may be repeated) |
| `--version`, `-v` | Show version |
| `--help`, `-h` | Show help |

Exit codes: `0` — conversion completed without errors; `1` — the input file contained syntax errors (the result is partial); `2` — usage error or file read/write error.

## Example

Input file `Main.java`:

```java
import java.util.ArrayList;
import java.util.List;

public class Main {
    public static void main(String[] args) {
        List<String> names = new ArrayList<>();
        names.add("Alice");
        names.add("Bob");
        for (String n : names) {
            System.out.println("Hello, " + n + "!");
        }
        System.out.println("Count: " + names.size());
    }
}
```

Run:

```
javasharp --convert Main.java to Main.cs
```

Result `Main.cs`:

```csharp
using System;
using System.Collections.Generic;

public class Main
{
    public static void Main(string[] args)
    {
        List<string> names = new List<string>();
        names.Add("Alice");
        names.Add("Bob");
        foreach (string n in names)
        {
            Console.WriteLine("Hello, " + n + "!");
        }
        Console.WriteLine("Count: " + names.Count);
    }
}
```

## Prebuilt binaries

The `build/` directory contains prebuilt binaries:

| File | Platform |
| --- | --- |
| `javasharp-windows-x64.exe` | Windows x64 |
| `javasharp-linux-x64` | Linux x64 |
| `javasharp-macos-x64` | macOS x64 (10.15+) |
| `javasharp-macos-arm64` | macOS ARM64 (11.0+) |
| `javasharp-android-arm64` | Android ARM64 |

On Linux/macOS, remember to make the file executable: `chmod +x javasharp-linux-x64`.

## Building from source

The sources are located in `src/`. A single file, `main.cpp`, which includes the entire chain of headers, is compiled:

```bash
# Windows (from PowerShell, MinGW-w64) or Linux:
g++ -std=c++17 -O2 -s -Wall -Wextra -o javasharp main.cpp
```

Cross-compilation (see `build/how-to-build.txt`):

```bash
# macOS x64 / macOS ARM64 — via ziglang (pip install ziglang):
python -m ziglang c++ -target x86_64-macos.10.15  -std=c++17 -O2 -s -o javasharp-macos-x64 main.cpp
python -m ziglang c++ -target aarch64-macos.11.0 -std=c++17 -O2 -s -o javasharp-macos-arm64 main.cpp

# Android ARM64 — via NDK (adjust the path to your NDK version):
$ANDROID_NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android28-clang++ \
    -std=c++17 -O2 -DNDEBUG -ffunction-sections -fdata-sections \
    -static-libstdc++ -Wl,--gc-sections -s main.cpp -o javasharp-android-arm64
```

The toolchain used to build the binaries in `build/` is listed in `build/versions.txt` (MinGW-w64 GCC 15.2.0, GCC 13.3.0, ziglang 0.17.1, NDK 26.1.10909125).

## Project structure

```
.
├── LICENSE                  # Apache License 2.0
├── updates.txt              # changelog
├── build/
│   ├── how-to-build.txt     # build commands for all platforms
│   ├── versions.txt         # build tool versions
│   └── javasharp-*          # prebuilt binaries (5 platforms)
└── src/
    ├── main.cpp             # CLI, conversion driver, expression generation
    ├── js_base.hpp          # basic utilities, type mapping tables
    ├── js_parser.hpp        # Java lexer and parser
    ├── js_semantic.hpp      # semantic analysis
    ├── js_emit.hpp          # C# code generation
    └── onefile/
        └── js.cpp           # everything in one file (standalone build)
```

## License

The project is distributed under the [Apache License 2.0](LICENSE).
