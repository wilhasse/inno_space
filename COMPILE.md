# Inno Space Compilation Instructions

This document explains how to compile the Inno Space project and run its tests.

## Prerequisites

Make sure you have the following installed on your system:
- Git
- GCC/G++ with C++11 support
- Make
- zlib development libraries

On Debian/Ubuntu systems, you can install the prerequisites with:
```bash
sudo apt-get update
sudo apt-get install build-essential git zlib1g-dev
```

On CentOS/RHEL/Fedora:
```bash
sudo dnf install gcc gcc-c++ make git zlib-devel
```

## Cloning the Repository

```bash
git clone https://github.com/wilhasse/inno_space.git
cd inno_space
```

## Running the Tests

To compile and run the unit tests:

```bash
make test
./unit_tests
```

This will:
1. Compile the necessary components for testing
2. Build the unit tests executable
3. Run the tests

If successful, you should see output like:
```
Running test_page_zip_decompress_low
Running test_fil_header_parsing
All tests passed
```

## Compiling the Main Project

To build the main executable:

```bash
make all
```

This compiles all source files and creates an executable named `inno` in the project root.

## Verifying the Installation

You can verify the installation by running:

```bash
./inno -h
```

This should display the help information showing available commands.

## Running with Sample Files

The project includes sample files in the `tool` directory. You can test the executable with:

```bash
./inno -f tool/sbtest1.ibd -c list-page-type
```

This will display information about the page types in the sample InnoDB table file.

## Cleaning Up

To clean the project (remove object files and executables):

```bash
make clean
```

## Troubleshooting

If you encounter any compilation issues:

1. Make sure all dependencies are installed
2. Check that you have a compatible C++ compiler (supporting C++11)
3. Verify that zlib is properly installed on your system

For any other issues, please refer to the project documentation or submit an issue on the GitHub repository.