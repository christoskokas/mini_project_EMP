#include "FeatureDrawer.h"
#include <nav_msgs/Odometry.h>
#include <math.h>       /* tan */
#include <boost/assign.hpp>
#include <tf2/LinearMath/Matrix3x3.h>
#include <tf2/LinearMath/Transform.h>
#include <tf2/LinearMath/Vector3.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.h>
#include "ceres/ceres.h"
#include "Optimizer.h"


#define PI 3.14159265


static const std::string OPENCV_WINDOW = "Features Detected";

namespace vio_slam
{

bool matIsEqual(const cv::Mat Mat1, const cv::Mat Mat2)
{
  if( Mat1.dims == Mat2.dims && 
    Mat1.size == Mat2.size && 
    Mat1.elemSize() == Mat2.elemSize())
  {
    if( Mat1.isContinuous() && Mat2.isContinuous())
    {
      return 0==memcmp( Mat1.ptr(), Mat2.ptr(), Mat1.total()*Mat1.elemSize());
    }
    else
    {
      const cv::Mat* arrays[] = {&Mat1, &Mat2, 0};
      uchar* ptrs[2];
      cv::NAryMatIterator it( arrays, ptrs, 2);
      for(unsigned int p = 0; p < it.nplanes; p++, ++it)
        if( 0!=memcmp( it.ptrs[0], it.ptrs[1], it.size*Mat1.elemSize()) )
          return false;

      return true;
    }
  }

  return false;
}

void FeatureDrawer::printMat(cv::Mat matrix)
{
  for (size_t i = 0; i < matrix.cols; i++)
  {
    for (size_t j = 0; j < matrix.rows; j++)
    {
      std::cout << matrix.at<double>(j,i) << "  ";
    }
    std::cout << '\n';
  }
  
}


FeatureDrawer::FeatureDrawer(ros::NodeHandle *nh, const Zed_Camera* zedptr) : m_it(*nh), img_sync(MySyncPolicy(10), leftIm, rightIm)
{
    this->zedcamera = zedptr;
    if (!zedcamera->rectified)
    {
      setUndistortMap(nh);
    }
    std::cout << '\n';
    cv::Mat rod;
    cv::Rodrigues(R1, rod);
    camera[0] = rod.at<double>(0);
    camera[1] = rod.at<double>(1);
    camera[2] = rod.at<double>(2);
    camera[3] = zedcamera->sensorsTranslate.at<double>(0);
    camera[4] = zedcamera->sensorsTranslate.at<double>(1);
    camera[5] = zedcamera->sensorsTranslate.at<double>(2);
    std::string position_path;
    nh->getParam("ground_truth_path", position_path);
    leftIm.subscribe(*nh, zedcamera->cameraLeft.path, 1);
    rightIm.subscribe(*nh, zedcamera->cameraRight.path, 1);
    img_sync.registerCallback(boost::bind(&FeatureDrawer::featureDetectionCallback, this, _1, _2));
    mImageMatches = m_it.advertise("/camera/matches", 1);
    pose_pub = nh->advertise<nav_msgs::Odometry>(position_path,1);
}

void FeatureDrawer::featureDetectionCallback(const sensor_msgs::ImageConstPtr& lIm, const sensor_msgs::ImageConstPtr& rIm)
{
    prevTime = ros::Time::now();
    leftImage.image = setImage(lIm);
    rightImage.image = setImage(rIm);
    if (!zedcamera->rectified)
    {
      cv::remap(leftImage.image, leftImage.image, rmap[0][0], rmap[0][1], cv::INTER_LINEAR);
      cv::remap(rightImage.image, rightImage.image, rmap[1][0], rmap[1][1], cv::INTER_LINEAR);
    }
    allMatches(lIm->header);
    
    
    firstImage = false;
}

void FeatureDrawer::setUndistortMap(ros::NodeHandle *nh)
{
    cv::Size imgSize = cv::Size(zedcamera->mWidth, zedcamera->mHeight);
    cv::stereoRectify(zedcamera->cameraLeft.cameraMatrix, zedcamera->cameraLeft.distCoeffs, zedcamera->cameraRight.cameraMatrix, zedcamera->cameraRight.distCoeffs, imgSize, zedcamera->sensorsRotate, zedcamera->sensorsTranslate, R1, R2, P1, P2, Q);
    cv::initUndistortRectifyMap(zedcamera->cameraLeft.cameraMatrix, zedcamera->cameraLeft.distCoeffs, R1, P1, imgSize, CV_32FC1, rmap[0][0], rmap[0][1]);
    cv::initUndistortRectifyMap(zedcamera->cameraRight.cameraMatrix, zedcamera->cameraRight.distCoeffs, R2, P2, imgSize, CV_32FC1, rmap[1][0], rmap[1][1]);
    
}

cv::Mat FeatureDrawer::calculateFeaturePosition(const std::vector<cv::DMatch>& matches)
{

  cv::Mat points4D(4,1,CV_64F);
  if ( matches.size() > 0 )
  {
    std::vector<cv::Point2d> pointsL;
    std::vector<cv::Point2d> pointsR;
    for (size_t i = 0; i < matches.size(); i++)
    {
      pointsL.push_back(leftImage.keypoints[matches[i].queryIdx].pt);
      pointsR.push_back(rightImage.keypoints[matches[i].trainIdx].pt);
    }
    cv::triangulatePoints(P1, P2, pointsL, pointsR, points4D);
    cv::Mat points3D(3, points4D.cols,CV_64F);
    for (size_t i = 0; i < points4D.cols; i++)
    {
      for (size_t j = 0; j < 3; j++)
      {
        points3D.at<double>(j,i) = points4D.at<double>(j,i)/points4D.at<double>(3,i);
      }
    }
    return points3D;
  }
  return points4D;
}

void Features::findFeatures()
{
    cv::Ptr<cv::FeatureDetector> detector = cv::ORB::create();
    // detect features and descriptor
    detector->detectAndCompute( image, cv::Mat(), keypoints, descriptors);
}

void FeatureDrawer::allMatches(const std_msgs::Header& header)
{
    bool LR = true;
    std::vector<cv::DMatch> matches = leftImage.findMatches(rightImage, header, mImageMatches, LR);
    cv::Mat points3D = calculateFeaturePosition(matches);
    if (!firstImage)
    {
      LR = false;
      std::vector<cv::DMatch> matchesLpL = leftImage.findMatches(previousLeftImage, header, mImageMatches, LR);
      keepMatches(matches, matchesLpL, previousLeftImage, points3D, true);
      publishMovement(header);
    }
    
    setPrevious(points3D);

}

void FeatureDrawer::keepMatches(const std::vector<cv::DMatch>& matches, const std::vector<cv::DMatch>& matches2, const vio_slam::Features& secondImage, const cv::Mat& points3D, bool left)
{
  std::vector<cv::DMatch> matched;
  ceres::Problem problem;
  ceres::LossFunction* lossfunction = NULL;
  for (size_t i = 0; i < matches.size(); i++)
  {
    for (size_t j = 0; j < matches2.size(); j++)
    {
      if ((left && (leftImage.keypoints[matches[i].queryIdx].pt.x == leftImage.keypoints[matches2[j].queryIdx].pt.x) && (leftImage.keypoints[matches[i].queryIdx].pt.y == leftImage.keypoints[matches2[j].queryIdx].pt.y)) || (!left && (secondImage.keypoints[matches[i].trainIdx].pt.x == secondImage.keypoints[matches2[j].trainIdx].pt.x) && (secondImage.keypoints[matches[i].trainIdx].pt.y == secondImage.keypoints[matches2[j].trainIdx].pt.y)))
      {
        double x = points3D.at<double>(0,i);
        double y = points3D.at<double>(1,i);
        double z = points3D.at<double>(2,i);
        double xp = previouspoints3D.at<double>(0,j);
        double yp = previouspoints3D.at<double>(1,j);
        double zp = previouspoints3D.at<double>(2,j);
        if ((abs(x) < 100) && (abs(y) < 100) && (abs(z) < 100) && !isnan(abs(x)) && !isnan(abs(y)) && !isnan(abs(z)) && !isinf(abs(x)) && !isinf(abs(y)) && !isinf(abs(z)))
        {
          Eigen::Vector3d p3d(x, y, z);
          Eigen::Vector3d pp3d(xp, yp, zp);
          // std::cout << p3d << '\n';
          // std::cout << '\n';
          ceres::CostFunction* costfunction = Reprojection3dError::Create(pp3d, p3d);
          problem.AddResidualBlock(costfunction, lossfunction, camera);
        }
        else
        {

        }
        break;
      }
    }
  }
  ceres::Solver::Options options;
  options.linear_solver_type = ceres::SPARSE_NORMAL_CHOLESKY;
  options.max_num_iterations = 100;
  options.trust_region_strategy_type = ceres::LEVENBERG_MARQUARDT;
  options.minimizer_progress_to_stdout = false;
  ceres::Solver::Summary summary;
  ceres::Solve(options, &problem, &summary);
  // std::cout << summary.BriefReport() << std::endl;
  // std::cout << "After Optimizing: "  << std::endl;
  
  double quat[4];
  ceres::AngleAxisToQuaternion(camera, quat);
  Eigen::Quaterniond q(quat[0], quat[1], quat[2], quat[3]);
  Eigen::Isometry3d Transform(q.matrix());
  Transform.pretranslate(Eigen::Vector3d(camera[3], camera[4], camera[5]));
  T = Transform.matrix();
}

void FeatureDrawer::publishMovement(const std_msgs::Header& header)
{
  nav_msgs::Odometry position;
  if (abs(T(0,3)) < 100 && abs(T(1,3)) < 100 && abs(T(2,3)) < 100)
  {
    sumsMovement[0] += T(0,3);
    sumsMovement[1] += T(1,3);
    sumsMovement[2] += T(2,3);
    Eigen::Matrix3d Rot;
    {
      // Eigen::Matrix4d temp = previousT;
      // for (size_t i = 0; i < 3; i++)
      // {
      //   for (size_t j = 0; j < 4; j++)
      //   {
      //     previousT(i,j) = temp(i,0)*T(0,j) + temp(i,1)*T(1,j) + temp(i,2)*T(2,j) + temp(i,3)*T(3,j);
      //     if (j < 3)
      //       Rot(i,j) = previousT(i,j);
      //   }
      // }
      previousT = previousT * T;
    }
    Eigen::Quaterniond quat(previousT.topLeftCorner<3,3>());
    // std::cout << "T=\n" << T << std::endl;
    // std::cout << "Tprev=\n" << previousT << std::endl;
    // std::cout << "Rot=\n" << Rot << std::endl;
    // std::cout << "quat=\n" << quat.x() << " " << quat.y() << " " << quat.z() << " " << quat.w() << std::endl;
    // ceres::AngleAxisToQuaternion(camera, quat);
    tf::poseTFToMsg(tf::Pose(tf::Quaternion(quat.x(),quat.y(),quat.z(),quat.w()),  tf::Vector3(previousT(0,3), previousT(1,3), previousT(2,3))), position.pose.pose); //Aria returns pose in mm.
    position.pose.covariance =  boost::assign::list_of(1e-3) (0) (0)  (0)  (0)  (0)
                                                        (0) (1e-3)  (0)  (0)  (0)  (0)
                                                        (0)   (0)  (1e6) (0)  (0)  (0)
                                                        (0)   (0)   (0) (1e6) (0)  (0)
                                                        (0)   (0)   (0)  (0) (1e6) (0)
                                                        (0)   (0)   (0)  (0)  (0)  (1e3) ;

    position.twist.twist.linear.x = 0.0;                  //(sumsMovement[0]-previoussumsMovement[0])*15 //15 fps
    position.twist.twist.angular.z = 0.0;
    position.twist.covariance =  boost::assign::list_of(1e-3) (0)   (0)  (0)  (0)  (0)
                                                        (0) (1e-3)  (0)  (0)  (0)  (0)
                                                        (0)   (0)  (1e6) (0)  (0)  (0)
                                                        (0)   (0)   (0) (1e6) (0)  (0)
                                                        (0)   (0)   (0)  (0) (1e6) (0)
                                                        (0)   (0)   (0)  (0)  (0)  (1e3) ; 

    position.header.frame_id = header.frame_id;
    position.header.stamp = ros::Time::now();
    pose_pub.publish(position);
  }
}

std::vector<cv::DMatch> Features::findMatches(Features& secondImage, const std_msgs::Header& header, image_transport::Publisher& mImageMatches, bool LR)
{
    findFeatures();
    secondImage.findFeatures();
    
    if ( descriptors.empty() )
      cvError(0,"MatchFinder","1st descriptor empty",__FILE__,__LINE__);    
    if ( secondImage.descriptors.empty() )
      cvError(0,"MatchFinder","2nd descriptor empty",__FILE__,__LINE__);
    std::vector< std::vector<cv::DMatch> > matches;
    cv::FlannBasedMatcher matcher = cv::FlannBasedMatcher(cv::makePtr<cv::flann::LshIndexParams>(12, 20, 2));
    // cv::BFMatcher matcher(cv::NORM_HAMMING);  
    matcher.knnMatch(descriptors, secondImage.descriptors, matches, 2);
    std::vector<cv::KeyPoint> matched1, matched2;
    std::vector<cv::Point2f> pointl, pointr;
    for(size_t i = 0; i < matches.size(); i++) 
    {
      if(matches[i].size() != 0)
      {
        cv::DMatch first = matches[i][0];
        float dist1 = matches[i][0].distance;
        float dist2 = matches[i][1].distance;

        if(dist1 < 0.8f * dist2) 
        {

          matched1.push_back(keypoints[first.queryIdx]);
          matched2.push_back(secondImage.keypoints[first.trainIdx]);
          pointl.push_back(keypoints[first.queryIdx].pt);
          pointr.push_back(secondImage.keypoints[first.trainIdx].pt);
          
          
        }
      }

    }
    std::vector<cv::KeyPoint> inliers1, inliers2;
    std::vector<cv::DMatch> good_matches;
    cv::Mat h = findHomography( pointl, pointr, cv::RANSAC);
    if (h.rows == 3)
    {
      for(size_t i = 0; i < matched1.size(); i++) 
      {
        
        cv::Mat col = cv::Mat::ones(3, 1, CV_64F);
        col.at<double>(0) = matched1[i].pt.x;
        col.at<double>(1) = matched1[i].pt.y;
        col = h * col;
        col /= col.at<double>(2);
        double dist = sqrt( pow(col.at<double>(0) - matched2[i].pt.x, 2) +
                            pow(col.at<double>(1) - matched2[i].pt.y, 2));
        if(dist < 2.5f) {
            int new_i = static_cast<int>(inliers1.size());
            inliers1.push_back(matched1[i]);
            inliers2.push_back(matched2[i]);
            good_matches.push_back(cv::DMatch(new_i, new_i, 0));
        }
      
      }
      cv::Mat img_matches;
      cv_bridge::CvImage out_msg;
      drawMatches( image, inliers1, secondImage.image, inliers2, good_matches, img_matches, cv::Scalar::all(-1),
            cv::Scalar::all(-1), std::vector<char>(), cv::DrawMatchesFlags::NOT_DRAW_SINGLE_POINTS );
      if(!LR)
      {
        out_msg.header   = header; // Same timestamp and tf frame as input image
        out_msg.encoding = sensor_msgs::image_encodings::RGB8; // Or whatever
        out_msg.image    = img_matches; // Your cv::Mat
        mImageMatches.publish(out_msg.toImageMsg());
      }
    }
    
  

    return good_matches;
}

void FeatureDrawer::setPrevious(cv::Mat& points3D)
{

    // std::cout << "MATRICES EQUAL : " << matIsEqual(previousLeftImage.image, leftImage.image) << '\n';
    previousLeftImage.image = leftImage.image;

    // std::cout << "MATRICES EQUAL AFTER : " << matIsEqual(previousLeftImage.image, leftImage.image) << '\n';
    previousRightImage.image = rightImage.image;
    previouspoints3D = points3D;
    // std::cout << "MATRICES EQUAL AFTER : " << matIsEqual(previouspoints3D, points3D) << '\n';
}

cv::Mat FeatureDrawer::setImage(const sensor_msgs::ImageConstPtr& imageRef)
{
    cv_bridge::CvImagePtr cv_ptr;
    try
    {
      cv_ptr = cv_bridge::toCvCopy(imageRef, sensor_msgs::image_encodings::RGB8);
    }
    catch (cv_bridge::Exception& e)
    {
      ROS_ERROR("cv_bridge exception: %s", e.what());
      return cv::Mat();
    }
    cv::Mat image;
    cv::cvtColor(cv_ptr->image, image, cv::COLOR_BGR2GRAY);
    return image;
}

FeatureDrawer::~FeatureDrawer()
{
    cv::destroyWindow(OPENCV_WINDOW);
}

} //namespace vio_slam