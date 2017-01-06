# flag

  Go-style flag parsing for C programs.

## Installation

  Install with [clib](https://github.com/clibs/clib):

```
$ clib install flag
```

## Example

Much like the Go flag package you have the choice of using a lower level `flagset_t`, or relying on the less flexible singleton. The following example uses the singleton, which handles the `--version` and `--help` output for you, as well as reporting errors.

```c
#include <stdio.h>
#include "flag.h"

#define VERSION "v1.0.0"

int
main(int argc, const char **argv) {
  int requests = 5000;
  int concurrency = 10;
  const char *url = ":3000";

  flag_int(&requests, "requests", "Number of total requests");
  flag_int(&concurrency, "concurrency", "Number of concurrent requests");
  flag_str(&url, "url", "Target url");
  flag_parse(argc, argv, VERSION);

  puts("");
  printf("     requests: %d\n", requests);
  printf("  concurrency: %d\n", concurrency);
  printf("          url: %s\n", url);
  puts("");

  return 0;
}
```

Default help output:

```
Usage: ./example [options] [arguments]

Options:
  --requests     Number of total requests (5000)
  --concurrency  Number of concurrent requests (10)
  --url          Target url (:3000)
  --version      Output version
  --help         Output help
```

## License

MIT
