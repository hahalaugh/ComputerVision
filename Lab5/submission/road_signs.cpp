#ifdef _CH_
#	pragma package <opencv>
#endif

#include "cv.h"
#include "highgui.h"
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>

using namespace std;

#include "../utilities.h"

#define NUM_IMAGES		5
#define RED_THRESHOLD	50

const int		g_TemplateImageNums = 10;						// number of template images
IplImage*		g_TemplateImages[g_TemplateImageNums] = {NULL};	// pointer of template images
vector<CvRect>	g_ContourRects;									// the bounding rectangles of the original signs
const int		g_ContourSizeThreshold = 10;					// min size of the contour, use this value as a filter 
int				g_MaxTemplateDimension = 0;						// max dimension of the templates

IplImage* CalculateBlackRedImage(IplImage* sourceImage,vector<CvRect>&);

// get sub image by a specific rectangle
IplImage* GetSubImage(IplImage *image, CvRect roi)
{
	IplImage *result;
	cvSetImageROI(image,roi);	// use the rect as roi parameter
	result = cvCreateImage( cvSize(roi.width, roi.height), image->depth, image->nChannels );
	// copy the sub image
	cvCopy(image,result);
	cvResetImageROI(image);	// reset the roi of the image
	return result;
}

void LoadTemplateImages()
{
	// the file names
	// using std::vector and std::string
	vector<string>	templates;	
	templates.push_back("GoLeft.JPG");
	templates.push_back("GoRight.JPG");
	templates.push_back("GoStraight.JPG");
	templates.push_back("NoLeftTurn.JPG");
	templates.push_back("NoParking.JPG");
	templates.push_back("NoRightTurn.JPG");
	templates.push_back("NoRightTurn2.JPG");
	templates.push_back("NoStraight.JPG");
	templates.push_back("Parking.JPG");
	templates.push_back("Yield.JPG");

	g_MaxTemplateDimension = 0;

	// iterate each image and load it
	size_t i = 0;
	for (auto it = templates.begin(); it != templates.end(); ++it,++i)
	{
		// load the image and save the result to the array
		IplImage* originalTemplate = cvLoadImage(it->c_str(),-1);

		// always check whether the file exists
		assert(originalTemplate && "Cannot locate the image");

		vector<CvRect> boundingBoxes;

		// generate the black-red-white image of the each template
		IplImage* brwImage = CalculateBlackRedImage(originalTemplate,boundingBoxes);
		assert(boundingBoxes.size() == 1);

		g_TemplateImages[i] = GetSubImage(brwImage,boundingBoxes[0]);

		// calculate the max dimension of the template
		// i will use this value later
		if (boundingBoxes[0].width > g_MaxTemplateDimension)
		{
			g_MaxTemplateDimension = boundingBoxes[0].width;
		}
		if (boundingBoxes[0].height > g_MaxTemplateDimension)
		{
			g_MaxTemplateDimension = boundingBoxes[0].height;
		}

		// avoid memory leak
		cvReleaseImage(&originalTemplate);
		cvReleaseImage(&brwImage);
	}
}

// Locate the red pixels in the source image.
void find_red_points( IplImage* source, IplImage* result, IplImage* temp )
{
	//	Write code to select all the red road sign points.  You may need to clean up the result
	//  using mathematical morphology.  The result should be a binary image with the selected red
	//  points as white points.  The temp image passed may be used in your processing.

	int widthStep = source->widthStep;
	int pixelStep = source->widthStep/source->width;
	int numberChannels = source->nChannels;
	cvZero( result );
	unsigned char white_pixel[4] = {255,255,255,0};
	
	// Find all red points in the image
	for (int i=0; i < result->height; i++)
	{
		for (int j=0; j < result->width; j++)
		{
			unsigned char* currentPixel = GETPIXELPTRMACRO( source, j, i, widthStep, pixelStep );

			// first step is the same as previous red spoon finding function
			if ( (currentPixel[RED_CH]  >= RED_THRESHOLD) &&
				((currentPixel[RED_CH]) > currentPixel[BLUE_CH]) &&
				((currentPixel[RED_CH]) > currentPixel[GREEN_CH]) )
			{
				// because some scene is so dark, the value of three channels are too close
				// we have to shrink red pixel to compare with other two channels
				float shrinkedRed = currentPixel[RED_CH]*0.8f;

				if (((currentPixel[BLUE_CH] < shrinkedRed) || 
					(currentPixel[GREEN_CH] < shrinkedRed)))
				{
					PUTPIXELMACRO( result, j, i, white_pixel, widthStep, pixelStep, numberChannels );
				}
			}
		}
	}

	// apply morphological opening and closing operations to clean up the image
	cvMorphologyEx( result, temp, NULL, NULL, CV_MOP_OPEN, 1 );
	cvMorphologyEx( temp, result, NULL, NULL, CV_MOP_CLOSE, 1 );

}

// find the bounding rectangle of each contour in the original image
CvRect FindContourRect(CvSeq* contour)
{
	// result rectangle
	CvRect rect;
	rect.x		= 0;
	rect.y		= 0;
	rect.width	= 0;
	rect.height = 0;

	CvPoint pt0;
	pt0.x = 100000;	// the maximum value
	pt0.y = 100000;

	CvPoint pt1;
	pt1.x = 0;		// the minimum value
	pt1.y = 0;

	// always check the null pointer
	if (!contour)
	{
		return rect;
	}

	// iterate the contour
	CvSeq *seq = contour;
	{
		int num = seq->total;
		int i;

		for(i = 0; i < num; ++i)
		{
			CvPoint *p = (CvPoint*)cvGetSeqElem(seq, i);
			CvPoint pt = *p;

			// find the left-top point
			if (pt.x < pt0.x)
			{
				pt0.x = pt.x;
			}
			if (pt.y < pt0.y)
			{
				pt0.y = pt.y;
			}

			// find the right-bottom point
			if (pt.x > pt1.x)
			{
				pt1.x = pt.x;
			}
			if (pt.y > pt1.y)
			{
				pt1.y = pt.y;
			}
		}
	}

	// return the result
	rect.x		= pt0.x;
	rect.y		= pt0.y;
	rect.width	= pt1.x - pt0.x;
	rect.height = pt1.y - pt0.y;
	return rect;
}

void Recognition(IplImage* source, IplImage* origin)
{
	size_t i = 0;
	for (auto it = g_ContourRects.begin(); it != g_ContourRects.end(); ++it,++i)
	{
		IplImage* image = GetSubImage(source,*it); 

		IplImage* newImage = cvCreateImage(cvSize(g_MaxTemplateDimension,g_MaxTemplateDimension),
			image->depth,image->nChannels);
		cvResize(image,newImage);

		IplImage* bestMatch = NULL;
		float matchValue = -1000;
		for (size_t itemp = 0; itemp < g_TemplateImageNums; itemp++)
		{
			IplImage* tem = g_TemplateImages[itemp];
			int w = newImage->width  - tem->width;
			int h = newImage->height - tem->height;

			// match template
			IplImage* res = cvCreateImage(cvSize(w+1,h+1),IPL_DEPTH_32F,1);
			cvMatchTemplate(newImage,tem,res,CV_TM_CCOEFF_NORMED);

			CvPoint		minloc, maxloc;
			double		minval, maxval;
			cvMinMaxLoc( res, &minval, &maxval, &minloc, &maxloc, 0 );

			if (maxval > matchValue)
			{
				matchValue = maxval;
				bestMatch = tem;
			}
		}

		// 0.5f is a threshold
		if (bestMatch && matchValue > 0.45f)
		{
			// create a temp image
			IplImage* temp = cvCreateImage(cvSize(bestMatch->width,bestMatch->height),origin->depth,origin->nChannels);
			cvConvertImage(bestMatch,temp);
			IplImage* resizedTem = cvCreateImage(cvSize(it->width/2,it->width/2),
				origin->depth,origin-> nChannels);
			cvResize(temp,resizedTem);

			// copy the template image to the original image
			cvSetImageROI(origin, cvRect(it->x,it->y, resizedTem->width, resizedTem->height));  // the rectangle
			cvCopy(resizedTem, origin); //core function
			cvResetImageROI(origin); 

			cvReleaseImage(&temp);
			cvReleaseImage(&resizedTem);
		}
	}
}

CvSeq* connected_components( IplImage* source, IplImage* result, bool calculateRect,vector<CvRect>& rects)
{
	if (calculateRect)
	{
		rects.clear();
	}

	IplImage* binary_image = cvCreateImage( cvGetSize(source), 8, 1 );
	cvConvertImage( source, binary_image );
	CvMemStorage* storage = cvCreateMemStorage(0);
	CvSeq* contours = 0;
	cvThreshold( binary_image, binary_image, 1, 255, CV_THRESH_BINARY );

	cvFindContours( binary_image, storage, &contours, sizeof(CvContour),CV_RETR_CCOMP, CV_CHAIN_APPROX_SIMPLE );
	if (result)
	{
		cvZero( result );
		for(CvSeq* contour = contours ; contour != 0; contour = contour->h_next )
		{
			if (calculateRect)
			{
				CvRect rect = FindContourRect(contour);
				if (rect.width > g_ContourSizeThreshold &&
					rect.height > g_ContourSizeThreshold)
				{
					rects.push_back(rect);
				}
			}

			CvScalar color = CV_RGB( rand()&255, rand()&255, rand()&255 );
			/* replace CV_FILLED with 1 to see the outlines */
			cvDrawContours( result, contour, color, color, -1, CV_FILLED, 8 );
		}
	}

	return contours;
}

void invert_image( IplImage* source, IplImage* result )
{
	int width_step=source->widthStep;
	int pixel_step=source->widthStep/source->width;
	int number_channels=source->nChannels;
	cvZero( result );
	int row=0,col=0;
	// Find all red points in the image
	for (row=0; row < result->height; row++)
	{
		for (col=0; col < result->width; col++)
		{
			unsigned char* curr_point = GETPIXELPTRMACRO( source, col, row, width_step, pixel_step );
			unsigned char pixel[4] = {255-curr_point[RED_CH],255-curr_point[BLUE_CH],255-curr_point[GREEN_CH],0};
			PUTPIXELMACRO( result, col, row, pixel, width_step, pixel_step, number_channels );
		}
	}
}

// Assumes a 1D histogram of 256 elements.
int determine_optimal_threshold( CvHistogram* hist )
{	
	// Given a 1-D CvHistogram you need to determine and return the optimal threshold value.

	int currentThreshold = 127;// initial threshold value
	int threshold_final = currentThreshold;

	float backgroundCount = 1;	// to avoid dividing by zero
	float objectCount = 1;

	float backgroundIntensity = 0;
	float objectIntensity = 0;

	// optimal thresholding algorithm
	// see the notes of CS7008
	do
	{
		currentThreshold = threshold_final;

		// iterate the histogram
		for (int i=0; i<256; i++)
		{
			int histogramValue = ((int) *cvGetHistValue_1D(hist, i));
			
			if (histogramValue < currentThreshold)
			{
				backgroundIntensity += (float)(histogramValue * i);
				backgroundCount += (float)histogramValue;
			}
			else
			{
				objectIntensity += (float)(histogramValue * i);
				objectCount += (float)histogramValue;
			}
		}

		// compute the average of background and object
		float averageBackground = (float)backgroundIntensity / backgroundCount;
		float averageObject = (float)objectIntensity / objectCount;

		// clear the counter
		backgroundCount		= 1;
		objectCount			= 1;
		backgroundIntensity = 0;
		objectIntensity		= 0;

		//convert the float value to integer and than compare it with initial threshold
		threshold_final = static_cast<int>((averageBackground + averageObject)/2);

	}while(threshold_final == currentThreshold); // the algorithm finish when two values are equal

	return threshold_final;
}

void apply_threshold_with_mask(IplImage* grayscale_image,IplImage* result_image,IplImage* mask_image,int threshold)
{
	int grayWidthStep	= grayscale_image->widthStep;
	int grayPixelStep	= grayscale_image->widthStep / grayscale_image->width;

	int resultWidthStep = result_image->widthStep;
	int resultPixelStep = result_image->widthStep / result_image->width;
	int resultChannels	= result_image->nChannels;

	unsigned char white[4] = {255,255,255,0};
	unsigned char black[4] = {0,0,0,0};

	for(int i = 0; i < grayscale_image->height; i++)
	{
		for(int j = 0; j < grayscale_image->width; j++)
		{
			unsigned char * maskPixel = GETPIXELPTRMACRO(mask_image, j, i, grayWidthStep, grayPixelStep);
			
			// only process the pixel when mask point at the same position is 255
			if(maskPixel[0] != 0)
			{
				unsigned char * grey_point = GETPIXELPTRMACRO(grayscale_image, j, i, grayWidthStep, grayPixelStep);
				
				if(grey_point[0] >= threshold)
				{
					PUTPIXELMACRO(result_image, j, i, white, resultWidthStep, resultPixelStep, resultChannels);
				}
				else
				{
					PUTPIXELMACRO(result_image, j, i, black, resultWidthStep, resultPixelStep, resultChannels);                                    
				}
			}
		}
	}
}

void determine_optimal_sign_classification( IplImage* original_image, IplImage* red_point_image, CvSeq* red_components, CvSeq* , IplImage* result_image )
{
	int width_step=original_image->widthStep;
	int pixel_step=original_image->widthStep/original_image->width;
	IplImage* mask_image = cvCreateImage( cvGetSize(original_image), 8, 1 );
	IplImage* grayscale_image = cvCreateImage( cvGetSize(original_image), 8, 1 );
	cvConvertImage( original_image, grayscale_image );
	IplImage* thresholded_image = cvCreateImage( cvGetSize(original_image), 8, 1 );
	cvZero( thresholded_image );
	cvZero( result_image );
	int row=0,col=0;
	CvSeq* curr_red_region = red_components;
	// For every connected red component
	while (curr_red_region != NULL)
	{
		cvZero( mask_image );
		CvScalar color = CV_RGB( 255, 255, 255 );
		CvScalar mask_value = cvScalar( 255 );
		// Determine which background components are contained within the red component (i.e. holes)
		//  and create a binary mask of those background components.
		CvSeq* curr_background_region = curr_red_region->v_next;
		if (curr_background_region != NULL)
		{
			while (curr_background_region != NULL)
			{
				cvDrawContours( mask_image, curr_background_region, mask_value, mask_value, -1, CV_FILLED, 8 );
				cvDrawContours( result_image, curr_background_region, color, color, -1, CV_FILLED, 8 );
				curr_background_region = curr_background_region->h_next;
			}
			int hist_size=256;
			CvHistogram* hist = cvCreateHist( 1, &hist_size, CV_HIST_ARRAY );
			cvCalcHist( &grayscale_image, hist, 0, mask_image );

			// Determine an optimal threshold on the points within those components (using the mask)
			int optimal_threshold = determine_optimal_threshold( hist );
			apply_threshold_with_mask(grayscale_image,result_image,mask_image,optimal_threshold);
		}
		curr_red_region = curr_red_region->h_next;
	}

	for (row=0; row < result_image->height; row++)
	{
		unsigned char* curr_red = GETPIXELPTRMACRO( red_point_image, 0, row, width_step, pixel_step );
		unsigned char* curr_result = GETPIXELPTRMACRO( result_image, 0, row, width_step, pixel_step );
		for (col=0; col < result_image->width; col++)
		{
			curr_red += pixel_step;
			curr_result += pixel_step;
			if (curr_red[0] > 0)
				curr_result[2] = 255;
		}
	}

	cvReleaseImage( &mask_image );
}

IplImage* CalculateBlackRedImage(IplImage* sourceImage, vector<CvRect>& outRects)
{
	IplImage* selected_image = sourceImage;
	IplImage* red_point_image = NULL;
	IplImage* temp_image = NULL;
	IplImage* connected_reds_image = NULL;
	IplImage* connected_background_image = NULL;
	IplImage* blackredwhite_image = NULL;
	CvSeq* red_components = NULL;
	CvSeq* background_components = NULL;

	if (red_point_image != NULL)
	{
		cvReleaseImage( &red_point_image );
		cvReleaseImage( &temp_image );
		cvReleaseImage( &connected_reds_image );
		cvReleaseImage( &connected_background_image );
		cvReleaseImage( &blackredwhite_image );
	}
	red_point_image = cvCloneImage( selected_image );
	blackredwhite_image = cvCloneImage( selected_image );
	temp_image = cvCloneImage( selected_image );
	connected_reds_image = cvCloneImage( selected_image );
	connected_background_image = cvCloneImage( selected_image );

	// Process image
	find_red_points( selected_image, red_point_image, temp_image );
	red_components = connected_components( red_point_image, connected_reds_image, true, outRects);
	invert_image( red_point_image, temp_image );
	background_components = connected_components( temp_image, connected_background_image, false, outRects);
	determine_optimal_sign_classification( selected_image, red_point_image, red_components, 
		background_components, blackredwhite_image );

	return blackredwhite_image;
}

int main( int , char**  )
{
	int selected_image_num = 1;
	char show_ch = 's';
	IplImage* images[NUM_IMAGES];
	IplImage* selected_image = NULL;
	IplImage* orignial_image = NULL;
	IplImage* temp_image = NULL;
	IplImage* red_point_image = NULL;
	IplImage* connected_reds_image = NULL;
	IplImage* connected_background_image = NULL;
	IplImage* blackredwhite_image = NULL;
	IplImage* result_image = NULL;
	CvSeq* red_components = NULL;
	CvSeq* background_components = NULL;

	// Load all the images.
	//for (int file_num=1; (file_num <= NUM_IMAGES); file_num++)
	{
		if( (images[0] = cvLoadImage("./RealRoadSigns.jpg",-1)) == 0 )
			return 0;
		if( (images[1] = cvLoadImage("./RealRoadSigns2.jpg",-1)) == 0 )
			return 0;
		if( (images[2] = cvLoadImage("./ExampleRoadSigns.jpg",-1)) == 0 )
			return 0;
		if( (images[3] = cvLoadImage("./Parking.jpg",-1)) == 0 )
			return 0;
		if( (images[4] = cvLoadImage("./NoParking.jpg",-1)) == 0 )
			return 0;
	}

	// Load all template images
	LoadTemplateImages();

	// Explain the User Interface
    printf( "Hot keys: \n"
            "\tESC - quit the program\n"
			"\t1 - Real Road Signs (image 1)\n"
			"\t2 - Real Road Signs (image 2)\n"
			"\t3 - Synthetic Road Signs\n"
			"\t4 - Synthetic Parking Road Sign\n"
			"\t5 - Synthetic No Parking Road Sign\n"
			"\tr - Show red points\n"
			"\tc - Show connected red points\n"
			"\th - Show connected holes (non-red points)\n"
			"\ts - Show optimal signs\n"
			);
    
	// Create display windows for images
    cvNamedWindow( "Original", 1 );
    cvNamedWindow( "Processed Image", 1 );

	// Setup mouse callback on the original image so that the user can see image values as they move the
	// cursor over the image.
    cvSetMouseCallback( "Original", on_mouse_show_values, 0 );
	window_name_for_on_mouse_show_values="Original";
	image_for_on_mouse_show_values=selected_image;

	int user_clicked_key = 0;
	do 
	{
		// Create images to do the processing in.
		if (red_point_image != NULL)
		{
			cvReleaseImage( &red_point_image );
			cvReleaseImage( &temp_image );
			cvReleaseImage( &connected_reds_image );
			cvReleaseImage( &connected_background_image );
			cvReleaseImage( &blackredwhite_image );
			cvReleaseImage( &orignial_image );
			cvReleaseImage( &result_image );
		}
		selected_image = images[selected_image_num-1];
		red_point_image = cvCloneImage( selected_image );
		blackredwhite_image = cvCloneImage( selected_image );
		temp_image = cvCloneImage( selected_image );
		connected_reds_image = cvCloneImage( selected_image );
		connected_background_image = cvCloneImage( selected_image );
		orignial_image = cvCloneImage( selected_image );
		image_for_on_mouse_show_values = orignial_image;

		// Process image
		find_red_points( selected_image, red_point_image, temp_image );
		red_components = connected_components( red_point_image, connected_reds_image, true, g_ContourRects);
		invert_image( red_point_image, temp_image );
		background_components = connected_components( temp_image, connected_background_image, false, g_ContourRects);
		determine_optimal_sign_classification( selected_image, red_point_image, red_components, 
			background_components, blackredwhite_image );
		
		result_image = cvCloneImage( blackredwhite_image );


		// recognize the road signs
		Recognition(blackredwhite_image,orignial_image);

		// Show the original & result
        cvShowImage( "Original", orignial_image );
		do
		{
			if ((user_clicked_key == 'r') || (user_clicked_key == 'c') ||
				(user_clicked_key == 'h') || (user_clicked_key == 's'))
			{
				show_ch = static_cast<char>(user_clicked_key);
			}
			switch (show_ch)
			{
			case 'c':
				cvShowImage( "Processed Image", connected_reds_image );
				break;
			case 'h':
				cvShowImage( "Processed Image", connected_background_image );
				break;
			case 'r':
				cvShowImage( "Processed Image", red_point_image );
				break;
			case 's':
			default:
				cvShowImage( "Processed Image", blackredwhite_image );
				break;
			}
	        user_clicked_key = cvWaitKey(0);
		} while ((!((user_clicked_key >= '1') && (user_clicked_key <= '0'+NUM_IMAGES))) &&
			     ( user_clicked_key != ESC ));
		if ((user_clicked_key >= '1') && (user_clicked_key <= '0'+NUM_IMAGES))
		{
			selected_image_num = user_clicked_key-'0';
		}
	} while ( user_clicked_key != ESC );

    return 1;
}
