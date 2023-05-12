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

## Thoughts on adding a Fixed-size/Resizable Flag on StringBuilder

`StringBuilder` may refer either to dynamically allocated buffers or to
fixed-sized buffers. This is a nice feature, but it has some pitfalls. For
example, if you decide to change a dynamically allocated buffer to a
fixed-sized one, you better remember to change all the builder function calls
to their `_fixed` variants or else they may call realloc on them and crash.
Such an error might go unnoticed for a long time and then suddenly manifest
when a file/line/string is long enough to trigger a realloc.

I can think of a few ways to mitigate this:

1. Use Hungarian notation. Fixed-sized StringBuilders should end in `_f` or
some such and dynamically-allocated ones must not. Then, if you ever change a
dynamically-allocated builder to a fixed-sized one, you manually rename all
mentions of the builder and update the function calls as you go. Maybe a regex
could look for infractions.

2. Include a flag on the `StringBuilder` struct indicating if the buffer is
resizable or fixed-size. Using 0 to mean "resizable" would allow for
zero-initialization to indicate resizable, which is probably the right default.
Then the `builder_from_fixed()` macro would set this bit to 1. Then I could
pass both types to the same builder functions and delete all the `_fixed`
functions.

Option 2 allows for an interesting possibility: when a fixed-sized builder runs
out of space, it could automatically convert to a dynamically allocated buffer,
copy over the data, and clear the "fixed-size" flag; the `builder_destroy()`
function would have to check the bit to decide whether to call `free()`. If you
wanted to restrict it to remaining a fixed-sized buffer, there could be an
additional flag to prevent this.

There are a couple things about option 2 I'm not crazy about:

A. It adds some complication to the implementation of the builder functions as
they now all have to check this bit. My preference for such things is to define
the behavior of a function and put the burden on the user of an API to ensure
that they aren't violating the requirements rather than to try to be super
paranoid about what I'm being passed. Like memcpy doesn't waste time checking
if the arguments are NULL pointers, it just expects you to pass valid pointers.
I find programming in this style leads to simpler code.

B. I hate the idea of adding a byte to the `StringBuilder` struct just to hold
a flag or two. On a 64-bit system, it takes up 16 bytes; adding a flag byte
would make it take up a full 24 bytes with padding. I could do something clever
where I encode the flag in the LSB of the capacity. But this seems like it's
just begging for bugs. I could maybe make it work with some cleverness: when
allocating a buffer on the heap, round the capacity up to the nearest even
number (to ensure LSB=0), and when pointing to a fixed-size buffer, round down
to the nearest odd number (to ensure LSB=1 without saying the capacity is
greater than it is). Then the capacity field is never actually lying.

Note: I have now implemented this. I went with the solution described above of
using the LSB of the capacity to indicate if the buffer is fixed-size or
dynamically-allocated.
