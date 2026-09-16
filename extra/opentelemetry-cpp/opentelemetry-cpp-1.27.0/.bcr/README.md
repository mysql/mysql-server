# BCR Publishing Configuration

This directory contains the template files required for publishing opentelemetry-cpp to the [Bazel Central Registry (BCR)](https://github.com/bazelbuild/bazel-central-registry).

## Files

### metadata.template.json
Contains the module metadata including homepage, maintainers, and repository information. This is used to create/update the BCR metadata for the module.

### source.template.json
Defines the source archive location and format. Placeholders like `{OWNER}`, `{REPO}`, `{TAG}`, and `{VERSION}` are automatically replaced during publishing.

### presubmit.yml
Defines the BCR presubmit tests that run when a new version is published. Currently configured to test on:
- Platforms: debian10, macos, ubuntu2004, windows
- Bazel versions: 7.x, 8.x, 9.*

## Publishing Process

The publish-to-bcr workflow (`.github/workflows/publish-to-bcr.yml`) automates publishing to BCR.

### Prerequisites

**Create a Personal Access Token (PAT)**:
   - Create a Classic PAT with `workflow` and `repo` permissions
   - Add it as a secret named `BCR_PUBLISH_TOKEN` in repository settings

### How to Publish

The workflow can be triggered in two ways:

1. **Automatically**: When a new GitHub release is published
2. **Manually**: Via workflow dispatch in GitHub Actions UI
   - Go to Actions → Publish to BCR → Run workflow
   - Enter the tag name (e.g., `v1.24.0`)

### What Happens

When triggered, the workflow:
1. Generates a BCR entry using these templates
2. Pushes the entry to your BCR fork
3. Opens a pull request against the upstream BCR

### Using a Fork Registry

For development and testing, you can use a fork of the Bazel Central Registry:

1. Fork https://github.com/bazelbuild/bazel-central-registry
2. Configure your fork URL in the workflow (modify the `publish-to-bcr.yml` file)
3. Test changes in your fork before submitting to the official BCR

The configuration in this repository points to the **official** Bazel Central Registry at https://github.com/bazelbuild/bazel-central-registry. However, using a fork registry is recommended for testing and development to avoid polluting the official registry during experimentation.

## References

- [Publish to BCR Action](https://github.com/bazel-contrib/publish-to-bcr)
- [Bazel Central Registry](https://github.com/bazelbuild/bazel-central-registry)
- [BCR Module: opentelemetry-cpp](https://github.com/bazelbuild/bazel-central-registry/tree/main/modules/opentelemetry-cpp)
