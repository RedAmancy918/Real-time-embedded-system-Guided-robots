/*
version 1.1  Author: Geo
Using libcamera to connect OpenCV. Fail

version 1.2  
Using Prof libcam2opencv.h rewrite cpp fail

version 1.3
add callback function MK" servo2 control the up and down 

version 1.4 
searching prograss need a loop control and callback function

version 1.5
All function is OK, but the searching prograss is not perfect, need to be improved

version 1.51
add the searching prograss, but the searching prograss is not perfect, change red color to green color(ok fine forget it)

version 1.52 date: 17/04/2024 01:22
modify the specific color HSV value(MD add red color cant detect ,and the HSV value is not i expected)
*/
#include <iostream>
#include <opencv2/opencv.hpp>
#include "libcam2opencv.h"
#include "arm_sys.h"

using namespace cv;
using namespace std;

class MyCallback : public Libcam2OpenCV::Callback {
public:
    mg90s servo0, servo1, servo2, servo3;  // 初始化四个舵机控制器
    bool redDetected = false;  // 用于标志是否检测到红色物体

    MyCallback() : servo0(21), servo1(20), servo2(19), servo3(6) {
        initiateSLE(); // 启动SLE程序
    }

    // SLE程序: 0号电机进行左右转动，同时检测红色物体
    void initiateSLE() {
        if (!redDetected) {
            servo0.setTargetAngleAsync(-90, [this]() { // 0号电机左转90度
                servo0.setTargetAngleAsync(180, [this]() { // 接着右转180度
                    servo0.setTargetAngleAsync(-90, [this]() { // 最后左转回90度
                        initiateSLE(); // 如果没有检测到红色物体，继续SLE程序
                    });
                });
            });
        }
    }

    // 处理每一帧图像，检测红色物体或进行物体追踪
    virtual void hasFrame(const cv::Mat &frame, const libcamera::ControlList &metadata) override {
        if (!redDetected) {
            processFrameForRedDetection(frame);  // 继续寻找红色物体
        } else {
            processFrame(frame);  // 如果检测到红色物体，进入ALE程序
        }
    }

    // 检测帧中的红色物体，切换到ALE程序
    void processFrameForRedDetection(const cv::Mat &frame) {
        Mat hsv, redMask1, redMask2;
        cvtColor(frame, hsv, COLOR_BGR2HSV);  // 将图像转换为HSV色彩空间进行处理
        inRange(hsv, Scalar(110, 150, 200), Scalar(130, 180, 240), redMask1);
        inRange(hsv, Scalar(170, 70, 50), Scalar(180, 255, 255), redMask2);

        Mat redMask = redMask1 | redMask2;
        if (countNonZero(redMask) > 0) {
            redDetected = true;  // 如果检测到红色物体，停止SLE，开始ALE
        }
    }

    // ALE程序: 对象居中调整
    void processFrame(const cv::Mat &frame) {
        Mat hsv, redMask1, redMask2, redMask;
        cvtColor(frame, hsv, COLOR_BGR2HSV);  // 再次转换色彩空间进行红色检测
        inRange(hsv, Scalar(110, 150, 200), Scalar(130, 180, 240), redMask1);
        inRange(hsv, Scalar(170, 70, 50), Scalar(180, 255, 255), redMask2);
        redMask = redMask1 | redMask2;

        Mat kernel = getStructuringElement(MORPH_ELLIPSE, Size(5, 5));  // 形态学处理，改善掩模
        morphologyEx(redMask, redMask, MORPH_CLOSE, kernel);  // 进行闭操作

        vector<vector<Point>> contours;
        findContours(redMask, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);  // 轮廓检测

        if (!contours.empty()) {
            auto largestContour = max_element(contours.begin(), contours.end(),
                                              [](const vector<Point>& a, const vector<Point>& b) {
                                                  return contourArea(a) < contourArea(b);
                                              });
            Rect objectBoundingRectangle = boundingRect(*largestContour);  // 计算包围矩形
            rectangle(frame, objectBoundingRectangle, Scalar(0, 255, 0), 2);  // 在帧上画出矩形

            handleServo(frame, objectBoundingRectangle);  // 处理舵机调整
        }

        imshow("Detection", frame);  // 显示结果
        if (waitKey(10) == 27) {
            cout << "Exiting..." << endl;
            exit(0);  // 按ESC退出程序
        }
    }

    // 根据对象位置调整舵机，进行ALE和BLE程序
    void handleServo(const Mat& frame, const Rect& rect) {
        int frameCenterX = frame.cols / 2;
        int frameCenterY = frame.rows / 2;
        int objCenterX = rect.x + rect.width / 2;
        int objCenterY = rect.y + rect.height / 2;
        double areaRatio = (double)rect.area() / (frame.rows * frame.cols);

        // ALE条件: 对象居中调整
        checkAndAdjustCentering(objCenterX, frameCenterX, objCenterY, frameCenterY);

        // BLE条件: 根据对象大小调整
        if (areaRatio < 0.75) {
            adjustForScale(objCenterX, frameCenterX, objCenterY, frameCenterY);
        } else {
            // CLE条件: 最终调整
            performFinalAdjustments();
        }
    }

    // 根据对象位置中心调整舵机
    void checkAndAdjustCentering(int objCenterX, int frameCenterX, int objCenterY, int frameCenterY) {
        if (abs(objCenterX - frameCenterX) > 20) {
            if (objCenterX < frameCenterX) {
                servo0.setTargetAngleAsync(-10, []() { cout << "Adjusting left." << endl; });
            } else {
                servo0.setTargetAngleAsync(10, []() { cout << "Adjusting right." << endl; });
            }
        }

        if (abs(objCenterY - frameCenterY) > 20) {
            if (objCenterY < frameCenterY) {
                servo1.setTargetAngleAsync(5, []() { cout << "Adjusting up." << endl; });
            } else {
                servo1.setTargetAngleAsync(-5, []() { cout << "Adjusting down." << endl; });
            }
        }
    }

    // BLE程序中调整2号电机控制前进，同时保持ALE条件
    void adjustForScale(int objCenterX, int frameCenterX, int objCenterY, int frameCenterY) {
        servo2.setTargetAngleAsync(10, []() { cout << "Moving forward." << endl; });
        checkAndAdjustCentering(objCenterX, frameCenterX, objCenterY, frameCenterY);
    }

    // CLE程序: 执行最终抓取动作并重置舵机位置
    void performFinalAdjustments() {
        servo3.setTargetAngleAsync(90, []() { cout << "Gripping." << endl; });
        servo0.setTargetAngleAsync(90, []() { cout << "Centering." << endl; });  // 回到中心位置
        servo3.setTargetAngleAsync(-90, []() { cout << "Releasing grip." << endl; });
        exit(0); // 完成后退出程序
    }
};

int main() {
    Libcam2OpenCV cameraInterface;
    MyCallback frameProcessor;

    cameraInterface.registerCallback(&frameProcessor);

    Libcam2OpenCVSettings settings;
    settings.width = 640;  // 设定摄像头宽度
    settings.height = 480;  // 设定摄像头高度
    settings.framerate = 30;  // 设定帧率

    try {
        cameraInterface.start(settings);
        cout << "Press enter to exit..." << endl;
        cin.get();  // 等待用户输入以退出
        cameraInterface.stop();
    } catch (const std::exception& e) {
        cerr << "Exception caught during camera operation: " << e.what() << endl;
        return -1;
    }

    return 0;
}
