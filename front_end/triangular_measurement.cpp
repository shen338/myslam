#include <iostream>
#include <opencv2/core/core.hpp>
#include <opencv2/features2d/features2d.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/calib3d/calib3d.hpp>

using namespace std;
using namespace cv;

void feature_matching(const Mat& img1, const Mat& img2,
    std::vector<KeyPoint>& keypoints1,
    std::vector<KeyPoint>& keypoints2,
    std::vector< DMatch >& matches)
    {
        Mat descriptors_1, descriptors_2;
        Ptr<FeatureDetector> detector = ORB::create();
        Ptr<DescriptorExtractor> descriptor = ORB::create();
        
        // compute FAST corners 
        detector->detect(img1, keypoints1);
        detector->detect(img2, keypoints2);

        // compute BRIEF descriptor
        descriptor->compute(img1, keypoints1, descriptors_1);
        descriptor->compute(img2, keypoints2, descriptors_2);

        Ptr<DescriptorMatcher> matcher = DescriptorMatcher::create(4);
        /*
        FLANNBASED = 1, 
        BRUTEFORCE = 2, 
        BRUTEFORCE_L1 = 3, 
        BRUTEFORCE_HAMMING = 4, 
        BRUTEFORCE_HAMMINGLUT = 5, 
        BRUTEFORCE_SL2 = 6 */

        vector<DMatch> allmatches;
        matcher->match(descriptors_1, descriptors_2, allmatches);

        // Filter all match points
        // Find maximum and minimum distance
        double min_dist = 1000000, max_dist = 0;
        for ( int i = 0; i < descriptors_1.rows; i++ )
        {
            double dist = allmatches[i].distance;
            if ( dist < min_dist ) min_dist = dist;
            if ( dist > max_dist ) max_dist = dist;
        }

        cout << "max distance: " << max_dist << endl;
        cout << "min distance: " << min_dist << endl;

        for ( int i = 0; i < descriptors_1.rows; i++ )
        {
            if ( allmatches[i].distance <= max ( 2*min_dist, 30.0 ) )
            {
                matches.push_back ( allmatches[i] );
            }
        }
    }

void pose_estimation_2d2d(
    std::vector<KeyPoint>& keypoints1,
    std::vector<KeyPoint>& keypoints2,
    std::vector< DMatch >& matches,
    Mat &R, Mat&t){

        Mat K = ( Mat_<double> ( 3,3 ) << 520.9, 0, 325.1, 0, 521.0, 249.7, 0, 0, 1 );

        vector<Point2f> points1;
        vector<Point2f> points2;

        for ( int i = 0; i < ( int ) matches.size(); i++ )
        {
            points1.push_back ( keypoints1[matches[i].queryIdx].pt );
            points2.push_back ( keypoints2[matches[i].trainIdx].pt );
        }

        Mat fundamental_matrix;
        fundamental_matrix = findFundamentalMat ( points1, points2, CV_FM_8POINT );
        cout<<"fundamental_matrix is "<<endl<< fundamental_matrix<<endl;

        Point2d principal_point ( 325.1, 249.7 );	
        double focal_length = 521;		
        Mat essential_matrix;
        essential_matrix = findEssentialMat ( points1, points2, focal_length, principal_point );
        cout<<"essential_matrix is "<<endl<< essential_matrix<<endl;

        Mat homography_matrix;
        homography_matrix = findHomography ( points1, points2, RANSAC, 3 );
        cout<<"homography_matrix is "<<endl<<homography_matrix<<endl;

        recoverPose ( essential_matrix, points1, points2, R, t, focal_length, principal_point );
        cout<<"R is "<<endl<<R<<endl;
        cout<<"t is "<<endl<<t<<endl;
    }
    
Point2f pixel2cam ( const Point2d& p, const Mat& K )
{
    return Point2f
    (
        ( p.x - K.at<double>(0,2) ) / K.at<double>(0,0), 
        ( p.y - K.at<double>(1,2) ) / K.at<double>(1,1) 
    );
}

void triangulation (
    const vector<KeyPoint>& keypoint1,
    const vector<KeyPoint>& keypoint2,
    const std::vector< DMatch >& matches,
    const Mat& R, const Mat& t,
    vector<Point3d>& points){
        Mat T1 = (Mat_<float> (3,4) <<
        1,0,0,0,
        0,1,0,0,
        0,0,1,0);
        Mat T2 = (Mat_<float> (3,4) <<
            R.at<double>(0,0), R.at<double>(0,1), R.at<double>(0,2), t.at<double>(0,0),
            R.at<double>(1,0), R.at<double>(1,1), R.at<double>(1,2), t.at<double>(1,0),
            R.at<double>(2,0), R.at<double>(2,1), R.at<double>(2,2), t.at<double>(2,0)
        );
        
        Mat K = ( Mat_<double> ( 3,3 ) << 520.9, 0, 325.1, 0, 521.0, 249.7, 0, 0, 1 );
        vector<Point2f> pts_1, pts_2;

        for ( DMatch mm:matches )
        {
            Point2f pt1 = Point2d
            (
                ( keypoint1[ mm.queryIdx ].pt.x - K.at<double> ( 0,2 ) ) / K.at<double> ( 0,0 ),
                ( keypoint1[ mm.queryIdx ].pt.y - K.at<double> ( 1,2 ) ) / K.at<double> ( 1,1 )
            );
            pts_1.push_back(pt1);

            Point2f pt2 = Point2d
            (
                ( keypoint2[ mm.trainIdx ].pt.x - K.at<double> ( 0,2 ) ) / K.at<double> ( 0,0 ),
                ( keypoint2[ mm.trainIdx ].pt.y - K.at<double> ( 1,2 ) ) / K.at<double> ( 1,1 )
            );
            pts_2.push_back (pt2);
        }

        Mat pts_4d;
        cv::triangulatePoints( T1, T2, pts_1, pts_2, pts_4d );
        
        // Transform to real world 3D coordinates
        for ( int i=0; i<pts_4d.cols; i++ )
        {
            Mat x = pts_4d.col(i);
            x /= x.at<float>(3,0); 
            Point3d p (
                x.at<float>(0,0), 
                x.at<float>(1,0), 
                x.at<float>(2,0) 
            );
            points.push_back( p );
        }
    }

int main(int argc, char** argv){

    Mat img1 = imread(argv[1], CV_LOAD_IMAGE_COLOR);
    Mat img2 = imread(argv[2], CV_LOAD_IMAGE_COLOR);

    vector<KeyPoint> keypoints1, keypoints2;
    vector<DMatch> matches;
    feature_matching(img1, img2, keypoints1, keypoints2, matches);

    cout<< "Total " << matches.size() << "matched feature points" << endl;

    // pose estimation for two images
    Mat R, t;  // Rotation and translation
    pose_estimation_2d2d(keypoints1, keypoints2, matches, R, t);
    
    // epipolar geometry
    Mat K = ( Mat_<double> (3, 3) << 529.0, 0, 325.1, 0, 521.0, 249.1, 0, 0, 1);

    vector<Point3d> points;
    triangulation( keypoints1, keypoints2, matches, R, t, points );

    cout << points[0] << endl;

    for ( int i=0; i<matches.size(); i++ )
    {
        Point2d pt1_cam = Point2d
            (
                ( keypoints1[ matches[i].queryIdx ].pt.x - K.at<double> ( 0,2 ) ) / K.at<double> ( 0,0 ),
                ( keypoints1[ matches[i].queryIdx ].pt.y - K.at<double> ( 1,2 ) ) / K.at<double> ( 1,1 )
            );
        Point2d pt1_cam_3d(
            points[i].x/points[i].z, 
            points[i].y/points[i].z 
        );
        
        cout<<"point in the first camera frame: "<<pt1_cam<<endl;
        cout<<"point projected from 3D "<<pt1_cam_3d<<", d="<<points[i].z<<endl;
        
        // Second image as reference 
        Point2d pt2_cam = Point2d
            (
                ( keypoints2[ matches[i].trainIdx ].pt.x - K.at<double> ( 0,2 ) ) / K.at<double> ( 0,0 ),
                ( keypoints2[ matches[i].trainIdx ].pt.y - K.at<double> ( 1,2 ) ) / K.at<double> ( 1,1 )
            );
        Mat pt2_trans = R*( Mat_<double>(3,1) << points[i].x, points[i].y, points[i].z ) + t;
        pt2_trans /= pt2_trans.at<double>(2,0);
        cout<<"point in the second camera frame: "<<pt2_cam<<endl;
        cout<<"point reprojected from second frame: "<<pt2_trans.t()<<endl;
        cout<<endl;
    }

    return 0;
}