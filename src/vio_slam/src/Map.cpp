#include "Map.h"

namespace vio_slam
{

MapPoint::MapPoint(const Eigen::Vector4d& p, const cv::Mat& _desc, const cv::KeyPoint& obsK, const bool _close, const unsigned long _kdx, const unsigned long _idx) : close(_close), wp(p), kdx(_kdx), idx(_idx)
{
    wp3d = Eigen::Vector3d(p(0), p(1), p(2));
    obs.push_back(obsK);
    desc.push_back(_desc);
}

void MapPoint::updatePos(const Eigen::Matrix4d& camPoseInv, const Zed_Camera* zedPtr)
{
    Eigen::Vector4d p = wp;
    p = camPoseInv * p;

    const double invZ = 1.0/p(2);
    const double fx {zedPtr->cameraLeft.fx};
    const double fy {zedPtr->cameraLeft.fy};
    const double cx {zedPtr->cameraLeft.cx};
    const double cy {zedPtr->cameraLeft.cy};

    const double u {fx*p(0)*invZ + cx};
    const double v {fy*p(1)*invZ + cy};
    obs[0].pt = cv::Point2f((float)u, (float)v);
}

bool MapPoint::GetInFrame() const
{
    return inFrame;
}

bool MapPoint::GetIsOutlier() const
{
    return isOutlier;
}

bool MapPoint::getActive() const
{
    return isActive;
}

void MapPoint::SetInFrame(bool infr)
{
    inFrame = infr;
}

void MapPoint::SetIsOutlier(bool isOut)
{
    isOutlier = isOut;
}

void MapPoint::setActive(bool act)
{
    isActive = act;
}

Eigen::Vector4d MapPoint::getWordPose4d()
{
    return wp;
}

Eigen::Vector3d MapPoint::getWordPose3d()
{
    return wp3d;
}



void MapPoint::setWordPose4d(const Eigen::Vector4d& p)
{
    wp = p;
    wp3d = Eigen::Vector3d(p(0), p(1), p(2));
}

void MapPoint::setWordPose3d(const Eigen::Vector3d& p)
{
    wp3d = p;
    wp = Eigen::Vector4d(p(0), p(1), p(2), 1.0);
}

void MapPoint::addTCnt()
{
    trackCnt++;
}

void MapPoint::updateMapPoint(Eigen::Vector4d& p, const cv::Mat& _desc, cv::KeyPoint& _obs)
{
    setWordPose4d(p);
    addTCnt();
    obs.push_back(_obs);
    desc.push_back(_desc);

}

void Map::addMapPoint(Eigen::Vector4d& p, const cv::Mat& _desc, cv::KeyPoint& obsK, bool _useable)
{
    MapPoint* mp = new MapPoint(p, _desc, obsK, _useable, kIdx, pIdx);
    mp->added = true;
    mapPoints.insert(std::pair<unsigned long, MapPoint*>(pIdx, mp));
    pIdx++;
}

void Map::addMapPoint(MapPoint* mp)
{
    mp->added = true;
    mapPoints.insert(std::pair<unsigned long, MapPoint*>(pIdx, mp));
    pIdx++;
}

void Map::removeKeyFrame(int idx)
{
    keyFrames.erase((unsigned long) idx);
}

void Map::addKeyFrame(Eigen::Matrix4d _pose)
{
    KeyFrame* kF = new KeyFrame(_pose, kIdx);
    keyFrames.insert(std::pair<unsigned long, KeyFrame*>(kIdx, kF));
    kIdx ++;
}

void Map::addKeyFrame(KeyFrame* kF)
{
    keyFrames.insert(std::pair<unsigned long, KeyFrame*>(kIdx, kF));
    kIdx ++;
}

} // namespace vio_slam