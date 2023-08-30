# Bazel Example

This directory shows an example of using LibCbor in a project that builds with Bazel.

## Compile

To build the project:

```shell
bazel build src:all
```

## Test

To test the code:

```shell
bazel test src:all
```

## Run

To run the demo:

```shell
bazel run src:hello
```

or

```shell
bazel-bin/src/hello
```
