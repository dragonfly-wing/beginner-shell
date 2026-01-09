# Unix-like Shell in C

A minimal Unix-like shell implemented in C, supporting command execution,
pipelines, and I/O redirection.

The project focuses on parsing and execution design rather than feature
completeness.

---

## Features

- Execution of external commands
- Pipelines (`cmd1 | cmd2 | cmd3`)
- Input and output redirection (`<`, `>`, `>>`)
- Combination of pipelines and redirection (`cmd1 | cmd2 > file`)
- Handwritten lexer and AST-based parser

---

## Parser Design

The shell uses a handwritten lexer to tokenize input into words, pipes,
and redirection operators.

Parsing is implemented using an AST-based approach:
- Simple commands are parsed into command nodes
- Pipelines construct pipeline nodes containing multiple stages
- Redirection operators wrap existing AST nodes, ensuring correct
  precedence over pipelines

This design avoids ambiguity present in linear parsers and makes
extensions easier.

---

## Execution Model

Execution is performed by recursively walking the AST:
- Command nodes execute via `execvp`
- Pipeline nodes fork processes and connect them with pipes
- Redirection nodes adjust file descriptors before executing child nodes

---

## Build & Run

```bash
gcc -Wall -Wextra shell.c -o shell
./shell
