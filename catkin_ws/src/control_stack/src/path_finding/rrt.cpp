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

#include "cs/path_finding/rrt.h"

#include <algorithm>
#include <limits>
#include <string>

#include "config_reader/macros.h"

namespace cs {
namespace path_finding {

namespace params {
CONFIG_INT(max_iterations, "rrt.max_iterations");
CONFIG_FLOAT(delta_q, "rrt.delta_q");
CONFIG_FLOAT(goal_bias, "rrt.goal_bias");
CONFIG_FLOAT(is_goal_threshold, "rrt.is_goal_threshold");
}  // namespace params

RRT::RRT(const util::Map& map,
         const float& robot_radius,
         const float& safety_margin)
    : PathFinder(map),
      robot_radius(robot_radius),
      safety_margin(safety_margin) {}

Path2d RRT::FindPath(const util::Map& dynamic_map,
                     const Eigen::Vector2f& start,
                     const Eigen::Vector2f& goal,
                     ros::Publisher* pub) {
  // Drive straight to goal.
  if (!IsLineColliding(dynamic_map, start, goal)) {
    return UsePrevPathOrUpdate(dynamic_map,
                               Path2d({start, goal}, (start - goal).norm()));
  }

  return UsePrevPathOrUpdate(dynamic_map,
                             GenerateNewPath(dynamic_map, start, goal, pub));
}

bool RRT::IsLineColliding(const util::Map& dynamic_map,
                          const Eigen::Vector2f& p1,
                          const Eigen::Vector2f& p2) const {
  for (const auto& w : map_.walls) {
    if (geometry::CheckLineLineCollision(w.p1, w.p2, p1, p2)) {
      return true;
    }

    if (geometry::MinDistanceLineLine(w.p1, w.p2, p1, p2) <
        (robot_radius + safety_margin)) {
      return true;
    }
  }
  for (const auto& w : dynamic_map.walls) {
    if (geometry::CheckLineLineCollision(w.p1, w.p2, p1, p2)) {
      return true;
    }

    if (geometry::MinDistanceLineLine(w.p1, w.p2, p1, p2) <
        (robot_radius + safety_margin)) {
      return true;
    }
  }
  return false;
}

int RRT::AddPoint(const util::Map& dynamic_map,
                  RRT::Tree* tree,
                  const Eigen::Vector2f& steering_point) {
  const int closest_idx = tree->FindClosestPoint(steering_point);
  NP_CHECK(closest_idx < static_cast<int>(tree->points.size()));
  const Eigen::Vector2f& tree_point = tree->points[closest_idx].point;
  const Eigen::Vector2f norm_delta = (steering_point - tree_point).normalized();
  const Eigen::Vector2f final_point =
      norm_delta * params::CONFIG_delta_q + tree_point;

  if (!IsLineColliding(dynamic_map, tree_point, final_point)) {
    tree->points.push_back({final_point, closest_idx});
    return static_cast<int>(tree->points.size()) - 1;
  }
  return TreePoint::kRootParent;
}

bool RRT::IsNearGoal(const Eigen::Vector2f& goal,
                     const Eigen::Vector2f& point) const {
  return (goal - point).squaredNorm() <
         math_util::Sq(params::CONFIG_is_goal_threshold);
}

Path2d RRT::SmoothPath(const util::Map& dynamic_map, Path2d path) {
  if (!path.IsValid()) {
    return path;
  }

  NP_CHECK(!IsPathColliding(dynamic_map, path));
  for (size_t i = 2; i < path.waypoints.size(); ++i) {
    if (IsLineColliding(dynamic_map, path.waypoints[0], path.waypoints[i])) {
      if (i > 3) {
        path.waypoints.erase(path.waypoints.begin() + 1,
                             path.waypoints.begin() + (i - 1));
      }
      break;
    }
  }
  NP_CHECK(!IsPathColliding(dynamic_map, path));
  return path;
}

Path2d RRT::GenerateNewPath(const util::Map& dynamic_map,
                            const Eigen::Vector2f& start,
                            const Eigen::Vector2f& goal,
                            ros::Publisher* pub) {
  Tree tree(start);

  float x_min = std::min(start.x(), goal.x());
  float x_max = std::max(start.x(), goal.x());
  float y_min = std::min(start.y(), goal.y());
  float y_max = std::max(start.y(), goal.y());

  for (const auto& w : map_.walls) {
    x_min = std::min({x_min, w.p1.x(), w.p2.x()});
    x_max = std::max({x_max, w.p1.x(), w.p2.x()});
    y_min = std::min({y_min, w.p1.y(), w.p2.y()});
    y_max = std::max({y_max, w.p1.y(), w.p2.y()});
  }

  std::uniform_real_distribution<float> x_dist(x_min, x_max);
  std::uniform_real_distribution<float> y_dist(y_min, y_max);
  std::uniform_real_distribution<float> goal_bias_dist(0, 1);

  for (int i = 0; i < params::CONFIG_max_iterations; ++i) {
    const bool choose_goal =
        (goal_bias_dist(generator) <= params::CONFIG_goal_bias);
    const float x_sample = (choose_goal) ? goal.x() : x_dist(generator);
    const float y_sample = (choose_goal) ? goal.y() : y_dist(generator);
    const Eigen::Vector2f sample(x_sample, y_sample);
    const int added_point_idx = AddPoint(dynamic_map, &tree, sample);
    if (TreePoint::kRootParent == added_point_idx) {
      continue;
    }
    const Eigen::Vector2f& added_point = tree.points[added_point_idx].point;
    if (IsNearGoal(goal, added_point)) {
      if (pub != nullptr) {
        pub->publish(DrawTree(tree, "map", "tree"));
      }
      auto path = tree.UnwindPath(added_point_idx);
      std::reverse(path.waypoints.begin(), path.waypoints.end());
      return SmoothPath(dynamic_map, path);
    }
  }
  if (pub != nullptr) {
    pub->publish(DrawTree(tree, "map", "tree"));
  }
  ROS_WARN("Reached max iterations without finding goal!");
  return {};
}

bool RRT::IsPathColliding(const util::Map& dynamic_map,
                          const Path2d& path) const {
  if (path.waypoints.size() < 2) {
    return true;
  }
  for (size_t i = 0; i < path.waypoints.size() - 1; ++i) {
    NP_CHECK(i < path.waypoints.size());
    NP_CHECK(i + 1 < path.waypoints.size());
    const auto& curr = path.waypoints[i];
    const auto& next = path.waypoints[i + 1];
    if (IsLineColliding(dynamic_map, curr, next)) {
      return true;
    }
  }
  return false;
}

int RRT::Tree::FindClosestPoint(const Eigen::Vector2f& steering_point) {
  float min_sq_distance = std::numeric_limits<float>::max();
  float min_i = 0;
  for (int i = 0; i < static_cast<int>(points.size()); ++i) {
    const float i_sq_dist = (points[i].point - steering_point).squaredNorm();
    if (i_sq_dist < min_sq_distance) {
      min_i = i;
      min_sq_distance = i_sq_dist;
    }
  }

  return min_i;
}

Path2d RRT::Tree::UnwindPath(const int goal_idx) const {
  Path2d p;
  p.waypoints.push_back(points[goal_idx].point);
  TreePoint current_point = points[goal_idx];
  while (current_point.parent != TreePoint::kRootParent) {
    TreePoint parent_point = points[current_point.parent];
    p.cost += (parent_point.point - current_point.point).norm();
    p.waypoints.push_back(parent_point.point);
    current_point = parent_point;
  }
  return p;
}

visualization_msgs::Marker RRT::DrawTree(
    const cs::path_finding::RRT::Tree& tree,
    const std::string& frame_id,
    const std::string& ns) {
  visualization_msgs::Marker marker;
  marker.header.frame_id = frame_id;
  marker.header.stamp = ros::Time();
  marker.ns = ns;
  marker.id = 0;
  marker.type = visualization_msgs::Marker::LINE_LIST;
  marker.action = visualization_msgs::Marker::ADD;
  marker.scale.x = 0.02;
  marker.scale.y = 0.1;
  marker.scale.z = 0.1;
  marker.color.a = 1;
  marker.color.r = 0;
  marker.color.g = 0;
  marker.color.b = 1;

  for (const auto& tree_point : tree.points) {
    if (tree_point.parent == cs::path_finding::RRT::TreePoint::kRootParent) {
      continue;
    }
    const auto& parent_tree_point = tree.points[tree_point.parent];
    geometry_msgs::Point p1;
    p1.x = tree_point.point.x();
    p1.y = tree_point.point.y();
    geometry_msgs::Point p2;
    p2.x = parent_tree_point.point.x();
    p2.y = parent_tree_point.point.y();
    marker.points.push_back(p1);
    marker.points.push_back(p2);
  }

  return marker;
}

RRT::TreePoint::TreePoint(const Eigen::Vector2f& point, const int parent)
    : point(point), parent(parent) {}
}  // namespace path_finding
}  // namespace cs