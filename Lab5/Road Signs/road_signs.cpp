#ifdef _CH_
#pragma package <opencv>
#endif


/*
The following parts of this program is created to recognize the road signs.

methods:
double template_original_match(IplImage* original_image,IplImage* template_image){}
sub_image* extract_sub_image(IplImage* original_image, int min_width, int min_height){}
void recognize(IplImage* original_images, IplImage* processed_original_images, IplImage** templates){}
int  load_templates(IplImage** templates){}
void normalize_template(IplImage** templates){}

struct sub_image

some macro definition.

some file loading methods is added to the main() method.


the other codes is from my previous assignment(assignment 1).
*/
#include "cv.h"
#include "highgui.h"
#include <stdio.h>
#include <stdlib.h>
#include "../utilities.h"

#define THRESHOLD_H_RANGE1_LOW 0
#define THRESHOLD_H_RANGE1_HIGH 6
#define THRESHOLD_H_RANGE2_LOW 160
#define THRESHOLD_H_RANGE2_HIGH 180
#define THRESHOLD_S_LOW 40
#define THRESHOLD_V_LOW 55

#define CHAN_H 0
#define CHAN_S 1
#define CHAN_V 2

#define NUM_IMAGES 5

//scale of template
#define MATCHING_WIDTH 75
#define MATCHING_HEIGHT 75

//to exclude those incorrect recognition when extract the sub region images from whole image
#define MIN_SUB_IMAGE_WIDTH 25
#define MIN_SUB_IMAGE_HEIGHT 25

#define TEMPLATES_NUM 10

//if the matching result max is smaller than the threshold
#define MATCHING_FALIED_THRESHOLD 0.521

//struct to store the partial image extract from the whole image
typedef struct sub_image
{
	IplImage* image;
	struct sub_image* next_image;

	//coordinates in the original image
	int image_left;
	int image_top;
	int image_right;
	int image_buttom;
}sub_image;


void find_red_points( IplImage* source, IplImage* result, IplImage* temp )
{
	// TO DO:  Write code to select all the red road sign points.  You may need to clean up the result
	//        using mathematical morphology.  The result should be a binary image with the selected red
	//        points as white points.  The temp image passed may be used in your processing.
	
	 IplImage* temp_hsv= cvCloneImage(source);
	 cvCvtColor(source,temp_hsv,CV_BGR2HSV);

	 int width_step=source->widthStep;
	 int pixel_step=source->widthStep/source->width;
	 int number_channels=source->nChannels;
	 unsigned char white_pixel[4] = {255,255,255,0};
	 cvZero( result );
	 int row=0,col=0;


	 for (row=0; row < result->height; row++)
	 {
		  for (col=0; col < result->width; col++)
		  {
			   unsigned char* curr_point = GETPIXELPTRMACRO( temp_hsv, col, row, width_step, pixel_step );
			   //determine the range of different channels which represente the red part on the sign.
			  if
			  (
			  	(
			  		(
			  			(curr_point[CHAN_H] >= THRESHOLD_H_RANGE1_LOW)
			  			&&
			  			(curr_point[CHAN_H] <= THRESHOLD_H_RANGE1_HIGH)
			  		) 
			  		||
			  		(
			  			(curr_point[CHAN_H] >= THRESHOLD_H_RANGE2_LOW)
			  			&&
			  			(curr_point[CHAN_H] < THRESHOLD_H_RANGE2_HIGH)
			  		)
			  	)
			  	&&
			  	(
			  		(curr_point[CHAN_S] > THRESHOLD_S_LOW)
			  		&&
			  		(curr_point[CHAN_V] > THRESHOLD_V_LOW)
			  	)
			  )
			   {
					PUTPIXELMACRO( result, col, row, white_pixel, width_step, pixel_step, number_channels );
			   }
		  }
	 }

	 //clean the result image
	 cvMorphologyEx( result, result, NULL, NULL, CV_MOP_OPEN, 1);
	 cvMorphologyEx( result, result, NULL, NULL, CV_MOP_CLOSE, 1 );
}

CvSeq* connected_components( IplImage* source, IplImage* result )
{
	IplImage* binary_image = cvCreateImage( cvGetSize(source), 8, 1 );
	cvConvertImage( source, binary_image );
	CvMemStorage* storage = cvCreateMemStorage(0);
	CvSeq* contours = 0;
	cvThreshold( binary_image, binary_image, 1, 255, CV_THRESH_BINARY );
	cvFindContours( binary_image, storage, &contours, sizeof(CvContour),	CV_RETR_CCOMP, CV_CHAIN_APPROX_SIMPLE );
	if (result)
	{
		cvZero( result );
		for(CvSeq* contour = contours ; contour != 0; contour = contour->h_next )
		{
			CvScalar color = CV_RGB( rand()&255, rand()&255, rand()&255 );
			/* replace CV_FILLED with 1 to see the outlines */
			cvDrawContours( result, contour, color, color, -1, CV_FILLED, 8 );
		}
	}
	return contours;
}

void invert_image( IplImage* source, IplImage* result )
{
	// TO DO:  Write code to invert all points in the source image (i.e. for each channel for each pixel in the result
	//        image the value should be 255 less the corresponding value in the source image).

	 int width_step=source->widthStep;
	 int pixel_step=source->widthStep/source->width;
	 int number_channels=source->nChannels;
	 cvZero( result );
	 int row=0,col=0;

	 IplImage* grayscale_image= cvCloneImage(source);
	 cvZero(grayscale_image);
	 cvConvertImage(source,grayscale_image);
	 
	 for (row=0; row < result->height; row++)
	 {
		  for (col=0; col < result->width; col++)
		  {
			   unsigned char* curr_point = GETPIXELPTRMACRO( grayscale_image, col, row, width_step, pixel_step );
			   unsigned char* curr_point_temp = GETPIXELPTRMACRO( result, col, row, width_step, pixel_step );
			   curr_point_temp[0] =255-curr_point[0] ; 		//invert the pixel value
		  }
	 }
}

int  sum_of_pixel_value(int* hist_value, int threshold_from, int threshold_to)
{
	int sum = 0;
	for(int i = threshold_from;i < threshold_to; i++)
	{
		sum += (i * hist_value[i]);			//x * hist(x)
	}
	return sum;
}

int sum_of_pixel_number(int* hist_value, int threshold_from, int threshold_to)
{
	int sum = 0;
	for(int i = threshold_from; i < threshold_to; i++)
	{
		sum += hist_value[i];
	}
	return sum;
}
int determine_optimal_threshold( CvHistogram* hist )
{
	// TO DO:  Given a 1-D CvHistogram you need to determine and return the optimal threshold value.

	// NOTES:  Assume there are 256 elements in the histogram.
	//         To get the histogram value at index i
	//            int histogram_value_at_i = ((int) *cvGetHistValue_1D(hist, i));

	int histogram_value_at[256];		// amount of pixel of every greyscale value 
	
	int temp_threshold_sum = 0;		//for calculating the initial_threshold
	int threshold_sum = 0;
	
	int initial_threshold = 0;			//T(1)
	int final_threshold = 0;			//T(n+1)

	int background = 0;				//summation of greyscale values of background
	int object = 0;					//summation of greyscale values of object

	int threshold_1 = 0;
	int threshold_2 = 0;

	//determine the first threshold. 
	//I initial the first threshold value by finding the value which divides the area of the curve into halves.
	for(int i = 0; i< 256; i++)
	{
		histogram_value_at[i] =  ((int) *cvGetHistValue_1D(hist, i));
		threshold_sum += histogram_value_at[i];
	}

	while(temp_threshold_sum < (threshold_sum/2))
	{
		temp_threshold_sum += histogram_value_at[initial_threshold++];
	}
	
	threshold_1 = initial_threshold;

	//optimal thresholding
	while(1)
	{
		object = sum_of_pixel_value(histogram_value_at, 0, threshold_1)/sum_of_pixel_number(histogram_value_at, 0, threshold_1);
		background = sum_of_pixel_value(histogram_value_at, threshold_1, 256)/sum_of_pixel_number(histogram_value_at, threshold_1, 256);

		threshold_2 = threshold_1;

		threshold_1 = (object + background)/2;
		
		if(abs(threshold_2 - threshold_1) < 1)		//T(n) = T(n+1) approximately
		{
			break;
		}

		//else continue doing the loop until T(n) = T(n+1)
	}

	return threshold_1;
	
}

void apply_threshold_with_mask(IplImage* grayscale_image,IplImage* result_image,IplImage* mask_image,int threshold)
{
	// TO DO:  Apply binary thresholding to those points in the passed grayscale_image which correspond to non-zero
	//        points in the passed mask_image.  The binary results (0 or 255) should be stored in the result_image.

	//The step width for result_image(HSV) and grayscale_image(greyscale) is different.
	//Set the width step and pixel step respectively for images in different format
	
	 int result_width_step=result_image->widthStep;
	 int result_pixel_step=result_image->widthStep/result_image->width;
	 int number_channels=result_image->nChannels;

	 int width_step=grayscale_image->widthStep;
	int pixel_step=grayscale_image->widthStep/grayscale_image->width;

	 int row=0,col=0;
	 unsigned char black_pixel[4] = {0,0,0,0};
	 
	 IplImage* tmp_binary_image = cvCloneImage(grayscale_image); 
	 cvZero(tmp_binary_image);

	 //Different threshold for every small subpicture.
	 cvThreshold(grayscale_image, tmp_binary_image, threshold, 255, CV_THRESH_BINARY_INV);
	 
	 for (row=0; row < result_image->height; row++)
	 {
		  for (col=0; col < result_image->width; col++)
		  {
			   unsigned char* curr_point_mask_image = GETPIXELPTRMACRO( mask_image, col, row, width_step, pixel_step );
			   unsigned char* curr_point_tmp_binary_image = GETPIXELPTRMACRO( tmp_binary_image, col, row, width_step, pixel_step );
			   //if the black object is in the area of mask (signs)
			   if((curr_point_mask_image[0] > 0) && (curr_point_tmp_binary_image[0] > 0))
			   {
			   		//overlay the corresponding pixel to the result image.
			   		PUTPIXELMACRO( result_image, col, row, black_pixel, result_width_step, result_pixel_step, number_channels );
			   }
			   
		  }
	 }
}

void determine_optimal_sign_classification( IplImage* original_image, IplImage* red_point_image, CvSeq* red_components, CvSeq* background_components, IplImage* result_image )
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
	
	while (curr_red_region != NULL)
	{
		cvZero( mask_image );
		CvScalar color = CV_RGB( 255, 255, 255 );
		CvScalar mask_value = cvScalar( 255 );	//white point
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

double template_original_match(IplImage* original_image,IplImage* template_image)
{
	//resize the template and original sub image. 
	//the scale difference 8 is concluded by repeated trials.
	//8*8 times matches will be done.The max value is more reasonable than resize the template and original with the same size and only do the matching once.
	
	IplImage* resized_original_image = cvCreateImage(cvSize(MATCHING_WIDTH + 8, MATCHING_HEIGHT + 8),original_image->depth,original_image-> nChannels);
	IplImage* resized_template_image = cvCreateImage(cvSize(MATCHING_WIDTH, MATCHING_HEIGHT),template_image->depth,template_image-> nChannels);
	IplImage* matching_result = cvCreateImage( cvSize(resized_original_image->width - resized_template_image -> width + 1,resized_original_image->height - resized_template_image->height + 1), IPL_DEPTH_32F, 1 );
	
	double min_val;  
	double max_val;
	CvPoint min_loc;
	CvPoint max_loc;  

	cvResize(original_image,resized_original_image);
	cvResize(template_image,resized_template_image);

	//cvSmooth(resized_original_image,resized_original_image);
	//cvSmooth(resized_template_image,resized_template_image);


	//The match with max_val is the best match
	cvMatchTemplate(resized_original_image,resized_template_image,matching_result,CV_TM_CCOEFF_NORMED);
	cvMinMaxLoc(matching_result, &min_val, &max_val, &min_loc, &max_loc, NULL);

	return max_val;
	
}

//extract the sub image from the whole image. 
//The first node of a linkedlist with images and coordinates in the original image is returned.
sub_image* extract_sub_image(IplImage* original_image, int min_width, int min_height)
{
	int left = 0;
	int top = 0;
	int right = 0;
	int buttom = 0;
	int temp_x = 0;
	int temp_y = 0;

	int contour_flag = 0;
	int flag = 0;


	sub_image* temp_result =  (sub_image*)malloc(sizeof(sub_image));;
	sub_image* result = NULL;
	sub_image* prev = NULL;

	IplImage* temp_original_image = cvCloneImage(original_image);
	IplImage* temp = cvCloneImage(original_image);

	//search the connected components(here search the connected area)
	CvSeq* contours = connected_components(temp_original_image, temp);

	for (CvSeq* c=contours ;c!=NULL; c=c->h_next)
	{
		for (int i = 0; i < c->total; i ++)
		{
			CvPoint* p = (CvPoint*)cvGetSeqElem(c, i);
			temp_x = p->x;
			temp_y = p->y;
			//printf("x = %d, y = %d\n",temp_x,temp_y);

			//do the approximation
			if(i == 0)
			{
				left = temp_x;
				right = temp_x;
				top = temp_y;
				buttom = temp_y;
			}
			else
			{
				if(temp_x <= left)	left = temp_x;
				if(temp_x >= right) right = temp_x;
				if(temp_y <= top) top = temp_y;
				if(temp_y >= buttom) buttom = temp_y;
			}
		}

		//Not a correct area.To define the value of min_width and min_height I estimate the smallest scale of sub image.
		if(right-left < min_width || buttom - top < min_height)
		{
			continue;
		}
		else
		{
			//printf("find subimage %d\n",flag++);
			cvSetImageROI(temp_original_image, cvRect(left,top,right - left,buttom - top));

			if(contour_flag++ == 0)
			{
				result = temp_result;
			}
			
			temp_result->image = cvCreateImage(cvSize(right - left, buttom - top),temp_original_image->depth,temp_original_image-> nChannels);
			
			temp_result->image_left = left;
			temp_result->image_top = top;
			temp_result->image_right = right;
			temp_result->image_buttom = buttom;
			
			//copy the region of interesting(the sub image) to the destination image.
			cvCopy(temp_original_image, temp_result->image, NULL);
			
			//cvShowImage(string[flag++],temp_result->image);

			//organize the linkedlist.
			temp_result->next_image =  (sub_image*)malloc(sizeof(sub_image));
			prev = temp_result;
			temp_result = temp_result->next_image;

			cvResetImageROI(temp_original_image);
		}
	}
	
	temp_result = prev;
	temp_result->next_image= NULL;
	//the head node of the linkedlist is returned.
	return result;
}

void recognize(IplImage* original_images, IplImage* processed_original_images, IplImage** templates)
{
	double matching_result = 0;
	double matching_max = 0;
	double matching_first_max = 0;
	int template_flag = -1;

	IplImage* drawed_original_images = cvCloneImage(original_images);
	//extract the sub images first and do the matching image by image.
	sub_image* sub_images = extract_sub_image(processed_original_images,MIN_SUB_IMAGE_WIDTH,MIN_SUB_IMAGE_HEIGHT);

	int ori_flag = 0;	
	while(sub_images != NULL)
	{
		//different original sub image to be matched.
		printf("ori_flag  = %d\n",ori_flag++);
		for(int i = 0; i < TEMPLATES_NUM; i++)
		{	
			//match the given image with the provided templates.
			
			//extract the template as well to cut the black edges which might leads negative impacts to the matching.
			sub_image* temp_template = extract_sub_image(templates[i],MIN_SUB_IMAGE_WIDTH,MIN_SUB_IMAGE_HEIGHT);

			//match the original sub image with template.
			matching_result = template_original_match(sub_images->image, temp_template->image);
			
			printf("with template %d, result = %f\n",i,matching_result);
			
			//find the maximum
			if(i == 0)
			{
				matching_first_max = matching_result;
				matching_max = matching_result;
			}
			else
			{
				if(matching_result > matching_max)
				{					
					matching_max = matching_result ;
					template_flag = i;
				}
			}
		}
		
		if(matching_first_max == matching_max)
		{
			template_flag = 0;
		}

		//if the object not found or the match result is not reasonable, do nothing.
		if(template_flag == -1 || matching_max < MATCHING_FALIED_THRESHOLD)
		{
			printf("image not matched\n");	
		}
		else
		{
			//draw the matched template on the original image.
			IplImage* drawed_templates_resized = cvCreateImage(cvSize(30,30),processed_original_images->depth,processed_original_images-> nChannels);
			sub_image* min_temp_template = extract_sub_image(templates[template_flag],MIN_SUB_IMAGE_WIDTH,MIN_SUB_IMAGE_HEIGHT);
			cvResize(min_temp_template->image,drawed_templates_resized);
			
			printf("draw left = %d, top = %d\n",sub_images->image_left,sub_images->image_top);
			//draw the template.
			cvSetImageROI(drawed_original_images, cvRect(sub_images->image_left, sub_images->image_top, drawed_templates_resized->width, drawed_templates_resized->height));  
			cvCopy(drawed_templates_resized, drawed_original_images);  
			cvResetImageROI(drawed_original_images); 
		}
		sub_images = sub_images->next_image;
	}
	
	cvShowImage("result",drawed_original_images);
}

//load the template images.
int  load_templates(IplImage** templates)
{

	if( (templates[0] = cvLoadImage("./GoLeft.jpg",-1)) == 0 )
		return 0;
	if( (templates[1] = cvLoadImage("./GoRight.jpg",-1)) == 0 )
		return 0;
	if( (templates[2] = cvLoadImage("./GoStraight.jpg",-1)) == 0 )
		return 0;
	if( (templates[3] = cvLoadImage("./NoLeftTurn.jpg",-1)) == 0 )
		return 0;
	if( (templates[4] = cvLoadImage("./NoParking.jpg",-1)) == 0 )
		return 0;
	if( (templates[5] = cvLoadImage("./NoRightTurn.jpg",-1)) == 0 )
		return 0;
	if( (templates[6] = cvLoadImage("./NoRightTurn2.jpg",-1)) == 0 )
		return 0;
	if( (templates[7] = cvLoadImage("./NoStraight.jpg",-1)) == 0 )
		return 0;
	if( (templates[8] = cvLoadImage("./Parking.jpg",-1)) == 0 )
		return 0;
	if( (templates[9] = cvLoadImage("./Yield.jpg",-1)) == 0 )
		return 0;

	return 1;
	
}

//process the templates into images with same color features of processed very original image with real objects.
void normalize_template(IplImage** templates)
{
	IplImage* selected_image = NULL;
	IplImage* temp_image = NULL;
	IplImage* red_point_image = NULL;
	IplImage* connected_reds_image = NULL;
	IplImage* connected_background_image = NULL;
	IplImage* result_image = NULL;
	CvSeq* red_components = NULL;
	CvSeq* background_components = NULL;

	for(int i = 0; i < TEMPLATES_NUM; i++)
	{
		if (red_point_image != NULL)
		{
			cvReleaseImage( &red_point_image );
			cvReleaseImage( &temp_image );
			cvReleaseImage( &connected_reds_image );
			cvReleaseImage( &connected_background_image );
			cvReleaseImage( &result_image );
		}
		selected_image = templates[i];
		red_point_image = cvCloneImage( selected_image );
		result_image = cvCloneImage( selected_image );
		temp_image = cvCloneImage( selected_image );
		connected_reds_image = cvCloneImage( selected_image );
		connected_background_image = cvCloneImage( selected_image );

		//the same algorithm as process the image with real objects is used. 
		find_red_points( selected_image, red_point_image, temp_image );
		red_components = connected_components( red_point_image, connected_reds_image );
		invert_image( red_point_image, temp_image );
		background_components = connected_components( temp_image, connected_background_image );
		determine_optimal_sign_classification( selected_image, red_point_image, red_components, background_components, result_image );

		cvCopy(result_image, templates[i]);  
	}
}
int main( int argc, char** argv )
{
	int selected_image_num = 1;
	char show_ch = 's';
	IplImage* images[NUM_IMAGES];
	IplImage* selected_image = NULL;
	IplImage* temp_image = NULL;
	IplImage* red_point_image = NULL;
	IplImage* connected_reds_image = NULL;
	IplImage* connected_background_image = NULL;
	IplImage* result_image = NULL;
	CvSeq* red_components = NULL;
	CvSeq* background_components = NULL;

	// Load all the images.
	for (int file_num=1; (file_num <= NUM_IMAGES); file_num++)
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

	//load the template images and do normalization.
	IplImage* templates[TEMPLATES_NUM];
	load_templates(templates);
	normalize_template(templates);


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
    //cvNamedWindow( "Original", 1 );
    cvNamedWindow( "Processed Image", 1 );


	// Setup mouse callback on the original image so that the user can see image values as they move the
	// cursor over the image.
    cvSetMouseCallback( "Original", on_mouse_show_values, 0 );
	window_name_for_on_mouse_show_values="Original";
	image_for_on_mouse_show_values=selected_image;

	int user_clicked_key = 0;
	do {
		// Create images to do the processing in.
		if (red_point_image != NULL)
		{
			cvReleaseImage( &red_point_image );
			cvReleaseImage( &temp_image );
			cvReleaseImage( &connected_reds_image );
			cvReleaseImage( &connected_background_image );
			cvReleaseImage( &result_image );
		}
		selected_image = images[selected_image_num-1];
		red_point_image = cvCloneImage( selected_image );
		result_image = cvCloneImage( selected_image );
		temp_image = cvCloneImage( selected_image );
		connected_reds_image = cvCloneImage( selected_image );
		connected_background_image = cvCloneImage( selected_image );

		// Process image
		image_for_on_mouse_show_values = selected_image;
		find_red_points( selected_image, red_point_image, temp_image );
		red_components = connected_components( red_point_image, connected_reds_image );
		invert_image( red_point_image, temp_image );
		background_components = connected_components( temp_image, connected_background_image );
		determine_optimal_sign_classification( selected_image, red_point_image, red_components, background_components, result_image );

		//recognize the result_image(with white/black/red only) with the processed templates.
		recognize(selected_image,result_image,templates);

		// Show the original & result
        //cvShowImage( "Original", selected_image );
		do {
			if ((user_clicked_key == 'r') || (user_clicked_key == 'c') || (user_clicked_key == 'h') || (user_clicked_key == 's'))
				show_ch = user_clicked_key;
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
				cvShowImage( "Processed Image", result_image );
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
