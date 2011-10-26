// -*- coding: utf-8 -*-
// Copyright (C) 2011 Rosen Diankov <rosen.diankov@gmail.com>
//
// This file is part of OpenRAVE.
// OpenRAVE is free software: you can redistribute it and/or modify
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
// functions that allow plugins to program for the RAVE simulator
#include "../ravep.h"

#include "XFileHelper.h"
#include "XFileParser.h"

using namespace OpenRAVE;
using namespace std;

class XFileReader
{
public:
    XFileReader(EnvironmentBasePtr penv) : _penv(penv) {
    }

    void ReadFile(KinBodyPtr pbody, const std::string& filename, const AttributesList& atts)
    {
        std::ifstream f(filename.c_str());
        if( !f ) {
            throw OPENRAVE_EXCEPTION_FORMAT("failed to read %s filename",filename,ORE_InvalidArguments);
        }
        f.seekg(0,ios::end);
        string filedata; filedata.resize(f.tellg());
        f.seekg(0,ios::beg);
        f.read(&filedata[0], filedata.size());
        Read(pbody,filedata,atts);
        pbody->__struri = filename;
        if( filename.size() > 2 && pbody->GetName().size() == 0 ) {
            pbody->SetName(filename.substr(0,filename.size()-2));
        }
    }

    void ReadFile(RobotBasePtr probot, const std::string& filename, const AttributesList& atts)
    {
        std::ifstream f(filename.c_str());
        if( !f ) {
            throw OPENRAVE_EXCEPTION_FORMAT("failed to read %s filename",filename,ORE_InvalidArguments);
        }
        f.seekg(0,ios::end);
        string filedata; filedata.resize(f.tellg());
        f.seekg(0,ios::beg);
        f.read(&filedata[0], filedata.size());
        Read(probot,filedata,atts);
        probot->__struri = filename;
        if( filename.size() > 2 && probot->GetName().size() == 0 ) {
            probot->SetName(filename.substr(0,filename.size()-2));
        }
    }

    void Read(KinBodyPtr pbody, const std::string& data,const AttributesList& atts)
    {
        _ProcessAtts(atts, pbody);
        Assimp::XFileParser parser(data.c_str());
        _Read(pbody,parser.GetImportedData());
        pbody->SetName("body");
    }

    void Read(RobotBasePtr probot, const std::string& data,const AttributesList& atts)
    {
        _ProcessAtts(atts,probot);
        Assimp::XFileParser parser(data.c_str());
        _Read(probot,parser.GetImportedData());
        probot->SetName("robot");
        // add manipulators
        FOREACH(itmanip,_listendeffectors) {
            RobotBase::ManipulatorPtr pmanip(new RobotBase::Manipulator(probot));
            pmanip->_name = itmanip->first->_name;
            pmanip->_pEndEffector = itmanip->first;
            pmanip->_pBase = probot->GetLinks().at(0);
            pmanip->_tLocalTool = itmanip->second;
            probot->_vecManipulators.push_back(pmanip);
        }
    }

protected:
    void _ProcessAtts(const AttributesList& atts, KinBodyPtr pbody)
    {
        _listendeffectors.clear();
        _vScaleGeometry = Vector(0.001,0.001,0.001);
        _bSkipGeometry = false;
        _prefix = "";
        FOREACHC(itatt,atts) {
            if( itatt->first == "skipgeometry" ) {
                _bSkipGeometry = stricmp(itatt->second.c_str(), "true") == 0 || itatt->second=="1";
            }
            else if( itatt->first == "scalegeometry" ) {
                stringstream ss(itatt->second);
                _vScaleGeometry = Vector(0.001,0.001,0.001);
                ss >> _vScaleGeometry.x >> _vScaleGeometry.y >> _vScaleGeometry.z;
                if( !ss ) {
                    _vScaleGeometry.z = _vScaleGeometry.y = _vScaleGeometry.x;
                }
            }
            else if( itatt->first == "prefix" ) {
                _prefix = itatt->second;
            }
            else if( itatt->first == "name" ) {
                pbody->SetName(itatt->second);
            }
        }
    }

    void _Read(KinBodyPtr pbody, const Assimp::XFile::Scene* scene)
    {
        BOOST_ASSERT(!!scene);
        pbody->Destroy();
        _Read(pbody, KinBody::LinkPtr(), scene->mRootNode, Transform());
    }

    void _Read(KinBodyPtr pbody, KinBody::LinkPtr plink, const Assimp::XFile::Node* node, const Transform& transparent)
    {
        BOOST_ASSERT(!!node);
        Transform tnode = transparent * ExtractTransform(node->mTrafoMatrix);

        RAVELOG_VERBOSE(str(boost::format("node=%s, parent=%s, children=%d, meshes=%d, pivot=%d")%node->mName%(!node->mParent ? string() : node->mParent->mName)%node->mChildren.size()%node->mMeshes.size()%(!!node->mFramePivot)));

        if( !!node->mFramePivot ) {
            Transform tpivot = tnode*ExtractTransform(node->mFramePivot->mPivotMatrix);

            KinBody::JointPtr pjoint(new KinBody::Joint(pbody));
            if( node->mFramePivot->mType == 1 ) {
                pjoint->_type = KinBody::Joint::JointRevolute;
                pjoint->_vlowerlimit[0] = -PI;
                pjoint->_vupperlimit[0] = PI;
            }
            else if( node->mFramePivot->mType == 2 ) {
                pjoint->_type = KinBody::Joint::JointPrismatic;
                pjoint->_vlowerlimit[0] = -1;
                pjoint->_vupperlimit[0] = 1;
            }
            else {
                if( node->mFramePivot->mType != 0 ) {
                    RAVELOG_WARN(str(boost::format("unknown joint type %d")%node->mFramePivot->mType));
                }
                pjoint.reset();
            }

            KinBody::LinkPtr pchildlink;
            if( !!pjoint || !plink ) {
                pchildlink.reset(new KinBody::Link(pbody));
                pchildlink->_name = node->mName;
                pchildlink->_t = tpivot;
                pchildlink->_bStatic = false;
                pchildlink->_bIsEnabled = true;
                pchildlink->_index = pbody->_veclinks.size();
                pbody->_veclinks.push_back(pchildlink);
            }

            if( !!pjoint ) {
                if( node->mFramePivot->mAttribute & 2 ) {
                    // usually this is set for the first and last joints of the file?
                }
                if( node->mFramePivot->mAttribute & 4 ) {
                    // end effector?
                    _listendeffectors.push_back(make_pair(pchildlink,pchildlink->_t.inverse()*tpivot));
                }

                pjoint->_name = node->mFramePivot->mName;
                pjoint->_bIsCircular[0] = false;
                std::vector<Vector> vaxes(1);
                Transform t = plink->_t.inverse()*tpivot;
                vaxes[0] = Vector(node->mFramePivot->mMotionDirection.x, node->mFramePivot->mMotionDirection.y, node->mFramePivot->mMotionDirection.z);
                vaxes[0] = t.rotate(vaxes[0]);
                std::vector<dReal> vcurrentvalues;
                pjoint->_ComputeInternalInformation(plink,pchildlink,t.trans,vaxes,vcurrentvalues);
                pbody->_vecjoints.push_back(pjoint);
            }

            if( !!pchildlink ) {
                plink = pchildlink;
            }
        }

        FOREACH(it,node->mMeshes) {
            Assimp::XFile::Mesh* pmesh = *it;
            plink->_listGeomProperties.push_back(KinBody::Link::GEOMPROPERTIES(plink));
            KinBody::Link::GEOMPROPERTIES& g = plink->_listGeomProperties.back();
            g._t = plink->_t.inverse() * tnode;
            g._type = KinBody::Link::GEOMPROPERTIES::GeomTrimesh;
            g.collisionmesh.vertices.resize(pmesh->mPositions.size());
            for(size_t i = 0; i < pmesh->mPositions.size(); ++i) {
                g.collisionmesh.vertices[i] = Vector(pmesh->mPositions[i].x*_vScaleGeometry.x,pmesh->mPositions[i].y*_vScaleGeometry.y,pmesh->mPositions[i].z*_vScaleGeometry.z);
            }
            size_t numindices = 0;
            for(size_t iface = 0; iface < pmesh->mPosFaces.size(); ++iface) {
                numindices += 3*(pmesh->mPosFaces[iface].mIndices.size()-2);
            }
            g.collisionmesh.indices.resize(numindices);
            std::vector<int>::iterator itindex = g.collisionmesh.indices.begin();
            for(size_t iface = 0; iface < pmesh->mPosFaces.size(); ++iface) {
                for(size_t i = 2; i < pmesh->mPosFaces[iface].mIndices.size(); ++i) {
                    *itindex++ = pmesh->mPosFaces[iface].mIndices.at(0);
                    *itindex++ = pmesh->mPosFaces[iface].mIndices.at(1);
                    *itindex++ = pmesh->mPosFaces[iface].mIndices.at(i);
                }
            }

            size_t matindex = 0;
            if( pmesh->mFaceMaterials.size() > 0 ) {
                matindex = pmesh->mFaceMaterials.at(0);
            }
            if( matindex < pmesh->mMaterials.size() ) {
                const Assimp::XFile::Material& mtrl = pmesh->mMaterials.at(matindex);
                g.diffuseColor = Vector(mtrl.mDiffuse.r, mtrl.mDiffuse.g, mtrl.mDiffuse.b, mtrl.mDiffuse.a);
                g.ambientColor = Vector(mtrl.mEmissive.r, mtrl.mEmissive.g, mtrl.mEmissive.b, 1);
            }
        }

        FOREACH(it,node->mChildren) {
            _Read(pbody, plink, *it,tnode);
        }
    }

    Transform ExtractTransform(const aiMatrix4x4& aimatrix)
    {
        TransformMatrix tmnode;
        tmnode.m[0] = aimatrix.a1; tmnode.m[1] = aimatrix.a2; tmnode.m[2] = aimatrix.a3; tmnode.trans[0] = aimatrix.a4*_vScaleGeometry.x;
        tmnode.m[4] = aimatrix.b1; tmnode.m[5] = aimatrix.b2; tmnode.m[6] = aimatrix.b3; tmnode.trans[1] = aimatrix.b4*_vScaleGeometry.y;
        tmnode.m[8] = aimatrix.c1; tmnode.m[9] = aimatrix.c2; tmnode.m[10] = aimatrix.c3; tmnode.trans[2] = aimatrix.c4*_vScaleGeometry.z;
        return tmnode;
    }

    EnvironmentBasePtr _penv;
    std::string _prefix;
    Vector _vScaleGeometry;
    std::list< pair<KinBody::LinkPtr, Transform> > _listendeffectors;
    bool _bSkipGeometry;
};

bool RaveParseXFile(EnvironmentBasePtr penv, KinBodyPtr& ppbody, const std::string& filename,const AttributesList& atts)
{
    if( !ppbody ) {
        ppbody = RaveCreateKinBody(penv,"");
    }
    XFileReader reader(penv);
    reader.ReadFile(ppbody,filename,atts);
    return true;
}

bool RaveParseXFile(EnvironmentBasePtr penv, RobotBasePtr& pprobot, const std::string& filename,const AttributesList& atts)
{
    if( !pprobot ) {
        pprobot = RaveCreateRobot(penv,"GenericRobot");
    }
    XFileReader reader(penv);
    reader.ReadFile(pprobot,filename,atts);
    return true;
}

bool RaveParseXData(EnvironmentBasePtr penv, KinBodyPtr& ppbody, const std::string& data,const AttributesList& atts)
{
    if( !ppbody ) {
        ppbody = RaveCreateKinBody(penv,"");
    }
    XFileReader reader(penv);
    reader.Read(ppbody,data,atts);
    return true;
}

bool RaveParseXData(EnvironmentBasePtr penv, RobotBasePtr& pprobot, const std::string& data,const AttributesList& atts)
{
    if( !pprobot ) {
        pprobot = RaveCreateRobot(penv,"GenericRobot");
    }
    XFileReader reader(penv);
    reader.Read(pprobot,data,atts);
    return true;
}
