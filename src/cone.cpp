#include <iostream>
#include <math.h>
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <ros/ros.h>
#include <image_transport/image_transport.h>
#include <cv_bridge/cv_bridge.h>
#include "std_msgs/Float32.h"
#include "std_msgs/Int64.h"
#include "semear_ptr/Cone.h"

using namespace cv;
using namespace std;

void colorChanges(Mat&, Mat&);

class ImageConverter
{
    ros::NodeHandle nh_;
    image_transport::ImageTransport it_;
    image_transport::Subscriber image_sub_;

    public:

    bool foi_processado_;
    bool vejo_cone_;
    int64 pos_rel_cone_;

    ImageConverter()
    : it_(nh_)
    {
        foi_processado_=false;
        // Recebe a imagem da camera
        image_sub_ = it_.subscribe("/usb_cam/image_raw", 10, &ImageConverter::coneCallback, this);

        // Janelas
        namedWindow("Original", CV_WINDOW_NORMAL);
        namedWindow("Thresholded Image", CV_WINDOW_NORMAL); 
        namedWindow("Contornos", CV_WINDOW_NORMAL);
    }

    ~ImageConverter()
    {
        destroyWindow("Original");
        destroyWindow("Thresholded Image"); 
        destroyWindow("Contornos");
    }

    void coneCallback(const sensor_msgs::ImageConstPtr& msg)
    {
        cv_bridge::CvImagePtr cv_ptr;
        try
        {
            cv_ptr = cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::BGR8);
        }
        catch (cv_bridge::Exception& e)
        {
            ROS_ERROR("cv_bridge exception: %s", e.what());
            return;
        }

        Mat src, imgHSV, imgHist1, imgHist2, imgCone;
        vector<vector<Point> > contours;
        vector<Vec4i> hierarchy;
        RNG rng(12345);
    
        src = cv_ptr->image;

        // Faz algumas conversoes e manipulacoes de cor na imagem original
        colorChanges(src, imgHSV);

        // Encontra os contornos na imagem
        findContours( imgHSV, contours, hierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE, Point(0, 0) );

        vector<vector<Point> > contours_poly( contours.size() );
        vector<vector<Point> > contours_selected;
        vector<Rect> boundRect( contours.size() );
        vector<Point2f>center( contours.size() );
        vector<float>radius( contours.size() );
        Mat drawing = src.clone();
        float min_area = (src.rows * src.cols) * 0.01;
        for( size_t i = 0; i < contours.size(); i++ )
        {
            approxPolyDP( Mat(contours[i]), contours_poly[i], 3, true );
            boundRect[i] = boundingRect( Mat(contours_poly[i]) );
            if( (boundRect[i].width * boundRect[i].height)  >= min_area )
            {
                drawContours( drawing, contours_poly, (int)i, Scalar( 180,255,0 ), 3, 8, vector<Vec4i>(), 0, Point() );
                rectangle( drawing, boundRect[i].tl(), boundRect[i].br(), Scalar( 255,0,255 ), 3, 8, 0 );
                contours_selected.push_back(contours_poly[i]);
                //cout << "X do contorno: " << boundRect[i].x << endl;
            }
        }

        
        //cout << "   -> Numero de contornos: " << contours_selected.size() << endl;
        //cout << endl;

        vector<Rect> boundRect_selected( contours_selected.size() );
        for( size_t i = 0; i < contours_selected.size(); i++)
            boundRect_selected[i] = boundingRect( Mat(contours_selected[i]) );

        vector<vector<Point> > contours_cone;
        Rect boundRect_aux;
        float cone_error = 0.05 * src.cols;
        for( size_t i = 0; i < contours_selected.size(); i++)
        {
            if(i == 0)
            {
                contours_cone.push_back(contours_selected[i]);
                boundRect_aux = boundingRect( Mat(contours_selected[i]) );
            }
            else if( (boundRect_selected[i].x > (boundRect_aux.x - cone_error ))
                        && (boundRect_selected[i].x < (boundRect_aux.x + cone_error )) )
                contours_cone.push_back(contours_selected[i]);
        }

        int xMedio_cone = 0;
        vector<Rect> boundRect_cone( contours_cone.size() );
        for( size_t i = 0; i < contours_cone.size(); i++)
        {
            boundRect_cone[i] = boundingRect( Mat(contours_cone[i]) );
            drawContours( drawing, contours_cone, (int)i, Scalar( 180,255,0 ), 3, 8, vector<Vec4i>(), 0, Point() );
            rectangle( drawing, boundRect_cone[i].tl(), boundRect_cone[i].br(), Scalar( 255,0,255 ), 5, 8, 0 );
            xMedio_cone += boundRect_cone[i].x;
        }

        if(contours_cone.size() > 0)
        {
            xMedio_cone = xMedio_cone / contours_cone.size();
            cout << "> X medio do cone: " << xMedio_cone << endl;
            float posCone =  ( (xMedio_cone - (src.cols/2)) * 90 ) / (src.cols/2);
            cout << "> Posicao relativa do cone: " << posCone << endl;
            cout << endl;

            vejo_cone_ = true;
            pos_rel_cone_ = (int) posCone;
        }
        else
            vejo_cone_ = false;
        
        foi_processado_ = true;

        imshow("Original", src);
        imshow("Thresholded Image", imgHSV);
        imshow("Contornos", drawing);

        //waitKey(0);
        waitKey(3); // para teste
    }
};

bool checa_cone(semear_ptr::Cone::Request &req,
                semear_ptr::Cone::Response &res)
{
    ImageConverter ic;

    //while( ic.foi_processado_ == false){
    while( waitKey(3) != 27 ){ // para teste
        ros::Duration(0.1).sleep();
        ros::spinOnce();
    }

    res.vejo_cone = ic.vejo_cone_;
    res.pos_rel_cone = ic.pos_rel_cone_;
    
    return true;
}

int main(int argc, char** argv)
{
    ros::init(argc, argv, "cone_vision");

    ros::NodeHandle n;
    ros::ServiceServer service = n.advertiseService("cone_vision", checa_cone);

    ros::spin();
    return 0;
}

void colorChanges(Mat &in, Mat &out)
{
    int iLowH = 0;
    //int iHighH = 30; // cone.jpg e cone2.jpg
    //int iHighH = 18; // cone3.jpg e cone4.jpg
    int iHighH = 20; // video cone
    //int iHighH = 10;


    //int iLowS = 0; // cone.jpg, cone2.jpg e cone3.jpg
    //int iLowS = 125; // cone4.jpg
    int iLowS = 70; // video cone
    //int iLowS = 99; 
    int iHighS = 255;


    int iLowV = 0;
    int iHighV = 255;

    cvtColor(in, out, COLOR_BGR2HSV); //Convert the captured frame from BGR to HSV

    inRange(out, Scalar(iLowH, iLowS, iLowV), Scalar(iHighH, iHighS, iHighV), out); //Threshold the image
        
    //morphological opening (remove small objects from the foreground)
    erode(out, out, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)) );
    dilate(out, out, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)) ); 

    //morphological closing (fill small holes in the foreground)
    dilate(out, out, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)) ); 
    erode(out, out, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)) );
}