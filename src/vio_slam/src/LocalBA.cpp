#include "LocalBA.h"

namespace vio_slam
{

LocalMapper::LocalMapper(Map* _map, Zed_Camera* _zedPtr, FeatureMatcher* _fm) : map(_map), zedPtr(_zedPtr), fm(_fm), fx(_zedPtr->cameraLeft.fx), fy(_zedPtr->cameraLeft.fy), cx(_zedPtr->cameraLeft.cx), cy(_zedPtr->cameraLeft.cy)
{

}

// From ColMap/src/base/triangulation.cc

Eigen::Vector3d LocalMapper::TriangulateMultiViewPoint(
    const std::vector<Eigen::Matrix<double, 3, 4>>& proj_matrices,
    const std::vector<Eigen::Vector2d>& points) {
  CHECK_EQ(proj_matrices.size(), points.size());

  Eigen::Matrix4d A = Eigen::Matrix4d::Zero();

  for (size_t i = 0; i < points.size(); i++) {
    const Eigen::Vector3d point = points[i].homogeneous().normalized();
    const Eigen::Matrix<double, 3, 4> term =
        proj_matrices[i] - point * point.transpose() * proj_matrices[i];
    A += term.transpose() * term;
  }

  Eigen::SelfAdjointEigenSolver<Eigen::Matrix4d> eigen_solver(A);

  return eigen_solver.eigenvectors().col(0).hnormalized();
}

void LocalMapper::calcProjMatrices(std::unordered_map<int, Eigen::Matrix<double,3,4>>& projMatrices, std::vector<KeyFrame*>& actKeyF)
{
    Eigen::Matrix<double,3,3>& K = zedPtr->cameraLeft.intrisics;
    Eigen::Matrix<double,3,3>& Kr = zedPtr->cameraRight.intrisics;
    const int aKFsize {actKeyF.size()};
    Eigen::Matrix4d projL = Eigen::Matrix4d::Identity();
    projL.block<3,3>(0,0) = K;
    Eigen::Matrix4d projR = Eigen::Matrix4d::Identity();
    // projR.block<3,3>(0,0) = Kr;
    projR.block<3,3>(0,0) = K;
    projR(0,3) = -fx * zedPtr->extrinsics(0,3);
    std::vector<KeyFrame*>::const_iterator it, end(actKeyF.end());
    bool first = true;
    for ( it = actKeyF.begin(); it != end; it++)
    {
        const int kIdx {(*it)->numb};
        Eigen::Matrix<double,4,4> extr2 = (*it)->pose.poseInverse;
        extr2 = projL * extr2;
        Eigen::Matrix<double,3,4> extr = extr2.block<3,4>(0,0);
        if ( first )
        {
            Eigen::Matrix<double,4,4> extrRight = (*it)->pose.poseInverse;
            extrRight =  projR * extrRight;
            Eigen::Matrix<double,3,4> extrR = extrRight.block<3,4>(0,0);
            projMatrices.emplace( - kIdx, extrR);
            first = false;
        }
        projMatrices.emplace(kIdx, extr);
        // if ( kIdx != 0 )
        // {
        //     projMatrices.emplace(kIdx, extr);
        //     // projMatrices.emplace( - kIdx, extrRight2);
        // }
        // else
        // {
        //     projMatrices.emplace(aKFsize, extr);
        //     projMatrices.emplace( - aKFsize, extrRight2);
        // }
    }
}

void LocalMapper::drawLBA(const char* com,std::vector<std::vector<std::pair<int, int>>>& matchedIdxs, const KeyFrame* lastKF, const KeyFrame* otherKF)
{
    std::vector<cv::Point2f> last, other;
    const int kIdx {otherKF->numb};
    for (size_t i {0}, end{lastKF->keys.keyPoints.size()}; i < end; i ++)
    {
        for (size_t j{0}, jend{matchedIdxs[i].size()}; j < jend; j ++)
        {
            if (matchedIdxs[i][j].first == kIdx)
            {
                last.emplace_back(lastKF->keys.keyPoints[i].pt);
                other.emplace_back(otherKF->keys.keyPoints[matchedIdxs[i][j].second].pt);
            }
        }
    }
    cv::Mat im = otherKF->rLeftIm.clone();
    for ( size_t i{0}, end{last.size()}; i < end; i ++)
    {
        cv::circle(im, last[i],2,cv::Scalar(0,255,0));
        cv::line(im, last[i], other[i],cv::Scalar(0,0,255));
        cv::circle(im, other[i],2,cv::Scalar(255,0,0));

    }
    cv::imshow(com, im);
    cv::waitKey(1);
}

void LocalMapper::drawPred(KeyFrame* lastKF, std::vector<cv::KeyPoint>& keys,std::vector<cv::KeyPoint>& predKeys)
{
    cv::Mat im = lastKF->rLeftIm.clone();
    for ( size_t i{0}, end{keys.size()}; i < end; i ++)
    {
        if ( predKeys[i].pt.x <= 0 )
            continue;
        cv::circle(im, keys[i].pt,2,cv::Scalar(0,255,0));
        cv::line(im, keys[i].pt, predKeys[i].pt,cv::Scalar(0,0,255));
        cv::circle(im, predKeys[i].pt,2,cv::Scalar(255,0,0));
    }
    cv::imshow("predictions", im);
    cv::waitKey(1);
}

void LocalMapper::drawPred(KeyFrame* lastKF, std::vector<cv::KeyPoint>& keys,std::vector<cv::Point2f>& predKeys)
{
    cv::Mat im = lastKF->rLeftIm.clone();
    for ( size_t i{0}, end{keys.size()}; i < end; i ++)
    {
        if ( predKeys[i].x <= 0 )
            continue;
        cv::circle(im, keys[i].pt,2,cv::Scalar(0,255,0));
        cv::line(im, keys[i].pt, predKeys[i],cv::Scalar(0,0,255));
        cv::circle(im, predKeys[i],2,cv::Scalar(255,0,0));
    }
    cv::imshow("New predictions", im);
    cv::waitKey(1);
}

void LocalMapper::predictKeysPos(TrackedKeys& keys, const Eigen::Matrix4d& curPose, const Eigen::Matrix4d& camPoseInv, std::vector<float>& keysAngles, const std::vector<Eigen::Vector4d>& p4d, std::vector<cv::Point2f>& predPoints)
{
    keys.predKeyPoints = keys.keyPoints;
    predPoints.resize(keys.keyPoints.size());
    keysAngles.resize(keys.keyPoints.size(), 0.0);
    for ( size_t i {0}, end{keys.keyPoints.size()}; i < end; i ++)
    {
        if ( keys.estimatedDepth[i] <= 0 )
        {
            predPoints[i] = cv::Point2f(-1,-1);
            continue;
        }
        Eigen::Vector4d p = camPoseInv * p4d[i];

        if ( p(2) <= 0.0)
        {
            predPoints[i] = cv::Point2f(-1,-1);
            continue;
        }

        const double invZ = 1.0f/p(2);


        double u {fx*p(0)*invZ + cx};
        double v {fy*p(1)*invZ + cy};

        const int w {zedPtr->mWidth};
        const int h {zedPtr->mHeight};

        if ( u < 0 )
            u = 0.0;
        if ( v < 0 )
            v = 0.0;
        if ( u >= w )
            u = w - 1.0;
        if ( v >= h )
            v = h - 1.0;

        predPoints[i] = cv::Point2f((float)u, (float)v);
        keysAngles[i] = atan2((float)v - keys.keyPoints[i].pt.y, (float)u - keys.keyPoints[i].pt.x);

    }
}

void LocalMapper::processMatches(std::vector<std::pair<int, int>>& matchesOfPoint, std::unordered_map<int, Eigen::Matrix<double,3,4>>& allProjMatrices, std::vector<Eigen::Matrix<double, 3, 4>>& proj_matrices, std::vector<Eigen::Vector2d>& points, std::vector<KeyFrame*>& actKeyF)
{
    proj_matrices.reserve(matchesOfPoint.size());
    points.reserve(matchesOfPoint.size());
    const int aKFSize = actKeyF.size();
    std::vector<std::pair<int, int>>::const_iterator it, end(matchesOfPoint.end());
    for ( it = matchesOfPoint.begin(); it != end; it++)
    {
        proj_matrices.emplace_back(allProjMatrices.at(it->first));
        int kFIdx = it->first;

        if ( it->first >= 0)
        {
            KeyFrame* kF = map->keyFrames.at(kFIdx);
            Eigen::Vector2d vec2d((double)kF->keys.keyPoints[it->second].pt.x, (double)kF->keys.keyPoints[it->second].pt.y);
            points.emplace_back(vec2d);
        }
        else
        {
            KeyFrame* kF = map->keyFrames.at( - kFIdx);
            Eigen::Vector2d vec2d((double)kF->keys.rightKeyPoints[it->second].pt.x, (double)kF->keys.rightKeyPoints[it->second].pt.y);
            points.emplace_back(vec2d);
        }
    }
}

void LocalMapper::projectToPlane(Eigen::Vector4d& vec, cv::Point2f& p2f)
{
    const double invZ {1.0/vec(2)};
    const double u {fx*vec(0)*invZ + cx};
    const double v {fy*vec(1)*invZ + cy};
    p2f = cv::Point2f((float)u, (float)v);
}

bool LocalMapper::checkReprojErr(Eigen::Vector4d& calcVec, std::vector<std::pair<int, int>>& matchesOfPoint, const std::unordered_map<int, Eigen::Matrix<double,3,4>>& allProjMatrices)
{
    if ( calcVec(2) <= 0 )
        return false;
    // Eigen::Vector4d p4d(calcVec(0), calcVec(1), calcVec(2),1.0);

    for (size_t i {0}, end{matchesOfPoint.size()}; i < end; i++)
    {
        int kfNumb {matchesOfPoint[i].first};
        KeyFrame* kF = map->keyFrames.at(abs(kfNumb));
        Eigen::Vector3d p3dnew = allProjMatrices.at(kfNumb) * calcVec;
        p3dnew = p3dnew/p3dnew(2);
        // p4dnew = kF->pose.poseInverse * calcVec;
        // projectToPlane(p4dnew,p2f);
        cv::Point2f p2f((float)p3dnew(0), (float)p3dnew(1));
        float err1,err2;
        if (kfNumb >= 0)
        {
            err1 = kF->keys.keyPoints[matchesOfPoint[i].second].pt.x - p2f.x;
            err2 = kF->keys.keyPoints[matchesOfPoint[i].second].pt.y - p2f.y;

        }
        else
        {
            err1 = kF->keys.rightKeyPoints[matchesOfPoint[i].second].pt.x - p2f.x;
            err2 = kF->keys.rightKeyPoints[matchesOfPoint[i].second].pt.y - p2f.y;

        }
        float err = err1*err1 + err2*err2;
        // Logging("err", err,3);
        if ( err > reprjThreshold )
            return false;
    }
    return true;
}

void LocalMapper::addMultiViewMapPoints(const Eigen::Vector4d& posW, const std::vector<std::pair<int, int>>& matchesOfPoint, std::unordered_map<MapPoint*, Eigen::Vector3d>& allMapPoints, KeyFrame* lastKF, const size_t& keyPos)
{
    const TrackedKeys& temp = lastKF->keys; 
    MapPoint* mp = new MapPoint(posW, temp.Desc.row(keyPos),temp.keyPoints[keyPos], temp.close[keyPos], lastKF->numb, map->pIdx);
    int count {0};
    std::lock_guard<std::mutex> lock(map->mapMutex);
    for (size_t i {0}, end{matchesOfPoint.size()}; i < end; i++)
    {
        if ( matchesOfPoint[i].first >= 0)
        {
            KeyFrame* kf = map->keyFrames.at(matchesOfPoint[i].first);
            mp->kFWithFIdx.insert(std::pair<KeyFrame*, size_t>(kf, matchesOfPoint[i].second));
            count ++;
            kf->unMatchedF[matchesOfPoint[i].second] = false;
        }
    }
    mp->trackCnt = count;
    map->activeMapPoints.emplace_back(mp);
    map->addMapPoint(mp);
    lastKF->localMapPoints.emplace_back(mp);

}

void LocalMapper::calcp4d(KeyFrame* lastKF, std::vector<Eigen::Vector4d>& p4d)
{
    TrackedKeys& keys = lastKF->keys;
    Eigen::Matrix4d& curPose = lastKF->pose.pose;
    p4d.reserve(keys.keyPoints.size());
    for ( size_t i {0}, end{keys.keyPoints.size()}; i < end; i ++)
    {
        if ( keys.estimatedDepth[i] <= 0 )
        {
            p4d.emplace_back(Eigen::Vector4d());
            continue;
        }
        const double zp = (double)keys.estimatedDepth[i];
        const double xp = (double)(((double)keys.keyPoints[i].pt.x-cx)*zp/fx);
        const double yp = (double)(((double)keys.keyPoints[i].pt.y-cy)*zp/fy);
        Eigen::Vector4d p(xp, yp, zp, 1);
        p = curPose * p;
        p4d.emplace_back(p);
    }
}

void LocalMapper::triangulateCeres(Eigen::Vector3d& p3d, const std::vector<Eigen::Matrix<double, 3, 4>>& proj_matrices, const std::vector<Eigen::Vector2d>& obs, const Eigen::Matrix4d& lastKFPose)
{
    // Eigen::Matrix<double,3,3>& K = zedPtr->cameraLeft.intrisics;
    const Eigen::Matrix4d& camPose = lastKFPose;
    ceres::Problem problem;
    // ceres::Manifold* quaternion_local_parameterization = new ceres::EigenQuaternionManifold;
    // Logging("before", p3d,3);
    ceres::LossFunction* loss_function = new ceres::HuberLoss(sqrt(7.815f));
    // Logging("p3d", p3d,3);
    for (size_t i {0}, end{obs.size()}; i < end; i ++)
    {
        ceres::CostFunction* costf = MultiViewTriang::Create(camPose, proj_matrices[i], obs[i]);
        problem.AddResidualBlock(costf, loss_function /* squared loss */,p3d.data());

        // problem.SetManifold(frame_qcw.coeffs().data(),quaternion_local_parameterization);
    }

    ceres::Solver::Options options;
    options.linear_solver_type = ceres::SPARSE_NORMAL_CHOLESKY;
    // options.use_explicit_schur_complement = true;
    options.max_num_iterations = 20;
    // options.minimizer_progress_to_stdout = false;
    problem.SetParameterLowerBound(p3d.data(), 2, 0.1);
    ceres::Solver::Summary summary;
    ceres::Solve(options, &problem, &summary);

    Eigen::Vector4d p4d(p3d(0), p3d(1), p3d(2), 1.0);
    p4d = lastKFPose * p4d;
    // Logging("p4d", p4d, 3);
    // Logging("after", p3d,3);
    p3d(0) = p4d(0);
    p3d(1) = p4d(1);
    p3d(2) = p4d(2);
    // Logging("sum",summary.BriefReport(),3);
    // Logging("after", p3d,3);

}

void LocalMapper::computeAllMapPoints()
{
    std::unordered_map<MapPoint*, Eigen::Vector3d> allMapPoints;
    std::vector<vio_slam::KeyFrame *> actKeyF = map->activeKeyFrames;
    std::vector<KeyFrame*>::const_iterator it, end(actKeyF.end());
    for ( it = actKeyF.begin(); it != end; it++)
    {
        std::vector<MapPoint*>::const_iterator itmp, endmp((*it)->localMapPoints.end());
        for ( itmp = (*it)->localMapPoints.begin(); itmp != endmp; itmp++)
        {
            if ( (*itmp)->GetIsOutlier() )
                continue;

            allMapPoints.insert(std::pair<MapPoint*, Eigen::Vector3d>((*itmp), (*itmp)->getWordPose3d()));
        }
    }
    
    // Logging("size", allMapPoints.size(),3);
    KeyFrame* lastKF = actKeyF.front();

    const int lastKFIdx = lastKF->numb;

    std::vector<std::vector<std::pair<int, int>>> matchedIdxs(lastKF->keys.keyPoints.size(),std::vector<std::pair<int, int>>());

    std::vector<Eigen::Vector4d> p4d;
    calcp4d(lastKF, p4d);
    const int aKFsize {actKeyF.size()};
    bool first = true;
    for ( it = actKeyF.begin(); it != end; it++)
    {
        if ( (*it)->numb == lastKFIdx)
            continue;
        // if ( (*it)->numb < lastKFIdx - 5)
        //     continue;
        std::vector<float> keysAngles;
        std::vector<cv::Point2f> predPoints;
        predictKeysPos(lastKF->keys, lastKF->pose.pose, (*it)->pose.poseInverse, keysAngles, p4d, predPoints);
        // drawPred(lastKF, lastKF->keys.keyPoints, predPoints);
        fm->matchLocalBA(matchedIdxs, lastKF, (*it), aKFsize, 4, first, keysAngles, predPoints);
        // if (first)
        drawLBA("LBA matches",matchedIdxs, lastKF,(*it));
        first = false;
        
        // drawPred(lastKF, lastKF->keys.keyPoints, lastKF->keys.predKeyPoints);
    }



    // match only last keyframe.

    // to act as an idx finder
    // you find last keyframe leftIdx of matched feature and go to that place in the vector and add the Kidx and leftIdx
    // std::vector<kIdx, leftIdx>(currentKeys.size());

    // after matching calculate 3d mappoints and then add all the mappoints to the allmappoints while appending to std::unordered_map<KeyFrame*, size_t> kFWithFIdx to be able to get them for the optimization
    // the mappoints that are calculated and are not outliers after optimization should update the unmachedFeatures in the keyframes class


    // if estimated depth > 0 then use projection matrix from right Idx.
    // if currframe = 1 then use only those that have estimated depth ( 3 projection matrices at least )


    // here match all different features of each keyframe create a map of which keyframe to which are matched so the proj matrices can be used to calculate the 3d point



    // the 3d point calc is given to the first keyframe

    // std::vector<Eigen::Matrix<double,3,4>> projMatrices;
    std::unordered_map<int, Eigen::Matrix<double,3,4>> allProjMatrices;
    allProjMatrices.reserve(aKFsize);

    calcProjMatrices(allProjMatrices, actKeyF);
    int newMapPointsCount {0};
    lastKF->localMapPoints.reserve(lastKF->keys.keyPoints.size());
    // Logging("activeSizeLOCAL", map->activeMapPoints.size(),3);
    for ( size_t i{0}, end {lastKF->keys.keyPoints.size()}; i < end; i ++)
    {
        std::vector<std::pair<int, int>>& matchesOfPoint = matchedIdxs[i];
        if (matchesOfPoint.size() < 3)
            continue;
        std::vector<Eigen::Matrix<double, 3, 4>> proj_mat;
        std::vector<Eigen::Vector2d> pointsVec;
        processMatches(matchesOfPoint, allProjMatrices, proj_mat, pointsVec, actKeyF);
        double zp;
        if (lastKF->keys.estimatedDepth[i] > 0)
            zp = (double)lastKF->keys.estimatedDepth[i];
        else
            zp = 20.0;
        const double xp = (double)(((double)lastKF->keys.keyPoints[i].pt.x-cx)*zp/fx);
        const double yp = (double)(((double)lastKF->keys.keyPoints[i].pt.y-cy)*zp/fy);
        Eigen::Vector4d vecCalc(xp, yp, zp, 1);
        // vecCalc = lastKF->pose.pose * vecCalc;
        
        Eigen::Vector3d vec3d(vecCalc(0), vecCalc(1), vecCalc(2));
        triangulateCeres(vec3d, proj_mat, pointsVec, lastKF->pose.pose);
        vecCalc(0) = vec3d(0);
        vecCalc(1) = vec3d(1);
        vecCalc(2) = vec3d(2);

        // Eigen::Vector4d vecCalcCheckD = lastKF->pose.poseInverse * vecCalc;
        // if ( vecCalcCheckD(2) <= 0)
        //     continue;
        // Eigen::Vector3d vec3(vecCalc(0), vecCalc(1), vecCalc(2));
        // Eigen::Vector3d vec3d = TriangulateMultiViewPoint(proj_mat, pointsVec);
        // Eigen::Vector4d temp(vec3d(0),vec3d(1),vec3d(2),1);
        if ( !checkReprojErr(vecCalc, matchesOfPoint, allProjMatrices) )
            continue;
        // else
        //     Logging("Success!", "", 3);

        addMultiViewMapPoints(vecCalc, matchesOfPoint, allMapPoints, lastKF, i);
        
        newMapPointsCount ++;
        // Logging("calc 3d", vecCalcbef, 3);
        // Logging("calc 3d", vecCalc, 3);


        // Logging("est depth", lastKF->keys.estimatedDepth[i],3);
        // if ( lastKF->keys.estimatedDepth[i]> 0)
        // {
        //     const double zp = (double)lastKF->keys.estimatedDepth[i];
        //     const double xp = (double)(((double)lastKF->keys.keyPoints[i].pt.x-cx)*zp/fx);
        //     const double yp = (double)(((double)lastKF->keys.keyPoints[i].pt.y-cy)*zp/fy);
        //     Eigen::Vector4d vecCalc(xp, yp, zp, 1);
        //     vecCalc = lastKF->pose.pose * vecCalc;
        //     Eigen::Vector3d vec3(vecCalc(0), vecCalc(1), vecCalc(2));
        //     Logging("vecCalc", vec3, 3);
        // }
        // Logging("KF pose",lastKF->pose.pose,3);
    }
    Logging("Success!", newMapPointsCount, 3);
    




    // for each active keyframe
    // if feature not matched and estim depth > 0
    // add mappoint

    // afterwards fuse mappoints somehow

    // feature match features with no estim depth and then triangulate them. features with estim depth dont need triangulation because their position will be optimized by ceres.

    // then feature match all features (even those with no estim depth) between all keyframes and triangulate them into mappoints using the provided poses.
    // remove mappoints that their triangulations have big differences
    // predict each mappoint position in each keyframe ( if no estim depth then no prediction )
    // have a link of mappoints and features for each keyframe (the unmatchedF)

    // run through all mappoints one by one and get each keyframe observation (if any) and pass them to ceres
    // have fixed keyframes ( taken from the connections of each keyframe ) if they are not active.
    // check each KF connection to see if there are connections with inactive keyframes (check if connections[idx] = kIdx -> if kIdx->active = false (these are fixed keyframes))
    
}

void LocalMapper::beginLocalMapping()
{
    using namespace std::literals::chrono_literals;
    while ( !map->endOfFrames )
    {
        if ( map->keyFrameAdded )
        {
            Timer matchingInLBA("matching LBA");
            map->keyFrameAdded = false;
            computeAllMapPoints();
        }
        std::this_thread::sleep_for(20ms);
    }
}



} // namespace vio_slam