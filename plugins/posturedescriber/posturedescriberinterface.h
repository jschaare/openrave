// -*- coding: utf-8 -*-
// Copyright (C) 2006-2020 Guangning Tan, Kei Usui, Rosen Diankov <rosen.diankov@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef PLUGINS_POSTUREDESCRIBER_POSTUREDESCRIBERINTERFACE_H
#define PLUGINS_POSTUREDESCRIBER_POSTUREDESCRIBERINTERFACE_H

#include <openrave/posturedescriber.h> // PostureDescriberBasePtr
#include "posturesupporttype.h" // NeighbouringTwoJointsRelation, RobotPostureSupportType

namespace OpenRAVE {

using PostureValueFn = std::function<void(const std::vector<KinBody::JointPtr>& vjoints, const double fTol, std::vector<uint16_t>& posturestates)>;

class OPENRAVE_API PostureDescriber : public PostureDescriberBase
{
public:
    PostureDescriber() = delete;
    PostureDescriber(EnvironmentBasePtr penv, const double fTol = 1e-6);
    virtual ~PostureDescriber();

    /// \brief Initialize with a kinematics chain
    virtual bool Init(const LinkPair& kinematicsChain) override;

    /// \brief Checks if this class can be used to compute posture values for this robot
    /// \return true if can handle this kinematics chain
    virtual bool Supports(const LinkPair& kinematicsChain) const override;

    /// \brief Computes an integer value to describe current robot posture
    /// Computes a value describing descrete posture of robot kinematics between baselink and eelink
    virtual bool ComputePostureStates(std::vector<uint16_t>& values, const std::vector<double>& jointvalues = {}) override;

    /// \brief Set the tolerance for determining whether a robot posture value is close to 0 (i.e. singularity, branch point)
    bool SetPostureValueThreshold(const double fTol);

    const std::vector<KinBody::JointPtr>& GetJoints() const {
        return _joints;
    }

protected:
    /// \brief Gets joints along a kinematics chain from baselink to eelink
    void _GetJointsFromKinematicsChain(const LinkPair& kinematicsChain,
                                       std::vector<KinBody::JointPtr>& vjoints) const;

    /// \brief `SendCommand` APIs
    bool _SetPostureValueThresholdCommand(std::ostream& ssout, std::istream& ssin);
    bool _GetPostureValueThresholdCommand(std::ostream& ssout, std::istream& ssin) const;
    bool _GetArmIndicesCommand(std::ostream& ssout, std::istream& ssin) const;

    LinkPair _kinematicsChain; ///< baselink and eelink
    std::vector<KinBody::JointPtr> _joints; ///< joints from baselink to eelink
    std::vector<int> _armindices; ///< dof indices from baselink to eelink
    double _fTol = 1e-6; ///< tolerance for computing robot posture values
    PostureValueFn _posturefn; ///< function that computes posture values and states for a kinematics chain
};

using PostureDescriberPtr = boost::shared_ptr<PostureDescriber>;
using PostureFormulation = std::array<std::array<int, 2>, 3>;

inline uint16_t compute_single_state(const double x, const double fTol) {
    return (x > fTol) ? 0 : (x < -fTol) ? 1 : 2; // >= or <= ?
}

template <size_t N>
inline void compute_robot_posture_states(const std::array<double, N>& posturevalues,
                                         const double fTol,
                                         std::vector<uint16_t>& posturestates) {
    std::array<uint16_t, N> singlestates;
    for(size_t i = 0; i < N; ++i) {
        singlestates[i] = compute_single_state(posturevalues[i], fTol);
    }

    posturestates = {0};
    posturestates.reserve(1 << N);
    for(size_t i = 0; i < N; ++i) {
        for(uint16_t &state : posturestates) {
            state <<= 1;
        }
        if(singlestates[i] == 1) {
            for(uint16_t &state : posturestates) {
                state |= 1;
            }
        }
        else if (singlestates[i] == 2) {
            const size_t nstates = posturestates.size();
            posturestates.insert(end(posturestates), begin(posturestates), end(posturestates));
            for(size_t j = nstates; j < 2 * nstates; ++j) {
                posturestates[j] |= 1;
            }
        }
    }
}
} // namespace OpenRAVE

#endif // PLUGINS_POSTUREDESCRIBER_POSTUREDESCRIBERINTERFACE_H