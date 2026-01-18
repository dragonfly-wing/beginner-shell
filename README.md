
---

# Unix-like Shell in C

A minimal Unix-like shell implemented in C, supporting command execution, pipelines, and I/O redirection.

This project focuses on parsing, process control, and file-descriptor management, rather than feature completeness or strict POSIX compliance.

---

## Features

* Execution of external commands via execvp
* Built-in commands: cd, help, exit
* Pipelines (cmd1 | cmd2 | cmd3)
* Input and output redirection (<, >, >>)
* Combination of pipelines and redirection
  (example: cat < in.txt | grep foo | wc -l > out.txt)
* Handwritten lexer
* Recursive-descent, AST-based parser

---

## Project Structure

Root directory contains:

* include/

  * common.h – shared types and error codes
  * tokenizer.h
  * parser.h
  * executor.h
  * builtins.h

* src/

  * main.c – shell loop and control flow
  * tokenizer.c – lexer implementation
  * parser.c – AST construction
  * executor.c – process creation and file descriptor wiring
  * builtins.c – built-in command implementations

* Makefile

* README.md

The shell was originally implemented in a single file and later refactored into multiple modules to separate concerns and improve maintainability.

---

## Parser Design

Input is tokenized into words, pipes, and redirection operators.

Parsing is performed using a recursive-descent approach that builds an explicit Abstract Syntax Tree (AST):

* Command nodes store argv and associated redirections
* Pipeline nodes represent ordered execution stages
* Redirections are parsed with higher precedence than pipelines

This structure avoids ambiguity found in linear parsers and makes execution behavior explicit and easier to extend.

---

## Execution Model

Execution proceeds by walking the AST:

* Each pipeline stage runs in its own process
* Pipes are created eagerly and connected using dup2
* Redirections override inherited file descriptors
* Built-in commands execute in the parent shell when required
* Child processes are synchronized using waitpid

The execution model closely mirrors how Unix shells manage processes and file descriptors.

---

## Build and Run

Build the shell:

make

Run the shell:

./sh

Clean build artifacts:

make fclean

---

## Notes

* This is a learning-focused shell, not a full POSIX implementation
* Quoting, globbing, job control, and environment expansion are not supported
* The project prioritizes correctness, clarity, and systems-level understanding over feature breadth

---

