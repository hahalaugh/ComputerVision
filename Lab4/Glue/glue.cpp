#ifdef _CH_
#pragma package <opencv>
#endif

#include "cv.h"
#include "highgui.h"
#include <stdio.h>
#include <stdlib.h>
#include "../utilities.h"


#define NUM_IMAGES 9
#define FIRST_LABEL_ROW_TO_CHECK 390
#define LAST_LABEL_ROW_TO_CHECK 490
#define ROW_STEP_FOR_LABEL_CHECK 20
#define DEVIATION 5		// Deviation of the edge points

//intersections of line and edges.
int bottle_edge_left[6] = {0,0,0,0,0,0};		
int bottle_edge_right[6] = {0,0,0,0,0,0};
int label_edge_left[6] = {0,0,0,0,0,0};
int label_edge_right[6] = {0,0,0,0,0,0};
int temp_edge[4] = {0,0,0,0};


bool find_label_edges( IplImage* edge_image, IplImage* result_image, int row, int& left_label_column, int& right_label_column )
{
	// TO-DO:  Search for the sides of the labels from both the left and right on "row".  The side of the label is taken	
	//        taken to be the second edge located on that row (the side of the bottle being the first edge).  If the label
	//        are found set the left_label_column and the right_label_column and return true.  Otherwise return false.
	//        The routine should mark the points searched (in yellow), the edges of the bottle (in blue) and the edges of the
	//        label (in red) - all in the result_image.
	int width_step=edge_image->widthStep;
	int pixel_step=edge_image->widthStep/edge_image->width;
	int temp_edge[4] = {0,0,0,0};		
	
	int col = 0;
	int temp_edge_flag = 0;	//to indicate the current point from different direction. bottle,label,label,bottle.
		
		for (col=0; col < edge_image->width; col++)
		{
			unsigned char* curr_point = GETPIXELPTRMACRO( edge_image, col, row, width_step, pixel_step );
			
			if(curr_point[0] == 255)
			{
				temp_edge[temp_edge_flag++] = col;
			}

			//from left to right find the most first two points.
			if(temp_edge_flag == 2)
			{
				temp_edge_flag = 3;
				break;
			}
		}
		//jump to another side of the picture
		for (col=edge_image->width; col > 0; col--)
		{
			unsigned char* curr_point = GETPIXELPTRMACRO( edge_image, col, row, width_step, pixel_step );
			
			if(curr_point[0] == 255)
			{
				temp_edge[temp_edge_flag--] = col;
			}

			//find two edge points and break.
			if(temp_edge_flag == 1)
			{
				break;
			}

		}
		//store the two label edge points.
		left_label_column = temp_edge[1];
		right_label_column = temp_edge[2];
		
		//draw lines from edge of the image to edge of the label.
		cvLine(result_image,cvPoint(0,row),cvPoint(left_label_column,row),cvScalar(255,255,255));
		cvLine(result_image,cvPoint(right_label_column,row),cvPoint(edge_image->width,row),cvScalar(255,255,255));

		//no label edge in the image.
		if(left_label_column == 0||right_label_column ==0||left_label_column > 100 ||right_label_column < 100)
		{
			for(int i = 0; i< 6; i++)
			{
				//clean the previous edge which might impact showing the intersections.
				label_edge_left[i] = 0;				
				label_edge_right[i] = 0;
			}
			return false;
		}
		else
		{
			//lable in the picture.
			return true;
		}
		
}

void check_glue_bottle( IplImage* original_image, IplImage* result_image )
{
	// TO-DO:  Inspect the image of the glue bottle passed.  This routine should check a number of rows as specified by 
	//        FIRST_LABEL_ROW_TO_CHECK, LAST_LABEL_ROW_TO_CHECK and ROW_STEP_FOR_LABEL_CHECK.  If any of these searches
	//        fail then "No Label" should be written on the result image.  Otherwise if all left and right column values
	//        are roughly the same "Label Present" should be written on the result image.  Otherwise "Label crooked" should
	//        be written on the result image.

	//         To implement this you may need to use smoothing (cv::GaussianBlur() perhaps) and edge detection (cvCanny() perhaps).
	//        You might also need cvConvertImage() which converts between different types of image.


	IplImage* grayscale_image = cvCreateImage( cvGetSize(original_image), 8, 1 );
	cvConvertImage( original_image, grayscale_image ); 

	IplImage* temp = cvCloneImage(grayscale_image);
	//GAUSSIAN SMOOTH.
	cvSmooth(temp, grayscale_image, CV_GAUSSIAN,7,7,0,0);
	//find the edge pixels.
	cvCanny(grayscale_image,grayscale_image,20,100);
	
	int temp_edge_left = 0;
	int temp_edge_right = 0;

	int label_flag = false;
	int i = 0;
	int row = 0;
	for(row = FIRST_LABEL_ROW_TO_CHECK,i = 0;row <= LAST_LABEL_ROW_TO_CHECK;i++, row += ROW_STEP_FOR_LABEL_CHECK )
	{
		if(!find_label_edges(grayscale_image,grayscale_image,row,temp_edge_left,temp_edge_right))
		{
			//no label found.
			label_flag = false;
			break;
		}
		else
		{
			//label in the image and store the pixel coordinates.
			label_edge_left[i] = temp_edge_left;
			label_edge_right[i] = temp_edge_right;
			label_flag = true;
		}
	}
	
	int width_step =grayscale_image->widthStep;
	int pixel_step =grayscale_image->widthStep/grayscale_image->width;

	int result_width_step =result_image->widthStep;
	int result_pixel_step =result_image->widthStep/result_image->width;
	cvZero(result_image);

	unsigned char white_pixel[4] = {255,255,255,0};
	unsigned char yellow_pixel[4] = {255,255,0,0};
	unsigned char red_pixel[4] = {255,0,0,0};
	unsigned char blue_pixel[4] = {0,0,255,0};

	//convert the gray scale image to a RGB image.
	for (row=0; row < grayscale_image->height; row++)
	{
		for (int col=0; col < grayscale_image->width; col++)
		{
			unsigned char* curr_point = GETPIXELPTRMACRO( grayscale_image, col, row, width_step, pixel_step );
			if(curr_point[0] == 255)
			{
				PUTPIXELMACRO( result_image, col, row, white_pixel, result_width_step, result_pixel_step, 4 );
			}
		}
	}
	


	int edge_flag = 0;
	int temp_col = 0;
	//detect the edge pixels of bottle and label respectively.
	for(row = FIRST_LABEL_ROW_TO_CHECK;row <= LAST_LABEL_ROW_TO_CHECK;row++)
	{
		for (int col=0; col < result_image->width; col++)
		{
			unsigned char* curr_point = GETPIXELPTRMACRO( result_image, col, row, result_width_step, result_pixel_step );
			if(curr_point[0] == 255)
			{
				//an edge pixel.
				if(edge_flag == 0)
				{
					//the firest edge pixel from left to right. it is the bottle edge pixel.
					PUTPIXELMACRO( result_image, col, row, red_pixel, result_width_step, result_pixel_step, 4 );
					temp_col = col;	//store the current col value.
					edge_flag++;
				}
				else if(edge_flag == 1)
				{
					//the seconde edge pixel from left to right. it is the lable edge pixel if the spacing between to edge pixel is less than 
					// a specific value(in case of taking other edge of the bottle as the label edge in an image without label)
					if(abs(temp_col - col) > 100)
					{
						temp_col = 0;
						edge_flag = 0;
						break;
					}
					else
					{	
						//is a label pixel.
						PUTPIXELMACRO( result_image, col, row, blue_pixel, result_width_step, result_pixel_step, 4 );
						edge_flag = 0;
						temp_col = 0;
						break;	
					}
				}
				
			}
		}
	}
	//reset the temp col value and do the same thing from the opposite direction.
	temp_col = 0;
	for(row = FIRST_LABEL_ROW_TO_CHECK;row <= LAST_LABEL_ROW_TO_CHECK;row++)
	{
		for (int col=result_image->width; col > 0 ; col--)
		{
			unsigned char* curr_point = GETPIXELPTRMACRO( result_image, col, row, result_width_step, result_pixel_step );
			if(curr_point[0] == 255)
			{
				if(edge_flag == 0)
				{
					PUTPIXELMACRO( result_image, col, row, red_pixel, result_width_step, result_pixel_step, 4 );
					temp_col = col;
					edge_flag++;
				}
				else if(edge_flag == 1)
				{
					if(abs(temp_col - col) > 100)
					{
						temp_col = 0;
						edge_flag = 0;
						break;
					}
					else
					{	
						PUTPIXELMACRO( result_image, col, row, blue_pixel, result_width_step, result_pixel_step, 4 );
						edge_flag = 0;
						temp_col = 0;
						break;	
					}
				}
				
			}
		}
	}

	//draw yellow pixels on the intersections.
	for(row = FIRST_LABEL_ROW_TO_CHECK,i = 0;row <= LAST_LABEL_ROW_TO_CHECK;i++, row += ROW_STEP_FOR_LABEL_CHECK )
	{
		PUTPIXELMACRO( result_image, label_edge_left[i], row, yellow_pixel, result_width_step, result_pixel_step, 4 );
		PUTPIXELMACRO( result_image, label_edge_right[i], row, yellow_pixel, result_width_step, result_pixel_step, 4 );
	}

	if(!label_flag)
	{
		write_text_on_image(result_image,0,0,"No Label");
	}
	else
	{
		//if the deviation of 6 edge pixels is greater than a specific value, the label is crooked.
		if(abs(label_edge_left[5] - label_edge_left[0] )> DEVIATION)
		{
			write_text_on_image(result_image,0,0,"Crooked Label");
		}
		else
		{	//label position is appropriate.
			write_text_on_image(result_image,0,0,"Label Presentation");
		}
	}
}

int main( int argc, char** argv )
{
	int selected_image_num = 1;
	IplImage* selected_image = NULL;
	IplImage* images[NUM_IMAGES];
	IplImage* result_image = NULL;

	// Load all the images.
	for (int file_num=1; (file_num <= NUM_IMAGES); file_num++)
	{
		char filename[100];
		sprintf(filename,"./Glue%d.jpg",file_num);
		if( (images[file_num-1] = cvLoadImage(filename,-1)) == 0 )
			return 0;
	}

	// Explain the User Interface
    printf( "Hot keys: \n"
            "\tESC - quit the program\n");
    printf( "\t1..%d - select image\n",NUM_IMAGES);
    
	// Create display windows for images
    cvNamedWindow( "Original", 1 );
    cvNamedWindow( "Processed Image", 1 );

	// Create images to do the processing in.
	selected_image = cvCloneImage( images[selected_image_num-1] );
    result_image = cvCloneImage( selected_image );

	// Setup mouse callback on the original image so that the user can see image values as they move the
	// cursor over the image.
    cvSetMouseCallback( "Original", on_mouse_show_values, 0 );
	window_name_for_on_mouse_show_values="Original";
	image_for_on_mouse_show_values=selected_image;

	int user_clicked_key = 0;
	do {
		// Process image (i.e. setup and find the number of spoons)
		cvCopyImage( images[selected_image_num-1], selected_image );
        cvShowImage( "Original", selected_image );
		image_for_on_mouse_show_values=selected_image;
		check_glue_bottle( selected_image, result_image );
		cvShowImage( "Processed Image", result_image );

		// Wait for user input
        user_clicked_key = cvWaitKey(0);
		if ((user_clicked_key >= '1') && (user_clicked_key <= '0'+NUM_IMAGES))
		{
			selected_image_num = user_clicked_key-'0';
		}
	} while ( user_clicked_key != ESC );

    return 1;
}
