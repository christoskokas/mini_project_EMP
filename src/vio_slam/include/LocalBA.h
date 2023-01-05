#pragma once

#ifndef LOCALBA_H
#define LOCALBA_H


#include "Camera.h"
#include "KeyFrame.h"
#include "Map.h"
#include "PoseEstimator.h"
#include "FeatureManager.h"
#include "FeatureExtractor.h"
#include "FeatureMatcher.h"
#include "Conversions.h"
#include "Settings.h"
#include "Optimizer.h"
#include "Eigen/Dense"
#include <fstream>
#include <string>
#include <iostream>
#include <random>

namespace vio_slam
{


class LocalMapper
{
    private:

    public:

        const double fx,fy,cx,cy;

        Map* map;

        const float reprjThreshold {7.815f};

        const int actvKFMaxSize {10};
        const int minCount {3};

        Zed_Camera* zedPtr;

        FeatureMatcher* fm;

        LocalMapper(Map* _map, Zed_Camera* _zedPtr, FeatureMatcher* _fm);

        void predictKeysPos(TrackedKeys& keys, const Eigen::Matrix4d& curPose, const Eigen::Matrix4d& camPoseInv, std::vector<float>& keysAngles, const std::vector<Eigen::Vector4d>& p4d, std::vector<cv::Point2f>& predPoints);
        void calcp4d(KeyFrame* lastKF, std::vector<Eigen::Vector4d>& p4d);
        void beginLocalMapping();
        void computeAllMapPoints(std::vector<vio_slam::KeyFrame *>& actKeyF);
        void localBA(std::vector<vio_slam::KeyFrame *>& actKeyF);
        Eigen::Vector3d TriangulateMultiViewPoint(
                const std::vector<Eigen::Matrix<double, 3, 4>>& proj_matrices,
                const std::vector<Eigen::Vector2d>& points);
        bool checkOutlier(const Eigen::Matrix3d& K, const Eigen::Vector2d& obs, const Eigen::Vector3d posW,const Eigen::Vector3d& tcw, const Eigen::Quaterniond& qcw, const float thresh);
        Eigen::Vector3d get3d(const cv::KeyPoint& key, const float depth);
        void triangulateCeres(Eigen::Vector3d& p3d, const std::vector<Eigen::Matrix<double, 3, 4>>& proj_matrices, const std::vector<Eigen::Vector2d>& obs, const Eigen::Matrix4d& lastKFPose);
        void triangulateCeresNew(Eigen::Vector3d& p3d, const std::vector<Eigen::Matrix<double, 3, 4>>& proj_matrices, const std::vector<Eigen::Vector2d>& obs, const Eigen::Matrix4d& lastKFPose, bool first);
        void calcProjMatrices(std::unordered_map<int, Eigen::Matrix<double,3,4>>& projMatrices, std::vector<KeyFrame*>& actKeyF);

        void calcAllMpsOfKF(std::vector<std::vector<std::pair<int, int>>>& matchedIdxs, KeyFrame* lastKF, std::vector<vio_slam::KeyFrame *>& actKeyF, const int kFsize, std::vector<Eigen::Vector4d>& p4d);

        void processMatches(std::vector<std::pair<int, int>>& matchesOfPoint, std::unordered_map<int, Eigen::Matrix<double,3,4>>& allProjMatrices, std::vector<Eigen::Matrix<double, 3, 4>>& proj_matrices, std::vector<Eigen::Vector2d>& points, std::vector<KeyFrame*>& actKeyF);
        bool checkReprojErr(Eigen::Vector4d& calcVec, std::vector<std::pair<int, int>>& matchesOfPoint, const std::unordered_map<int, Eigen::Matrix<double,3,4>>& allProjMatrices);
        bool checkReprojErrNew(KeyFrame* lastKF, const int keyPos, Eigen::Vector4d& calcVec, std::vector<std::pair<int, int>>& matchesOfPoint, const std::unordered_map<int, Eigen::Matrix<double,3,4>>& allProjMatrices, std::vector<Eigen::Matrix<double, 3, 4>>& proj_mat, std::vector<Eigen::Vector2d>& pointsVec);
        void projectToPlane(Eigen::Vector4d& vec, cv::Point2f& p2f);

        void triangulateNewPoints();

        void addMultiViewMapPoints(const Eigen::Vector4d& posW, const std::vector<std::pair<int, int>>& matchesOfPoint, std::vector<MapPoint*>& pointsToAdd, KeyFrame* lastKF, const size_t& keyPos);
        void addToMap(KeyFrame* lastKF, const std::vector<MapPoint*>& pointsToAdd);
        void addToMapRemoveCon(KeyFrame* lastKF, std::vector<MapPoint*>& pointsToAdd, std::vector<std::vector<std::pair<int, int>>>& matchedIdxs);
        void removeCon(MapPoint* mp, std::vector<std::pair<int, int>>& matchesOfPoint, const int lastKFNumb);

        void drawPred(KeyFrame* lastKF, std::vector<cv::KeyPoint>& keys,std::vector<cv::KeyPoint>& predKeys);
        void drawPred(KeyFrame* lastKF, std::vector<cv::KeyPoint>& keys,std::vector<cv::Point2f>& predKeys);
        void drawLBA(const char* com,std::vector<std::vector<std::pair<int, int>>>& matchedIdxs, const KeyFrame* lastKF, const KeyFrame* otherKF);

};



} // namespace vio_slam


#endif // LOCALBA_H