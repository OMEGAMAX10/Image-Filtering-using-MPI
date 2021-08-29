#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

/* return codes*/
#define STATUS_SUCCES 0
#define ERR_BASE 100
#define ERR_IO (ERR_BASE + 1)
#define ERR_INVALID_ARG_NR (ERR_BASE + 2)
#define ERR_MEMORY (ERR_BASE + 3)
#define ERR_INVALID_FILTER_NAME (ERR_BASE + 4)

/* Filter Names */
char *filter_names[] = {"smooth", "blur", "sharpen", "mean", "emboss"};

/* Filters */
float filters[5][3][3] = {
	{{1.0 / 9, 1.0 / 9, 1.0 / 9}, {1.0 / 9, 1.0 / 9, 1.0 / 9}, {1.0 / 9, 1.0 / 9, 1.0 / 9}},		/////	Smooth
	{{1.0 / 16, 2.0 / 16, 1.0 / 16}, {2.0 / 16, 4.0 / 16, 2.0 / 16}, {1.0 / 16, 2.0 / 16, 1.0 / 16}},	/////	Gaussian Blur
	{{0, -2.0 / 3, 0}, {-2.0 / 3, 11.0 / 3, -2.0 / 3}, {0, -2.0 / 3, 0}},					/////	Sharpen
	{{-1.0, -1.0, -1.0}, {-1.0, 9.0, -1.0}, {-1.0, -1.0, -1.0}},						/////	Mean
	{{0.0, 1.0, 0.0}, {0.0, 0.0, 0.0}, {0.0, -1.0, 0.0}}};							/////	Emboss

/** Types of pixels **/
/* RGB */
typedef struct
{
	unsigned char red, green, blue;
} rgb_pixel;

/* Gray */
typedef unsigned char gray_pixel;

/** Structure that stores a .pnm image, either a grayscale or a rgb one **/
typedef struct
{
	int width, height;
	int type; // 5 - grayscale, 6 - rgb
	unsigned char maxval;
	rgb_pixel *rgb_image;
	gray_pixel *gray_image;
} image;

// Read image from .pnm file
int read_image(char *img_filename, image *img)
{
	FILE *img_file = fopen(img_filename, "r");
	if (img_file == NULL)
	{
		printf("Cannot open image %s\n", img_filename);
		return ERR_IO;
	}
	char buff_type[3];
	fscanf(img_file, "%s\n%d %d\n%hhd\n", buff_type, &(img->width), &(img->height), &(img->maxval));
	img->type = buff_type[1] - '0';
	int start_image = ftell(img_file);
	fclose(img_file);
	img_file = fopen(img_filename, "rb");
	fseek(img_file, start_image, SEEK_SET);
	if (img->type == 5) // grayscale
	{
		img->rgb_image = NULL;
		img->gray_image = (gray_pixel *)malloc(img->height * img->width * sizeof(gray_pixel));
		if (img->gray_image == NULL)
			return ERR_MEMORY;
		fread((img->gray_image), sizeof(gray_pixel) * img->height * img->width, 1, img_file);
	}
	else // color
	{
		img->gray_image = NULL;
		img->rgb_image = (rgb_pixel *)malloc(img->height * img->width * sizeof(rgb_pixel));
		if (img->rgb_image == NULL)
			return ERR_MEMORY;
		fread((img->rgb_image), sizeof(rgb_pixel) * img->height * img->width, 1, img_file);
	}
	fclose(img_file);
	return STATUS_SUCCES;
}

// Write image to .pnm file
int write_image(char *img_filename, image *img)
{
	FILE *img_file = fopen(img_filename, "wb");
	if (img_file == NULL)
	{
		printf("Cannot open image %s\n", img_filename);
		return ERR_IO;
	}
	char header[100];
	sprintf(header, "P%d\n%d %d\n%d\n", img->type, img->width, img->height, img->maxval);
	fwrite(header, strlen(header), 1, img_file);
	if (img->type == 5) // grayscale
	{
		fwrite((img->gray_image), sizeof(gray_pixel) * img->width * img->height, 1, img_file);
		free(img->gray_image);
		img->gray_image = NULL;
	}
	else // color
	{
		fwrite((img->rgb_image), sizeof(rgb_pixel) * img->width * img->height, 1, img_file);
		free(img->rgb_image);
		img->rgb_image = NULL;
	}
	fclose(img_file);
	return STATUS_SUCCES;
}

// Apply a filter whose name is from the filter_name list on a grayscale image
int apply_filter_gray(image *img_in, gray_pixel **img_block, int rank, int block_size, int length, char *filter_name)
{
	for (int filter_idx = 0; filter_idx < 5; filter_idx++)
	{
		if (strcmp(filter_name, filter_names[filter_idx]) == 0)
		{
			float new_gray_pixel;
			int lin, col;
			for (int i = 0; i < length; i++)
			{
				lin = (block_size * rank + i) / (img_in->width);
				col = (block_size * rank + i) % (img_in->width);
				if ((lin > 0) && (lin < (img_in->height - 1)) && (col > 0) && (col < (img_in->width - 1)))
				{
					new_gray_pixel = 0;
					for (int i_f = -1; i_f < 2; i_f++)
						for (int j_f = -1; j_f < 2; j_f++)
							new_gray_pixel += filters[filter_idx][i_f + 1][j_f + 1] * (float)(img_in->gray_image)[(lin + i_f) * (img_in->width) + (col + j_f)];
					(*img_block)[i] = (gray_pixel)new_gray_pixel;
				}
				else
					(*img_block)[i] = (img_in->gray_image)[lin * (img_in->width) + col];
			}
			return STATUS_SUCCES;
		}
		else if (filter_idx == 4)
		{
			printf("Filter \"%s\" is an invalid filter name. Available filters: smooth, blur, sharpen, mean, emboss.\n", filter_name);
			return ERR_INVALID_FILTER_NAME;
		}
	}
	return STATUS_SUCCES;
}

// Apply a filter whose name is from the filter_name list on a color image
int apply_filter_color(image *img_in, rgb_pixel **img_block, int rank, int block_size, int length, char *filter_name)
{
	for (int filter_idx = 0; filter_idx < 5; filter_idx++)
	{
		if (strcmp(filter_name, filter_names[filter_idx]) == 0)
		{
			float new_pixel_r, new_pixel_g, new_pixel_b;
			int lin, col;
			for (int i = 0; i < length; i++)
			{
				lin = (block_size * rank + i) / (img_in->width);
				col = (block_size * rank + i) % (img_in->width);
				if ((lin > 0) && (lin < (img_in->height - 1)) && (col > 0) && (col < (img_in->width - 1)))
				{
					new_pixel_r = 0;
					new_pixel_g = 0;
					new_pixel_b = 0;
					for (int i_f = -1; i_f < 2; i_f++)
					{
						for (int j_f = -1; j_f < 2; j_f++)
						{
							new_pixel_r += filters[filter_idx][i_f + 1][j_f + 1] * (float)(img_in->rgb_image)[(lin + i_f) * (img_in->width) + (col + j_f)].red;
							new_pixel_g += filters[filter_idx][i_f + 1][j_f + 1] * (float)(img_in->rgb_image)[(lin + i_f) * (img_in->width) + (col + j_f)].green;
							new_pixel_b += filters[filter_idx][i_f + 1][j_f + 1] * (float)(img_in->rgb_image)[(lin + i_f) * (img_in->width) + (col + j_f)].blue;
						}
					}
					(*img_block)[i].red = (unsigned char)new_pixel_r;
					(*img_block)[i].green = (unsigned char)new_pixel_g;
					(*img_block)[i].blue = (unsigned char)new_pixel_b;
				}
				else
				{
					(*img_block)[i].red = (img_in->rgb_image)[lin * (img_in->width) + col].red;
					(*img_block)[i].green = (img_in->rgb_image)[lin * (img_in->width) + col].green;
					(*img_block)[i].blue = (img_in->rgb_image)[lin * (img_in->width) + col].blue;
				}
			}

			return STATUS_SUCCES;
		}
		else if (filter_idx == 4)
		{
			printf("Filter \"%s\" is an invalid filter name. Available filters: smooth, blur, sharpen, mean, emboss.\n", filter_name);
			return ERR_INVALID_FILTER_NAME;
		}
	}
	return STATUS_SUCCES;
}

int main(int argc, char *argv[])
{
	int rank, nProcesses, length, block_size;
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &nProcesses);
	if (argc < 4)
	{
		printf("ERROR: Not enough arguments: <inputFileName> <outputFileName> <filter1> ... <filterK>\n");
		exit(ERR_INVALID_ARG_NR);
	}
	int status = STATUS_SUCCES;
	image img;
	gray_pixel *gray_blk = NULL;
	rgb_pixel *rgb_blk = NULL;
	char img_in_name[100], img_out_name[100];
	strcpy(img_in_name, argv[1]);
	strcpy(img_out_name, argv[2]);
	status = read_image(img_in_name, &img);
	if (status != STATUS_SUCCES)
		return status;

	block_size = (img.width * img.height + (nProcesses - ((img.width * img.height) % nProcesses)) % nProcesses) / nProcesses;
	if (rank < nProcesses - 1)
		length = block_size;
	else
		length = block_size - (nProcesses - ((img.width * img.height) % nProcesses)) % nProcesses;

	if (img.type == 5) // grayscale
	{
		gray_blk = (gray_pixel *)malloc(sizeof(gray_pixel) * block_size);
		for (int i = 3; i < argc; i++)
		{
			MPI_Scatter(&((img.gray_image)[0]), block_size * sizeof(gray_pixel), MPI_UNSIGNED_CHAR, &(gray_blk[0]), block_size * sizeof(gray_pixel), MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
			status = apply_filter_gray(&img, &gray_blk, rank, block_size, length, argv[i]);
			if (status != STATUS_SUCCES)
				return status;
			MPI_Gather(&(gray_blk[0]), block_size * sizeof(gray_pixel), MPI_UNSIGNED_CHAR, &((img.gray_image)[0]), block_size * sizeof(gray_pixel), MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
			MPI_Bcast(&((img.gray_image)[0]), img.width * img.height * sizeof(gray_pixel), MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
		}
		free(gray_blk);
	}
	else
	{
		rgb_blk = (rgb_pixel *)malloc(sizeof(rgb_pixel) * block_size);
		for (int i = 3; i < argc; i++)
		{
			MPI_Scatter(&((img.rgb_image)[0]), block_size * sizeof(rgb_pixel), MPI_UNSIGNED_CHAR, &(rgb_blk[0]), block_size * sizeof(rgb_pixel), MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
			status = apply_filter_color(&img, &rgb_blk, rank, block_size, length, argv[i]);
			if (status != STATUS_SUCCES)
				return status;
			MPI_Gather(&(rgb_blk[0]), block_size * sizeof(rgb_pixel), MPI_UNSIGNED_CHAR, &((img.rgb_image)[0]), block_size * sizeof(rgb_pixel), MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
			MPI_Bcast(&((img.rgb_image)[0]), img.width * img.height * sizeof(rgb_pixel), MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
		}
		free(rgb_blk);
	}
	if (rank == 0)
	{
		status = write_image(img_out_name, &img);
		if (status != STATUS_SUCCES)
			return status;
	}
	MPI_Finalize();
	return STATUS_SUCCES;
}
