

#ifdef _MSC_VER
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#endif


#include <math.h>
#include <chrono>
#include <iostream>
#include <memory>

#include "rclcpp/clock.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp/time_source.hpp"
#include "sensor_msgs/msg/laser_scan.hpp"
#include "std_srvs/srv/empty.hpp"
#include <vector>
#include <iostream>
#include <string>
#include <signal.h>


#include <stdio.h>

#include <string> 
#include <sstream>
#include <algorithm>
#include <fstream>

#include "base/HcData.h"
#include "base/HcSDK.h"




std::string  g_strLidarID = "";


void sdkCallBackFunErrorCode(int iErrorCode)
{
	char buff[128] = { 0 };
	sprintf(buff, "Error code=%d\n",iErrorCode);
	printf(buff);
}

void sdkCallBackFunSecondInfo(tsSDKStatistic sInfo)
{
	std::string strFile = "";
	strFile = "FPS_" + g_strLidarID + ".csv";
	std::ofstream outFile;
	outFile.open(strFile, std::ios::app);

	char buff[128] = { 0 };
	sprintf(buff, "%lld,%d,%d,%lld,%0.2f,%d,%d,%d,%lld\n",
		sInfo.u64TimeStampS, sInfo.iNumPerPacket, sInfo.iGrayBytes, sInfo.u64FPS
		, sInfo.dRMS,  sInfo.iValid, sInfo.iInvalid, sInfo.iPacketPerSecond
		, sInfo.u64ErrorPacketCount);

	outFile.write(buff, strlen(buff));
	outFile.close();

	printf(buff);
	
}

void sdkCallBackFunPointCloud(LstPointCloud lstG)
{
 
	std::string strFile = "";
	strFile = "Raw_" + g_strLidarID + ".csv";

	std::ofstream outFile;
	outFile.open(strFile, std::ios::app);

	char buff[128] = { 0 };
	sprintf(buff, "---------------Size=%d\n", lstG.size());
	printf(buff);

    for(auto sInfo : lstG)
    {
		//if (sInfo.dAngle > 310 || sInfo.dAngle < 50)
		//{

			memset(buff, 0, 128);
			sprintf(buff, "%lld,%0.3f,%0.3f,%d,%d,%d,%d\n",
				sInfo.u64TimeStampNs, sInfo.dAngle, sInfo.dAngleRaw, sInfo.u16Dist, sInfo.bValid, sInfo.u16Speed, sInfo.u16Gray);

			outFile.write(buff, strlen(buff));

			//printf(buff);
		//}

		
    }
		
	outFile.close();
	
}


int getPort()
{
	printf("Please select COM:\n");
	int iPort = 3;
    std::cin >> iPort;
	return iPort;
}

int getBaud()
{
	printf("Please select COM baud:\n");
	int iBaud = 115200;
	std::cin >> iBaud;
	return iBaud;
}

std::string getLidarModel()
{
	printf("Please select Lidar model:\n");
	std::string str = "X2M";
	std::cin >> str;
	std::transform(str.begin(), str.end(), str.begin(), ::toupper);
	return str;
}

int main(int argc, char *argv[]) {
  rclcpp::init(argc, argv);

  auto node = rclcpp::Node::make_shared("hclidar_driver_ros2_node");

  RCLCPP_INFO(node->get_logger(), "Start  ROS2 Driver\n");

int rtn = 0;

    bool bPollMode = true;
    bool bDistQ2 = false;
    bool bLoop = false;

	std::string strVer = getSDKVersion();
    std::cout << "Main: SDK verion=" << strVer.c_str()<< std::endl;

    auto funErrorCode = std::bind(sdkCallBackFunErrorCode, std::placeholders::_1);
	setSDKCallBackFunErrorCode(funErrorCode);

    auto funSecondInfo = std::bind(sdkCallBackFunSecondInfo, std::placeholders::_1);
    setSDKCallBackFunSecondInfo(funSecondInfo);

    if(!bPollMode)//call back
    {
        auto funPointCloud = std::bind(sdkCallBackFunPointCloud, std::placeholders::_1);
        setSDKCallBackFunPointCloud(funPointCloud);

    }

    
    int iPort = 0;
    std::string strPort;
    strPort = "/dev/ttyUSB" + std::to_string(iPort);              // For Linux OS

    node->declare_parameter("port", "/dev/ttyUSB0");
    node->get_parameter("port", strPort);
    printf( "Port=%s\n" , strPort.c_str());

    int iBaud = 115200;
    node->declare_parameter("baudrate", 115200);
    node->get_parameter("baudrate", iBaud);
    printf( "baudrate=%d\n" , iBaud);

    //std::string strLidarModel = "X2M";
    std::string strLidarModel = "X1";
    node->declare_parameter("lidar_model", strLidarModel);
    node->get_parameter("lidar_model", strLidarModel);
    printf( "strLidarModel=%s\n" , strLidarModel.c_str());


    std::string frame_id = "laser_frame";
    node->declare_parameter("frame_id", "laser_frame");
    node->get_parameter("frame_id", frame_id);
    printf( "frame_id=%s\n" , frame_id.c_str());



    int iReadTimeoutms = 2;//10

    setSDKCircleDataMode();
    rtn = hcSDKInitialize(strPort.c_str(), strLidarModel.c_str(), iBaud, iReadTimeoutms, bDistQ2, bLoop, bPollMode);

    if (rtn != 1)
    {
      hcSDKUnInit();
      RCLCPP_ERROR(node->get_logger(), "SDK init failed\n");
      exit(0);
      return 0;
          
    }

    setSDKLidarPowerOn(true);

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    g_strLidarID = getSDKLidarID();
    
    
    printf( "Lidar ID=%s\n" , getSDKLidarID());
    printf( "Factory Info:%s\n" , getSDKFactoryInfo());
    printf( "Main: Firmware ver:%s\n", getSDKFirmwareVersion() );
    printf( "Main: Hardware ver:%s\n", getSDKHardwareVersion());
    printf( "Main: Lidar model:%s\n" , getSDKLidarModel() );

  
  auto laser_pub = node->create_publisher<sensor_msgs::msg::LaserScan>("scan", rclcpp::SensorDataQoS());

  
  float angle_max = 3.141592;
  node->declare_parameter("angle_max", 3.1415926f);
  node->get_parameter("angle_max", angle_max);

  float angle_min = -3.141592;
  node->declare_parameter("angle_min", -3.1415926f);
  node->get_parameter("angle_min", angle_min);

  printf( "Main: angle_min=%f\n" , angle_min );

  float range_max = 10.f;
  node->declare_parameter("range_max", 8.0f);
  node->get_parameter("range_max", range_max);

  float range_min = 0.1f;
  node->declare_parameter("range_min", 0.08f);
  node->get_parameter("range_min", range_min);


  rclcpp::WallRate loop_rate(100);

  rclcpp::Time  start_time = node->get_clock()->now();
  while (rclcpp::ok()) {

    LstPointCloud lstG;
				
				if (getSDKRxPointClouds(lstG))
				{

					if (lstG.size() > 0)
          {
                        rclcpp::Time   end_time =  node->get_clock()->now();
                        printf("start_time=%lu, end_time=%lu , diff_offset=%ld \n", 
							start_time.nanoseconds() , end_time.nanoseconds() , (end_time.nanoseconds()-start_time.nanoseconds())/1e6 );
                        
                        printf("Main: total Rx Points=%ld , \n", lstG.size());
                      
                        reverse(lstG.begin(),lstG.end());
                       
                        sensor_msgs::msg::LaserScan scan_msg;

                        rclcpp::Time start_scan_time = start_time;    
                                        
                        //start_scan_time. = start_time.sec;//微妙除以10的5次方
                        //start_scan_time.nsec = start_time.nsec;
                        scan_msg.header.stamp = start_scan_time;
                        scan_msg.header.frame_id = frame_id;
                        scan_msg.angle_min = angle_min; 
                        scan_msg.angle_max = angle_max; 
                        scan_msg.angle_increment = (scan_msg.angle_max - scan_msg.angle_min)/(lstG.size()- 1);       
                        
                       // uint64_t scan_time = m_PointTime * (lstG.size() - 1);
                        //scan_msg.scan_time =  static_cast<double>(scan_time * 1.0 / 1e9);
                        //scan_msg.time_increment =  scan_msg.scan_time/ (double)(lstG.size() - 1);
                        
                            
          
                        //uint64_t time_offset_ns = end_time.toNSec()-start_time.toNSec();
                        //printf("end_time-start_time ns = %ld , diff=%f \n", time_offset_ns , time_offset_ns/1000000000.f);
                        scan_msg.scan_time = static_cast<double>((end_time.nanoseconds()-start_time.nanoseconds())/1000000000.f);
                        scan_msg.time_increment =  scan_msg.scan_time/ (double)(lstG.size() - 1);
                        
                        scan_msg.range_min = range_min;
                        scan_msg.range_max = range_max;
                        scan_msg.ranges.resize(lstG.size());
                        scan_msg.intensities.resize(lstG.size());  

						start_time = end_time;
                        
                        double rad_angel;
          
                        for(int i=0; i < lstG.size(); i++)
                        {
                            if(lstG[i].dAngle > 180.0)
                            {
                                rad_angel= (360-lstG[i].dAngle)*3.141592 /180.0f;
                            }
                            else
                            {
                                rad_angel=-1*lstG[i].dAngle*3.141592 /180.0f;
                            }
                            
                            int index = std::ceil((rad_angel - angle_min)/scan_msg.angle_increment);

                            //printf("Main: index=%d , ranges=%d, valid=%d,angle=%f,angle_increment=%f,angle_min=%f\n", 
                            //index,lstG[i].u16Dist,lstG[i].bValid,rad_angel,scan_msg.angle_increment,angle_min);
                            
                            if(index >=0 && index < lstG.size()) 
                            {
                                    //如果是无效点，即杂，则置0
                                if(!lstG[i].bValid)
                                {
                                      scan_msg.ranges[index] = 0.0;
                                }
                                else
                                {

                                    scan_msg.ranges[index] = (lstG[i].u16Dist)/1000.0f;
                                }
                                    
                                scan_msg.intensities[index] = lstG[i].u16Gray;

                                
                            }
                        }
                        laser_pub->publish(scan_msg);
                        
          }
				}
				else
				{
					int iError = getSDKLastErrCode();
					if (iError != LIDAR_SUCCESS)
					{
						printf( "Main: Poll Rx Points error code=%d\n", iError );
						switch (iError)
						{
						case ERR_SHARK_MOTOR_BLOCKED:
							break;
						case ERR_SHARK_INVALID_POINTS:
							break;
						case ERR_LIDAR_SPEED_LOW:
							break;
						case ERR_LIDAR_SPEED_HIGH:
							break;
						case ERR_DISCONNECTED:
							break;
						case ERR_LIDAR_FPS_INVALID:
							break;
						default:
							break;
						}
					}
				}
    if(!rclcpp::ok()) {
      break;
    }
    rclcpp::spin_some(node);
    loop_rate.sleep();
				                
    }

   


  RCLCPP_INFO(node->get_logger(), "Now HCLIDAR is stopping .......");

  rclcpp::shutdown();

  return 0;

}
