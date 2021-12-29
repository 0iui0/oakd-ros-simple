#ifndef OAKD_MAIN_H
#define OAKD_MAIN_H

///// depthai - OAK-D
#include <depthai/depthai.hpp>

///// common headers
#include <string>
#include <vector>
#include <fstream>
#include <unistd.h>

///// ROS
#include <ros/ros.h>
#include <ros/package.h>
#include <std_msgs/Header.h>
#include <sensor_msgs/Image.h>
#include <sensor_msgs/Imu.h>
#include <sensor_msgs/CompressedImage.h>
#include <sensor_msgs/PointCloud2.h>
#include <oakd_ros/bboxes.h>

///// image processing
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <cv_bridge/cv_bridge.h>

///// pointcloud
#include <pcl/point_types.h>
#include <pcl/PCLPointCloud2.h>
#include <pcl/conversions.h>
#include <pcl_conversions/pcl_conversions.h>
#include <pcl/common/transforms.h>

///// utils
#include <signal.h>
void signal_handler(sig_atomic_t s) {
  std::cout << "You pressed Ctrl + C, exiting" << std::endl;
  exit(1);
}

using namespace std;

sensor_msgs::PointCloud2 cloud2msg(pcl::PointCloud<pcl::PointXYZ> cloud, std::string frame_id = "camera_link")
{
  sensor_msgs::PointCloud2 cloud_ROS;
  pcl::toROSMsg(cloud, cloud_ROS);
  cloud_ROS.header.frame_id = frame_id;
  cloud_ROS.header.stamp = ros::Time::now();
  return cloud_ROS;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
class oakd_ros_class{
  public:
    dai::Pipeline pipeline;
    ////////// auto can be used
    std::shared_ptr<dai::node::IMU> IMU_node            = pipeline.create<dai::node::IMU>();
    std::shared_ptr<dai::node::ColorCamera> camRgb      = pipeline.create<dai::node::ColorCamera>();
    std::shared_ptr<dai::node::MonoCamera> monoLeft     = pipeline.create<dai::node::MonoCamera>();
    std::shared_ptr<dai::node::MonoCamera> monoRight    = pipeline.create<dai::node::MonoCamera>();
    std::shared_ptr<dai::node::StereoDepth> stereodepth = pipeline.create<dai::node::StereoDepth>();
    std::shared_ptr<dai::node::XLinkOut> xoutIMU        = pipeline.create<dai::node::XLinkOut>();
    std::shared_ptr<dai::node::XLinkOut> xoutRgb        = pipeline.create<dai::node::XLinkOut>();
    std::shared_ptr<dai::node::XLinkOut> xoutLeft       = pipeline.create<dai::node::XLinkOut>();
    std::shared_ptr<dai::node::XLinkOut> xoutRight      = pipeline.create<dai::node::XLinkOut>();
    std::shared_ptr<dai::node::XLinkOut> xoutDepth      = pipeline.create<dai::node::XLinkOut>();
    std::shared_ptr<dai::node::XLinkOut> xoutInference  = pipeline.create<dai::node::XLinkOut>();
    std::shared_ptr<dai::node::XLinkOut> nnOut = pipeline.create<dai::node::XLinkOut>();

    std::shared_ptr<dai::DataOutputQueue> imuQueue;
    std::shared_ptr<dai::DataOutputQueue> rgbQueue;
    std::shared_ptr<dai::DataOutputQueue> leftQueue;
    std::shared_ptr<dai::DataOutputQueue> rightQueue;
    std::shared_ptr<dai::DataOutputQueue> DepthQueue;
    std::shared_ptr<dai::DataOutputQueue> nNetDataQueue;
    std::shared_ptr<dai::DataOutputQueue> nNetImgQueue;

    string path;

    sensor_msgs::Image l_img_msg, r_img_msg, rgb_img_msg, depth_img_msg, nn_img_msg;
    sensor_msgs::CompressedImage l_img_comp_msg, r_img_comp_msg, rgb_img_comp_msg, nn_img_comp_msg;

    bool initialized=false;
    bool get_imu, get_stereo_ir, get_rgb, get_stereo_depth, get_stereo_disparity, get_YOLO, get_pointcloud, get_raw, get_compressed;
    string topic_prefix, blob_file, class_file;
    int fps_IMU, infer_img_width, infer_img_height, class_num, thread_num, bilateral_sigma, depth_confidence;
    double fps_rgb_yolo, fps_stereo_depth, confidence_threshold, iou_threshold, pcl_max_range;
    vector<string> class_names;
    //for PCL, calib data
    vector<vector<float>> intrinsics;
    int width, height;

    ///// ros and tf
    ros::NodeHandle nh;
    ros::Publisher imu_pub, l_pub, l_comp_pub, r_pub, r_comp_pub, rgb_pub, rgb_comp_pub, d_pub, pcl_pub, nn_pub, nn_comp_pub, nn_bbox_pub;
    void main_initialize();



    oakd_ros_class(ros::NodeHandle& n) : nh(n){
      ///// params
      nh.param<std::string>("/topic_prefix", topic_prefix, "/oakd");
      nh.param("/fps_rgb_yolo", fps_rgb_yolo, 30.0);
      nh.param("/fps_stereo_depth", fps_stereo_depth, 30.0);
      nh.param("/fps_IMU", fps_IMU, 200);
      nh.param<bool>("/get_raw", get_raw, false);
      nh.param<bool>("/get_compressed", get_compressed, false);
      nh.param<bool>("/get_imu", get_imu, false);
      nh.param<bool>("/get_rgb", get_rgb, false);
      nh.param<bool>("/get_stereo_ir", get_stereo_ir, false);
      nh.param<bool>("/get_stereo_depth", get_stereo_depth, false);
      nh.param<bool>("/get_stereo_disparity", get_stereo_disparity, false);
      nh.param<bool>("/get_pointcloud", get_pointcloud, false);
      nh.param("/pcl_max_range", pcl_max_range, 6.0);
      nh.param<bool>("/get_YOLO", get_YOLO, false);
      nh.param("/thread_num", thread_num, 3);
      nh.param("/bilateral_sigma", bilateral_sigma, 500);
      nh.param("/depth_confidence", depth_confidence, 200);
      nh.param<std::string>("/blob_file", blob_file, "/blob_files/tiny-yolo-v4.blob");
      nh.param<std::string>("/class_file", class_file, "/blob_files/class.txt");
      nh.param("/infer_img_width", infer_img_width, 416);
      nh.param("/infer_img_height", infer_img_height, 416);
      nh.param("/class_num", class_num, 80);
      nh.param("/confidence_threshold", confidence_threshold, 0.7);
      nh.param("/iou_threshold", iou_threshold, 0.7);

      ///// sub pub
      if (get_rgb){
        if (get_raw)
          rgb_pub = nh.advertise<sensor_msgs::Image>(topic_prefix+"/rgb/image_raw", 10);
        if (get_compressed)
          rgb_comp_pub = nh.advertise<sensor_msgs::CompressedImage>(topic_prefix+"/rgb/image_raw/compressed", 10);
      }
      if (get_stereo_ir){
        if (get_raw){
          l_pub = nh.advertise<sensor_msgs::Image>(topic_prefix+"/stereo_ir/left/image_raw", 10);
          r_pub = nh.advertise<sensor_msgs::Image>(topic_prefix+"/stereo_ir/right/image_raw", 10);
        }
        if (get_compressed){
          l_comp_pub = nh.advertise<sensor_msgs::CompressedImage>(topic_prefix+"/stereo_ir/left/image_raw/compressed", 10);
          r_comp_pub = nh.advertise<sensor_msgs::CompressedImage>(topic_prefix+"/stereo_ir/right/image_raw/compressed", 10);
        }
      }
      if (get_stereo_depth)
        d_pub = nh.advertise<sensor_msgs::Image>(topic_prefix+"/depth/image_raw", 10);
      if (get_pointcloud)
        pcl_pub = nh.advertise<sensor_msgs::PointCloud2>(topic_prefix+"/pointcloud", 10);
      if (get_YOLO){
        nn_bbox_pub = nh.advertise<oakd_ros::bboxes>(topic_prefix+"/yolo/bboxes", 10);
        if (get_raw)
          nn_pub = nh.advertise<sensor_msgs::Image>(topic_prefix+"/yolo/image_raw", 10);
        if (get_compressed)
          nn_comp_pub = nh.advertise<sensor_msgs::CompressedImage>(topic_prefix+"/yolo/image_raw/compressed", 10);
      }
      if (get_imu)
        imu_pub = nh.advertise<sensor_msgs::Imu>(topic_prefix+"/imu",10);

      ///// Init
      path = ros::package::getPath("oakd_ros");
      main_initialize();
      ROS_WARN("class heritated, starting node...");
    }
};




//////////// can be separated into .cpp source file
void oakd_ros_class::main_initialize(){
  xoutIMU->setStreamName("imu");
  xoutRgb->setStreamName("rgb");
  xoutLeft->setStreamName("left");
  xoutRight->setStreamName("right");
  xoutDepth->setStreamName("depth");
  nnOut->setStreamName("detections");
  xoutInference->setStreamName("detected_img");

  if (get_imu){
    IMU_node->enableIMUSensor({dai::IMUSensor::ACCELEROMETER, dai::IMUSensor::GYROSCOPE_CALIBRATED, dai::IMUSensor::ROTATION_VECTOR}, fps_IMU);
    // IMU_node->setBatchReportThreshold(1);
    IMU_node->setMaxBatchReports(1);
    IMU_node->out.link(xoutIMU->input);
  }
  if(get_rgb){
    camRgb->setBoardSocket(dai::CameraBoardSocket::RGB);
    camRgb->setResolution(dai::ColorCameraProperties::SensorResolution::THE_1080_P);
    camRgb->setColorOrder(dai::ColorCameraProperties::ColorOrder::BGR);
    camRgb->setFps(fps_rgb_yolo);
    // camRgb->initialControl.setManualFocus(135);
    camRgb->setPreviewSize(640, 480);
    camRgb->setInterleaved(false);
    camRgb->preview.link(xoutRgb->input);
    
    if(get_YOLO){
      std::shared_ptr<dai::node::ImageManip> Manipulator  = pipeline.create<dai::node::ImageManip>();
      std::shared_ptr<dai::node::YoloDetectionNetwork> detectionNetwork = pipeline.create<dai::node::YoloDetectionNetwork>();
      camRgb->preview.link(Manipulator->inputImage);
      Manipulator->initialConfig.setResizeThumbnail(infer_img_width, infer_img_height);
      Manipulator->initialConfig.setFrameType(dai::ImgFrame::Type::BGR888p);
      Manipulator->inputImage.setBlocking(false);
      Manipulator->out.link(xoutInference->input);
      Manipulator->out.link(detectionNetwork->input);
      detectionNetwork->setBlobPath(path+blob_file);
      detectionNetwork->setNumInferenceThreads(thread_num);
      detectionNetwork->setConfidenceThreshold(confidence_threshold);
      detectionNetwork->setIouThreshold(iou_threshold);
      detectionNetwork->setNumClasses(class_num);
      detectionNetwork->input.setBlocking(false);
      detectionNetwork->setCoordinateSize(4);
      detectionNetwork->setAnchors({10, 14, 23, 27, 37, 58, 81, 82, 135, 169, 344, 319});
      detectionNetwork->setAnchorMasks({{"side26", {1, 2, 3}}, {"side13", {3, 4, 5}}});
      detectionNetwork->out.link(nnOut->input);
      // detectionNetwork->passthrough.link(xoutInference->input);
      ifstream readfile;
      readfile.open(path+class_file);
      while (!readfile.eof()){
        string str;
        getline(readfile, str);
        class_names.push_back(str);
      }
      readfile.close();
    }
  }

  if(get_stereo_ir || get_stereo_depth || get_pointcloud){
    monoLeft->setResolution(dai::MonoCameraProperties::SensorResolution::THE_480_P);
    monoLeft->setBoardSocket(dai::CameraBoardSocket::LEFT);
    monoLeft->setFps(fps_stereo_depth);
    monoRight->setResolution(dai::MonoCameraProperties::SensorResolution::THE_480_P);
    monoRight->setBoardSocket(dai::CameraBoardSocket::RIGHT);
    monoRight->setFps(fps_stereo_depth);

    stereodepth->initialConfig.setConfidenceThreshold(depth_confidence);
    stereodepth->setLeftRightCheck(true);
    stereodepth->initialConfig.setBilateralFilterSigma(bilateral_sigma);
    stereodepth->initialConfig.setMedianFilter(dai::MedianFilter::KERNEL_7x7);
    stereodepth->setDepthAlign(dai::CameraBoardSocket::RIGHT); //default: Right
      // stereo->setInputResolution(1280, 720);
    // stereodepth->setRectifyEdgeFillColor(0); // black, to better see the cutout
    // stereodepth->initialConfig.setLeftRightCheckThreshold(1);
    stereodepth->setExtendedDisparity(false);
    stereodepth->setSubpixel(true);
    monoLeft->out.link(stereodepth->left);
    monoRight->out.link(stereodepth->right);

    // monoLeft->out.link(xoutLeft->input);
    // monoRight->out.link(xoutRight->input);
    stereodepth->syncedLeft.link(xoutLeft->input);
    stereodepth->syncedRight.link(xoutRight->input);
    stereodepth->depth.link(xoutDepth->input);
  }

  initialized=true;
}



#endif