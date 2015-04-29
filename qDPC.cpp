#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/video/background_segm.hpp>
#include <opencv2/contrib/contrib.hpp>
#include <iostream>
#include <string>
#include <stdio.h>
#include <dirent.h>
#include <complex.h>
#include <ctime>
#include <unordered_map>


#include "omp.h"


#define FILENAME_LENGTH 32
#define FILE_HOLENUM_DIGITS 3
using namespace std;
using namespace cv;

#include "include/domeCoordinates.h"

float zMin, zMax, zStep;
string datasetRoot;

Mat fgMaskMOG; //fg mask generated by MOG method
Mat fgMaskMOG2; //fg mask fg mask generated by MOG2 method
Ptr<BackgroundSubtractor> pMOG; //MOG Background subtractor
Ptr<BackgroundSubtractor> pMOG2; //MOG2 Background subtractor

class R_image{
  
  public:
        cv::Mat Image;
        int led_num;
        float tan_x;
        float tan_y;
};

void circularShift(Mat img, Mat result, int x, int y){
    int w = img.cols;
    int h  = img.rows;

    int shiftR = x % w;
    int shiftD = y % h;
    
    if (shiftR < 0)
        shiftR += w;
    
    if (shiftD < 0)
        shiftD += h;

    cv::Rect gate1(0, 0, w-shiftR, h-shiftD);
    cv::Rect out1(shiftR, shiftD, w-shiftR, h-shiftD);
    
	 cv::Rect gate2(w-shiftR, 0, shiftR, h-shiftD); //rect(x, y, width, height)
	 cv::Rect out2(0, shiftD, shiftR, h-shiftD);
    
	 cv::Rect gate3(0, h-shiftD, w-shiftR, shiftD);
	 cv::Rect out3(shiftR, 0, w-shiftR, shiftD);
    
	 cv::Rect gate4(w-shiftR, h-shiftD, shiftR, shiftD);
	 cv::Rect out4(0, 0, shiftR, shiftD);
   
    cv::Mat shift1 = img ( gate1 );
    cv::Mat shift2 = img ( gate2 );
    cv::Mat shift3 = img ( gate3 );
    cv::Mat shift4 = img ( gate4 );
   
//   if(shiftD != 0 && shiftR != 0)

	   shift1.copyTo(cv::Mat(result, out1));
	if(shiftR != 0)
    	shift2.copyTo(cv::Mat(result, out2));
	if(shiftD != 0)
    	shift3.copyTo(cv::Mat(result, out3));
	if(shiftD != 0 && shiftR != 0)
    	shift4.copyTo(cv::Mat(result, out4));

    //result.convertTo(result,img.type());
}


/*
int loadImages(string datasetRoot, vector<R_image> *images) {
	DIR *dir;
	struct dirent *ent;
	if ((dir = opendir (datasetRoot.c_str())) != NULL) {
	  
      int num_images = 0;
      cout << "Loading Images..." << endl;
	  while ((ent = readdir (dir)) != NULL) {
		//add to list
		string fileName = ent->d_name;
		string filePrefix = "_scanning_";
		if (fileName.compare(".") != 0 && fileName.compare("..") != 0)
		{
		   string holeNum = fileName.substr(fileName.find(filePrefix)+filePrefix.length(),FILE_HOLENUM_DIGITS);
		   //cout << "Filename is: " << fileName << endl;
		 //  cout << "Filename is: " << fileName << ". HoleNumber is: " << holeNum << endl;
		R_image currentImage;
		currentImage.led_num = atoi(holeNum.c_str());
		//currentImage.Image = imread(datasetRoot + "/" + fileName, CV_8UC1);
		currentImage.Image = imread(datasetRoot + "/" + fileName, -1);//apparently - loads with a?

		currentImage.tan_x = -domeCoordinates[currentImage.led_num][0] / domeCoordinates[currentImage.led_num][2];
		currentImage.tan_y = domeCoordinates[currentImage.led_num][1] / domeCoordinates[currentImage.led_num][2];
		(*images).push_back(currentImage);
		num_images ++;
		}
	  }
	  closedir (dir);
	  return num_images;

	} else {
	  //could not open directory
	  perror ("");
	  return EXIT_FAILURE;
	}
}
*/

void computeFocusDPC(vector<R_image> iStack, int fileCount, float z, int width, int height, int xcrop, int ycrop, Mat* results) {
    int newWidth = width;// - 2*xcrop;
    int newHeight = height;// - 2*ycrop;

    cv::Mat bf_result = cv::Mat(newHeight, newWidth, CV_16UC3, double(0));
	 cv::Mat dpc_result_tb = cv::Mat(newHeight, newWidth, CV_16SC1,double(0));
 	 cv::Mat dpc_result_lr = cv::Mat(newHeight, newWidth, CV_16SC1,double(0));
 	 
    cv::Mat bf_result8 = cv::Mat(newHeight, newWidth, CV_8UC3);
    cv::Mat dpc_result_tb8 = cv::Mat(newHeight, newWidth, CV_8UC1);
    cv::Mat dpc_result_lr8 = cv::Mat(newHeight, newWidth, CV_8UC1);
    
    cv::Mat img;
	 cv::Mat img16;
    cv::Mat shifted = cv::Mat(iStack[0].Image.rows, iStack[0].Image.cols, CV_16UC3,double(0));
    vector<Mat> channels(3);
    for (int idx = 0; idx < fileCount; idx++)
        {
         // Load image, convert to 16 bit grayscale image
         img = iStack[idx].Image;

         // Get home number
         int holeNum = iStack[idx].led_num;

         // Calculate shift based on array coordinates and desired z-distance
         int xShift = (int) round(z*iStack[idx].tan_x);
         int yShift = (int) round(z*iStack[idx].tan_y);

         // Shift the Image in x and y
			circularShift(img, shifted, yShift, xShift);
			
			// Add Brightfield image
			cv::add(bf_result, shifted, bf_result);
			
			// Convert shifted to b/w for DPC
			split(shifted, channels);
			channels[1].convertTo(channels[1],dpc_result_lr.type());
			
			if (leftMap[holeNum])
             cv::add(dpc_result_lr, channels[1], dpc_result_lr);
         else
             cv::subtract(dpc_result_lr, channels[1], dpc_result_lr);

         if (topMap[holeNum])
             cv::add(dpc_result_tb, channels[1], dpc_result_tb);
         else
             cv::subtract(dpc_result_tb, channels[1], dpc_result_tb);

         //float progress = 100*((idx+1) / (float)fileCount);
         //cout << progress << endl;
        }
        
        // Scale the values to 8-bit images
        double min_1, max_1, min_2, max_2, min_3, max_3;
        
        cv::minMaxLoc(bf_result, &min_1, &max_1);
	     bf_result.convertTo(bf_result8, CV_8UC4, 255/(max_1 - min_1), - min_1 * 255.0/(max_1 - min_1));
       
        cv::minMaxLoc(dpc_result_lr.reshape(1), &min_2, &max_2);
        dpc_result_lr.convertTo(dpc_result_lr8, CV_8UC4, 255/(max_2 - min_2), -min_2 * 255.0/(max_2 - min_2));
        
        cv::minMaxLoc(dpc_result_tb.reshape(1), &min_3, &max_3);
        dpc_result_tb.convertTo(dpc_result_tb8, CV_8UC4, 255/(max_3 - min_3), -min_3 * 255.0/(max_3 - min_3));
        
        results[0] = bf_result8;
        results[1] = dpc_result_lr8;
        results[2] = dpc_result_tb8;
}

cv::Mat calcDPC( cv::Mat image1, cv::Mat image2)
{

  /*
  // Background Subtraction
  pMOG = new BackgroundSubtractorMOG(10,2,2,2);
  pMOG->operator()(image1, fgMaskMOG);
  
  namedWindow("Img1", WINDOW_NORMAL);
  imshow("Img1", image1);
  namedWindow("Img1_bg", WINDOW_NORMAL);
  imshow("Img1_bg", fgMaskMOG);

	waitKey(0);
  */
  std::cout << image1.type() << endl;
  std::cout << image2.type() << endl;
  cv::cvtColor(image1,image1,COLOR_BGR2GRAY);
  cv::cvtColor(image2,image2,COLOR_BGR2GRAY);
  
  image1.convertTo(image1,CV_32FC1);
  image2.convertTo(image2,CV_32FC1);
  
  cv::Mat tmp1, tmp2;
  cv::subtract(image1, image2, tmp1);
  cv::add(image1, image2, tmp2);
  
  cv::Mat output;
  cv::divide(tmp1,tmp2,output);
  
  tmp1.release();
  tmp2.release();
  
  // Crop the ROI to a circle to get rid of noise
  int16_t circPad = 100;
  cv::Mat circMask(output.size(),CV_32FC1,cv::Scalar(0));
  cv::Point center(cvRound(output.rows/2),cvRound(output.cols/2));
  cv::circle(circMask, center, cvRound(output.rows/2)-circPad ,cv::Scalar(1.0), -1,8,0);
  cv::multiply(output,circMask,output);
	
  double min_1, max_1;
  cv::minMaxLoc(output, &min_1, &max_1);
  output.convertTo(output, CV_8UC1, 255/(max_1 - min_1), - min_1 * 255.0/(max_1 - min_1));

  return output;
}

cv::Mat qDPC_loop(vector<cv::Mat>dpcList, vector<cv::Mat>transferFunctionList, double reg)
{
   clock_t begin = clock();
   vector<Mat> complexPlanes;
   
   Mat h[2];
   Mat planes[2];
   
   // Initialize Resilts
   Mat ph_dpc_ft_real = Mat::zeros(dpcList.at(0).size(), CV_64F);
   Mat ph_dpc_ft_imag = Mat::zeros(dpcList.at(0).size(), CV_64F);
   
   Mat dpcComplex, tmp;
   
   vector<cv::Mat> complexTransferFunctionList(dpcList.size());
   vector<cv::Mat> dpcFtList(dpcList.size());
   vector<cv::Mat> dpcFtRealList(dpcList.size());
   vector<cv::Mat> dpcFtImagList(dpcList.size());
   
   /*
   namedWindow("DPC1", WINDOW_NORMAL);
   imshow("DPC1", dpcList.at(0));
   
   namedWindow("DPC2", WINDOW_NORMAL);
   imshow("DPC2", dpcList.at(1));
   */
   // Pad all values
   
   Mat padded;                            //expand input image to optimal size
   int m = getOptimalDFTSize( dpcList.at(0).rows );
   int n = getOptimalDFTSize( dpcList.at(0).cols ); // on the border add zero values

   for (int idx = 0; idx<transferFunctionList.size(); idx++)
   {

      copyMakeBorder(transferFunctionList.at(idx), padded, 0, m - transferFunctionList.at(idx).rows, 0, n - transferFunctionList.at(idx).cols, BORDER_CONSTANT, Scalar::all(0));
      copyMakeBorder(dpcList.at(idx), padded, 0, m - dpcList.at(idx).rows, 0, n - dpcList.at(idx).cols, BORDER_CONSTANT, Scalar::all(0));
      
      // Take Fourier Transforms of DPC Images
      planes[0] = Mat_<double>(dpcList.at(idx));
      planes[1] = Mat::zeros(dpcList.at(idx).size(), CV_64F);
      
      merge(planes, 2, dpcComplex);
      dft(dpcComplex, dpcComplex);
      split(dpcComplex, complexPlanes);
      dpcFtRealList.at(idx) = complexPlanes[0];
      dpcFtImagList.at(idx) = complexPlanes[1];
   }
   clock_t c1 = clock();
   double elapsed_secs = double(c1 - begin) / CLOCKS_PER_SEC;
   cout << elapsed_secs << endl;

   //complex<double> H_sum(0,0);
   //complex<double> Hi_sum(0,0);
   //complex<double> I_sum(0,0);
   //complex<double> result;
   
   // Try all of this inside of a loop to properly deal with complex values
   #pragma omp parallel for
	for (int i=0; i < dpcComplex.cols; i++){
	   for (int j=0; j < dpcComplex.rows; j++){

	     	complex<double> H_sum = std::complex<double>(0,0);
	        complex<double> Hi_sum = std::complex<double>(0,0);
	        complex<double> I_sum = std::complex<double>(0,0);
	        complex<double> result;
	        
	        H_sum += std::complex<double>(0,transferFunctionList.at(0).at<double>(i,j));
	        Hi_sum += std::complex<double>(0,-1*transferFunctionList.at(0).at<double>(j,i));
	        I_sum += std::complex<double>(dpcFtRealList.at(0).at<double>(i,j),dpcFtImagList.at(0).at<double>(i,j));
	        
	        H_sum += std::complex<double>(0,transferFunctionList.at(1).at<double>(i,j));
	        Hi_sum += std::complex<double>(0,-1*transferFunctionList.at(1).at<double>(j,i));
	        I_sum += std::complex<double>(dpcFtRealList.at(1).at<double>(i,j),dpcFtImagList.at(1).at<double>(i,j));
	      
	      //}

	      result = ((I_sum*Hi_sum)/(abs(H_sum)*abs(H_sum) + reg));
	      
	      #pragma omp critical
	      {
	        ph_dpc_ft_real.at<double>(i,j) = result.real();
	        ph_dpc_ft_imag.at<double>(i,j) = result.imag();
	      }
	      

	   }
	}
	double c2 = clock();
	cout << double(c2 - c1) / CLOCKS_PER_SEC<<endl;;
	
	Mat ph_complex_ft, ph_complex;
	planes[0] = ph_dpc_ft_real;
	planes[1] = ph_dpc_ft_imag;
	
	/*
	namedWindow("ResultFT", WINDOW_NORMAL);
   imshow("ResultFT", planes[0]);
   */
   
   merge(planes, 2, ph_complex_ft);    
	dft(ph_complex_ft, ph_complex, DFT_INVERSE);
	split(ph_complex, complexPlanes); 
	
	return complexPlanes[0]; // Real Part
}


int main(int argc, char** argv )
{
    double startTime = omp_get_wtime();
   /*
   Inputs:
   RefocusMin
   RefocusStep
   RefocusMax
   DatasetRoot
   */
     
   char * dpc_fName_11;
   char * dpc_fName_12;
   char * dpc_fName_21;
   char * dpc_fName_22;
   
   char * trans_fName_1;
   char * trans_fName_2;
   

   /*
   if (argc < 1)
   {
      cout << "Error: selectNot enough inputs.\nUSAGE: ./Refocusing zMin zStep zMax DatasetRoot" << endl;
      return 0;
   }else{

   }
   */
   dpc_fName_11 = argv[1];
   dpc_fName_12 = argv[2];
   dpc_fName_21 = argv[3];
   dpc_fName_22 = argv[4];
   trans_fName_1 = argv[5];
   trans_fName_2 = argv[6];
   
   std::cout << "dpc 11: " << dpc_fName_11 << std::endl;
   std::cout << "dpc 12: " << dpc_fName_12 << std::endl;
   std::cout << "dpc 21: " << dpc_fName_21 << std::endl;
   std::cout << "dpc 22: " << dpc_fName_22 << std::endl;
   
   std::cout << "transfer function 1: " << trans_fName_1 << std::endl;
   std::cout << "transfer function 2: " << trans_fName_2 << std::endl;
   
   cv::Mat dpc11 = imread(dpc_fName_11, -1);
   cv::Mat dpc12 = imread(dpc_fName_12, -1);
   cv::Mat dpc21 = imread(dpc_fName_21, -1);
   cv::Mat dpc22 = imread(dpc_fName_22, -1);
   
   cv::Mat trans1 = imread(trans_fName_1, -1);
   cv::Mat trans2 = imread(trans_fName_2, -1);
   
   trans1.convertTo(trans1,CV_64FC1);
   trans2.convertTo(trans2,CV_64FC1);
   
   normalize(trans1, trans1, -1, 1, CV_MINMAX);
	normalize(trans2, trans2, -1, 1, CV_MINMAX);
	
   vector<cv::Mat> transferFunctionList;
	transferFunctionList.push_back(trans1);
	transferFunctionList.push_back(trans2);
   
   // Crop to square
   dpc11 = dpc11 (CellScopeCropHorz);
   dpc12 = dpc12 (CellScopeCropHorz);
   dpc21 = dpc21 (CellScopeCropHorz);
   dpc22 = dpc22 (CellScopeCropHorz);
   
   //Compute DPC Images
   cv::Mat dpc1 = calcDPC(dpc11,dpc12);
   cv::Mat dpc2 = calcDPC(dpc21,dpc22);
   
   vector<cv::Mat> dpcList;
   dpcList.push_back(dpc1);
   dpcList.push_back(dpc2);

	double reg = 0.8;
	Mat ph_dpc = qDPC_loop(dpcList,transferFunctionList,reg);
   normalize(ph_dpc, ph_dpc, -0.3, 1.4, CV_MINMAX);
   
   Mat cm_ph_dpc;
   cv::applyColorMap(ph_dpc, cm_ph_dpc, COLORMAP_COOL);
   namedWindow("Phase Image", WINDOW_NORMAL);

   imshow("Phase Image", ph_dpc);
   cout << "execution took " << omp_get_wtime() - startTime << " seconds" << endl;

   waitKey(0);
} 

