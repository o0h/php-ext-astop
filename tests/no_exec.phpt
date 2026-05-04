--TEST--
astop: no_exec mode dumps AST and opcodes but suppresses execution
--EXTENSIONS--
astop
--ENV--
XDEBUG_MODE=off
ASTOP_MODE=no_exec
--FILE--
<?php
echo "hello";
--EXPECT--

=== AST ===
ZEND_AST_STMT_LIST lineno=1
  stmt[0]: ZEND_AST_STMT_LIST lineno=2
    stmt[0]: ZEND_AST_ECHO lineno=2
      expr: ZEND_AST_ZVAL("hello") lineno=2

=== OPCODES {main} ===
  line  num  opcode                     op1           op2           result
     2    0  ZEND_ECHO                  "hello"       -             -
     3    1  ZEND_RETURN                1             -             -
