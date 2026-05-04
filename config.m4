PHP_ARG_ENABLE([astop],
  [whether to enable astop support],
  [AS_HELP_STRING([--enable-astop], [Enable astop])])

if test "$PHP_ASTOP" != "no"; then
  PHP_NEW_EXTENSION(astop, astop.c, $ext_shared)
fi
