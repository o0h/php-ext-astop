# php-ext-astop

A PHP extension that intercepts the compilation pipeline and prints the **AST** and **opcodes** for every file or string compiled during a request.

> **Note:** This project is a personal learning exercise to explore PHP internals — the compiler pipeline, AST structure, and opcode generation. It is not intended for production use.

---

## What it does

When loaded, `astop` hooks into three PHP internals:

| Hook | What it captures |
|---|---|
| `zend_ast_process` | AST produced after parsing |
| `zend_compile_file` | Opcodes for each compiled file |
| `zend_compile_string` | Opcodes for `eval()` / `assert()` strings |

An optional **no-exec mode** (`ASTOP_MODE=no_exec`) also replaces `zend_execute_ex` with a no-op, so you can inspect compilation output without actually running the code.

---

## Docker

The easiest way to try it out — no PHP dev headers needed locally.

```bash
docker build -t astop .
```

Run an inline snippet:

```bash
docker run --rm astop -r 'echo "hello";'
```

Run a local file:

```bash
docker run --rm -v "$(pwd):/work" astop /work/your_script.php
```

no_exec mode (dump only, skip execution):

```bash
docker run --rm -e ASTOP_MODE=no_exec -v "$(pwd):/work" astop /work/your_script.php
```

---

## Install via PIE

[PIE](https://github.com/php/pie) is the official PHP extension installer.

```bash
pie install o0h/astop
```

## Build from source

```bash
phpize
./configure
make
```

The extension will be built at `modules/astop.so`.

---

## Usage

### Basic — dump AST and opcodes, then run the script

```bash
php -d extension=modules/astop.so your_script.php
```

### no_exec — dump only, skip execution

```bash
ASTOP_MODE=no_exec php -d extension=modules/astop.so your_script.php
```

---

## Example

```php
<?php
echo "hello";
```

```
=== AST ===
ZEND_AST_STMT_LIST lineno=1
  stmt[0]: ZEND_AST_STMT_LIST lineno=2
    stmt[0]: ZEND_AST_ECHO lineno=2
      expr: ZEND_AST_ZVAL("hello") lineno=2

=== OPCODES {main} ===
  line  num  opcode                     op1           op2           result
     2    0  ZEND_ECHO                  "hello"       -             -
     3    1  ZEND_RETURN                1             -             -

hello
```

With `ASTOP_MODE=no_exec`, the AST and opcode sections appear as above but `hello` is not printed.

---

## Running tests

```bash
make test
```
