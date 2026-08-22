/* Draw a mask on an image.
 *
 * Copied into nip4 to get a direct draw path,
 */

#include <stdio.h>
#include <stdlib.h>

#include <vips/vips.h>

static int
draw_mask_set(VipsImage *image, VipsImage *mask, VipsPel *ink,
	VipsRect *image_clip, VipsRect *mask_clip)
{
	int width = image_clip->width;
	int height = image_clip->height;
	int ps = VIPS_IMAGE_SIZEOF_PEL(image);

	for (int y = 0; y < height; y++) {
		VipsPel *to =
			VIPS_IMAGE_ADDR(image, image_clip->left, y + image_clip->top);
		VipsPel *m =
			VIPS_IMAGE_ADDR(mask, mask_clip->left, y + mask_clip->top);

		for (int x = 0; x < width; x++) {
			if (m[x])
				VIPS_MEMCPY(to, ink, ps);

			to += ps;
		}
	}

	return 0;
}

/* Direct path for draw-mask-along-line or draw-mask-along-circle. We want to
 * avoid function dispatch overhead. For speed, just paint, don't blend.
 */
int
draw_mask(VipsImage *image, VipsPel *ink, VipsImage *mask, int x, int y)
{
	VipsRect image_rect;
	VipsRect area_rect;
	VipsRect image_clip;
	VipsRect mask_clip;

	if (vips_check_coding_noneorlabq("draw_mask", image) ||
		vips_image_inplace(image) ||
		vips_image_wio_input(mask) ||
		vips_check_mono("draw_mask", mask) ||
		vips_check_uncoded("draw_mask", mask) ||
		vips_check_format("draw_mask", mask, VIPS_FORMAT_UCHAR))
		return -1;

	/* Find the area we draw on the image.
	 */
	area_rect.left = x;
	area_rect.top = y;
	area_rect.width = mask->Xsize;
	area_rect.height = mask->Ysize;
	image_rect.left = 0;
	image_rect.top = 0;
	image_rect.width = image->Xsize;
	image_rect.height = image->Ysize;
	vips_rect_intersectrect(&area_rect, &image_rect, &image_clip);

	/* And the area of the mask image we use.
	 */
	mask_clip = image_clip;
	mask_clip.left -= x;
	mask_clip.top -= y;

	if (!vips_rect_isempty(&image_clip) &&
		draw_mask_set(image, mask, ink, &image_clip, &mask_clip))
		return -1;

	return 0;
}
