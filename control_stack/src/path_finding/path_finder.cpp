// Copyright 2019 kvedder@seas.upenn.edu
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

#include "cs/path_finding/path_finder.h"

#include <algorithm>
#include <limits>
#include <string>

#include "config_reader/macros.h"
#include "cs/util/constants.h"
#include "shared/math/geometry.h"

namespace cs {
namespace path_finding {

namespace params {
CONFIG_FLOAT(switch_historesis_threshold,
             "path_finding.switch_historesis_threshold");
CONFIG_FLOAT(goal_delta_change, "path_finding.goal_delta_change");
CONFIG_FLOAT(max_distance_off_path, "path_finding.max_distance_off_path");
};  // namespace params

Path2d PathFinder::UsePrevPathOrUpdate(const util::Map& dynamic_map,
                                       const Path2d& proposed_path) {
  static constexpr bool kDebug = false;
  NP_CHECK(!proposed_path.IsValid() ||
           !IsPathColliding(dynamic_map, proposed_path));

  // Old path invalid.
  if (!prev_path_.IsValid()) {
    if (kDebug) {
      ROS_INFO("Prev path invalid");
    }
    prev_path_ = proposed_path;
    return proposed_path;
  }

  // Old path colliding.
  if (IsPathColliding(dynamic_map, prev_path_)) {
    if (kDebug) {
      ROS_INFO("Prev path colliding");
    }
    prev_path_ = proposed_path;
    return proposed_path;
  }

  // New path invalid.
  if (!proposed_path.IsValid()) {
    return prev_path_;
  }

  const float distance_between_goals_sq =
      (prev_path_.waypoints.back() - proposed_path.waypoints.back())
          .squaredNorm();

  // Goal moved.
  if (distance_between_goals_sq >
      math_util::Sq(params::CONFIG_goal_delta_change)) {
    if (kDebug) {
      ROS_INFO("Goal Changed");
    }
    prev_path_ = proposed_path;
    return proposed_path;
  }

  const Eigen::Vector2f robot_position = proposed_path.waypoints.front();
  const float distance_from_first_segment_sq =
      (geometry::ProjectPointOntoLine(
           robot_position, prev_path_.waypoints[0], prev_path_.waypoints[1]) -
       robot_position)
          .squaredNorm();

  // Too far away from old path.
  if (distance_from_first_segment_sq >
      math_util::Sq(params::CONFIG_max_distance_off_path)) {
    prev_path_ = proposed_path;
    return proposed_path;
  }

  // New path is significantly less expensive.
  if (params::CONFIG_switch_historesis_threshold + proposed_path.cost <
      prev_path_.cost) {
    if (kDebug) {
      ROS_INFO("Proposed path better");
    }

    if (prev_path_.waypoints.size() >= 3) {
      prev_path_.waypoints.erase(prev_path_.waypoints.begin());
      const float distance_from_new_first_segment_sq =
          (geometry::ProjectPointOntoLine(robot_position,
                                          prev_path_.waypoints[0],
                                          prev_path_.waypoints[1]) -
           robot_position)
              .squaredNorm();
      if (distance_from_new_first_segment_sq <=
          math_util::Sq(params::CONFIG_max_distance_off_path)) {
        return prev_path_;
      }
    }

    prev_path_ = proposed_path;
    return proposed_path;
  }
  return prev_path_;
}
}  // namespace path_finding
}  // namespace cs
