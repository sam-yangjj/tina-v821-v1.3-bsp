#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kfifo.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/fs.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/atomic.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <uapi/linux/sched/types.h>

#include "aglink_gyro.h"
#include "debug.h"
#include "aglink_gyro_osintf.h"

#define GYRO_FIFO_CNT  (1024 * 10)
#define GYRO_FIFO_CNT_WATER_LINE  (1024 * 8)
#define GYRO_STORAGE_WATER_LINE  (1024 * 2)

#define MAX_FILE_PATH_SIZE  (64)
#define TAKE_GYRO_PATCH "/mnt/UDISK/video-"
#define TAKE_GYRO_NAME "gyro-"
#define VIDEO_SUBDIR_NAME_POINT "/proc/video_subdir_name"

struct gyro_timestamp {
	u64 timestamp;
};

#define GYRO_FIFO_SIZE		(sizeof(struct gyro_pkt) * GYRO_FIFO_CNT)
#define GYRO_FIFO_SIZE_WATER_LINE		(sizeof(struct gyro_pkt) * GYRO_FIFO_CNT_WATER_LINE)
#define GYRO_STORAGE_SIZE   (64 * GYRO_STORAGE_WATER_LINE)

typedef struct video_subdir_t {
	char video_subdir_name_str[32];
	char video_subdir_path_str[MAX_FILE_PATH_SIZE];
	size_t count;
	size_t create;
} video_subdir_t;

typedef struct gyro_storage_t {
	char *dat;
	int pos;
	int cnt;
} gyro_storage_t;

static loff_t pos;
static video_subdir_t video_subdir;
static gyro_storage_t gyro_storage;

static DECLARE_WAIT_QUEUE_HEAD(consumer_waitq);
static DEFINE_SPINLOCK(fifo_lock);
static struct kfifo gyro_fifo;
static struct task_struct *consumer_thread;
static struct file *output_file;
static int gyro_init;
static struct gyro_config gyro_param;

static struct hrtimer gyro_timer;
static ktime_t interval;
static void aglink_gyro_timer_work_handler(struct work_struct *work);
static struct workqueue_struct *hi_pri_wq;
static DECLARE_WORK(timer_work, aglink_gyro_timer_work_handler);

static int aglink_gyro_dat2txt(struct gyro_pkt buffer, char *gyro_txt, int size)
{
	int len = 0;
	struct timespec ts;
	s32 xh = abs(buffer.gyr_dat.x / 1000000);
	s32 xl = abs(buffer.gyr_dat.x % 1000000);
	s32 yh = abs(buffer.gyr_dat.y / 1000000);
	s32 yl = abs(buffer.gyr_dat.y % 1000000);
	s32 zh = abs(buffer.gyr_dat.z / 1000000);
	s32 zl = abs(buffer.gyr_dat.z % 1000000);

	ts.tv_sec = buffer.time / 1000000000;
	ts.tv_nsec = buffer.time % 1000000000;

	len = snprintf(gyro_txt, size, "%ld.%06ld %s%d.%06d %s%d.%06d %s%d.%06d\n",
				  ts.tv_sec, ts.tv_nsec / 1000,
				  buffer.gyr_dat.x > 0 ? "" : "-", xh, xl,
				  buffer.gyr_dat.y > 0 ? "" : "-", yh, yl,
				  buffer.gyr_dat.z > 0 ? "" : "-", zh, zl);

	return len;
}

static int aglink_gyro_save_data(void)
{
	struct gyro_pkt buffer;
	char gyro_dat[64];
	unsigned int recv;
	int len = 0;
	while (!kfifo_is_empty(&gyro_fifo)) {
		unsigned long flags;

		spin_lock_irqsave(&fifo_lock, flags);
		recv = kfifo_out(&gyro_fifo, &buffer, sizeof(buffer));
		spin_unlock_irqrestore(&fifo_lock, flags);

		if (recv != sizeof(buffer)) {
			gyro_printk(AGLINK_GYRO_DBG_ERROR, "actual data read:%d; expected data:%d ,incomplete!\n",
						 recv, sizeof(buffer));
			continue;
		}

		len = aglink_gyro_dat2txt(buffer, gyro_dat, sizeof(gyro_dat) - 1);

		gyro_storage.cnt++;
		memcpy(gyro_storage.dat + gyro_storage.pos, gyro_dat, len);
		gyro_storage.pos += len;
		if (gyro_storage.cnt >= GYRO_STORAGE_WATER_LINE) {
			kernel_write(output_file, gyro_storage.dat, gyro_storage.pos, &pos);
			gyro_storage.cnt = 0;
			gyro_storage.pos = 0;
		}

	}
	return 0;
}

static int aglink_gyro_consumer(void *data)
{
	while (!kthread_should_stop()) {
		wait_event_interruptible(consumer_waitq,
								(!kfifo_is_empty(&gyro_fifo))
								|| kthread_should_stop());

		if (video_subdir.create)
			aglink_gyro_save_data();
	}
	return 0;
}

static int aglink_gyro_create_file(void)
{
	const char *file_path = NULL;
	char aw_license_head[513] = "\0";
	if (video_subdir.create)
		return 0;
	if (!video_subdir.count)
		return -1;
	snprintf(video_subdir.video_subdir_path_str, sizeof(video_subdir.video_subdir_path_str) - 1, "%s%s/%s%s.txt",
			TAKE_GYRO_PATCH, video_subdir.video_subdir_name_str,
			TAKE_GYRO_NAME, video_subdir.video_subdir_name_str);
	file_path = video_subdir.video_subdir_path_str;
	output_file = filp_open(file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (IS_ERR(output_file)) {
		gyro_printk(AGLINK_GYRO_DBG_ERROR, "Failed to open file\n");
		return -1;
	}
	aw_license_head[512] = '\n';
	kernel_write(output_file, &aw_license_head, 513, &pos);
	gyro_printk(AGLINK_GYRO_DBG_ALWY, "%s\n", file_path);
	video_subdir.create = 1;
	return 0;
}

static void aglink_gyro_producer(void)
{
	s16 cnt = 0;
	unsigned long flags;
	struct gyro_pkt *buffer = NULL;

	buffer = aglink_gyro_get_fifo_dat(&cnt);
	if (!buffer)
		return;

	spin_lock_irqsave(&fifo_lock, flags);
	if (kfifo_avail(&gyro_fifo) >= sizeof(struct gyro_pkt) * cnt) {
		kfifo_in(&gyro_fifo, buffer, sizeof(struct gyro_pkt) * cnt);
	} else {
		gyro_printk(AGLINK_GYRO_DBG_ERROR, "Insufficient space in fifo !\n");
	}
	spin_unlock_irqrestore(&fifo_lock, flags);

	aglink_gyro_free(buffer);

	aglink_gyro_create_file();

	if (video_subdir.create) {
		if (kfifo_len(&gyro_fifo) >= GYRO_FIFO_SIZE_WATER_LINE) {
			wake_up_interruptible(&consumer_waitq);
			gyro_printk(AGLINK_GYRO_DBG_ALWY, "gpio sample num %d\n", kfifo_len(&gyro_fifo));
		}
	}
}

static irqreturn_t aglink_gyro_gpio_irq_work_handler(int irq, void *dev_id)
{
	aglink_gyro_producer();
	return IRQ_HANDLED;
}

static irqreturn_t aglink_gyro_gpio_irq_handler(int irq, void *dev_id)
{
	return IRQ_WAKE_THREAD;
}

static void aglink_gyro_timer_work_handler(struct work_struct *work)
{
	aglink_gyro_producer();
}

static enum hrtimer_restart aglink_gyro_timer_handler(struct hrtimer *timer)
{
	queue_work(hi_pri_wq, &timer_work);

	hrtimer_forward_now(timer, interval);
	return HRTIMER_RESTART;
}

static int aglink_gyro_timer_init(void)
{
	u64 gyro_sample_cycle;
	gyro_sample_cycle = aglink_gyro_get_speed();
	interval = ktime_set(0, gyro_sample_cycle);
	gyro_printk(AGLINK_GYRO_DBG_ALWY, "gpio sample cycle %llu\n", gyro_sample_cycle);

	hi_pri_wq = alloc_workqueue("hi_pri_wq",
								WQ_MEM_RECLAIM, 1);
	hrtimer_init(&gyro_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	gyro_timer.function = &aglink_gyro_timer_handler;
	hrtimer_start(&gyro_timer, interval, HRTIMER_MODE_REL);

	return 0;
}

static void aglink_gyro_timer_deinit(void)
{
	hrtimer_cancel(&gyro_timer);
	destroy_workqueue(hi_pri_wq);
}

static int aglink_gyro_sample_init(void)
{
	int virq;
	int ret;
	u32 flags;

	if (gyro_param.gpio_num < 0) {
		aglink_gyro_timer_init();
	} else {
		if (gpio_request(gyro_param.gpio_num, "GPIO_Interrupt") < 0) {
			gyro_printk(AGLINK_GYRO_DBG_ERROR, "gpio request fail\n");
			return -1;
		}
		flags = gyro_param.gpio_active ? IRQF_TRIGGER_RISING : IRQF_TRIGGER_FALLING;
		virq = gpio_to_irq(gyro_param.gpio_num);
		ret = request_threaded_irq(virq, aglink_gyro_gpio_irq_handler,
								aglink_gyro_gpio_irq_work_handler,
								flags,
								"gpio_irq", NULL);
		if (ret) {
			gyro_printk(AGLINK_GYRO_DBG_ERROR, "gpio request thread fail, ret: %d\n", ret);
			gpio_free(gyro_param.gpio_num);
			return -1;
		}
	}

	return 0;
}

static int aglink_gyro_sample_deinit(void)
{
	int virq;

	if (gyro_param.gpio_num < 0) {
		aglink_gyro_timer_deinit();
	} else {
		virq = gpio_to_irq(gyro_param.gpio_num);

		if (free_irq(virq, NULL)) {
			return -1;
		}
		gpio_free(gyro_param.gpio_num);
	}

	return 0;
}

int aglink_gyro_sample_kick_start(struct gyro_config gyroconfig)
{
	if (gyro_init) {
		return 0;
	}
	gyro_printk(AGLINK_GYRO_DBG_ALWY, "sample start\n");

	video_subdir.count = 0;
	video_subdir.create = 0;

	gyro_storage.dat = aglink_gyro_zmalloc(GYRO_STORAGE_SIZE);
	if (!gyro_storage.dat) {
		gyro_printk(AGLINK_GYRO_DBG_ERROR, "Failed to allocate gyro storage\n");
		return -1;
	}
	gyro_storage.pos = 0;
	gyro_storage.cnt = 0;

	if (kfifo_alloc(&gyro_fifo, GYRO_FIFO_SIZE, GFP_KERNEL)) {
		gyro_printk(AGLINK_GYRO_DBG_ERROR, "Failed to allocate FIFO\n");
		goto fail0;
	}

	consumer_thread = kthread_run(aglink_gyro_consumer, NULL, "gyro_consumer");
	if (IS_ERR(consumer_thread)) {
		gyro_printk(AGLINK_GYRO_DBG_ERROR, "Failed to create consumer thread\n");
		goto fail1;
	}
	set_user_nice(consumer_thread, 1);

	memcpy(&gyro_param, &gyroconfig, sizeof(struct gyro_config));

	if (aglink_gyro_sample_init()) {
		gyro_printk(AGLINK_GYRO_DBG_ERROR, "Failed to create gyro sample task\n");
		goto fail2;
	}

	if (aglink_gyro_read_fifo_start()) {
		gyro_printk(AGLINK_GYRO_DBG_ERROR, "Failed to read gyro fifo\n");
		goto fail3;
	}

	gyro_init = 1;

	return 0;

fail3:
	aglink_gyro_sample_deinit();
fail2:
	kthread_stop(consumer_thread);
fail1:
	kfifo_free(&gyro_fifo);
fail0:
	aglink_gyro_free(gyro_storage.dat);
	return -1;
}
EXPORT_SYMBOL(aglink_gyro_sample_kick_start);

void aglink_gyro_sample_stop(void)
{
	if (!gyro_init) {
		return;
	}

	aglink_gyro_read_fifo_stop();

	aglink_gyro_sample_deinit();

	kthread_stop(consumer_thread);

	if (video_subdir.create) {
		aglink_gyro_save_data();
		if (gyro_storage.cnt)
			kernel_write(output_file, gyro_storage.dat, gyro_storage.pos, &pos);
		vfs_fsync(output_file, 0);
		filp_close(output_file, NULL);
	}

	kfifo_free(&gyro_fifo);
	aglink_gyro_free(gyro_storage.dat);
	gyro_init = 0;

	gyro_printk(AGLINK_GYRO_DBG_ALWY, "sample stop\n");
}
EXPORT_SYMBOL(aglink_gyro_sample_stop);

static ssize_t video_proc_write(struct file *file, const char __user *buf,
								size_t count, loff_t *ppos)
{
	if (count > sizeof(video_subdir.video_subdir_name_str)) {
		return -EINVAL;
	}

	memset(video_subdir.video_subdir_name_str, 0, sizeof(video_subdir.video_subdir_name_str));
	if (copy_from_user(video_subdir.video_subdir_name_str, buf, count)) {
		return -EFAULT;
	}
	video_subdir.count = count;

	if (video_subdir.count == 4 &&
		!strncmp("NULL", video_subdir.video_subdir_name_str, strlen("NULL"))) {
		aglink_gyro_sample_stop();
	}
	return count;
}

static struct file_operations video_proc_fops = {
	.write = video_proc_write,
};

int aglink_video_subdirfs_init(void)
{
	struct proc_dir_entry *entry;

	entry = proc_create("video_subdir_name", 0644, NULL, &video_proc_fops);
	if (!entry) {
		gyro_printk(AGLINK_GYRO_DBG_ERROR, "Failed to create proc file: %s\n", VIDEO_SUBDIR_NAME_POINT);
		return -ENOMEM;
	}
	return 0;
}
EXPORT_SYMBOL(aglink_video_subdirfs_init);

void aglink_video_subdirfs_deinit(void)
{
	remove_proc_entry("video_subdir_name", NULL);
}
EXPORT_SYMBOL(aglink_video_subdirfs_deinit);

