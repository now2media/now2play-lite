# now2play-lite

This is a simplified (lite) version of the now2play application.

## SDK Installation

The SDK libraries required to compile and run the application are not included in this Git repository due to their size.

Before compiling the project:
1. Download the latest SDK package from http://sdk.now2media.com.
2. Copy all files and the `header` directory from the downloaded archive into the `SDK` folder at the root directory of the project.

## Build and Run

After placing the required library files, you can build the project using the following commands:

```bash
cmake -B build -S .
cmake --build build
```
