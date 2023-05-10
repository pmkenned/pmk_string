# TODO:

- Document the API more
- Consider adding `PMK_NO_SHORT_NAMES` macro to help avoid name collisions.
- Consider using `_Generic` to allow for either `String` or `char *` for
  functions such as `builder_append()`
- Something similar to `scscatrepr`
- Certain functions such as `string_substr()` currently take start and end as
  parameters. Perhaps this should be start and length? Also, they allow start
  and end to be negative in which case they count from the end of the string,
  but this probably isn't as useful as it is in Python, say, where you can omit
  the end parameter to indicate that you want to include from start to the end
  of the string, e.g. `s[-2:]`. In our case, the best we could do would be
  `s[-2:-1]` which would leave off the last character.
- `string_find()` returns the index of the substring being searched for, but it
  might be preferable to return a pointer?
- Decide best way to handle and/or indicate errors. However it is done, it
  should be made consistent across all the functions. Perhaps the best way is
  to return `-errno`. That way, you can always just do `if (f() < 0)` to check
  for errors and you can just use `perror()` or `strerror()` to print a helpful
  error message.
- Decide if a more versatile way to control allocations is needed. Right now,
  there are only two ways to control it: defining `PMK_REALLOC` and passing a
  context pointer. That may or may not be sufficient.
- Consider adding flag to `StringBuilder` saying if buffer is dynamically
  allocated or not. Could maybe eliminate the `_fixed` functions.
