# This is a renovate-friendly source of Docker images.
FROM davidanson/markdownlint-cli2:v0.21.0@sha256:0c238c341f1bb9b111aec96fcd8848329e74fc7d9e6a318306c958f5ed7d520a AS markdownlint
FROM lycheeverse/lychee:sha-3a09227-alpine@sha256:5853bd7c283663a1200dbb15924a5047f8d4c50adfa7a4c212a94f04bbac831c AS lychee
