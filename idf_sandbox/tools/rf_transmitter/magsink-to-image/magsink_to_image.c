#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "stb_image_write.h"

int main()
{
	FILE * f = fopen( "/tmp/samplelog.dat", "rb" );
	if( !f ) { fprintf( stderr, "Error: can't open /tmp/samplelog.dat\n" ); exit( -59 ); }
	fseek( f, 0, SEEK_END );
	int len = ftell( f );
	fseek( f, 0, SEEK_SET );
	len/=sizeof(float);
	float * data = malloc( len * sizeof( float ) );
	float fMin = 1e20;
	float fMax = -1e20;
	int i;
	for( i = 0; i < len; i++ )
	{
		float val;
		fread( &val, 1, sizeof(float), f );
		if( val > fMax ) fMax = val;
		if( val < fMin ) fMin = val;
		data[i] = val;
	}

	int width = 512;
	int height = len / width;

	uint32_t * outimage = malloc( sizeof( uint32_t ) * width * height );
	int y, x;
	for( y = 0; y < height; y++ )
	{
		float linemin = 1e20;
		float linemax = -1e20;
		for( x = 0; x < width; x++ )
		{
			float v = data[x+y*width];
			if( v < linemin ) linemin = v;
			if( v > linemax ) linemax = v;
			data[x+y*width] = v;
		}

		for( x = 0; x < width; x++ )
		{
			float v = data[x+y*width];
			int intensity = (v - linemin) / (linemax - linemin) * 255.5 * 2 - 256;
			if( intensity < 0 ) intensity = 0;
			if( intensity > 255 ) intensity = 255;
			outimage[x+y*width] = (intensity) | (intensity<<8) | (intensity<<16) | 0xff000000; 
		}
	}
	stbi_write_png( "image.png", width, height, 4, outimage, width*4 );
}

