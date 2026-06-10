# Customizing Your Dev Container

Customize your dev container using build arguments (for direct Docker builds) or
environment variables (for evaluation in `devcontainer.json`).

* **CXX standard:**
  This is the C++ standard to build from (eg: 17, 20, ...). (Default: 17)
  * Docker ARG:
   `CXX_STANDARD`
  * Host Environment Variable:
   `OTEL_CPP_DEVCONTAINER_CXX_STANDARD`

* **User ID (UID):**
  User ID (Default: `1000`)
  * Docker ARG:
   `USER_UID`
  * Host Environment Variable:
   `OTEL_CPP_DEVCONTAINER_USER_UID`

* **Group ID (GID):**
  User group ID (Default: `1000`)
  * Docker ARG:
   `USER_GID`
  * Host Environment Variable:
   `OTEL_CPP_DEVCONTAINER_USER_GID`

* **Install Packages:**
  These are the additional packages that will be installed via `apt install` in the devcontainer. This is a space separated list.
  * Docker ARG:
   `INSTALL_PACKAGES`  (Default: ``)
  * Host Environment Variable:
   `OTEL_CPP_DEVCONTAINER_INSTALL_PACKAGES` (Default: ``)

## Examples

* `docker build --build-arg CXX_STANDARD="20" --build-arg INSTALL_PACKAGES="nano gitk"...`
* `export OTEL_CPP_DEVCONTAINER_CXX_STANDARD=20`
* `export OTEL_CPP_DEVCONTAINER_INSTALL_PACKAGES="nano gitk"`
