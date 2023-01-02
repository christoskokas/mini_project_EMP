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

class KeyFrame;

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
        int seenCnt {1};
        // std::vector<Observation> obs;
        std::vector<cv::KeyPoint> obs;
        cv::Mat desc;
        std::unordered_map<KeyFrame*, size_t> kFWithFIdx;

        bool isActive {true};

        bool inFrame {true};
        bool isOutlier {false};
        bool close {true};
        bool added {false};

        int keyFrameNb {0};
        const unsigned long idx;
        const unsigned long kdx;

        void eraseKFConnection(KeyFrame* kF);
        void setActive(bool act);
        void SetInFrame(bool infr);
        void SetIsOutlier(bool isOut);
        bool getActive() const;
        bool GetIsOutlier() const;
        bool GetInFrame() const;
        MapPoint(const Eigen::Vector4d& p, const cv::Mat& _desc, const cv::KeyPoint& obsK, const bool _close, const unsigned long _kdx, const unsigned long _idx);
        MapPoint(const unsigned long _idx, const unsigned long _kdx);

        void copyMp(const MapPoint* mp);


        // MapPoint operator = (MapPoint const& obj)
        // {
        //     MapPoint res(obj.idx, obj.kdx);

        // }

        void addTCnt();

        Eigen::Vector4d getWordPose4d() const;
        Eigen::Vector3d getWordPose3d() const;
        void updatePos(const Eigen::Matrix4d& camPoseInv, const Zed_Camera* zedPtr);
        void setWordPose4d(const Eigen::Vector4d& p);
        void setWordPose3d(const Eigen::Vector3d& p);
        void updateMapPoint(Eigen::Vector4d& p, const cv::Mat& _desc, cv::KeyPoint& _obs);
};

class Map
{
    private:

    public:

        bool endOfFrames {false};

        std::unordered_map<unsigned long, KeyFrame*> keyFrames;
        std::unordered_map<unsigned long, MapPoint*> mapPoints;
        std::vector<MapPoint*> activeMapPoints;
        std::vector<KeyFrame*> activeKeyFrames;
        unsigned long kIdx {0};
        unsigned long pIdx {0};
        
        bool keyFrameAdded {false};
        bool frameAdded {false};
        bool LBADone {false};
        int endLBAIdx {0};

        Map(){};
        void addMapPoint(Eigen::Vector4d& p, const cv::Mat& _desc, cv::KeyPoint& obsK, bool _useable);
        void addMapPoint(MapPoint* mp);
        void addKeyFrame(Eigen::Matrix4d _pose);
        void addKeyFrame(KeyFrame* kF);
        void removeKeyFrame(int idx);
        mutable std::mutex mapMutex;

    protected:
};

} // namespace vio_slam

#endif // MAP_H