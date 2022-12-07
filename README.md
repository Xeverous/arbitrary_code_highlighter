# Arbitrary Code Highlighter

A tool that given source code and additional information, outputs it embedded in HTML `<span>` tags containing classes for CSS highlight. [GeSHi](http://qbnz.com/highlighter/) and [Pygments](https://pygments.org) are a good comparison but instead of supporting highlight output for a vast set of languages, this tool focuses on specific use cases where the source code is accompanied by additional information for a richer colorization.

The uses cases are:

- highlighting arbitrary code with "code mirror color file"

```cpp
int main() // a comment
{
    std::cout << "sizeof(long) = " << sizeof(long) << '\n';
}
```

```
keyword func() 0comment
{
    namespace::global << str << keyword(keyword) << chr;
}
```

This would produce HTML code where the `int` gets a span with class `keyword`, `main` a span with class `func` and so on...

The format has been specifically designed to visually mirror original text. This allows fast, copy-and-paste editing of original text while still being human readable.

By default sequences of tokens - that is whitespace, *identifiers* (an alpha character followed by any number of alphanumeric characters) and symbols are expected to match 1:1 but there are many additional features like fixed-length tokens (`0` in the above example meaning to-the-end-of-line) and automatic quotation matching (`str` and `chr`).

The goal is to provide a very customizable highlight (typically for small snippets) where there is no highligher available and/or only manual coloration can achieve desired effect. Intended to colorize specifications, syntax examples and anything else that has no dedicated highlighter.

- highlighting C or C++ code with information provided by [clangd](https://clangd.llvm.org)

This is similar to already available highlighters but instead of implementing a more-or-less fuzzy/wonky/lax parser that doesn't fully understand the language the goal is to implement a parser that is able to highlight the code with maximum precision by utilizing compiler-level knowledge about the code delivered by clangd.

Below is a simplified example of a block of information delived by clangd about a sample program (token text positions ommited for brevity):

```
"#error Misconfigured build!", type: comment, modifiers:
"MACRO",        type: macro,         modifiers: globalScope
"T",            type: typeParameter, modifiers: declaration
"T",            type: typeParameter, modifiers:
"result",       type: unknown,       modifiers: dependentName, classScope
"main",         type: function,      modifiers: declaration, globalScope
"power_states", type: enum,          modifiers: declaration, globalScope
"on",           type: enumMember,    modifiers: declaration, readonly, globalScope
"sleep",        type: enumMember,    modifiers: declaration, readonly, globalScope
"off",          type: enumMember,    modifiers: declaration, readonly, globalScope
"point",        type: class,         modifiers: declaration, globalScope
"x",            type: property,      modifiers: declaration, classScope
"y",            type: property,      modifiers: declaration, classScope
"auto",         type: class,         modifiers: deduced, defaultLibrary, globalScope
"auto",         type: typeParameter, modifiers: functionScope
"std",          type: namespace,     modifiers: defaultLibrary, globalScope
"cout",         type: variable,      modifiers: defaultLibrary, globalScope
```

Specifically, this is taken from [Semantic Tokens LSP call](https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_semanticTokens). The same token (by string contents but not by position) can be reported multiple times (e.g. a keyword or variable used multiple times). As of writing this, roughly speaking, clangd reports every non-keyword identifier and few additional tokens like preprocessor-disabled code. With basic syntax understanding (for comments, keywords, operators, literals etc.) it's possible to implement IDE level of highlight in generated HTML. Obviously input code snippets should be self-contained and free of errors. Otherwise clangd may not deliver full information. In other words, the code must be compileable.

## Documentation - mirror

In general, tokens are matched 1:1, starting by reading the mirror color file first. The color file contents dictate how the code file contents should be consumed and transformed into HTML.

When the color file contains an identifier, it will be matched against an arbitrary identifier in the code.

- An identifier within the color file is a non-zero sequence of characters from the set of `a-zA-Z_`.
- An identifier within the code file is a character from the set of `a-zA-Z_` followed by any number of characters from the set of `a-zA-Z_0-9`.

This means that a color identifier like `var` can be matched against `x1` identifier in the code. Identifiers in the color file are not allowed to contain digits because digits have other, special meaning for color specification.

Example (code/color/output):

```
int x1
keyword var
<span class="keyword">int</span> <span class="var">x1</span>
```

Symbols without special meaning and whitespace are compared exactly. This forces the color file to "mirror" the code file contents in regards to syntax:

```
foo(bar);
func(arg);
<span class="func">foo</span>(<span class="arg">bar</span>);
```

3 color identifiers have special meaning:

- `num` - matches against a sequence of digits
- `chr` - matches against a literal embedded in `'` quotes
- `str` - matches against a literal embedded in `"` quotes

The literals may contain simples form of escapes (`\` followed by any single character) in which case these receive `chr_esc` and `str_esc` span classes:

```
"string\nwith\bescapes"
str
<span class="str">"string<span class="str_esc">\n</span>with<span class="str_esc">\b</span>escapes"</span>
```

Because CSS class names frequently use `-` and it is not allowed in color identifier names, one of generation option allows to convert `_` to `-`.

If the color identifier is preceeded by a number, it changes the matching behavior from color-identifier-to-code-identifier to color-identifier-to-exactly-this-many-characters:

```
onelongidentifier
3a4b10c
<span class="a">one</span><span class="b">long</span><span class="c">identifier</span>
```

these characters can be any characters:

```
"text = %s"
"7str2fmt"
"<span class="str">text = </span><span class="fmt">%s</span>"
```

If the number is 0, it will consume all characters left on the line:

```
f(); // comment
func(); 0com
<span class="func">f</span>(); <span class="com">// comment</span>
```

If specifying length is not desired and there is no whitespace between tokens the ` character can be used to separate color token names:

```
**kwargs
2op`param
<span class="op">**</span><span class="param">kwargs</span>
```

this includes the possibility to generate fixed-length spanless outputs:

```
onetwothree
3first3`5third
<span class="first">one</span>two<span class="third">three</span>
```

See `src/test/mirror_tests.cpp` for more examples.

## Documentation - clangd

This section assumes the reader is familiar with [LSP](https://microsoft.github.io/language-server-protocol/overviews/lsp/overview/), particularly with the [Semantic Tokens call](https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_semanticTokens). If not, read the Semantic Tokens section now.

Clangd has no documentation how exactly it implements LSP. Because LSP gives some freedom of implementation (like defining custom token types and modifiers, in addition to the standard ones) the information below is based on own experiments.

As of clangd 15.0.2, all of reported tokens are:

- preprocessor-disabled code (whole lines)
- preprocessor macro names (only macro definitions, if the macro has a body and the body uses other macros those are not reported)
- macro usages outside preprocessor code (if the macro has parameters they *may* be reported)
- identifiers that name C++ entities

Keywords and special constructs like literal prefixes/suffixes and overloaded operators are not reported.

Reported types (each token has exactly 1 type):

- objects: `variable`, `parameter`, `property` (field), `enumMember`
- functions: `function`, `method`
- types: `class`, `interface`, `enum`, `type`
- templates: `typeParameter` (both TTP and NTTP), `concept`
- `namespace`
- `comment` (used to report preprocessor-disabled code, not comments)
- `macro` (definitions in preprocessor and usages outside preprocessor)
- `unknown`

Reported modifiers (each token can have 0+ modifiers):

- `declaration`
- `deprecated`
- `deduced`
- `readonly`
- `static`
- `abstract`
- `virtual`
- `dependentName`
- `defaultLibrary`
- `usedAsMutableReference`
- 0 or 1 of `functionScope`, `classScope`, `fileScope`, `globalScope`

Discoveries and details:

- `declaration` works more like "definition" - it's present when the entity appears for the first time (e.g. type definition, function parameter name in the parameters list).
- Keywords are not reported (LSP assumes other tools are aware of basic syntax like comments, keywords and literals) except `auto` which *may* be reported if it substitues a type name (other uses of `auto` such as trailing retun type syntax are not reported). In such case the token will have appropriate type (e.g. `enum`, `class`) and *may* have the `deduced` token modifier (deduction is not always possible - particularly in generic lambdas and templates).
- `readonly` is present for `const` and `constexpr` objects, including references (e.g. parameter names that have const reference type).
- `static` is not reported for every entity using `static` keyword in C and C++. It's present only for static class members (`static` non-member functions and globals do not have this token modifier).
- `usedAsMutableReference` is reported for objects passed by non-const reference, only at the caller side.
- All or almost all entities will have set one of scope modifiers. It's basically entity's visibility.
- Macro calls outside preprocessor lines are reported. If such macros have parameters, they *may* be reported depending on how they are being used inside the macro (basically it depends on what the macro parameter becomes after macro expansion).

## Usage

The program has 3 interfaces:

- (unfinished) command-line
- directly through C++ (see `src/ach/mirror/core.hpp` and `src/ach/clangd/core.hpp`)
- indirectly through Python bindings

The mirror API requires to provide contents of 2 files (code and "mirror" color specification).

The clangd API requires to provide contents of the source file and clangd-semantic-token-information (already transformed into a form required by the project's C++ or Python interface). The clangd API does only parsing and application of clangd information - invoking clangd on the source file (with appropriate compiler settings) and then transforming received JSONs to required interface is your responsibility.

## Building

Modern CMake build recipe (targets not variables). See top-level `CMakeLists.txt` for details.

- (static library) Project core is the only mandatory part and requires only C++17.
- (executable) Unit tests require Boost test library (header-only).
- (executable) Command-line interface requires Boost with program_options library built.
- (shared library) Python bindings require Python 3.6+ development installation. Everything else is provided in submodules.
