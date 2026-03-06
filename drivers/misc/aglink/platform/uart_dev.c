#include <platform.h>
#include <linux/kernel.h>
#include "debug.h"

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/rpbuf.h>
#include <linux/of_reserved_mem.h>
#include <linux/dma-mapping.h>

#define AGLINK_UART_DEV_ID          0
#define AGLINK_MAX_RPBUF_SIZE       (4 * 1024)
#define AGLINK_RX_RPBUF_BUFFER_NAME	"aglink_rx_rpbuf"
#define AGLINK_TX_RPBUF_BUFFER_NAME	"aglink_tx_rpbuf"

extern int aglink_register_dev_ops(struct aglink_bus_ops *ops, int dev_id);

struct aglink_rpbuf_dev {
	struct rpbuf_controller *controller;
	struct rpbuf_buffer *rx_buffer;
	struct rpbuf_buffer *tx_buffer;
	int buff_size;
};

struct aglink_rpbuf_dev *uart_rpbuf_dev;
static struct aglink_bus_ops uart_ops;

static int aglink_rx_rpbuf_alloc_payload_memory(struct rpbuf_buffer *buffer, void *priv)
{
	struct device *dev = priv;
	int ret;
	void *va;
	dma_addr_t pa;

	va = dma_alloc_coherent(dev, rpbuf_buffer_len(buffer), &pa, GFP_KERNEL);
	if (IS_ERR_OR_NULL(va)) {
		dev_err(dev, "dma_alloc_coherent for len %d failed\n",
			rpbuf_buffer_len(buffer));
		ret = -ENOMEM;
		goto err_out;
	}
	dev_info(dev, "allocate payload memory: va 0x%pK, pa %pad, len %d\n",
				va, &pa, rpbuf_buffer_len(buffer));
	rpbuf_buffer_set_va(buffer, va);
	rpbuf_buffer_set_pa(buffer, (phys_addr_t)pa);

	return 0;

err_out:
	return ret;
}

static int aglink_tx_rpbuf_alloc_payload_memory(struct rpbuf_buffer *buffer, void *priv)
{
	struct device *dev = priv;
	int ret;
	void *va;
	dma_addr_t pa;

	va = dma_alloc_coherent(dev, rpbuf_buffer_len(buffer), &pa, GFP_KERNEL);
	if (IS_ERR_OR_NULL(va)) {
		dev_err(dev, "dma_alloc_coherent for len %d failed\n",
			rpbuf_buffer_len(buffer));
		ret = -ENOMEM;
		goto err_out;
	}
	dev_info(dev, "allocate payload memory: va 0x%pK, pa %pad, len %d\n",
				va, &pa, rpbuf_buffer_len(buffer));
	rpbuf_buffer_set_va(buffer, va);
	rpbuf_buffer_set_pa(buffer, (phys_addr_t)pa);

	return 0;

err_out:
	return ret;
}


static void aglink_rx_rpbuf_free_payload_memory(struct rpbuf_buffer *buffer, void *priv)
{
	void *va;
	dma_addr_t pa;
	struct device *dev = priv;

	va = rpbuf_buffer_va(buffer);
	pa = (dma_addr_t)(rpbuf_buffer_pa(buffer));

	dev_info(dev, "free payload memory: va %pK, pa %pad, len %d\n",
				va, &pa, rpbuf_buffer_len(buffer));
	dma_free_coherent(dev, rpbuf_buffer_len(buffer), va, pa);
}

static void aglink_tx_rpbuf_free_payload_memory(struct rpbuf_buffer *buffer, void *priv)
{
	void *va;
	dma_addr_t pa;
	struct device *dev = priv;

	va = rpbuf_buffer_va(buffer);
	pa = (dma_addr_t)(rpbuf_buffer_pa(buffer));

	dev_info(dev, "free payload memory: va %pK, pa %pad, len %d\n",
				va, &pa, rpbuf_buffer_len(buffer));
	dma_free_coherent(dev, rpbuf_buffer_len(buffer), va, pa);
}

static const struct rpbuf_buffer_ops aglink_rx_rpbuf_ops = {
	.alloc_payload_memory = aglink_rx_rpbuf_alloc_payload_memory,
	.free_payload_memory = aglink_rx_rpbuf_free_payload_memory,
};

static const struct rpbuf_buffer_ops aglink_tx_rpbuf_ops = {
	.alloc_payload_memory = aglink_tx_rpbuf_alloc_payload_memory,
	.free_payload_memory = aglink_tx_rpbuf_free_payload_memory,
};


static void aglink_rx_rpbuf_available_cb(struct rpbuf_buffer *buffer, void *priv)
{
	plat_printk(AGLINK_DBG_ALWY, "buffer \"%s\" is available\n", rpbuf_buffer_name(buffer));
}

static void aglink_tx_rpbuf_available_cb(struct rpbuf_buffer *buffer, void *priv)
{
	plat_printk(AGLINK_DBG_ALWY, "aglink_tx buffer \"%s\" is available\n", rpbuf_buffer_name(buffer));
}

static int aglink_rx_rpbuf_rx_cb(struct rpbuf_buffer *buffer,
							void *data, int data_len, void *priv)
{
	//plat_printk(AGLINK_DBG_MSG, "rx len: %d\n", data_len);
#if 0
	int i;
	for (i = 0; i < data_len; ++i) {
		printk("0x%x,", *((u8 *)(data) + i));
	}
	printk("\n");
#endif
	if (uart_ops.rx_cb) {
		//uart_ops.rx_cb(AGLINK_UART_DEV_ID, data, data_len);
		uart_ops.rx_disorder_cb(AGLINK_UART_DEV_ID, data, data_len);
	}

	return 0;
}

static int aglink_tx_rpbuf_rx_cb(struct rpbuf_buffer *buffer,
							void *data, int data_len, void *priv)
{
	plat_printk(AGLINK_DBG_ERROR, "aglink_tx rpbuff not use rx: %d\n", data_len);

	return 0;
}

static int aglink_rx_rpbuf_destroyed_cb(struct rpbuf_buffer *buffer, void *priv)
{
	plat_printk(AGLINK_DBG_ALWY, "buffer \"%s\": remote buffer destroyed\n", rpbuf_buffer_name(buffer));

	return 0;
}

static int aglink_tx_rpbuf_destroyed_cb(struct rpbuf_buffer *buffer, void *priv)
{
	plat_printk(AGLINK_DBG_ALWY, "aglink_tx buffer \"%s\": remote buffer destroyed\n", rpbuf_buffer_name(buffer));

	return 0;
}

static const struct rpbuf_buffer_cbs aglink_rx_rpbuf_cbs = {
	.available_cb = aglink_rx_rpbuf_available_cb,
	.rx_cb = aglink_rx_rpbuf_rx_cb,
	.destroyed_cb = aglink_rx_rpbuf_destroyed_cb,
};

static const struct rpbuf_buffer_cbs aglink_tx_rpbuf_cbs = {
	.available_cb = aglink_tx_rpbuf_available_cb,
	.rx_cb = aglink_tx_rpbuf_rx_cb,
	.destroyed_cb = aglink_tx_rpbuf_destroyed_cb,
};

static int aglink_rpbuf_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *tmp_np;

	plat_printk(AGLINK_DBG_ALWY, "aglink rpbuf probe start\n");
	uart_rpbuf_dev = devm_kzalloc(dev, sizeof(struct aglink_rpbuf_dev), GFP_KERNEL);
	if (!uart_rpbuf_dev) {
		ret = -ENOMEM;
		goto err_out;
	}

	tmp_np = of_parse_phandle(np, "memory-region", 0);
	if (tmp_np) {
		ret = of_reserved_mem_device_init(dev);
		if (ret < 0) {
			dev_err(dev, "failed to get reserved memory (ret: %d)\n", ret);
			goto err_out;
		}
	}

	uart_rpbuf_dev->controller = rpbuf_get_controller_by_of_node(np, 0);
	if (!uart_rpbuf_dev->controller) {
		dev_err(dev, "cannot get rpbuf controller\n");
		ret = -ENOENT;
		goto err_free_rpbuf_dev;
	}

	if (rpbuf_wait_controller_ready(uart_rpbuf_dev->controller, 3000)) {
		plat_printk(AGLINK_DBG_ALWY, "aglink rpbuf wait controller ready timeout.\n");
		return -ENOENT;
	}

	uart_rpbuf_dev->buff_size = AGLINK_MAX_RPBUF_SIZE;

	uart_rpbuf_dev->rx_buffer = rpbuf_alloc_buffer(uart_rpbuf_dev->controller,  AGLINK_RX_RPBUF_BUFFER_NAME, AGLINK_MAX_RPBUF_SIZE,
					    &aglink_rx_rpbuf_ops,
					    &aglink_rx_rpbuf_cbs,
					    (void *)dev);
	if (!uart_rpbuf_dev->rx_buffer) {
		dev_err(dev, "rpbuf_alloc_buffer failed\n");
		return -ENOENT;
	}

	uart_rpbuf_dev->tx_buffer = rpbuf_alloc_buffer(uart_rpbuf_dev->controller, AGLINK_TX_RPBUF_BUFFER_NAME, uart_rpbuf_dev->buff_size,
					    &aglink_tx_rpbuf_ops,
					    &aglink_tx_rpbuf_cbs,
					    (void *)dev);
	if (!uart_rpbuf_dev->tx_buffer) {
		dev_err(dev, "rpbuf_alloc_buffer failed\n");
		return -ENOENT;
	}

	dev_set_drvdata(dev, uart_rpbuf_dev);

	rpbuf_buffer_set_sync(uart_rpbuf_dev->rx_buffer, true);
	rpbuf_buffer_set_sync(uart_rpbuf_dev->tx_buffer, true);
	plat_printk(AGLINK_DBG_ALWY, "aglink rpbuf probe end\n");
	return 0;

err_free_rpbuf_dev:
	devm_kfree(dev, uart_rpbuf_dev);
err_out:
	return ret;
}

static int aglink_rpbuf_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct aglink_rpbuf_dev *rpbuf_dev = dev_get_drvdata(dev);
	struct rpbuf_buffer *rx_buffer = rpbuf_dev->rx_buffer;
	struct rpbuf_buffer *tx_buffer = rpbuf_dev->tx_buffer;

	if (!uart_rpbuf_dev) {
		plat_printk(AGLINK_DBG_ERROR, "aglink_rpbuf_dev is NULL\n");
		return -1;
	}

	if (rx_buffer)
		rpbuf_free_buffer(rx_buffer);
	if (tx_buffer)
		rpbuf_free_buffer(tx_buffer);

	devm_kfree(dev, uart_rpbuf_dev);
	uart_rpbuf_dev = NULL;

	return 0;
}

static const struct of_device_id aglink_rpbuf_ids[] = {
	{ .compatible = "allwinner,rpbuf-aglink" },
	{}
};

static struct platform_driver aglink_rpbuf_driver = {
	.probe	= aglink_rpbuf_probe,
	.remove	= aglink_rpbuf_remove,
	.driver	= {
		.owner = THIS_MODULE,
		.name = "aglink_rpbuf",
		.of_match_table = aglink_rpbuf_ids,
	},
};

int aglink_rpbuf_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&aglink_rpbuf_driver);
	if (ret) {
		plat_printk(AGLINK_DBG_ERROR, "platform driver register aglink fail\n");
		return ret;
	}

	plat_printk(AGLINK_DBG_ALWY, "aglink rpbuf init\n");

	return 0;
}

void aglink_rpbuf_deinit(void)
{
	platform_driver_unregister(&aglink_rpbuf_driver);
	plat_printk(AGLINK_DBG_ALWY, "aglink rpbuf deinit\n");
}

int uart_dev_start(void)
{
	plat_printk(AGLINK_DBG_ALWY, "uart dev start.\n");
	return 0;
}

void uart_dev_stop(void)
{
	aglink_rpbuf_deinit();
	plat_printk(AGLINK_DBG_ALWY, "uart dev stop.\n");
}

int uart_dev_tx(int dev_id, unsigned char *buff, unsigned short len)
{
	int ret = 0;
	void *buf_va;
	struct rpbuf_buffer *buffer = uart_rpbuf_dev->tx_buffer;

	if (!buffer) {
		plat_printk(AGLINK_DBG_ALWY, "tx_buffer not created\n");
		return -ENOENT;
	}

	while (!rpbuf_buffer_is_available(buffer))
		msleep(1);

	plat_printk(AGLINK_DBG_MSG, "uart tx:%d\n", len);
	//data_hex_dump("uart tx", 20, buff, len);
	buf_va = rpbuf_buffer_va(buffer);

	if (uart_rpbuf_dev->buff_size < len) {
		plat_printk(AGLINK_DBG_ERROR, "buffer \"%s\": data size (%d) out of buffer length (%d)\n",
				rpbuf_buffer_name(buffer), len, uart_rpbuf_dev->buff_size);
		return -EINVAL;
	}

	if (!rpbuf_buffer_is_available(buffer)) {
		plat_printk(AGLINK_DBG_ALWY, "buffer \"%s\" not available\n", rpbuf_buffer_name(buffer));
		return -EACCES;
	}

	memcpy(buf_va, buff, len);
	ret = rpbuf_transmit_buffer(buffer, 0, len);
	if (ret < 0) {
		plat_printk(AGLINK_DBG_ERROR, "buffer \"%s\": rpbuf_transmit_buffer failed\n", rpbuf_buffer_name(buffer));
		return ret;
	}

	return ret;
}

static struct aglink_bus_ops uart_ops = {
	.start = uart_dev_start,
	.stop = uart_dev_stop,
	.tx = uart_dev_tx,
};

void uart_dev_init(void)
{
	plat_printk(AGLINK_DBG_ALWY, "register uart device, ID = %d\n", AGLINK_UART_DEV_ID);
	aglink_rpbuf_init();
	aglink_register_dev_ops(&uart_ops, AGLINK_UART_DEV_ID);
}
