#ifndef _SSTV_H_
#define _SSTV_H_

#define IMG_BUFFER_SIZE 65536 // default size of JPEG buffer

#define IMG_WIDTH       320 // total image width
#define IMG_HEIGHT      16  // height of decompressed JPEG block

// sizes for text overlay
#define TEXT_Z1_WIDTH   39  /* IMG_WIDTH/8  - 1 */
#define TEXT_Z2_WIDTH   19  /* IMG_WIDTH/16 - 1 */
#define TEXT_Z3_WIDTH   13  /* IMG_WIDTH/24     */
#define TEXT_LEN        40  /* max of above + trailing zero */

#define OVERLAY_HEADER  0
#define OVERLAY_IMG     1
#define OVERLAY_LARGE   2
#define OVERLAY_FROM    3

extern bool jpeg_thumbnail(uint8_t *jpeg, uint8_t **thumbnail);
extern bool jpeg_decompress(uint8_t *jpeg);
extern bool jpeg_test(uint8_t *jpeg, uint32_t length);

extern bool sstv_play_jpeg(uint8_t* jpeg, uint8_t mode);
extern bool sstv_play_thumbnail(uint8_t mode);
extern void sstv_set_overlay(uint8_t line, const char *overlay);

#endif /* _SSTV_H_ */
