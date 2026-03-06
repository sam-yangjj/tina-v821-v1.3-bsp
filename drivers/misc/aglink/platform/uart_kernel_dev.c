#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/tty.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/serial.h>
#include <linux/file.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <platform.h>

#ifndef TTY_NAME
#define TTY_NAME "ttyS2"
#endif

#ifndef UART_RATE
#define UART_RATE 150000
#endif

#define AGLINK_UART_DEV_ID  0

extern int aglink_register_dev_ops(struct aglink_bus_ops *ops, int dev_id);

static struct tty_struct *kernel_tty;
static struct aglink_bus_ops uart_kernel_ops;

const struct tty_port_client_operations *tty_port_client_ops_old;

//refer to driver/tty/n_tty.c n_tty_receive_buf_common
static int receive_buf(struct tty_struct *tty, const unsigned char *cp,
			 char *fp, int count)
{
	down_read(&tty->termios_rwsem);

	if (uart_kernel_ops.rx_cb) {
		uart_kernel_ops.rx_cb(AGLINK_UART_DEV_ID, cp, count);
	} else if (uart_kernel_ops.rx_disorder_cb) {
		uart_kernel_ops.rx_disorder_cb(AGLINK_UART_DEV_ID, cp, count);
	}

	tty->receive_room = count;

	up_read(&tty->termios_rwsem);

	return count;
}

static int tty_port_receive_buf(struct tty_port *port,
					const unsigned char *p,
					const unsigned char *f, size_t count)
{
	int ret;
	struct tty_struct *tty;
	struct tty_ldisc *disc;

	tty = READ_ONCE(port->itty);
	if (!tty)
		return 0;

	disc = tty_ldisc_ref(tty);
	if (!disc)
		return 0;

	ret = receive_buf(tty, p, f, count);

	tty_ldisc_deref(disc);

	return ret;
}

static void tty_port_wakeup(struct tty_port *port)
{
	struct tty_struct *tty = tty_port_tty_get(port);

	if (tty) {
		tty_wakeup(tty);
		tty_kref_put(tty);
	}
}

const struct tty_port_client_operations tty_port_client_ops = {
	.receive_buf = tty_port_receive_buf,
	.write_wakeup = tty_port_wakeup,
};

static int uart_kernel_start(void)
{
	dev_t dev_num;

	if (kernel_tty == NULL) {
		if (tty_dev_name_to_number(TTY_NAME, &dev_num)) {
			printk("tty dev name to number false\n");
			return -ENOENT;
		}

		kernel_tty = tty_kopen(dev_num);
		if (kernel_tty != NULL) {
			/* set baud rate */
			tty_encode_baud_rate(kernel_tty, UART_RATE, UART_RATE);

			/* refactor client_ops */
			tty_port_client_ops_old = kernel_tty->port->client_ops;
			kernel_tty->port->client_ops = &tty_port_client_ops;

			if (kernel_tty->ops->open) {
				kernel_tty->ops->open(kernel_tty, NULL);
			}

			printk("tty name: %s\n", tty_name(kernel_tty));
			printk("tty rate: %d\n", tty_get_baud_rate(kernel_tty));
		} else {
			printk("tty kopen false\n");
			return -ENOENT;
		}
	} else {
		printk("tty Kernel uart already start\n");
	}

	return 0;
}

static void uart_kernel_stop(void)
{
	int send_count = 0;
	if (kernel_tty != NULL) {
		if (kernel_tty->ops->close) {
			kernel_tty->ops->close(kernel_tty, NULL);
		}

		/* refactor client_ops to old */
		kernel_tty->port->client_ops = tty_port_client_ops_old;

		tty_kclose(kernel_tty);
		kernel_tty = NULL;
	} else {
		printk("Kernel uart already stop\n");
	}
	return;
}

static int uart_kernel_tx(int dev_id, u8 *buff, u16 len)
{
	int send_count = 0;
	if ((kernel_tty != NULL) && (kernel_tty->ops->write)) {
		send_count = kernel_tty->ops->write(kernel_tty, buff, len);
		printk("kernel uart tx send count:%d\n", send_count);
	} else {
		printk("kernel uart tx already stop\n");
	}

	return send_count;
}

static struct aglink_bus_ops uart_kernel_ops = {
	.start = uart_kernel_start,
	.stop = uart_kernel_stop,
	.tx = uart_kernel_tx,
};

void uart_kernel_dev_init(void)
{
	kernel_tty = NULL;
	printk("register uart Kernel device, ID = %d\n", AGLINK_UART_DEV_ID);
	aglink_register_dev_ops(&uart_kernel_ops, AGLINK_UART_DEV_ID);
}
