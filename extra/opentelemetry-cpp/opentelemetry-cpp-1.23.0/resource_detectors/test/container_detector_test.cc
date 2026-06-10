// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <cstdio>
#include <fstream>
#include <string>

#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/resource_detectors/detail/container_detector_utils.h"

TEST(ContainerIdDetectorTest, ExtractValidContainerIdFromLine)
{
  std::string line =
      "13:name=systemd:/podruntime/docker/kubepods/ac679f8a8319c8cf7d38e1adf263bc08d23.aaaa";
  std::string extracted_id =
      opentelemetry::resource_detector::detail::ExtractContainerIDFromLine(line);
  EXPECT_EQ(std::string{"ac679f8a8319c8cf7d38e1adf263bc08d23"}, extracted_id);
}

TEST(ContainerIdDetectorTest, ExtractIdFromMockUpCGroupFile)
{
  const char *filename = "test_cgroup.txt";

  {
    std::ofstream outfile(filename);
    outfile << "13:name=systemd:/kuberuntime/containerd"
               "/kubepods-pod872d2066_00ef_48ea_a7d8_51b18b72d739:cri-containerd:"
               "e857a4bf05a69080a759574949d7a0e69572e27647800fa7faff6a05a8332aa1\n";
    outfile << "9:cpu:/not-a-container\n";
  }

  std::string container_id =
      opentelemetry::resource_detector::detail::GetContainerIDFromCgroup(filename);
  EXPECT_EQ(container_id,
            std::string{"e857a4bf05a69080a759574949d7a0e69572e27647800fa7faff6a05a8332aa1"});

  std::remove(filename);
}

TEST(ContainerIdDetectorTest, DoesNotExtractInvalidLine)
{
  std::string line = "this line does not contain a container id";
  std::string id   = opentelemetry::resource_detector::detail::ExtractContainerIDFromLine(line);
  EXPECT_EQ(id, std::string{""});
}

TEST(ContainerIdDetectorTest, ReturnsEmptyOnNoMatch)
{
  const char *filename = "test_empty_cgroup.txt";

  {
    std::ofstream outfile(filename);
    outfile << "no container id here\n";
  }

  std::string id = opentelemetry::resource_detector::detail::GetContainerIDFromCgroup(filename);
  EXPECT_EQ(id, std::string{""});

  std::remove(filename);  // cleanup
}

TEST(ContainerIdDetectorTest, ReturnsEmptyOnFileFailingToOpen)
{
  const char *filename = "test_invalid_cgroup.txt";

  std::string id = opentelemetry::resource_detector::detail::GetContainerIDFromCgroup(filename);
  EXPECT_EQ(id, std::string{""});
}
