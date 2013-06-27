///////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2013, PAL Robotics S.L.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright
//     notice, this list of conditions and the following disclaimer in the
//     documentation and/or other materials provided with the distribution.
//   * Neither the name of hiDOF, Inc. nor the names of its
//     contributors may be used to endorse or promote products derived from
//     this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//////////////////////////////////////////////////////////////////////////////

/// \author Adolfo Rodriguez Tsouroukdissian

#ifndef SAFETY_LIMITS_INTERFACE_SOFT_JOINT_LIMITS_INTERFACE_H
#define SAFETY_LIMITS_INTERFACE_SOFT_JOINT_LIMITS_INTERFACE_H

#include <algorithm>
#include <cassert>

#include <boost/shared_ptr.hpp>

#include <ros/duration.h>

#include <hardware_interface/internal/resource_manager.h>
#include <hardware_interface/joint_command_interface.h>

#include <safety_limits_interface/joint_limits.h>
#include <safety_limits_interface/safety_limits_interface_exception.h>

namespace safety_limits_interface
{

namespace internal
{

template<typename T>
T saturate(const T val, const T min_val, const T max_val)
{
  return std::min(std::max(val, min_val), max_val);
}

}

/**
 * TODO
 */
class JointSoftLimitsHandle
{
public:
  /** \return Joint name. */
  std::string getName() const {return jh_.getName();}

protected:
  JointSoftLimitsHandle() {}

  JointSoftLimitsHandle(const hardware_interface::JointHandle& jh, const JointLimits& limits)
    : jh_(jh),
      limits_(limits)
  {}

  hardware_interface::JointHandle jh_;
  JointLimits limits_;
};

/** \brief A handle used to enforce position and velocity limits of a position-controlled joint.
 * TODO
 */

class PositionJointSoftLimitsHandle : public JointSoftLimitsHandle
{
public:
  PositionJointSoftLimitsHandle() {}

  PositionJointSoftLimitsHandle(const hardware_interface::JointHandle& jh,
                                const JointLimits&                     limits,
                                const SoftJointLimits&                 soft_limits)
    : JointSoftLimitsHandle(jh, limits),
      soft_limits_(soft_limits)
  {
    if (!limits.has_velocity_limits)
    {
      throw SafetyLimitsInterfaceException("Cannot enforce limits for joint '" + getName() +
                                           "'. It has no velocity limits specification.");
    }
  }

  /**
   * \brief Enforce joint limits. TODO
   * \param period Control period
   */
  void enforceLimits(const ros::Duration& period)
  {
    assert(period.toSec() > 0.0);

    using internal::saturate;

    // Current position
    const double pos = jh_.getPosition();

    // Velocity bounds
    double soft_min_vel = -limits_.max_velocity;
    double soft_max_vel =  limits_.max_velocity;

    if (limits_.has_position_limits)
    {
      // Velocity bounds depend on the velocity limit and the proximity to the position limit
      soft_min_vel = saturate(-soft_limits_.k_position * (pos - soft_limits_.min_position),
                              -limits_.max_velocity,
                               limits_.max_velocity);

      soft_max_vel = saturate(-soft_limits_.k_position * (pos - soft_limits_.max_position),
                              -limits_.max_velocity,
                               limits_.max_velocity);
    }

    // Position bounds
    const double dt = period.toSec();
    double pos_low  = pos + soft_min_vel * dt;
    double pos_high = pos + soft_max_vel * dt;

    if (limits_.has_position_limits)
    {
      // This extra measure safeguards against pathological cases, like when the soft limit lies beyond the hard limit
      pos_low  = std::max(pos_low,  limits_.min_position);
      pos_high = std::min(pos_high, limits_.max_position);
    }

    // Saturate position command according to bounds
    const double pos_cmd = saturate(jh_.getCommand(),
                                    pos_low,
                                    pos_high);
    jh_.setCommand(pos_cmd);
  }

private:
  SoftJointLimits soft_limits_;
};

/**
 * TODO
 */
class EffortJointSoftLimitsHandle : public JointSoftLimitsHandle
{
public:
  EffortJointSoftLimitsHandle() {}

  EffortJointSoftLimitsHandle(const hardware_interface::JointHandle& jh,
                              const JointLimits&                     limits,
                              const SoftJointLimits&                 soft_limits)
  : JointSoftLimitsHandle(jh, limits),
    soft_limits_(soft_limits)
  {
    if (!limits.has_velocity_limits)
    {
      throw SafetyLimitsInterfaceException("Cannot enforce limits for joint '" + getName() +
                                           "'. It has no velocity limits specification.");
    }
    if (!limits.has_effort_limits)
    {
      throw SafetyLimitsInterfaceException("Cannot enforce limits for joint '" + getName() +
                                           "'. It has no effort limits specification.");
    }
  }

  /**
   * \brief Enforce joint limits. If velocity or
   * \param period Control period
   */
  void enforceLimits(const ros::Duration& /*period*/)
  {
    using internal::saturate;

    // Current state
    const double pos = jh_.getPosition();
    const double vel = jh_.getVelocity();

    // Velocity bounds
    double soft_min_vel = -limits_.max_velocity;
    double soft_max_vel =  limits_.max_velocity;

    if (limits_.has_position_limits)
    {
      // Velocity bounds depend on the velocity limit and the proximity to the position limit
      soft_min_vel  = saturate(-soft_limits_.k_position * (pos - soft_limits_.min_position),
                               -limits_.max_velocity,
                                limits_.max_velocity);

      soft_max_vel = saturate(-soft_limits_.k_position * (pos - soft_limits_.max_position),
                              -limits_.max_velocity,
                               limits_.max_velocity);
    }

    // Effort bounds depend on the velocity and effort bounds
    const double soft_min_eff = saturate(-soft_limits_.k_velocity * (vel - soft_min_vel),
                                         -limits_.max_effort,
                                          limits_.max_effort);

    const double soft_max_eff = saturate(-soft_limits_.k_velocity * (vel - soft_max_vel),
                                         -limits_.max_effort,
                                          limits_.max_effort);

    // Saturate effort command according to bounds
    const double eff_cmd = saturate(jh_.getCommand(),
                                    soft_min_eff,
                                    soft_max_eff);
    jh_.setCommand(eff_cmd);
  }

private:
  SoftJointLimits soft_limits_;
};

/**
 * TODO
 */
class VelocityJointSaturationHandle : public JointSoftLimitsHandle
{
public:
  VelocityJointSaturationHandle () {}

  VelocityJointSaturationHandle(const hardware_interface::JointHandle& jh, const JointLimits& limits)
    : JointSoftLimitsHandle(jh, limits)
  {
    if (!limits.has_velocity_limits)
    {
      throw SafetyLimitsInterfaceException("Cannot enforce limits for joint '" + getName() +
                                           "'. It has no velocity limits specification.");
    }
  }

  /**
   * \brief Enforce joint limits. If velocity or
   * \param period Control period
   */
  void enforceLimits(const ros::Duration& /*period*/)
  {
    using internal::saturate;

    // Saturate velocity command according to limits
    const double vel_cmd = saturate(jh_.getCommand(),
                                   -limits_.max_velocity,
                                    limits_.max_velocity);
    jh_.setCommand(vel_cmd);
  }
};

/** \brief TODO
 */
template <class HandleType>
class JointLimitsInterface : public hardware_interface::ResourceManager<HandleType>
{
public:
  HandleType getHandle(const std::string& name)
  {
    // Rethrow exception with a meanungful type
    try
    {
      return this->hardware_interface::ResourceManager<HandleType>::getHandle(name);
    }
    catch(const std::logic_error& e)
    {
      throw SafetyLimitsInterfaceException(e.what());
    }
  }

  /** \name Real-Time Safe Functions
   *\{*/
  /** \brief Enforce limits for all managed handles. */
  void enforceLimits(const ros::Duration& period)
  {
    typedef typename hardware_interface::ResourceManager<HandleType>::ResourceMap::iterator ItratorType;
    for (ItratorType it = this->resource_map_.begin(); it != this->resource_map_.end(); ++it)
    {
      it->second.enforceLimits(period);
    }
  }
  /*\}*/
};

/** Interface for enforcing limits on a position-controlled joint with soft position limits. */
class PositionJointSoftLimitsInterface : public JointLimitsInterface<PositionJointSoftLimitsHandle> {};

/** Interface for enforcing limits on an effort-controlled joint with soft position limits. */
class EffortJointSoftLimitsInterface : public JointLimitsInterface<EffortJointSoftLimitsHandle> {};

/** Interface for enforcing limits on a velocity-controlled joint through saturation. */
class VelocityJointSaturationInterface : public JointLimitsInterface<VelocityJointSaturationHandle> {};

}

#endif
