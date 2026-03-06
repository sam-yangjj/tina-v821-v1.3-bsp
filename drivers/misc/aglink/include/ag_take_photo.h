#ifndef __AGLINK_AGLINK_TAKE_PHOTO_H
#define __AGLINK_AGLINK_TAKE_PHOTO_H

enum TAKE_PHOTO_CODE {
	TP_SUCCESS = 0,
	TP_NO_READY,
	TP_NO_MEM,
	TP_FAILED,
	TP_TIME_OUT,
};

#ifdef CONFIG_AG_USE_VAMLLOC_SAVE_PHOTO
struct wrapper_buff {
	uint8_t *data;
	uint64_t len;
};
#endif
typedef void (*photo_save_cb)(void);

int aglink_take_photo_cache(struct sk_buff *skb);

int aglink_take_photo(int width,
		int height, int channel, struct sk_buff **skb);

int aglink_take_photo_save_init(photo_save_cb cb);

void aglink_take_photo_save_deinit(void);

int aglink_get_rt_media_jpg_resolution(int channel, int *width, int *height);

#ifdef CONFIG_AG_USE_VAMLLOC_SAVE_PHOTO
void aglink_free_wrapeer_buff(struct sk_buff *skb, const char *func);
#endif

#endif
