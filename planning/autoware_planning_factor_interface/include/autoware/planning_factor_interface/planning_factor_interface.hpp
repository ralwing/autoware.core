// Copyright 2024 TIER IV, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef AUTOWARE__PLANNING_FACTOR_INTERFACE__PLANNING_FACTOR_INTERFACE_HPP_
#define AUTOWARE__PLANNING_FACTOR_INTERFACE__PLANNING_FACTOR_INTERFACE_HPP_

#include <autoware/motion_utils/trajectory/trajectory.hpp>
#include <rclcpp/rclcpp.hpp>

#include <autoware_internal_planning_msgs/msg/control_point.hpp>
#include <autoware_internal_planning_msgs/msg/path_point_with_lane_id.hpp>
#include <autoware_internal_planning_msgs/msg/planning_factor.hpp>
#include <autoware_internal_planning_msgs/msg/planning_factor_array.hpp>
#include <autoware_internal_planning_msgs/msg/safety_factor_array.hpp>
#include <autoware_planning_msgs/msg/path_point.hpp>
#include <autoware_planning_msgs/msg/trajectory_point.hpp>
#include <geometry_msgs/msg/pose.hpp>

#include <sstream>
#include <string>
#include <vector>

namespace autoware::planning_factor_interface
{

using autoware_internal_planning_msgs::msg::ControlPoint;
using autoware_internal_planning_msgs::msg::PlanningFactor;
using autoware_internal_planning_msgs::msg::PlanningFactorArray;
using autoware_internal_planning_msgs::msg::SafetyFactorArray;
using geometry_msgs::msg::Pose;

class PlanningFactorInterface
{
public:
  PlanningFactorInterface(
    rclcpp::Node * node, const std::string & name, bool enable_console_output = false,
    int throttle_duration_ms = 1000)
  : name_{name},
    pub_factors_{
      node->create_publisher<PlanningFactorArray>("/planning/planning_factors/" + name, 1)},
    clock_{node->get_clock()},
    enable_console_output_{enable_console_output},
    throttle_duration_ms_{throttle_duration_ms}
  {
  }

  /**
   * @brief factor setter for single control point.
   *
   * @param path points.
   * @param ego current pose.
   * @param control point pose. (e.g. stop or slow down point pose)
   * @param behavior of this planning factor.
   * @param safety factor.
   * @param driving direction.
   * @param target velocity of the control point.
   * @param shift length of the control point.
   * @param detail information.
   */
  template <class PointType>
  void add(
    const std::vector<PointType> & points, const Pose & ego_pose, const Pose & control_point_pose,
    const uint16_t behavior, const SafetyFactorArray & safety_factors,
    const bool is_driving_forward = true, const double velocity = 0.0,
    const double shift_length = 0.0, const std::string & detail = "")
  {
    const auto distance = static_cast<float>(autoware::motion_utils::calcSignedArcLength(
      points, ego_pose.position, control_point_pose.position));
    add(
      distance, control_point_pose, behavior, safety_factors, is_driving_forward, velocity,
      shift_length, detail);
  }

  /**
   * @brief factor setter for two control points (section).
   *
   * @param path points.
   * @param ego current pose.
   * @param control section start pose. (e.g. lane change start point pose)
   * @param control section end pose. (e.g. lane change end point pose)
   * @param behavior of this planning factor.
   * @param safety factor.
   * @param driving direction.
   * @param target velocity of the 1st control point.
   * @param target velocity of the 2nd control point.
   * @param shift length of the 1st control point.
   * @param shift length of the 2nd control point.
   * @param detail information.
   */
  template <class PointType>
  void add(
    const std::vector<PointType> & points, const Pose & ego_pose, const Pose & start_pose,
    const Pose & end_pose, const uint16_t behavior, const SafetyFactorArray & safety_factors,
    const bool is_driving_forward = true, const double start_velocity = 0.0,
    const double end_velocity = 0.0, const double start_shift_length = 0.0,
    const double end_shift_length = 0.0, const std::string & detail = "")
  {
    const auto start_distance = static_cast<float>(
      autoware::motion_utils::calcSignedArcLength(points, ego_pose.position, start_pose.position));
    const auto end_distance = static_cast<float>(
      autoware::motion_utils::calcSignedArcLength(points, ego_pose.position, end_pose.position));
    add(
      start_distance, end_distance, start_pose, end_pose, behavior, safety_factors,
      is_driving_forward, start_velocity, end_velocity, start_shift_length, end_shift_length,
      detail);
  }

  /**
   * @brief factor setter for single control point.
   *
   * @param distance to control point.
   * @param control point pose. (e.g. stop point pose)
   * @param behavior of this planning factor.
   * @param safety factor.
   * @param driving direction.
   * @param target velocity of the control point.
   * @param shift length of the control point.
   * @param detail information.
   */
  void add(
    const double distance, const Pose & control_point_pose, const uint16_t behavior,
    const SafetyFactorArray & safety_factors, const bool is_driving_forward = true,
    const double velocity = 0.0, const double shift_length = 0.0, const std::string & detail = "")
  {
    const auto control_point = autoware_internal_planning_msgs::build<ControlPoint>()
                                 .pose(control_point_pose)
                                 .velocity(velocity)
                                 .shift_length(shift_length)
                                 .distance(distance);

    const auto factor = autoware_internal_planning_msgs::build<PlanningFactor>()
                          .module(name_)
                          .is_driving_forward(is_driving_forward)
                          .control_points({control_point})
                          .behavior(behavior)
                          .detail(detail)
                          .safety_factors(safety_factors);

    factors_.push_back(factor);
  }

  /**
   * @brief factor setter for two control points (section).
   *
   * @param distance to control section start point.
   * @param distance to control section end point.
   * @param control section start pose. (e.g. lane change start point pose)
   * @param control section end pose. (e.g. lane change end point pose)
   * @param behavior of this planning factor.
   * @param safety factor.
   * @param driving direction.
   * @param target velocity of the 1st control point.
   * @param target velocity of the 2nd control point.
   * @param shift length of the 1st control point.
   * @param shift length of the 2nd control point.
   * @param detail information.
   */
  void add(
    const double start_distance, const double end_distance, const Pose & start_pose,
    const Pose & end_pose, const uint16_t behavior, const SafetyFactorArray & safety_factors,
    const bool is_driving_forward = true, const double start_velocity = 0.0,
    const double end_velocity = 0.0, const double start_shift_length = 0.0,
    const double end_shift_length = 0.0, const std::string & detail = "")
  {
    const auto control_start_point = autoware_internal_planning_msgs::build<ControlPoint>()
                                       .pose(start_pose)
                                       .velocity(start_velocity)
                                       .shift_length(start_shift_length)
                                       .distance(start_distance);

    const auto control_end_point = autoware_internal_planning_msgs::build<ControlPoint>()
                                     .pose(end_pose)
                                     .velocity(end_velocity)
                                     .shift_length(end_shift_length)
                                     .distance(end_distance);

    const auto factor = autoware_internal_planning_msgs::build<PlanningFactor>()
                          .module(name_)
                          .is_driving_forward(is_driving_forward)
                          .control_points({control_start_point, control_end_point})
                          .behavior(behavior)
                          .detail(detail)
                          .safety_factors(safety_factors);

    factors_.push_back(factor);
  }

  /**
   * @brief publish planning factors.
   */
  void publish()
  {
    PlanningFactorArray msg;
    msg.header.frame_id = "map";
    msg.header.stamp = clock_->now();
    msg.factors = factors_;

    pub_factors_->publish(msg);

    if (enable_console_output_ && !factors_.empty()) {
      print_factors_to_console(msg);
    }

    factors_.clear();
  }

private:
  /**
   * @brief Print message to console in YAML format
   * @param msg The message to print
   */
  void print_factors_to_console(const PlanningFactorArray & msg)
  {
    const std::string output_str =
      "Planning factor:\n" + autoware_internal_planning_msgs::msg::to_yaml(msg);
    if (throttle_duration_ms_ > 0) {
      RCLCPP_INFO_THROTTLE(
        rclcpp::get_logger(name_), *clock_, throttle_duration_ms_, "%s", output_str.c_str());
    } else {
      RCLCPP_INFO(rclcpp::get_logger(name_), "%s", output_str.c_str());
    }
  }

  std::string name_;

  rclcpp::Publisher<PlanningFactorArray>::SharedPtr pub_factors_;

  rclcpp::Clock::SharedPtr clock_;

  std::vector<PlanningFactor> factors_;

  bool enable_console_output_{false};
  int throttle_duration_ms_{0};
};

extern template void
PlanningFactorInterface::add<autoware_internal_planning_msgs::msg::PathPointWithLaneId>(
  const std::vector<autoware_internal_planning_msgs::msg::PathPointWithLaneId> &, const Pose &,
  const Pose &, const uint16_t behavior, const SafetyFactorArray &, const bool, const double,
  const double, const std::string &);
extern template void PlanningFactorInterface::add<autoware_planning_msgs::msg::PathPoint>(
  const std::vector<autoware_planning_msgs::msg::PathPoint> &, const Pose &, const Pose &,
  const uint16_t behavior, const SafetyFactorArray &, const bool, const double, const double,
  const std::string &);
extern template void PlanningFactorInterface::add<autoware_planning_msgs::msg::TrajectoryPoint>(
  const std::vector<autoware_planning_msgs::msg::TrajectoryPoint> &, const Pose &, const Pose &,
  const uint16_t behavior, const SafetyFactorArray &, const bool, const double, const double,
  const std::string &);

extern template void
PlanningFactorInterface::add<autoware_internal_planning_msgs::msg::PathPointWithLaneId>(
  const std::vector<autoware_internal_planning_msgs::msg::PathPointWithLaneId> &, const Pose &,
  const Pose &, const Pose &, const uint16_t behavior, const SafetyFactorArray &, const bool,
  const double, const double, const double, const double, const std::string &);
extern template void PlanningFactorInterface::add<autoware_planning_msgs::msg::PathPoint>(
  const std::vector<autoware_planning_msgs::msg::PathPoint> &, const Pose &, const Pose &,
  const Pose &, const uint16_t behavior, const SafetyFactorArray &, const bool, const double,
  const double, const double, const double, const std::string &);
extern template void PlanningFactorInterface::add<autoware_planning_msgs::msg::TrajectoryPoint>(
  const std::vector<autoware_planning_msgs::msg::TrajectoryPoint> &, const Pose &, const Pose &,
  const Pose &, const uint16_t behavior, const SafetyFactorArray &, const bool, const double,
  const double, const double, const double, const std::string &);

}  // namespace autoware::planning_factor_interface

#endif  // AUTOWARE__PLANNING_FACTOR_INTERFACE__PLANNING_FACTOR_INTERFACE_HPP_
