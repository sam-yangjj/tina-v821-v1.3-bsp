#include <platform.h>
#include <os_dep/os_intf.h>
#include <linux/kernel.h>
#include "debug.h"

#ifdef CONFIG_AGLINK_DEV_NUM
#define AGLINK_DEV_NUM CONFIG_AGLINK_DEV_NUM
#else
#error "CONFIG_AGLINK_DEV_NUM is not defined! Please set the value in Kconfig."
#endif

#define LOOPBACK_UP_DATA 0

int loop_dev_start(void);
void loop_dev_stop(void);
int loop_dev_tx(int dev_id, unsigned char *buff, unsigned short len);

static struct aglink_bus_ops loop_ops = {
	.start = loop_dev_start,
	.stop = loop_dev_stop,
	.tx = loop_dev_tx,
};

#if LOOPBACK_UP_DATA
static void loop_dev_up_data_stress_init(void);
static void loop_dev_up_data_stress_deinit(void);
static int start_push;
#endif
int loop_dev_start(void)
{
	printk("plat test start.\n");
#if LOOPBACK_UP_DATA
	loop_dev_up_data_stress_init();
#endif
	return 0;
}

void loop_dev_stop(void)
{
#if LOOPBACK_UP_DATA
	loop_dev_up_data_stress_deinit();
#endif
	printk("plat test stop.\n");
}

int loop_dev_tx(int dev_id, unsigned char *buff, unsigned short len)
{
	printk("device ID:%d\n", dev_id);
	data_hex_dump(__func__, 20, buff, len);

	if (loop_ops.rx_cb) {
#if LOOPBACK_UP_DATA
		start_push = 1;
#else
		loop_ops.rx_disorder_cb(dev_id, buff, len);
#endif
	}
	return 0;
}

#if LOOPBACK_UP_DATA
static xr_thread_handle_t up_thread;
static int up_thread_exit;

#define KEEP_UP_DATA_TIME_MS (1) /*10ms*/
#include <uapi/aglink/ag_proto.h>

#define UP_BUFF_LEN 768
static uint8_t up_buff[UP_BUFF_LEN];
#define MAX_PACKET_SIZE 256

static int up_data_thread(void *arg)
{
	struct aglink_hdr *hdr;
	int i;
	int pos = 0;
	int send_len;
	int remaining_len;
	hdr = (struct aglink_hdr *)up_buff;
	memcpy(hdr, TAG, TAG_LEN);
	hdr->len = UP_BUFF_LEN - sizeof(struct aglink_hdr);
	hdr->type = AG_AD_AUDIO;
	for (i = 16; i < UP_BUFF_LEN; i++) {
		up_buff[i] = 0xab;
	}
	while (!up_thread_exit) {
		if (start_push && loop_ops.rx_disorder_cb) {
			remaining_len = UP_BUFF_LEN - pos;
			send_len = (remaining_len > MAX_PACKET_SIZE) ? (get_random_int() % MAX_PACKET_SIZE + 1) : remaining_len;
			loop_ops.rx_disorder_cb(0, up_buff + pos, send_len);
			pos += send_len;
			if (pos >= UP_BUFF_LEN) {
				pos = 0;
			}
		}
		msleep(KEEP_UP_DATA_TIME_MS);
	}
	aglink_k_thread_exit(&up_thread);
	return 0;
}

static void loop_dev_up_data_stress_init(void)
{
	aglink_k_thread_create(&up_thread,
			"up_d_s", up_data_thread, (void *)NULL, 0, 4096);
}

static void loop_dev_up_data_stress_deinit(void)
{
	up_thread_exit = 1;
	aglink_k_thread_delete(&up_thread);

}
#endif
extern int aglink_register_dev_ops(struct aglink_bus_ops *ops, int dev_id);

void loop_dev_init(void)
{
	int i;
	for (i = 0; i < AGLINK_DEV_NUM; i++) {
		printk("register loopback device, ID = %d\n", i);
		aglink_register_dev_ops(&loop_ops, i);
	}
}
