#include "FeatureTracker.h"

namespace vio_slam
{

void ImageData::setImage(const int frameNumber, const char* whichImage, const std::string& seq)
{
    std::string imagePath;
    std::string first;
    std::string second, format;
    std::string t = whichImage;
#if KITTI_DATASET
    first = "/home/christos/catkin_ws/src/mini_project_kokas/src/vio_slam/images/kitti/" + seq + "/";
    second = "/00";
    format = ".png";
#elif ZED_DATASET
    first = "/home/christos/catkin_ws/src/mini_project_kokas/src/vio_slam/images/zed_exp/";
    second = "/" + t + "00";
    format = ".png";
#elif ZED_DEMO
    first = "/home/christos/catkin_ws/src/mini_project_kokas/src/vio_slam/images/demo_1/";
    second = "/frame";
    format = ".jpg";
#elif V1_02
    first = "/home/christos/catkin_ws/src/mini_project_kokas/src/vio_slam/images/V1_02/";
    second = "/frame";
    format = ".jpg";
#elif SIMULATION
    first = "/home/christos/catkin_ws/src/mini_project_kokas/src/vio_slam/images/simulation/";
    second = "/frame";
    format = ".jpg";
    t = t;
    
#else
    first = "/home/christos/catkin_ws/src/mini_project_kokas/src/vio_slam/images/";
    second = "/frame";
    format = ".jpg";
#endif

    if (frameNumber > 999)
    {
        imagePath = first + t + second + std::to_string(frameNumber/(int)(pow(10,3))%10) + std::to_string(frameNumber/(int)(pow(10,2))%10) + std::to_string(frameNumber/(int)(pow(10,1))%10) + std::to_string(frameNumber%10) + format;
        int i{};
    }
    else if (frameNumber > 99)
    {
        imagePath = first + t + second + "0" + std::to_string(frameNumber/(int)(pow(10,2))%10) + std::to_string(frameNumber/(int)(pow(10,1))%10) + std::to_string(frameNumber%10) + format;
    }
    else if (frameNumber > 9)
    {
        imagePath = first + t + second + "00" + std::to_string(frameNumber/(int)(pow(10,1))%10) + std::to_string(frameNumber%10) + format;
    }
    else
    {
        imagePath = first + t + second + "000" + std::to_string(frameNumber) + format;
    }
    im = cv::imread(imagePath,cv::IMREAD_GRAYSCALE);
    rIm = cv::imread(imagePath,cv::IMREAD_COLOR);
}

void ImageData::rectifyImage(cv::Mat& image, const cv::Mat& map1, const cv::Mat& map2)
{
    cv::remap(image, image, map1, map2, cv::INTER_LINEAR);
}

FeatureData::FeatureData(Zed_Camera* _zedPtr) : zedPtr(_zedPtr), fx(_zedPtr->cameraLeft.fx), fy(_zedPtr->cameraLeft.fy), cx(_zedPtr->cameraLeft.cx), cy(_zedPtr->cameraLeft.cy)
{

}

void FeatureData::compute3DPoints(SubPixelPoints& prePnts, const int keyNumb)
{
    const size_t end{prePnts.left.size()};

    const size_t start{prePnts.points3D.size()};

    prePnts.points3D.reserve(end);
    for (size_t i = start; i < end; i++)
    {   

        const double zp = (double)prePnts.depth[i];
        const double xp = (double)(((double)prePnts.left[i].x-cx)*zp/fx);
        const double yp = (double)(((double)prePnts.left[i].y-cy)*zp/fy);
        Eigen::Vector4d p4d(xp,yp,zp,1);
        p4d = zedPtr->cameraPose.pose * p4d;
        prePnts.points3D.emplace_back(p4d(0),p4d(1),p4d(2));
        
    }
}

FeatureTracker::FeatureTracker(cv::Mat _rmap[2][2], Zed_Camera* _zedPtr, Map* _map) : zedPtr(_zedPtr), map(_map), fm(zedPtr, &feLeft, &feRight, zedPtr->mHeight,feLeft.getGridRows(), feLeft.getGridCols()), fmB(zedPtr, &feLeft, &feRight, zedPtr->mHeight,feLeft.getGridRows(), feLeft.getGridCols()), pE(zedPtr), fd(zedPtr), dt(1.0f/(double)zedPtr->mFps), lkal(dt), datafile(filepath), fx(_zedPtr->cameraLeft.fx), fy(_zedPtr->cameraLeft.fy), cx(_zedPtr->cameraLeft.cx), cy(_zedPtr->cameraLeft.cy), fxr(_zedPtr->cameraRight.fx), fyr(_zedPtr->cameraRight.fy), cxr(_zedPtr->cameraRight.cx), cyr(_zedPtr->cameraRight.cy), activeMapPoints(_map->activeMapPoints), activeMapPointsB(_map->activeMapPointsB), activeKeyFrames(_map->activeKeyFrames)
{
    // std::vector<MapPoint*>& temp = map->activeMapPoints;
    // activeMapPoints = map->activeMapPoints;
    rmap[0][0] = _rmap[0][0];
    rmap[0][1] = _rmap[0][1];
    rmap[1][0] = _rmap[1][0];
    rmap[1][1] = _rmap[1][1];
    K(0,0) = fx;
    K(1,1) = fy;
    K(0,2) = cx;
    K(1,2) = cy;
}

FeatureTracker::FeatureTracker(Zed_Camera* _zedPtr, Map* _map) : zedPtr(_zedPtr), map(_map), fm(zedPtr, &feLeft, &feRight, zedPtr->mHeight,feLeft.getGridRows(), feLeft.getGridCols()), fmB(zedPtr, &feLeft, &feRight, zedPtr->mHeight,feLeft.getGridRows(), feLeft.getGridCols()), pE(zedPtr), fd(zedPtr), dt(1.0f/(double)zedPtr->mFps), lkal(dt), datafile(filepath), fx(_zedPtr->cameraLeft.fx), fy(_zedPtr->cameraLeft.fy), cx(_zedPtr->cameraLeft.cx), cy(_zedPtr->cameraLeft.cy), fxr(_zedPtr->cameraRight.fx), fyr(_zedPtr->cameraRight.fy), cxr(_zedPtr->cameraRight.cx), cyr(_zedPtr->cameraRight.cy), activeMapPoints(_map->activeMapPoints), activeMapPointsB(_map->activeMapPointsB), activeKeyFrames(_map->activeKeyFrames)
{
    allFrames.reserve(zedPtr->numOfFrames);
    K(0,0) = fx;
    K(1,1) = fy;
    K(0,2) = cx;
    K(1,2) = cy;
}

FeatureTracker::FeatureTracker(Zed_Camera* _zedPtr, Zed_Camera* _zedPtrB, Map* _map) : zedPtr(_zedPtr), map(_map), fm(zedPtr, &feLeft, &feRight, zedPtr->mHeight,feLeft.getGridRows(), feLeft.getGridCols()), fmB(zedPtr, &feLeftB, &feRightB, zedPtr->mHeight,feLeftB.getGridRows(), feLeftB.getGridCols()), pE(zedPtr), fd(zedPtr), dt(1.0f/(double)zedPtr->mFps), lkal(dt), datafile(filepath), fx(_zedPtr->cameraLeft.fx), fy(_zedPtr->cameraLeft.fy), cx(_zedPtr->cameraLeft.cx), cy(_zedPtr->cameraLeft.cy), fxr(_zedPtr->cameraRight.fx), fyr(_zedPtr->cameraRight.fy), cxr(_zedPtr->cameraRight.cx), cyr(_zedPtr->cameraRight.cy), activeMapPoints(_map->activeMapPoints), activeMapPointsB(_map->activeMapPointsB), activeKeyFrames(_map->activeKeyFrames)
{
    zedPtrB = _zedPtrB;
    allFrames.reserve(zedPtr->numOfFrames);
    K(0,0) = fx;
    K(1,1) = fy;
    K(0,2) = cx;
    K(1,2) = cy;
}

void FeatureTracker::setMask(const SubPixelPoints& prePnts, cv::Mat& mask)
{
    // const int rad {3};
    mask = cv::Mat(zedPtr->mHeight, zedPtr->mWidth, CV_8UC1, cv::Scalar(255));

    std::vector<cv::Point2f>::const_iterator it, end{prePnts.left.end()};
    for (it = prePnts.left.begin();it != end; it++)
    {
        if (mask.at<uchar>(*it) == 255)
        {
            cv::circle(mask, *it, maskRadius, 0, cv::FILLED);
        }
    }

}

void FeatureTracker::setPopVec(const SubPixelPoints& prePnts, std::vector<int>& pop)
{
    const int gRows {fe.getGridRows()};
    const int gCols {fe.getGridCols()};
    pop.resize(gRows * gCols);
    const int wid {(int)zedPtr->mWidth/gCols + 1};
    const int hig {(int)zedPtr->mHeight/gRows + 1};
    std::vector<cv::Point2f>::const_iterator it, end(prePnts.left.end());
    for (it = prePnts.left.begin(); it != end; it ++)
    {
        const int w {(int)it->x/wid};
        const int h {(int)it->y/hig};
        pop[(int)(w + h*gCols)] += 1;
    }
}

void FeatureTracker::stereoFeaturesPop(cv::Mat& leftIm, cv::Mat& rightIm, std::vector<cv::DMatch>& matches, SubPixelPoints& pnts, const SubPixelPoints& prePnts)
{
    StereoDescriptors desc;
    StereoKeypoints keys;
    std::vector<int> pop;
    setPopVec(prePnts, pop);
    fe.extractFeaturesPop(leftIm, rightIm, desc, keys, pop);
    fm.computeStereoMatches(leftIm, rightIm, desc, matches, pnts, keys);
    // std::vector<uchar> inliers;
    // if ( pnts.left.size() >  6)
    // {
    //     cv::findFundamentalMat(pnts.left, pnts.right, inliers, cv::FM_RANSAC, 3, 0.99);

    //     pnts.reduce<uchar>(inliers);
    //     reduceVectorTemp<cv::DMatch,uchar>(matches, inliers);
    // }
    Logging("matches size", matches.size(),1);

#if KEYSIM
    drawKeys("left", pLIm.rIm, keys.left);
    drawKeys("right", pRIm.rIm, keys.right);
#endif


#if MATCHESIM
    drawMatches(pLIm.rIm, pnts, matches);
#endif
}

void FeatureTracker::stereoFeaturesMask(cv::Mat& leftIm, cv::Mat& rightIm, std::vector<cv::DMatch>& matches, SubPixelPoints& pnts, const SubPixelPoints& prePnts)
{
    StereoDescriptors desc;
    StereoKeypoints keys;

    extractFAST(leftIm, rightIm, keys, desc, prePnts.left);

    fm.findStereoMatchesFAST(leftIm, rightIm, desc,pnts, keys);

#if KEYSIM
    drawKeys("left", pLIm.rIm, keys.left);
    drawKeys("right", pRIm.rIm, keys.right);
#endif

    Logging("matches size", pnts.left.size(),3);
#if MATCHESIM
    drawPointsTemp<cv::Point2f,cv::Point2f>("Matches",pLIm.rIm,pnts.left, pnts.right);
#endif
}

void FeatureTracker::stereoFeaturesClose(cv::Mat& leftIm, cv::Mat& rightIm, std::vector<cv::DMatch>& matches, SubPixelPoints& pnts)
{
    StereoDescriptors desc;
    StereoKeypoints keys;

    extractFAST(leftIm, rightIm, keys, desc, prePnts.left);

    // fm.findStereoMatchesClose(desc,pnts, keys);
    fm.findStereoMatchesFAST(leftIm, rightIm, desc,pnts, keys);

    // std::vector<uchar> inliers;
    // cv::Mat F = cv::findFundamentalMat(pnts.left, pnts.right, inliers, cv::FM_RANSAC, 3, 0.99);
    // cv::correctMatches(F,pnts.left, pnts.right,pnts.left, pnts.right);

    // pnts.reduce<uchar>(inliers);
    // reduceVectorTemp<cv::DMatch,uchar>(matches, inliers);
#if MATCHESIM
    drawPointsTemp<cv::Point2f,cv::Point2f>("Matches",pLIm.rIm,pnts.left, pnts.right);
#endif
}

void FeatureTracker::extractORB(cv::Mat& leftIm, cv::Mat& rightIm, StereoKeypoints& keys, StereoDescriptors& desc)
{
    Timer orb("ORB");
    std::thread extractLeft(&FeatureExtractor::computeKeypoints, std::ref(feLeft), std::ref(leftIm), std::ref(keys.left), std::ref(prePnts.left), std::ref(desc.left), 0);
    std::thread extractRight(&FeatureExtractor::computeKeypoints, std::ref(feRight), std::ref(rightIm), std::ref(keys.right), std::ref(prePnts.left),std::ref(desc.right), 1);
    extractLeft.join();
    extractRight.join();
}

void FeatureTracker::assignKeysToGrids(TrackedKeys& keysLeft, std::vector<cv::KeyPoint>& keypoints,std::vector<std::vector<std::vector<int>>>& keyGrid, const int width, const int height)
{
    const float imageRatio = (float)width/(float)height;
    keysLeft.xGrids = 64;
    keysLeft.yGrids = cvCeil((float)keysLeft.xGrids/imageRatio);
    keysLeft.xMult = (float)keysLeft.xGrids/(float)width;
    keysLeft.yMult = (float)keysLeft.yGrids/(float)height;
    keyGrid = std::vector<std::vector<std::vector<int>>>(keysLeft.yGrids, std::vector<std::vector<int>>(keysLeft.xGrids, std::vector<int>()));
    int kpCount {0};
    for ( std::vector<cv::KeyPoint>::const_iterator it = keypoints.begin(), end(keypoints.end()); it !=end; it ++, kpCount++)
    {
        const cv::KeyPoint& kp = *it;
        int xPos = cvRound(kp.pt.x * keysLeft.xMult);
        int yPos = cvRound(kp.pt.y * keysLeft.yMult);
        if ( xPos < 0 )
            xPos = 0;
        if ( yPos < 0 )
            yPos = 0;
        if ( xPos >= keysLeft.xGrids )
            xPos = keysLeft.xGrids - 1;
        if ( yPos >= keysLeft.yGrids )
            yPos = keysLeft.yGrids - 1;
        if ( keyGrid[yPos][xPos].empty() )
            keyGrid[yPos][xPos].reserve(200);
        keyGrid[yPos][xPos].emplace_back(kpCount);
    }
}

void FeatureTracker::extractORBStereoMatch(cv::Mat& leftIm, cv::Mat& rightIm, TrackedKeys& keysLeft)
{
    // Timer orb("ORB");
    std::vector<cv::KeyPoint> rightKeys, temp;
    cv::Mat rightDesc;
    std::thread extractLeft(&FeatureExtractor::extractKeysNew, std::ref(feLeft), std::ref(leftIm), std::ref(keysLeft.keyPoints), std::ref(keysLeft.Desc));
    std::thread extractRight(&FeatureExtractor::extractKeysNew, std::ref(feRight), std::ref(rightIm), std::ref(keysLeft.rightKeyPoints),std::ref(keysLeft.rightDesc));
    extractLeft.join();
    extractRight.join();



    fm.findStereoMatchesORB2(lIm.im, rIm.im, keysLeft.rightDesc, keysLeft.rightKeyPoints, keysLeft);

    keysLeft.mapPointIdx.resize(keysLeft.keyPoints.size(), -1);
    keysLeft.trackCnt.resize(keysLeft.keyPoints.size(), 0);
#if DRAWMATCHES
    drawKeys("left Keys", lIm.rIm, keysLeft.keyPoints);


    drawKeyPointsCloseFar("new method", lIm.rIm, keysLeft, keysLeft.rightKeyPoints);
#endif
}

void FeatureTracker::extractORBStereoMatchR(cv::Mat& leftIm, cv::Mat& rightIm, TrackedKeys& keysLeft)
{
    // Timer orb("ORB");
    std::thread extractLeft(&FeatureExtractor::extractKeysNew, std::ref(feLeft), std::ref(leftIm), std::ref(keysLeft.keyPoints), std::ref(keysLeft.Desc));
    std::thread extractRight(&FeatureExtractor::extractKeysNew, std::ref(feRight), std::ref(rightIm), std::ref(keysLeft.rightKeyPoints),std::ref(keysLeft.rightDesc));
    extractLeft.join();
    extractRight.join();

    // drawKeys("right Keys Before", rIm.rIm, keysLeft.rightKeyPoints);


    fm.findStereoMatchesORB2R(leftIm, rightIm, keysLeft.rightDesc, keysLeft.rightKeyPoints, keysLeft);

    assignKeysToGrids(keysLeft, keysLeft.keyPoints, keysLeft.lkeyGrid, zedPtr->mWidth, zedPtr->mHeight);
    assignKeysToGrids(keysLeft, keysLeft.rightKeyPoints, keysLeft.rkeyGrid, zedPtr->mWidth, zedPtr->mHeight);

    // keysLeft.mapPointIdx.resize(keysLeft.keyPoints.size(), -1);
    // keysLeft.trackCnt.resize(keysLeft.keyPoints.size(), 0);
    // drawKeys("left Keys", lIm.rIm, keysLeft.keyPoints);
    // drawKeyPointsCloseFar("new method", lIm.rIm, keysLeft, keysLeft.rightKeyPoints);
    // drawKeys("right Keys AFTER", rIm.rIm, keysLeft.rightKeyPoints);
#if DRAWMATCHES


#endif
}

void FeatureTracker::extractORBStereoMatchRB(const Zed_Camera* zedCam, cv::Mat& leftIm, cv::Mat& rightIm, FeatureExtractor& feLeft, FeatureExtractor& feRight, FeatureMatcher& fm, TrackedKeys& keysLeft)
{
    // Timer orb("ORB");
    std::thread extractLeft(&FeatureExtractor::extractKeysNew, std::ref(feLeft), std::ref(leftIm), std::ref(keysLeft.keyPoints), std::ref(keysLeft.Desc));
    std::thread extractRight(&FeatureExtractor::extractKeysNew, std::ref(feRight), std::ref(rightIm), std::ref(keysLeft.rightKeyPoints),std::ref(keysLeft.rightDesc));
    extractLeft.join();
    extractRight.join();

    // drawKeys("right Keys Before", rIm.rIm, keysLeft.rightKeyPoints);


    fm.findStereoMatchesORB2R(leftIm, rightIm, keysLeft.rightDesc, keysLeft.rightKeyPoints, keysLeft);

    assignKeysToGrids(keysLeft, keysLeft.keyPoints, keysLeft.lkeyGrid, zedCam->mWidth, zedCam->mHeight);
    assignKeysToGrids(keysLeft, keysLeft.rightKeyPoints, keysLeft.rkeyGrid, zedCam->mWidth, zedCam->mHeight);

    // keysLeft.mapPointIdx.resize(keysLeft.keyPoints.size(), -1);
    // keysLeft.trackCnt.resize(keysLeft.keyPoints.size(), 0);
    // drawKeys("left Keys", lIm.rIm, keysLeft.keyPoints);
    // drawKeyPointsCloseFar("new method", lIm.rIm, keysLeft, keysLeft.rightKeyPoints);
    // drawKeys("right Keys AFTER", rIm.rIm, keysLeft.rightKeyPoints);
#if DRAWMATCHES


#endif
}

void FeatureTracker::extractFAST(const cv::Mat& leftIm, const cv::Mat& rightIm, StereoKeypoints& keys, StereoDescriptors& desc, const std::vector<cv::Point2f>& prevPnts)
{
    Timer fast("FAST");
    std::thread extractLeft(&FeatureExtractor::computeFASTandDesc, std::ref(feLeft), std::ref(leftIm), std::ref(keys.left), std::ref(prevPnts), std::ref(desc.left));
    std::thread extractRight(&FeatureExtractor::computeFASTandDesc, std::ref(feRight), std::ref(rightIm), std::ref(keys.right), std::ref(prevPnts),std::ref(desc.right));
    // std::thread extractLeft(&FeatureExtractor::extractFeaturesMask, std::ref(feLeft), std::ref(leftIm), std::ref(keys.left), std::ref(desc.left));
    // std::thread extractRight(&FeatureExtractor::extractFeaturesMask, std::ref(feRight), std::ref(rightIm), std::ref(keys.right),std::ref(desc.right));
    extractLeft.join();
    extractRight.join();
}

void FeatureTracker::stereoFeatures(cv::Mat& leftIm, cv::Mat& rightIm, std::vector<cv::DMatch>& matches, SubPixelPoints& pnts)
{
    StereoDescriptors desc;
    StereoKeypoints keys;
    extractFAST(leftIm, rightIm, keys, desc, prePnts.left);

    // fm.findStereoMatches(desc,pnts, keys);
    fm.findStereoMatchesFAST(leftIm, rightIm, desc,pnts, keys);

    // reduceStereoKeys<bool>(stereoKeys, inliersL, inliersR);
    // fm.computeStereoMatches(leftIm, rightIm, desc, matches, pnts, keys);
    // std::vector<uchar> inliers;
    // cv::findFundamentalMat(pnts.left, pnts.right, inliers, cv::FM_RANSAC, 3, 0.99);

    // pnts.reduce<uchar>(inliers);
    // reduceVectorTemp<cv::DMatch,uchar>(matches, inliers);
    Logging("matches size", matches.size(),1);
#if MATCHESIM
    drawPointsTemp<cv::Point2f,cv::Point2f>("Matches",lIm.rIm,pnts.left, pnts.right);
#endif
}

void FeatureTracker::stereoFeaturesGoodFeatures(cv::Mat& leftIm, cv::Mat& rightIm, SubPixelPoints& pnts, const SubPixelPoints& prePnts)
{
    const size_t gfcurCount {prePnts.left.size()};
    if ( curFrame != 0)
    {
        cv::Mat mask;
        setMask(prePnts, mask);
        cv::goodFeaturesToTrack(leftIm,pnts.left,gfmxCount - gfcurCount, 0.01, gfmnDist, mask);
    }
    else
        cv::goodFeaturesToTrack(leftIm,pnts.left,gfmxCount - gfcurCount, 0.01, gfmnDist);


    std::vector<uchar> status;
    std::vector<float> err;
    cv::calcOpticalFlowPyrLK(leftIm, rightIm, pnts.left, pnts.right, status, err, cv::Size(21,21), 1,criteria);

    reduceVectorTemp<cv::Point2f,uchar>(pnts.left,status);
    reduceVectorTemp<cv::Point2f,uchar>(pnts.right,status);

    std::vector<uchar>inliers;
    cv::findFundamentalMat(pnts.left, pnts.right, inliers, cv::FM_RANSAC, 3, 0.99);

    pnts.reduce<uchar>(inliers);

    fm.slWinGF(leftIm, rightIm,pnts);

    Logging("matches size", pnts.left.size(),1);
#if MATCHESIM
    drawMatchesGoodFeatures(lIm.rIm, pnts);
#endif
}

void FeatureTracker::initializeTracking()
{
    // gridTraX.resize(gridVelNumb * gridVelNumb);
    // gridTraY.resize(gridVelNumb * gridVelNumb);
    startTime = std::chrono::high_resolution_clock::now();
    setLRImages(0);
    std::vector<cv::DMatch> matches;
    stereoFeatures(lIm.im, rIm.im, matches,pnts);
    cv::Mat rot = (cv::Mat_<double>(3,3) << 1.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,1.0);
    uStereo = pnts.left.size();
    uMono = uStereo;
    pE.setPrevR(rot);
    cv::Mat tr = (cv::Mat_<double>(3,1) << 0.0,0.0,0.0);
    pE.setPrevT(tr);
    setPreInit();
    fd.compute3DPoints(prePnts, keyNumb);
    uStereo = prePnts.points3D.size();
    keyframes.emplace_back(zedPtr->cameraPose.pose,prePnts.points3D,keyNumb);
    keyNumb++;
#if SAVEODOMETRYDATA
    saveData();
#endif
    // addFeatures = checkFeaturesArea(prePnts);
}

void FeatureTracker::initializeTrackingGoodFeatures()
{
    // gridTraX.resize(gridVelNumb * gridVelNumb);
    // gridTraY.resize(gridVelNumb * gridVelNumb);
    startTime = std::chrono::high_resolution_clock::now();
    setLRImages(0);
    std::vector<cv::DMatch> matches;
    stereoFeaturesGoodFeatures(lIm.im, rIm.im,pnts, prePnts);
    setPreInit();
    fd.compute3DPoints(prePnts, keyNumb);
    uStereo = prePnts.points3D.size();
    keyframes.emplace_back(zedPtr->cameraPose.pose,prePnts.points3D,keyNumb);
    keyNumb++;
#if SAVEODOMETRYDATA
    saveData();
#endif
    // addFeatures = checkFeaturesArea(prePnts);
}

void FeatureTracker::beginTracking(const int frames)
{
    for (int32_t frame {1}; frame < frames; frame++)
    {
        curFrame = frame;
        setLRImages(frame);
        if (addFeatures || uStereo < mnSize)
        {
            zedPtr->addKeyFrame = true;
            updateKeys(frame);
            fd.compute3DPoints(prePnts, keyNumb);
            keyframes.emplace_back(zedPtr->cameraPose.pose,prePnts.points3D,keyNumb);
            keyNumb ++;
            
        }
        opticalFlow();

        // Logging("addf", addFeatures,3);
        Logging("ustereo", uStereo,3);

        // getSolvePnPPoseWithEss();

        // getPoseCeres();
        getPoseCeresNew();

        setPre();

        addFeatures = checkFeaturesAreaCont(prePnts);
    }
    datafile.close();
}

void FeatureTracker::beginTrackingTrial(const int frames)
{
    for (int32_t frame {1}; frame < frames; frame++)
    {
        curFrame = frame;
        setLRImages(frame);
        fm.checkDepthChange(pLIm.im,pRIm.im,prePnts);
        if ( addFeatures || uStereo < mnSize )
        {
            zedPtr->addKeyFrame = true;
            updateKeys(frame);
            fd.compute3DPoints(prePnts, keyNumb);
            keyframes.emplace_back(zedPtr->cameraPose.pose,prePnts.points3D,keyNumb);
            keyNumb ++;
            
        }
        
        // opticalFlow();
        opticalFlowPredict();

        // Logging("addf", addFeatures,3);

        // getSolvePnPPoseWithEss();

        // getPoseCeres();
        getPoseCeresNew();

        setPreTrial();

        addFeatures = checkFeaturesArea(prePnts);
        // addFeatures = checkFeaturesAreaCont(prePnts);
        Logging("ustereo", uStereo,3);
        Logging("umono", uMono,3);
    }
    datafile.close();
}

void FeatureTracker::beginTrackingTrialClose(const int frames)
{
    for (int32_t frame {1}; frame < frames; frame++)
    {
        curFrame = frame;
        setLRImages(frame);
        // fm.checkDepthChange(pLIm.im,pRIm.im,prePnts);
        if ( (addFeatures || uStereo < mnSize || cv::norm(pTvec)*zedPtr->mFps > highSpeed) && ( uStereo < mxSize) )
        {
            // Logging("ptvec",pTvec,3);
            // Logging("cv::norm(pTvec)",cv::norm(pTvec),3);

            zedPtr->addKeyFrame = true;
            if ( uMono > mxMonoSize )
                updateKeysClose(frame);
            else
                updateKeys(frame);
            fd.compute3DPoints(prePnts, keyNumb);
            keyframes.emplace_back(zedPtr->cameraPose.pose,prePnts.points3D,keyNumb);
            keyNumb ++;
            
        }
        // std::vector<cv::Point3d> p3D;
        // pointsInFrame(p3D);
        

        // std::vector<cv::Point2f> pPnts, curPnts;

        // optFlow(p3D, pPnts, curPnts);

        // estimatePose(p3D,curPnts);

        if ( curFrame == 1 )
            opticalFlow();
        else
            opticalFlowPredict();
        getPoseCeresNew();


        // Logging("addf", addFeatures,3);

        // getSolvePnPPoseWithEss();

        // getPoseCeres();

        setPreTrial();

        addFeatures = checkFeaturesArea(prePnts);
        // addFeatures = checkFeaturesAreaCont(prePnts);
        Logging("ustereo", uStereo,3);
        Logging("umono", uMono,3);
    }
    datafile.close();
}

void FeatureTracker::findStereoFeatures(cv::Mat& leftIm, cv::Mat& rightIm, SubPixelPoints& pnts)
{
    StereoDescriptors desc;
    StereoKeypoints keys;
    std::vector<cv::Point2f> temp;

    // extractORB(leftIm, rightIm, keys, desc);
    // fm.findStereoMatches(desc,pnts, keys);


    extractFAST(leftIm, rightIm, keys, desc, temp);
    fm.findStereoMatchesFAST(leftIm, rightIm, desc,pnts, keys);

    // float min {leftIm.cols};
    // float max {0};
    // std::vector<bool>inBox(pnts.left.size(),true);
    // std::vector<cv::Point2f>::const_iterator it, end(pnts.left.end());
    // for (it = pnts.left.begin(); it != end; it++)
    // {
    //     const float& px = it->x;
    //     if ( px > max )
    //         max = px;
    //     else if (px < min)
    //         min = px;
    // }

    // float difMax {leftIm.cols - max};
    // float maxDist {0};
    // if ( min > difMax )
    //     maxDist = min;
    // else
    //     maxDist = difMax;

    // float maxDistR = leftIm.cols - maxDist;

    // size_t boxC {0};

    // for (it = pnts.left.begin(); it != end; it++, boxC ++)
    // {
    //     const float& px = it->x;
    //     if ( px < maxDist || px > maxDistR)
    //         inBox[boxC] = false;
    // }
    // pnts.reduce<bool>(inBox);

    Logging("matches size", pnts.left.size(),3);
#if MATCHESIM
    drawPointsTemp<cv::Point2f,cv::Point2f>("Matches",lIm.rIm,pnts.left, pnts.right);
#endif
}

void FeatureTracker::triangulate3DPoints(SubPixelPoints& pnts)
{
    const size_t end{pnts.left.size()};

    pnts.points3D.reserve(end);
    for (size_t i = 0; i < end; i++)
    {

        const double zp = (double)pnts.depth[i];
        const double xp = (double)(((double)pnts.left[i].x-cx)*zp/fx);
        const double yp = (double)(((double)pnts.left[i].y-cy)*zp/fy);
        pnts.points3D.emplace_back(xp, yp, zp);
        // Eigen::Vector4d p4d(xp,yp,zp,1);
        // p4d = zedPtr->cameraPose.pose * p4d;
        // pnts.points3D.emplace_back(p4d(0),p4d(1),p4d(2));

    }
}

void FeatureTracker::setPre3DPnts(SubPixelPoints& prePnts, SubPixelPoints& pnts)
{
    const size_t end {pnts.points3D.size()};
    const size_t res { end + prePnts.points3D.size()};

    cv::Mat mask;
    setMask(prePnts, mask);

    prePnts.points3D.reserve(res);
    prePnts.left.reserve(res);

    for ( size_t iP = 0; iP < end; iP++ )
    {
        if (mask.at<uchar>(pnts.left[iP]) == 0)
            continue;
        const double x = pnts.points3D[iP].x;
        const double y = pnts.points3D[iP].y;
        const double z = pnts.points3D[iP].z;
        Eigen::Vector4d p4d(x,y,z,1);
        p4d = zedPtr->cameraPose.pose * p4d;
        prePnts.points3D.emplace_back(p4d(0), p4d(1), p4d(2));
        prePnts.left.emplace_back(pnts.left[iP]);
    }
}

void FeatureTracker::setPreviousValuesIni()
{
    setPreLImage();
    setPreRImage();
    setPre3DPnts(prePnts, pnts);
    pnts.clear();
}

void FeatureTracker::setPreviousValues()
{
    setPreLImage();
    setPreRImage();
    prePnts.left = prePnts.newPnts;
    // fm.checkDepthChange(pLIm.im, pRIm.im,prePnts);
    setPre3DPnts(prePnts, pnts);
    pnts.clear();
    prePnts.newPnts.clear();
}

bool FeatureTracker::inBorder(cv::Point3d& p3d, cv::Point2d& p2d)
{
    Eigen::Vector4d point(p3d.x, p3d.y, p3d.z,1);
    point = zedPtr->cameraPose.poseInverse * point;
    const double pointX = point(0);
    const double pointY = point(1);
    const double pointZ = point(2);

    if (pointZ <= 0.0f)
        return false;
    const double invZ = 1.0f/pointZ;

    const double invfx = 1.0f/fx;
    const double invfy = 1.0f/fy;


    double u {fx*pointX*invZ + cx};
    double v {fy*pointY*invZ + cy};


    const int min {0};
    const int maxW {zedPtr->mWidth};
    const int maxH {zedPtr->mHeight};

    if ( u < min || u > maxW || v < min || v > maxH)
        return false;

    p3d = cv::Point3d(pointX, pointY, pointZ);

    p2d = cv::Point2d(u,v);

    return true;
}

void FeatureTracker::checkInBorder(SubPixelPoints& pnts)
{
    const size_t end {pnts.points3D.size()};
    std::vector<bool> in;
    in.resize(end,false);
    pnts.points3DCurr = pnts.points3D;
    for (size_t i{0}; i < end; i++)
    {
        cv::Point2d pd((double)pnts.left[i].x, (double)pnts.left[i].y);
        if ( inBorder(pnts.points3DCurr[i], pd) )
        {
            cv::Point2f pf((float)pd.x, (float)pd.y);
            // if ( pointsDist(pf,pnts.left[i]) <= 16.0 )
            // {
                // pnts.left[i] = pf;
                in[i] = true;
            // }
        }

    }
    pnts.reduce<bool>(in);
}

void FeatureTracker::calcOpticalFlow(SubPixelPoints& pnts)
{
    Timer optical("optical");
    std::vector<float> err, err1;
    std::vector <uchar>  inliers, inliers2;
    std::vector<cv::Point3d> p3D;
    cv::Mat fIm, sIm, rightIm;
    fIm = pLIm.im;
    sIm = lIm.im;
    rightIm = rIm.im;
    checkInBorder(pnts);



    if ( curFrame == 1 )
    {
        cv::calcOpticalFlowPyrLK(fIm, sIm, pnts.left, pnts.newPnts, inliers, err1,cv::Size(21,21),3, criteria);
    }
    else
    {
        predictNewPnts(pnts, false);
// #if OPTICALIM
//     if ( new3D )
//         drawOptical("pred new", pLIm.rIm,pnts.left, pnts.newPnts);
//     else
//         drawOptical("pred old", pLIm.rIm,pnts.left, pnts.newPnts);
// #endif
        cv::calcOpticalFlowPyrLK(fIm, sIm, pnts.left, pnts.newPnts, inliers, err,cv::Size(21,21),3, criteria, cv::OPTFLOW_USE_INITIAL_FLOW);

    }
    std::vector<cv::Point2f> temp = pnts.left;
    cv::calcOpticalFlowPyrLK(sIm, fIm, pnts.newPnts, temp, inliers2, err,cv::Size(21,21),3, criteria, cv::OPTFLOW_USE_INITIAL_FLOW);

    for (size_t i {0}; i < pnts.left.size(); i ++)
    {
        if ( inliers[i] && inliers2[i] && pointsDist(temp[i],pnts.left[i]) <= 0.25)
            inliers[i] = true;
        else
            inliers[i] = false;
    }

    pnts.reduce<uchar>(inliers);

    inliers.clear();
    inliers2.clear();

    cv::calcOpticalFlowPyrLK(lIm.im, rightIm, pnts.newPnts, pnts.right, inliers, err,cv::Size(21,21),3, criteria);

    temp = pnts.newPnts;
    cv::calcOpticalFlowPyrLK(rightIm, lIm.im, pnts.right, temp, inliers2, err,cv::Size(21,21),3, criteria);

    pnts.depth = std::vector<float>(pnts.newPnts.size(),-1.0f);
    pnts.points3DStereo = std::vector<cv::Point3d>(pnts.newPnts.size(),cv::Point3d(0,0,-1.0f));

    for (size_t i {0}; i < pnts.newPnts.size(); i ++)
    {
        if ( inliers[i] && inliers2[i] && pointsDist(temp[i],pnts.newPnts[i]) <= 0.25 && abs(pnts.newPnts[i].y - pnts.right[i].y) < 2.0f)
        {
            const double zp = fx * zedPtr->mBaseline/((double)pnts.newPnts[i].x - (double)pnts.right[i].x);
            if ( zp > 0.0f )
            {
                const double xp = (double)(((double)pnts.newPnts[i].x-cx)*zp/fx);
                const double yp = (double)(((double)pnts.newPnts[i].y-cy)*zp/fy);
                pnts.points3DStereo[i] = cv::Point3d(xp, yp, zp);
            }
        }
            // pnts.depth[i] = pnts.newPnts[i].x - pnts.right[i].x;
    }



    // inliers.clear();
    // cv::findFundamentalMat(pnts.left, pnts.newPnts,inliers, cv::FM_RANSAC, 2, 0.99);

    // pnts.reduce<uchar>(inliers);


}

void FeatureTracker::calcOptical(SubPixelPoints& pnts, const bool new3D)
{
    Timer optical("optical");
    std::vector<float> err, err1;
    std::vector <uchar>  inliers, inliers2;
    std::vector<cv::Point3d> p3D;
    cv::Mat fIm, sIm, rightIm;
    if ( new3D )
    {
        fIm = lIm.im;
        sIm = pLIm.im;
        rightIm = pRIm.im;
        pnts.points3DCurr = pnts.points3D;
    }
    else
    {
        fIm = pLIm.im;
        sIm = lIm.im;
        rightIm = rIm.im;
        checkInBorder(pnts);
    }



    if ( curFrame == 1 )
    {
        cv::calcOpticalFlowPyrLK(fIm, sIm, pnts.left, pnts.newPnts, inliers, err1,cv::Size(21,21),3, criteria);
    }
    else
    {
        predictNewPnts(pnts, new3D);
// #if OPTICALIM
//     if ( new3D )
//         drawOptical("pred new", pLIm.rIm,pnts.left, pnts.newPnts);
//     else
//         drawOptical("pred old", pLIm.rIm,pnts.left, pnts.newPnts);
// #endif
        cv::calcOpticalFlowPyrLK(fIm, sIm, pnts.left, pnts.newPnts, inliers, err,cv::Size(21,21),1, criteria, cv::OPTFLOW_USE_INITIAL_FLOW);

    }
    std::vector<cv::Point2f> temp = pnts.left;
    cv::calcOpticalFlowPyrLK(sIm, fIm, pnts.newPnts, temp, inliers2, err,cv::Size(21,21),1, criteria, cv::OPTFLOW_USE_INITIAL_FLOW);

    for (size_t i {0}; i < pnts.left.size(); i ++)
    {
        if ( inliers[i] && inliers2[i] && pointsDist(temp[i],pnts.left[i]) <= 0.25)
            inliers[i] = true;
        else
            inliers[i] = false;
    }

    pnts.reduce<uchar>(inliers);

    // inliers.clear();
    // cv::findFundamentalMat(pnts.left, pnts.newPnts,inliers, cv::FM_RANSAC, 2, 0.99);

    // pnts.reduce<uchar>(inliers);


}

bool FeatureTracker::predProj(const cv::Point3d& p3d, cv::Point2d& p2d, const bool new3D)
{
    // Logging("key",keyFrameNumb,3);
    Eigen::Vector4d point(p3d.x, p3d.y, p3d.z,1);
    // Logging("point",point,3);
    // point = zedPtr->cameraPose.poseInverse * point;
    if ( !new3D )
        point = predNPoseInv * point;
    else
        point = poseEstFrame * point;
    // Logging("point",point,3);
    // Logging("zedPtr",zedPtr->cameraPose.poseInverse,3);
    // Logging("getPose",keyframes[keyFrameNumb].getPose(),3);
    const double pointX = point(0);
    const double pointY = point(1);
    const double pointZ = point(2);

    if (pointZ <= 0.0f)
        return false;
    const double invZ = 1.0f/pointZ;

    const double invfx = 1.0f/fx;
    const double invfy = 1.0f/fy;


    double u {fx*pointX*invZ + cx};
    double v {fy*pointY*invZ + cy};

    const int off {10};
    const int min {-off};
    const int maxW {zedPtr->mWidth + off};
    const int maxH {zedPtr->mHeight + off};

    if ( u < min || u > maxW || v < min || v > maxH)
        return false;

    p2d = cv::Point2d(u,v);
    return true;
}

void FeatureTracker::predictNewPnts(SubPixelPoints& pnts, const bool new3D)
{
    const size_t end {pnts.points3D.size()};
    pnts.newPnts.resize(end);
    std::vector<bool> in;
    in.resize(end,true);
    for (size_t i{0}; i < end; i++)
    {
        cv::Point2d pd((double)pnts.left[i].x, (double)pnts.left[i].y);
        if ( predProj(pnts.points3D[i], pd, new3D) )
            pnts.newPnts[i] = cv::Point2f((float)pd.x, (float)pd.y);
        else
            in[i] = false;

    }
    pnts.reduce<bool>(in);
}

void FeatureTracker::solvePnPIni(SubPixelPoints& pnts, cv::Mat& Rvec, cv::Mat& tvec, const bool new3D)
{
    std::vector<int>idxs;


    cv::solvePnPRansac(pnts.points3DCurr, pnts.newPnts ,zedPtr->cameraLeft.cameraMatrix, cv::Mat::zeros(5,1,CV_64F),Rvec,tvec,true,100, 4.0f, 0.99, idxs);
    // cv::solvePnP(pnts.points3DCurr, pnts.newPnts ,zedPtr->cameraLeft.cameraMatrix, cv::Mat::zeros(5,1,CV_64F),Rvec,tvec,true);

    // pnts.reduceWithInliers<int>(idxs);

    if ( new3D )
    {
        cv::Mat Rot;
        cv::Rodrigues(Rvec, Rot);
        cv::transpose(Rot, Rot);
        cv::Rodrigues(Rot, Rvec);
        tvec = -tvec;
    }
}

void FeatureTracker::checkRotTra(cv::Mat& Rvec, cv::Mat& tvec,cv::Mat& RvecN, cv::Mat& tvecN)
{
    const double R1 = cv::norm(Rvec,pRvec);
    const double R2 = cv::norm(RvecN,pRvec);
    const double T1 = cv::norm(tvec,pTvec);
    const double T2 = cv::norm(tvecN,pTvec);

    if ( (T1 > 1.0f && T2 > 1.0f) || (R1 > 0.5f && R2 > 0.5f))
    {
        tvec = pTvec.clone();
        Rvec = pRvec.clone();
    }
    else if ( T1 > 1.0f && T2 < 1.0f  && R2 < 0.5f )
    {
        tvec = tvecN.clone();
        Rvec = RvecN.clone();
    }
    else if (T2 < 1.0f && T1 < 1.0f  && R1 < 0.5f && R2 < 0.5f)
    {
        tvec.at<double>(0) =  (tvec.at<double>(0) +  tvecN.at<double>(0)) / 2.0f;
        tvec.at<double>(1) =  (tvec.at<double>(1) +  tvecN.at<double>(1)) / 2.0f;
        tvec.at<double>(2) =  (tvec.at<double>(2) +  tvecN.at<double>(2)) / 2.0f;
        Rvec.at<double>(0) =  (Rvec.at<double>(0) +  RvecN.at<double>(0)) / 2.0f;
        Rvec.at<double>(1) =  (Rvec.at<double>(1) +  RvecN.at<double>(1)) / 2.0f;
        Rvec.at<double>(2) =  (Rvec.at<double>(2) +  RvecN.at<double>(2)) / 2.0f;
    }
    else if ( T2 > 1.0f && T1 < 1.0f  && R1 < 0.5f )
    {
        tvec = tvec.clone();
        Rvec = Rvec.clone();
    }
    else
    {
        tvec = pTvec.clone();
        Rvec = pRvec.clone();
    }

    pTvec = tvec.clone();
    pRvec = Rvec.clone();

}

void FeatureTracker::estimatePoseN()
{
    cv::Mat Rvec = pRvec.clone();
    cv::Mat tvec = pTvec.clone();

    // cv::Mat RvecN = pRvec.clone();
    // cv::Mat tvecN = pTvec.clone();

    // optWithSolve(prePnts, Rvec, tvec, false);

    // calcOptical(prePnts, false);
    calcOpticalFlow(prePnts);

    if (curFrame == 1)
    {
        solvePnPIni(prePnts, Rvec, tvec, false);
        Eigen::Vector3d tra;
        Eigen::Matrix3d Rot;

        cv::Rodrigues(Rvec, Rvec);
        cv::cv2eigen(Rvec, Rot);
        cv::cv2eigen(tvec, tra);

        
        poseEstFrame.block<3, 3>(0, 0) = Rot.transpose();
        poseEstFrame.block<3, 1>(0, 3) = - tra;
    }

    // std::thread prevPntsThread(&FeatureTracker::optWithSolve, this, std::ref(prePnts), std::ref(Rvec), std::ref(tvec), false);
    // std::thread pntsThread(&FeatureTracker::optWithSolve, this, std::ref(pnts), std::ref(RvecN), std::ref(tvecN), true);

    // prevPntsThread.join();
    // pntsThread.join();

#if OPTICALIM
    // drawOptical("new", pLIm.rIm,pnts.left, pnts.newPnts);
    drawOptical("old", pLIm.rIm,prePnts.left, prePnts.newPnts);
#endif

    // checkRotTra(Rvec, tvec, RvecN, tvecN);
    // poseEstKal(Rvec, tvec, uStereo);

    optimizePose(prePnts, pnts, Rvec, tvec);


}

bool FeatureTracker::checkOutlier(const Eigen::Matrix4d& estimatedP, const cv::Point3d& p3d, const cv::Point2f& obs, const double thres, const float weight, cv::Point2f& out2d)
{
    Eigen::Vector4d p4d(p3d.x, p3d.y, p3d.z,1);
    p4d = estimatedP * p4d;
    const double invZ = 1.0f/p4d(2);

    const double invfx = 1.0f/fx;
    const double invfy = 1.0f/fy;


    const double u {fx*p4d(0)*invZ + cx};
    const double v {fy*p4d(1)*invZ + cy};

    const double errorU = weight * ((double)obs.x - u);
    const double errorV = weight * ((double)obs.y - v);

    const double error = (errorU * errorU + errorV * errorV);
    out2d = cv::Point2f((float)u, (float)v);
    if (error > thres)
        return false;
    else
        return true;
}

int FeatureTracker::checkOutliers(const Eigen::Matrix4d& estimatedP, const std::vector<cv::Point3d>& p3d, const std::vector<cv::Point2f>& obs, std::vector<bool>& inliers, const double thres, const std::vector<float>& weights)
{
    // std::vector<cv::Point2f>out2d;
    int nOut = 0;
    for (size_t i {0}; i < p3d.size(); i++)
    {
        cv::Point2f out;
        if ( !checkOutlier(estimatedP, p3d[i],obs[i], thres, weights[i],out))
        {
            nOut++;
            inliers[i] = false;
        }
        else
            inliers[i] = true;
        // out2d.push_back(out);
    }

// #if PROJECTIM
//     // drawOptical("new", pLIm.rIm,pnts.left, pnts.newPnts);
//     drawOptical("reproj", pLIm.rIm,obs, out2d);
//     cv::waitKey(waitTrials);
// #endif
    return nOut;
}

bool FeatureTracker::checkOutlierMap(const Eigen::Matrix4d& estimatedP, Eigen::Vector4d& p4d, const cv::Point2f& obs, const double thres, const float weight, cv::Point2f& out2d)
{
    p4d = estimatedP * p4d;
    const double invZ = 1.0f/p4d(2);

    const double invfx = 1.0f/fx;
    const double invfy = 1.0f/fy;


    const double u {fx*p4d(0)*invZ + cx};
    const double v {fy*p4d(1)*invZ + cy};

    const double errorU = weight * ((double)obs.x - u);
    const double errorV = weight * ((double)obs.y - v);

    const double error = (errorU * errorU + errorV * errorV);
    out2d = cv::Point2f((float)u, (float)v);
    if (error > thres)
        return false;
    else
        return true;
}

bool FeatureTracker::checkOutlierMap3d(const Eigen::Matrix4d& estimatedP, Eigen::Vector4d& p4d, const double thres, const float weight,  Eigen::Vector4d& obs)
{
    p4d = estimatedP * p4d;
    const double invZ = 1.0f/p4d(2);

    const double invfx = 1.0f/fx;
    const double invfy = 1.0f/fy;


    const double u {fx*p4d(0)*invZ + cx};
    const double v {fy*p4d(1)*invZ + cy};

    const double errorX = weight * ((double)obs(0) - p4d(0));
    const double errorY = weight * ((double)obs(1) - p4d(1));
    const double errorZ = weight * ((double)obs(2) - p4d(2));

    const double error = (errorX * errorX + errorY * errorY + errorZ * errorZ);
    if (error > thres)
        return false;
    else
        return true;
}

int FeatureTracker::checkOutliersMap(const Eigen::Matrix4d& estimatedP, TrackedKeys& prevKeysLeft, TrackedKeys& newKeys, std::vector<bool>& inliers, const double thres, const std::vector<float>& weights)
{
    // std::vector<cv::Point2f>out2d;
    int nOut = 0;
    for (size_t i {0}, end{prevKeysLeft.keyPoints.size()}; i < end; i++)
    {
        if (prevKeysLeft.mapPointIdx[i] < 0 || prevKeysLeft.matchedIdxs[i] < 0)
            continue;
        MapPoint* mp = map->mapPoints[prevKeysLeft.mapPointIdx[i]];
        if ( !mp->GetInFrame() || mp->GetIsOutlier())
        {
            inliers[i] = false;
            continue;
        }
        cv::Point2f out;
        Eigen::Vector4d p4d = mp->getWordPose4d();
        p4d = zedPtr->cameraPose.poseInverse * p4d;
        if ( newKeys.estimatedDepth[prevKeysLeft.matchedIdxs[i]] <= 0)
        {
            if ( !checkOutlierMap(estimatedP, p4d, prevKeysLeft.predKeyPoints[i].pt, thres, weights[i],out))
            {
                nOut++;
                inliers[i] = false;
            }
            else
                inliers[i] = true;

        }
        else
        {
            Eigen::Vector4d np4d;
            get3dFromKey(np4d, newKeys.keyPoints[prevKeysLeft.matchedIdxs[i]], newKeys.estimatedDepth[prevKeysLeft.matchedIdxs[i]]);
            if ( !checkOutlierMap3d(estimatedP, p4d, thres, weights[i],np4d))
            {
                nOut++;
                inliers[i] = false;
            }
            else
                inliers[i] = true;

        }
        // out2d.push_back(out);

    }

// #if PROJECTIM
//     // drawOptical("new", pLIm.rIm,pnts.left, pnts.newPnts);
//     drawOptical("reproj", pLIm.rIm,obs, out2d);
//     cv::waitKey(waitTrials);
// #endif
    return nOut;
}

void FeatureTracker::calcWeights(const SubPixelPoints& pnts, std::vector<float>& weights)
{
    const size_t end {pnts.points3DCurr.size()};
    weights.resize(end, 1.0f);
    // float leftN {0};
    // float rightN {0};
    // const int off {10};
    // const float mid {(float)zedPtr->mWidth/2.0f};
    // for (size_t i {0}; i < end; i++)
    // {
    //     const float& px = pnts.newPnts[i].x;
    //     if ( px < mid )
    //         leftN += mid - px;
    //     else if ( px > mid )
    //         rightN += px - mid;
    // }
    // Logging("LeftN", leftN,3);
    // Logging("rightN", rightN,3);
    // const float multL {(float)rightN/(float)leftN};
    // const float multR {(float)leftN/(float)rightN};
    // float sum {0};
    // const float vd {zedPtr->mBaseline * 40};
    // const float sig {vd};
    // for (size_t i {0}; i < end; i++)
    // {
    //     // const float& px = pnts.newPnts[i].x;
    //     // if ( px < mid )
    //     //     weights[i] *= multL;
    //     // else if ( px > mid )
    //     //     weights[i] *= multR;
    //     const float& depth  = (float)pnts.points3DCurr[i].z;
    //     if ( depth > vd)
    //     {
    //         float prob = norm_pdf(depth, vd, sig);
    //         weights[i] *= 2 * prob * vd;
    //     }
    //     // sum += weights[i];
    // }

    // float aver {sum/(float)end};
    // float xd {0};
    // for (size_t i {0}; i < end; i++)
    // {
    //     weights[i] /= aver;
    //     xd += weights[i];
    // }

    // Logging("summm", xd/end,3);


}

void FeatureTracker::optimizePose(SubPixelPoints& prePnts, SubPixelPoints& pnts, cv::Mat& Rvec, cv::Mat& tvec)
{

    std::vector<bool> inliers(prePnts.points3DCurr.size(),true);
    std::vector<bool> prevInliers(prePnts.points3DCurr.size(),true);

    std::vector<float> weights;
    calcWeights(prePnts, weights);

    std::vector<double>thresholds = {15.6f,9.8f,7.815f,7.815f};
    // bool rerun {false};
    int nIn = prePnts.points3DCurr.size();
    bool rerun = true;

    Eigen::Matrix4d prevCalcPose = poseEstFrame;

    for (size_t times = 0; times < 4; times++)
    {
        ceres::Problem problem;

        Eigen::Vector3d frame_tcw;
        Eigen::Quaterniond frame_qcw;

        Eigen::Matrix4d frame_pose = poseEstFrame;
        Eigen::Matrix3d frame_R;
        frame_R = frame_pose.block<3, 3>(0, 0);
        frame_tcw = frame_pose.block<3, 1>(0, 3);
        frame_qcw = Eigen::Quaterniond(frame_R);
        
        Eigen::Matrix3d K = Eigen::Matrix3d::Identity();
        K(0,0) = fx;
        K(1,1) = fy;
        K(0,2) = cx;
        K(1,2) = cy;
        ceres::Manifold* quaternion_local_parameterization = new ceres::EigenQuaternionManifold;
        ceres::LossFunction* loss_function = new ceres::HuberLoss(1.0);
        // problem.AddParameterBlock(cameraR,4);
        // problem.AddParameterBlock(cameraT,3);
        for (size_t i{0}, end{prePnts.points3DCurr.size()}; i < end; i++)
        {
            if ( !inliers[i] )
                continue;

            Eigen::Vector2d obs((double)prePnts.newPnts[i].x, (double)prePnts.newPnts[i].y);
            Eigen::Vector3d point(prePnts.points3DCurr[i].x, prePnts.points3DCurr[i].y, prePnts.points3DCurr[i].z);
            ceres::CostFunction* costf = OptimizePose::Create(K, point, obs, weights[i]);
            
            problem.AddResidualBlock(costf, loss_function /* squared loss */,frame_tcw.data(), frame_qcw.coeffs().data());

            problem.SetManifold(frame_qcw.coeffs().data(),
                                        quaternion_local_parameterization);
        }
        ceres::Solver::Options options;
        options.linear_solver_type = ceres::SPARSE_NORMAL_CHOLESKY;
        options.use_explicit_schur_complement = true;
        
        options.max_num_iterations = 100;
        // options.max_solver_time_in_seconds = 0.05;

        // options.trust_region_strategy_type = ceres::DOGLEG;
        options.minimizer_progress_to_stdout = false;
        ceres::Solver::Summary summary;
        ceres::Solve(options, &problem, &summary);
        // Logging("sum",summary.FullReport(),3);
        Eigen::Matrix3d R = frame_qcw.normalized().toRotationMatrix();
        Eigen::Matrix4d pose = Eigen::Matrix4d::Identity();
        pose.block<3, 3>(0, 0) = R.transpose();
        pose.block<3, 1>(0, 3) = - frame_tcw;
        // zedPtr->cameraPose.setPose(pose);
        // zedPtr->cameraPose.setInvPose(pose.inverse());

        poseEstFrame = pose;
        Eigen::Matrix4d pFInv = pose.inverse();
        int nOut = checkOutliers(pFInv, prePnts.points3DCurr, prePnts.newPnts, inliers, thresholds[times], weights);
        int nInAfter = nIn - nOut;

        // if ( nInAfter < (nIn / 2))
        // {
        //     // if more than half then keep the other bunch
        //     // that means that we take for granted that more than half are right.

        //     // You can change all the inliers to the opposite (false = true, true = false)
        //     // And rerun the last loop with the new inliers


        //     if ( rerun )
        //     {
        //         for (size_t i{0}, end{prePnts.points3DCurr.size()}; i < end; i++)
        //         {
        //             if ( inliers[i] )
        //                 inliers[i] = false;
        //             else
        //                 inliers[i] = true;
        //         }
        //         times = times - 1;
        //         poseEstFrame = prevCalcPose;
        //         prevInliers = inliers;
        //         rerun = false;
        //         continue;

        //     }
        //     else
        //     {
        //         poseEstFrame = prevCalcPose;
        //         inliers = prevInliers;
        //     }
            

        //     break;
        // }
        // else
        // {
            prevCalcPose = poseEstFrame;
            prevInliers = inliers;
        // }

    }

    prePnts.reduce<bool>(prevInliers);

#if PROJECTIM
    // drawOptical("new", pLIm.rIm,pnts.left, pnts.newPnts);
    drawOptical("optimization", pLIm.rIm,prePnts.left, prePnts.newPnts);
    // cv::waitKey(waitTrials);
#endif

    publishPose();
    // float sum {0};
    // const float middd = (float)zedPtr->mWidth/2.0f;
    // const size_t sizeee = prePnts.points3DCurr.size();
    // for ( size_t iS {0}; iS < sizeee; iS++ )
    //     sum += (prePnts.left[iS].x - middd);

    // leftRight += sum/ sizeee;

    // Logging("LEEEEEEEEEEEEEEEFTTTTTTTTTT", leftRight,3);


    // Eigen::Matrix3d Reig = temp.block<3, 3>(0, 0);
    // Eigen::Vector3d Teig = temp.block<3, 1>(0, 3);
    // cv::Mat Rot, tra;

    // cv::eigen2cv(Reig, Rot);
    // cv::eigen2cv(Teig, tra);

    // cv::Rodrigues(Rot, Rot);

    // poseEstKal(Rot, tra, uStereo);

    

    // poseEst = poseEst * poseEstFrame;
    // poseEstFrameInv = poseEstFrame.inverse();
    // prevWPose = zedPtr->cameraPose.pose;
    // prevWPoseInv = zedPtr->cameraPose.poseInverse;
    // zedPtr->cameraPose.setPose(poseEst);
    // zedPtr->cameraPose.setInvPose(poseEst.inverse());
    // predNPose = poseEst * (prevWPoseInv * poseEst);
    // predNPoseInv = predNPose.inverse();
    // options.gradient_tolerance = 1e-16;
    // options.function_tolerance = 1e-16;
    // options.parameter_tolerance = 1e-16;
    // double cost {0.0};
    // problem.Evaluate(ceres::Problem::EvaluateOptions(), &cost, NULL, NULL, NULL);
    // Logging("cost ", summary.final_cost,3);
    // Logging("R bef", Rvec,3);
    // Logging("T bef", tvec,3);
    // Rvec.at<double>(0) = euler[0];
    // Rvec.at<double>(1) = euler[1];
    // Rvec.at<double>(2) = euler[2];
    // tvec.at<double>(0) = cameraT[0];
    // tvec.at<double>(1) = cameraT[1];
    // tvec.at<double>(2) = cameraT[2];
    // Logging("R after", Rvec,3);
    // Logging("T after", tvec,3);
}

void FeatureTracker::get3dFromKey(Eigen::Vector4d& pnt4d, const cv::KeyPoint& pnt, const float depth)
{
    const double zp = (double)depth;
    const double xp = (double)(((double)pnt.pt.x-cx)*zp/fx);
    const double yp = (double)(((double)pnt.pt.y-cy)*zp/fy);
    pnt4d = Eigen::Vector4d(xp, yp, zp,1);
}

void FeatureTracker::optimizePoseCeres(TrackedKeys& prevKeys, TrackedKeys& newKeys)
{


    const size_t prevS { prevKeys.keyPoints.size()};
    const size_t newS { newKeys.keyPoints.size()};
    const size_t startPrev {newS - prevS};
    prevKeys.inliers.resize(prevS,true);
    std::vector<bool> inliers(prevS,true);
    std::vector<bool> prevInliers(prevS,true);

    std::vector<float> weights;
    // calcWeights(prePnts, weights);
    weights.resize(newS, 1.0f);

    std::vector<double>thresholds = {15.6f,9.8f,7.815f,7.815f};
    // bool rerun {false};
    int nIn = prevS;
    bool rerun = true;

    Eigen::Matrix3d K = Eigen::Matrix3d::Identity();
    K(0,0) = fx;
    K(1,1) = fy;
    K(0,2) = cx;
    K(1,2) = cy;
    Eigen::Matrix4d prevCalcPose = poseEstFrameInv;
    std::vector<cv::Point2f> calc, predictedd;
    for (size_t times = 0; times < 4; times++)
    {
        std::vector<cv::Point2f> found, observ;
        ceres::Problem problem;
        Eigen::Vector3d frame_tcw;
        Eigen::Quaterniond frame_qcw;

        Eigen::Matrix4d frame_pose = poseEstFrameInv;
        Eigen::Matrix3d frame_R;
        frame_R = frame_pose.block<3, 3>(0, 0);
        frame_tcw = frame_pose.block<3, 1>(0, 3);
        frame_qcw = Eigen::Quaterniond(frame_R);
        
        ceres::Manifold* quaternion_local_parameterization = new ceres::EigenQuaternionManifold;
        ceres::LossFunction* loss_function = new ceres::HuberLoss(sqrt(7.815f));
        int count {0};
        for (size_t i{0}, end{prevKeys.keyPoints.size()}; i < end; i++)
        {
            if (prevKeys.mapPointIdx[i] < 0 || prevKeys.matchedIdxs[i] < 0)
                continue;
            MapPoint* mp = map->mapPoints[prevKeys.mapPointIdx[i]];
            if ( !mp->GetInFrame() || mp->GetIsOutlier())
                continue;
            if ( !inliers[i] )
                continue;
            if ( newKeys.estimatedDepth[prevKeys.matchedIdxs[i]] > 0)
                continue;
            count ++;
            Eigen::Vector2d obs((double)prevKeys.predKeyPoints[i].pt.x, (double)prevKeys.predKeyPoints[i].pt.y);
            observ.push_back(prevKeys.predKeyPoints[i].pt);
            Eigen::Vector4d point = mp->getWordPose4d();
            point = zedPtr->cameraPose.poseInverse * point;
            Eigen::Vector3d point3d(point(0), point(1),point(2));
            ceres::CostFunction* costf = OptimizePose::Create(K, point3d, obs, (double)weights[i]);
            Eigen::Vector3d pmoved;
            pmoved = K * point3d;
            found.push_back(cv::Point2f((float)pmoved(0)/(float)pmoved(2), (float)pmoved(1)/(float)pmoved(2)));
            
            problem.AddResidualBlock(costf, loss_function /* squared loss */,frame_tcw.data(), frame_qcw.coeffs().data());

            problem.SetManifold(frame_qcw.coeffs().data(),
                                        quaternion_local_parameterization);
        }

        for (size_t i{0}, end{prevKeys.keyPoints.size()}; i < end; i++)
        {
            if (prevKeys.mapPointIdx[i] < 0 || prevKeys.matchedIdxs[i] < 0)
                continue;
            MapPoint* mp = map->mapPoints[prevKeys.mapPointIdx[i]];
            if ( !mp->GetInFrame() || mp->GetIsOutlier())
                continue;
            if ( !inliers[i] )
                continue;
            if ( newKeys.estimatedDepth[prevKeys.matchedIdxs[i]] <= 0)
                continue;
            count ++;

            Eigen::Vector4d np4d;
            get3dFromKey(np4d, newKeys.keyPoints[prevKeys.matchedIdxs[i]], newKeys.estimatedDepth[prevKeys.matchedIdxs[i]]);
            observ.push_back(newKeys.keyPoints[prevKeys.matchedIdxs[i]].pt);
            // np4d = zedPtr->cameraPose.pose * np4d;
            Eigen::Vector3d obs(np4d(0), np4d(1),np4d(2));
            Eigen::Vector4d point = mp->getWordPose4d();
            point = zedPtr->cameraPose.poseInverse * point;
            Eigen::Vector3d point3d(point(0), point(1),point(2));
            ceres::CostFunction* costf = OptimizePoseICP::Create(K, point3d, obs, (double)weights[i]);
            Eigen::Vector3d pmoved;
            pmoved = K * point3d;
            found.push_back(cv::Point2f((float)pmoved(0)/(float)pmoved(2), (float)pmoved(1)/(float)pmoved(2)));
            
            problem.AddResidualBlock(costf, loss_function /* squared loss */,frame_tcw.data(), frame_qcw.coeffs().data());

            problem.SetManifold(frame_qcw.coeffs().data(),
                                        quaternion_local_parameterization);
        }

        ceres::Solver::Options options;
        options.linear_solver_type = ceres::SPARSE_NORMAL_CHOLESKY;
        options.use_explicit_schur_complement = true;
        
        options.max_num_iterations = 100;
        options.minimizer_progress_to_stdout = false;
        ceres::Solver::Summary summary;
        ceres::Solve(options, &problem, &summary);
        Eigen::Matrix3d R = frame_qcw.normalized().toRotationMatrix();
        Eigen::Matrix4d pose = Eigen::Matrix4d::Identity();
        pose.block<3, 3>(0, 0) = R.transpose();
        pose.block<3, 1>(0, 3) = - frame_tcw;

        Eigen::Matrix4d pFInv = pose.inverse();
        int nOut = checkOutliersMap(pFInv, prevKeys, newKeys, inliers, thresholds[times], weights);
        // int nInAfter = nIn - nOut;
        Logging("POSE EST", pose,3);
        prevCalcPose = pose;
        prevInliers = inliers;
        drawOptical("optimization", pLIm.rIm, observ, found);
        cv::waitKey(1);

    }
    poseEstFrame = prevCalcPose;
    for ( size_t i {0}; i < inliers.size(); i++)
    {
        if ( !inliers[i] )
        {
            if (prevKeys.mapPointIdx[i] < 0)
                    continue;
            MapPoint* mp = map->mapPoints[prevKeys.mapPointIdx[i]];
            mp->SetIsOutlier(true);
            
            newKeys.close[prevKeys.matchedIdxs[i]] = false;
        }
        else
            if ( prevKeys.mapPointIdx[i] >= 0)
            {
                if ( prevKeys.matchedIdxs[i] >= 0)
                {
                    MapPoint* mp = map->mapPoints[prevKeys.mapPointIdx[i]];
                    Eigen::Vector4d point = mp->getWordPose4d();
                    point = zedPtr->cameraPose.poseInverse * point;
                    point = poseEstFrame.inverse() * point;
                    Eigen::Vector3d point3d(point(0), point(1),point(2));
                    point3d = K * point3d;
                    calc.push_back(cv::Point2f((float)point3d(0)/(float)point3d(2), (float)point3d(1)/(float)point3d(2)));
                    predictedd.push_back(newKeys.keyPoints[prevKeys.matchedIdxs[i]].pt);
                }
            }
    }
    drawOptical("reproj erro", pLIm.rIm, predictedd, calc);
    cv::waitKey(1);
    // prevKeys.reduce<bool>(prevInliers);
    // prePnts.reduce<bool>(prevInliers);

#if PROJECTIM
    // drawOptical("new", pLIm.rIm,pnts.left, pnts.newPnts);
#endif

    publishPoseCeres();
}

void FeatureTracker::optimizePoseORB(TrackedKeys& prevKeys, TrackedKeys& newKeys)
{


    const size_t prevS { prevKeys.keyPoints.size()};
    const size_t newS { newKeys.keyPoints.size()};
    const size_t startPrev {newS - prevS};
    prevKeys.inliers.resize(prevS,true);
    std::vector<bool> inliers(prevS,true);
    std::vector<bool> prevInliers(prevS,true);

    std::vector<float> weights;
    // calcWeights(prePnts, weights);
    weights.resize(newS, 1.0f);

    std::vector<double>thresholds = {15.6f,9.8f,7.815f,7.815f};
    // bool rerun {false};
    int nIn = prevS;
    bool rerun = true;

    Eigen::Matrix4d prevCalcPose = poseEst;

    std::vector<cv::Point2f> found, observ;
    for (size_t times = 0; times < 1; times++)
    {
        ceres::Problem problem;
        Eigen::Vector3d frame_tcw;
        Eigen::Quaterniond frame_qcw;

        Eigen::Matrix4d frame_pose = poseEst;
        Eigen::Matrix3d frame_R;
        frame_R = frame_pose.block<3, 3>(0, 0);
        frame_tcw = frame_pose.block<3, 1>(0, 3);
        frame_qcw = Eigen::Quaterniond(frame_R);
        
        Eigen::Matrix3d K = Eigen::Matrix3d::Identity();
        K(0,0) = fx;
        K(1,1) = fy;
        K(0,2) = cx;
        K(1,2) = cy;
        ceres::Manifold* quaternion_local_parameterization = new ceres::EigenQuaternionManifold;
        ceres::LossFunction* loss_function = new ceres::HuberLoss(sqrt(7.815f));
        for (size_t i{0}, end{prevKeys.keyPoints.size()}; i < end; i++)
        {
            if ( !prevKeys.close[i] )
                continue;
            MapPoint* mp = map->mapPoints[prevKeys.mapPointIdx[i]];
            if ( !mp->GetInFrame() || mp->GetIsOutlier())
                continue;
            Eigen::Vector2d obs((double)prevKeys.predKeyPoints[i].pt.x, (double)prevKeys.predKeyPoints[i].pt.y);
            observ.push_back(prevKeys.predKeyPoints[i].pt);
            Eigen::Vector3d point = mp->getWordPose3d();
            ceres::CostFunction* costf = OptimizePose::Create(K, point, obs, weights[i]);
            Eigen::Vector3d pmoved = frame_R * point + frame_tcw;
            pmoved = K * pmoved;
            found.push_back(cv::Point2f((float)pmoved(0)/(float)pmoved(2), (float)pmoved(1)/(float)pmoved(2)));
            
            problem.AddResidualBlock(costf, loss_function /* squared loss */,frame_tcw.data(), frame_qcw.coeffs().data());

            problem.SetManifold(frame_qcw.coeffs().data(),
                                        quaternion_local_parameterization);
        }

        // for (size_t i{0}, end{prevKeys.keyPoints.size()}; i < end; i++)
        // {
        //     if ( !inliers[i] )
        //         continue;

        //     Eigen::Vector2d obs((double)prePnts.newPnts[i].x, (double)prePnts.newPnts[i].y);
        //     Eigen::Vector3d point(prePnts.points3DCurr[i].x, prePnts.points3DCurr[i].y, prePnts.points3DCurr[i].z);
        //     ceres::CostFunction* costf = OptimizePose::Create(K, point, obs, weights[i]);
            
        //     problem.AddResidualBlock(costf, loss_function /* squared loss */,frame_tcw.data(), frame_qcw.coeffs().data());

        //     problem.SetManifold(frame_qcw.coeffs().data(),
        //                                 quaternion_local_parameterization);
        // }

        ceres::Solver::Options options;
        options.linear_solver_type = ceres::SPARSE_NORMAL_CHOLESKY;
        options.use_explicit_schur_complement = true;
        
        options.max_num_iterations = 100;
        options.minimizer_progress_to_stdout = false;
        ceres::Solver::Summary summary;
        ceres::Solve(options, &problem, &summary);
        Eigen::Matrix3d R = frame_qcw.normalized().toRotationMatrix();
        Eigen::Matrix4d pose = Eigen::Matrix4d::Identity();
        pose.block<3, 3>(0, 0) = R.transpose();
        pose.block<3, 1>(0, 3) = - frame_tcw;

        poseEst = pose;
        Eigen::Matrix4d pFInv = pose.inverse();
        int nOut = checkOutliersMap(pFInv, prevKeys, newKeys, inliers, thresholds[times], weights);
        int nInAfter = nIn - nOut;
        Logging("POSE EST", pose,3);
        prevCalcPose = poseEst;
        prevInliers = inliers;

    }
    // prevKeys.reduce<bool>(prevInliers);
    // prePnts.reduce<bool>(prevInliers);

#if PROJECTIM
    // drawOptical("new", pLIm.rIm,pnts.left, pnts.newPnts);
    drawOptical("optimization", pLIm.rIm, observ, found);
    cv::waitKey(waitTrials);
#endif

    publishPoseCeres();
}

void FeatureTracker::optWithSolve(SubPixelPoints& pnts, cv::Mat& Rvec, cv::Mat& tvec, const bool new3D)
{
    calcOptical(pnts, new3D);
    solvePnPIni(pnts, Rvec, tvec, new3D);

}

void FeatureTracker::computeStereoMatches(TrackedKeys& keysLeft, TrackedKeys& prevLeftKeys)
{
    std::vector<cv::Point2f> tobeRemoved;

    std::vector<cv::KeyPoint> rightKeys;
    cv::Mat rightDesc;

    Timer fast("FAST");
    std::thread extractLeft(&FeatureExtractor::extractLeftFeatures, std::ref(feLeft), std::ref(lIm.im), std::ref(keysLeft.keyPoints), std::ref(keysLeft.Desc), std::ref(prevLeftKeys));
    std::thread extractRight(&FeatureExtractor::computeFASTandDesc, std::ref(feRight), std::ref(rIm.im), std::ref(rightKeys), std::ref(tobeRemoved),std::ref(rightDesc));

    extractLeft.join();
    extractRight.join();

    fm.findStereoMatchesCloseFar(lIm.im, rIm.im, rightDesc, rightKeys, keysLeft);

    drawKeyPointsCloseFar("new method", lIm.rIm, keysLeft, rightKeys);
}

void FeatureTracker::computeStereoMatchesORB(TrackedKeys& keysLeft, TrackedKeys& prevLeftKeys)
{
    Timer both ("Both");
    std::vector<cv::KeyPoint> rightKeys;
    cv::Mat rightDesc;

    Timer orb("orb");
    std::thread extractLeft(&FeatureExtractor::extractLeftFeaturesORB, std::ref(feLeft), std::ref(lIm.im), std::ref(keysLeft.keyPoints), std::ref(keysLeft.Desc), std::ref(prevLeftKeys));
    std::thread extractRight(&FeatureExtractor::computeORBandDesc, std::ref(feRight), std::ref(rIm.im), std::ref(rightKeys),std::ref(rightDesc));

    extractLeft.join();
    extractRight.join();

    fm.findStereoMatchesCloseFar(lIm.im, rIm.im, rightDesc, rightKeys, keysLeft);

    drawKeyPointsCloseFar("new method", lIm.rIm, keysLeft, rightKeys);
}

void FeatureTracker::drawKeyPointsCloseFar(const char* com, const cv::Mat& im, const TrackedKeys& keysLeft, const std::vector<cv::KeyPoint>& right)
{
        cv::Mat outIm = im.clone();
        const size_t end {keysLeft.keyPoints.size()};
        for (size_t i{0};i < end; i ++ )
        {
            if ( keysLeft.estimatedDepth[i] > 0)
            {
                cv::circle(outIm, keysLeft.keyPoints[i].pt,2,cv::Scalar(0,255,0));
                cv::line(outIm, keysLeft.keyPoints[i].pt, right[keysLeft.rightIdxs[i]].pt,cv::Scalar(0,0,255));
                cv::circle(outIm, right[keysLeft.rightIdxs[i]].pt,2,cv::Scalar(255,0,0));
            }
        }
        cv::imshow(com, outIm);
        cv::waitKey(waitImClo);

}

void FeatureTracker::drawLeftMatches(const char* com, const cv::Mat& im, const TrackedKeys& prevKeysLeft, const TrackedKeys& keysLeft)
{
        cv::Mat outIm = im.clone();
        const size_t end {prevKeysLeft.keyPoints.size()};
        for (size_t i{0};i < end; i ++ )
        {
            if ( prevKeysLeft.matchedIdxs[i] >= 0)
            {
                cv::circle(outIm, prevKeysLeft.keyPoints[i].pt,2,cv::Scalar(0,255,0));
                cv::line(outIm, prevKeysLeft.keyPoints[i].pt, keysLeft.keyPoints[prevKeysLeft.matchedIdxs[i]].pt,cv::Scalar(0,0,255));
                cv::circle(outIm, keysLeft.keyPoints[prevKeysLeft.matchedIdxs[i]].pt,2,cv::Scalar(255,0,0));
            }
        }
        cv::imshow(com, outIm);
        cv::waitKey(waitImClo);

}

void FeatureTracker::Track(const int frames)
{
    for (curFrame = 0; curFrame < frames; curFrame++)
    {

        zedPtr->addKeyFrame = true;
        setLRImages(curFrame);

    //     Eigen::Vector4d p(0,0,0,1);
    // map->addMapPoint(p);

    // Eigen::Vector4d p2(500,432,5234,3211);
    // map->addMapPoint(p2);

        findStereoFeatures(lIm.im, rIm.im, pnts);

        triangulate3DPoints(pnts);

        if ( curFrame == 0 )
        {
            setPreviousValuesIni();
            continue;
        }


        estimatePoseN();

        setPreviousValues();

        // calcOptical(pnts, true);
        // calcOptical(prePnts, false);

        // solvePnPIni(prePnts, Rvec, tvec, false);
        // solvePnPIni(pnts, Rvec, tvec, true);




        // if ( curFrame == 1 )
        //     opticalFlow();
        // else
        //     opticalFlowPredict();
        // getPoseCeresNew();

        // setPreTrial();

        // Logging("ustereo", uStereo,3);
        // Logging("umono", uMono,3);
    }
    datafile.close();
}

void FeatureTracker::predictPntsLeft(TrackedKeys& keysLeft)
{
    std::vector<bool>inliers (keysLeft.keyPoints.size(),true);
    for ( size_t i{0}, end {keysLeft.keyPoints.size()}; i < end; i++)
    {
        if (keysLeft.mapPointIdx[i] >= 0)
        {
            cv::Point2f predPnt;
            if ( getPredInFrame(predNPoseInv, map->mapPoints[keysLeft.mapPointIdx[i]], predPnt))
                keysLeft.predKeyPoints[i].pt = predPnt;
            else
                inliers[i] = false;
        }
    }
    keysLeft.reduce<bool>(inliers);
}

void FeatureTracker::computeOpticalLeft(TrackedKeys& keysLeft)
{
    const size_t keysLeftSize {keysLeft.keyPoints.size()};
    std::vector<float> err;
    std::vector<uchar> inliers, inliers2;
    std::vector<cv::Point2f> keysp2f, predkeysp2f;
    keysLeft.predKeyPoints = keysLeft.keyPoints;
    if ( curFrame == 1 )
    {

        cv::KeyPoint::convert(keysLeft.keyPoints, keysp2f);
        cv::calcOpticalFlowPyrLK(pLIm.im, lIm.im, keysp2f, predkeysp2f, inliers, err,cv::Size(21,21),3, criteria);

    }
    else
    {
        predictPntsLeft(keysLeft);
        keysp2f.reserve(keysLeft.keyPoints.size());
        predkeysp2f.reserve(keysLeft.keyPoints.size());
        for (size_t i{0}, end(keysLeft.keyPoints.size()); i < end; i++)
        {
            keysp2f.emplace_back(keysLeft.keyPoints[i].pt);
            predkeysp2f.emplace_back(keysLeft.predKeyPoints[i].pt);
        }
        // cv::KeyPoint::convert(keysLeft.keyPoints, keysp2f);
        // cv::KeyPoint::convert(keysLeft.predKeyPoints, predkeysp2f);

        drawPointsTemp<cv::Point2f,cv::Point2f>("optical", pLIm.rIm, keysp2f, predkeysp2f);


        cv::calcOpticalFlowPyrLK(pLIm.im, lIm.im, keysp2f, predkeysp2f, inliers, err,cv::Size(21,21),1, criteria, cv::OPTFLOW_USE_INITIAL_FLOW);
    }

    std::vector<cv::Point2f> temp = keysp2f;
    cv::calcOpticalFlowPyrLK(lIm.im, pLIm.im, predkeysp2f, temp, inliers2, err,cv::Size(21,21),1, criteria, cv::OPTFLOW_USE_INITIAL_FLOW);

    for (size_t i {0}; i < pnts.left.size(); i ++)
    {
        if ( inliers[i] && inliers2[i] && pointsDist(temp[i],keysp2f[i]) <= 0.25)
            inliers[i] = true;
        else
            inliers[i] = false;
    }

    const int w {zedPtr->mWidth};
    const int h {zedPtr->mHeight};

    for ( size_t i{0}, end{prevLeftPnts.predKeyPoints.size()}; i < end; i++)
    {
        const cv::Point2f& p2f = predkeysp2f[i];
        if (inliers[i] )
        {
            if (p2f.x > 0 && p2f.x < w && p2f.y > 0 && p2f.y < h)
            {
                prevLeftPnts.predKeyPoints[i].pt = predkeysp2f[i];
            }
            else
                inliers[i] = false;
        }
    }
    prevLeftPnts.reduce<uchar>(inliers);
    reduceVectorTemp<cv::Point2f,uchar>(predkeysp2f, inliers);
    reduceVectorTemp<cv::Point2f,uchar>(keysp2f, inliers);
    inliers.clear();
    cv::findFundamentalMat(keysp2f, predkeysp2f,inliers);
    prevLeftPnts.reduce<uchar>(inliers);
    reduceVectorTemp<cv::Point2f,uchar>(predkeysp2f, inliers);
    reduceVectorTemp<cv::Point2f,uchar>(keysp2f, inliers);
    size_t s {keysp2f.size()};
    size_t s2 {predkeysp2f.size()};


}

void setMaskOfIdxs(cv::Mat& mask, const TrackedKeys& keysLeft)
{
    std::vector<cv::KeyPoint>::const_iterator it, end(keysLeft.keyPoints.end());
    for ( it = keysLeft.keyPoints.begin(); it != end; it++)
    {

    }
}

void FeatureTracker::addMapPnts(TrackedKeys& keysLeft)
{
    const size_t prevE { prevLeftPnts.keyPoints.size()};
    const size_t newE { keysLeft.keyPoints.size()};
    const size_t start {newE - prevE};
    prevLeftPnts.keyPoints.reserve(keysLeft.keyPoints.size());
    // cv::Mat mask = cv::Mat(zedPtr->mHeight , zedPtr->mWidth , CV_16UC1, cv::Scalar(0));

    for (size_t i{start}, end {newE}; i < end; i++)
    {

        if ( keysLeft.estimatedDepth[i] > 0)
        {
            const double zp = (double)keysLeft.estimatedDepth[i];
            const double xp = (double)(((double)keysLeft.keyPoints[i].pt.x-cx)*zp/fx);
            const double yp = (double)(((double)keysLeft.keyPoints[i].pt.y-cy)*zp/fy);
            Eigen::Vector4d p(xp, yp, zp, 1);
            p = zedPtr->cameraPose.pose * p;
            if ( prevLeftPnts.mapPointIdx[i - start] >= 0)
            {
                Eigen::Vector4d mp = map->mapPoints[prevLeftPnts.mapPointIdx[i-start]]->getWordPose4d();
                p(0) = (p(0) + mp(0)) / 2;
                p(1) = (p(1) + mp(1)) / 2;
                p(2) = (p(2) + mp(2)) / 2;
                map->mapPoints[prevLeftPnts.mapPointIdx[i-start]]->updateMapPoint(p, keysLeft.Desc.row(i), keysLeft.keyPoints[i]);
            }
            else
            {
                prevLeftPnts.mapPointIdx[i - start] = map->pIdx;
                map->addMapPoint(p, keysLeft.Desc.row(i), keysLeft.keyPoints[i], keysLeft.close[i]);
            }
            prevLeftPnts.estimatedDepth[i - start] = keysLeft.estimatedDepth[i];
            prevLeftPnts.close[i - start] = keysLeft.close[i];
        }
        prevLeftPnts.keyPoints[i - start] = keysLeft.keyPoints[i];
        prevLeftPnts.trackCnt[i - start] ++;
    }
    for (size_t i{0}, end {start}; i < end; i++)
    {

        if ( keysLeft.estimatedDepth[i] > 0 )
        {
            const double zp = (double)keysLeft.estimatedDepth[i];
            const double xp = (double)(((double)keysLeft.keyPoints[i].pt.x-cx)*zp/fx);
            const double yp = (double)(((double)keysLeft.keyPoints[i].pt.y-cy)*zp/fy);
            Eigen::Vector4d p(xp, yp, zp, 1);
            p = zedPtr->cameraPose.pose * p;
            prevLeftPnts.add(keysLeft.keyPoints[i], map->pIdx, keysLeft.estimatedDepth[i], keysLeft.close[i], 0 );
            map->addMapPoint(p, keysLeft.Desc.row(i), keysLeft.keyPoints[i], keysLeft.close[i]);
            // Here check if another point is close to see if that point is 3d
            
        }
        else
        {
            prevLeftPnts.add(keysLeft.keyPoints[i], -1, -1.0f, keysLeft.close[i], 0 );

        }
    }
    map->addKeyFrame(zedPtr->cameraPose.pose);
}

bool FeatureTracker::getPredInFrame(const Eigen::Matrix4d& predPose, MapPoint* mp, cv::Point2f& predPnt)
{
    if ( mp->GetInFrame() && !mp->GetIsOutlier())
    {
        Eigen::Vector4d p4d = predPose * mp->getWordPose4d();

        if (p4d(2) <= 0.0f )
        {
            mp->SetInFrame(false);
            return false;
        }

        const double invfx = 1.0f/fx;
        const double invfy = 1.0f/fy;


        double u {fx*p4d(0)/p4d(2) + cx};
        double v {fy*p4d(1)/p4d(2) + cy};

        const int h = zedPtr->mHeight;
        const int w = zedPtr->mWidth;

        if ( u < 0 || u > w  || v < 0 || v > h )
        {
            mp->SetInFrame(false);
            return false;
        }
        else
        {
            predPnt = cv::Point2f((float)u, (float)v);
            return true;
        }
    }
    return false;
}

bool FeatureTracker::getPoseInFrame(const Eigen::Matrix4d& pose, const Eigen::Matrix4d& predPose, MapPoint* mp, cv::Point2f& pnt, cv::Point2f& predPnt)
{
    if ( mp->GetInFrame() && !mp->GetIsOutlier())
    {
        Eigen::Vector4d p4d = pose * mp->getWordPose4d();
        Eigen::Vector4d predP4d = predPose * mp->getWordPose4d();
        

        if (p4d(2) <= 0.0f || predP4d(2) <= 0.0f)
        {
            mp->SetInFrame(false);
            return false;
        }

        const double invfx = 1.0f/fx;
        const double invfy = 1.0f/fy;


        double u {fx*p4d(0)/p4d(2) + cx};
        double v {fy*p4d(1)/p4d(2) + cy};

        double pu {fx*predP4d(0)/predP4d(2) + cx};
        double pv {fy*predP4d(1)/predP4d(2) + cy};

        const int h = zedPtr->mHeight;
        const int w = zedPtr->mWidth;

        if ( u < 0 || u > w || pu < 0 || pu > w || v < 0 || v > h || pv < 0 || pv > h)
        {
            mp->SetInFrame(false);
            return false;
        }
        else
        {
            pnt = cv::Point2f((float)u, (float)v);
            predPnt = cv::Point2f((float)pu, (float)pv);
            return true;
        }
    }
    return false;
}

void FeatureTracker::addStereoPnts()
{

    // std::unordered_map<unsigned long, MapPoint*>::const_iterator end(map->mapPoints.end());
    // std::unordered_map<unsigned long, MapPoint*>::iterator it;
    // for ( it = map->mapPoints.begin(); it!= end; it++)
    // {
    //     cv::Point2f pnt, predPnt;
    //     if ( getPoseInFrame(zedPtr->cameraPose.pose, predNPoseInv, it->second, pnt, predPnt))
    //     {
    //         const cv::KeyPoint& kp = it->second->obs[0];
    //         prevLeftPnts.keyPoints.emplace_back(pnt, kp.size, kp.angle, kp.response, kp.octave);
    //         prevLeftPnts.predPos.emplace_back(predPnt);
    //     }
    // }
}

void FeatureTracker::getNewMatchedPoints(TrackedKeys& keysMatched, TrackedKeys& newPnts)
{
    const size_t prevTrS { prevLeftPnts.keyPoints.size()};
    const size_t keysMS { keysMatched.keyPoints.size()};

    const size_t start { keysMS - prevTrS};
}

void FeatureTracker::Track2(const int frames)
{
    for (curFrame = 0; curFrame < frames; curFrame++)
    {
        zedPtr->addKeyFrame = true;
        setLRImages(curFrame);

        TrackedKeys keysLeft;



        if ( curFrame == 0 )
        {
            computeStereoMatches(keysLeft, prevLeftPnts);
            // prevLeftPnts.clone(keysLeft);
            addMapPnts(keysLeft);
            setPreLImage();
            setPreRImage();
            continue;
        }

        // add the projected mappoint here that are in frame 
        // addStereoPnts();

        computeOpticalLeft(prevLeftPnts);

        // here check size of predKeysPoints and start from end of new keypoints - size (it is the first predKeyPoint)
        computeStereoMatches(keysLeft, prevLeftPnts);

        // getNewMatchedPoints(keysLeft, prevLeftPnts);
        // optimization goes here
        optimizePoseCeres(prevLeftPnts, keysLeft);

        addMapPnts(keysLeft);

        // prevLeftPnts.clear();
        const size_t s {prevLeftPnts.keyPoints.size()};
        const size_t s4 {prevLeftPnts.estimatedDepth.size()};
        const size_t s5 {prevLeftPnts.predKeyPoints.size()};
        const size_t s6 {prevLeftPnts.mapPointIdx.size()};
        const size_t s7 {prevLeftPnts.trackCnt.size()};
        const size_t s2 {keysLeft.keyPoints.size()};
        const size_t s3 {keysLeft.keyPoints.size()};


        setPreLImage();
        setPreRImage();

        // zedPtr->addKeyFrame = true;
    //     setLRImages(curFrame);

    // //     Eigen::Vector4d p(0,0,0,1);
    // // map->addMapPoint(p);

    // // Eigen::Vector4d p2(500,432,5234,3211);
    // // map->addMapPoint(p2);

    //     findStereoFeatures(lIm.im, rIm.im, pnts);

    //     triangulate3DPoints(pnts);

    //     if ( curFrame == 0 )
    //     {
            // setPreviousValuesIni();
    //         continue;
    //     }


    //     estimatePoseN();

    //     setPreviousValues();

        // calcOptical(pnts, true);
        // calcOptical(prePnts, false);

        // solvePnPIni(prePnts, Rvec, tvec, false);
        // solvePnPIni(pnts, Rvec, tvec, true);




        // if ( curFrame == 1 )
        //     opticalFlow();
        // else
        //     opticalFlowPredict();
        // getPoseCeresNew();

        // setPreTrial();

        // Logging("ustereo", uStereo,3);
        // Logging("umono", uMono,3);
    }
    datafile.close();
}

void FeatureTracker::updateMapPoints(TrackedKeys& prevLeftKeys)
{
    for (size_t i{0}, end {prevLeftKeys.keyPoints.size()}; i < end; i++)
    {

        if ( prevLeftKeys.mapPointIdx[i] >= 0)
        {
            if ( prevLeftKeys.estimatedDepth[i] > 0 )
            {
                const double zp = (double)prevLeftKeys.estimatedDepth[i];
                const double xp = (double)(((double)prevLeftKeys.keyPoints[i].pt.x-cx)*zp/fx);
                const double yp = (double)(((double)prevLeftKeys.keyPoints[i].pt.y-cy)*zp/fy);
                Eigen::Vector4d p(xp, yp, zp, 1);
                p = zedPtr->cameraPose.pose * p;

                Eigen::Vector4d mp = map->mapPoints[prevLeftKeys.mapPointIdx[i]]->getWordPose4d();
                p(0) = (p(0) + mp(0)) / 2;
                p(1) = (p(1) + mp(1)) / 2;
                p(2) = (p(2) + mp(2)) / 2;
                map->mapPoints[prevLeftKeys.mapPointIdx[i]]->updateMapPoint(p, prevLeftKeys.Desc.row(i), prevLeftKeys.keyPoints[i]);
            }
        }
        else if ( prevLeftKeys.close[i] )
        {
            // if ( curFrame == 0 || prevLeftKeys.trackCnt[i] > 2 )
            // {
                const double zp = (double)prevLeftKeys.estimatedDepth[i];
                const double xp = (double)(((double)prevLeftKeys.keyPoints[i].pt.x-cx)*zp/fx);
                const double yp = (double)(((double)prevLeftKeys.keyPoints[i].pt.y-cy)*zp/fy);
                Eigen::Vector4d p(xp, yp, zp, 1);
                p = zedPtr->cameraPose.pose * p;
                prevLeftKeys.mapPointIdx[i] = map->pIdx;
                map->addMapPoint(p, prevLeftKeys.Desc.row(i), prevLeftKeys.keyPoints[i], prevLeftKeys.close[i]);
            // }
        }
        // else if ( prevLeftKeys.close[i] )
        // {
        //     const double zp = (double)prevLeftKeys.estimatedDepth[i];
        //     const double xp = (double)(((double)prevLeftKeys.keyPoints[i].pt.x-cx)*zp/fx);
        //     const double yp = (double)(((double)prevLeftKeys.keyPoints[i].pt.y-cy)*zp/fy);
        //     Eigen::Vector4d p(xp, yp, zp, 1);
        //     p = zedPtr->cameraPose.pose * p;
        //     prevLeftPnts.mapPointIdx[i] = map->pIdx;
        //     map->addMapPoint(p, prevLeftKeys.Desc.row(i), prevLeftKeys.keyPoints[i], prevLeftKeys.close[i]);
        //     // Here check if another point is close to see if that point is 3d
            
        // }
    }
    map->addKeyFrame(zedPtr->cameraPose.pose);
}

void FeatureTracker::predictORBPoints(TrackedKeys& prevLeftKeys)
{
    const size_t prevS { prevLeftKeys.keyPoints.size()};
    prevLeftKeys.inliers = std::vector<bool>(prevS, true);
    prevLeftKeys.hasPrediction = std::vector<bool>(prevS, false);
    prevLeftKeys.predKeyPoints = prevLeftKeys.keyPoints;
    
    if ( curFrame == 1 )
        return;

    for ( size_t i{0}; i < prevS; i++)
    {
        if ( prevLeftKeys.mapPointIdx[i] >= 0 )
        {
            MapPoint* mp = map->mapPoints[prevLeftKeys.mapPointIdx[i]];
            cv::Point2f p2f;
            if ( getPredInFrame(predNPoseInv, mp, p2f))
            {
                prevLeftKeys.predKeyPoints[i].pt = p2f;
                prevLeftKeys.hasPrediction[i] = true;
            }
            else
                prevLeftKeys.inliers[i] = false;
        }
    }
}

void FeatureTracker::reduceTrackedKeys(TrackedKeys& leftKeys, std::vector<bool>& inliers)
{
    int j {0};
    for (int i = 0; i < int(leftKeys.keyPoints.size()); i++)
    {
        if (inliers[i])
        {
            leftKeys.keyPoints[j] = leftKeys.keyPoints[i];
            leftKeys.estimatedDepth[j] = leftKeys.estimatedDepth[i];
            leftKeys.mapPointIdx[j] = leftKeys.mapPointIdx[i];
            leftKeys.matchedIdxs[j] = leftKeys.matchedIdxs[i];
            leftKeys.close[j] = leftKeys.close[i];
            leftKeys.trackCnt[j] = leftKeys.trackCnt[i];
            j++;
        }

    }
    leftKeys.keyPoints.resize(j);
    leftKeys.estimatedDepth.resize(j);
    leftKeys.mapPointIdx.resize(j);
    leftKeys.matchedIdxs.resize(j);
    leftKeys.close.resize(j);
    leftKeys.trackCnt.resize(j);
}

void FeatureTracker::reduceTrackedKeysMatches(TrackedKeys& prevLeftKeys, TrackedKeys& leftKeys)
{
    const size_t end{prevLeftKeys.keyPoints.size()};
    const size_t endNew {leftKeys.keyPoints.size()};

    std::vector<bool>inliers(end,true);
    std::vector<bool>inliersNew(endNew,true);
    int descIdx {0};
    cv::Mat desc = cv::Mat(endNew, 32, CV_8U);

    for ( size_t i{0}; i < end; i++)
    {
        if ( prevLeftKeys.matchedIdxs[i] < 0)
        {
            inliers[i] = false;
        }
        else
        {
            prevLeftKeys.keyPoints[i] = leftKeys.keyPoints[prevLeftKeys.matchedIdxs[i]];
            prevLeftKeys.estimatedDepth[i] = leftKeys.estimatedDepth[prevLeftKeys.matchedIdxs[i]];
            prevLeftKeys.close[i] = leftKeys.close[prevLeftKeys.matchedIdxs[i]];
            leftKeys.Desc.row(prevLeftKeys.matchedIdxs[i]).copyTo(desc.row(descIdx));
                descIdx++;


            prevLeftKeys.trackCnt[i] ++;
            inliersNew[prevLeftKeys.matchedIdxs[i]] = false;
        }
    }
    reduceTrackedKeys(prevLeftKeys, inliers);
    // reduceTrackedKeys(leftKeys, inliersNew);

    for ( size_t i {0}; i < endNew; i++)
    {
        if ( inliersNew[i])
        {
            prevLeftKeys.keyPoints.emplace_back(leftKeys.keyPoints[i]);
            prevLeftKeys.estimatedDepth.emplace_back(leftKeys.estimatedDepth[i]);
            prevLeftKeys.close.emplace_back(leftKeys.close[i]);
            leftKeys.Desc.row(i).copyTo(desc.row(descIdx));
                descIdx++;
        }
    }
    prevLeftKeys.Desc = desc.clone();
    prevLeftKeys.mapPointIdx.resize(endNew, -1);
    prevLeftKeys.trackCnt.resize(endNew, 0);

}

void  FeatureTracker::cloneTrackedKeys(TrackedKeys& prevLeftKeys, TrackedKeys& leftKeys)
{
    prevLeftKeys.keyPoints = leftKeys.keyPoints;
    prevLeftKeys.Desc = leftKeys.Desc.clone();
    // prevLeftKeys.mapPointIdx = leftKeys.mapPointIdx;
    prevLeftKeys.estimatedDepth = leftKeys.estimatedDepth;
    prevLeftKeys.close = leftKeys.close;
    // prevLeftKeys.trackCnt = leftKeys.trackCnt;
    const size_t prevE{prevLeftKeys.mapPointIdx.size()};
    std::vector<int>newMapPointIdx(prevE, -1);
    std::vector<int>newTrackCnt(prevE, 0);
    for (size_t i{0}; i < prevE; i++)
    {
        if (prevLeftKeys.matchedIdxs[i] >= 0)
        {
            newMapPointIdx[prevLeftKeys.matchedIdxs[i]] = prevLeftKeys.mapPointIdx[i];
            newTrackCnt[prevLeftKeys.matchedIdxs[i]] = prevLeftKeys.trackCnt[i] + 1;

        }
    }
    prevLeftKeys.mapPointIdx = newMapPointIdx;
    prevLeftKeys.trackCnt = newTrackCnt;
}

void FeatureTracker::Track3(const int frames)
{
    for (curFrame = 0; curFrame < frames; curFrame++)
    {
        zedPtr->addKeyFrame = true;
        setLRImages(curFrame);

        TrackedKeys keysLeft;



        if ( curFrame == 0 )
        {
            extractORBStereoMatch(lIm.im, rIm.im, keysLeft);

            prevLeftPnts.clone(keysLeft);
            updateMapPoints(prevLeftPnts);
            // addMapPnts(keysLeft);
            setPreLImage();
            setPreRImage();
            continue;
        }
        extractORBStereoMatch(lIm.im, rIm.im, keysLeft);
        predictORBPoints(prevLeftPnts);
        fm.matchORBPoints(prevLeftPnts, keysLeft);
        // std::vector<cv::Point2f> ppnts, pntsn;
        // cv::KeyPoint::convert(prevLeftPnts.keyPoints, ppnts);
        // cv::KeyPoint::convert(keysLeft.keyPoints, pntsn);
        // prevLeftPnts.inliers.clear();
        // cv::findFundamentalMat(ppnts, pntsn,prevLeftPnts.inliers2);
        drawLeftMatches("left Matches", pLIm.rIm, prevLeftPnts, keysLeft);
        cv::waitKey(1);
        optimizePoseCeres(prevLeftPnts, keysLeft);

        cloneTrackedKeys(prevLeftPnts, keysLeft);
        updateMapPoints(prevLeftPnts);


        setPreLImage();
        setPreRImage();

    }
    datafile.close();
}

void FeatureTracker::initializeMap(TrackedKeys& keysLeft)
{
    KeyFrame* kF = new KeyFrame(zedPtr->cameraPose.pose, lIm.im, lIm.rIm,map->kIdx, curFrame);
    kF->scaleFactor = fe.scalePyramid;
    kF->nScaleLev = fe.nLevels;
    kF->keyF = true;
    kF->fixed = true;
    // KeyFrame* kF = new KeyFrame(zedPtr->cameraPose.pose, map->kIdx);
    // kF->unMatchedF.resize(keysLeft.keyPoints.size(), true);
    kF->unMatchedF.resize(keysLeft.keyPoints.size(), -1);
    kF->localMapPoints.resize(keysLeft.keyPoints.size(), nullptr);
    // kF->localMapPoints.reserve(activeMapPoints.size());
    activeMapPoints.reserve(keysLeft.keyPoints.size());
    activeKeyFrames.reserve(1000);
    mPPerKeyFrame.reserve(1000);
    // std::lock_guard<std::mutex> lock(map->mapMutex);
    int trckedKeys {0};
    for (size_t i{0}, end{keysLeft.keyPoints.size()}; i < end; i++)
    {
        if ( keysLeft.estimatedDepth[i] > 0 )
        {
            const double zp = (double)keysLeft.estimatedDepth[i];
            const double xp = (double)(((double)keysLeft.keyPoints[i].pt.x-cx)*zp/fx);
            const double yp = (double)(((double)keysLeft.keyPoints[i].pt.y-cy)*zp/fy);
            Eigen::Vector4d p(xp, yp, zp, 1);
            p = zedPtr->cameraPose.pose * p;
            keysLeft.mapPointIdx[i] = map->pIdx;
            MapPoint* mp = new MapPoint(p, keysLeft.Desc.row(i), keysLeft.keyPoints[i], keysLeft.close[i], map->kIdx, map->pIdx);
            mp->kFWithFIdx.insert(std::pair<KeyFrame*, size_t>(kF, i));
            map->addMapPoint(mp);
            mp->desc.push_back(keysLeft.rightDesc.row(keysLeft.rightIdxs[i]));
            mp->lastObsKF = kF;
            mp->lastObsL = keysLeft.keyPoints[i];
            activeMapPoints.emplace_back(mp);
            kF->localMapPoints[i] = mp;
            kF->unMatchedF[i] = mp->kdx;
            trckedKeys++;
            // kF->localMapPoints.emplace_back(mp);
        }
    }
    lastKFTrackedNumb = trckedKeys;
    mPPerKeyFrame.push_back(activeMapPoints.size());
    kF->keys.getKeys(keysLeft);
    activeKeyFrames.insert(activeKeyFrames.begin(),kF);
    map->addKeyFrame(kF);
    allFrames.emplace_back(kF);
    lastKFPose = zedPtr->cameraPose.pose;
    lastKFPoseInv = lastKFPose.inverse();
    lastKFImage = lIm.im.clone();
}

void FeatureTracker::initializeMapR(TrackedKeys& keysLeft)
{
    KeyFrame* kF = new KeyFrame(zedPtr->cameraPose.pose, lIm.im, lIm.rIm,map->kIdx, curFrame);
    kF->scaleFactor = fe.scalePyramid;
    kF->sigmaFactor = fe.sigmaFactor;
    kF->InvSigmaFactor = fe.InvSigmaFactor;
    kF->nScaleLev = fe.nLevels;
    kF->logScale = log(fe.imScale);
    kF->keyF = true;
    kF->fixed = true;
    // KeyFrame* kF = new KeyFrame(zedPtr->cameraPose.pose, map->kIdx);
    // kF->unMatchedF.resize(keysLeft.keyPoints.size(), true);
    kF->unMatchedF.resize(keysLeft.keyPoints.size(), -1);
    kF->unMatchedFR.resize(keysLeft.rightKeyPoints.size(), -1);
    kF->localMapPoints.resize(keysLeft.keyPoints.size(), nullptr);
    kF->localMapPointsR.resize(keysLeft.rightKeyPoints.size(), nullptr);
    // kF->localMapPoints.reserve(activeMapPoints.size());
    activeMapPoints.reserve(keysLeft.keyPoints.size());
    activeKeyFrames.reserve(1000);
    mPPerKeyFrame.reserve(1000);
    // std::lock_guard<std::mutex> lock(map->mapMutex);
    kF->keys.getKeys(keysLeft);
    int trckedKeys {0};
    for (size_t i{0}, end{keysLeft.keyPoints.size()}; i < end; i++)
    {
        if ( keysLeft.estimatedDepth[i] > 0 )
        {
            const int rIdx {keysLeft.rightIdxs[i]};
            const double zp = (double)keysLeft.estimatedDepth[i];
            const double xp = (double)(((double)keysLeft.keyPoints[i].pt.x-cx)*zp/fx);
            const double yp = (double)(((double)keysLeft.keyPoints[i].pt.y-cy)*zp/fy);
            Eigen::Vector4d p(xp, yp, zp, 1);
            p = zedPtr->cameraPose.pose * p;
            MapPoint* mp = new MapPoint(p, keysLeft.Desc.row(i), keysLeft.keyPoints[i], keysLeft.close[i], map->kIdx, map->pIdx);
            // mp->kFWithFIdx.insert(std::pair<KeyFrame*, size_t>(kF, i));
            mp->kFMatches.insert(std::pair<KeyFrame*, std::pair<int,int>>(kF, std::pair<int,int>(i,rIdx)));
            mp->calcDescriptor();
            map->addMapPoint(mp);
            // mp->desc.push_back(keysLeft.rightDesc.row(keysLeft.rightIdxs[i]));
            mp->lastObsKF = kF;
            mp->lastObsL = keysLeft.keyPoints[i];
            mp->scaleLevelL = keysLeft.keyPoints[i].octave;
            mp->lastObsR = keysLeft.rightKeyPoints[rIdx];
            mp->scaleLevelR = keysLeft.rightKeyPoints[rIdx].octave;
            mp->update(kF);
            activeMapPoints.emplace_back(mp);
            kF->localMapPoints[i] = mp;
            kF->localMapPointsR[rIdx] = mp;
            kF->unMatchedF[i] = mp->kdx;
            kF->unMatchedFR[rIdx] = mp->kdx;
            trckedKeys++;
            // kF->localMapPoints.emplace_back(mp);
        }
    }
    lastKFTrackedNumb = trckedKeys;
    activeKeyFrames.insert(activeKeyFrames.begin(),kF);
    map->addKeyFrame(kF);
    allFrames.emplace_back(kF);
    lastKFPose = zedPtr->cameraPose.pose;
    lastKFPoseInv = lastKFPose.inverse();
    lastKFImage = lIm.im.clone();
}

void FeatureTracker::initializeMapRB(TrackedKeys& keysLeft, TrackedKeys& keysLeftB)
{
    KeyFrame* kF = new KeyFrame(zedPtr->cameraPose.pose, lIm.im, lIm.rIm,map->kIdx, curFrame);
    kF->setBackPose(kF->pose.pose * zedPtr->TCamToCam);
    kF->scaleFactor = fe.scalePyramid;
    kF->sigmaFactor = fe.sigmaFactor;
    kF->InvSigmaFactor = fe.InvSigmaFactor;
    kF->nScaleLev = fe.nLevels;
    kF->logScale = log(fe.imScale);
    kF->keyF = true;
    kF->fixed = true;
    // KeyFrame* kF = new KeyFrame(zedPtr->cameraPose.pose, map->kIdx);
    // kF->unMatchedF.resize(keysLeft.keyPoints.size(), true);
    kF->unMatchedF.resize(keysLeft.keyPoints.size(), -1);
    kF->unMatchedFR.resize(keysLeft.rightKeyPoints.size(), -1);
    kF->localMapPoints.resize(keysLeft.keyPoints.size(), nullptr);
    kF->localMapPointsR.resize(keysLeft.rightKeyPoints.size(), nullptr);

    kF->unMatchedFB.resize(keysLeftB.keyPoints.size(), -1);
    kF->unMatchedFRB.resize(keysLeftB.rightKeyPoints.size(), -1);
    kF->localMapPointsB.resize(keysLeftB.keyPoints.size(), nullptr);
    kF->localMapPointsRB.resize(keysLeftB.rightKeyPoints.size(), nullptr);
    // kF->localMapPoints.reserve(activeMapPoints.size());
    activeMapPoints.reserve(keysLeft.keyPoints.size());
    activeKeyFrames.reserve(1000);
    mPPerKeyFrame.reserve(1000);
    // std::lock_guard<std::mutex> lock(map->mapMutex);
    kF->keys.getKeys(keysLeft);
    kF->keysB.getKeys(keysLeftB);
    int trckedKeys {0};
    for (size_t i{0}, end{keysLeft.keyPoints.size()}; i < end; i++)
    {
        if ( keysLeft.estimatedDepth[i] > 0 )
        {
            const int rIdx {keysLeft.rightIdxs[i]};
            const double zp = (double)keysLeft.estimatedDepth[i];
            const double xp = (double)(((double)keysLeft.keyPoints[i].pt.x-cx)*zp/fx);
            const double yp = (double)(((double)keysLeft.keyPoints[i].pt.y-cy)*zp/fy);
            Eigen::Vector4d p(xp, yp, zp, 1);
            p = zedPtr->cameraPose.pose * p;
            MapPoint* mp = new MapPoint(p, keysLeft.Desc.row(i), keysLeft.keyPoints[i], keysLeft.close[i], map->kIdx, map->pIdx);
            mp->kFMatches.insert(std::pair<KeyFrame*, std::pair<int,int>>(kF, std::pair<int,int>(i,rIdx)));
            mp->calcDescriptor();
            map->addMapPoint(mp);
            mp->lastObsKF = kF;
            mp->lastObsL = keysLeft.keyPoints[i];
            mp->scaleLevelL = keysLeft.keyPoints[i].octave;
            mp->lastObsR = keysLeft.rightKeyPoints[rIdx];
            mp->scaleLevelR = keysLeft.rightKeyPoints[rIdx].octave;
            mp->update(kF);
            activeMapPoints.emplace_back(mp);
            kF->localMapPoints[i] = mp;
            kF->localMapPointsR[rIdx] = mp;
            kF->unMatchedF[i] = mp->kdx;
            kF->unMatchedFR[rIdx] = mp->kdx;
            trckedKeys++;
        }
    }
    for (size_t i{0}, end{keysLeftB.keyPoints.size()}; i < end; i++)
    {
        if ( keysLeftB.estimatedDepth[i] > 0 )
        {
            const int rIdx {keysLeftB.rightIdxs[i]};
            const double zp = (double)keysLeftB.estimatedDepth[i];
            const double xp = (double)(((double)keysLeftB.keyPoints[i].pt.x-cx)*zp/fx);
            const double yp = (double)(((double)keysLeftB.keyPoints[i].pt.y-cy)*zp/fy);
            Eigen::Vector4d p(xp, yp, zp, 1);
            p = zedPtrB->cameraPose.pose * p;
            MapPoint* mp = new MapPoint(p, keysLeftB.Desc.row(i), keysLeftB.keyPoints[i], keysLeftB.close[i], map->kIdx, map->pIdx);
            mp->kFMatchesB.insert(std::pair<KeyFrame*, std::pair<int,int>>(kF, std::pair<int,int>(i,rIdx)));
            mp->calcDescriptor();
            map->addMapPoint(mp);
            mp->lastObsKF = kF;
            mp->lastObsL = keysLeftB.keyPoints[i];
            mp->scaleLevelL = keysLeftB.keyPoints[i].octave;
            mp->lastObsR = keysLeftB.rightKeyPoints[rIdx];
            mp->scaleLevelR = keysLeftB.rightKeyPoints[rIdx].octave;
            mp->update(kF, true);
            activeMapPointsB.emplace_back(mp);
            kF->localMapPointsB[i] = mp;
            kF->localMapPointsRB[rIdx] = mp;
            kF->unMatchedFB[i] = mp->kdx;
            kF->unMatchedFRB[rIdx] = mp->kdx;
            trckedKeys++;
        }
    }
    lastKFTrackedNumb = trckedKeys;
    activeKeyFrames.insert(activeKeyFrames.begin(),kF);
    map->addKeyFrame(kF);
    allFrames.emplace_back(kF);
    lastKFPose = zedPtr->cameraPose.pose;
    lastKFPoseInv = lastKFPose.inverse();
    lastKFImage = lIm.im.clone();
}

void FeatureTracker::worldToImg(std::vector<MapPoint*>& MapPointsVec, std::vector<cv::KeyPoint>& projectedPoints)
{
    projectedPoints.resize(MapPointsVec.size());
    for ( size_t i {0}, end{MapPointsVec.size()}; i < end; i++)
    {
        Eigen::Vector4d p4d =  MapPointsVec[i]->getWordPose4d();

        if (p4d(2) <= 0.0f )
        {
            MapPointsVec[i]->SetInFrame(false);
        }

        const double invfx = 1.0f/fx;
        const double invfy = 1.0f/fy;


        double u {fx*p4d(0)/p4d(2) + cx};
        double v {fy*p4d(1)/p4d(2) + cy};

        const int h = zedPtr->mHeight;
        const int w = zedPtr->mWidth;

        if ( u < 0 )
            u = 0.1;
        if ( v < 0 )
            v = 0.1;
        if ( u > w )
            u = w - 1;
        if ( v > h )
            v = h - 1;
        projectedPoints[i].pt = cv::Point2f((float)u, (float)v);
    }
}

void FeatureTracker::worldToImg(std::vector<MapPoint*>& MapPointsVec, std::vector<cv::KeyPoint>& projectedPoints, const Eigen::Matrix4d& currPoseInv)
{
    projectedPoints.resize(MapPointsVec.size());
    for ( size_t i {0}, end{MapPointsVec.size()}; i < end; i++)
    {
        projectedPoints[i].pt = cv::Point2f(-1, -1);
        Eigen::Vector4d p4d = currPoseInv * MapPointsVec[i]->getWordPose4d();

        if (p4d(2) <= 0.0f )
        {
            // MapPointsVec[i]->SetInFrame(false);
            continue;
        }

        const double invfx = 1.0f/fx;
        const double invfy = 1.0f/fy;


        double u {fx*p4d(0)/p4d(2) + cx};
        double v {fy*p4d(1)/p4d(2) + cy};

        const int h = zedPtr->mHeight;
        const int w = zedPtr->mWidth;

        if ( u < 0 || v < 0 || u > w || v > h )
        {
            // MapPointsVec[i]->SetInFrame(false);
            continue;
        }

        // if ( u < 0 )
        //     u = 0.1;
        // if ( v < 0 )
        //     v = 0.1;
        // if ( u > w )
        //     u = w - 1;
        // if ( v > h )
        //     v = h - 1;
        projectedPoints[i].pt = cv::Point2f((float)u, (float)v);
    }
}

void FeatureTracker::worldToImgR(std::vector<MapPoint*>& activeMapPoints, std::vector<std::pair<cv::Point2f,cv::Point2f>>& projectedPoints, const Eigen::Matrix4d& currPose)
{
    projectedPoints.reserve(activeMapPoints.size());
    Eigen::Matrix4d currPoseInvL = currPose.inverse();
    Eigen::Matrix4d currPoseInvR = (currPose * zedPtr->extrinsics).inverse();
    for ( size_t i {0}, end{activeMapPoints.size()}; i < end; i++)
    {
        Eigen::Vector4d wPos = activeMapPoints[i]->getWordPose4d();
        Eigen::Vector4d p4d = currPoseInvL * wPos;
        Eigen::Vector4d p4dR = currPoseInvR * wPos;


        double u {fx*p4d(0)/p4d(2) + cx};
        double v {fy*p4d(1)/p4d(2) + cy};

        double ur {fxr*p4dR(0)/p4dR(2) + cxr};
        double vr {fyr*p4dR(1)/p4dR(2) + cyr};


        projectedPoints.emplace_back(cv::Point2f((float)u, (float)v),cv::Point2f((float)ur, (float)vr));
    }
}

void FeatureTracker::calcScale(const double prevD, const double newD)
{
    double scaletrial = prevD / newD;
    int level = cvRound(scaletrial/feLeft.scalePyramid[1]);
}

void FeatureTracker::worldToImgScale(std::vector<MapPoint*>& MapPointsVec, std::vector<cv::KeyPoint>& projectedPoints, const Eigen::Matrix4d& currPoseInv, std::vector<int> scaleLevels)
{
    projectedPoints.resize(MapPointsVec.size());
    scaleLevels.resize(MapPointsVec.size(),1);
    for ( size_t i {0}, end{MapPointsVec.size()}; i < end; i++)
    {
        projectedPoints[i].pt = cv::Point2f(-1, -1);
        Eigen::Vector4d mpWp = MapPointsVec[i]->getWordPose4d();
        Eigen::Vector4d p4d = currPoseInv * mpWp;

        if (p4d(2) <= 0.0f )
        {
            // MapPointsVec[i]->SetInFrame(false);
            continue;
        }

        const double invfx = 1.0f/fx;
        const double invfy = 1.0f/fy;


        double u {fx*p4d(0)/p4d(2) + cx};
        double v {fy*p4d(1)/p4d(2) + cy};

        const int h = zedPtr->mHeight;
        const int w = zedPtr->mWidth;

        if ( u < 0 || v < 0 || u > w || v > h )
        {
            // MapPointsVec[i]->SetInFrame(false);
            continue;
        }

        // if ( u < 0 )
        //     u = 0.1;
        // if ( v < 0 )
        //     v = 0.1;
        // if ( u > w )
        //     u = w - 1;
        // if ( v > h )
        //     v = h - 1;
        double scaletrial = mpWp(2) / p4d(2);
        scaleLevels[i] = cvRound(scaletrial/feLeft.scalePyramid[1]);
        projectedPoints[i].pt = cv::Point2f((float)u, (float)v);
    }
}

void FeatureTracker::worldToImgScaleR(std::vector<MapPoint*>& activeMapPoints, std::vector<cv::KeyPoint>& projectedPoints, const Eigen::Matrix4d& currPose, const Eigen::Matrix4d& predPose, std::vector<int> scaleLevels)
{
    projectedPoints.resize(activeMapPoints.size());
    scaleLevels.resize(activeMapPoints.size(),1);
    for ( size_t i {0}, end{activeMapPoints.size()}; i < end; i++)
    {
        projectedPoints[i].pt = cv::Point2f(-1, -1);
        Eigen::Vector4d mpWp = activeMapPoints[i]->getWordPose4d();
        Eigen::Vector4d p4d = currPose * mpWp;

        if (p4d(2) <= 0.0f )
        {
            // MapPointsVec[i]->SetInFrame(false);
            continue;
        }

        const double invfx = 1.0f/fx;
        const double invfy = 1.0f/fy;


        double u {fx*p4d(0)/p4d(2) + cx};
        double v {fy*p4d(1)/p4d(2) + cy};

        const int h = zedPtr->mHeight;
        const int w = zedPtr->mWidth;

        if ( u < 0 || v < 0 || u > w || v > h )
        {
            // MapPointsVec[i]->SetInFrame(false);
            continue;
        }

        // if ( u < 0 )
        //     u = 0.1;
        // if ( v < 0 )
        //     v = 0.1;
        // if ( u > w )
        //     u = w - 1;
        // if ( v > h )
        //     v = h - 1;
        double scaletrial = mpWp(2) / p4d(2);
        scaleLevels[i] = cvRound(scaletrial/feLeft.scalePyramid[1]);
        projectedPoints[i].pt = cv::Point2f((float)u, (float)v);
    }
}

void FeatureTracker::worldToImgAng(std::vector<MapPoint*>& MapPointsVec, std::vector<float>& mapAngles, const Eigen::Matrix4d& currPoseInv, std::vector<cv::KeyPoint>& prevKeyPos, std::vector<cv::KeyPoint>& projectedPoints)
{
    projectedPoints.resize(MapPointsVec.size());
    mapAngles.resize(MapPointsVec.size(), -5.0);
    for ( size_t i {0}, end{MapPointsVec.size()}; i < end; i++)
    {
        Eigen::Vector4d p4dmap = MapPointsVec[i]->getWordPose4d();
        p4dmap = p4dmap/(10*p4dmap(2));
        // Logging("p4d bef", p4dmap,3);
        p4dmap(3) = 1;
        p4dmap = currPoseInv * p4dmap;
        // Logging("p4d after", p4dmap,3);

        double umap {fx*p4dmap(0)/p4dmap(2) + cx};
        double vmap {fy*p4dmap(1)/p4dmap(2) + cy};
        projectedPoints[i].pt = cv::Point2f((float)umap, (float)vmap);
        mapAngles[i] = atan2((float)vmap - prevKeyPos[i].pt.y, (float)umap - prevKeyPos[i].pt.x);

    }
}

void FeatureTracker::getPoints3dFromMapPoints(std::vector<MapPoint*>& activeMapPoints, TrackedKeys& keysLeft, std::vector<cv::Point3d>& points3d, std::vector<cv::Point2d>& points2d, std::vector<int>& matchedIdxsN)
{
    points2d.reserve(matchedIdxsN.size());
    points3d.reserve(matchedIdxsN.size());
    for ( size_t i{0}, end{matchedIdxsN.size()}; i < end; i++)
    {
        if ( matchedIdxsN[i] >= 0 )
        {
            points2d.emplace_back((double)keysLeft.keyPoints[i].pt.x, (double)keysLeft.keyPoints[i].pt.y);
            Eigen::Vector3d p3d = activeMapPoints[matchedIdxsN[i]]->getWordPose3d();
            points3d.emplace_back(p3d(0), p3d(1), p3d(2));
        }
    }
}

void FeatureTracker::removePnPOut(std::vector<int>& idxs, std::vector<int>& matchedIdxsN, std::vector<int>& matchedIdxsB)
{
    int idxIdx {0};
    int count {0};
    for ( size_t i{0}, end{matchedIdxsN.size()}; i < end; i++)
    {
        if ( matchedIdxsN[i] >= 0 )
        {
            if (idxs[idxIdx] == count )
                idxIdx++;
            else
            {
                matchedIdxsB[matchedIdxsN[i]] = -1;
                matchedIdxsN[i] = -1;
            }
            count ++;
        }
    }
}

bool FeatureTracker::check2dError(Eigen::Vector4d& p4d, const cv::Point2f& obs, const double thres, const float weight)
{
    const double invZ = 1.0f/p4d(2);

    const double u {fx*p4d(0)*invZ + cx};
    const double v {fy*p4d(1)*invZ + cy};

    const double errorU = ((double)obs.x - u);
    const double errorV = ((double)obs.y - v);

    const double error = (errorU * errorU + errorV * errorV) * weight;
    if (error > thres)
        return true;
    else
        return false;
}

bool FeatureTracker::check2dErrorB(const Zed_Camera* zedCam, Eigen::Vector4d& p4d, const cv::Point2f& obs, const double thres, const float weight)
{
    const double invZ = 1.0f/p4d(2);

    const double fx = zedCam->cameraLeft.fx;
    const double fy = zedCam->cameraLeft.fy;
    const double cx = zedCam->cameraLeft.cx;
    const double cy = zedCam->cameraLeft.cy;

    const double u {fx*p4d(0)*invZ + cx};
    const double v {fy*p4d(1)*invZ + cy};

    const double errorU = weight * ((double)obs.x - u);
    const double errorV = weight * ((double)obs.y - v);

    const double error = (errorU * errorU + errorV * errorV) * weight;
    if (error > thres)
        return true;
    else
        return false;
}

bool FeatureTracker::check3dError(const Eigen::Vector4d& p4d, const Eigen::Vector4d& obs, const double thres, const float weight)
{
    const double errorX = weight * ((double)obs(0) - p4d(0));
    const double errorY = weight * ((double)obs(1) - p4d(1));
    const double errorZ = weight * ((double)obs(2) - p4d(2));

    const double error = (errorX * errorX + errorY * errorY + errorZ * errorZ);
    if (error > thres)
        return true;
    else
        return false;
}

int FeatureTracker::OutliersReprojErr(const Eigen::Matrix4d& estimatedP, std::vector<MapPoint*>& activeMapPoints, TrackedKeys& keysLeft, std::vector<int>& matchedIdxsB, const double thres, const std::vector<float>& weights, int& nInliers)
{
    // std::vector<cv::Point2f>out2d;
    int nStereo = 0;
    for (size_t i {0}, end{matchedIdxsB.size()}; i < end; i++)
    {
        if ( matchedIdxsB[i] < 0 )
            continue;
        MapPoint* mp = activeMapPoints[i];
        if ( !mp->GetInFrame() )
        {
            // mp->SetIsOutlier(true);
            continue;
        }
        cv::Point2f out;
        Eigen::Vector4d p4d = mp->getWordPose4d();
        p4d = estimatedP * p4d;
        const int nIdx {matchedIdxsB[i]};
        if ( mp->close && keysLeft.close[nIdx] )
        {
            Eigen::Vector4d obs;
            get3dFromKey(obs, keysLeft.keyPoints[nIdx], keysLeft.estimatedDepth[nIdx]);
            bool outlier = check3dError(p4d, obs, 0.01 * thres, 1.0);
            mp->SetIsOutlier(outlier);
            if ( !outlier )
            {
                nInliers++;
                nStereo++;
            }

        }
        else
        {
            bool outlier = check2dError(p4d, keysLeft.keyPoints[nIdx].pt, thres, 1.0);
            mp->SetIsOutlier(outlier);
            if ( !outlier )
            {
                nInliers++;
                if ( keysLeft.close[nIdx] )
                    nStereo++;
            }

        }
    }

// #if PROJECTIM
//     // drawOptical("new", pLIm.rIm,pnts.left, pnts.newPnts);
//     drawOptical("reproj", pLIm.rIm,obs, out2d);
//     cv::waitKey(waitTrials);
// #endif
    return nStereo;
}

int FeatureTracker::findOutliers(const Eigen::Matrix4d& estimatedP, std::vector<MapPoint*>& activeMapPoints, TrackedKeys& keysLeft, std::vector<int>& matchedIdxsB, const double thres, std::vector<bool>& MPsOutliers, const std::vector<float>& weights, int& nInliers)
{
    // std::vector<cv::Point2f>out2d;
    int nStereo = 0;
    for (size_t i {0}, end{matchedIdxsB.size()}; i < end; i++)
    {
        if ( matchedIdxsB[i] < 0 )
            continue;
        MapPoint* mp = activeMapPoints[i];
        if ( !mp->GetInFrame() )
        {
            // mp->SetIsOutlier(true);
            continue;
        }
        cv::Point2f out;
        Eigen::Vector4d p4d = mp->getWordPose4d();
        p4d = estimatedP * p4d;
        const int nIdx {matchedIdxsB[i]};
        // if ( mp->close && keysLeft.close[nIdx] )
        // {
        //     Eigen::Vector4d obs;
        //     get3dFromKey(obs, keysLeft.keyPoints[nIdx], keysLeft.estimatedDepth[nIdx]);
        //     bool outlier = check3dError(p4d, obs, 0.01 * thres, 1.0);
        //     MPsOutliers[i] = outlier;
        //     if ( !outlier )
        //     {
        //         nInliers++;
        //         nStereo++;
        //     }

        // }
        // else
        // {
            bool outlier = check2dError(p4d, keysLeft.keyPoints[nIdx].pt, thres, 1.0);
            MPsOutliers[i] = outlier;
            if ( !outlier )
            {
                nInliers++;
                if ( mp->close && keysLeft.close[nIdx] )
                    nStereo++;
            }

        // }
    }

// #if PROJECTIM
//     // drawOptical("new", pLIm.rIm,pnts.left, pnts.newPnts);
//     drawOptical("reproj", pLIm.rIm,obs, out2d);
//     cv::waitKey(waitTrials);
// #endif
    return nStereo;
}

int FeatureTracker::findOutliersR(const Eigen::Matrix4d& estimPose, std::vector<MapPoint*>& activeMapPoints, TrackedKeys& keysLeft, std::vector<std::pair<int,int>>& matchesIdxs, const double thres, std::vector<bool>& MPsOutliers, const std::vector<float>& weights, int& nInliers)
{
    // std::vector<cv::Point2f>out2d;
    const Eigen::Matrix4d estimPoseInv = estimPose.inverse();
    const Eigen::Matrix4d toCameraR = (estimPoseInv * zedPtr->extrinsics).inverse();
    int nStereo = 0;
    for (size_t i {0}, end{matchesIdxs.size()}; i < end; i++)
    {
        const std::pair<int,int>& keyPos = matchesIdxs[i];
        MapPoint* mp = activeMapPoints[i];
        if ( !mp )
            continue;
        Eigen::Vector4d p4d = mp->getWordPose4d();
        int nIdx;
        cv::Point2f obs;
        bool right {false};
        if ( keyPos.first >= 0 )
        {
            if  ( !mp->inFrame )
                continue;
            p4d = estimPose * p4d;
            nIdx = keyPos.first;
            obs = keysLeft.keyPoints[nIdx].pt;
        }
        else if ( keyPos.second >= 0 )
        {
            if  ( !mp->inFrameR )
                continue;
            right = true;
            p4d = toCameraR * p4d;
            nIdx = keyPos.second;
            obs = keysLeft.rightKeyPoints[nIdx].pt;
        }
        else
            continue;

        bool outlier = check2dError(p4d, obs, thres, 1.0);
        MPsOutliers[i] = outlier;
        if ( !outlier )
        {
            nInliers++;
            if ( p4d(2) < zedPtr->mBaseline * fm.closeNumber && keysLeft.close[nIdx] && !right )
                nStereo++;
        }
    }

// #if PROJECTIM
//     // drawOptical("new", pLIm.rIm,pnts.left, pnts.newPnts);
//     drawOptical("reproj", pLIm.rIm,obs, out2d);
//     cv::waitKey(waitTrials);
// #endif
    return nStereo;
}

int FeatureTracker::findOutliersRB(const Zed_Camera* zedCam, const Eigen::Matrix4d& estimPose, std::vector<MapPoint*>& activeMapPoints, TrackedKeys& keysLeft, std::vector<std::pair<int,int>>& matchesIdxs, const double thres, std::vector<bool>& MPsOutliers, int& nInliers)
{
    // std::vector<cv::Point2f>out2d;
    const Eigen::Matrix4d estimPoseInv = estimPose.inverse();
    const Eigen::Matrix4d toCameraR = (estimPoseInv * zedCam->extrinsics).inverse();
    int nStereo = 0;
    for (size_t i {0}, end{matchesIdxs.size()}; i < end; i++)
    {
        const std::pair<int,int>& keyPos = matchesIdxs[i];
        MapPoint* mp = activeMapPoints[i];
        if ( !mp )
            continue;
        Eigen::Vector4d p4d = mp->getWordPose4d();
        int nIdx;
        cv::Point2f obs;
        bool right {false};
        if ( keyPos.first >= 0 )
        {
            if  ( !mp->inFrame )
                continue;
            p4d = estimPose * p4d;
            nIdx = keyPos.first;
            obs = keysLeft.keyPoints[nIdx].pt;
        }
        else if ( keyPos.second >= 0 )
        {
            if  ( !mp->inFrameR )
                continue;
            right = true;
            p4d = toCameraR * p4d;
            nIdx = keyPos.second;
            obs = keysLeft.rightKeyPoints[nIdx].pt;
        }
        else
            continue;

        bool outlier = check2dErrorB(zedCam, p4d, obs, thres, 1.0);
        MPsOutliers[i] = outlier;
        if ( !outlier )
        {
            nInliers++;
            if ( p4d(2) < zedCam->mBaseline * fm.closeNumber && keysLeft.close[nIdx] && !right )
                nStereo++;
        }
    }

    return nStereo;
}

std::pair<int,int> FeatureTracker::refinePose(std::vector<MapPoint*>& activeMapPoints, TrackedKeys& keysLeft, std::vector<int>& matchedIdxsB, Eigen::Matrix4d& estimPose)
{


    const size_t prevS { activeMapPoints.size()};
    const size_t newS { keysLeft.keyPoints.size()};
    const size_t startPrev {newS - prevS};
    std::vector<bool> inliers(prevS,true);
    std::vector<bool> prevInliers(prevS,true);

    std::vector<float> weights;
    // calcWeights(prePnts, weights);
    weights.resize(prevS, 1.0f);
    std::vector<double>thresholds = {15.6f,9.8f,7.815f,7.815f};
    double thresh = 7.815f;

    ceres::Problem problem;
    Eigen::Vector3d frame_tcw;
    Eigen::Quaterniond frame_qcw;
    // OutliersReprojErr(estimPose, activeMapPoints, keysLeft, matchedIdxsB, thresh, weights, nIn);
    Eigen::Matrix4d prevPose = estimPose;
    Eigen::Matrix4d frame_pose = estimPose;
    Eigen::Matrix3d frame_R;
    frame_R = frame_pose.block<3, 3>(0, 0);
    frame_tcw = frame_pose.block<3, 1>(0, 3);
    frame_qcw = Eigen::Quaterniond(frame_R);
    std::vector<cv::Point2f> found, observed;
    ceres::Manifold* quaternion_local_parameterization = new ceres::EigenQuaternionManifold;
    ceres::LossFunction* loss_function = new ceres::HuberLoss(sqrt(7.815f));
    // ceres::LossFunction* loss_function = nullptr;
    for (size_t i{0}, end{matchedIdxsB.size()}; i < end; i++)
    {
        if ( matchedIdxsB[i] < 0 )
            continue;
        MapPoint* mp = activeMapPoints[i];
        if  ( !mp->GetInFrame() || mp->GetIsOutlier() )
            continue;
        const int nIdx {matchedIdxsB[i]};
        if ( mp->close && keysLeft.close[nIdx] )
        {
            Eigen::Vector4d np4d;
            get3dFromKey(np4d, keysLeft.keyPoints[nIdx], keysLeft.estimatedDepth[nIdx]);
            Eigen::Vector3d obs(np4d(0), np4d(1),np4d(2));
            Eigen::Vector3d point = mp->getWordPose3d();
            Eigen::Vector4d point4d = mp->getWordPose4d();
            point4d = estimPose * point4d;
            const double u {fx*point4d(0)/point4d(2) + cx};
            const double v {fy*point4d(1)/point4d(2) + cy};
            found.emplace_back((float)u, (float)v);
            observed.emplace_back(keysLeft.keyPoints[nIdx].pt);
            // Logging("obs", obs,3);
            // Logging("point", point,3);
            ceres::CostFunction* costf = OptimizePoseICP::Create(K, point, obs, (double)weights[i]);
            problem.AddResidualBlock(costf, loss_function /* squared loss */,frame_tcw.data(), frame_qcw.coeffs().data());

            problem.SetManifold(frame_qcw.coeffs().data(),
                                        quaternion_local_parameterization);
        }
        else
        {
            Eigen::Vector2d obs((double)keysLeft.keyPoints[nIdx].pt.x, (double)keysLeft.keyPoints[nIdx].pt.y);
            Eigen::Vector3d point = mp->getWordPose3d();
            Eigen::Vector4d point4d = mp->getWordPose4d();
            point4d = estimPose * point4d;
            const double u {fx*point4d(0)/point4d(2) + cx};
            const double v {fy*point4d(1)/point4d(2) + cy};
            found.emplace_back((float)u, (float)v);
            observed.emplace_back(keysLeft.keyPoints[nIdx].pt);
            // if ( point4d(2) <= 0 || std::isnan(keysLeft.keyPoints[nIdx].pt.x) || std::isnan(keysLeft.keyPoints[nIdx].pt.y))
            //     Logging("out", point4d,3);
            // if (u < 0 || v < 0 || v > zedPtr->mHeight || u > zedPtr->mWidth)
            //     Logging("out", point4d,3);
            
            ceres::CostFunction* costf = OptimizePose::Create(K, point, obs, (double)weights[i]);
            // Logging("obs", obs,3);
            // Logging("point", point,3);
            problem.AddResidualBlock(costf, loss_function /* squared loss */,frame_tcw.data(), frame_qcw.coeffs().data());

            problem.SetManifold(frame_qcw.coeffs().data(),
                                        quaternion_local_parameterization);
        }
    }
    ceres::Solver::Options options;
    options.linear_solver_type = ceres::SPARSE_NORMAL_CHOLESKY;
    // options.use_explicit_schur_complement = true;
    options.max_num_iterations = 100;
    // options.minimizer_progress_to_stdout = false;
    ceres::Solver::Summary summary;
    ceres::Solve(options, &problem, &summary);
    // Logging("ceres report", summary.FullReport(),3);
    Eigen::Matrix3d R = frame_qcw.normalized().toRotationMatrix();
    estimPose.block<3, 3>(0, 0) = R;
    estimPose.block<3, 1>(0, 3) = frame_tcw;

    drawPointsTemp<cv::Point2f, cv::Point2f>("ceres", lIm.rIm, found, observed);
    cv::waitKey(1);
    
    // if ( checkDisplacement(prevPose, estimPose) )
    //     return std::pair<int,int>(0, nIn);
    
    int nIn {0};
    int nStereo = OutliersReprojErr(estimPose, activeMapPoints, keysLeft, matchedIdxsB, thresh, weights, nIn);
    // Logging("pose", estimPose,3);
    return std::pair<int,int>(nIn, nStereo);
}

std::pair<int,int> FeatureTracker::estimatePoseCeres(std::vector<MapPoint*>& activeMapPoints, TrackedKeys& keysLeft, std::vector<int>& matchedIdxsB, Eigen::Matrix4d& estimPose, std::vector<bool>& MPsOutliers, const bool first)
{


    const size_t prevS { activeMapPoints.size()};
    const size_t newS { keysLeft.keyPoints.size()};
    const size_t startPrev {newS - prevS};

    std::vector<float> weights;
    // calcWeights(prePnts, weights);
    weights.resize(prevS, 1.0f);
    std::vector<double>thresholds = {15.6f,9.8f,7.815f,7.815f};
    double thresh = 7.815f;

    // ceres::LossFunction* loss_function = nullptr;
    size_t maxIter {2};
    int nIn {0}, nStereo {0};
    // if ( first )
    //     maxIter = 2;
    for (size_t iter {0}; iter < maxIter; iter ++ )
    {
        ceres::Problem problem;
        Eigen::Vector3d frame_tcw;
        Eigen::Quaterniond frame_qcw;
        // OutliersReprojErr(estimPose, activeMapPoints, keysLeft, matchedIdxsB, thresh, weights, nIn);
        Eigen::Matrix4d prevPose = estimPose;
        Eigen::Matrix4d frame_pose = estimPose;
        Eigen::Matrix3d frame_R;
        frame_R = frame_pose.block<3, 3>(0, 0);
        frame_tcw = frame_pose.block<3, 1>(0, 3);
        frame_qcw = Eigen::Quaterniond(frame_R);
        // std::vector<cv::Point2f> found, observed;
        ceres::Manifold* quaternion_local_parameterization = new ceres::EigenQuaternionManifold;
        ceres::LossFunction* loss_function = new ceres::HuberLoss(sqrt(7.815f));
        for (size_t i{0}, end{matchedIdxsB.size()}; i < end; i++)
        {
            if ( matchedIdxsB[i] < 0 )
                continue;
            MapPoint* mp = activeMapPoints[i];
            if  ( !mp->GetInFrame() || mp->GetIsOutlier() )
                continue;
            if ( MPsOutliers[i] )
                continue;
            const int nIdx {matchedIdxsB[i]};
            if ( mp->close && keysLeft.close[nIdx] )
            {
                Eigen::Vector4d np4d;
                get3dFromKey(np4d, keysLeft.keyPoints[nIdx], keysLeft.estimatedDepth[nIdx]);
                Eigen::Vector3d obs(np4d(0), np4d(1),np4d(2));
                Eigen::Vector3d point = mp->getWordPose3d();
                // Eigen::Vector4d point4d = mp->getWordPose4d();
                // point4d = estimPose * point4d;
                // const double u {fx*point4d(0)/point4d(2) + cx};
                // const double v {fy*point4d(1)/point4d(2) + cy};
                // found.emplace_back((float)u, (float)v);
                // observed.emplace_back(keysLeft.keyPoints[nIdx].pt);
                // Logging("obs", obs,3);
                // Logging("point", point,3);
                ceres::CostFunction* costf = OptimizePoseICP::Create(K, point, obs, (double)weights[i]);
                problem.AddResidualBlock(costf, loss_function /* squared loss */,frame_tcw.data(), frame_qcw.coeffs().data());

                problem.SetManifold(frame_qcw.coeffs().data(),
                                            quaternion_local_parameterization);
            }
            else
            {
                Eigen::Vector2d obs((double)keysLeft.keyPoints[nIdx].pt.x, (double)keysLeft.keyPoints[nIdx].pt.y);
                Eigen::Vector3d point = mp->getWordPose3d();
                // Eigen::Vector4d point4d = mp->getWordPose4d();
                // point4d = estimPose * point4d;
                // const double u {fx*point4d(0)/point4d(2) + cx};
                // const double v {fy*point4d(1)/point4d(2) + cy};
                // found.emplace_back((float)u, (float)v);
                // observed.emplace_back(keysLeft.keyPoints[nIdx].pt);
                // // if ( point4d(2) <= 0 || std::isnan(keysLeft.keyPoints[nIdx].pt.x) || std::isnan(keysLeft.keyPoints[nIdx].pt.y))
                // //     Logging("out", point4d,3);
                // // if (u < 0 || v < 0 || v > zedPtr->mHeight || u > zedPtr->mWidth)
                // //     Logging("out", point4d,3);
                
                ceres::CostFunction* costf = OptimizePose::Create(K, point, obs, (double)weights[i]);
                // Logging("obs", obs,3);
                // Logging("point", point,3);
                problem.AddResidualBlock(costf, loss_function /* squared loss */,frame_tcw.data(), frame_qcw.coeffs().data());

                problem.SetManifold(frame_qcw.coeffs().data(),
                                            quaternion_local_parameterization);
            }
        }
        ceres::Solver::Options options;
        options.linear_solver_type = ceres::SPARSE_NORMAL_CHOLESKY;
        // options.use_explicit_schur_complement = true;
        options.max_num_iterations = 100;
        // options.minimizer_progress_to_stdout = false;
        ceres::Solver::Summary summary;
        ceres::Solve(options, &problem, &summary);
        // Logging("ceres report", summary.FullReport(),3);
        Eigen::Matrix3d R = frame_qcw.normalized().toRotationMatrix();
        estimPose.block<3, 3>(0, 0) = R;
        estimPose.block<3, 1>(0, 3) = frame_tcw;
        // if ( checkDisplacement(prevPose, estimPose) )
        //     return std::pair<int,int>(0, nIn);
        // if ( iter == maxIter - 1)
        // {
            nIn = 0;
            nStereo = 0;
            nStereo = findOutliers(estimPose, activeMapPoints, keysLeft, matchedIdxsB, thresh, MPsOutliers, weights, nIn);
        // }
    }
    // Logging("pose", estimPose,3);
// #if DRAWMATCHES
//     drawPointsTemp<cv::Point2f, cv::Point2f>("ceres", lIm.rIm, found, observed);
//     cv::waitKey(1);
// #endif
    return std::pair<int,int>(nIn, nStereo);
}

std::pair<int,int> FeatureTracker::estimatePoseCeresR(std::vector<MapPoint*>& activeMapPoints, TrackedKeys& keysLeft, std::vector<std::pair<int,int>>& matchesIdxs, Eigen::Matrix4d& estimPose, std::vector<bool>& MPsOutliers, const bool first)
{

    const Eigen::Matrix4d estimPoseRInv = zedPtr->extrinsics.inverse();
    const Eigen::Matrix3d qc1c2 = estimPoseRInv.block<3,3>(0,0);
    const Eigen::Matrix<double,3,1> tc1c2 = estimPoseRInv.block<3,1>(0,3);

    const size_t prevS { activeMapPoints.size()};
    const size_t newS { keysLeft.keyPoints.size()};

    const Eigen::Matrix3d& K = zedPtr->cameraLeft.intrisics;

    std::vector<float> weights;
    weights.resize(prevS, 1.0f);
    std::vector<double>thresholds = {15.6f,9.8f,7.815f,7.815f};
    double thresh = 7.815f;

    size_t maxIter {2};
    int nIn {0}, nStereo {0};
    for (size_t iter {0}; iter < maxIter; iter ++ )
    {
        ceres::Problem problem;
        Eigen::Vector3d frame_tcw;
        Eigen::Quaterniond frame_qcw;
        Eigen::Matrix4d prevPose = estimPose;
        Eigen::Matrix4d frame_pose = estimPose;
        Eigen::Matrix3d frame_R;
        frame_R = frame_pose.block<3, 3>(0, 0);
        frame_tcw = frame_pose.block<3, 1>(0, 3);
        frame_qcw = Eigen::Quaterniond(frame_R);
        ceres::Manifold* quaternion_local_parameterization = new ceres::EigenQuaternionManifold;
        ceres::LossFunction* loss_function = new ceres::HuberLoss(sqrt(7.815f));
        for (size_t i{0}, end{matchesIdxs.size()}; i < end; i++)
        {
            if ( MPsOutliers[i] )
                continue;
            const std::pair<int,int>& keyPos = matchesIdxs[i];

            MapPoint* mp = activeMapPoints[i];
            if ( mp->GetIsOutlier() )
                continue;
            ceres::CostFunction* costf;
            
            if ( keyPos.first >= 0 )
            {
                if  ( !mp->inFrame )
                    continue;
                const int nIdx {keyPos.first};
                if ( /* mp->close && */ keysLeft.close[nIdx] )
                {
                    Eigen::Vector4d depthCheck = estimPose * mp->getWordPose4d();
                    if ( depthCheck(2) < zedPtr->mBaseline * fm.closeNumber )
                        continue;
                    Eigen::Vector4d np4d;
                    get3dFromKey(np4d, keysLeft.keyPoints[nIdx], keysLeft.estimatedDepth[nIdx]);
                    Eigen::Vector3d point = mp->getWordPose3d();
                    Eigen::Vector3d obs(np4d(0), np4d(1),np4d(2));
                    costf = OptimizePoseICP::Create(K, point, obs, 1.0f);
                }
                else
                {
                    Eigen::Vector2d obs((double)keysLeft.keyPoints[nIdx].pt.x, (double)keysLeft.keyPoints[nIdx].pt.y);
                    Eigen::Vector3d point = mp->getWordPose3d();
                    costf = OptimizePose::Create(K, point, obs, 1.0f);
                }

            }
            else if ( keyPos.second >= 0)
            {
                if  ( !mp->inFrameR )
                    continue;
                const int nIdx {keyPos.second};
                Eigen::Vector2d obs((double)keysLeft.rightKeyPoints[nIdx].pt.x, (double)keysLeft.rightKeyPoints[nIdx].pt.y);
                Eigen::Vector3d point = mp->getWordPose3d();
                costf = OptimizePoseR::Create(K,tc1c2, qc1c2, point, obs, 1.0f);
            }
            else
                continue;
            problem.AddResidualBlock(costf, loss_function /* squared loss */,frame_tcw.data(), frame_qcw.coeffs().data());

            problem.SetManifold(frame_qcw.coeffs().data(),
                                        quaternion_local_parameterization);
        }
        ceres::Solver::Options options;
        options.linear_solver_type = ceres::SPARSE_NORMAL_CHOLESKY;
        // options.use_explicit_schur_complement = true;
        options.max_num_iterations = 100;
        // options.minimizer_progress_to_stdout = false;
        ceres::Solver::Summary summary;
        ceres::Solve(options, &problem, &summary);
        // Logging("ceres report", summary.FullReport(),3);
        Eigen::Matrix3d R = frame_qcw.normalized().toRotationMatrix();
        estimPose.block<3, 3>(0, 0) = R;
        estimPose.block<3, 1>(0, 3) = frame_tcw;
        nIn = 0;
        nStereo = 0;
        nStereo = findOutliersR(estimPose, activeMapPoints, keysLeft, matchesIdxs, thresh, MPsOutliers, weights, nIn);
    }
    return std::pair<int,int>(nIn, nStereo);
}

std::pair<std::pair<int,int>, std::pair<int,int>> FeatureTracker::estimatePoseCeresRB(std::vector<MapPoint*>& activeMapPoints, TrackedKeys& keysLeft, std::vector<std::pair<int,int>>& matchesIdxs, std::vector<bool>& MPsOutliers, std::vector<MapPoint*>& activeMapPointsB, TrackedKeys& keysLeftB, std::vector<std::pair<int,int>>& matchesIdxsB, std::vector<bool>& MPsOutliersB, Eigen::Matrix4d& estimPose)
{

    const Eigen::Matrix4d estimPoseRInv = zedPtr->extrinsics.inverse();
    const Eigen::Matrix3d qc1c2 = estimPoseRInv.block<3,3>(0,0);
    const Eigen::Matrix<double,3,1> tc1c2 = estimPoseRInv.block<3,1>(0,3);
    const Eigen::Matrix4d estimPoseBInv = zedPtr->TCamToCamInv;
    const Eigen::Matrix4d estimPoseBRInv = zedPtrB->extrinsics.inverse() * estimPoseBInv;
    const Eigen::Matrix3d qc1c2B = estimPoseBInv.block<3,3>(0,0);
    const Eigen::Matrix<double,3,1> tc1c2B = estimPoseBInv.block<3,1>(0,3);
    const Eigen::Matrix3d qc1c2BR = estimPoseBRInv.block<3,3>(0,0);
    const Eigen::Matrix<double,3,1> tc1c2BR = estimPoseBRInv.block<3,1>(0,3);



    const Eigen::Matrix3d& K = zedPtr->cameraLeft.intrisics;
    const Eigen::Matrix3d& KB = zedPtrB->cameraLeft.intrisics;

    double thresh = 7.815f;

    size_t maxIter {2};
    int nIn {0}, nStereo {0},nInB {0}, nStereoB {0};
    for (size_t iter {0}; iter < maxIter; iter ++ )
    {
        ceres::Problem problem;
        Eigen::Vector3d frame_tcw;
        Eigen::Quaterniond frame_qcw;
        Eigen::Matrix4d frame_pose = estimPose;
        Eigen::Matrix3d frame_R;
        frame_R = frame_pose.block<3, 3>(0, 0);
        frame_tcw = frame_pose.block<3, 1>(0, 3);
        frame_qcw = Eigen::Quaterniond(frame_R);
        ceres::Manifold* quaternion_local_parameterization = new ceres::EigenQuaternionManifold;
        ceres::LossFunction* loss_function = new ceres::HuberLoss(sqrt(7.815f));
        for (size_t i{0}, end{matchesIdxs.size()}; i < end; i++)
        {
            if ( MPsOutliers[i] )
                continue;
            const std::pair<int,int>& keyPos = matchesIdxs[i];

            MapPoint* mp = activeMapPoints[i];
            if ( mp->GetIsOutlier() )
                continue;
            Eigen::Vector4d depthCheck = estimPose * mp->getWordPose4d();
            ceres::CostFunction* costf;
            if ( keyPos.first >= 0 )
            {
                if  ( !mp->inFrame )
                    continue;
                const int nIdx {keyPos.first};
                if (  keysLeft.close[nIdx] )
                {
                    Eigen::Vector4d depthCheck = estimPose * mp->getWordPose4d();
                    if ( depthCheck(2) < zedPtr->mBaseline * fm.closeNumber )
                        continue;
                    Eigen::Vector4d np4d;
                    get3dFromKey(np4d, keysLeft.keyPoints[nIdx], keysLeft.estimatedDepth[nIdx]);
                    Eigen::Vector3d point = mp->getWordPose3d();
                    Eigen::Vector3d obs(np4d(0), np4d(1),np4d(2));
                    costf = OptimizePoseICP::Create(K, point, obs, 1.0f);
                }
                else
                {
                    Eigen::Vector2d obs((double)keysLeft.keyPoints[nIdx].pt.x, (double)keysLeft.keyPoints[nIdx].pt.y);
                    Eigen::Vector3d point = mp->getWordPose3d();
                    costf = OptimizePose::Create(K, point, obs, 1.0f);
                }
                // if ( depthCheck(2) > zedPtr->mBaseline * fm.closeNumber && keysLeft.close[nIdx] )
                //     keysLeft.close[nIdx] = false;
            }
            else if ( keyPos.second >= 0)
            {
                if  ( !mp->inFrameR )
                    continue;
                const int nIdx {keyPos.second};
                Eigen::Vector2d obs((double)keysLeft.rightKeyPoints[nIdx].pt.x, (double)keysLeft.rightKeyPoints[nIdx].pt.y);
                Eigen::Vector3d point = mp->getWordPose3d();
                costf = OptimizePoseR::Create(K,tc1c2, qc1c2, point, obs, 1.0f);
            }
            else
                continue;
            problem.AddResidualBlock(costf, loss_function /* squared loss */,frame_tcw.data(), frame_qcw.coeffs().data());

            problem.SetManifold(frame_qcw.coeffs().data(),
                                        quaternion_local_parameterization);
        }
        for (size_t i{0}, end{matchesIdxsB.size()}; i < end; i++)
        {
            if ( MPsOutliersB[i] )
                continue;
            const std::pair<int,int>& keyPos = matchesIdxsB[i];

            MapPoint* mp = activeMapPointsB[i];
            if ( mp->GetIsOutlier() )
                continue;
            
            ceres::CostFunction* costf;
            if ( keyPos.first >= 0 )
            {
                if  ( !mp->inFrame )
                    continue;
                const int nIdx {keyPos.first};
                if ( keysLeftB.close[nIdx] )
                {
                    Eigen::Vector4d depthCheck = zedPtr->TCamToCamInv * estimPose * mp->getWordPose4d();
                    if ( depthCheck(2) < zedPtr->mBaseline * fm.closeNumber )
                        continue;
                    Eigen::Vector4d np4d;
                    get3dFromKey(np4d, keysLeftB.keyPoints[nIdx], keysLeftB.estimatedDepth[nIdx]);
                    Eigen::Vector3d point = mp->getWordPose3d();
                    Eigen::Vector3d obs(np4d(0), np4d(1),np4d(2));
                    costf = OptimizePoseICPB::Create(KB,tc1c2B, qc1c2B, point, obs, 1.0f);
                }
                else
                {
                    Eigen::Vector2d obs((double)keysLeftB.keyPoints[nIdx].pt.x, (double)keysLeftB.keyPoints[nIdx].pt.y);
                    Eigen::Vector3d point = mp->getWordPose3d();
                    costf = OptimizePoseR::Create(KB,tc1c2B, qc1c2B, point, obs, 1.0f);
                }
                // if ( depthCheck(2) > zedPtr->mBaseline * fm.closeNumber && keysLeftB.close[nIdx] )
                //     keysLeftB.close[nIdx] = false;

            }
            else if ( keyPos.second >= 0)
            {
                if  ( !mp->inFrameR )
                    continue;
                const int nIdx {keyPos.second};
                Eigen::Vector2d obs((double)keysLeftB.rightKeyPoints[nIdx].pt.x, (double)keysLeftB.rightKeyPoints[nIdx].pt.y);
                Eigen::Vector3d point = mp->getWordPose3d();
                costf = OptimizePoseR::Create(KB,tc1c2BR, qc1c2BR, point, obs, 1.0f);
            }
            else
                continue;
            problem.AddResidualBlock(costf, loss_function /* squared loss */,frame_tcw.data(), frame_qcw.coeffs().data());

            problem.SetManifold(frame_qcw.coeffs().data(),
                                        quaternion_local_parameterization);
        }
        ceres::Solver::Options options;
        options.linear_solver_type = ceres::SPARSE_NORMAL_CHOLESKY;
        // options.use_explicit_schur_complement = true;
        options.max_num_iterations = 100;
        // options.minimizer_progress_to_stdout = false;
        ceres::Solver::Summary summary;
        ceres::Solve(options, &problem, &summary);
        // Logging("ceres report", summary.FullReport(),3);
        Eigen::Matrix3d R = frame_qcw.normalized().toRotationMatrix();
        estimPose.block<3, 3>(0, 0) = R;
        estimPose.block<3, 1>(0, 3) = frame_tcw;
        nIn = 0;
        nStereo = 0;
        nStereo = findOutliersRB(zedPtr, estimPose, activeMapPoints, keysLeft, matchesIdxs, thresh, MPsOutliers, nIn);
        nInB = 0;
        nStereoB = 0;
        Eigen::Matrix4d estimPoseB = zedPtr->TCamToCamInv * estimPose;
        nStereoB = findOutliersRB(zedPtrB, estimPoseB, activeMapPointsB, keysLeftB, matchesIdxsB, thresh, MPsOutliersB, nInB);
    }
    return std::pair<std::pair<int,int>, std::pair<int,int>>(std::pair<int,int>(nIn, nStereo),std::pair<int,int>(nInB, nStereoB));
}

void FeatureTracker::removeKeyFrame(std::vector<KeyFrame *>& activeKeyFrames)
{
    int count {activeKeyFrames.size() - 1};
    int removeCount {0};
    std::vector<bool> removeIdx;
    const int maxRemoval {maxActvKFMaxSize - actvKFMaxSize};
    removeIdx.resize(activeKeyFrames.size());
    std::vector<KeyFrame* >::reverse_iterator it, end(activeKeyFrames.rend());
    for ( it = activeKeyFrames.rbegin(); it != end; ++it, count --)
    {
        if (count < 2)
            break;
        if ((*it)->keyF)
        {
            lastValidKF = (*it)->numb;
            continue;
        }
        const int validKeyFramesCount {(*it)->numb - lastValidKF};

        if (validKeyFramesCount >= maxKeyFrameDist)
        {
            (*it)->keyF = true;
            lastValidKF = (*it)->numb;
            continue;
        }
        const size_t validMapPointsSize {map->keyFrames.at(lastValidKF)->localMapPoints.size()};
        const size_t kFMPSize {(*it)->nKeysTracked};
        const float thres {0.9f * (float)validMapPointsSize};
        if (kFMPSize > 100 && kFMPSize > thres)
        {
            (*it)->visualize = false;
            (*it)->active = false;
            removeCount++ ;
            removeIdx[count] = true;
            if (removeCount >= maxRemoval)
                break;
            continue;
        }
        else
        {
            (*it)->keyF = true;
            lastValidKF = (*it)->numb;
        }
    }
    count = activeKeyFrames.size() - 1;
    if ( removeCount < maxRemoval )
    {
        for ( it = activeKeyFrames.rbegin(); it != end; ++it, count --)
        {
            if (removeIdx[count])
                continue;
            (*it)->active = false;
            removeCount++;
            removeIdx[count] = true;
            if (removeCount >= maxRemoval)
                break;
        }
    }

    int j {0};
    for (size_t i{0}, end{activeKeyFrames.size()}; i < end; i ++)
    {
        if (!removeIdx[i])
            activeKeyFrames[j++] = activeKeyFrames[i];
        else
            activeKeyFrames[i]->active = false;
    }
    activeKeyFrames.resize(j);

}

void FeatureTracker::addKeyFrame(TrackedKeys& keysLeft, std::vector<int>& matchedIdxsN, const int nStereo)
{
    KeyFrame* kF = new KeyFrame(zedPtr->cameraPose.pose, lIm.im, lIm.rIm,map->kIdx, curFrame);
    const unsigned long minKIdx {0};
    const unsigned long maxKIdx {map->kIdx};
    std::vector<int>kfCon(maxKIdx - minKIdx + 1,0);
    std::vector<MapPoint*>::const_iterator it, end(activeMapPoints.end());
    for (it = activeMapPoints.begin(); it != end; it ++)
    {
        kfCon[(*it)->kdx - minKIdx] ++;
    }
    kF->connections.resize(maxKIdx - minKIdx + 1,0);
    // kF->connectionWeights.reserve(maxKIdx - minKIdx + 1);
    for (size_t i {0}, end{kfCon.size()}; i < end; i ++)
    {
        if (kfCon[i] > keyFrameConThresh)
        {
            kF->connections[i] = kfCon[i];
            // kF->connectionWeights.emplace_back(kfCon[i]);
        }
    }
    
    kF->unMatchedF.resize(keysLeft.keyPoints.size(), true);
    kF->localMapPoints.reserve(activeMapPoints.size());
    activeMapPoints.reserve(activeMapPoints.size() + keysLeft.keyPoints.size());
    mPPerKeyFrame.reserve(1000);
    // std::lock_guard<std::mutex> lock(map->mapMutex);
    for (size_t i{0}, end{keysLeft.keyPoints.size()}; i < end; i++)
    {
        // if ( keysLeft.close[i] >  )
        if ( matchedIdxsN[i] >= 0 )
        {
            MapPoint* mp = activeMapPoints[matchedIdxsN[i]];
            if ( mp->GetIsOutlier() )
                continue;
            mp->kFWithFIdx.insert(std::pair<KeyFrame*, size_t>(kF, i));
            kF->localMapPoints.emplace_back(mp);
            kF->unMatchedF[i] = false;
            continue;
        }
        if ( nStereo > minNStereo)
            continue;
        if ( keysLeft.estimatedDepth[i] > 0 )
        {
            const double zp = (double)keysLeft.estimatedDepth[i];
            const double xp = (double)(((double)keysLeft.keyPoints[i].pt.x-cx)*zp/fx);
            const double yp = (double)(((double)keysLeft.keyPoints[i].pt.y-cy)*zp/fy);
            Eigen::Vector4d p(xp, yp, zp, 1);
            p = zedPtr->cameraPose.pose * p;
            MapPoint* mp = new MapPoint(p, keysLeft.Desc.row(i), keysLeft.keyPoints[i], keysLeft.close[i], map->kIdx, map->pIdx);
            activeMapPoints.emplace_back(mp);
            map->addMapPoint(mp);
        }
    }
    kF->nKeysTracked = kF->localMapPoints.size();
    kF->keys.getKeys(keysLeft);
    mPPerKeyFrame.push_back(activeMapPoints.size());
    // std::lock_guard<std::mutex> lock(map->mapMutex);
    map->addKeyFrame(kF);
    activeKeyFrames.insert(activeKeyFrames.begin(),kF);
    // removeKeyFrame(activeKeyFrames);
    if ( activeKeyFrames.size() > maxActvKFMaxSize )
    {
        removeKeyFrame(activeKeyFrames);
        // activeKeyFrames.back()->active = false;
        // activeKeyFrames.resize(actvKFMaxSize);
    }
    map->keyFrameAdded = true;
    lastKFPose = zedPtr->cameraPose.pose;
    lastKFPoseInv = lastKFPose.inverse();
    lastKFImage = lIm.im.clone();

}

void FeatureTracker::removeMapPointOut(std::vector<MapPoint*>& activeMapPoints, const Eigen::Matrix4d& estimPose, std::vector<bool>& toRemove)
{
    const size_t end{activeMapPoints.size()};
    toRemove.resize(end);
    int j {0};
    for ( size_t i {0}; i < end; i++)
    {
        MapPoint* mp = activeMapPoints[i];
        if (worldToFrame(mp, estimPose) /* && !mp->GetIsOutlier() */ )
        {
            mp->seenCnt++;
            // if ( activeMapPoints.size() > maxActiveMPSize )
            // {
            //     if ( mp->GetIsOutlier() )
            //     {
            //         mp->setActive(false);
            //         continue;
            //     }
            // }
            mp->setActive(true);
            // if ( !mp->GetIsOutlier() )
            // {
                // mp->addTCnt();
            // }
        }
        else
        {
            // toRemove[i] = true;
            // if ( mp->trackCnt < 5 )
            //     mp->SetIsOutlier(true);
            mp->setActive(false);
            // if ( mp->kFWithFIdx.size() < 3 )
            //     mp->SetIsOutlier(true);
            continue;
        }
        // std::vector<KeyFrame* >::const_iterator it, end(activeKeyFrames.end());
        // for ( it = activeKeyFrames.begin(); it != end; it++)
        // {
        //     if (worldToFrameKF(mp,(*it)->pose.poseInverse))
        //     {
        //         setActive = true;
        //         break;
        //     }
        // }
        // if (!setActive)
        // {
        //     mp->setActive(false);
        //     continue;
        // }
        activeMapPoints[j++] = mp;
    }
    activeMapPoints.resize(j);
}

bool FeatureTracker::worldToFrameKF(MapPoint* mp, const Eigen::Matrix4d& pose)
{
    Eigen::Vector4d point = mp->getWordPose4d();
    point = pose * point;

    if ( point(2) <= 0.0 )
    {
        return false;
    }
    const double invZ = 1.0f/point(2);

    const double u {fx*point(0)*invZ + cx};
    const double v {fy*point(1)*invZ + cy};

    const int h {zedPtr->mHeight};
    const int w {zedPtr->mWidth};

    if ( u < 0 || v < 0 || u >= w || v >= h)
    {
        return false;
    }
    return true;
}

bool FeatureTracker::worldToFrame(MapPoint* mp, const Eigen::Matrix4d& pose)
{
    Eigen::Vector4d point = mp->getWordPose4d();
    point = pose * point;

    if ( point(2) <= 0.0 )
    {
        mp->SetInFrame(false);
        return false;
    }
    const double invZ = 1.0f/point(2);

    const double u {fx*point(0)*invZ + cx};
    const double v {fy*point(1)*invZ + cy};

    const int h {zedPtr->mHeight};
    const int w {zedPtr->mWidth};
    if ( u < 15 || v < 15 || u >= w - 15 || v >= h - 15)
    {
        mp->SetInFrame(false);
        return false;
    }
    mp->SetInFrame(true);
    return true;
}

bool FeatureTracker::worldToFrameR(MapPoint* mp, const bool right, const Eigen::Matrix4d& pose)
{
    Eigen::Vector4d point = mp->getWordPose4d();
    point = pose * point;

    if ( point(2) <= 0.0 )
    {
        if ( right )
            mp->inFrameR = false;
        else
            mp->inFrame = false;
        return false;
    }
    const double invZ = 1.0f/point(2);

    const double u {fx*point(0)*invZ + cx};
    const double v {fy*point(1)*invZ + cy};

    const int h {zedPtr->mHeight};
    const int w {zedPtr->mWidth};
    if ( u < 15 || v < 15 || u >= w - 15 || v >= h - 15)
    {
        if ( right )
            mp->inFrameR = false;
        else
            mp->inFrame = false;
        return false;
    }
    if ( right )
            mp->inFrameR = true;
        else
            mp->inFrame = true;
    return true;
}

bool FeatureTracker::worldToFrameRTrack(MapPoint* mp, const bool right, const Eigen::Matrix4d& predPoseInv, const Eigen::Matrix4d& tempPose)
{
    Eigen::Vector4d wPos = mp->getWordPose4d();
    Eigen::Vector4d point = predPoseInv * wPos;

    double fxc, fyc, cxc, cyc;
    if ( right )
    {
        fxc = fxr;
        fyc = fyr;
        cxc = cxr;
        cyc = cyr;
    }
    else
    {
        fxc = fx;
        fyc = fy;
        cxc = cx;
        cyc = cy;
    }


    if ( point(2) <= 0.0 )
    {
        if ( right )
            mp->inFrameR = false;
        else
            mp->inFrame = false;
        return false;
    }
    const double invZ = 1.0f/point(2);

    const double u {fxc*point(0)*invZ + cxc};
    const double v {fyc*point(1)*invZ + cyc};

    const int h {zedPtr->mHeight};
    const int w {zedPtr->mWidth};
    if ( u < 0 || v < 0 || u >= w || v >= h )
    {
        if ( right )
            mp->inFrameR = false;
        else
            mp->inFrame = false;
        return false;
    }

    Eigen::Vector3d tPoint = point.block<3,1>(0,0);
    float dist = tPoint.norm();

    int predScale = mp->predictScale(dist);

    if ( right )
    {
        // mp->predAngleR = atan2((float)v - (float)va, (float)u - (float)ua);
        mp->scaleLevelR = predScale;
        mp->inFrameR = true;
        mp->predR = cv::Point2f((float)u, (float)v);
    }
    else
    {
        // mp->predAngleL = atan2((float)v - (float)va, (float)u - (float)ua);
        mp->scaleLevelL = predScale;
        mp->inFrame = true;
        mp->predL = cv::Point2f((float)u, (float)v);
    }

    return true;
}

bool FeatureTracker::worldToFrameRTrackB(MapPoint* mp, const Zed_Camera* zedCam, const bool right, const Eigen::Matrix4d& predPoseInv)
{
    Eigen::Vector4d wPos = mp->getWordPose4d();
    Eigen::Vector4d point = predPoseInv * wPos;

    const double fxc = zedCam->cameraLeft.fx;
    const double fyc = zedCam->cameraLeft.fy;
    const double cxc = zedCam->cameraLeft.cx;
    const double cyc = zedCam->cameraLeft.cy;

    if ( point(2) <= 0.0 )
    {
        if ( right )
            mp->inFrameR = false;
        else
            mp->inFrame = false;
        return false;
    }
    const double invZ = 1.0f/point(2);

    const double u {fxc*point(0)*invZ + cxc};
    const double v {fyc*point(1)*invZ + cyc};

    const int h {zedCam->mHeight};
    const int w {zedCam->mWidth};
    if ( u < 0 || v < 0 || u >= w || v >= h )
    {
        if ( right )
            mp->inFrameR = false;
        else
            mp->inFrame = false;
        return false;
    }

    Eigen::Vector3d tPoint = point.block<3,1>(0,0);
    float dist = tPoint.norm();

    int predScale = mp->predictScale(dist);

    if ( right )
    {
        mp->scaleLevelR = predScale;
        mp->inFrameR = true;
        mp->predR = cv::Point2f((float)u, (float)v);
    }
    else
    {
        mp->scaleLevelL = predScale;
        mp->inFrame = true;
        mp->predL = cv::Point2f((float)u, (float)v);
    }

    return true;
}

void FeatureTracker::removeMapPointOutBackUp(std::vector<MapPoint*>& activeMapPoints, const Eigen::Matrix4d& estimPose)
{
    const size_t end{activeMapPoints.size()};
    int j {0};
    for ( size_t i {0}; i < end; i++)
    {
        if ( activeMapPoints[i]->GetIsOutlier() || !activeMapPoints[i]->GetInFrame())
        {
            activeMapPoints[i]->setActive(false);
            continue;
        }
        MapPoint* mp = activeMapPoints[i];
        Eigen::Vector4d point = mp->getWordPose4d();
        point = estimPose * point;

        if ( point(2) <= 0.0 )
        {
            mp->setActive(false);
            mp->SetInFrame(false);
            if (mp->trackCnt >= 3 && !mp->added)
                map->addMapPoint(mp);
            continue;
        }
        const double invZ = 1.0f/point(2);

        const double u {fx*point(0)*invZ + cx};
        const double v {fy*point(1)*invZ + cy};

        const int h {zedPtr->mHeight};
        const int w {zedPtr->mWidth};

        if ( u < 0 || v < 0 || u >= w || v >= h)
        {
            mp->setActive(false);
            mp->SetInFrame(false);
            if (mp->trackCnt >= 3 && !mp->added)
                map->addMapPoint(mp);
            continue;
        }
        mp->addTCnt();
        
        activeMapPoints[j++] = mp;
    }
    activeMapPoints.resize(j);
}

bool FeatureTracker::checkDisplacement(const Eigen::Matrix4d& currPose, Eigen::Matrix4d& estimPose)
{
    const double errorX = currPose(0,3) - estimPose(0,3);
    const double errorY = currPose(1,3) - estimPose(1,3);
    const double errorZ = currPose(2,3) - estimPose(2,3);
    const double error = errorX * errorX + errorY * errorY + errorZ * errorZ;
    if (error > 10.0)
    {
        estimPose = currPose;
        return true;
    }
    else
        return false;
}

void FeatureTracker::calcAngles(std::vector<MapPoint*>& activeMapPoints, std::vector<cv::KeyPoint>& projectedPoints, std::vector<cv::KeyPoint>& prevKeyPos, std::vector<float>& mapAngles)
{
    const size_t keysSize {activeMapPoints.size()};
    for (size_t i {0}; i < keysSize; i ++)
    {
        if (projectedPoints[i].pt.x > 0)
        {
            mapAngles[i] = atan2((float)projectedPoints[i].pt.y - prevKeyPos[i].pt.y, (float)projectedPoints[i].pt.x - prevKeyPos[i].pt.x);
        }
    }
}

void FeatureTracker::Track4(const int frames)
{
    int keyFrameInsert {0};
    for (curFrame = 0; curFrame < frames; curFrame++)
    {
        setLRImages(curFrame);

        TrackedKeys keysLeft;



        if ( curFrame == 0 )
        {
            extractORBStereoMatch(lIm.im, rIm.im, keysLeft);

            initializeMap(keysLeft);
            // addMapPnts(keysLeft);
            setPreLImage();
            setPreRImage();
            continue;
        }
        extractORBStereoMatch(lIm.im, rIm.im, keysLeft);

        Eigen::Matrix4d currPose = predNPoseInv;
        Eigen::Matrix4d prevPose = zedPtr->cameraPose.poseInverse;
        std::vector<int> matchedIdxsN(keysLeft.keyPoints.size(), -1);
        std::lock_guard<std::mutex> lock(map->mapMutex);
        std::vector<int> matchedIdxsB(activeMapPoints.size(), -1);
        // Logging("activeSize", activeMapPoints.size(),3);
        
        if ( curFrame == 1 )
            int nMatches = fm.matchByProjection(activeMapPoints, keysLeft, matchedIdxsN, matchedIdxsB);
        else
        {
            std::vector<cv::KeyPoint> ConVelPoints;
            worldToImg(activeMapPoints, ConVelPoints, predNPoseInv);
            int nNewMatches = fm.matchByProjectionConVel(activeMapPoints, ConVelPoints, keysLeft, matchedIdxsN, matchedIdxsB, 2);
        }

        // std::vector<cv::Point3d> points3d;
        // std::vector<cv::Point2d> points2d;
        // getPoints3dFromMapPoints(activeMapPoints,keysLeft, points3d, points2d, matchedIdxsN);
        // cv::Mat Rvec, tvec;
        // Converter::convertEigenPoseToMat(currPose, Rvec, tvec);
        // std::vector<int>idxs;
        // cv::solvePnPRansac(points3d, points2d, zedPtr->cameraLeft.cameraMatrix, cv::Mat::zeros(5,1,CV_64F), Rvec, tvec,true,100, 4.0f, 0.99, idxs);
        // Eigen::Matrix4d estimPose = Converter::convertRTtoPose(Rvec, tvec);

        // if ( !checkDisplacement(currPose, estimPose) )
        //     removePnPOut(idxs, matchedIdxsN, matchedIdxsB);
        

        // Logging("prednpose ", predNPoseInv,3);


        // Logging("ransac pose", estimPose,3);
        Eigen::Matrix4d estimPose = predNPoseInv;
        refinePose(activeMapPoints, keysLeft, matchedIdxsB, estimPose);

        // Logging("first refine pose", estimPose,3);

        // set outliers

        // after last refine check all matches, change outliers to inliers if they are no more, and in the end remove all outliers from vector. they are already saved on mappoints.

        // the outliers from first refine are not used on the next refines.

        // check for big displacement, if there is, use constant velocity model


        // std::cout << estimPose << std::endl;


        // std::vector<cv::Point2f> mpnts, pnts2f;
        // for ( size_t i {0}, end{activeMapPoints.size()}; i < end; i++)
        // {
        //     if ( matchedIdxsB[i] >= 0)
        //     {
        //         mpnts.emplace_back(activeMapPoints[i]->obs[0].pt);
        //         pnts2f.emplace_back(keysLeft.keyPoints[matchedIdxsB[i]].pt);
        //     }
        // }
        // drawPointsTemp<cv::Point2f>("matches left Pleft", pLIm.rIm, mpnts, pnts2f);
        // cv::waitKey(1);


        std::vector<cv::KeyPoint> projectedPoints, prevKeyPos;
        worldToImg(activeMapPoints, projectedPoints, estimPose);
        worldToImg(activeMapPoints, prevKeyPos, zedPtr->cameraPose.poseInverse);
        std::vector<float> mapAngles;
        calcAngles(activeMapPoints, projectedPoints, prevKeyPos, mapAngles);
        int nNewMatches = fm.matchByProjectionPredWA(activeMapPoints, projectedPoints, prevKeyPos, keysLeft, matchedIdxsN, matchedIdxsB, 7, mapAngles);

        std::vector<cv::Point2f> mpnts, pnts2f;
        for ( size_t i {0}, end{activeMapPoints.size()}; i < end; i++)
        {
            if ( projectedPoints[i].pt.x > 0)
            {
                if ( matchedIdxsB[i] >= 0)
                {
                    mpnts.emplace_back(keysLeft.keyPoints[matchedIdxsB[i]].pt);
                    pnts2f.emplace_back(projectedPoints[i].pt);
                }
            }
        }

        drawPointsTemp<cv::Point2f>("Projected", lIm.rIm, mpnts, pnts2f);
        cv::waitKey(1);

        std::vector<cv::KeyPoint> prevProjectedPoints;
        worldToImg(activeMapPoints, prevProjectedPoints, prevPose);
        std::vector<cv::Point2f> Nmpnts, Npnts2f;
        for ( size_t i {0}, end{activeMapPoints.size()}; i < end; i++)
        {
            if ( matchedIdxsB[i] >= 0)
            {
                if ( prevProjectedPoints[i].pt.x > 0)
                {
                    Nmpnts.emplace_back(prevProjectedPoints[i].pt);
                    Npnts2f.emplace_back(keysLeft.keyPoints[matchedIdxsB[i]].pt);
                }
            }
        }
        drawPointsTemp<cv::Point2f>("NEWW matches left Pleft", lIm.rIm, Nmpnts, Npnts2f);
        cv::waitKey(1);

        std::pair<int,int> nStIn = refinePose(activeMapPoints, keysLeft, matchedIdxsB, estimPose);

        // Logging("second refine pose", estimPose,3);

        Logging("nIn", nStIn.first,3);
        Logging("nStereo", nStIn.second,3);

        // cv::Rodrigues(Rvec, Rvec);
        // estimPose = Converter::convertRTtoPose(Rvec, tvec);

        poseEst = estimPose.inverse();
        // Logging("estimPose after inv", poseEst,3);

        publishPoseNew();
        // if ( nStIn.first < 200 )
        // {
        // keyFrameInsert++;
        // if (keyFrameInsert >= keyFrameInsertThresh || nStIn.second <= 150)
        // {
            // keyFrameInsert = 0;
        addKeyFrame(keysLeft, matchedIdxsN, nStIn.second);
        // }

        removeMapPointOutBackUp(activeMapPoints, estimPose);
        // Logging("activeSizeAfterRemoval", activeMapPoints.size(),3);


        // Logging("I AM IIIIIIIIIIN", activeMapPoints.size(),3);
        // }

        setPreLImage();
        setPreRImage();

        // Remove only out of frame mappoints

        // maybe after last refine triangulate with the new refined pose the far stereo keypoints.

        // ICP only ON CLOSE KEYPOINTS


        // cv::Mat R,t;
        // std::cout << R << std::endl << t << std::endl;

        // projected points ( only those that are not matched or are outliers )

        // fm.matchByProjection(currPose) // overloaded function that does not calculate vector of idxs

        // motion only ba only if keysleft.close = true you do icp if not then do 3d-2d





        // predictORBPoints(prevLeftPnts);
        // fm.matchORBPoints(prevLeftPnts, keysLeft);
        // // std::vector<cv::Point2f> ppnts, pntsn;
        // // cv::KeyPoint::convert(prevLeftPnts.keyPoints, ppnts);
        // // cv::KeyPoint::convert(keysLeft.keyPoints, pntsn);
        // // prevLeftPnts.inliers.clear();
        // // cv::findFundamentalMat(ppnts, pntsn,prevLeftPnts.inliers2);
        // drawLeftMatches("left Matches", pLIm.rIm, prevLeftPnts, keysLeft);
        // cv::waitKey(1);
        // optimizePoseCeres(prevLeftPnts, keysLeft);

        // cloneTrackedKeys(prevLeftPnts, keysLeft);
        // updateMapPoints(prevLeftPnts);


        

    }
    datafile.close();
    map->endOfFrames = true;
}

void FeatureTracker::insertKeyFrame(TrackedKeys& keysLeft, std::vector<int>& matchedIdxsN, const int nStereo, const int nMono, const Eigen::Matrix4d& estimPose)
{
    const KeyFrame* closeKF = activeKeyFrames.front();
    referencePose = estimPose * closeKF->pose.getInvPose();
    // Logging("referencePose in keyframe", referencePose,3);
    KeyFrame* kF = new KeyFrame(referencePose, estimPose, lIm.im, lIm.rIm,map->kIdx, curFrame);
    // Logging("REALPOSE in keyframe", kF->getPose(),3);
    KeyFrame* lastKF = activeKeyFrames.front();
    kF->closestKF = lastKF->numb;
    kF->scaleFactor = fe.scalePyramid;
    kF->nScaleLev = fe.nLevels;
    kF->logScale = log(fe.imScale);
    kF->keyF = true;
    kF->prevKF = lastKF;
    lastKF->nextKF = kF;
    // Logging("closestKF in keyframe", map->keyFrames.at(kF->closestKF)->getPose(),3);
    // Logging("lastKFPose", lastKFPose,3);
    // Logging("lastKFPoseInv", lastKFPoseInv,3);
    kF->keyF = true;
    const unsigned long minKIdx {0};
    const unsigned long maxKIdx {map->kIdx};
    std::vector<int>kfCon(maxKIdx - minKIdx + 1,0);
    std::vector<MapPoint*>::const_iterator it, end(activeMapPoints.end());
    for (it = activeMapPoints.begin(); it != end; it ++)
    {
        kfCon[(*it)->kdx - minKIdx] ++;
    }
    kF->connections.resize(maxKIdx - minKIdx + 1,0);
    // kF->connectionWeights.reserve(maxKIdx - minKIdx + 1);
    for (size_t i {0}, end{kfCon.size()}; i < end; i ++)
    {
        if (kfCon[i] > keyFrameConThresh)
        {
            kF->connections[i] = kfCon[i];
            // kF->connectionWeights.emplace_back(kfCon[i]);
        }
    }
    
    kF->unMatchedF.resize(keysLeft.keyPoints.size(), -1);
    kF->localMapPoints.resize(keysLeft.keyPoints.size(), nullptr);
    activeMapPoints.reserve(activeMapPoints.size() + keysLeft.keyPoints.size());
    mPPerKeyFrame.reserve(1000);
    std::lock_guard<std::mutex> lock(map->mapMutex);
    int trckedKeys {0};
    for (size_t i{0}, end{keysLeft.keyPoints.size()}; i < end; i++)
    {
        // if ( keysLeft.close[i] >  )
        if ( matchedIdxsN[i] >= 0 )
        {
            MapPoint* mp = activeMapPoints[matchedIdxsN[i]];
            if ( mp->GetIsOutlier() || !mp->GetInFrame() )
                continue;
            mp->kFWithFIdx.insert(std::pair<KeyFrame*, size_t>(kF, i));
            mp->desc.push_back(keysLeft.Desc.row(i));
            if ( keysLeft.estimatedDepth[i] > 0 )
                mp->desc.push_back(keysLeft.rightDesc.row(keysLeft.rightIdxs[i]));
            mp->addTCnt();
            kF->localMapPoints[i] = mp;
            kF->unMatchedF[i] = mp->kdx;
            trckedKeys++;
            continue;
        }
        if ( nStereo > minNStereo )
            continue;
        if ( keysLeft.close[i] || ( nMono <= minNMono && keysLeft.estimatedDepth[i] > 0) )
        {
            const double zp = (double)keysLeft.estimatedDepth[i];
            const double xp = (double)(((double)keysLeft.keyPoints[i].pt.x-cx)*zp/fx);
            const double yp = (double)(((double)keysLeft.keyPoints[i].pt.y-cy)*zp/fy);
            Eigen::Vector4d p(xp, yp, zp, 1);
            p = estimPose * p;
            MapPoint* mp = new MapPoint(p, keysLeft.Desc.row(i), keysLeft.keyPoints[i], keysLeft.close[i], map->kIdx, map->pIdx);
            mp->desc.push_back(keysLeft.rightDesc.row(keysLeft.rightIdxs[i]));
            mp->kFWithFIdx.insert(std::pair<KeyFrame*, size_t>(kF, i));
            // kF->unMatchedF[i] = false;
            // kF->localMapPoints.emplace_back(mp);
            kF->localMapPoints[i] = mp;
            // kF->unMatchedF[i] = mp->kdx;
            // if ( keysLeft.close[i] )
            // {
            mp->added = true;
            activeMapPoints.emplace_back(mp);
            map->addMapPoint(mp);
            trckedKeys ++;
            // }
        }
    }
    lastKFTrackedNumb = trckedKeys;
    kF->nKeysTracked = trckedKeys;
    kF->keys.getKeys(keysLeft);
    mPPerKeyFrame.push_back(activeMapPoints.size());
    // std::lock_guard<std::mutex> lock(map->mapMutex);
    map->addKeyFrame(kF);
    activeKeyFrames.insert(activeKeyFrames.begin(),kF);
    // removeKeyFrame(activeKeyFrames);
    // if ( activeKeyFrames.size() > actvKFMaxSize )
    // {
    //     // removeKeyFrame(activeKeyFrames);
    //     activeKeyFrames.back()->active = false;
    //     activeKeyFrames.resize(actvKFMaxSize);
    // }
    // referencePose = Eigen::Matrix4d::Identity();
    // zedPtr->cameraPose.refPose = referencePose;
    lastKFPose = estimPose;
    lastKFPoseInv = lastKFPose.inverse();
    lastKFImage = lIm.im.clone();
    allFrames.emplace_back(kF);
    if ( activeKeyFrames.size() > 3 )
        map->keyFrameAdded = true;

}

void FeatureTracker::insertKeyFrameR(TrackedKeys& keysLeft, std::vector<int>& matchedIdxsL, std::vector<std::pair<int,int>>& matchesIdxs, const int nStereo, const Eigen::Matrix4d& estimPose, std::vector<bool>& MPsOutliers, const cv::Mat& leftIm)
{
    KeyFrame* lastKF = activeKeyFrames.front();
    referencePose = lastKF->pose.getInvPose() * estimPose;
    // Logging("referencePose in keyframe", referencePose,3);
    KeyFrame* kF = new KeyFrame(referencePose, estimPose, lIm.im, lIm.rIm,map->kIdx, curFrame);
    // Logging("REALPOSE in keyframe", kF->getPose(),3);
    kF->closestKF = lastKF->numb;
    kF->scaleFactor = fe.scalePyramid;
    kF->sigmaFactor = fe.sigmaFactor;
    kF->InvSigmaFactor = fe.InvSigmaFactor;
    kF->nScaleLev = fe.nLevels;
    kF->logScale = log(fe.imScale);
    kF->keyF = true;
    kF->prevKF = lastKF;
    lastKF->nextKF = kF;
    // const unsigned long minKIdx {0};
    // const unsigned long maxKIdx {map->kIdx};
    // std::vector<int>kfCon(maxKIdx - minKIdx + 1,0);
    // std::vector<MapPoint*>::const_iterator it, end(activeMapPoints.end());
    // for (it = activeMapPoints.begin(); it != end; it ++)
    // {
    //     kfCon[(*it)->kdx - minKIdx] ++;
    // }
    // kF->connections.resize(maxKIdx - minKIdx + 1,0);
    // // kF->connectionWeights.reserve(maxKIdx - minKIdx + 1);
    // for (size_t i {0}, end{kfCon.size()}; i < end; i ++)
    // {
    //     if (kfCon[i] > keyFrameConThresh)
    //     {
    //         kF->connections[i] = kfCon[i];
    //         // kF->connectionWeights.emplace_back(kfCon[i]);
    //     }
    // }
    
    kF->unMatchedF.resize(keysLeft.keyPoints.size(), -1);
    kF->unMatchedFR.resize(keysLeft.rightKeyPoints.size(), -1);
    kF->localMapPoints.resize(keysLeft.keyPoints.size(), nullptr);
    kF->localMapPointsR.resize(keysLeft.rightKeyPoints.size(), nullptr);
    activeMapPoints.reserve(activeMapPoints.size() + keysLeft.keyPoints.size());
    kF->keys.getKeys(keysLeft);
    std::lock_guard<std::mutex> lock(map->mapMutex);
    int trckedKeys {0};
    for ( size_t i{0}, end {matchesIdxs.size()}; i < end; i++)
    {
        MapPoint* mp = activeMapPoints[i];
        std::pair<int,int>& keyPos = matchesIdxs[i];
        if ( !mp )
            continue;
        if ( keyPos.first < 0 && keyPos.second < 0 )
            continue;
        if ( MPsOutliers[i] )
            continue;
        mp->kFMatches.insert(std::pair<KeyFrame*, std::pair<int,int>>(kF, keyPos));
        mp->update(kF);

        if ( keyPos.first >= 0 )
        {
            kF->localMapPoints[keyPos.first] = mp;
            kF->unMatchedF[keyPos.first] = mp->kdx;
        }
        if ( keyPos.second >= 0 )
        {
            kF->localMapPointsR[keyPos.second] = mp;
            kF->unMatchedFR[keyPos.second] = mp->kdx;
        }
        trckedKeys++;
        

    }

    if ( nStereo < minNStereo)
    {
        std::vector<std::pair<float, int>> allDepths;
        allDepths.reserve(keysLeft.keyPoints.size());
        for (size_t i{0}, end{keysLeft.keyPoints.size()}; i < end; i++)
        {
            if ( keysLeft.estimatedDepth[i] > 0 && matchedIdxsL[i] < 0 ) 
                allDepths.emplace_back(keysLeft.estimatedDepth[i], i);
        }
        std::sort(allDepths.begin(), allDepths.end());
        int count {0};
        for (size_t i{0}, end{allDepths.size()}; i < end; i++)
        {
            const int lIdx {allDepths[i].second};
            const int rIdx {keysLeft.rightIdxs[lIdx]};
            if ( count >= maxAddedStereo && !keysLeft.close[lIdx] )
                break;
            count ++;
            const double zp = (double)keysLeft.estimatedDepth[lIdx];
            const double xp = (double)(((double)keysLeft.keyPoints[lIdx].pt.x-cx)*zp/fx);
            const double yp = (double)(((double)keysLeft.keyPoints[lIdx].pt.y-cy)*zp/fy);
            Eigen::Vector4d p(xp, yp, zp, 1);
            p = estimPose * p;
            MapPoint* mp = new MapPoint(p, keysLeft.Desc.row(lIdx), keysLeft.keyPoints[lIdx], keysLeft.close[lIdx], map->kIdx, map->pIdx);
            mp->kFMatches.insert(std::pair<KeyFrame*, std::pair<int,int>>(kF, std::pair<int,int>(lIdx,rIdx)));
            mp->update(kF);
            kF->localMapPoints[lIdx] = mp;
            kF->localMapPointsR[rIdx] = mp;
            activeMapPoints.emplace_back(mp);
            map->addMapPoint(mp);
            trckedKeys ++;
        }

    }
    kF->calcConnections();
    lastKFTrackedNumb = trckedKeys;
    kF->nKeysTracked = trckedKeys;
    map->addKeyFrame(kF);
    activeKeyFrames.insert(activeKeyFrames.begin(),kF);
    lastKFPose = estimPose;
    lastKFPoseInv = lastKFPose.inverse();
    lastKFImage = leftIm.clone();
    allFrames.emplace_back(kF);
    if ( activeKeyFrames.size() > 3 )
        map->keyFrameAdded = true;

}

void FeatureTracker::insertKeyFrameRB(TrackedKeys& keysLeft, std::vector<int>& matchedIdxsL, std::vector<std::pair<int,int>>& matchesIdxs, std::vector<bool>& MPsOutliers, TrackedKeys& keysLeftB, std::vector<int>& matchedIdxsLB, std::vector<std::pair<int,int>>& matchesIdxsB, std::vector<bool>& MPsOutliersB, const int nStereo, const int nStereoB, const Eigen::Matrix4d& estimPose, const cv::Mat& leftIm)
{
    KeyFrame* lastKF = activeKeyFrames.front();
    referencePose = lastKF->pose.getInvPose() * estimPose;
    // Logging("referencePose in keyframe", referencePose,3);
    KeyFrame* kF = new KeyFrame(referencePose, estimPose, lIm.im, lIm.rIm,map->kIdx, curFrame);
    kF->setBackPose(kF->pose.pose * zedPtr->TCamToCam);
    // Logging("REALPOSE in keyframe", kF->getPose(),3);
    kF->closestKF = lastKF->numb;
    kF->scaleFactor = fe.scalePyramid;
    kF->sigmaFactor = fe.sigmaFactor;
    kF->InvSigmaFactor = fe.InvSigmaFactor;
    kF->nScaleLev = fe.nLevels;
    kF->logScale = log(fe.imScale);
    kF->keyF = true;
    kF->prevKF = lastKF;
    lastKF->nextKF = kF;
    kF->unMatchedF.resize(keysLeft.keyPoints.size(), -1);
    kF->unMatchedFR.resize(keysLeft.rightKeyPoints.size(), -1);
    kF->localMapPoints.resize(keysLeft.keyPoints.size(), nullptr);
    kF->localMapPointsR.resize(keysLeft.rightKeyPoints.size(), nullptr);

    kF->unMatchedFB.resize(keysLeftB.keyPoints.size(), -1);
    kF->unMatchedFRB.resize(keysLeftB.rightKeyPoints.size(), -1);
    kF->localMapPointsB.resize(keysLeftB.keyPoints.size(), nullptr);
    kF->localMapPointsRB.resize(keysLeftB.rightKeyPoints.size(), nullptr);

    activeMapPoints.reserve(activeMapPoints.size() + keysLeft.keyPoints.size());
    activeMapPointsB.reserve(activeMapPointsB.size() + keysLeftB.keyPoints.size());
    kF->keys.getKeys(keysLeft);
    kF->keysB.getKeys(keysLeftB);
    const Eigen::Matrix4d backCameraPose = estimPose * zedPtr->TCamToCam;
    std::lock_guard<std::mutex> lock(map->mapMutex);
    int trckedKeys {0};
    for ( size_t i{0}, end {matchesIdxs.size()}; i < end; i++)
    {
        MapPoint* mp = activeMapPoints[i];
        std::pair<int,int>& keyPos = matchesIdxs[i];
        if ( !mp )
            continue;
        if ( keyPos.first < 0 && keyPos.second < 0 )
            continue;
        if ( MPsOutliers[i] )
            continue;
        mp->kFMatches.insert(std::pair<KeyFrame*, std::pair<int,int>>(kF, keyPos));
        mp->update(kF);

        if ( keyPos.first >= 0 )
        {
            kF->localMapPoints[keyPos.first] = mp;
            kF->unMatchedF[keyPos.first] = mp->kdx;
        }
        if ( keyPos.second >= 0 )
        {
            kF->localMapPointsR[keyPos.second] = mp;
            kF->unMatchedFR[keyPos.second] = mp->kdx;
        }
        trckedKeys++;
        

    }

    for ( size_t i{0}, end {matchesIdxsB.size()}; i < end; i++)
    {
        MapPoint* mp = activeMapPointsB[i];
        std::pair<int,int>& keyPos = matchesIdxsB[i];
        if ( !mp )
            continue;
        if ( keyPos.first < 0 && keyPos.second < 0 )
            continue;
        if ( MPsOutliersB[i] )
            continue;
        mp->kFMatchesB.insert(std::pair<KeyFrame*, std::pair<int,int>>(kF, keyPos));
        mp->update(kF, true);

        if ( keyPos.first >= 0 )
        {
            kF->localMapPointsB[keyPos.first] = mp;
            kF->unMatchedFB[keyPos.first] = mp->kdx;
        }
        if ( keyPos.second >= 0 )
        {
            kF->localMapPointsRB[keyPos.second] = mp;
            kF->unMatchedFRB[keyPos.second] = mp->kdx;
        }
        trckedKeys++;
        

    }

    if ( nStereo < minNStereo)
    {

        std::vector<std::pair<float, int>> allDepths;
        allDepths.reserve(keysLeft.keyPoints.size());
        for (size_t i{0}, end{keysLeft.keyPoints.size()}; i < end; i++)
        {
            if ( keysLeft.estimatedDepth[i] > 0 && matchedIdxsL[i] < 0 ) 
                allDepths.emplace_back(keysLeft.estimatedDepth[i], i);
        }
        std::sort(allDepths.begin(), allDepths.end());
        int count {0};
        for (size_t i{0}, end{allDepths.size()}; i < end; i++)
        {
            const int lIdx {allDepths[i].second};
            const int rIdx {keysLeft.rightIdxs[lIdx]};
            if ( count >= maxAddedStereo && !keysLeft.close[lIdx] )
                break;
            count ++;
            const double zp = (double)keysLeft.estimatedDepth[lIdx];
            const double xp = (double)(((double)keysLeft.keyPoints[lIdx].pt.x-cx)*zp/fx);
            const double yp = (double)(((double)keysLeft.keyPoints[lIdx].pt.y-cy)*zp/fy);
            Eigen::Vector4d p(xp, yp, zp, 1);
            p = estimPose * p;
            MapPoint* mp = new MapPoint(p, keysLeft.Desc.row(lIdx), keysLeft.keyPoints[lIdx], keysLeft.close[lIdx], map->kIdx, map->pIdx);
            mp->kFMatches.insert(std::pair<KeyFrame*, std::pair<int,int>>(kF, std::pair<int,int>(lIdx,rIdx)));
            mp->update(kF);
            kF->localMapPoints[lIdx] = mp;
            kF->localMapPointsR[rIdx] = mp;
            activeMapPoints.emplace_back(mp);
            map->addMapPoint(mp);
            trckedKeys ++;
        }
    }
    if ( nStereoB < minNStereo)
    {
        std::vector<std::pair<float, int>> allDepths;
        allDepths.reserve(keysLeftB.keyPoints.size());
        for (size_t i{0}, end{keysLeftB.keyPoints.size()}; i < end; i++)
        {
            if ( keysLeftB.estimatedDepth[i] > 0 && matchedIdxsLB[i] < 0 ) 
                allDepths.emplace_back(keysLeftB.estimatedDepth[i], i);
        }
        std::sort(allDepths.begin(), allDepths.end());
        int count = 0;
        for (size_t i{0}, end{allDepths.size()}; i < end; i++)
        {
            const int lIdx {allDepths[i].second};
            const int rIdx {keysLeftB.rightIdxs[lIdx]};
            if ( count >= maxAddedStereo && !keysLeftB.close[lIdx] )
                break;
            count ++;
            const double zp = (double)keysLeftB.estimatedDepth[lIdx];
            const double xp = (double)(((double)keysLeftB.keyPoints[lIdx].pt.x-cx)*zp/fx);
            const double yp = (double)(((double)keysLeftB.keyPoints[lIdx].pt.y-cy)*zp/fy);
            Eigen::Vector4d p(xp, yp, zp, 1);
            p = backCameraPose * p;
            MapPoint* mp = new MapPoint(p, keysLeftB.Desc.row(lIdx), keysLeftB.keyPoints[lIdx], keysLeftB.close[lIdx], map->kIdx, map->pIdx);
            mp->kFMatchesB.insert(std::pair<KeyFrame*, std::pair<int,int>>(kF, std::pair<int,int>(lIdx,rIdx)));
            mp->update(kF, true);
            kF->localMapPointsB[lIdx] = mp;
            kF->localMapPointsRB[rIdx] = mp;
            activeMapPointsB.emplace_back(mp);
            map->addMapPoint(mp);
            trckedKeys ++;
        }

    }
    kF->calcConnections();
    lastKFTrackedNumb = trckedKeys;
    kF->nKeysTracked = trckedKeys;
    map->addKeyFrame(kF);
    activeKeyFrames.insert(activeKeyFrames.begin(),kF);
    lastKFPose = estimPose;
    lastKFPoseInv = lastKFPose.inverse();
    lastKFImage = leftIm.clone();
    allFrames.emplace_back(kF);
    if ( activeKeyFrames.size() > 3 )
        map->keyFrameAdded = true;

}

KeyFrame* FeatureTracker::insertKeyFrameOut(TrackedKeys& keysLeft, const Eigen::Matrix4d& estimPose)
{
    const KeyFrame* closeKF = activeKeyFrames.front();
    referencePose = estimPose * closeKF->pose.getInvPose();
    // Logging("referencePose in keyframe", referencePose,3);
    KeyFrame* kF = new KeyFrame(referencePose, estimPose, lIm.im, lIm.rIm,map->kIdx, curFrame);
    // Logging("REALPOSE in keyframe", kF->getPose(),3);
    kF->closestKF = activeKeyFrames.front()->numb;
    // Logging("closestKF in keyframe", map->keyFrames.at(kF->closestKF)->getPose(),3);
    // Logging("lastKFPose", lastKFPose,3);
    // Logging("lastKFPoseInv", lastKFPoseInv,3);
    kF->keyF = false;
    const unsigned long minKIdx {0};
    const unsigned long maxKIdx {map->kIdx};
    std::vector<int>kfCon(maxKIdx - minKIdx + 1,0);
    std::vector<MapPoint*>::const_iterator it, end(activeMapPoints.end());
    for (it = activeMapPoints.begin(); it != end; it ++)
    {
        kfCon[(*it)->kdx - minKIdx] ++;
    }
    kF->connections.resize(maxKIdx - minKIdx + 1,0);
    // kF->connectionWeights.reserve(maxKIdx - minKIdx + 1);
    for (size_t i {0}, end{kfCon.size()}; i < end; i ++)
    {
        if (kfCon[i] > keyFrameConThresh)
        {
            kF->connections[i] = kfCon[i];
            // kF->connectionWeights.emplace_back(kfCon[i]);
        }
    }
    kF->keys.getKeys(keysLeft);
    allFrames.emplace_back(kF);
    return kF;

}

void FeatureTracker::insertFrame(TrackedKeys& keysLeft, std::vector<int>& matchedIdxsN, const int nStereo, const Eigen::Matrix4d& estimPose)
{
    referencePose = lastKFPoseInv * estimPose;
    KeyFrame* kF = new KeyFrame(referencePose, estimPose, lIm.im, lIm.rIm,map->kIdx, curFrame);
    kF->closestKF = activeKeyFrames.front()->numb;
    kF->keyF = false;
    kF->active = false;
    kF->visualize = false;
    const unsigned long minKIdx {0};
    const unsigned long maxKIdx {map->kIdx};
    std::vector<int>kfCon(maxKIdx - minKIdx + 1,0);
    std::vector<MapPoint*>::const_iterator it, end(activeMapPoints.end());
    for (it = activeMapPoints.begin(); it != end; it ++)
    {
        kfCon[(*it)->kdx - minKIdx] ++;
    }
    kF->connections.resize(maxKIdx - minKIdx + 1,0);
    for (size_t i {0}, end{kfCon.size()}; i < end; i ++)
    {
        if (kfCon[i] > keyFrameConThresh)
        {
            kF->connections[i] = kfCon[i];
        }
    }
    
    kF->unMatchedF.resize(keysLeft.keyPoints.size(), -1);
    kF->localMapPoints.resize(keysLeft.keyPoints.size(), nullptr);
    // kF->localMapPoints.resize(activeMapPoints.size(), nullptr);
    // kF->localMapPoints.reserve(activeMapPoints.size());
    activeMapPoints.reserve(activeMapPoints.size() + keysLeft.keyPoints.size());
    for (size_t i{0}, end{keysLeft.keyPoints.size()}; i < end; i++)
    {
        if ( matchedIdxsN[i] >= 0 )
        {
            MapPoint* mp = activeMapPoints[matchedIdxsN[i]];
            if ( mp->GetIsOutlier() || !mp->GetInFrame() )
                continue;
            mp->kFWithFIdx.insert(std::pair<KeyFrame*, size_t>(kF, i));
            mp->desc.push_back(keysLeft.Desc.row(i));
            mp->addTCnt();
            kF->localMapPoints[i] = mp;
            // kF->localMapPoints.emplace_back(mp);
            kF->unMatchedF[i] = mp->kdx;
            continue;
        }
    }
    kF->nKeysTracked = kF->localMapPoints.size();
    kF->keys.getKeys(keysLeft);
    map->addKeyFrame(kF);
    // map->frameAdded = true;
}

void FeatureTracker::addFrame(TrackedKeys& keysLeft, std::vector<int>& matchedIdxsN, const int nStereo, const Eigen::Matrix4d& estimPose)
{
    const KeyFrame* closeKF = activeKeyFrames.front();
    referencePose = estimPose * closeKF->pose.getInvPose();
    KeyFrame* kF = new KeyFrame(referencePose, estimPose, lIm.im, lIm.rIm,map->kIdx, curFrame);
    kF->closestKF = activeKeyFrames.front()->numb;
    kF->keyF = false;
    kF->active = false;
    kF->visualize = false;
    allFrames.emplace_back(kF);
    
}

void FeatureTracker::changePosesLBA()
{
    KeyFrame* kf = map->keyFrames.at(map->endLBAIdx);
    while ( true )
    {
        KeyFrame* nextKF = kf->nextKF;
        if ( nextKF )
        {
            Eigen::Matrix4d keyPose = kf->getPose();
            nextKF->pose.changePose(keyPose);
            kf = nextKF;
        }
        else
            break;
    }
    Eigen::Matrix4d keyPose = kf->getPose();
    zedPtr->cameraPose.changePose(keyPose);

    lastKFPose = keyPose;
    lastKFPoseInv = lastKFPose.inverse();

    Eigen::Matrix4d prevPose = prevKF->pose.pose * prevReferencePose;

    predNPose = zedPtr->cameraPose.pose * (prevPose.inverse() * zedPtr->cameraPose.pose);
    predNPoseInv = predNPose.inverse();

}

void FeatureTracker::changePosesLBAB()
{
    KeyFrame* kf = map->keyFrames.at(map->endLBAIdx);
    while ( true )
    {
        KeyFrame* nextKF = kf->nextKF;
        if ( nextKF )
        {
            Eigen::Matrix4d keyPose = kf->getPose();
            nextKF->pose.setPose(keyPose * nextKF->pose.refPose);
            nextKF->setBackPose(nextKF->pose.pose * zedPtr->TCamToCam);
            kf = nextKF;
        }
        else
            break;
    }
    Eigen::Matrix4d keyPose = kf->getPose();
    zedPtr->cameraPose.changePose(keyPose);
    zedPtrB->cameraPose.setPose(zedPtr->cameraPose.pose * zedPtr->TCamToCam);

    lastKFPose = keyPose;
    lastKFPoseInv = lastKFPose.inverse();

    Eigen::Matrix4d prevPose = prevKF->pose.pose * prevReferencePose;

    predNPose = zedPtr->cameraPose.pose * (prevPose.inverse() * zedPtr->cameraPose.pose);
    predNPoseInv = predNPose.inverse();

}

void FeatureTracker::checkWithFund(const std::vector<cv::KeyPoint>& activeKeys, const std::vector<cv::KeyPoint>& newKeys, std::vector<int>& matchedIdxsB, std::vector<int>& matchedIdxsN)
{
    const size_t end {matchedIdxsB.size()};
    std::vector<cv::Point2f> prevKeys, currKeys;
    std::vector<int> idxs;
    idxs.reserve(end);
    prevKeys.reserve(end);
    currKeys.reserve(end);
    for (size_t i {0}; i < end; i++)
    {
        if ( matchedIdxsB[i] < 0)
            continue;
        prevKeys.emplace_back(activeKeys[i].pt);
        currKeys.emplace_back(newKeys[matchedIdxsB[i]].pt);
        idxs.emplace_back(i);
    }
    std::vector<uchar> inliers;
    cv::findFundamentalMat(prevKeys, currKeys, inliers, cv::FM_RANSAC, 1, 0.99);
    for (size_t i {0}, end2 {prevKeys.size()}; i < end2; i++)
    {
        if ( inliers[i] )
            continue;
        matchedIdxsN[matchedIdxsB[idxs[i]]] = -1;
        matchedIdxsB[idxs[i]] = -1;
    }

}

void FeatureTracker::checkPrevAngles(std::vector<float>& mapAngles, std::vector<cv::KeyPoint>& prevKeys, std::vector<int>& matchedIdxsN, std::vector<int>& matchedIdxsB, const TrackedKeys& keysLeft)
{
    for (size_t i{0}, end{matchedIdxsB.size()}; i < end; i++)
    {
        if ( matchedIdxsB[i] < 0 )
            continue;
        const cv::KeyPoint& kPO = keysLeft.keyPoints[matchedIdxsB[i]];
        const cv::KeyPoint& kPL = prevKeys[i];
        float ang = atan2(kPO.pt.y - kPL.pt.y, kPO.pt.x - kPL.pt.x);
        if (abs(ang - mapAngles[i]) <= 0.2 || (pow(kPO.pt.x - kPL.pt.x,2) + pow(kPO.pt.y - kPL.pt.y,2) < maxDistAng) )
            continue;
        matchedIdxsN[matchedIdxsB[i]] = -1;
        matchedIdxsB[i] = -1;
    }
}

void FeatureTracker::calculatePrevKeyPos(std::vector<MapPoint*>& activeMapPoints, std::vector<cv::KeyPoint>& projectedPoints, const Eigen::Matrix4d& currPoseInv, const Eigen::Matrix4d& predPoseInv)
{
    projectedPoints.resize(activeMapPoints.size());
    Eigen::Matrix4d temp = currPoseInv * predPoseInv.inverse();
    for ( size_t i {0}, end{activeMapPoints.size()}; i < end; i++)
    {
        projectedPoints[i].pt = cv::Point2f(-1, -1);
        MapPoint* mp = activeMapPoints[i];
        // const Eigen::Matrix4d& currPoseInv = map->keyFrames.at(mp->kdx)->pose.getInvPose();
        Eigen::Vector4d p4d = predPoseInv * mp->getWordPose4d();

        const double mult = 0.1/p4d(2);



        p4d = p4d*mult;
        p4d(3) = 1;

        p4d = temp * p4d;

        // if (p4d(2) <= 0.0f )
        // {
        //     mp->SetInFrame(false);
        //     continue;
        // }

        const double invfx = 1.0f/fx;
        const double invfy = 1.0f/fy;


        double u {fx * p4d(0)/p4d(2) + cx};
        double v {fy * p4d(1)/p4d(2) + cy};

        const int h = zedPtr->mHeight;
        const int w = zedPtr->mWidth;

        // if ( u < 0 )
        //     u = 0.1;
        // if ( v < 0 )
        //     v = 0.1;
        // if ( u > w )
        //     u = w - 1;
        // if ( v > h )
        //     v = h - 1;
        projectedPoints[i].pt = cv::Point2f((float)u, (float)v);
    }
}

void FeatureTracker::Track5(const int frames)
{
    int keyFrameInsert {0};
    const int imageH {zedPtr->mHeight};
    const int imageW {zedPtr->mWidth};
    const int numbOfPixels {zedPtr->mHeight * zedPtr->mWidth};
    
    allFrames.reserve(frames);
    for (curFrame = 0; curFrame < frames; curFrame++)
    {
        // Timer all("all");
        // if ( curFrame == 25)
        // {
        //     map->LBADone = true;
        //     map->endLBAIdx = activeKeyFrames[5]->numb;
        //     activeKeyFrames[5]->pose.pose = Eigen::Matrix4d::Identity();
        // }
        if ( map->LBADone )
        {
            // changePosesLBA();
            std::lock_guard<std::mutex> lock(map->mapMutex);
            if ( map->activeKeyFrames.size() > actvKFMaxSize )
            {
                // removeKeyFrame(activeKeyFrames);
                for ( size_t i{ actvKFMaxSize }, end{map->activeKeyFrames.size()}; i < end; i++)
                {
                    map->activeKeyFrames[i]->active = false;
                }
                activeKeyFrames.resize(actvKFMaxSize);
            }
            map->LBADone = false;
        }
        // if ( curFrame > 2)
        //     Logging("after waitkey activeKeyFrames", activeKeyFrames.front()->getPose(),3);
        
        // cv::waitKey(0);
        // if (curFrame == 25)
        // {
        //     Logging("frame","25",3);
        // }
        // Logging("1","",3);
        // if ( curFrame%3 != 0 && curFrame != 0  && curFrame != 1 )
        //     continue;
        setLRImages(curFrame);

        TrackedKeys keysLeft;



        if ( curFrame == 0 )
        {
            extractORBStereoMatch(lIm.im, rIm.im, keysLeft);

            initializeMap(keysLeft);
            // addMapPnts(keysLeft);
            setPreLImage();
            setPreRImage();
            continue;
        }
        extractORBStereoMatch(lIm.im, rIm.im, keysLeft);
        // Timer lel3("after extract");
        // Logging("2","",3);

        // Eigen::Matrix4d currPose = predNPoseInv;
        // Eigen::Matrix4d prevPose = zedPtr->cameraPose.poseInverse;
        std::vector<int> matchedIdxsN(keysLeft.keyPoints.size(), -1);
        std::lock_guard<std::mutex> lock(map->mapMutex);
        std::vector<int> matchedIdxsB(activeMapPoints.size(), -1);
        std::vector<bool> MPsOutliers(activeMapPoints.size(), false);
        
        
        const double dif = cv::norm(lastKFImage,lIm.im, cv::NORM_L2);
        const double similarity = 1 - dif/(numbOfPixels);

        if ( curFrame == 1 )
            int nMatches = fm.matchByProjection(activeMapPoints, keysLeft, matchedIdxsN, matchedIdxsB);
        else
        {
            std::vector<cv::KeyPoint> ConVelPoints;
            // std::vector<int>scaleKeys;
            worldToImg(activeMapPoints, ConVelPoints, predNPoseInv);
            // worldToImgScale(activeMapPoints, ConVelPoints, predNPoseInv, scaleKeys);
            std::vector<float> mapAngles(activeMapPoints.size(), -5.0);
            std::vector<cv::KeyPoint> prevKeyPos;

            // worldToImg(activeMapPoints, prevKeyPos, zedPtr->cameraPose.poseInverse);
            calculatePrevKeyPos(activeMapPoints, prevKeyPos, zedPtr->cameraPose.poseInverse, predNPoseInv);
            // if ( similarity < noMovementCheck )
            //     calcAngles(activeMapPoints, ConVelPoints, prevKeyPos, mapAngles);

            int nNewMatches = fm.matchByProjectionConVelAng(activeMapPoints, ConVelPoints, prevKeyPos, keysLeft, matchedIdxsN, matchedIdxsB, 2, mapAngles);
            // int nNewMatches = fm.matchByProjectionConVelAngScale(activeMapPoints, ConVelPoints, prevKeyPos, keysLeft, matchedIdxsN, matchedIdxsB, 2, mapAngles, scaleKeys);
        }

        // {
        // std::vector<cv::KeyPoint>activeKeys;
        // worldToImg(activeMapPoints, activeKeys, zedPtr->cameraPose.poseInverse);
        // checkWithFund(activeKeys, keysLeft.keyPoints, matchedIdxsB, matchedIdxsN);
        // }

        Eigen::Matrix4d estimPose = predNPoseInv;
        estimatePoseCeres(activeMapPoints, keysLeft, matchedIdxsB, estimPose, MPsOutliers, true);
        // Logging("3","",3);

        // for (size_t i{0}, end{matchedIdxsB.size()}; i < end; i++)
        // {
        //     if ( activeMapPoints[i]->GetIsOutlier() && matchedIdxsB[i] >= 0)
        //     {
        //         matchedIdxsN[matchedIdxsB[i]] = -1;
        //         matchedIdxsB[i] = -1;
        //     }
        // }

        for (size_t i{0}, end{matchedIdxsB.size()}; i < end; i++)
        {
            if ( matchedIdxsB[i] < 0 )
                continue;
            MapPoint* mp = activeMapPoints[i];
            if ( mp->GetIsOutlier() )
            {
                matchedIdxsN[matchedIdxsB[i]] = -1;
                matchedIdxsB[i] = -1;
            }
        }

        // after last refine check all matches, change outliers to inliers if they are no more, and in the end remove all outliers from vector. they are already saved on mappoints.

        // the outliers from first refine are not used on the next refines.

        // check for big displacement, if there is, use constant velocity model
        
        // Timer lel("after first refine");
        // Logging("similarity", similarity,3);

        std::vector<cv::KeyPoint> projectedPoints, projectedPointsfromang, realPrevKeys;
        worldToImg(activeMapPoints, projectedPoints, estimPose);
        worldToImg(activeMapPoints, realPrevKeys, zedPtr->cameraPose.poseInverse);
        std::vector<cv::KeyPoint> prevKeyPos;

        calculatePrevKeyPos(activeMapPoints, prevKeyPos, zedPtr->cameraPose.poseInverse, estimPose);
        
        std::vector<float> mapAngles(activeMapPoints.size(), -5.0);
        // worldToImgAng(activeMapPoints, mapAngles, estimPose,prevKeyPos, projectedPointsfromang);
        if ( similarity < noMovementCheck )
        {
            calcAngles(activeMapPoints, projectedPoints, prevKeyPos, mapAngles);
            checkPrevAngles(mapAngles, prevKeyPos, matchedIdxsN, matchedIdxsB, keysLeft);
        }
        int nNewMatches = fm.matchByProjectionPredWA(activeMapPoints, projectedPoints, realPrevKeys, keysLeft, matchedIdxsN, matchedIdxsB, 10, mapAngles);
        // Logging("4","",3);
#if DRAWMATCHES
        std::vector<cv::Point2f> prevpnts, projpnts, matchedpnts;
        for ( size_t i {0}, end{activeMapPoints.size()}; i < end; i++)
        {
            if ( projectedPoints[i].pt.x > 0)
            {
                if ( matchedIdxsB[i] >= 0 && !activeMapPoints[i]->GetIsOutlier())
                {
                    prevpnts.emplace_back(prevKeyPos[i].pt);
                    projpnts.emplace_back(projectedPoints[i].pt);
                    matchedpnts.emplace_back(keysLeft.keyPoints[matchedIdxsB[i]].pt);
                }
            }
        }
        draw3PointsTemp<cv::Point2f,cv::Point2f,cv::Point2f>("proj, matched",lIm.rIm,prevpnts,projpnts,matchedpnts);

        std::vector<cv::Point2f> prevpnts2, projpnts2, matchedpnts2;
        for ( size_t i {0}, end{activeMapPoints.size()}; i < end; i++)
        {
            if ( projectedPoints[i].pt.x > 0)
            {
                if ( matchedIdxsB[i] >= 0 && !activeMapPoints[i]->GetIsOutlier())
                {
                    prevpnts2.emplace_back(realPrevKeys[i].pt);
                    projpnts2.emplace_back(projectedPoints[i].pt);
                    matchedpnts2.emplace_back(keysLeft.keyPoints[matchedIdxsB[i]].pt);
                }
            }
        }
        draw3PointsTemp<cv::Point2f,cv::Point2f,cv::Point2f>("realPrevKeys, matched",lIm.rIm,prevpnts2,projpnts2,matchedpnts2);
        cv::waitKey(1);
#endif
        // drawPointsTemp<cv::Point2f>("projected matches", lIm.rIm, mpnts, pnts2f);

        // std::vector<cv::Point2f> Nmpnts, Npnts2f;
        // for ( size_t i {0}, end{activeMapPoints.size()}; i < end; i++)
        // {
        //     if ( matchedIdxsB[i] >= 0 && !activeMapPoints[i]->GetIsOutlier() )
        //     {
        //         if ( prevKeyPos[i].pt.x > 0)
        //         {
        //             Nmpnts.emplace_back(prevKeyPos[i].pt);
        //             Npnts2f.emplace_back(keysLeft.keyPoints[matchedIdxsB[i]].pt);
        //         }
        //     }
        // }
        // drawPointsTemp<cv::Point2f>("real matches", lIm.rIm, Nmpnts, Npnts2f);
        // cv::waitKey(0);

        // {
        // std::vector<cv::KeyPoint>activeKeys;
        // worldToImg(activeMapPoints, activeKeys, zedPtr->cameraPose.poseInverse);
        // checkWithFund(activeKeys, keysLeft.keyPoints, matchedIdxsB, matchedIdxsN);
        // }



        std::pair<int,int> nStIn = estimatePoseCeres(activeMapPoints, keysLeft, matchedIdxsB, estimPose, MPsOutliers, true);
        cv::waitKey(1);
        std::vector<cv::KeyPoint> trckK;
        trckK.reserve(matchedIdxsB.size());
        for (size_t i{0}, end{matchedIdxsB.size()}; i < end; i++)
        {
            if ( matchedIdxsB[i] < 0 )
                continue;
            MapPoint* mp = activeMapPoints[i];
            if ( mp->GetIsOutlier() )
            {
                matchedIdxsN[matchedIdxsB[i]] = -1;
                matchedIdxsB[i] = -1;
            }
            else
                trckK.emplace_back(realPrevKeys[i]);
        }
#if !DRAWMATCHES
        drawKeys("TrackedKeys", lIm.rIm,trckK);
#endif
        // cv::waitKey(0);
        // Logging("second refine pose", estimPose,3);

        // Logging("nIn", nStIn.first,3);
        // Logging("nStereo", nStIn.second,3);

        // Timer lel2("after second refine");

        poseEst = estimPose.inverse();

        Eigen::Matrix4d poseDif = poseEst * zedPtr->cameraPose.poseInverse;

        Eigen::Matrix3d Rdif = poseDif.block<3,3>(0,0);
        Eigen::Matrix<double,3,1> tdif =  poseDif.block<3,1>(0,3);

        Sophus::SE3<double> se3t(Rdif, tdif);
        // Sophus::SE3;

        Sophus::Vector6d prevDisp = displacement;

        displacement = se3t.log();
        // std::cout << "dif displacement " << displacement - prevDisp << std::endl;
        // std::cout << "displacement " << displacement << std::endl;
        // std::cout << "prevDisp " << prevDisp << std::endl;

        // kalmanF(poseDif);

        // poseEst = poseDif * zedPtr->cameraPose.pose;
        // estimPose = poseEst.inverse();

        // get percentage of displacement. keep the previous one and calculate the percentage.
        const int nMono = nStIn.first - nStIn.second;
        keyFrameInsert++;
        if ( nStIn.second < minNStereo)
        {
            keyFrameInsert = 0;
            insertKeyFrame(keysLeft, matchedIdxsN, nStIn.second, nMono, poseEst);
            Logging("New KeyFrame!","",3);
        }
        else
        {
            
            // Logging("similarity", similarity,3);
            if ( similarity < imageDifThres && keyFrameInsert >= maxKeyFrameDist )
            {
                keyFrameInsert = 0;
                insertKeyFrame(keysLeft, matchedIdxsN, nStIn.second, nMono, poseEst);
                Logging("New KeyFrame!","",3);
            }
            else
            {
                addFrame(keysLeft, matchedIdxsN, nStIn.second, poseEst);
                // insertKeyFrame(keysLeft, matchedIdxsN, nStIn.second, poseEst);
                // Logging("New Frame!","",3);
            }
        }
        // Logging("5","",3);

        std::vector<bool> toRemove;
        removeMapPointOut(activeMapPoints, estimPose, toRemove);
        // removeMapPointOutBackUp(activeMapPoints, estimPose);
        // removeMapPoints(activeMapPoints, toRemove);

        publishPoseNew();
        // addKeyFrame(keysLeft, matchedIdxsN, nStIn.second);

        // Logging("6","",3);


        setPreLImage();
        setPreRImage();



        

    }

    saveData();

    datafile.close();
    map->endOfFrames = true;
}

void FeatureTracker::removeOutOfFrameMPs(const Eigen::Matrix4d& prevCameraPose, std::vector<MapPoint*>& activeMapPoints)
{
    const size_t end{activeMapPoints.size()};
    Eigen::Matrix4d toCamera = prevCameraPose.inverse();
    int j {0};
    std::lock_guard<std::mutex> lock(map->mapMutex);
    for ( size_t i {0}; i < end; i++)
    {
        MapPoint* mp = activeMapPoints[i];
        if ( !mp )
            continue;
        if (worldToFrame(mp, toCamera) && !mp->GetIsOutlier())
        {
            mp->seenCnt++;
            mp->setActive(true);
        }
        else
        {
            mp->setActive(false);
            continue;
        }
        activeMapPoints[j++] = mp;
    }
    activeMapPoints.resize(j);
}

void FeatureTracker::removeOutOfFrameMPsRB(const Zed_Camera* zedCam, const Eigen::Matrix4d& predNPose, std::vector<MapPoint*>& activeMapPoints)
{
    const size_t end{activeMapPoints.size()};
    Eigen::Matrix4d toRCamera = (predNPose * zedCam->extrinsics).inverse();
    Eigen::Matrix4d toCamera = predNPose.inverse();
    int j {0};
    for ( size_t i {0}; i < end; i++)
    {
        MapPoint* mp = activeMapPoints[i];
        if ( !mp )
            continue;
        bool c1 = worldToFrameRTrackB(mp, zedCam, false, toCamera);
        bool c2 = worldToFrameRTrackB(mp, zedCam, true, toRCamera);
        if (c1 && c2 && !mp->GetIsOutlier())
        {
            mp->seenCnt++;
            mp->setActive(true);
        }
        else
        {
            mp->setActive(false);
            continue;
        }
        activeMapPoints[j++] = mp;
    }
    activeMapPoints.resize(j);
}

void FeatureTracker::removeOutOfFrameMPsR(const Eigen::Matrix4d& currCamPose, const Eigen::Matrix4d& predNPose, std::vector<MapPoint*>& activeMapPoints)
{
    const size_t end{activeMapPoints.size()};
    Eigen::Matrix4d toRCamera = (predNPose * zedPtr->extrinsics).inverse();
    Eigen::Matrix4d toCamera = predNPose.inverse();
    int j {0};
    Eigen::Matrix4d temp = currCamPose.inverse() * predNPose;
    Eigen::Matrix4d tempR = currCamPose.inverse() * (predNPose * zedPtr->extrinsics);
    // std::lock_guard<std::mutex> lock(map->mapMutex);
    for ( size_t i {0}; i < end; i++)
    {
        MapPoint* mp = activeMapPoints[i];
        if ( !mp )
            continue;
        bool c1 = worldToFrameRTrack(mp, false, toCamera, temp);
        bool c2 = worldToFrameRTrack(mp, true, toRCamera, tempR);
        if (c1 && c2 && !mp->GetIsOutlier())
        {
            mp->seenCnt++;
            mp->setActive(true);
        }
        else
        {
            mp->setActive(false);
            continue;
        }
        activeMapPoints[j++] = mp;
    }
    activeMapPoints.resize(j);
}

void FeatureTracker::newPredictMPs(const Eigen::Matrix4d& currCamPose, const Eigen::Matrix4d& predNPose, std::vector<MapPoint*>& activeMapPoints, std::vector<int>& matchedIdxsL, std::vector<int>& matchedIdxsR, std::vector<std::pair<int,int>>& matchesIdxs, std::vector<bool> &MPsOutliers)
{
    const size_t end{activeMapPoints.size()};
    Eigen::Matrix4d toRCamera = (predNPose * zedPtr->extrinsics).inverse();
    Eigen::Matrix4d toCamera = predNPose.inverse();
    int j {0};
    Eigen::Matrix4d temp = currCamPose.inverse() * predNPose;
    Eigen::Matrix4d tempR = currCamPose.inverse() * (predNPose * zedPtr->extrinsics);
    std::lock_guard<std::mutex> lock(map->mapMutex);
    for ( size_t i {0}; i < end; i++)
    {
        MapPoint* mp = activeMapPoints[i];
        if ( !mp )
            continue;
        std::pair<int,int>& keyPos = matchesIdxs[i];
        if (!worldToFrameRTrack(mp, false, toCamera, temp))
        {
            if ( keyPos.first >=0 )
            {
                matchedIdxsL[keyPos.first] = -1;
                keyPos.first = -1;
            }
        }
        if (!worldToFrameRTrack(mp, true, toRCamera, tempR))
        {
            if ( keyPos.second >= 0 )
            {
                matchedIdxsR[keyPos.second] = -1;
                keyPos.second = -1;
            }
        }
        // if ( keyPos.first < 0 && keyPos.second < 0 )
        //     continue;
        if ( MPsOutliers[i] )
        {
            MPsOutliers[i] = false;
            if ( keyPos.first >=0 )
            {
                matchedIdxsL[keyPos.first] = -1;
                keyPos.first = -1;
            }
            if ( keyPos.second >= 0 )
            {
                matchedIdxsR[keyPos.second] = -1;
                keyPos.second = -1;
            }
        }
    }
}

void FeatureTracker::newPredictMPsB(const Zed_Camera* zedCam, const Eigen::Matrix4d& predNPose, std::vector<MapPoint*>& activeMapPoints, std::vector<int>& matchedIdxsL, std::vector<int>& matchedIdxsR, std::vector<std::pair<int,int>>& matchesIdxs, std::vector<bool> &MPsOutliers)
{
    const size_t end{activeMapPoints.size()};
    Eigen::Matrix4d toRCamera = (predNPose * zedCam->extrinsics).inverse();
    Eigen::Matrix4d toCamera = predNPose.inverse();
    int j {0};
    std::lock_guard<std::mutex> lock(map->mapMutex);
    for ( size_t i {0}; i < end; i++)
    {
        MapPoint* mp = activeMapPoints[i];
        if ( !mp )
            continue;
        std::pair<int,int>& keyPos = matchesIdxs[i];
        if (!worldToFrameRTrackB(mp, zedCam, false, toCamera))
        {
            if ( keyPos.first >=0 )
            {
                matchedIdxsL[keyPos.first] = -1;
                keyPos.first = -1;
            }
        }
        if (!worldToFrameRTrackB(mp, zedCam, true, toRCamera))
        {
            if ( keyPos.second >= 0 )
            {
                matchedIdxsR[keyPos.second] = -1;
                keyPos.second = -1;
            }
        }
        // if ( keyPos.first < 0 && keyPos.second < 0 )
        //     continue;
        if ( MPsOutliers[i] )
        {
            MPsOutliers[i] = false;
            if ( keyPos.first >=0 )
            {
                matchedIdxsL[keyPos.first] = -1;
                keyPos.first = -1;
            }
            if ( keyPos.second >= 0 )
            {
                matchedIdxsR[keyPos.second] = -1;
                keyPos.second = -1;
            }
        }
    }
}

Eigen::Matrix4d FeatureTracker::TrackImage(const cv::Mat& leftRect, const cv::Mat& rightRect, const Eigen::Matrix4d& prevCameraPose, const Eigen::Matrix4d& predPoseInv, std::vector<MapPoint*>& activeMpsTemp, std::vector<bool>& MPsOutliers, std::vector<bool>& MPsMatches, const int frameNumb, bool& newKF)
{
    const int imageH {zedPtr->mHeight};
    const int imageW {zedPtr->mWidth};
    const int numbOfPixels {zedPtr->mHeight * zedPtr->mWidth};
    curFrame = frameNumb;
    curFrameNumb++;
    predNPoseInv = predPoseInv;

    // if ( map->keyFrameAdded )
    // {
    //     lastKFPose = prevCameraPose;
    //     lastKFPoseInv = prevCameraPose.inverse();
    //     lastKFImage = pLIm.im.clone();
    // }
    
    if ( map->LBADone )
    {
        std::lock_guard<std::mutex> lock(map->mapMutex);
        if ( map->activeKeyFrames.size() > actvKFMaxSize )
        {
            // removeKeyFrame(activeKeyFrames);
            for ( size_t i{ actvKFMaxSize }, end{map->activeKeyFrames.size()}; i < end; i++)
            {
                map->activeKeyFrames[i]->active = false;
            }
            activeKeyFrames.resize(actvKFMaxSize);
        }
        map->LBADone = false;
    }
    // setLRImages(frameNumb);

    lIm.rIm = leftRect.clone();
    rIm.rIm = rightRect.clone();
    // cv::imshow("color", lIm.rIm);
    cv::cvtColor(lIm.rIm, lIm.im, cv::COLOR_BGR2GRAY);
    cv::cvtColor(rIm.rIm, rIm.im, cv::COLOR_BGR2GRAY);
    // cv::imshow("gray", lIm.im);
    TrackedKeys keysLeft;


    if ( curFrameNumb == 0 )
    {
        extractORBStereoMatch(lIm.im, rIm.im, keysLeft);

        initializeMap(keysLeft);
        // addMapPnts(keysLeft);
        setPreLImage();
        setPreRImage();
        return Eigen::Matrix4d::Identity();
    }

    removeOutOfFrameMPs(prevCameraPose, activeMapPoints);

    extractORBStereoMatch(lIm.im, rIm.im, keysLeft);
    // Timer lel3("after extract");
    // Logging("2","",3);

    // Eigen::Matrix4d currPose = predNPoseInv;
    // Eigen::Matrix4d prevPose = zedPtr->cameraPose.poseInverse;
    std::vector<int> matchedIdxsN(keysLeft.keyPoints.size(), -1);
    activeMpsTemp = activeMapPoints;
    MPsOutliers = std::vector<bool>(activeMpsTemp.size(),false);
    MPsMatches = std::vector<bool>(activeMpsTemp.size(),false);
    // std::lock_guard<std::mutex> lock(map->mapMutex);
    std::vector<int> matchedIdxsB(activeMpsTemp.size(), -1);
    
    
    const double dif = cv::norm(lastKFImage,lIm.im, cv::NORM_L2);
    const double similarity = 1 - dif/(numbOfPixels);


    if ( curFrameNumb == 1 )
        int nMatches = fm.matchByProjection(activeMpsTemp, keysLeft, matchedIdxsN, matchedIdxsB);
    else
    {
        std::vector<cv::KeyPoint> ConVelPoints;
        worldToImg(activeMpsTemp, ConVelPoints, predNPoseInv);
        std::vector<float> mapAngles(activeMpsTemp.size(), -5.0);
        std::vector<cv::KeyPoint> prevKeyPos;
        drawKeys("convelPoints", lIm.rIm, ConVelPoints);
        // worldToImg(activeMapPoints, prevKeyPos, zedPtr->cameraPose.poseInverse);
        calculatePrevKeyPos(activeMpsTemp, prevKeyPos, zedPtr->cameraPose.poseInverse, predNPoseInv);
        // if ( similarity < noMovementCheck )
        calcAngles(activeMpsTemp, ConVelPoints, prevKeyPos, mapAngles);

        int nNewMatches = fm.matchByProjectionConVelAng(activeMpsTemp, ConVelPoints, prevKeyPos, keysLeft, matchedIdxsN, matchedIdxsB, 2, mapAngles);
        // int nIn;
        // std::vector<float> weights;
        // // calcWeights(prePnts, weights);
        // weights.resize(activeMapPoints.size(), 1.0f);
        // OutliersReprojErr(predNPoseInv, activeMapPoints, keysLeft, matchedIdxsB, 20.0, weights, nIn);

        // for (size_t i{0}, end{matchedIdxsB.size()}; i < end; i++)
        // {
        //     if ( matchedIdxsB[i] < 0 )
        //         continue;
        //     MapPoint* mp = activeMapPoints[i];
        //     if ( mp->GetIsOutlier() )
        //     {
        //         matchedIdxsN[matchedIdxsB[i]] = -1;
        //         matchedIdxsB[i] = -1;
        //     }
        // }
    }

    // {
    // std::vector<cv::KeyPoint>activeKeys;
    // worldToImg(activeMapPoints, activeKeys, zedPtr->cameraPose.poseInverse);
    // checkWithFund(activeKeys, keysLeft.keyPoints, matchedIdxsB, matchedIdxsN);
    // }

    Eigen::Matrix4d estimPose = predNPoseInv;
    estimatePoseCeres(activeMpsTemp, keysLeft, matchedIdxsB, estimPose, MPsOutliers, true);
    // Logging("3","",3);

    // for (size_t i{0}, end{matchedIdxsB.size()}; i < end; i++)
    // {
    //     if ( activeMapPoints[i]->GetIsOutlier() && matchedIdxsB[i] >= 0)
    //     {
    //         matchedIdxsN[matchedIdxsB[i]] = -1;
    //         matchedIdxsB[i] = -1;
    //     }
    // }

    for (size_t i{0}, end{matchedIdxsB.size()}; i < end; i++)
    {
        if ( matchedIdxsB[i] < 0 )
            continue;
        MapPoint* mp = activeMpsTemp[i];
        if ( mp->GetIsOutlier() )
        {
            matchedIdxsN[matchedIdxsB[i]] = -1;
            matchedIdxsB[i] = -1;
        }
    }


    // after last refine check all matches, change outliers to inliers if they are no more, and in the end remove all outliers from vector. they are already saved on mappoints.

    // the outliers from first refine are not used on the next refines.

    // check for big displacement, if there is, use constant velocity model
    
    // Timer lel("after first refine");
    // Logging("similarity", similarity,3);

    std::vector<cv::KeyPoint> projectedPoints, projectedPointsfromang, realPrevKeys;
    worldToImg(activeMpsTemp, projectedPoints, estimPose);
    worldToImg(activeMpsTemp, realPrevKeys, zedPtr->cameraPose.poseInverse);
// #if DRAWMATCHES
    std::vector<cv::Point2f> Nmpnts, Npnts2f;
    for ( size_t i {0}, end{activeMpsTemp.size()}; i < end; i++)
    {
        if ( matchedIdxsB[i] >= 0 && !activeMpsTemp[i]->GetIsOutlier() )
        {
            if ( realPrevKeys[i].pt.x > 0)
            {
                Nmpnts.emplace_back(realPrevKeys[i].pt);
                Npnts2f.emplace_back(keysLeft.keyPoints[matchedIdxsB[i]].pt);
            }
        }
    }
    drawPointsTemp<cv::Point2f>("first estimated matches", lIm.rIm, Nmpnts, Npnts2f);
    cv::waitKey(1);
// #endif
    std::vector<cv::KeyPoint> prevKeyPos;

    calculatePrevKeyPos(activeMpsTemp, prevKeyPos, zedPtr->cameraPose.poseInverse, estimPose);
    
    std::vector<float> mapAngles(activeMpsTemp.size(), -5.0);
    // worldToImgAng(activeMapPoints, mapAngles, estimPose,prevKeyPos, projectedPointsfromang);
    if ( similarity < noMovementCheck )
    {
        calcAngles(activeMpsTemp, projectedPoints, prevKeyPos, mapAngles);
        checkPrevAngles(mapAngles, prevKeyPos, matchedIdxsN, matchedIdxsB, keysLeft);
    }
    int nNewMatches = fm.matchByProjectionPredWA(activeMpsTemp, projectedPoints, realPrevKeys, keysLeft, matchedIdxsN, matchedIdxsB, 10, mapAngles);
    // Logging("4","",3);
#if DRAWMATCHES
    std::vector<cv::Point2f> prevpnts, projpnts, matchedpnts;
    for ( size_t i {0}, end{activeMpsTemp.size()}; i < end; i++)
    {
        if ( projectedPoints[i].pt.x > 0)
        {
            if ( matchedIdxsB[i] >= 0 && !activeMpsTemp[i]->GetIsOutlier())
            {
                prevpnts.emplace_back(prevKeyPos[i].pt);
                projpnts.emplace_back(projectedPoints[i].pt);
                matchedpnts.emplace_back(keysLeft.keyPoints[matchedIdxsB[i]].pt);
            }
        }
    }
    draw3PointsTemp<cv::Point2f,cv::Point2f,cv::Point2f>("proj, matched",lIm.rIm,prevpnts,projpnts,matchedpnts);

    std::vector<cv::Point2f> prevpnts2, projpnts2, matchedpnts2;
    for ( size_t i {0}, end{activeMpsTemp.size()}; i < end; i++)
    {
        if ( projectedPoints[i].pt.x > 0)
        {
            if ( matchedIdxsB[i] >= 0 && !activeMpsTemp[i]->GetIsOutlier())
            {
                prevpnts2.emplace_back(realPrevKeys[i].pt);
                projpnts2.emplace_back(projectedPoints[i].pt);
                matchedpnts2.emplace_back(keysLeft.keyPoints[matchedIdxsB[i]].pt);
            }
        }
    }
    draw3PointsTemp<cv::Point2f,cv::Point2f,cv::Point2f>("realPrevKeys, matched",lIm.rIm,prevpnts2,projpnts2,matchedpnts2);
    cv::waitKey(1);
#endif
    // drawPointsTemp<cv::Point2f>("projected matches", lIm.rIm, mpnts, pnts2f);

    // std::vector<cv::Point2f> Nmpnts, Npnts2f;
    // for ( size_t i {0}, end{activeMapPoints.size()}; i < end; i++)
    // {
    //     if ( matchedIdxsB[i] >= 0 && !activeMapPoints[i]->GetIsOutlier() )
    //     {
    //         if ( prevKeyPos[i].pt.x > 0)
    //         {
    //             Nmpnts.emplace_back(prevKeyPos[i].pt);
    //             Npnts2f.emplace_back(keysLeft.keyPoints[matchedIdxsB[i]].pt);
    //         }
    //     }
    // }
    // drawPointsTemp<cv::Point2f>("real matches", lIm.rIm, Nmpnts, Npnts2f);
    // cv::waitKey(0);

    // {
    // std::vector<cv::KeyPoint>activeKeys;
    // worldToImg(activeMapPoints, activeKeys, zedPtr->cameraPose.poseInverse);
    // checkWithFund(activeKeys, keysLeft.keyPoints, matchedIdxsB, matchedIdxsN);
    // }



    std::pair<int,int> nStIn = estimatePoseCeres(activeMpsTemp, keysLeft, matchedIdxsB, estimPose, MPsOutliers, true);
    cv::waitKey(1);
    std::vector<cv::KeyPoint> trckK;
    trckK.reserve(matchedIdxsB.size());
    for (size_t i{0}, end{matchedIdxsB.size()}; i < end; i++)
    {
        MapPoint* mp = activeMpsTemp[i];
        if ( matchedIdxsB[i] < 0 )
        {
            continue;
        }
        if ( mp->GetIsOutlier() )
        {
            matchedIdxsN[matchedIdxsB[i]] = -1;
            matchedIdxsB[i] = -1;
        }
        else
        {
            MPsMatches[i] = true;
            trckK.emplace_back(keysLeft.keyPoints[matchedIdxsB[i]]);
        }
    }
#if !DRAWMATCHES
    drawKeys("TrackedKeys", lIm.rIm,trckK);
#endif
// #if DRAWMATCHES
    std::vector<cv::Point2f> Nmpnts2, Npnts2f2;
    for ( size_t i {0}, end{activeMpsTemp.size()}; i < end; i++)
    {
        if ( matchedIdxsB[i] >= 0 )
        {
            if ( realPrevKeys[i].pt.x > 0)
            {
                Nmpnts2.emplace_back(realPrevKeys[i].pt);
                Npnts2f2.emplace_back(keysLeft.keyPoints[matchedIdxsB[i]].pt);
            }
        }
    }
    drawPointsTemp<cv::Point2f>("second estimated matches", lIm.rIm, Nmpnts2, Npnts2f2);
    cv::waitKey(1);
// #endif
    // cv::waitKey(0);
    // Logging("second refine pose", estimPose,3);

    // Logging("nIn", nStIn.first,3);
    // Logging("nStereo", nStIn.second,3);

    // Timer lel2("after second refine");

    poseEst = estimPose.inverse();

    Eigen::Matrix4d poseDif = poseEst * zedPtr->cameraPose.poseInverse;

    Eigen::Matrix3d Rdif = poseDif.block<3,3>(0,0);
    Eigen::Matrix<double,3,1> tdif =  poseDif.block<3,1>(0,3);

    Sophus::SE3<double> se3t(Rdif, tdif);
    // Sophus::SE3;

    Sophus::Vector6d prevDisp = displacement;

    displacement = se3t.log();
    // std::cout << "dif displacement " << displacement - prevDisp << std::endl;
    // std::cout << "displacement " << displacement << std::endl;
    // std::cout << "prevDisp " << prevDisp << std::endl;

    // kalmanF(poseDif);

    // poseEst = poseDif * zedPtr->cameraPose.pose;
    // estimPose = poseEst.inverse();

    // get percentage of displacement. keep the previous one and calculate the percentage.
    insertKeyFrameCount ++;
    const int nMono = nStIn.first - nStIn.second;
    // Logging("inliers", nStIn.first,3);
    // Logging("stereo", nStIn.second,3);
    // Logging("lastKFTrackedNumb", 0.9* lastKFTrackedNumb,3);
    // KeyFrame* kFCand;
    if ( nStIn.second < minNStereo || (nStIn.first < 0.9 * lastKFTrackedNumb && insertKeyFrameCount >= keyFrameCountEnd))
    {
        newKF = true;
        insertKeyFrameCount = 0;
        insertKeyFrame(keysLeft, matchedIdxsN, nStIn.second, nMono, poseEst);
        Logging("New KeyFrame!","",3);
        Logging("nStIn.first!",nStIn.first,3);
        Logging("0.9 * lastKFTrackedNumb!",0.9 * lastKFTrackedNumb,3);
        Logging("nStIn.second",nStIn.second,3);
    }
    else
    {
        // kFCand = insertKeyFrameOut(keysLeft, poseEst);
        addFrame(keysLeft, matchedIdxsN, nStIn.second, poseEst);
    }

    setPreLImage();
    setPreRImage();



    if ( frameNumb == zedPtr->numOfFrames - 1)

    {
        saveData();

        datafile.close();
        map->endOfFrames = true;
    }

    return poseEst;
}

void FeatureTracker::initialization(cv::Mat& leftIm, cv::Mat& rightIm, TrackedKeys& keysLeft)
{
    // for back camera 2 threads for extraction and initialize map on the main thread because the initialization adds mappoints on both activeMPs and activeMPsB
    // insertKF is from both cameras on the main thread
    extractORBStereoMatchR(leftIm, rightIm, keysLeft);

    initializeMapR(keysLeft);
}

void FeatureTracker::setActiveOutliers(std::vector<MapPoint*>& activeMPs, std::vector<bool>& MPsOutliers, std::vector<bool>& MPsMatches)
{
    std::lock_guard<std::mutex> lock(map->mapMutex);
    for ( size_t i{0}, end{MPsOutliers.size()}; i < end; i++)
    {
        MapPoint*& mp = activeMPs[i];
        if ( MPsMatches[i] )
        {
            mp->unMCnt = 0;
            mp->outLCnt = 0;
        }
        else
            mp->unMCnt++;

        if ( MPsOutliers[i] )
            mp->outLCnt ++;
        
        // if ( mp->outLCnt < 2 && mp->unMCnt < 20 )
        // {
        //     continue;
        // }
        if ( !MPsOutliers[i] && mp->unMCnt < 20 )
        {
            continue;
        }
        mp->SetIsOutlier( true );
    }
}

void FeatureTracker::TrackImageT(const cv::Mat& leftRect, const cv::Mat& rightRect, const int frameNumb)
{
    curFrame = frameNumb;
    curFrameNumb++;
    
    if ( map->LBADone )
    {
        std::lock_guard<std::mutex> lock(map->mapMutex);
        changePosesLBA();
        if ( map->activeKeyFrames.size() > actvKFMaxSize )
        {
            // removeKeyFrame(activeKeyFrames);
            for ( size_t i{ actvKFMaxSize }, end{map->activeKeyFrames.size()}; i < end; i++)
            {
                map->activeKeyFrames[i]->active = false;
            }
            activeKeyFrames.resize(actvKFMaxSize);
        }
        map->LBADone = false;
    }
    // setLRImages(frameNumb);

    cv::Mat realLeftIm, realRightIm;
    cv::Mat leftIm, rightIm;

    realLeftIm = leftRect;
    realRightIm = rightRect;

    if(realLeftIm.channels()==3)
    {
        cvtColor(realLeftIm,leftIm,cv::COLOR_BGR2GRAY);
        cvtColor(realRightIm,rightIm,cv::COLOR_BGR2GRAY);
    }
    else if(realLeftIm.channels()==4)
    {
        cvtColor(realLeftIm,leftIm,cv::COLOR_BGRA2GRAY);
        cvtColor(realRightIm,rightIm,cv::COLOR_BGRA2GRAY);
    }
    else
    {
        leftIm = realLeftIm.clone();
        rightIm = realRightIm.clone();
    }
    
    TrackedKeys keysLeft;


    if ( curFrameNumb == 0 )
    {
        extractORBStereoMatchR(leftIm, rightIm, keysLeft);

        initializeMapR(keysLeft);

        return;
    }
    std::vector<vio_slam::MapPoint *> activeMpsTemp;
    {
    std::lock_guard<std::mutex> lock(map->mapMutex);
    removeOutOfFrameMPsR(zedPtr->cameraPose.pose, predNPose, activeMapPoints);
    activeMpsTemp = activeMapPoints;
    }

    

    extractORBStereoMatchR(leftIm, rightIm, keysLeft);

    std::vector<int> matchedIdxsL(keysLeft.keyPoints.size(), -1);
    std::vector<int> matchedIdxsR(keysLeft.rightKeyPoints.size(), -1);
    std::vector<std::pair<int,int>> matchesIdxs(activeMpsTemp.size(), std::make_pair(-1,-1));
    
    std::vector<bool> MPsOutliers(activeMpsTemp.size(),false);
    std::vector<bool> MPsMatches(activeMpsTemp.size(),false);

    Eigen::Matrix4d estimPose = predNPoseInv;

    float rad {10.0};

    if ( curFrameNumb == 1 )
        rad = 120;
    else
        rad = 10;

    std::pair<int,int> nIn(-1,-1);
    int prevIn = -1;
    float prevrad = rad;
    bool toBreak {false};
    while ( nIn.first < minInliers )
    {
        int nMatches = fm.matchByProjectionRPred(activeMpsTemp, keysLeft, matchedIdxsL, matchedIdxsR, matchesIdxs, rad, true);

        nIn = estimatePoseCeresR(activeMpsTemp, keysLeft, matchesIdxs, estimPose, MPsOutliers, true);

        if ( nIn.first < minInliers  && !toBreak )
        {
            estimPose = predNPoseInv;
            matchedIdxsL = std::vector<int>(keysLeft.keyPoints.size(), -1);
            matchedIdxsR = std::vector<int>(keysLeft.rightKeyPoints.size(), -1);
            MPsOutliers = std::vector<bool>(activeMpsTemp.size(),false);
            matchesIdxs = std::vector<std::pair<int,int>>(activeMpsTemp.size(), std::make_pair(-1,-1));
            if ( nIn.first < prevIn )
            {
                rad = prevrad;
                toBreak = true;
            }
            else
            {
                prevrad = rad;
                prevIn = nIn.first;
                rad += 30.0;
            }
        }
        else
            break;

    }

    newPredictMPs(zedPtr->cameraPose.pose, estimPose.inverse(), activeMpsTemp, matchedIdxsL, matchedIdxsR, matchesIdxs, MPsOutliers);

    rad = 4;
    int nMatches = fm.matchByProjectionRPred(activeMpsTemp, keysLeft, matchedIdxsL, matchedIdxsR, matchesIdxs, rad, true);

     std::pair<int,int> nStIn = estimatePoseCeresR(activeMpsTemp, keysLeft, matchesIdxs, estimPose, MPsOutliers, true);

    std::vector<cv::KeyPoint> lp;
    std::vector<bool> closeL;
    lp.reserve(matchesIdxs.size());
    closeL.reserve(matchesIdxs.size());
    for ( size_t i{0}; i < matchesIdxs.size(); i++)
    {
        const std::pair<int,int>& keyPos = matchesIdxs[i];
        if ( keyPos.first >= 0 )
        {
            lp.emplace_back(keysLeft.keyPoints[keyPos.first]);
            if ( MPsOutliers[i] )
                continue;
            if ( keysLeft.close[keyPos.first] )
                closeL.emplace_back(true);
            else
                closeL.emplace_back(false);
        }
    }
    drawKeys("left", realLeftIm, lp, closeL);
    cv::waitKey(1);

    poseEst = estimPose.inverse();

    for ( size_t i{0}, endmp {activeMpsTemp.size()}; i < endmp; i++)
    {
        std::pair<int,int>& keyPos = matchesIdxs[i];
        if ( (keyPos.first >= 0 || keyPos.second >= 0) && !MPsOutliers[i] )
            MPsMatches[i] = true;
    }

    insertKeyFrameCount ++;
    prevKF = activeKeyFrames.front();
    if ( (nStIn.second < minNStereo || insertKeyFrameCount >= keyFrameCountEnd) && nStIn.first < 0.9 * lastKFTrackedNumb )
    {
        insertKeyFrameCount = 0;
        insertKeyFrameR(keysLeft, matchedIdxsL,matchesIdxs, nStIn.second, poseEst, MPsOutliers, leftIm);
    }
    publishPoseNew();

    setActiveOutliers(activeMpsTemp,MPsOutliers, MPsMatches);



    if ( curFrameNumb == zedPtr->numOfFrames - 1)

    {
        saveData();

        datafile.close();
        map->endOfFrames = true;
    }


    // return std::make_pair(kFCand, poseEst);
}

void FeatureTracker::TrackImageTB(const cv::Mat& leftRect, const cv::Mat& rightRect, const cv::Mat& leftRectB, const cv::Mat& rightRectB, const int frameNumb)
{
    curFrame = frameNumb;
    curFrameNumb++;
    
    if ( map->LBADone )
    {
        std::lock_guard<std::mutex> lock(map->mapMutex);
        changePosesLBAB();
        if ( map->activeKeyFrames.size() > actvKFMaxSize )
        {
            // removeKeyFrame(activeKeyFrames);
            for ( size_t i{ actvKFMaxSize }, end{map->activeKeyFrames.size()}; i < end; i++)
            {
                map->activeKeyFrames[i]->active = false;
            }
            activeKeyFrames.resize(actvKFMaxSize);
        }
        map->LBADone = false;
    }
    // setLRImages(frameNumb);

    cv::Mat realLeftIm, realRightIm;
    cv::Mat leftIm, rightIm;

    cv::Mat realLeftImB, realRightImB;
    cv::Mat leftImB, rightImB;

    realLeftIm = leftRect.clone();
    realRightIm = rightRect.clone();

    realLeftImB = leftRectB.clone();
    realRightImB = rightRectB.clone();

    cv::cvtColor(realLeftIm, leftIm, cv::COLOR_BGR2GRAY);
    cv::cvtColor(realRightIm, rightIm, cv::COLOR_BGR2GRAY);

    cv::cvtColor(realLeftImB, leftImB, cv::COLOR_BGR2GRAY);
    cv::cvtColor(realRightImB, rightImB, cv::COLOR_BGR2GRAY);
    
    TrackedKeys keysLeft;
    TrackedKeys keysLeftB;


    if ( curFrameNumb == 0 )
    {

        std::thread front(&FeatureTracker::extractORBStereoMatchRB, this, std::ref(zedPtr), std::ref(leftIm), std::ref(rightIm), std::ref(feLeft), std::ref(feRight), std::ref(fm), std::ref(keysLeft));
        std::thread back(&FeatureTracker::extractORBStereoMatchRB, this, std::ref(zedPtrB), std::ref(leftImB), std::ref(rightImB), std::ref(feLeftB), std::ref(feRightB), std::ref(fmB), std::ref(keysLeftB));
        front.join();
        back.join();
        
        initializeMapRB(keysLeft, keysLeftB);

        return;
    }
    Eigen::Matrix4d predNPoseB = predNPose * zedPtr->TCamToCam;
    std::vector<vio_slam::MapPoint *> activeMpsTemp, activeMpsTempB;
    {
    std::lock_guard<std::mutex> lock(map->mapMutex);
    removeOutOfFrameMPsRB(zedPtr, predNPose, activeMapPoints);
    removeOutOfFrameMPsRB(zedPtrB, predNPoseB, activeMapPointsB);
    activeMpsTemp = activeMapPoints;
    activeMpsTempB = activeMapPointsB;
    }

    {
    std::thread front(&FeatureTracker::extractORBStereoMatchRB, this, std::ref(zedPtr), std::ref(leftIm), std::ref(rightIm), std::ref(feLeft), std::ref(feRight), std::ref(fm), std::ref(keysLeft));
    std::thread back(&FeatureTracker::extractORBStereoMatchRB, this, std::ref(zedPtrB), std::ref(leftImB), std::ref(rightImB), std::ref(feLeftB), std::ref(feRightB), std::ref(fmB), std::ref(keysLeftB));
    front.join();
    back.join();
    }
    std::vector<int> matchedIdxsL(keysLeft.keyPoints.size(), -1);
    std::vector<int> matchedIdxsR(keysLeft.rightKeyPoints.size(), -1);
    std::vector<std::pair<int,int>> matchesIdxs(activeMpsTemp.size(), std::make_pair(-1,-1));

    std::vector<int> matchedIdxsLB(keysLeftB.keyPoints.size(), -1);
    std::vector<int> matchedIdxsRB(keysLeftB.rightKeyPoints.size(), -1);
    std::vector<std::pair<int,int>> matchesIdxsB(activeMpsTempB.size(), std::make_pair(-1,-1));
    
    std::vector<bool> MPsOutliers(activeMpsTemp.size(),false);
    std::vector<bool> MPsMatches(activeMpsTemp.size(),false);

    std::vector<bool> MPsOutliersB(activeMpsTempB.size(),false);
    std::vector<bool> MPsMatchesB(activeMpsTempB.size(),false);

    Eigen::Matrix4d estimPose = predNPoseInv;

    float rad {10.0};
    float radB {10.0};

    if ( curFrameNumb == 1 )
    {
        rad = 120;
        radB = 120;
    }

    std::vector<std::pair<cv::Point2f,cv::Point2f>> prevPoints, prevPointsB;
    worldToImgR(activeMpsTemp, prevPoints, zedPtr->cameraPose.pose);
    worldToImgR(activeMpsTempB, prevPointsB, zedPtrB->cameraPose.pose);

    int prevIn = -1;
    int prevInB = -1;
    float prevrad = rad;
    float prevradB = radB;
    bool toBreak {false};
    bool toBreakB {false};
    {
    std::thread front(&FeatureMatcher::matchByProjectionRPred, fm, std::ref(activeMpsTemp), std::ref(keysLeft), std::ref(matchedIdxsL), std::ref(matchedIdxsR), std::ref(matchesIdxs), std::ref(rad), true);
    std::thread back(&FeatureMatcher::matchByProjectionRPred, fmB, std::ref(activeMpsTempB), std::ref(keysLeftB), std::ref(matchedIdxsLB), std::ref(matchedIdxsRB), std::ref(matchesIdxsB), std::ref(radB), true);
    front.join();
    back.join();
    }
    std::pair<std::pair<int,int>,std::pair<int,int>> both = estimatePoseCeresRB(activeMpsTemp, keysLeft, matchesIdxs, MPsOutliers, activeMpsTempB, keysLeftB, matchesIdxsB, MPsOutliersB, estimPose);
    while ( (both.first.first + both.second.first) < 2*minInliers )
    {

        if ( (both.first.first + both.second.first) < prevIn )
        {
            rad = prevrad;
            toBreak = true;
        }
        else
        {
            prevrad = rad;
            prevIn = both.first.first + both.second.first;
            rad += 30.0;
        }

        estimPose = predNPoseInv;
        matchedIdxsL = std::vector<int>(keysLeft.keyPoints.size(), -1);
        matchedIdxsR = std::vector<int>(keysLeft.rightKeyPoints.size(), -1);
        MPsOutliers = std::vector<bool>(activeMpsTemp.size(),false);
        matchesIdxs = std::vector<std::pair<int,int>>(activeMpsTemp.size(), std::make_pair(-1,-1));
        matchedIdxsLB = std::vector<int>(keysLeftB.keyPoints.size(), -1);
        matchedIdxsRB = std::vector<int>(keysLeftB.rightKeyPoints.size(), -1);
        MPsOutliersB = std::vector<bool>(activeMpsTempB.size(),false);
        matchesIdxsB = std::vector<std::pair<int,int>>(activeMpsTempB.size(), std::make_pair(-1,-1));

        {
        std::thread front(&FeatureMatcher::matchByProjectionRPred, fm, std::ref(activeMpsTemp), std::ref(keysLeft), std::ref(matchedIdxsL), std::ref(matchedIdxsR), std::ref(matchesIdxs), std::ref(rad), true);
        std::thread back(&FeatureMatcher::matchByProjectionRPred, fmB, std::ref(activeMpsTempB), std::ref(keysLeftB), std::ref(matchedIdxsLB), std::ref(matchedIdxsRB), std::ref(matchesIdxsB), std::ref(rad), true);
        front.join();
        back.join();
        }
        both = estimatePoseCeresRB(activeMpsTemp, keysLeft, matchesIdxs, MPsOutliers, activeMpsTempB, keysLeftB, matchesIdxsB, MPsOutliersB, estimPose);

        if ( toBreak )
            break;

        
        // if ( both.first.first > minInliers )
        // {
        //     matchedIdxsLB = std::vector<int>(keysLeftB.keyPoints.size(), -1);
        //     matchedIdxsRB = std::vector<int>(keysLeftB.rightKeyPoints.size(), -1);
        //     MPsOutliersB = std::vector<bool>(activeMpsTempB.size(),false);
        //     matchesIdxsB = std::vector<std::pair<int,int>>(activeMpsTempB.size(), std::make_pair(-1,-1));
        //     break;
        // }

        // if ( both.second.first > minInliers )
        // {
        //     matchedIdxsL = std::vector<int>(keysLeft.keyPoints.size(), -1);
        //     matchedIdxsR = std::vector<int>(keysLeft.rightKeyPoints.size(), -1);
        //     MPsOutliers = std::vector<bool>(activeMpsTemp.size(),false);
        //     matchesIdxs = std::vector<std::pair<int,int>>(activeMpsTemp.size(), std::make_pair(-1,-1));
        //     break;
        // }
        
        // if ( both.first.first < minInliers  && !toBreak )
        // {
        //     estimPose = predNPoseInv;
        //     matchedIdxsL = std::vector<int>(keysLeft.keyPoints.size(), -1);
        //     matchedIdxsR = std::vector<int>(keysLeft.rightKeyPoints.size(), -1);
        //     MPsOutliers = std::vector<bool>(activeMpsTemp.size(),false);
        //     matchesIdxs = std::vector<std::pair<int,int>>(activeMpsTemp.size(), std::make_pair(-1,-1));
        //     if ( both.first.first < prevIn )
        //     {
        //         rad = prevrad;
        //         toBreak = true;
        //     }
        //     else
        //     {
        //         prevrad = rad;
        //         prevIn = both.first.first;
        //         rad += 30.0;
        //     }
        //     fm.matchByProjectionRPred(activeMpsTemp, keysLeft, matchedIdxsL, matchedIdxsR, matchesIdxs, rad, true);
        // }
        // else
        //     toBreak = true;

        // if ( both.second.first < minInliers  && !toBreakB )
        // {
        //     estimPose = predNPoseInv;
        //     matchedIdxsLB = std::vector<int>(keysLeftB.keyPoints.size(), -1);
        //     matchedIdxsRB = std::vector<int>(keysLeftB.rightKeyPoints.size(), -1);
        //     MPsOutliersB = std::vector<bool>(activeMpsTempB.size(),false);
        //     matchesIdxsB = std::vector<std::pair<int,int>>(activeMpsTempB.size(), std::make_pair(-1,-1));
        //     if ( both.second.first < prevInB )
        //     {
        //         radB = prevradB;
        //         toBreakB = true;
        //     }
        //     else
        //     {
        //         prevradB = radB;
        //         prevInB = both.second.first;
        //         radB += 30.0;
        //     }
        //     fmB.matchByProjectionRPred(activeMpsTempB, keysLeftB, matchedIdxsLB, matchedIdxsRB, matchesIdxsB, radB, true);
        // }
        // else
        //     toBreakB = true;

        // std::vector<cv::KeyPoint> lp, rp;
        // std::vector<cv::KeyPoint> plp;
        // std::vector<bool> closeF, closeB;
        // for ( size_t i{0}; i < matchesIdxs.size(); i++)
        // {
        //     const std::pair<int,int>& keyPos = matchesIdxs[i];
        //     if ( MPsOutliers[i] )
        //         continue;
        //     if ( keyPos.first >= 0 )
        //     {
        //         lp.emplace_back(keysLeft.keyPoints[keyPos.first]);
        //         cv::KeyPoint pkp;
        //         pkp.pt = prevPoints[i].first;
        //         plp.emplace_back(pkp);
        //         if ( keysLeft.close[keyPos.first] )
        //             closeF.emplace_back(true);
        //         else
        //             closeF.emplace_back(false);
        //     }
        //     if ( keyPos.second >= 0 )
        //         rp.emplace_back(keysLeft.rightKeyPoints[keyPos.second]);
        // }
        // drawMatchesNew("leftMatches", realLeftIm, lp, plp);
        // // drawKeys("left", realLeftIm, lp, closeF);
        // drawKeys("right", realRightIm,rp);
        // cv::waitKey(1);
        // lp.clear();
        // rp.clear();
        // plp.clear();
        // for ( size_t i{0}; i < matchesIdxsB.size(); i++)
        // {
        //     const std::pair<int,int>& keyPos = matchesIdxsB[i];
        //     if ( MPsOutliersB[i] )
        //         continue;
        //     if ( keyPos.first >= 0 )
        //     {
        //         lp.emplace_back(keysLeftB.keyPoints[keyPos.first]);
        //         cv::KeyPoint pkp;
        //         pkp.pt = prevPointsB[i].first;
        //         plp.emplace_back(pkp);
        //         if ( keysLeftB.close[keyPos.first] )
        //             closeB.emplace_back(true);
        //         else
        //             closeB.emplace_back(false);
        //     }
        //     if ( keyPos.second >= 0 )
        //         rp.emplace_back(keysLeftB.rightKeyPoints[keyPos.second]);
        // }
        // drawMatchesNew("leftMatchesB", realLeftImB, lp, plp);
        // // drawKeys("leftB", realLeftImB, lp, closeB);
        // drawKeys("rightB", realRightImB,rp);
        // cv::waitKey(1);

        // both = estimatePoseCeresRB(activeMpsTemp, keysLeft, matchesIdxs, MPsOutliers, activeMpsTempB, keysLeftB, matchesIdxsB, MPsOutliersB, estimPose);
        // Logging("rad",rad,3);
        // Logging("radB",radB,3);

        // lp.clear();
        // rp.clear();
        // plp.clear();
        // closeB.clear();
        // closeF.clear();
        // for ( size_t i{0}; i < matchesIdxs.size(); i++)
        // {
        //     const std::pair<int,int>& keyPos = matchesIdxs[i];
        //     if ( MPsOutliers[i] )
        //         continue;
        //     if ( keyPos.first >= 0 )
        //     {
        //         lp.emplace_back(keysLeft.keyPoints[keyPos.first]);
        //         cv::KeyPoint pkp;
        //         pkp.pt = prevPoints[i].first;
        //         plp.emplace_back(pkp);
        //         if ( keysLeft.close[keyPos.first] )
        //             closeF.emplace_back(true);
        //         else
        //             closeF.emplace_back(false);
        //     }
        //     if ( keyPos.second >= 0 )
        //         rp.emplace_back(keysLeft.rightKeyPoints[keyPos.second]);
        // }
        // drawMatchesNew("leftMatches", realLeftIm, lp, plp);
        // // drawKeys("left", realLeftIm, lp, closeF);
        // drawKeys("right", realRightIm,rp);
        // cv::waitKey(1);
        // lp.clear();
        // rp.clear();
        // plp.clear();
        // for ( size_t i{0}; i < matchesIdxsB.size(); i++)
        // {
        //     const std::pair<int,int>& keyPos = matchesIdxsB[i];
        //     if ( MPsOutliersB[i] )
        //         continue;
        //     if ( keyPos.first >= 0 )
        //     {
        //         lp.emplace_back(keysLeftB.keyPoints[keyPos.first]);
        //         cv::KeyPoint pkp;
        //         pkp.pt = prevPointsB[i].first;
        //         plp.emplace_back(pkp);
        //         if ( keysLeftB.close[keyPos.first] )
        //             closeB.emplace_back(true);
        //         else
        //             closeB.emplace_back(false);
        //     }
        //     if ( keyPos.second >= 0 )
        //         rp.emplace_back(keysLeftB.rightKeyPoints[keyPos.second]);
        // }
        // drawMatchesNew("leftMatchesB", realLeftImB, lp, plp);
        // // drawKeys("leftB", realLeftImB, lp, closeB);
        // drawKeys("rightB", realRightImB,rp);
        // cv::waitKey(1);

        // if ( toBreak && toBreakB )
        //     break;

    }
    newPredictMPsB(zedPtr, estimPose.inverse(), activeMpsTemp, matchedIdxsL, matchedIdxsR, matchesIdxs, MPsOutliers);
    newPredictMPsB(zedPtrB, estimPose.inverse() * zedPtr->TCamToCam, activeMpsTempB, matchedIdxsLB, matchedIdxsRB, matchesIdxsB, MPsOutliersB);

    rad = 4;
    radB = 4;
    {
    std::thread front(&FeatureMatcher::matchByProjectionRPred, fm, std::ref(activeMpsTemp), std::ref(keysLeft), std::ref(matchedIdxsL), std::ref(matchedIdxsR), std::ref(matchesIdxs), std::ref(rad), true);
    std::thread back(&FeatureMatcher::matchByProjectionRPred, fmB, std::ref(activeMpsTempB), std::ref(keysLeftB), std::ref(matchedIdxsLB), std::ref(matchedIdxsRB), std::ref(matchesIdxsB), std::ref(radB), true);
    front.join();
    back.join();
    }

    both = estimatePoseCeresRB(activeMpsTemp, keysLeft, matchesIdxs, MPsOutliers, activeMpsTempB, keysLeftB, matchesIdxsB, MPsOutliersB, estimPose);

    std::vector<cv::KeyPoint> lp;
    std::vector<bool> closeL;
    lp.reserve(matchesIdxs.size());
    closeL.reserve(matchesIdxs.size());
    for ( size_t i{0}; i < matchesIdxs.size(); i++)
    {
        const std::pair<int,int>& keyPos = matchesIdxs[i];
        if ( MPsOutliers[i] )
            continue;
        if ( keyPos.first >= 0 )
        {
            lp.emplace_back(keysLeft.keyPoints[keyPos.first]);
            if ( keysLeft.close[keyPos.first] )
                closeL.emplace_back(true);
            else
                closeL.emplace_back(false);
        }
    }
    drawKeys("left", realLeftIm, lp, closeL);
    cv::waitKey(1);
    lp.clear();
    closeL.clear();
    for ( size_t i{0}; i < matchesIdxsB.size(); i++)
    {
        const std::pair<int,int>& keyPos = matchesIdxsB[i];
        if ( MPsOutliersB[i] )
            continue;
        if ( keyPos.first >= 0 )
        {
            lp.emplace_back(keysLeftB.keyPoints[keyPos.first]);
            if ( keysLeftB.close[keyPos.first] )
                closeL.emplace_back(true);
            else
                closeL.emplace_back(false);
        }
    }
    drawKeys("leftB", realLeftImB, lp, closeL);
    cv::waitKey(1);
    
    std::pair<int,int>& nStIn = both.first;
    std::pair<int,int>& nStInB = both.second;
    int allInliers = nStIn.first + nStInB.first;
    poseEst = estimPose.inverse();

    for ( size_t i{0}, endmp {activeMpsTemp.size()}; i < endmp; i++)
    {
        std::pair<int,int>& keyPos = matchesIdxs[i];
        if ( (keyPos.first >= 0 || keyPos.second >= 0) && !MPsOutliers[i] )
            MPsMatches[i] = true;
    }
    for ( size_t i{0}, endmp {activeMpsTempB.size()}; i < endmp; i++)
    {
        std::pair<int,int>& keyPos = matchesIdxsB[i];
        if ( (keyPos.first >= 0 || keyPos.second >= 0) && !MPsOutliersB[i] )
            MPsMatchesB[i] = true;
    }

    insertKeyFrameCount ++;
    prevKF = activeKeyFrames.front();
    if ( ((nStIn.second < minNStereo || nStInB.second < minNStereo) || insertKeyFrameCount >= keyFrameCountEnd) && allInliers < 0.9 * lastKFTrackedNumb )
    {
        insertKeyFrameCount = 0;
        insertKeyFrameRB(keysLeft, matchedIdxsL,matchesIdxs,MPsOutliers, keysLeftB, matchedIdxsLB,matchesIdxsB,MPsOutliersB, nStIn.second, nStInB.second, poseEst, leftIm);
        // insertKeyFrameR(keysLeft, matchedIdxsL,matchesIdxs, nStIn.second, poseEst, MPsOutliers, leftIm);
    }
    publishPoseNewB();

    setActiveOutliers(activeMpsTemp,MPsOutliers, MPsMatches);
    setActiveOutliers(activeMpsTempB,MPsOutliersB, MPsMatchesB);



    if ( curFrameNumb == zedPtr->numOfFrames - 1)

    {
        saveData();

        datafile.close();
        map->endOfFrames = true;
    }


    // return std::make_pair(kFCand, poseEst);
}

void FeatureTracker::TrackImageTBackUp(const cv::Mat& leftRect, const cv::Mat& rightRect, const Eigen::Matrix4d& prevCameraPose, const Eigen::Matrix4d& predPoseInv, std::vector<MapPoint*>& activeMpsTemp, std::vector<bool>& MPsOutliers, std::vector<bool>& MPsMatches, bool& newKF, const int frameNumb, std::vector<int>& matchedIdxsN, int& nStereo, int& nMono, KeyFrame*& kFCandidate, Eigen::Matrix4d& framePose)
{
    const int imageH {zedPtr->mHeight};
    const int imageW {zedPtr->mWidth};
    const int numbOfPixels {zedPtr->mHeight * zedPtr->mWidth};
    curFrame = frameNumb;
    curFrameNumb++;
    predNPoseInv = predPoseInv;

    if ( map->keyFrameAdded )
    {
        lastKFPose = prevCameraPose;
        lastKFPoseInv = prevCameraPose.inverse();
        lastKFImage = pLIm.im.clone();
        lastKFTrackedNumb = activeKeyFrames.front()->nKeysTracked;
        // std::lock_guard<std::mutex> lock(map->mapMutex);
        // map->keyFrameAdded = false;
    }
    
    if ( map->LBADone )
    {
        std::lock_guard<std::mutex> lock(map->mapMutex);
        if ( map->activeKeyFrames.size() > actvKFMaxSize )
        {
            // removeKeyFrame(activeKeyFrames);
            for ( size_t i{ actvKFMaxSize }, end{map->activeKeyFrames.size()}; i < end; i++)
            {
                map->activeKeyFrames[i]->active = false;
            }
            activeKeyFrames.resize(actvKFMaxSize);
        }
        map->LBADone = false;
    }
    // setLRImages(frameNumb);

    lIm.rIm = leftRect.clone();
    rIm.rIm = rightRect.clone();
    // cv::imshow("color", lIm.rIm);
    cv::cvtColor(lIm.rIm, lIm.im, cv::COLOR_BGR2GRAY);
    cv::cvtColor(rIm.rIm, rIm.im, cv::COLOR_BGR2GRAY);
    // cv::imshow("gray", lIm.im);
    TrackedKeys keysLeft;


    if ( curFrameNumb == 0 )
    {
        extractORBStereoMatch(lIm.im, rIm.im, keysLeft);

        initializeMap(keysLeft);
        // addMapPnts(keysLeft);
        setPreLImage();
        setPreRImage();
        return;
        // return std::make_pair(nullptr,Eigen::Matrix4d::Identity());
    }

    removeOutOfFrameMPs(prevCameraPose, activeMapPoints);

    extractORBStereoMatch(lIm.im, rIm.im, keysLeft);
    // Timer lel3("after extract");
    // Logging("2","",3);

    // Eigen::Matrix4d currPose = predNPoseInv;
    // Eigen::Matrix4d prevPose = zedPtr->cameraPose.poseInverse;
    matchedIdxsN = std::vector<int>(keysLeft.keyPoints.size(), -1);
    activeMpsTemp = activeMapPoints;
    MPsOutliers = std::vector<bool>(activeMpsTemp.size(),false);
    MPsMatches = std::vector<bool>(activeMpsTemp.size(),false);
    // std::lock_guard<std::mutex> lock(map->mapMutex);
    std::vector<int> matchedIdxsB(activeMpsTemp.size(), -1);
    
    
    const double dif = cv::norm(lastKFImage,lIm.im, cv::NORM_L2);
    const double similarity = 1 - dif/(numbOfPixels);


    if ( curFrameNumb == 1 )
        int nMatches = fm.matchByProjection(activeMpsTemp, keysLeft, matchedIdxsN, matchedIdxsB);
    else
    {
        std::vector<cv::KeyPoint> ConVelPoints;
        std::vector<int>scaleKeys;
        // worldToImg(activeMpsTemp, ConVelPoints, predNPoseInv);
        worldToImgScale(activeMpsTemp, ConVelPoints, predNPoseInv, scaleKeys);
        std::vector<float> mapAngles(activeMpsTemp.size(), -5.0);
        std::vector<cv::KeyPoint> prevKeyPos;
        // drawKeys("convelPoints", lIm.rIm, ConVelPoints);
        // worldToImg(activeMapPoints, prevKeyPos, zedPtr->cameraPose.poseInverse);
        calculatePrevKeyPos(activeMpsTemp, prevKeyPos, zedPtr->cameraPose.poseInverse, predNPoseInv);
        // if ( similarity < noMovementCheck )
        calcAngles(activeMpsTemp, ConVelPoints, prevKeyPos, mapAngles);

        // int nNewMatches = fm.matchByProjectionConVelAng(activeMpsTemp, ConVelPoints, prevKeyPos, keysLeft, matchedIdxsN, matchedIdxsB, 2, mapAngles);
        int nNewMatches = fm.matchByProjectionConVelAngScale(activeMpsTemp, ConVelPoints, prevKeyPos, keysLeft, matchedIdxsN, matchedIdxsB, 2, mapAngles, scaleKeys);
        // int nIn;
        // std::vector<float> weights;
        // // calcWeights(prePnts, weights);
        // weights.resize(activeMapPoints.size(), 1.0f);
        // OutliersReprojErr(predNPoseInv, activeMapPoints, keysLeft, matchedIdxsB, 20.0, weights, nIn);

        // for (size_t i{0}, end{matchedIdxsB.size()}; i < end; i++)
        // {
        //     if ( matchedIdxsB[i] < 0 )
        //         continue;
        //     MapPoint* mp = activeMapPoints[i];
        //     if ( mp->GetIsOutlier() )
        //     {
        //         matchedIdxsN[matchedIdxsB[i]] = -1;
        //         matchedIdxsB[i] = -1;
        //     }
        // }
    }

    // {
    // std::vector<cv::KeyPoint>activeKeys;
    // worldToImg(activeMapPoints, activeKeys, zedPtr->cameraPose.poseInverse);
    // checkWithFund(activeKeys, keysLeft.keyPoints, matchedIdxsB, matchedIdxsN);
    // }

    Eigen::Matrix4d estimPose = predNPoseInv;
    estimatePoseCeres(activeMpsTemp, keysLeft, matchedIdxsB, estimPose, MPsOutliers, true);
    // Logging("3","",3);

    // for (size_t i{0}, end{matchedIdxsB.size()}; i < end; i++)
    // {
    //     if ( activeMapPoints[i]->GetIsOutlier() && matchedIdxsB[i] >= 0)
    //     {
    //         matchedIdxsN[matchedIdxsB[i]] = -1;
    //         matchedIdxsB[i] = -1;
    //     }
    // }

    for (size_t i{0}, end{matchedIdxsB.size()}; i < end; i++)
    {
        if ( matchedIdxsB[i] < 0 )
            continue;
        MapPoint* mp = activeMpsTemp[i];
        if ( mp->GetIsOutlier() )
        {
            matchedIdxsN[matchedIdxsB[i]] = -1;
            matchedIdxsB[i] = -1;
        }
    }


    // after last refine check all matches, change outliers to inliers if they are no more, and in the end remove all outliers from vector. they are already saved on mappoints.

    // the outliers from first refine are not used on the next refines.

    // check for big displacement, if there is, use constant velocity model
    
    // Timer lel("after first refine");
    // Logging("similarity", similarity,3);

    std::vector<cv::KeyPoint> projectedPoints, projectedPointsfromang, realPrevKeys;
    worldToImg(activeMpsTemp, projectedPoints, estimPose);
    worldToImg(activeMpsTemp, realPrevKeys, zedPtr->cameraPose.poseInverse);
#if DRAWMATCHES
    std::vector<cv::Point2f> Nmpnts, Npnts2f;
    for ( size_t i {0}, end{activeMpsTemp.size()}; i < end; i++)
    {
        if ( matchedIdxsB[i] >= 0 && !activeMpsTemp[i]->GetIsOutlier() )
        {
            if ( realPrevKeys[i].pt.x > 0)
            {
                Nmpnts.emplace_back(realPrevKeys[i].pt);
                Npnts2f.emplace_back(keysLeft.keyPoints[matchedIdxsB[i]].pt);
            }
        }
    }
    drawPointsTemp<cv::Point2f>("first estimated matches", lIm.rIm, Nmpnts, Npnts2f);
    cv::waitKey(1);
#endif
    std::vector<cv::KeyPoint> prevKeyPos;

    calculatePrevKeyPos(activeMpsTemp, prevKeyPos, zedPtr->cameraPose.poseInverse, estimPose);
    
    std::vector<float> mapAngles(activeMpsTemp.size(), -5.0);
    // worldToImgAng(activeMapPoints, mapAngles, estimPose,prevKeyPos, projectedPointsfromang);
    // if ( similarity < noMovementCheck )
    // {
        calcAngles(activeMpsTemp, projectedPoints, prevKeyPos, mapAngles);
        // checkPrevAngles(mapAngles, prevKeyPos, matchedIdxsN, matchedIdxsB, keysLeft);
    // }
    int nNewMatches = fm.matchByProjectionPredWA(activeMpsTemp, projectedPoints, realPrevKeys, keysLeft, matchedIdxsN, matchedIdxsB, 10, mapAngles);
    // Logging("4","",3);
#if DRAWMATCHES
    std::vector<cv::Point2f> prevpnts, projpnts, matchedpnts;
    for ( size_t i {0}, end{activeMpsTemp.size()}; i < end; i++)
    {
        if ( projectedPoints[i].pt.x > 0)
        {
            if ( matchedIdxsB[i] >= 0 && !activeMpsTemp[i]->GetIsOutlier())
            {
                prevpnts.emplace_back(prevKeyPos[i].pt);
                projpnts.emplace_back(projectedPoints[i].pt);
                matchedpnts.emplace_back(keysLeft.keyPoints[matchedIdxsB[i]].pt);
            }
        }
    }
    draw3PointsTemp<cv::Point2f,cv::Point2f,cv::Point2f>("proj, matched",lIm.rIm,prevpnts,projpnts,matchedpnts);

    std::vector<cv::Point2f> prevpnts2, projpnts2, matchedpnts2;
    for ( size_t i {0}, end{activeMpsTemp.size()}; i < end; i++)
    {
        if ( projectedPoints[i].pt.x > 0)
        {
            if ( matchedIdxsB[i] >= 0 && !activeMpsTemp[i]->GetIsOutlier())
            {
                prevpnts2.emplace_back(realPrevKeys[i].pt);
                projpnts2.emplace_back(projectedPoints[i].pt);
                matchedpnts2.emplace_back(keysLeft.keyPoints[matchedIdxsB[i]].pt);
            }
        }
    }
    draw3PointsTemp<cv::Point2f,cv::Point2f,cv::Point2f>("realPrevKeys, matched",lIm.rIm,prevpnts2,projpnts2,matchedpnts2);
    cv::waitKey(1);
#endif
    // drawPointsTemp<cv::Point2f>("projected matches", lIm.rIm, mpnts, pnts2f);

    // std::vector<cv::Point2f> Nmpnts, Npnts2f;
    // for ( size_t i {0}, end{activeMapPoints.size()}; i < end; i++)
    // {
    //     if ( matchedIdxsB[i] >= 0 && !activeMapPoints[i]->GetIsOutlier() )
    //     {
    //         if ( prevKeyPos[i].pt.x > 0)
    //         {
    //             Nmpnts.emplace_back(prevKeyPos[i].pt);
    //             Npnts2f.emplace_back(keysLeft.keyPoints[matchedIdxsB[i]].pt);
    //         }
    //     }
    // }
    // drawPointsTemp<cv::Point2f>("real matches", lIm.rIm, Nmpnts, Npnts2f);
    // cv::waitKey(0);

    // {
    // std::vector<cv::KeyPoint>activeKeys;
    // worldToImg(activeMapPoints, activeKeys, zedPtr->cameraPose.poseInverse);
    // checkWithFund(activeKeys, keysLeft.keyPoints, matchedIdxsB, matchedIdxsN);
    // }



    std::pair<int,int> nStIn = estimatePoseCeres(activeMpsTemp, keysLeft, matchedIdxsB, estimPose, MPsOutliers, true);
    // cv::waitKey(1);
    std::vector<cv::KeyPoint> trckK;
    trckK.reserve(matchedIdxsB.size());
    for (size_t i{0}, end{matchedIdxsB.size()}; i < end; i++)
    {
        MapPoint* mp = activeMpsTemp[i];
        if ( matchedIdxsB[i] < 0 )
        {
            continue;
        }
        if ( mp->GetIsOutlier() )
        {
            matchedIdxsN[matchedIdxsB[i]] = -1;
            matchedIdxsB[i] = -1;
        }
        else
        {
            MPsMatches[i] = true;
            trckK.emplace_back(keysLeft.keyPoints[matchedIdxsB[i]]);
        }
    }
#if DRAWMATCHES
    drawKeys("TrackedKeys", lIm.rIm,trckK);
#endif
#if DRAWMATCHES
    std::vector<cv::Point2f> Nmpnts2, Npnts2f2;
    for ( size_t i {0}, end{activeMpsTemp.size()}; i < end; i++)
    {
        if ( matchedIdxsB[i] >= 0 )
        {
            if ( realPrevKeys[i].pt.x > 0)
            {
                Nmpnts2.emplace_back(realPrevKeys[i].pt);
                Npnts2f2.emplace_back(keysLeft.keyPoints[matchedIdxsB[i]].pt);
            }
        }
    }
    drawPointsTemp<cv::Point2f>("second estimated matches", lIm.rIm, Nmpnts2, Npnts2f2);
    cv::waitKey(1);
#endif
    // cv::waitKey(0);
    // Logging("second refine pose", estimPose,3);

    // Logging("nIn", nStIn.first,3);
    // Logging("nStereo", nStIn.second,3);

    // Timer lel2("after second refine");

    poseEst = estimPose.inverse();

    // Eigen::Matrix4d poseDif = poseEst * zedPtr->cameraPose.poseInverse;

    // Eigen::Matrix3d Rdif = poseDif.block<3,3>(0,0);
    // Eigen::Matrix<double,3,1> tdif =  poseDif.block<3,1>(0,3);

    // Sophus::SE3<double> se3t(Rdif, tdif);
    // // Sophus::SE3;

    // Sophus::Vector6d prevDisp = displacement;

    // displacement = se3t.log();
    // std::cout << "dif displacement " << displacement - prevDisp << std::endl;
    // std::cout << "displacement " << displacement << std::endl;
    // std::cout << "prevDisp " << prevDisp << std::endl;

    // kalmanF(poseDif);

    // poseEst = poseDif * zedPtr->cameraPose.pose;
    // estimPose = poseEst.inverse();

    // get percentage of displacement. keep the previous one and calculate the percentage.
    insertKeyFrameCount ++;
    nStereo = nStIn.second;
    nMono = nStIn.first - nStIn.second;
    // Logging("inliers", nStIn.first,3);
    Logging("stereo", nStIn.second,3);
    Logging("lastKFTrackedNumb", 0.9* lastKFTrackedNumb,3);
    Logging("activeMpsTemp.size()",activeMpsTemp.size(),3);
    KeyFrame* kFCand;

    if ( nStIn.second < minNStereo || (activeMpsTemp.size() < 0.9 * lastKFTrackedNumb && insertKeyFrameCount >= keyFrameCountEnd))
    {
        newKF = true;
        insertKeyFrameCount = 0;
        // insertKeyFrame(keysLeft, matchedIdxsN, nStIn.second, nMono, poseEst);
        // Logging("New KeyFrame!","",3);
        // Logging("nStIn.first!",nStIn.first,3);
        // Logging("0.9 * lastKFTrackedNumb!",0.9 * lastKFTrackedNumb,3);
        // Logging("nStIn.second",nStIn.second,3);
    }
    // else
    // {
    //     addFrame(keysLeft, matchedIdxsN, nStIn.second, poseEst);
    // }
    kFCand = insertKeyFrameOut(keysLeft, poseEst);

    setPreLImage();
    setPreRImage();



    if ( frameNumb == zedPtr->numOfFrames - 1)

    {
        saveData();

        datafile.close();
        map->endOfFrames = true;
    }

    kFCandidate = kFCand;
    framePose = poseEst;

    // return std::make_pair(kFCand, poseEst);
}

void FeatureTracker::removeMapPoints(std::vector<MapPoint*>& activeMapPoints, std::vector<bool>& toRemove)
{
    const size_t end{activeMapPoints.size()};
    toRemove.resize(end);
    int j {0};
    for ( size_t i {0}; i < end; i++)
    {
        if ( !toRemove[i] )
            activeMapPoints[j++] = activeMapPoints[i];
    }
    activeMapPoints.resize(j);
}

void FeatureTracker::beginTrackingGoodFeatures(const int frames)
{
    for (int32_t frame {1}; frame < frames; frame++)
    {
        curFrame = frame;
        setLRImages(frame);
        // fm.checkDepthChange(pLIm.im,pRIm.im,prePnts);
        if ( (addFeatures || uStereo < mnSize || cv::norm(pTvec)*zedPtr->mFps > highSpeed) && ( uStereo < mxSize) )
        {
            // Logging("ptvec",pTvec,3);
            // Logging("cv::norm(pTvec)",cv::norm(pTvec),3);

            zedPtr->addKeyFrame = true;
            // if ( uMono > mxMonoSize )
            //     updateKeysClose(frame);
            // else
            updateKeysGoodFeatures(frame);
            fd.compute3DPoints(prePnts, keyNumb);
            keyframes.emplace_back(zedPtr->cameraPose.pose,prePnts.points3D,keyNumb);
            keyNumb ++;
            
        }
        
        // opticalFlow();
        if ( curFrame == 1 )
            opticalFlow();
        else
            opticalFlowPredict();

        // Logging("addf", addFeatures,3);

        // getSolvePnPPoseWithEss();

        // getPoseCeres();
        getPoseCeresNew();

        setPreTrial();

        addFeatures = checkFeaturesArea(prePnts);
        // addFeatures = checkFeaturesAreaCont(prePnts);
        Logging("ustereo", uStereo,3);
        Logging("umono", uMono,3);
    }
    datafile.close();
}

void FeatureTracker::getWeights(std::vector<float>& weights, std::vector<cv::Point2d>& p2Dclose)
{
    const size_t end {prePnts.left.size()};
    weights.reserve(end);
    p2Dclose.reserve(end);
    const float vd {zedPtr->mBaseline * 40};
    const float sig {vd};
    uStereo = 0;
    for (size_t i {0}; i < end; i++)
    {
        p2Dclose.emplace_back((double)pnts.left[i].x, (double)pnts.left[i].y);
        if ( prePnts.depth[i] < vd)
        {
            uStereo ++;
            weights.emplace_back(1.0f);
        }
        else
        {
            float prob = norm_pdf(prePnts.depth[i], vd, sig);
            weights.emplace_back(2 * prob * vd);
        }
    }
}

float FeatureTracker::norm_pdf(float x, float mu, float sigma)
{
	return 1.0 / (sigma * sqrt(2.0 * M_PI)) * exp(-(pow((x - mu)/sigma, 2)/2.0));
}

bool FeatureTracker::checkFeaturesArea(const SubPixelPoints& prePnts)
{
    const size_t end{prePnts.left.size()};
    const int sep {3};
    std::vector<int> gridCount;
    gridCount.resize(sep * sep);
    const int wid {(int)zedPtr->mWidth/sep + 1};
    const int hig {(int)zedPtr->mHeight/sep + 1};
    for (size_t i{0};i < end; i++)
    {
        const int w {(int)prePnts.left[i].x/wid};
        const int h {(int)prePnts.left[i].y/hig};
        gridCount[(int)(h + sep*w)] += 1;
    }
    const int mnK {fe.numberPerCell/2};
    const int mnG {7};
    const size_t endgr {gridCount.size()};
    int count {0};
    for (size_t i{0}; i < endgr; i ++ )
    {
        if ( gridCount[i] > mnK)
            count ++;
    }
    if ( count < mnG)
        return true;
    else
        return false;
}

bool FeatureTracker::checkFeaturesAreaCont(const SubPixelPoints& prePnts)
{
    static int skip = 0;
    const size_t end{prePnts.left.size()};
    const int sep {3};
    std::vector<int> gridCount;
    gridCount.resize(sep * sep);
    const int wid {(int)zedPtr->mWidth/sep + 1};
    const int hig {(int)zedPtr->mHeight/sep + 1};
    for (size_t i{0};i < end; i++)
    {
        const int w {(int)prePnts.left[i].x/wid};
        const int h {(int)prePnts.left[i].y/hig};
        gridCount[(int)(h + sep*w)] += 1;
    }
    const int mnK {10};
    const int mnmxG {7};
    const int mnG {3};
    const size_t endgr {gridCount.size()};
    int count {0};
    for (size_t i{0}; i < endgr; i ++ )
    {
        if ( gridCount[i] > mnK)
            count ++;
    }
    if ( count < mnmxG)
        skip++;
    else if (count < mnG)
        return true;
    else
        skip = 0;
    Logging("skip", skip,3);
    Logging("count", count,3);
    if ( skip > 2 || skip == 0)
        return false;
    else
        return true;
}

void FeatureTracker::getEssentialPose()
{
    cv::Mat Rvec(3,3,CV_64F), tvec(3,1,CV_64F);
    std::vector <uchar> inliers;
    std::vector<cv::Point2f> p, pp;
    cv::Mat dist = (cv::Mat_<double>(1,5) << 0,0,0,0,0);
    
    
    cv::undistortPoints(pnts.left,p,zedPtr->cameraLeft.cameraMatrix, dist);
    cv::undistortPoints(prePnts.left,pp,zedPtr->cameraLeft.cameraMatrix, dist);
    cv::Mat E = cv::findEssentialMat(prePnts.left, pnts.left,zedPtr->cameraLeft.cameraMatrix,cv::FM_RANSAC,0.99,0.1, inliers);
    if (!inliers.empty())
    {
        prePnts.reduce<uchar>(inliers);
        pnts.reduce<uchar>(inliers);
        reduceVectorTemp<cv::Point2f,uchar>(p,inliers);
        reduceVectorTemp<cv::Point2f,uchar>(pp,inliers);
    }
    uStereo = prePnts.left.size();
    if (uStereo > 10)
    {
        cv::Mat R1,R2,t;
        cv::decomposeEssentialMat(E, R1, R2,t);
        if (cv::norm(prevR,R1) > cv::norm(prevR,R2))
            Rvec = R2;
        else
            Rvec = R1;
        prevR = Rvec.clone();
        tvec = -t/10;
        convertToEigen(Rvec,tvec,poseEstFrame);
        Logging("R1",R1,3);
        Logging("R2",R2,3);
        publishPose();

    }
    
}

void FeatureTracker::getSolvePnPPose()
{

    cv::Mat dist = (cv::Mat_<double>(1,5) << 0,0,0,0,0);
    std::vector<bool> inliers;
    const size_t end {prePnts.points3D.size()};
    inliers.resize(end);
    std::vector<cv::Point3d> p3D;
    std::vector<cv::Point2d> p2D;
    std::vector<cv::Point2d> outp2D;
    p3D.reserve(end);
    p2D.reserve(end);
    outp2D.reserve(end);
    for (size_t i {0};i < end;i++)
    {
        cv::Point3d point = prePnts.points3D[i];
        cv::Point2d p2dtemp;
        if (checkProjection3D(point,p2dtemp))
        {
            if (prePnts.useable[i])
            {
                inliers[i] = true;
                p3D.emplace_back(point);
                p2D.emplace_back(pnts.points2D[i]);
                outp2D.emplace_back(p2dtemp);
            }
        }
    }
    prePnts.reduce<bool>(inliers);
    pnts.reduce<bool>(inliers);
    // cv::projectPoints(p3D,cv::Mat::eye(3,3, CV_64F),cv::Mat::zeros(3,1, CV_64F),zedPtr->cameraLeft.cameraMatrix,cv::Mat::zeros(5,1, CV_64F),outp2D);
    inliers.clear();
    const size_t endproj{p3D.size()};
    inliers.resize(endproj);
    const int wid {zedPtr->mWidth - 1};
    const int hig {zedPtr->mHeight - 1};
    for (size_t i{0};i < endproj; i++)
    {
        if (!(outp2D[i].x > wid || outp2D[i].x < 0 || outp2D[i].y > hig || outp2D[i].y < 0))
            inliers[i] = true;
    }

    prePnts.reduce<bool>(inliers);
    pnts.reduce<bool>(inliers);
    reduceVectorTemp<cv::Point2d,bool>(outp2D,inliers);
    reduceVectorTemp<cv::Point2d,bool>(p2D,inliers);
    reduceVectorTemp<cv::Point3d,bool>(p3D,inliers);

    std::vector<uchar> check;
    cv::findFundamentalMat(outp2D, p2D, check, cv::FM_RANSAC, 1, 0.99);

    prePnts.reduce<uchar>(check);
    pnts.reduce<uchar>(check);
    reduceVectorTemp<cv::Point2d,uchar>(outp2D,check);
    reduceVectorTemp<cv::Point2d,uchar>(p2D,check);
    reduceVectorTemp<cv::Point3d,uchar>(p3D,check);

    uStereo = p3D.size();
    cv::Mat Rvec = cv::Mat::zeros(3,1, CV_64F);
    cv::Mat tvec = cv::Mat::zeros(3,1, CV_64F);
    if (uStereo > 10)
    {
        //  cv::solvePnP(p3D, p2D,zedPtr->cameraLeft.cameraMatrix, dist,Rvec,tvec,true);
        check.clear();
        cv::solvePnPRansac(p3D, p2D,zedPtr->cameraLeft.cameraMatrix, dist,Rvec,tvec,true,100,2.0f, 0.999, check);

    }

    // prePnts.reduce<uchar>(check);
    // pnts.reduce<uchar>(check);
    // reduceVectorTemp<cv::Point2d,uchar>(outp2D,check);
    // reduceVectorTemp<cv::Point2d,uchar>(p2D,check);
    // reduceVectorTemp<cv::Point3d,uchar>(p3D,check);
    cv::Mat measurements = cv::Mat::zeros(6,1, CV_64F);

    Logging("norm",cv::norm(tvec,pTvec),3);
    Logging("normr",cv::norm(Rvec,pRvec),3);
    if (cv::norm(tvec,pTvec) + cv::norm(Rvec,pRvec) > 1)
    {
        tvec = pTvec;
        Rvec = pRvec;
    }

    if (p3D.size() > mnInKal)
    {
        lkal.fillMeasurements(measurements, tvec, Rvec);
    }
    else
    {
        Logging("less than 50","",3);
    }

    pTvec = tvec;
    pRvec = Rvec;

    cv::Mat translation_estimated(3, 1, CV_64F);
    cv::Mat rotation_estimated(3, 3, CV_64F);

    lkal.updateKalmanFilter(measurements, translation_estimated, rotation_estimated);
    Logging("measurements",measurements,3);
    Logging("rot",rotation_estimated,3);
    Logging("tra",translation_estimated,3);
    pE.convertToEigenMat(rotation_estimated, translation_estimated, poseEstFrame);
    publishPose();
#if PROJECTIM
    draw2D3D(pLIm.rIm, outp2D, p2D);
#endif
}

void FeatureTracker::getSolvePnPPoseWithEss()
{

    cv::Mat dist = (cv::Mat_<double>(1,5) << 0,0,0,0,0);
    std::vector<bool> inliers;
    const size_t end {prePnts.points3D.size()};
    inliers.resize(end);
    std::vector<cv::Point3d> p3D;
    std::vector<cv::Point2d> p2D;
    std::vector<cv::Point2d> pp2Dess;
    std::vector<cv::Point2d> p3Dp2D;
    std::vector<cv::Point2d> outp2D;
    p3D.reserve(end);
    p3Dp2D.reserve(end);
    p2D.reserve(end);
    outp2D.reserve(end);
    for (size_t i {0};i < end;i++)
    {
        cv::Point3d point = prePnts.points3D[i];
        cv::Point2d p2dtemp;
        if (checkProjection3D(point,p2dtemp))
        {
            inliers[i] = true;
            outp2D.emplace_back(pnts.left[i]);
            pp2Dess.emplace_back(prePnts.left[i]);
            if (prePnts.useable[i])
            {

                p3D.emplace_back(point);
                p3Dp2D.emplace_back(p2dtemp);
                p2D.emplace_back(pnts.left[i]);
            }
        }
    }
    prePnts.reduce<bool>(inliers);
    pnts.reduce<bool>(inliers);


    // inliers.clear();
    // const size_t endproj{p3D.size()};
    // inliers.resize(endproj);
    // const int wid {zedPtr->mWidth - 1};
    // const int hig {zedPtr->mHeight - 1};
    // for (size_t i{0};i < endproj; i++)
    // {
    //     if (!(outp2D[i].x > wid || outp2D[i].x < 0 || outp2D[i].y > hig || outp2D[i].y < 0))
    //         inliers[i] = true;
    // }

    // prePnts.reduce<bool>(inliers);
    // pnts.reduce<bool>(inliers);
    // reduceVectorTemp<cv::Point2d,bool>(outp2D,inliers);
    // reduceVectorTemp<cv::Point2d,bool>(p2D,inliers);
    // reduceVectorTemp<cv::Point2d,bool>(pp2Dess,inliers);
    // reduceVectorTemp<cv::Point3d,bool>(p3D,inliers);

    // std::vector<uchar> check;
    // cv::findFundamentalMat(p3Dp2D, p2D, check, cv::FM_RANSAC, 1, 0.999);
    // reduceVectorTemp<cv::Point2d,uchar>(p2D,check);
    // reduceVectorTemp<cv::Point2d,uchar>(p3Dp2D,check);
    // reduceVectorTemp<cv::Point3d,uchar>(p3D,check);
    // prePnts.reduce<uchar>(check);
    // pnts.reduce<uchar>(check);
    // reduceVectorTemp<cv::Point2d,uchar>(outp2D,check);
    // reduceVectorTemp<cv::Point2d,uchar>(p2D,check);
    // reduceVectorTemp<cv::Point2d,uchar>(pp2Dess,check);
    cv::Mat Rvec = cv::Mat::zeros(3,1, CV_64F);
    cv::Mat tvec = cv::Mat::zeros(3,1, CV_64F);

    // cv::Mat E = cv::findEssentialMat(pp2Dess,outp2D,zedPtr->cameraLeft.cameraMatrix,cv::FM_RANSAC, 0.999,1.0f);
    // cv::Mat R1es,R2es,tes;
    // cv::decomposeEssentialMat(E,R1es, R2es, tes);
    // cv::Rodrigues(R1es,R1es);
    // cv::Rodrigues(R2es,R2es);
    // const double norm1 {cv::norm(Rvec,R1es)};
    // const double norm2 {cv::norm(Rvec,R2es)};

    // if (norm1 > norm2)
    //     Rvec = R2es;
    // else
    //     Rvec = R1es;

    uStereo = p3D.size();
    if (uStereo > 10)
    {
        //  cv::solvePnP(p3D, p2D,zedPtr->cameraLeft.cameraMatrix, dist,Rvec,tvec,true);
        std::vector<int>idxs;
        cv::solvePnPRansac(p3D, p2D,zedPtr->cameraLeft.cameraMatrix, dist,Rvec,tvec,true,100,2.0f, 0.99, idxs);

        reduceVectorInliersTemp<cv::Point2d,int>(p2D,idxs);
        reduceVectorInliersTemp<cv::Point2d,int>(p3Dp2D,idxs);
        reduceVectorInliersTemp<cv::Point3d,int>(p3D,idxs);
        // cv::solvePnPRefineLM(p3D, p2D,zedPtr->cameraLeft.cameraMatrix, dist,Rvec,tvec);
        // prePnts.reduce<uchar>(check);
        // pnts.reduce<uchar>(check);
        // reduceVectorTemp<cv::Point2d,uchar>(outp2D,check);
        // reduceVectorTemp<cv::Point2d,uchar>(pp2Dess,check);

    }

    cv::Mat measurements = cv::Mat::zeros(6,1, CV_64F);

    if (cv::norm(tvec,pTvec) + cv::norm(Rvec,pRvec) > 1)
    {
        tvec = pTvec;
        Rvec = pRvec;
    }

    if (p3D.size() > mnInKal)
    {
        lkal.fillMeasurements(measurements, tvec, Rvec);
    }
    else
    {
        Logging("less than ",mnInKal,3);
    }

    pTvec = tvec;
    pRvec = Rvec;

    cv::Mat translation_estimated(3, 1, CV_64F);
    cv::Mat rotation_estimated(3, 3, CV_64F);

    lkal.updateKalmanFilter(measurements, translation_estimated, rotation_estimated);
    pE.convertToEigenMat(rotation_estimated, translation_estimated, poseEstFrame);
    publishPose();
#if PROJECTIM
    draw2D3D(pLIm.rIm,p3Dp2D, p2D);
#endif
}

void FeatureTracker::getPoseCeres()
{

    std::vector<cv::Point3d> p3D;
    std::vector<cv::Point2d> p2D;

    get3dPointsforPoseAll(p3D, p2D);

    cv::Mat Rvec = cv::Mat::zeros(3,1, CV_64F);
    cv::Mat tvec = cv::Mat::zeros(3,1, CV_64F);

    essForMonoPose(Rvec, tvec,p3D);

    if (p3D.size() > 10)
    {
        pnpRansac(Rvec, tvec, p3D, p2D);
    }
    // uStereo = p3D.size();
    poseEstKal(Rvec, tvec, p3D.size());

}

void FeatureTracker::getPoseCeresNew()
{

    std::vector<cv::Point3d> p3D;
    std::vector<cv::Point2d> p2D;

    get3dPointsforPoseAll(p3D, p2D);
    // std::vector<uchar>err;
    // cv::findFundamentalMat(p2D,pnts.left,err,cv::FM_RANSAC,3,0.99);
    // reduceVectorTemp<cv::Point3d,uchar>(p3D, err);
    // pnts.reduce<uchar>(err);
    // prePnts.reduce<uchar>(err);

    cv::Mat Rvec = cv::Mat::zeros(3,1, CV_64F);
    cv::Mat tvec = pTvec.clone();


    // essForMonoPose(Rvec, tvec, p3D);

    if (p3D.size() > 10)
    {
        pnpRansac(Rvec, tvec, p3D, p2D);
    }
    // optimizePoseMO(p3D, Rvec, tvec);
    // if (abs(Rvec.at<double>(1)) > 0.04)
    //     bigRot = true;
    // else
    //     bigRot = false;
    // uStereo = p3D.size();
    poseEstKal(Rvec, tvec, uStereo);

}

void FeatureTracker::estimatePose(std::vector<cv::Point3d>& p3D, std::vector<cv::Point2f>& curPnts)
{
    cv::Mat Rvec = pRvec.clone();
    cv::Mat tvec = pTvec.clone();
    std::vector<cv::Point3d> close3D;
    std::vector<cv::Point2d> curPntsd;
    if (p3D.size() > 10)
    {
        curPntsd.reserve(curPnts.size());
        std::vector<cv::Point2f>::const_iterator it, end(curPnts.end());
        for (it = curPnts.begin(); it != end; it++)
            curPntsd.emplace_back((double)it->x, (double)it->y);


        std::vector<int>idxs;
        cv::solvePnPRansac(p3D, curPntsd,zedPtr->cameraLeft.cameraMatrix, cv::Mat::zeros(5,1,CV_64F),Rvec,tvec,true,100, 8.0f, 0.99, idxs);
        reduceVectorInliersTemp<cv::Point3d,int>(p3D, idxs);
        reduceVectorInliersTemp<cv::Point2d,int>(curPntsd, idxs);
        reduceVectorInliersTemp<int,int>(reprojIdxs, idxs);

        // ceresClose(p3D, curPntsd,Rvec,tvec);

        std::vector<cv::Point3d>::const_iterator p, pend(p3D.end());
        uStereo = 0;
        for (p = p3D.begin(); p != pend; p++)
            if ( p->z < zedPtr->mBaseline * 40)
                uStereo ++;
        uMono = p3D.size() - uStereo;
#if PROJECTIM
        std::vector<cv::Point2d> p2D, pn2D;

        compute2Dfrom3D(p3D, p2D, pn2D);

        drawPointsTemp<cv::Point2d, cv::Point2d>("solvepnp",pLIm.rIm,p2D,curPntsd);
#endif
    }
    poseEstKal(Rvec, tvec, uStereo);

    removeOutliers(curPntsd);
    

}

void FeatureTracker::removeOutliers(const std::vector<cv::Point2d>& curPntsd)
{
    std::vector<bool>in;
    const size_t endre {prePnts.left.size()};
    const int w {zedPtr->mWidth};
    const int h {zedPtr->mHeight};
    in.resize(endre,true);
    std::vector<cv::Point2d>rep, repnew;

    int count {0};
    for ( size_t i {0}; i < endre; i++)
    {
        cv::Point2d p2calc;
        cv::Point3d p3cam;
        bool inFr {wPntToCamPose(prePnts.points3D[i],p2calc, p3cam)};
        if ( i == reprojIdxs[count] )
        {
            if ( inFr ) 
            {
                if ( pointsDistTemp<cv::Point2d>(p2calc, curPntsd[count]) > 64 )
                    in[i] = false;
                else
                {
                    if ( !(curPntsd[count].x > w || curPntsd[count].x < 0 || curPntsd[count].y > h || curPntsd[count].y < 0) )
                    {
                        prePnts.left[i] = cv::Point2f((float)curPntsd[count].x,(float)curPntsd[count].y);
                        rep.emplace_back(p2calc);
                        repnew.emplace_back(curPntsd[count]);
                    }
                    else
                        in[i] = false;
                }
            }
            else
                in[i] = false;
            count ++;
        }
        else
        {
            if ( inFr )
                prePnts.left[i] = cv::Point2f((float)p2calc.x,(float)p2calc.y);
            else
                in[i] = false;
        }
    }

    // for ( auto& idx:reprojIdxs )
    // {
    //     cv::Point2d p2calc;
    //     wPntToCamPose(prePnts.points3D[idx],p2calc);
    //     // rep.emplace_back(p2calc);
    //     if ( pointsDistTemp<cv::Point2d>(p2calc, curPntsd[count]) > 64 || p2calc.x > zedPtr->mWidth || p2calc.x < 0 || p2calc.y > zedPtr->mHeight || p2calc.y < 0)
    //         in[idx] = false;
    //     else
    //     {
    //         prePnts.left[idx] = cv::Point2f((float)curPntsd[count].x,(float)curPntsd[count].y);
    //         rep.emplace_back(p2calc);
    //         repnew.emplace_back(curPntsd[count]);
    //     }
    //     count ++;
    // }
#if PROJECTIM
    drawPointsTemp<cv::Point2d, cv::Point2d>("reproj error",lIm.rIm,rep,repnew);
#endif
    prePnts.reduce<bool>(in);

}

void FeatureTracker::setTrackedLeft(std::vector<cv::Point2d>& curPntsd)
{
    const size_t endre {prePnts.points3D.size()};

    int count {0};
    for ( auto& idx:reprojIdxs )
    {
        prePnts.left[idx] = cv::Point2f((float)curPntsd[count].x,(float)curPntsd[count].y);
        count ++;
    }

}

int FeatureTracker::calcNumberOfStereo()
{
    int count {0};
    const size_t end {prePnts.left.size()};
    for (size_t i{0}; i < end; i++)
    {
        if ( prePnts.useable[i] )
            count ++;
    }
    return count;
}

void FeatureTracker::optimizePoseMotionOnly(std::vector<cv::Point3d>& p3D, cv::Mat& Rvec, cv::Mat& tvec)
{
    std::vector<cv::Point3d>p3Dclose;
    std::vector<cv::Point2d>p2Dclose;
    get3DClose(p3D,p3Dclose, p2Dclose);
    uStereo = p3Dclose.size();
    uMono = p3D.size() - uStereo;
    
    // ceresRansac(p3Dclose, p2Dclose, Rvec, tvec);
    ceresClose(p3Dclose, p2Dclose, Rvec, tvec);
    // ceresMO(p3Dclose, p2Dclose, Rvec, tvec);

    checkKeyDestrib(p2Dclose);
}

void FeatureTracker::optimizePoseMO(std::vector<cv::Point3d>& p3D, cv::Mat& Rvec, cv::Mat& tvec)
{
    std::vector<cv::Point2d>p2Dclose;
    std::vector<float> weights;
    getWeights(weights, p2Dclose);

    uMono = p3D.size() - uStereo;
    
    // ceresRansac(p3Dclose, p2Dclose, Rvec, tvec);
    ceresWeights(p3D, p2Dclose, Rvec, tvec, weights);
    // ceresMO(p3Dclose, p2Dclose, Rvec, tvec);

    // checkKeyDestrib(p2Dclose);
}

void FeatureTracker::ceresWeights(std::vector<cv::Point3d>& p3Dclose, std::vector<cv::Point2d>& p2Dclose, cv::Mat& Rvec, cv::Mat& tvec, std::vector<float>& weights)
{
    ceres::Problem problem;
    // make initial guess
    double cameraR[3] {Rvec.at<double>(0), Rvec.at<double>(1), Rvec.at<double>(2)};
    double cameraT[3] {tvec.at<double>(0), tvec.at<double>(1), tvec.at<double>(2)};
    size_t end {p3Dclose.size()};
    problem.AddParameterBlock(cameraR,3);
    problem.AddParameterBlock(cameraT,3);
    for (size_t i{0}; i < end; i++)
    {
        ceres::CostFunction* costf = ReprojectionErrorWeighted::Create(p3Dclose[i],p2Dclose[i], (double)weights[i]);
        
        problem.AddResidualBlock(costf, new ceres::HuberLoss(10.0) /* squared loss */, cameraR, cameraT);
    }
    ceres::Solver::Options options;
    options.linear_solver_type = ceres::SPARSE_NORMAL_CHOLESKY;
    
    options.max_num_iterations = 100;
    options.minimizer_progress_to_stdout = false;
    ceres::Solver::Summary summary;
    ceres::Solve(options, &problem, &summary);
    Rvec.at<double>(0) = cameraR[0];
    Rvec.at<double>(1) = cameraR[1];
    Rvec.at<double>(2) = cameraR[2];
    tvec.at<double>(0) = cameraT[0];
    tvec.at<double>(1) = cameraT[1];
    tvec.at<double>(2) = cameraT[2];
}

void FeatureTracker::checkKeyDestrib(std::vector<cv::Point2d>& p2Dclose)
{
    const int sep {2};
    const int w {zedPtr->mWidth/sep};
    const int h {zedPtr->mHeight/sep};
    std::vector<int> grids;
    grids.resize(sep * sep);
    const size_t end {prePnts.left.size()};

    for (size_t i{0}; i < end; i++)
    {
        int x {(int)prePnts.left[i].x/w};
        int y {(int)prePnts.left[i].y/h};
        grids[(int)(x + sep * y)] += 1;
    }
    for (size_t i {0}; i < sep * sep; i++)
    {
        Logging("grid",i,3);
        Logging("",grids[i],3);
    }
    grids.clear();
    grids.resize(sep * sep);
    const size_t end2 {p2Dclose.size()};
    for (size_t i{0}; i < end2; i++)
    {
        int x {(int)p2Dclose[i].x/w};
        int y {(int)p2Dclose[i].y/h};
        grids[(int)(x + sep * y)] += 1;
    }
    
    for (size_t i {0}; i < sep * sep; i++)
    {
        Logging("grid3d",i,3);
        Logging("",grids[i],3);
    }

}

void FeatureTracker::ceresRansac(std::vector<cv::Point3d>& p3Dclose, std::vector<cv::Point2d>& p2Dclose, cv::Mat& Rvec, cv::Mat& tvec)
{
    std::vector<int>idxVec;
    getIdxVec(idxVec, p3Dclose.size());

    float mnError {INFINITY};
    int earlyTerm {0};

    double outCamera[6];

    for (size_t i{0}; i < mxIter ; i++)
    {
        ceres::Problem problem;
        std::set<int> idxs;
        getSamples(idxVec, idxs);
        // make initial guess

        double camera[6] {Rvec.at<double>(0), Rvec.at<double>(1), Rvec.at<double>(2), tvec.at<double>(0), tvec.at<double>(1), tvec.at<double>(2)};
        std::set<int>::iterator it;
        for (it=idxs.begin(); it!=idxs.end(); ++it)
        {
            ceres::CostFunction* costf = ReprojectionErrorMono::Create(p3Dclose[*it],p2Dclose[*it]);
            problem.AddResidualBlock(costf, nullptr /* squared loss */, camera);
        }
        ceres::Solver::Options options;
        options.linear_solver_type = ceres::SPARSE_NORMAL_CHOLESKY;
        options.max_num_iterations = 25;
        // options.trust_region_strategy_type = ceres::DOGLEG;
        options.minimizer_progress_to_stdout = false;
        ceres::Solver::Summary summary;
        ceres::Solve(options, &problem, &summary);
        // double cost {0.0};
        // problem.Evaluate(ceres::Problem::EvaluateOptions(), &cost, NULL, NULL, NULL);
        // Logging("cost ", summary.final_cost,3);
        if ( mnError > summary.final_cost )
        {
            earlyTerm = 0;
            mnError = summary.final_cost;
            outCamera[0] = camera[0];
            outCamera[1] = camera[1];
            outCamera[2] = camera[2];
            outCamera[3] = camera[3];
            outCamera[4] = camera[4];
            outCamera[5] = camera[5];
        }
        else
            earlyTerm ++;
        if ( earlyTerm > 5 )
            break;
    }
    Rvec.at<double>(0) = outCamera[0];
    Rvec.at<double>(1) = outCamera[1];
    Rvec.at<double>(2) = outCamera[2];
    tvec.at<double>(0) = outCamera[3];
    tvec.at<double>(1) = outCamera[4];
    tvec.at<double>(2) = outCamera[5];
}

void FeatureTracker::ceresClose(std::vector<cv::Point3d>& p3Dclose, std::vector<cv::Point2d>& p2Dclose, cv::Mat& Rvec, cv::Mat& tvec)
{
    ceres::Problem problem;
    // make initial guess


    // double camera[6] {Rvec.at<double>(0), Rvec.at<double>(1), Rvec.at<double>(2), tvec.at<double>(0), tvec.at<double>(1), tvec.at<double>(2)};
    double cameraR[3] {Rvec.at<double>(0), Rvec.at<double>(1), Rvec.at<double>(2)};
    double cameraT[3] {tvec.at<double>(0), tvec.at<double>(1), tvec.at<double>(2)};
    size_t end {p3Dclose.size()};
    // Logging("R", Rvec.at<double>(0),3);
    // Logging("cam", camera[0],3);
    problem.AddParameterBlock(cameraR,3);
    problem.AddParameterBlock(cameraT,3);
    for (size_t i{0}; i < end; i++)
    {
        ceres::CostFunction* costf = ReprojectionErrorMono::Create(p3Dclose[i],p2Dclose[i]);
        
        problem.AddResidualBlock(costf, new ceres::HuberLoss(2.44765) /* squared loss */, cameraR, cameraT);
    }
    ceres::Solver::Options options;
    options.linear_solver_type = ceres::SPARSE_NORMAL_CHOLESKY;
    
    options.max_num_iterations = 100;
    options.max_solver_time_in_seconds = 0.05;

    // options.trust_region_strategy_type = ceres::DOGLEG;
    options.minimizer_progress_to_stdout = false;
    ceres::Solver::Summary summary;
    ceres::Solve(options, &problem, &summary);
    // options.gradient_tolerance = 1e-16;
    // options.function_tolerance = 1e-16;
    // options.parameter_tolerance = 1e-16;
    // double cost {0.0};
    // problem.Evaluate(ceres::Problem::EvaluateOptions(), &cost, NULL, NULL, NULL);
    // Logging("cost ", summary.final_cost,3);
    // Logging("R bef", Rvec,3);
    // Logging("T bef", tvec,3);
    Rvec.at<double>(0) = cameraR[0];
    Rvec.at<double>(1) = cameraR[1];
    Rvec.at<double>(2) = cameraR[2];
    tvec.at<double>(0) = cameraT[0];
    tvec.at<double>(1) = cameraT[1];
    tvec.at<double>(2) = cameraT[2];
    // Logging("R after", Rvec,3);
    // Logging("T after", tvec,3);
}

void FeatureTracker::ceresMO(std::vector<cv::Point3d>& p3Dclose, std::vector<cv::Point2d>& p2Dclose, cv::Mat& Rvec, cv::Mat& tvec)
{
    ceres::Problem problem;
    // make initial guess

    Eigen::Quaterniond q;
    q = Eigen::AngleAxisd(Rvec.at<double>(0), Eigen::Vector3d::UnitX()) * Eigen::AngleAxisd(Rvec.at<double>(1), Eigen::Vector3d::UnitY()) * Eigen::AngleAxisd(Rvec.at<double>(3), Eigen::Vector3d::UnitZ());
    Logging("q", q.x(),3);
    double camera[7] {q.w(), q.x(), q.y(), q.z(), tvec.at<double>(0), tvec.at<double>(1), tvec.at<double>(2)};
    size_t end {p3Dclose.size()};
    // Logging("R", Rvec.at<double>(0),3);
    // Logging("cam", camera[0],3);
    // problem.AddParameterBlock(camera);
    for (size_t i{0}; i < end; i++)
    {
        ceres::CostFunction* costf = ReprojectionErrorMO::Create(p3Dclose[i],p2Dclose[i]);
        problem.AddResidualBlock(costf, new ceres::HuberLoss(1.0) /* squared loss */, camera);
    }
    ceres::Solver::Options options;
    options.linear_solver_type = ceres::SPARSE_NORMAL_CHOLESKY;
    
    options.max_num_iterations = 100;
    // options.trust_region_strategy_type = ceres::DOGLEG;
    options.max_solver_time_in_seconds = 0.1;
    options.minimizer_progress_to_stdout = false;
    ceres::Solver::Summary summary;
    ceres::Solve(options, &problem, &summary);
    // options.gradient_tolerance = 1e-16;
    // options.function_tolerance = 1e-16;
    // options.parameter_tolerance = 1e-16;
    // double cost {0.0};
    // problem.Evaluate(ceres::Problem::EvaluateOptions(), &cost, NULL, NULL, NULL);
    // Logging("cost ", summary.final_cost,3);
    // Logging("R bef", Rvec,3);
    // Logging("T bef", tvec,3);
    Eigen::Quaterniond d(camera[0],camera[1],camera[2],camera[3]);
    auto euler = d.toRotationMatrix().eulerAngles(0, 1, 2);
    Logging("euler", euler[0],3);
    Logging("Rvec.at<double>(0)", Rvec.at<double>(0),3);
    Rvec.at<double>(0) = euler[0];
    Rvec.at<double>(1) = euler[1];
    Rvec.at<double>(2) = euler[2];
    tvec.at<double>(0) = camera[4];
    tvec.at<double>(1) = camera[5];
    tvec.at<double>(2) = camera[6];
    // Logging("R after", Rvec,3);
    // Logging("T after", tvec,3);
}

void FeatureTracker::getSamples(std::vector<int>& idxVec,std::set<int>& idxs)
{
    const size_t mxSize {idxVec.size()};
    while (idxs.size() < sampleSize)
    {
        std::random_device rd;
        std::mt19937 gen(rd());std::uniform_int_distribution<> distr(0, mxSize);
        idxs.insert(distr(gen));
    }

}

void FeatureTracker::get3DClose(std::vector<cv::Point3d>& p3D, std::vector<cv::Point3d>& p3Dclose, std::vector<cv::Point2d>& p2Dclose)
{
    const size_t end{prePnts.left.size()};
    p3Dclose.reserve(end);
    p2Dclose.reserve(end);
    for (size_t i{0}; i < end ; i++)
    {
        if ( prePnts.useable[i] )
        {
            p3Dclose.emplace_back(p3D[i]);
            p2Dclose.emplace_back((double)pnts.left[i].x, (double)pnts.left[i].y);
        }
    }
}

void FeatureTracker::getIdxVec(std::vector<int>& idxVec, const size_t size)
{
    idxVec.reserve(size);
    for (size_t i{0}; i < size ; i++)
    {
        idxVec.emplace_back(i);
    }
}

void FeatureTracker::compute2Dfrom3D(std::vector<cv::Point3d>& p3D, std::vector<cv::Point2d>& p2D, std::vector<cv::Point2d>& pn2D)
{
    const size_t end {p3D.size()};

    p2D.reserve(end);
    pn2D.reserve(end);

    for (size_t i{0}; i < end ; i ++)
    {
        const double px {p3D[i].x};
        const double py {p3D[i].y};
        const double pz {p3D[i].z};

        const double invZ = 1.0f/pz;
        const double fx = zedPtr->cameraLeft.fx;
        const double fy = zedPtr->cameraLeft.fy;
        const double cx = zedPtr->cameraLeft.cx;
        const double cy = zedPtr->cameraLeft.cy;

        p2D.emplace_back(fx*px*invZ + cx, fy*py*invZ + cy);
        pn2D.emplace_back((double)pnts.left[i].x, (double)pnts.left[i].y);
    }

}

void FeatureTracker::essForMonoPose(cv::Mat& Rvec, cv::Mat& tvec, std::vector<cv::Point3d>& p3D)
{
    std::vector<uchar> inliers;
    cv::Mat E = cv::findEssentialMat(prePnts.left, pnts.left,zedPtr->cameraLeft.cameraMatrix,cv::FM_RANSAC, 0.99,1.0f, inliers);
    cv::Mat R1es,R2es,tes;
    cv::decomposeEssentialMat(E,R1es, R2es, tes);
    cv::Rodrigues(R1es,R1es);
    cv::Rodrigues(R2es,R2es);
    const double norm1 {cv::norm(Rvec,R1es)};
    const double norm2 {cv::norm(Rvec,R2es)};

    if (norm1 > norm2)
        Rvec = R2es;
    else
        Rvec = R1es;

    pnts.reduce<uchar>(inliers);
    prePnts.reduce<uchar>(inliers);
    reduceVectorTemp<cv::Point3d,uchar>(p3D, inliers);

#if POINTSIM
    drawPoints(lIm.rIm,prePnts.left, pnts.left, "essential");
#endif

}

void FeatureTracker::pnpRansac(cv::Mat& Rvec, cv::Mat& tvec, std::vector<cv::Point3d>& p3D, std::vector<cv::Point2d>& p2D)
{

    std::vector<int>idxs;
    // Logging("Rvecbef", Rvec,3);
    // Logging("Tvecbef", tvec,3);


    cv::solvePnPRansac(p3D, p2D ,zedPtr->cameraLeft.cameraMatrix, cv::Mat::zeros(5,1,CV_64F),Rvec,tvec,true,100, 8.0f, 0.99, idxs);
    // Logging("Rvecaft", Rvec,3);
    // Logging("Tvecaft", tvec,3);
    // prePnts.reduceWithInliers<int>(idxs);
    // pnts.reduceWithInliers<int>(idxs);
    // reduceStereoKeysIdx<int,cv::Point2f>(stereoKeys,idxs, pnts.left,pnts.left);
    reduceVectorInliersTemp<cv::Point3d,int>(p3D,idxs);
    reduceVectorInliersTemp<cv::Point3d,int>(prePnts.points3D,idxs);
    reduceVectorInliersTemp<bool,int>(prePnts.useable,idxs);
    reduceVectorInliersTemp<cv::Point2f,int>(prePnts.left,idxs);
    reduceVectorInliersTemp<float,int>(prePnts.depth,idxs);
    reduceVectorInliersTemp<cv::Point2f,int>(pnts.left,idxs);

    // std::vector<cv::Point3d> p3dclose;
    // std::vector<cv::Point2d> p2dclose;
    // p2dclose.reserve(pnts.left.size());
    // p3dclose.reserve(pnts.left.size());
    // for (size_t i {0}; i < pnts.left.size(); i++)
    // {
    //     if ( prePnts.useable[i] )
    //     {
    //         p3dclose.emplace_back(p3D[i]);
    //         p2dclose.emplace_back((double)pnts.left[i].x, (double)pnts.left[i].y);
    //     }
    // }

    // ceresClose(p3dclose,p2dclose,Rvec, tvec);

    // reduceVectorInliersTemp<cv::Point2d,int>(p2Ddepth,idxs);
    // reduceVectorInliersTemp<cv::Point3d,int>(p3Ddepth,idxs);
    // cv::solvePnP(p3D, pnts.left,zedPtr->cameraLeft.cameraMatrix, cv::Mat::zeros(5,1,CV_64F),Rvec,tvec,true);

    // uStereo = p3D.size();


#if PROJECTIM
    std::vector<cv::Point2d> p2Dtr, pn2D;

    compute2Dfrom3D(p3D, p2Dtr, pn2D);

    draw2D3D(pLIm.rIm, p2Dtr, pn2D);
#endif

}

void FeatureTracker::kalmanF(Eigen::Matrix4d& calcPoseDif)
{
    cv::Mat measurements = cv::Mat::zeros(6,1, CV_64F);

    // Logging("tvec", cv::norm(tvec,pTvec), 3);
    // Logging("Rvec", cv::norm(Rvec,pRvec), 3);

    // // Logging("tvec", tvec, 3);
    // // Logging("pTvec", pTvec, 3);

    // // Logging("Rvec", Rvec, 3);
    // // Logging("pRvec", pRvec, 3);

    cv::Mat tvec, Rvec;
    Eigen::Matrix3d Reig = calcPoseDif.block<3,3>(0,0);
    Eigen::Matrix<double,3,1> teig = calcPoseDif.block<3,1>(0,3);
    cv::eigen2cv(Reig,Rvec);
    cv::eigen2cv(teig,tvec);
    cv::Rodrigues(Rvec, Rvec);
    Logging("Rvec", Rvec,3);
    Logging("tvec", tvec,3);

    lkal.fillMeasurements(measurements, tvec, Rvec);


    cv::Mat translation_estimated(3, 1, CV_64F);
    cv::Mat rotation_estimated(3, 3, CV_64F);

    lkal.updateKalmanFilter(measurements, translation_estimated, rotation_estimated);
    // translation_estimated = tvec.clone();
    // cv::Rodrigues(Rvec, rotation_estimated);
    pE.convertToEigenMat(rotation_estimated, translation_estimated, calcPoseDif);

    Logging("rotation_estimated", rotation_estimated,3);
    Logging("translation_estimated", translation_estimated,3);
    // publishPose();

    // publishPoseTrial();

}

void FeatureTracker::poseEstKal(cv::Mat& Rvec, cv::Mat& tvec, const size_t p3dsize)
{
    cv::Mat measurements = cv::Mat::zeros(6,1, CV_64F);

    // Logging("tvec", cv::norm(tvec,pTvec), 3);
    // Logging("Rvec", cv::norm(Rvec,pRvec), 3);

    // // Logging("tvec", tvec, 3);
    // // Logging("pTvec", pTvec, 3);

    // // Logging("Rvec", Rvec, 3);
    // // Logging("pRvec", pRvec, 3);

    if ((cv::norm(tvec,pTvec) > 1.0f || cv::norm(Rvec,pRvec) > 0.5f) && curFrame != 1)
    {
        tvec = pTvec.clone();
        Rvec = pRvec.clone();
    }
    else
    {
        pTvec = tvec.clone();
        pRvec = Rvec.clone();
    }
    lkal.fillMeasurements(measurements, tvec, Rvec);


    cv::Mat translation_estimated(3, 1, CV_64F);
    cv::Mat rotation_estimated(3, 3, CV_64F);

    lkal.updateKalmanFilter(measurements, translation_estimated, rotation_estimated);
    // translation_estimated = tvec.clone();
    // cv::Rodrigues(Rvec, rotation_estimated);
    pE.convertToEigenMat(rotation_estimated, translation_estimated, poseEstFrame);
    publishPose();

    // publishPoseTrial();

}

void FeatureTracker::refine3DPnts()
{
    // Eigen::Matrix4d temp = poseEstFrame.inverse();
    // std::vector<cv::Point3d>::iterator it;
    // std::vector<cv::Point3d>::const_iterator end(prePnts.points3D.end());
    // for (it = prePnts.points3D.begin(); it != end; it ++)
    // {
    //     Eigen::Vector4d p4d(it->x, it->y, it->z,1);
    //     p4d = temp * p4d;
    //     *it = cv::Point3d(p4d(0), p4d(1), p4d(2));
    // }

}

void FeatureTracker::get3dPointsforPose(std::vector<cv::Point3d>& p3D)
{
    std::vector<bool> inliers;
    const size_t end {prePnts.points3D.size()};
    inliers.resize(end);
    p3D.reserve(end);
    for (size_t i {0};i < end;i++)
    {
        cv::Point3d point = prePnts.points3D[i];
        cv::Point2d p2dtemp;
        if (checkProjection3D(point,p2dtemp))
        {
            inliers[i] = true;
            if (prePnts.useable[i])
                p3D.emplace_back(point);
            else
                p3D.emplace_back(cv::Point3d(prePnts.left[i].x, prePnts.left[i].y, 0.0f));
        }
    }
    prePnts.reduce<bool>(inliers);
    pnts.reduce<bool>(inliers);
}

void FeatureTracker::get3dPointsforPoseAll(std::vector<cv::Point3d>& p3D, std::vector<cv::Point2d>& p2D)
{
    std::vector<bool> inliers;
    const size_t end {prePnts.points3D.size()};
    inliers.resize(end);
    p3D.reserve(end);
    p2D.reserve(end);
    for (size_t i {0};i < end;i++)
    {
        cv::Point3d point = prePnts.points3D[i];
        cv::Point2d p2dtemp;
        if (checkProjection3D(point,p2dtemp))
        {
            inliers[i] = true;
            p3D.emplace_back(point);
            p2D.emplace_back((double)pnts.left[i].x, (double)pnts.left[i].y);
        }
    }
    prePnts.reduce<bool>(inliers);
    pnts.reduce<bool>(inliers);
}

void FeatureTracker::pointsInFrame(std::vector<cv::Point3d>& p3D)
{
    std::vector<bool> inliers;
    const size_t end {prePnts.points3D.size()};
    inliers.resize(end);
    p3D.reserve(end);
    reprojIdxs.clear();
    reprojIdxs.reserve(end);
    uStereo = 0;
    int countIdx {0};
    for (size_t i {0};i < end;i++)
    {
        cv::Point3d point = prePnts.points3D[i];
        cv::Point2d p2dtemp;
        if (checkProjection3D(point,p2dtemp))
        {
            // if (p2dtemp.x > zedPtr->mWidth || p2dtemp.x < 0 || p2dtemp.y > zedPtr->mHeight || p2dtemp.y < 0)
            //     continue;
            reprojIdxs.emplace_back(countIdx++);
            inliers[i] = true;
            p3D.emplace_back(point);
            // prePnts.left[i] = cv::Point2f((float)p2dtemp.x, (float)p2dtemp.y);
            // if ( prePnts.useable[i] )
            //     uStereo ++;
        }
    }
    prePnts.reduce<bool>(inliers);
    // uMono = prePnts.points3D.size() - uStereo;
}

bool FeatureTracker::wPntToCamPose(const cv::Point3d& p3, cv::Point2d& p2, cv::Point3d& p3cam)
{
    Eigen::Vector4d point(p3.x, p3.y, p3.z, 1);
    // Logging("point",point,3);
    point = zedPtr->cameraPose.poseInverse * point;
    // point = poseEstFrameInv * point;

    const double pointX = point(0);
    const double pointY = point(1);
    const double pointZ = point(2);

    if (pointZ <= 0.0f)
        return false;
    
    const double invZ = 1.0f/pointZ;


    double u {fx*pointX*invZ + cx};
    double v {fy*pointY*invZ + cy};

    const int min {0};
    const int maxW {zedPtr->mWidth};
    const int maxH {zedPtr->mHeight};

    if (u < min || u > maxW)
        return false;
    if (v < min || v > maxH)
        return false;

    p2 = cv::Point2d(u,v);
    
    p3cam = cv::Point3d(pointX, pointY, pointZ);

    return true;

}

void FeatureTracker::reprojError()
{
    std::vector<bool> inliers;
    const size_t end {prePnts.points3D.size()};
    inliers.resize(end);
    double err {0.0};
    int errC {0};
    for (size_t i {0};i < end;i++)
    {
        cv::Point3d point = prePnts.points3D[i];
        cv::Point2d p2dtemp;
        if (checkProjection3D(point,p2dtemp))
        {
            cv::Point2f ptr = cv::Point2f((float)p2dtemp.x, (float)p2dtemp.y);
            err += sqrt(pointsDist(ptr, prePnts.left[i]));
            errC ++;
        }
    }
    if (errC > 0)
    {
        const double avErr = err/errC;
        for (size_t i {0};i < end;i++)
        {
            // if ( )
        }
    }

    prePnts.reduce<bool>(inliers);
    pnts.reduce<bool>(inliers);
}

void FeatureTracker::calcGridVel()
{
    const int gRows {gridVelNumb};
    const int gCols {gridVelNumb};

    std::vector<float> gridx;
    std::vector<float> gridy;
    std::vector<int> counts;
    const int gridsq {gridVelNumb * gridVelNumb};
    gridx.resize(gridsq);
    gridy.resize(gridsq);
    counts.resize(gridsq);
    const int wid {(int)zedPtr->mWidth/gCols + 1};
    const int hig {(int)zedPtr->mHeight/gRows + 1};
    int ic {0};
    std::vector<cv::Point2f>::const_iterator it, end(prePnts.left.end());
    for (it = prePnts.left.begin(); it != end; it ++, ic++)
    {
        const int w {(int)it->x/wid};
        const int h {(int)it->y/hig};
        counts[(int)(w + h*gCols)] += 1;
        gridx[(int)(w + h*gCols)] += (it->x - pnts.left[ic].x);
        gridy[(int)(w + h*gCols)] += (it->y - pnts.left[ic].y);
    }

    for (size_t i {0}; i < gRows * gCols; i++)
    {
        if ( counts[i] != 0 )
        {
            gridTraX[i] = gridx[i]/counts[i];
            gridTraY[i] = gridy[i]/counts[i];
        }
        else
        {
            gridTraX[i] = gridTraX[i]/2;
            gridTraY[i] = gridTraY[i]/2;
        }
    }
}

void FeatureTracker::calculateNextPnts()
{
    const size_t end {prePnts.points3D.size()};
    pnts.left.reserve(end);
    std::vector<bool> in;
    in.resize(end,true);
    for (size_t i{0}; i < end; i++)
    {
        cv::Point2d pd((double)prePnts.left[i].x, (double)prePnts.left[i].y);
        if ( predictProjection3D(prePnts.points3D[i],pd) )
            pnts.left.emplace_back((float)pd.x, (float)pd.y);
        else
            in[i] = false;

    }
    prePnts.reduce<bool>(in);
}

void FeatureTracker::predictPts(std::vector<cv::Point2f>& curPnts)
{
    const size_t end {prePnts.points3D.size()};
    curPnts.reserve(end);
    for (size_t i{0}; i < end; i++)
    {
        cv::Point2d pd((double)prePnts.left[i].x, (double)prePnts.left[i].y);
        predictProjection3D(prePnts.points3D[i],pd);
        curPnts.emplace_back((float)pd.x, (float)pd.y);
    }
}

void FeatureTracker::calculateNextPntsGrids()
{
    const size_t end {prePnts.points3D.size()};
    const int gRows {gridVelNumb};
    const int gCols {gridVelNumb};
    const int wid {(int)zedPtr->mWidth/gCols + 1};
    const int hig {(int)zedPtr->mHeight/gRows + 1};
    pnts.left.reserve(end);
    for (size_t i{0}; i < end; i++)
    {
        cv::Point2f pf = prePnts.left[i];
        const int w  {(int)(prePnts.left[i].x/wid)};
        const int h  {(int)(prePnts.left[i].y/hig)};
        pf.x = pf.x - gridTraX[w + gCols*h];
        pf.y = pf.y - gridTraY[w + gCols*h];
        pnts.left.emplace_back(pf);
    }
}

void FeatureTracker::opticalFlow()
{
    std::vector<float> err, err1;
    std::vector <uchar>  inliers;
    cv::calcOpticalFlowPyrLK(pLIm.im, lIm.im, prePnts.left, pnts.left, inliers, err,cv::Size(21,21),3, criteria);

    prePnts.reduce<uchar>(inliers);
    pnts.reduce<uchar>(inliers);
    // reduceStereoKeys<uchar>(stereoKeys, inliers, inliers);
    // reduceVectorTemp<float,uchar>(err,inliers);

    // const float minErrValue {20.0f};

    // prePnts.reduceWithValue<float>(err, minErrValue);
    // pnts.reduceWithValue<float>(err, minErrValue);

    // cv::cornerSubPix(lIm.im,pnts.left,cv::Size(5,5),cv::Size(-1,-1),criteria);

    // cv::findFundamentalMat(prePnts.left, pnts.left, inliers, cv::FM_RANSAC, 3, 0.99);


    // prePnts.reduce<uchar>(inliers);
    // pnts.reduce<uchar>(inliers);

    // const size_t end{pnts.left.size()};
    // std::vector<bool> check;
    // check.resize(end);
    // for (size_t i{0};i < end;i++)
    // {
    //     if (!(pnts.left[i].x > zedPtr->mWidth || pnts.left[i].x < 0 || pnts.left[i].y > zedPtr->mHeight || pnts.left[i].y < 0))
    //         check[i] = true;
    // }

    // prePnts.reduce<bool>(check);
    // pnts.reduce<bool>(check);

#if OPTICALIM
    drawOptical("Optical", lIm.rIm,prePnts.left, pnts.left);
#endif
}

void FeatureTracker::changeUndef(std::vector<float>& err, std::vector <uchar>& inliers, std::vector<cv::Point2f>& temp)
{
    const float minErrValue {20.0f};
    const size_t end{pnts.left.size()};
    for (size_t i{0}; i < end; i++)
        if ( !inliers[i] || err[i] > minErrValue )
            pnts.left[i] = temp[i];
}

void FeatureTracker::opticalFlowPredict()
{
    Timer optical("optical");
    std::vector<float> err, err1;
    std::vector <uchar>  inliers, inliers2;
    std::vector<cv::Point3d> p3D;

    // reprojError();

    calculateNextPnts();
#if OPTICALIM
    drawOptical("before", pLIm.rIm,prePnts.left, pnts.left);
#endif
    std::vector<cv::Point2f> predPnts = pnts.left;
    cv::calcOpticalFlowPyrLK(pLIm.im, lIm.im, prePnts.left, pnts.left, inliers, err,cv::Size(21,21),1, criteria, cv::OPTFLOW_USE_INITIAL_FLOW);
    // prePnts.reduce<uchar>(inliers);
    // pnts.reduce<uchar>(inliers);
    // inliers.clear();
    std::vector<cv::Point2f> temp = prePnts.left;
    cv::calcOpticalFlowPyrLK(lIm.im, pLIm.im, pnts.left, temp, inliers2, err1,cv::Size(21,21),1, criteria, cv::OPTFLOW_USE_INITIAL_FLOW);
    // prePnts.reduce<uchar>(inliers);
    // pnts.reduce<uchar>(inliers);
    // reduceVectorTemp<cv::Point2f,uchar>(temp,inliers);

    // inliers.clear();
    for (size_t i {0}; i < pnts.left.size(); i ++)
    {
        if ( inliers[i] && inliers2[i] && pointsDist(temp[i],prePnts.left[i]) <= 0.25)
            inliers[i] = true;
        else
            inliers[i] = false;
    }
    // changeOptRes(inliers, predPnts, pnts.left);

    prePnts.reduce<uchar>(inliers);
    pnts.reduce<uchar>(inliers);
    // reduceStereoKeys<uchar>(stereoKeys, inliers, inliers);
    // cv::cornerSubPix(pLIm.im, prePnts.left, cv::Size(5,5),cv::Size(-1,-1),criteria);
    // cv::cornerSubPix(lIm.im, pnts.left, cv::Size(5,5),cv::Size(-1,-1),criteria);
    // reduceVectorTemp<cv::Point2f,bool>(predPnts, check);


// #if OPTICALIM
//     drawOptical("before", pLIm.rIm,prePnts.left, pnts.left);
// #endif
    // prePnts.reduce<uchar>(inliers);
    // pnts.reduce<uchar>(inliers);

    // matcherTrial();

    // cv::findFundamentalMat(prePnts.left, pnts.left, inliers, cv::FM_RANSAC, 3, 0.99);


    // prePnts.reduce<uchar>(inliers);
    // pnts.reduce<uchar>(inliers);
    // cv::imshow("prev left", pLIm.im);
    // cv::imshow("after left", lIm.im);
#if OPTICALIM
    drawOptical("after", pLIm.rIm,prePnts.left, pnts.left);
#endif

}

void FeatureTracker::optFlow(std::vector<cv::Point3d>& p3D, std::vector<cv::Point2f>& pPnts, std::vector<cv::Point2f>& curPnts)
{
    std::vector<float> err, err1;
    std::vector <uchar>  inliers, inliers2;
    pPnts = prePnts.left;
    std::vector<cv::Point2f> temp = pPnts;
    // reprojError();
    if (curFrame == 1)
    {
        cv::calcOpticalFlowPyrLK(pLIm.im, lIm.im, pPnts, curPnts, inliers, err,cv::Size(21,21),3, criteria);

        cv::calcOpticalFlowPyrLK(lIm.im, pLIm.im, curPnts, temp, inliers2, err1,cv::Size(21,21),3, criteria, cv::OPTFLOW_USE_INITIAL_FLOW);
    }
    else
    {
        predictPts(curPnts);
        cv::calcOpticalFlowPyrLK(pLIm.im, lIm.im, pPnts, curPnts, inliers, err,cv::Size(21,21),1, criteria, cv::OPTFLOW_USE_INITIAL_FLOW);
        cv::calcOpticalFlowPyrLK(lIm.im, pLIm.im, curPnts, temp, inliers2, err1,cv::Size(21,21),1, criteria, cv::OPTFLOW_USE_INITIAL_FLOW);
    }

    for (size_t i {0}; i < curPnts.size(); i ++)
    {
        if ( inliers[i] && inliers2[i] && pointsDist(temp[i],pPnts[i]) <= 0.25)
            inliers[i] = true;
        else
            inliers[i] = false;
    }

    reduceVectorTemp<cv::Point2f,uchar>(pPnts,inliers);
    reduceVectorTemp<cv::Point2f,uchar>(curPnts,inliers);
    reduceVectorTemp<cv::Point3d,uchar>(p3D,inliers);
    reduceVectorTemp<int,uchar>(reprojIdxs,inliers);

#if OPTICALIM
    drawOptical("after", pLIm.rIm,pPnts, curPnts);
#endif

}

void FeatureTracker::changeOptRes(std::vector <uchar>&  inliers, std::vector<cv::Point2f>& pnts1, std::vector<cv::Point2f>& pnts2)
{
    const size_t end {pnts1.size()};
    const int off{0};
    // const float mxPointsDist {400.0f};
    const int w {zedPtr->mWidth + off};
    const int h {zedPtr->mHeight + off};
    for ( size_t i {0}; i < end; i ++)
    {
        // if ( pnts1[i].x > w || pnts1[i].x < -off || pnts1[i].y > h || pnts1[i].y < -off )
        //     inliers[i] = false;
        // if (pnts1[i].x > w || pnts1[i].x < -off || pnts1[i].y > h || pnts1[i].y < -off )
        //     pnts2[i] = pnts1[i];
        if ( !inliers[i] && (pnts1[i].x > w || pnts1[i].x < -off || pnts1[i].y > h || pnts1[i].y < -off) )
        {
            pnts2[i] = pnts1[i];
            inliers[i] = true;
        }
            
    }
}

float FeatureTracker::pointsDist(const cv::Point2f& p1, const cv::Point2f& p2)
{
    return pow(p1.x - p2.x,2) + pow(p1.y - p2.y,2);
}

void FeatureTracker::matcherTrial()
{
    StereoKeypoints ke;
    StereoDescriptors de;
    fe.extractORBGrids(pLIm.im, lIm.im, de, ke);
    std::vector<cv::DMatch> matr;
    std::vector<cv::DMatch> newmatr;
    cv::Ptr<cv::BFMatcher> matcherer = cv::BFMatcher::create(cv::NORM_HAMMING,true);
    std::vector<uchar> in;
    matcherer->match(de.left,de.right,matr);

    for (int i = 0; i < matr.size(); i++)
    {
        tr.left.push_back(ke.left[matr[i].queryIdx].pt);
        tr.right.push_back(ke.right[matr[i].trainIdx].pt);
        newmatr.push_back(cv::DMatch(i,i, matr[i].distance));
    }

    cv::findFundamentalMat(tr.left,tr.right,in, cv::FM_RANSAC,3,0.99);
    reduceVectorTemp<cv::Point2f,uchar>(tr.left,in);
    reduceVectorTemp<cv::Point2f,uchar>(tr.right,in);
    reduceVectorTemp<cv::DMatch,uchar>(newmatr,in);
    
    drawMatchesKeys(pLIm.rIm, tr.left, tr.right, newmatr);

    tr.left.clear();
    tr.right.clear();
    matr.clear();
    newmatr.clear();
    ke.left.clear();
    ke.right.clear();


}

void FeatureTracker::drawMatchesKeys(const cv::Mat& lIm, const std::vector<cv::Point2f>& keys1, const std::vector<cv::Point2f>& keys2, const std::vector<cv::DMatch> matches)
{
    cv::Mat outIm = lIm.clone();
    for (auto m:matches)
    {
        cv::circle(outIm, keys1[m.queryIdx],2,cv::Scalar(0,255,0));
        cv::line(outIm, keys1[m.queryIdx], keys2[m.queryIdx],cv::Scalar(0,0,255));
        cv::circle(outIm, keys2[m.queryIdx],2,cv::Scalar(255,0,0));
    }
    cv::imshow("Matches matcherrr", outIm);
    cv::waitKey(waitImMat);
}

void FeatureTracker::drawMatchesNew(const char* com, const cv::Mat& lIm, const std::vector<cv::KeyPoint>& keys1, const std::vector<cv::KeyPoint>& keys2)
{
    cv::Mat outIm = lIm.clone();
    const size_t end {keys1.size()};
    for (size_t i{0};i < end; i ++ )
    {
        cv::circle(outIm, keys1[i].pt,2,cv::Scalar(0,255,0));
        cv::line(outIm, keys1[i].pt, keys2[i].pt,cv::Scalar(0,0,255));
        cv::circle(outIm, keys2[i].pt,2,cv::Scalar(255,0,0));
    }
    cv::imshow(com, outIm);
    cv::waitKey(waitImMat);
}

void FeatureTracker::drawMatchesNewP(const char* com, const cv::Mat& lIm, const std::vector<cv::Point2f>& keys1, const std::vector<cv::Point2f>& keys2)
{
    cv::Mat outIm = lIm.clone();
    const size_t end {keys1.size()};
    for (size_t i{0};i < end; i ++ )
    {
        cv::circle(outIm, keys1[i],2,cv::Scalar(0,255,0));
        cv::line(outIm, keys1[i], keys2[i],cv::Scalar(0,0,255));
        cv::circle(outIm, keys2[i],2,cv::Scalar(255,0,0));
    }
    cv::imshow(com, outIm);
    cv::waitKey(waitImMat);
}

void FeatureTracker::opticalFlowGood()
{
    std::vector<float> err;
    std::vector<cv::Point2f> pbef,pnex;
    std::vector <uchar>  inliers;
    cv::goodFeaturesToTrack(pLIm.im, pbef,300, 0.01, 30);
    cv::calcOpticalFlowPyrLK(pLIm.im, lIm.im, pbef, pnex, inliers, err,cv::Size(21,21),1, criteria);


#if OPTICALIM
    drawOptical("before fund good", pLIm.rIm,pbef, pnex);
#endif

    cv::findFundamentalMat(pbef, pnex, inliers, cv::FM_RANSAC, 3, 0.99);


    reduceVectorTemp<cv::Point2f, uchar>(pbef ,inliers);
    reduceVectorTemp<cv::Point2f, uchar>(pnex ,inliers);

#if OPTICALIM
    drawOptical("after fund good", pLIm.rIm,pbef, pnex);
#endif


}

void FeatureTracker::updateKeysGoodFeatures(const int frame)
{
    std::vector<cv::DMatch> matches;
    // stereoFeaturesPop(pLIm.im, pRIm.im, matches,pnts, prePnts);
    stereoFeaturesGoodFeatures(pLIm.im, pRIm.im,pnts, prePnts);
    prePnts.addLeft(pnts);
    pnts.clear();
}

void FeatureTracker::updateKeys(const int frame)
{
    std::vector<cv::DMatch> matches;
    // stereoFeaturesPop(pLIm.im, pRIm.im, matches,pnts, prePnts);
    stereoFeaturesMask(pLIm.im, pRIm.im, matches,pnts, prePnts);
    prePnts.addLeft(pnts);
    pnts.clear();
}

void FeatureTracker::updateKeysClose(const int frame)
{
    std::vector<cv::DMatch> matches;
    // stereoFeaturesPop(pLIm.im, pRIm.im, matches,pnts, prePnts);
    stereoFeaturesClose(pLIm.im, pRIm.im, matches,pnts);
    prePnts.addLeft(pnts);
    pnts.clear();
}

float FeatureTracker::calcDt()
{
    endTime =  std::chrono::high_resolution_clock::now();
    duration = endTime - startTime;
    startTime = std::chrono::high_resolution_clock::now();
    return duration.count();
}

void FeatureTracker::setLRImages(const int frameNumber)
{
    lIm.setImage(frameNumber,"left", zedPtr->seq);
    rIm.setImage(frameNumber,"right", zedPtr->seq);
    if (!zedPtr->rectified)
        rectifyLRImages();
}

void FeatureTracker::setLImage(const int frameNumber)
{
    lIm.setImage(frameNumber,"left", zedPtr->seq);
    if (!zedPtr->rectified)
        rectifyLImage();
    
}

void FeatureTracker::setPreLImage()
{
    pLIm.im = lIm.im.clone();
    pLIm.rIm = lIm.rIm.clone();
}

void FeatureTracker::setPreRImage()
{
    pRIm.im = rIm.im.clone();
    pRIm.rIm = rIm.rIm.clone();
}

void FeatureTracker::setPre()
{
    setPreLImage();
    setPreRImage();
    // calcGridVel();
    prePnts.left = pnts.left;
    clearPre();
}

void FeatureTracker::setPreTrial()
{
    setPreLImage();
    setPreRImage();
    // calcGridVel();
    checkBoundsLeft();
    clearPre();
}

void FeatureTracker::checkBoundsLeft()
{
    const int w {zedPtr->mWidth};
    const int h {zedPtr->mHeight};
    // prePnts.left = pnts.left;
    std::vector<bool> check;
    check.resize(pnts.left.size());
    int count {0};
    uStereo = 0;
    std::vector<cv::Point2f>::const_iterator it, end(pnts.left.end());
    for (it = pnts.left.begin(); it != end; it ++, count ++)
    {
        prePnts.left[count] = *it;
        if (!(it->x > w || it->x < 0 || it->y > h || it->y < 0))
        {
            check[count] = true;
            if ( prePnts.useable[count] )
                uStereo ++;
        }
    }
    prePnts.reduce<bool>(check);
    uMono = prePnts.left.size() - uStereo;
}

void FeatureTracker::setPreInit()
{
    setPreLImage();
    setPreRImage();
    prePnts.clone(pnts);
    clearPre();
}

void FeatureTracker::clearPre()
{
    pnts.clear();
}

cv::Mat FeatureTracker::getLImage()
{
    return lIm.im;
}

cv::Mat FeatureTracker::getRImage()
{
    return rIm.im;
}

cv::Mat FeatureTracker::getPLImage()
{
    return pLIm.im;
}

cv::Mat FeatureTracker::getPRImage()
{
    return pRIm.im;
}

void FeatureTracker::rectifyLRImages()
{
    // cv::imshow("before left",lIm.im);
    // cv::imshow("before right",rIm.im);
    lIm.rectifyImage(lIm.im, rmap[0][0], rmap[0][1]);
    lIm.rectifyImage(lIm.rIm, rmap[0][0], rmap[0][1]);
    rIm.rectifyImage(rIm.im, rmap[1][0], rmap[1][1]);
    rIm.rectifyImage(rIm.rIm, rmap[1][0], rmap[1][1]);
    // cv::imshow("left",lIm.im);
    // cv::imshow("right",rIm.im);
    // cv::waitKey(0);
}

void FeatureTracker::rectifyLImage()
{
    lIm.rectifyImage(lIm.im, rmap[0][0], rmap[0][1]);
    lIm.rectifyImage(lIm.rIm, rmap[0][0], rmap[0][1]);
}

void FeatureTracker::drawKeys(const char* com, cv::Mat& im, std::vector<cv::KeyPoint>& keys)
{
    cv::Mat outIm = im.clone();
    for (auto& key:keys)
    {
        cv::circle(outIm, key.pt,3,cv::Scalar(0,255,0),2);
    }
    cv::imshow(com, outIm);
    cv::waitKey(waitImKey);
}

void FeatureTracker::drawKeys(const char* com, cv::Mat& im, std::vector<cv::KeyPoint>& keys, std::vector<bool>& close)
{
    cv::Mat outIm = im.clone();
    int count {0};
    for (auto& key:keys)
    {
        if ( close[count] )
            cv::circle(outIm, key.pt,3,cv::Scalar(255,0,0),2);
        else
            cv::circle(outIm, key.pt,3,cv::Scalar(0,255,0),2);
        count++;
    }
    cv::imshow(com, outIm);
    cv::waitKey(waitImKey);
}


void FeatureTracker::drawMatches(const cv::Mat& lIm, const SubPixelPoints& pnts, const std::vector<cv::DMatch> matches)
{
    cv::Mat outIm = lIm.clone();
    for (auto m:matches)
    {
        cv::circle(outIm, pnts.left[m.queryIdx],2,cv::Scalar(0,255,0));
        cv::line(outIm, pnts.left[m.queryIdx], pnts.right[m.trainIdx],cv::Scalar(0,0,255));
        cv::circle(outIm, pnts.right[m.trainIdx],2,cv::Scalar(255,0,0));
    }
    cv::imshow("Matches", outIm);
    cv::waitKey(waitImMat);
}

void FeatureTracker::drawMatchesGoodFeatures(const cv::Mat& lIm, const SubPixelPoints& pnts)
{
    cv::Mat outIm = lIm.clone();
    const size_t size {pnts.left.size()};
    for (size_t i{0}; i < size; i ++)
    {
        cv::circle(outIm, pnts.left[i],2,cv::Scalar(0,255,0));
        cv::line(outIm, pnts.left[i], pnts.right[i],cv::Scalar(0,0,255));
        cv::circle(outIm, pnts.right[i],2,cv::Scalar(255,0,0));
    }
    cv::imshow("Matches", outIm);
    cv::waitKey(waitImMat);
}

void FeatureTracker::drawOptical(const char* com,const cv::Mat& im, const std::vector<cv::Point2f>& prePnts,const std::vector<cv::Point2f>& pnts)
{
    cv::Mat outIm = im.clone();
    const size_t end {prePnts.size()};
    for (size_t i{0};i < end; i ++ )
    {
        cv::circle(outIm, prePnts[i],2,cv::Scalar(0,255,0));
        cv::line(outIm, prePnts[i], pnts[i],cv::Scalar(0,0,255));
        cv::circle(outIm, pnts[i],2,cv::Scalar(255,0,0));
    }
    cv::imshow(com, outIm);
     cv::waitKey(waitImOpt);
}

void FeatureTracker::drawPoints(const cv::Mat& im, const std::vector<cv::Point2f>& prePnts,const std::vector<cv::Point2f>& pnts, const char* str)
{
    cv::Mat outIm = im.clone();
    const size_t end {prePnts.size()};
    for (size_t i{0};i < end; i ++ )
    {
        cv::circle(outIm, prePnts[i],2,cv::Scalar(0,255,0));
        cv::line(outIm, prePnts[i], pnts[i],cv::Scalar(0,0,255));
        cv::circle(outIm, pnts[i],2,cv::Scalar(255,0,0));
    }
    cv::imshow(str, outIm);
    cv::waitKey(waitImOpt);
}

void FeatureTracker::draw2D3D(const cv::Mat& im, const std::vector<cv::Point2d>& p2Dfp3D, const std::vector<cv::Point2d>& p2D)
{
    cv::Mat outIm = im.clone();
    const size_t end {p2Dfp3D.size()};
    for (size_t i{0};i < end; i ++ )
    {
        cv::circle(outIm, p2Dfp3D[i],2,cv::Scalar(0,255,0));
        cv::line(outIm, p2Dfp3D[i], p2D[i],cv::Scalar(0,0,255));
        cv::circle(outIm, p2D[i],2,cv::Scalar(255,0,0));
    }
    cv::imshow("Project", outIm);
    cv::waitKey(waitImPro);

}

bool FeatureTracker::checkProjection3D(cv::Point3d& point3D, cv::Point2d& point2d)
{
    
    // Logging("key",keyFrameNumb,3);
    Eigen::Vector4d point(point3D.x, point3D.y, point3D.z,1);
    // Logging("point",point,3);
    point = zedPtr->cameraPose.poseInverse * point;
    // Logging("point",point,3);
    // Logging("zedPtr",zedPtr->cameraPose.poseInverse,3);
    // Logging("getPose",keyframes[keyFrameNumb].getPose(),3);
    point3D.x = point(0);
    point3D.y = point(1);
    point3D.z = point(2);
    const double pointX = point(0);
    const double pointY = point(1);
    const double pointZ = point(2);

    if (pointZ <= 0.0f)
        return false;

    const double invZ = 1.0f/pointZ;
    const double fx = zedPtr->cameraLeft.fx;
    const double fy = zedPtr->cameraLeft.fy;
    const double cx = zedPtr->cameraLeft.cx;
    const double cy = zedPtr->cameraLeft.cy;

    const double invfx = 1.0f/fx;
    const double invfy = 1.0f/fy;


    double u {fx*pointX*invZ + cx};
    double v {fy*pointY*invZ + cy};


    const int min {0};
    const int maxW {zedPtr->mWidth};
    const int maxH {zedPtr->mHeight};

    if (u < min || u > maxW)
        return false;
    if (v < min || v > maxH)
        return false;

    // const double k1 = zedptr->cameraLeft.distCoeffs.at<double>(0,0);
    // const double k2 = zedptr->cameraLeft.distCoeffs.at<double>(0,1);
    // const double p1 = zedptr->cameraLeft.distCoeffs.at<double>(0,2);
    // const double p2 = zedptr->cameraLeft.distCoeffs.at<double>(0,3);
    // const double k3 = zedptr->cameraLeft.distCoeffs.at<double>(0,4);

    const double k1 {0};
    const double k2 {0};
    const double p1 {0};
    const double p2 {0};
    const double k3 {0};

    double u_distort, v_distort;

    double x = (u - cx) * invfx;
    double y = (v - cy) * invfy;
    
    double r2 = x * x + y * y;

    // Radial distorsion
    double x_distort = x * (1 + k1 * r2 + k2 * r2 * r2 + k3 * r2 * r2 * r2);
    double y_distort = y * (1 + k1 * r2 + k2 * r2 * r2 + k3 * r2 * r2 * r2);

    // Tangential distorsion
    x_distort = x_distort + (2 * p1 * x * y + p2 * (r2 + 2 * x * x));
    y_distort = y_distort + (p1 * (r2 + 2 * y * y) + 2 * p2 * x * y);

    u_distort = x_distort * fx + cx;
    v_distort = y_distort * fy + cy;


    // u = u_distort;
    // v = v_distort;

    point2d = cv::Point2d(u,v);
    

    if (u > maxW || u < 0 || v > maxH || v < 0)
        return false;

    return true;

}

bool FeatureTracker::predictProjection3D(const cv::Point3d& point3D, cv::Point2d& point2d)
{
    
    // Logging("key",keyFrameNumb,3);
    Eigen::Vector4d point(point3D.x, point3D.y, point3D.z,1);
    // Logging("point",point,3);
    // point = zedPtr->cameraPose.poseInverse * point;

    point = predNPose.inverse() * point;
    // Logging("point",point,3);
    // Logging("zedPtr",zedPtr->cameraPose.poseInverse,3);
    // Logging("getPose",keyframes[keyFrameNumb].getPose(),3);
     const double pointX = point(0);
    const double pointY = point(1);
    const double pointZ = point(2);

    if (pointZ <= 0.0f)
        return false;

    const double invZ = 1.0f/pointZ;
    const double fx = zedPtr->cameraLeft.fx;
    const double fy = zedPtr->cameraLeft.fy;
    const double cx = zedPtr->cameraLeft.cx;
    const double cy = zedPtr->cameraLeft.cy;

    const double invfx = 1.0f/fx;
    const double invfy = 1.0f/fy;


    double u {fx*pointX*invZ + cx};
    double v {fy*pointY*invZ + cy};


    const int min {0};
    const int maxW {zedPtr->mWidth};
    const int maxH {zedPtr->mHeight};

    // if ( u < min )
    //     u = min;
    // else if ( u > maxW )
    //     u = maxW ;

    // if ( v < min )
    //     v = min;
    // else if ( v > maxH )
    //     v = maxH ;


    point2d = cv::Point2d(u,v);
    return true;

}

void FeatureTracker::convertToEigen(cv::Mat& Rvec, cv::Mat& tvec, Eigen::Matrix4d& tr)
{
    Eigen::Matrix3d Reig;
    Eigen::Vector3d teig;
    cv::cv2eigen(Rvec.t(),Reig);
    cv::cv2eigen(-tvec,teig);

    tr.setIdentity();
    tr.block<3,3>(0,0) = Reig;
    tr.block<3,1>(0,3) = teig;
}

void FeatureTracker::publishPoseNew()
{
    prevReferencePose = prevKF->pose.getInvPose() * zedPtr->cameraPose.pose;
    prevWPose = zedPtr->cameraPose.pose;
    prevWPoseInv = zedPtr->cameraPose.poseInverse;
    referencePose = lastKFPoseInv * poseEst;
    // Eigen::Matrix4d lKFP = activeKeyFrames.front()->pose.pose;
    // zedPtr->cameraPose.setPose(referencePose, lKFP);
    zedPtr->cameraPose.setPose(poseEst);
    zedPtr->cameraPose.setInvPose(poseEst.inverse());
    zedPtr->cameraPose.refPose = referencePose;
    predNPose = poseEst * (prevWPoseInv * poseEst);
    predNPoseInv = predNPose.inverse();
}

void FeatureTracker::publishPoseNewB()
{
    prevReferencePose = prevKF->pose.getInvPose() * zedPtr->cameraPose.pose;
    prevWPose = zedPtr->cameraPose.pose;
    prevWPoseInv = zedPtr->cameraPose.poseInverse;
    referencePose = lastKFPoseInv * poseEst;
    // Eigen::Matrix4d lKFP = activeKeyFrames.front()->pose.pose;
    // zedPtr->cameraPose.setPose(referencePose, lKFP);
    zedPtr->cameraPose.setPose(poseEst);
    zedPtrB->cameraPose.setPose(poseEst * zedPtr->TCamToCam);
    zedPtr->cameraPose.refPose = referencePose;
    predNPose = poseEst * (prevWPoseInv * poseEst);
    predNPoseInv = predNPose.inverse();
}

void FeatureTracker::publishPoseLBA()
{
    // here you have to get the last FRAME from map-> get the ref pose from zedptr but the keypose from the keyNumb the lastframe has.
    Eigen::Matrix4d prevP = map->keyFrames.at(activeKeyFrames.front()->closestKF)->getPose();
    Eigen::Matrix4d lKFP = activeKeyFrames.front()->pose.pose;
    prevWPose = prevP * zedPtr->cameraPose.refPose;
    prevWPoseInv = prevWPose.inverse();
    referencePose = lastKFPoseInv * poseEst;
    zedPtr->cameraPose.setPose(referencePose, lKFP);
    // zedPtr->cameraPose.setPose(poseEst);
    // zedPtr->cameraPose.setInvPose(poseEst.inverse());
    predNPose = poseEst * (prevWPoseInv * poseEst);
    predNPoseInv = predNPose.inverse();
#if SAVEODOMETRYDATA
    saveData();
#endif
    // Logging zed("Zed Camera Pose", zedPtr->cameraPose.pose,3);
    // Logging("predNPose", predNPose,3);
}

void FeatureTracker::publishPoseCeres()
{
    poseEst = poseEst * poseEstFrame;
    poseEstFrameInv = poseEstFrame.inverse();
    prevWPose = zedPtr->cameraPose.pose;
    prevWPoseInv = zedPtr->cameraPose.poseInverse;
    zedPtr->cameraPose.setPose(poseEst);
    zedPtr->cameraPose.setInvPose(poseEst.inverse());
    predNPose = poseEst * (prevWPoseInv * poseEst);
    predNPoseInv = predNPose.inverse();
#if SAVEODOMETRYDATA
    saveData();
#endif
    // Logging zed("Zed Camera Pose", zedPtr->cameraPose.pose,3);
    // Logging("predNPose", predNPose,3);
}

void FeatureTracker::publishPose()
{
    poseEst = poseEst * poseEstFrame;
    poseEstFrameInv = poseEstFrame.inverse();
    prevWPose = zedPtr->cameraPose.pose;
    prevWPoseInv = zedPtr->cameraPose.poseInverse;
    zedPtr->cameraPose.setPose(poseEst);
    zedPtr->cameraPose.setInvPose(poseEst.inverse());
    predNPose = poseEst * (prevWPoseInv * poseEst);
    predNPoseInv = predNPose.inverse();
#if SAVEODOMETRYDATA
    saveData();
#endif
    // Logging zed("Zed Camera Pose", zedPtr->cameraPose.pose,3);
    // Logging("predNPose", predNPose,3);
}

void FeatureTracker::publishPoseTrial()
{
    const float velThresh {0.07f};
    static double pvx {}, pvy {}, pvz {};
    double px {},py {},pz {};
    px = poseEst(0,3);
    py = poseEst(1,3);
    pz = poseEst(2,3);
    Eigen::Matrix4d prePoseEst = poseEst;
    poseEst = poseEst * poseEstFrame;
    double vx {},vy {},vz {};
    vx = (poseEst(0,3) - px)/dt;
    vy = (poseEst(1,3) - py)/dt;
    vz = (poseEst(2,3) - pz)/dt;
    poseEstFrameInv = poseEstFrame.inverse();
    prevWPose = zedPtr->cameraPose.pose;
    prevWPoseInv = zedPtr->cameraPose.poseInverse;
    if ( curFrame != 1 )
    {

        if ( abs(vx - pvx) > velThresh || abs(vy - pvy) > velThresh || abs(vz - pvz) > velThresh )
        {
            poseEst = prePoseEst * prevposeEstFrame;
        }
        else
        {
            setVel(pvx, pvy, pvz, vx, vy, vz);
            prevposeEstFrame = poseEstFrame;
        }

    }
    else
    {
        setVel(pvx, pvy, pvz, vx, vy, vz);
        prevposeEstFrame = poseEstFrame;
    }
    predNPose = poseEst * (prevWPoseInv * poseEst);
    zedPtr->cameraPose.setPose(poseEst);
    zedPtr->cameraPose.setInvPose(poseEst.inverse());
#if SAVEODOMETRYDATA
    saveData();
#endif
    Logging zed("Zed Camera Pose", zedPtr->cameraPose.pose,3);
}

inline void FeatureTracker::setVel(double& pvx, double& pvy, double& pvz, double vx, double vy, double vz)
{
    pvx = vx;
    pvy = vy;
    pvz = vz;
}

void FeatureTracker::saveData()
{
    KeyFrame* closeKF = allFrames[0];

    std::vector<KeyFrame*>::iterator it;
    std::vector<KeyFrame*>::const_iterator end(allFrames.end());
    for ( it = allFrames.begin(); it != end; it ++)
    {
        KeyFrame* candKF = *it;
        Eigen::Matrix4d mat;
        if ( candKF->keyF )
        {
            mat = candKF->pose.pose.transpose();
            closeKF = candKF;
        }
        else
        {
            mat = (candKF->pose.refPose * closeKF->pose.getPose()).transpose();
        }

        for (int32_t i{0}; i < 12; i ++)
        {
            if ( i == 0 )
                datafile << mat(i);
            else
                datafile << " " << mat(i);
        }
        datafile << '\n';
    }

}

} // namespace vio_slam