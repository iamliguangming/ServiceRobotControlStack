#pragma once
// Copyright 2019 - 2020 kvedder@seas.upenn.edu
// School of Engineering and Applied Sciences,
// University of Pennsylvania
//
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
// ========================================================================

#include <sensor_msgs/PointCloud2.h>
#include <eigen3/Eigen/Geometry>

#include <algorithm>
#include <limits>
#include <string>
#include <utility>

#include "cs/util/constants.h"
#include "cs/util/image.h"

namespace util {
namespace pc {

using MapVector3f = Eigen::Map<Eigen::Vector3f, Eigen::Aligned32>;
using ConstMapVector3f = Eigen::Map<const Eigen::Vector3f, Eigen::Aligned32>;

struct __attribute__((packed)) Point16 {
  float x;
  float y;
  float z;
  float pad;

  Point16() = delete;

  MapVector3f GetMappedVector3f() {
    void* this_void = this;
    return MapVector3f(static_cast<float*>(this_void));
  }

  ConstMapVector3f GetMappedVector3f() const {
    const void* this_void = this;
    return ConstMapVector3f(static_cast<const float*>(this_void));
  }

  bool IsValid() const {
    return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
  }
};
static_assert(sizeof(Point16) == 16, "Point16 is not 16 bytes!");

struct __attribute__((packed)) Point20 {
  float x;
  float y;
  float z;
  float pad;
  float pad2;

  Point20() = delete;

  MapVector3f GetMappedVector3f() {
    void* this_void = this;
    return MapVector3f(static_cast<float*>(this_void));
  }

  ConstMapVector3f GetMappedVector3f() const {
    const void* this_void = this;
    return ConstMapVector3f(static_cast<const float*>(this_void));
  }

  bool IsValid() const {
    return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
  }
};
static_assert(sizeof(Point20) == 20, "Point20 is not 20 bytes!");

struct __attribute__((packed)) Point32 {
  float x;
  float y;
  float z;
  float pad;
  float pad2;
  float pad3;
  float pad4;
  float pad5;

  Point32() = delete;

  MapVector3f GetMappedVector3f() {
    void* this_void = this;
    return MapVector3f(static_cast<float*>(this_void));
  }

  ConstMapVector3f GetMappedVector3f() const {
    const void* this_void = this;
    return ConstMapVector3f(static_cast<const float*>(this_void));
  }

  bool IsValid() const {
    return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
  }
};
static_assert(sizeof(Point32) == 32, "Point32 is not 32 bytes!");

template <typename Point>
class PointCloud {
  sensor_msgs::PointCloud2 ros_pc_;

  void VerifyPointFields() const {
    static constexpr bool kDebug = false;
    if (kDebug) {
      for (const auto& f : ros_pc_.fields) {
        std::cout << f.name << " " << f.offset << std::endl;
      }
      ROS_INFO(
          "Given size %d vs sizeof %lu", ros_pc_.point_step, sizeof(Point));
    }
    NP_CHECK_EQ(ros_pc_.point_step, sizeof(Point));
  }

 public:
  PointCloud() {
    ros_pc_.point_step = sizeof(Point);
    VerifyPointFields();
  }
  explicit PointCloud(const sensor_msgs::PointCloud2& ros_pc)
      : ros_pc_(ros_pc) {
    VerifyPointFields();
  }

  void operator=(const sensor_msgs::PointCloud2& ros_pc) {
    ros_pc_ = ros_pc;
    VerifyPointFields();
  }

  void TransformFrame(const Eigen::Affine3f& transform,
                      const std::string& new_frame) {
    Transform(transform);
    ros_pc_.header.frame_id = new_frame;
  }

  void Transform(const Eigen::Affine3f& transform) {
    for (auto& p : *this) {
      p.GetMappedVector3f() = transform * p.GetMappedVector3f();
    }
  }

  int Width() const { return ros_pc_.width; }

  int Height() const { return ros_pc_.height; }

  std::pair<int, int> Shape() const { return {Width(), Height()}; }

  sensor_msgs::PointCloud2* GetRosPC() { return &ros_pc_; }

  const sensor_msgs::PointCloud2& GetRosPC() const { return ros_pc_; }

  util::img::Image<util::img::PixelRGB8> ToColorDepthImage(
      const float max_range) const {
    sensor_msgs::Image img;
    img.header = ros_pc_.header;
    img.width = ros_pc_.width;
    img.height = ros_pc_.height;
    img.encoding = sensor_msgs::image_encodings::RGB8;
    img.is_bigendian = false;
    img.step = 3 * img.width;

    for (const auto& p : *this) {
      const float normalized_val = p.GetMappedVector3f().norm() / max_range;
      const std::uint8_t scaled_val = static_cast<std::uint8_t>(
          normalized_val *
          static_cast<float>(std::numeric_limits<std::uint8_t>::max()));
      img.data.push_back(scaled_val);
      img.data.push_back(0);
      img.data.push_back(0);
    }

    return util::img::Image<util::img::PixelRGB8>(img);
  }

  util::img::Image<util::img::PixelMONO16> ToDepthImageMONO16(
      const float max_range) const {
    sensor_msgs::Image img;
    img.header = ros_pc_.header;
    img.width = ros_pc_.width;
    img.height = ros_pc_.height;
    img.encoding = sensor_msgs::image_encodings::MONO16;
    img.is_bigendian = true;
    img.step = 2 * img.width;

    for (const auto& p : *this) {
      const float normalized_val =
          std::max(0.0f, max_range - p.GetMappedVector3f().norm() / max_range);
      const std::uint16_t scaled_val = static_cast<std::uint16_t>(
          normalized_val *
          static_cast<float>(std::numeric_limits<std::uint16_t>::max()));
      img.data.push_back(scaled_val);
      img.data.push_back(scaled_val >> 8);
    }

    return util::img::Image<util::img::PixelMONO16>(img);
  }

  util::img::Image<util::img::PixelMONO8> ToDepthImageMONO8(
      const float max_range) const {
    sensor_msgs::Image img;
    img.header = ros_pc_.header;
    img.width = ros_pc_.width;
    img.height = ros_pc_.height;
    img.encoding = sensor_msgs::image_encodings::MONO8;
    img.is_bigendian = true;
    img.step = img.width;

    for (const auto& p : *this) {
      const float normalized_val =
          std::max(0.0f, max_range - p.GetMappedVector3f().norm() / max_range);
      const std::uint8_t scaled_val = static_cast<std::uint8_t>(
          normalized_val *
          static_cast<float>(std::numeric_limits<std::uint8_t>::max()));
      img.data.push_back(scaled_val);
    }

    return util::img::Image<util::img::PixelMONO8>(img);
  }

  double GetTime() const { return ros_pc_.header.stamp.toSec(); }

  bool IsEmpty() const { return ros_pc_.data.empty(); }

  Point* begin() {
    void* v = &ros_pc_.data[0];
    return static_cast<Point*>(v);
  }

  Point* end() {
    void* v = &ros_pc_.data[ros_pc_.data.size()];
    return static_cast<Point*>(v);
  }

  Point const* begin() const {
    void const* v = &ros_pc_.data[0];
    return static_cast<Point const*>(v);
  }

  Point const* end() const {
    void const* v = &ros_pc_.data[ros_pc_.data.size()];
    return static_cast<Point const*>(v);
  }
};

}  // namespace pc
}  // namespace util
