# Introduction

This repo stores all driver code, for example sensors, flash, etc, used across all of our boards. The intended usage of this is to be used as a submodule and then added to a STM32 project.

# Writing Drivers

To write a sensor, you must initialize a struct of the form

```c
struct sensor { 
  bool (*read)(void*, struct packet*);
  void* ctx;
};
```

See the existing sensors for how to do this. The gist is you must provide a `read` callback which tells the sensor how to read data and pack it into the packet. The `void * ctx` can be used to supply any struct you need for the sensor to work (for example SPI, UART handles, or buffers).

You may import `defs.h` to use any of the STM32 HAL functions as well as the STATIC keyword, which you may use to mark a static function for testing. This is useful to mark your `read` functions. That way, you don't have to expose it in your header files.

# Writing Tests 

We use [ceedling](https://www.throwtheswitch.org/ceedling), which bundles [unity](https://www.throwtheswitch.org/unity) and [cmock](https://www.throwtheswitch.org/cmock), for unit tests, you only need to download ceedling, which will pull in the rest. See the provided tests for an example of how to write one. The simplified explanation is you can mock header files which allows you to use mock functions. Use the mock functions which look something like

```c
#include "unity.h"
#include "cmock.h"
#include "mock_header.h"

struct fake_sensor_data fake = {
	.pressure = 341,
	.temperature = 231,
};
your_function_name_ExpectAnyArgsAndReturn(true);
// sensor_data is the parameter name of your_function_name 
your_function_name_ReturnMemThruPtr_sensor_data(&fake, sizeof(fake));
```

Then instead of actually calling the function, cmock will return your `fake` data and your code runs as usual. To tell "cmock" to mock a header file, just add `#include "mock_headername.c"` where headername is the name of your header file. See cmock documentation for list of things you can mock.

If you are running the tests and hit an undefined HAL function or reference, you may add it to `/test/support/stub_hal.h` 

```c
// For functions
#define HAL_I2C_Mem_Read(...)    (0)
// Typedefs
typedef int HAL_StatusTypeDef;
// Defines and Constants
#define HAL_OK 0
// Typedef structs
typedef struct { uint32_t dummy; } I2C_HandleTypeDef;
```

This allows the compilation to run smoothly while also allowing us to ignore the details of the HAL interface.

# Running Tests

After you write a test, you can run in the project root directory

```sh
// Only needed for the first time to fetch dependencies
cmake -B build
ceedling test:all
```

to run all tests. To see coverage, you must install `gcovr` and then run

```sh
ceedling gcov:all
```

which will generate a html file which you can view in your browser. If you want to ignore a function for coverage, you may add `// GCOVR_EXCL_FUNCTION` after the function definition. This is useful for functions which rely on actual hardware which should be excluded from unit tests.
