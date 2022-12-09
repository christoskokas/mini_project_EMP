#pragma once

#ifndef MAP_H
#define MAP_H

#include "Camera.h"
#include "KeyFrame.h"
#include "PoseEstimator.h"
#include "FeatureManager.h"
#include "Settings.h"
#include "Optimizer.h"
#include <fstream>
#include <string>
#include <iostream>
#include <random>
#include <unordered_map>

namespace vio_slam
{

struct Observation
{
    int frameId {};

    cv::KeyPoint obs;
};

class MapPoint
{
    private:

    public:

        Eigen::Vector4d wp;
        Eigen::Vector3d wp3d;
        int trackCnt {0};
        // std::vector<Observation> obs;
        std::vector<cv::KeyPoint> obs;
        cv::Mat desc;

        bool isActive {true};

        bool inFrame {true};
        bool isOutlier {false};
        bool close {true};
        bool added {false};

        int keyFrameNb {0};
        const int idx;
        const int kdx;

        void setActive(bool act);
        void SetInFrame(bool infr);
        void SetIsOutlier(bool isOut);
        bool getActive() const;
        bool GetIsOutlier() const;
        bool GetInFrame() const;
        MapPoint(Eigen::Vector4d& p, const cv::Mat& _desc, cv::KeyPoint& obsK, bool _close, const int _kdx, const int _idx);


        void addTCnt();

        Eigen::Vector4d getWordPose4d();
        Eigen::Vector3d getWordPose3d();
        void setWordPose4d(Eigen::Vector4d& p);
        void updateMapPoint(Eigen::Vector4d& p, const cv::Mat& _desc, cv::KeyPoint& _obs);
};

class Map
{
    private:

    public:

        std::unordered_map<unsigned long, KeyFrame*> keyFrames;
        std::unordered_map<unsigned long, MapPoint*> mapPoints;
        std::vector<MapPoint*> activeMapPoints;
        unsigned long kIdx {0};
        unsigned long pIdx {0};
        Map(){};
        void addMapPoint(Eigen::Vector4d& p, const cv::Mat& _desc, cv::KeyPoint& obsK, bool _useable);
        void addMapPoint(MapPoint* mp);
        void addKeyFrame(Eigen::Matrix4d _pose);
        mutable std::mutex mapMutex;

    protected:
};

} // namespace vio_slam

#endif // MAP_H