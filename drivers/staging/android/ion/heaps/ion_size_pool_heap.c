// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/spinlock.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/genalloc.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/of.h>
#include <linux/dma-contiguous.h>
#include <linux/cma.h>
#include <linux/debugfs.h>
#include <linux/bitmap.h>
#include <linux/ion.h>
#include <linux/sched/clock.h>
#include <asm/stacktrace.h>
#include <linux/kallsyms.h>
#include <../drivers/staging/android/ion/ion_private.h>
#include <linux/mm/size_pool.h>

#define ION_SIZE_POOL_ALLOCATE_FAIL -1

enum SMALL_ALLOC_SOURCE_E {
	SMALL_ALLOC_SOURCE_CMA,
	SMALL_ALLOC_SOURCE_SELF,
};

struct alloc_log {
	u64 time_nesc;
	phys_addr_t addr;
	size_t size;
	union {
		u32 flag;
		struct {
			uint32_t dir : 1; /* 1 alloc, 0 free */
			uint32_t source : 3;
		};
	};
};

#define ALLOC_FREE_LOG_CNT 32
struct ion_size_pool_heap {
	struct ion_heap heap;
	struct gen_pool **pool;
	phys_addr_t base;
	uint32_t fall_to_big_pool;
	uint32_t *thrs;
	uint32_t *sizes;
	uint32_t pool_cnt;
	struct dentry *debug_f_root;
	struct alloc_log log[ALLOC_FREE_LOG_CNT];
	int log_idx;
	enum SMALL_ALLOC_SOURCE_E source;
	struct list_head buffers;
	struct mutex buffer_lock;
	uint32_t caller_record_depth;
	struct device *ctrl_dev;
	struct list_head earlyboot_bufs;
};
struct list_head *earlyboot_bufs;
struct device *ctrl_dev;

struct size_pool_early_boot_buffer {
	char *name;
	uint32_t addr;
	uint32_t size;
	struct list_head list;
	struct gen_pool *pool;
};
enum stack_type { STACK_ALLOC, STACK_MAP };
struct stack_info {
	unsigned long *caller;
	struct {
		int max_depth : 16;
		int caller_cnt : 16;
	};
	enum stack_type type;
	struct list_head node;
};
struct size_pool_buffer_priv {
	enum SMALL_ALLOC_SOURCE_E source;
	struct list_head list;
	struct ion_buffer *buf;
	struct list_head stacks;
};

static struct class *ctrl_cls;

struct walk_stack_param {
	struct stack_info *data;
	int max_depth;
};
static bool record_trace_address(void *arg, unsigned long pc)
{
	struct walk_stack_param *param = arg;
	param->data->caller[param->data->caller_cnt] = pc;
	param->data->caller_cnt++;
	return param->data->caller_cnt >= param->max_depth;
}

struct mutex working_param_lock;
static struct gen_pool *pool_for_alloc_size(struct ion_size_pool_heap *heap,
					    unsigned long size)
{
	int i;
	for (i = 0; i < heap->pool_cnt - 1; i++) {
		if (size <= heap->thrs[i])
			break;
	}
	return heap->pool[i];
}

static struct stack_info *__inster_stack_info(struct list_head *head, enum stack_type type,
				int max_depth)
{
	struct stack_info *alloc_stack;
	alloc_stack = kzalloc(sizeof(*alloc_stack), GFP_KERNEL);
	if (alloc_stack)
		alloc_stack->caller = kcalloc(max_depth, sizeof(void *),
					      GFP_KERNEL | __GFP_ZERO);
	if (!alloc_stack || !alloc_stack->caller) {
		if (alloc_stack)
			kfree(alloc_stack);
		pr_err("%pS no space for caller record\n",
		       __builtin_return_address(0));
		return NULL;
	} else {
		struct walk_stack_param param = { .data = alloc_stack,
						  .max_depth = max_depth };
		alloc_stack->max_depth = max_depth;
		alloc_stack->caller_cnt = 0;
		alloc_stack->type = type;
		walk_stackframe(NULL, NULL, record_trace_address, &param);
		list_add(&alloc_stack->node, head);
		return alloc_stack;
	}
}

unsigned long gen_pool_last_fit(unsigned long *map, unsigned long size,
				unsigned long start, unsigned int nr,
				void *data, struct gen_pool *pool,
				unsigned long start_addr);
static phys_addr_t ion_size_pool_allocate(struct ion_heap *heap,
					  unsigned long size,
					  struct size_pool_buffer_priv *data)
{
	struct ion_size_pool_heap *size_pool_heap =
		container_of(heap, struct ion_size_pool_heap, heap);
	unsigned long offset = 0;
	int fall_to_big_pool;
	struct alloc_log *log;
	data->source = SMALL_ALLOC_SOURCE_SELF;

	mutex_lock(&working_param_lock);
	fall_to_big_pool = size_pool_heap->fall_to_big_pool;
	mutex_unlock(&working_param_lock);
	if (size <= size_pool_heap->thrs[0] &&
	    size_pool_heap->source == SMALL_ALLOC_SOURCE_CMA) {
		struct page *page =
			cma_alloc(dev_get_cma_area(NULL),
				  PAGE_ALIGN(size) / PAGE_SIZE, 0, false);
		if (page) {
			data->source = SMALL_ALLOC_SOURCE_CMA;
			offset = page_to_phys(page);
		}
	} else {
		offset = gen_pool_alloc(
			pool_for_alloc_size(size_pool_heap, size), size);
	}

	if (!offset) {
		if (fall_to_big_pool) {
			offset = gen_pool_alloc_algo(
				size_pool_heap
					->pool[size_pool_heap->pool_cnt - 1],
				size, gen_pool_last_fit, NULL);
		}
		if (!offset)
			return ION_SIZE_POOL_ALLOCATE_FAIL;
	}

	INIT_LIST_HEAD(&data->stacks);
	if (size_pool_heap->caller_record_depth) {
		__inster_stack_info(&data->stacks, STACK_ALLOC,
				    size_pool_heap->caller_record_depth);
	}

	mutex_lock(&size_pool_heap->buffer_lock);
	list_add(&data->list, &size_pool_heap->buffers);
	mutex_unlock(&size_pool_heap->buffer_lock);

	log = &size_pool_heap->log[size_pool_heap->log_idx];
	log->time_nesc = local_clock();
	log->addr = offset;
	log->size = size;
	log->dir = 1;
	log->source = data->source;
	size_pool_heap->log_idx =
		(size_pool_heap->log_idx + 1) % ARRAY_SIZE(size_pool_heap->log);

	return offset;
}

static void ion_size_pool_free(struct ion_heap *heap, phys_addr_t addr,
			       unsigned long size,
			       struct size_pool_buffer_priv *priv)
{
	struct ion_size_pool_heap *size_pool_heap =
		container_of(heap, struct ion_size_pool_heap, heap);
	int i;
	struct alloc_log *log;
	struct stack_info *info, *n;

	if (addr == ION_SIZE_POOL_ALLOCATE_FAIL)
		return;

	log = &size_pool_heap->log[size_pool_heap->log_idx];
	log->time_nesc = local_clock();
	log->addr = addr;
	log->size = size;
	log->dir = 0;
	log->source = priv->source;
	size_pool_heap->log_idx =
		(size_pool_heap->log_idx + 1) % ARRAY_SIZE(size_pool_heap->log);

	list_for_each_entry_safe (info, n, &priv->stacks, node) {
		if (info->caller)
			kfree(info->caller);
		if (info->type == STACK_MAP) {
			int i;
			pr_err("free without unmap, map stack:\n");
			for (i = 0; i < info->caller_cnt; i++) {
				print_ip_sym(info->caller[i]);
			}
		}
		list_del(&info->node);
		kfree(info);
	}

	if (priv->source == SMALL_ALLOC_SOURCE_CMA) {
		struct page *page = phys_to_page(addr);
		cma_release(dev_get_cma_area(NULL), page,
			    PAGE_ALIGN(size) / PAGE_SIZE);

		goto released;
	}

	for (i = 0; i < size_pool_heap->pool_cnt; i++) {
		if (!size_pool_heap->pool[i])
			continue;
		if (addr_in_gen_pool(size_pool_heap->pool[i], addr, size))
			gen_pool_free(size_pool_heap->pool[i], addr, size);
	}

released:
	mutex_lock(&size_pool_heap->buffer_lock);
	list_del(&priv->list);
	mutex_unlock(&size_pool_heap->buffer_lock);
}

static int ion_size_pool_heap_allocate(struct ion_heap *heap,
				       struct ion_buffer *buffer,
				       unsigned long size, unsigned long flags)
{
	struct sg_table *table;
	phys_addr_t paddr;
	int ret;
	struct size_pool_buffer_priv *priv;

	table = kmalloc(sizeof(*table), GFP_KERNEL);
	if (!table)
		return -ENOMEM;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		goto err_free;

	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret)
		goto err_priv;

	paddr = ion_size_pool_allocate(heap, size, priv);
	if (paddr == ION_SIZE_POOL_ALLOCATE_FAIL) {
		ret = -ENOMEM;
		goto err_free_table;
	}

	sg_set_page(table->sgl, pfn_to_page(PFN_DOWN(paddr)), size, 0);
	buffer->sg_table = table;
	buffer->priv_virt = priv;
	priv->buf = buffer;

	return 0;

err_free_table:
	sg_free_table(table);
err_priv:
	kfree(priv);
err_free:
	kfree(table);
	return ret;
}

static void ion_size_pool_heap_free(struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;
	struct sg_table *table = buffer->sg_table;
	struct page *page = sg_page(table->sgl);
	phys_addr_t paddr = PFN_PHYS(page_to_pfn(page));

	ion_buffer_zero(buffer);

	ion_size_pool_free(heap, paddr, buffer->size, buffer->priv_virt);
	kfree(buffer->priv_virt);
	sg_free_table(table);
	kfree(table);
}

static struct ion_heap_ops size_pool_heap_ops = {
	.allocate = ion_size_pool_heap_allocate,
	.free = ion_size_pool_heap_free,
};


static struct sg_table *size_pool_map_dma_buf(struct dma_buf_attachment *attachment,
					enum dma_data_direction direction)
{
	struct ion_buffer *buffer = attachment->dmabuf->priv;
	struct ion_heap *heap = buffer->heap;
	struct ion_dma_buf_attachment *a;
	struct sg_table *table;
	unsigned long attrs = attachment->dma_map_attrs;
	struct ion_size_pool_heap *size_pool_heap =
		container_of(heap, struct ion_size_pool_heap, heap);
	struct size_pool_buffer_priv *priv = buffer->priv_virt;

	a = attachment->priv;
	table = a->table;

	if (size_pool_heap->caller_record_depth) {
		struct stack_info *info = __inster_stack_info(
			&priv->stacks, STACK_MAP,
			size_pool_heap->caller_record_depth + 1);
		if (info) {
			/* last one slot for release handler */
			info->caller[info->max_depth - 1] = (unsigned long)a;
			if (info->caller_cnt == info->max_depth)
				info->caller_cnt -= 1;
		}
	}

	if (!(buffer->flags & ION_FLAG_CACHED))
		attrs |= DMA_ATTR_SKIP_CPU_SYNC;

	if (!dma_map_sg_attrs(attachment->dev, table->sgl, table->nents,
			      direction, attrs))
		return ERR_PTR(-ENOMEM);

	a->mapped = true;

	return table;
}

static void size_pool_unmap_dma_buf(struct dma_buf_attachment *attachment,
			      struct sg_table *table,
			      enum dma_data_direction direction)
{
	struct ion_buffer *buffer = attachment->dmabuf->priv;
	struct ion_dma_buf_attachment *a = attachment->priv;
	unsigned long attrs = attachment->dma_map_attrs;
	struct size_pool_buffer_priv *priv = buffer->priv_virt;
	struct stack_info *info, *n;

	list_for_each_entry_safe (info, n, &priv->stacks, node) {
		if (info->type != STACK_MAP)
			continue;
		if (info->caller[info->max_depth - 1] == (unsigned long)a)
			list_del(&info->node);
	}

	a->mapped = false;

	if (!(buffer->flags & ION_FLAG_CACHED))
		attrs |= DMA_ATTR_SKIP_CPU_SYNC;

	dma_unmap_sg_attrs(attachment->dev, table->sgl, table->nents,
			   direction, attrs);
}

static phys_addr_t start_addr_of_addr(phys_addr_t test_paddr,
				      struct ion_heap *heap)
{
	static struct ion_buffer *last;
	struct list_head *buf_lh;
	struct ion_size_pool_heap *size_pool_heap =
		container_of(heap, struct ion_size_pool_heap, heap);
	if (last) {
		phys_addr_t paddr = page_to_phys(sg_page(last->sg_table->sgl));
		if (paddr <= test_paddr && test_paddr < paddr + last->size) {
			return paddr;
		} else {
			last = NULL;
		}
	}

	list_for_each (buf_lh, &size_pool_heap->buffers) {
		struct sg_table *table;
		struct page *page;
		phys_addr_t paddr;
		struct ion_buffer *buffer =
			container_of(buf_lh, struct size_pool_buffer_priv, list)
				->buf;
		if (buffer->heap->id != heap->id)
			continue;
		table = buffer->sg_table;
		page = sg_page(table->sgl);
		paddr = PFN_PHYS(page_to_pfn(page));
		if (paddr <= test_paddr && test_paddr < paddr + buffer->size) {
			last = buffer;
			return paddr;
		}
	}
	return 0;
}

static int ion_size_pool_statistics_show(struct seq_file *s, void *unused)
{
	int idx;
	struct ion_heap *heap = s->private;
	struct ion_size_pool_heap *size_pool_heap =
		container_of(heap, struct ion_size_pool_heap, heap);
	struct gen_pool *pool = NULL;
	int ld, hi, size, avail;
	const char *type;
	const char *types[] = { "cma", "in_heap" };
	for (idx = 0; idx < size_pool_heap->pool_cnt; idx++) {
		pool = size_pool_heap->pool[idx];
		if (!pool) {
			ld = 0;
			hi = size_pool_heap->thrs[idx] / 1024;
			size = totalcma_pages * 4;
			avail = global_zone_page_state(NR_FREE_CMA_PAGES) * 4;
			type = types[0];
		} else {
			ld = idx == 0 ? 0 :
					size_pool_heap->thrs[idx - 1] / 1024;
			hi = idx < size_pool_heap->pool_cnt - 1 ?
				     size_pool_heap->thrs[idx] :
				     gen_pool_size(pool) / 1024;
			size = gen_pool_size(pool) / 1024;
			avail = gen_pool_avail(pool) / 1024;
			type = types[1];
		}
		seq_printf(
			s,
			" pool for size %dKB~%dKB area Total:%uKB Free:%uKB from %s\n",
			ld, hi, size, avail, type);
	}
	return 0;
}

static int ion_size_pool_debug_show(struct seq_file *s, void *unused)
{
	struct gen_pool *pool = NULL;
	struct ion_heap *heap = s->private;
	struct gen_pool_chunk *chunk;
	int size, total_bits, bits_per_unit;
	int i, tmp, busy;
	int busy_cnt = 0, free_cnt = 0, total_cnt;
	unsigned int dump_unit = SZ_4K;
	int idx;
	phys_addr_t last_paddr;
	char icon[] = { '-', '*', '/', '#' };
	int icon_idx;
	char busychar;

	struct ion_size_pool_heap *size_pool_heap =
		container_of(heap, struct ion_size_pool_heap, heap);

	mutex_lock(&size_pool_heap->buffer_lock);
	for (idx = 0; idx < size_pool_heap->pool_cnt; idx++) {
		pool = size_pool_heap->pool[idx];
		if (!pool)
			continue;

		last_paddr = 0;
		icon_idx = 0;
		rcu_read_lock();
		list_for_each_entry_rcu (chunk, &pool->chunks, next_chunk) {
			size = ALIGN(chunk->end_addr - chunk->start_addr,
				     1 << pool->min_alloc_order);
			total_bits = size >> pool->min_alloc_order;
			bits_per_unit = dump_unit >> pool->min_alloc_order;
			seq_printf(
				s,
				" size_pool memory layout(+: free, -*/#: busy, unit: %dKB):\n",
				dump_unit / 1024);
			total_cnt = (chunk->end_addr + 1 - chunk->start_addr) /
				    dump_unit;
			busy_cnt = 0;
			free_cnt = 0;
			for (i = 0, tmp = 0, busy = 0; i < total_bits; i++) {
				phys_addr_t bit_addr =
					chunk->start_addr +
					(i << pool->min_alloc_order);
				if ((i & ((bits_per_unit * 64) - 1)) == 0) {
					if (i)
						seq_printf(s, "\n");
					seq_printf(s, "%pad:", &bit_addr);
				}
				if (last_paddr !=
				    start_addr_of_addr(bit_addr, heap)) {
					last_paddr = start_addr_of_addr(
						bit_addr, heap);
					if (last_paddr != 0) {
						icon_idx = (icon_idx + 1) %
							   ARRAY_SIZE(icon);
						busychar = icon[icon_idx];
					}
				}
				if (last_paddr)
					busy = 1;
				else if ((chunk->bits[i >> 5]) &
					 (1 << (i % 32))) {
					busy = 1;
					busychar = 'o';
				}
				if (++tmp == bits_per_unit) {
					busy ? (seq_printf(s, "%c", busychar),
						busy_cnt++) :
					       (seq_printf(s, "+"), free_cnt);
					busy = 0;
					tmp = 0;
				}
			}

			free_cnt = total_cnt - busy_cnt;
			seq_printf(s, "\n pool area start:0x%lx end:0x%lx\n",
				   chunk->start_addr, chunk->end_addr + 1);
			seq_printf(s,
				   " pool area Total:%uMB Free:%uKB ~= %uMB\n",
				   total_cnt * dump_unit / (1024 * 1024),
				   free_cnt * dump_unit / 1024,
				   free_cnt * dump_unit / (1024 * 1024));
		}
		rcu_read_unlock();
	}
	mutex_unlock(&size_pool_heap->buffer_lock);
	return 0;
}

static int read_settings(struct device *dev, struct ion_size_pool_heap *heap)
{
	const char *source_str;
	int thr_cnt, region_cnt;
	int i;
	if (of_property_read_string(dev->of_node, "small_source", &source_str) <
	    0) {
		dev_warn(dev,
			 "no small_source configurated, use cma as default");
		heap->source = SMALL_ALLOC_SOURCE_CMA;
	} else {
		if (!strncmp(source_str, "cma", strlen("cma"))) {
			heap->source = SMALL_ALLOC_SOURCE_CMA;
		} else if (!strncmp(source_str, "self", strlen("self"))) {
			heap->source = SMALL_ALLOC_SOURCE_SELF;
		}
	}

	thr_cnt = of_property_count_u32_elems(dev->of_node, "thrs");
	region_cnt = of_property_count_u32_elems(dev->of_node, "sizes");
	if (region_cnt < 0 || thr_cnt < 0) {
		dev_err(dev, "no pool setting founds");
		return -1;
	}
	if (region_cnt != thr_cnt + 1) {
		dev_err(dev, "size count %d not equal thr count %d + 1",
			region_cnt, thr_cnt);
		return -1;
	}
	heap->thrs = devm_kcalloc(dev, thr_cnt, sizeof(u32), GFP_KERNEL);
	heap->sizes =
		devm_kcalloc(dev, region_cnt, sizeof(uint32_t), GFP_KERNEL);
	if (!heap->thrs || !heap->sizes) {
		return -ENOMEM;
	}
	heap->pool_cnt = region_cnt;
	of_property_read_u32_array(dev->of_node, "thrs", heap->thrs, thr_cnt);
	of_property_read_u32_array(dev->of_node, "sizes", heap->sizes,
				   region_cnt);
	dev_dbg(dev, "pools:\n");
	for (i = 0; i < thr_cnt; i++) {
		heap->thrs[i] *= 1024;
		heap->sizes[i] *= 1024;
		pr_debug("%d %d \n", heap->thrs[i], heap->sizes[i]);
	}
	heap->sizes[i] *= 1024;
	pr_debug("others %d \n", heap->sizes[i]);

	if (of_property_read_u32(dev->of_node, "fall_to_big_pool",
				 &heap->fall_to_big_pool)) {
		dev_warn(dev,
			 "no fall_to_big_pool configurated, disabled default");
		heap->fall_to_big_pool = 0;
	}

	if (of_property_read_u32(dev->of_node, "caller_record_depth",
				 &heap->caller_record_depth)) {
		heap->caller_record_depth = 0;
	}

	return 0;
}

struct import_data {
	struct size_pool_early_boot_buffer *search;
	struct ion_size_pool_heap *heap;
};
static void import_chunk_walk(struct gen_pool *pool,
			      struct gen_pool_chunk *chunk, void *data)
{
	struct import_data *args = (struct import_data *)data;
	struct size_pool_early_boot_buffer *search = args->search;
	struct list_head *head = &args->heap->earlyboot_bufs;
	uint32_t start = search->addr, end = search->addr + search->size;
	int index, offset, start_bit, bits;
	if (!list_empty(&search->list))
		return;

	if (start >= chunk->start_addr && start <= chunk->end_addr) {
		if (end <= chunk->end_addr + 1) {
			list_add(&search->list, head);
			search->pool = pool;
			start_bit = (start - chunk->start_addr) >>
				    pool->min_alloc_order;
			bits = search->size >> pool->min_alloc_order;
			index = start_bit >> 5;
			offset = start_bit % 31;
			while (bits) {
				if (bits < 32 - offset) {
					chunk->bits[index] |=
						(((1 << bits) - 1) << offset);
					bits -= bits;
				} else {
					bits -= 32;
					chunk->bits[index] |= 0xFFFFFFFF;
				}
				index++;
				offset = 0;
			}
		}
	}
}

static int import_earlyboot_buffers(struct device *dev,
				    struct ion_size_pool_heap *heap)
{
	struct device_node *node;
	int cnt;
	uint32_t addr, size;
	int i;

	for_each_available_child_of_node (dev->of_node, node) {
		pr_debug("%s %d\n", node->name,
			 of_property_read_bool(node, "early_boot_buf"));
		if (of_property_read_bool(node, "early_boot_buf")) {
			struct import_data input;
			struct size_pool_early_boot_buffer *data;

			cnt = of_property_count_u32_elems(node, "reg");
			if (cnt != 2)
				continue;
			if (of_property_read_u32_index(node, "reg", 0, &addr) ||
			    of_property_read_u32_index(node, "reg", 1, &size)) {
				continue;
			}
			data = kzalloc(
				sizeof(struct size_pool_early_boot_buffer),
				GFP_KERNEL);
			if (!data) {
				pr_err("not enought memory for import buffer meta\n");
				continue;
			}
			data->name = kzalloc(16, GFP_KERNEL);
			if (!data->name) {
				pr_err("not enought memory for import buffer meta\n");
				kfree(data);
				continue;
			}
			strncpy(data->name, node->name, 16);

			INIT_LIST_HEAD(&data->list);
			data->addr = addr;
			data->size = size;
			input.heap = heap;
			input.search = data;
			for (i = 0; i < heap->pool_cnt; i++) {
				if (heap->pool[i])
					gen_pool_for_each_chunk(
						heap->pool[i],
						import_chunk_walk, &input);
				if (!list_empty(&data->list))
					break;
			}
			if (list_empty(&data->list)) {
				pr_err("buffer 0x%x~0x%x not in any pool", addr,
				       addr + size);
				kfree(data->name);
				kfree(data);
			}
#ifdef CONFIG_DEBUG
			struct list_head *p;
			pr_debug("--\n");
			pr_debug("%px %px %px\n", &heap->earlyboot_bufs,
				 heap->earlyboot_bufs.next,
				 heap->earlyboot_bufs.prev);
			list_for_each (p, &heap->earlyboot_bufs) {
				struct size_pool_early_boot_buffer *pool_buf =
					list_entry(
						p,
						struct size_pool_early_boot_buffer,
						list);
				pr_debug("%s 0x%x~0x%x\t", pool_buf->name,
					 pool_buf->addr,
					 pool_buf->addr + pool_buf->size);
			}
			pr_debug("==\n");
#endif
		}
	}
	return 0;
}

static void early_boot_release(struct size_pool_early_boot_buffer *pool_buf)
{
	gen_pool_free(pool_buf->pool, pool_buf->addr, pool_buf->size);
	list_del(&pool_buf->list);
	kfree(pool_buf->name);
	kfree(pool_buf);
}
int size_pool_release_early_boot_buf_name(const char *name)
{
	struct list_head *buf, *buf1;
	struct size_pool_early_boot_buffer *pool_buf;
	list_for_each_safe (buf, buf1, earlyboot_bufs) {
		pool_buf = list_entry(buf, struct size_pool_early_boot_buffer,
				      list);
		if (strlen(name) == strlen(pool_buf->name) &&
		    !strcmp(name, pool_buf->name)) {
			early_boot_release(pool_buf);
			return 0;
		}
	}
	pr_err("no early boot buffer starts from 0x%s\n", name);
	return -EINVAL;
}
int size_pool_release_early_boot_buf(uint32_t addr)
{
	struct list_head *buf, *buf1;
	struct size_pool_early_boot_buffer *pool_buf;
	list_for_each_safe (buf, buf1, earlyboot_bufs) {
		pool_buf = list_entry(buf, struct size_pool_early_boot_buffer,
				      list);
		if (pool_buf->addr == addr) {
			early_boot_release(pool_buf);
			return 0;
		}
	}
	pr_err("no early boot buffer starts from 0x%x\n", addr);
	return -EINVAL;
}
static ssize_t early_boot_release_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct list_head *buf2, *buf1;
	struct size_pool_early_boot_buffer *pool_buf;

	list_for_each_safe (buf2, buf1, earlyboot_bufs) {
		pool_buf = list_entry(buf2, struct size_pool_early_boot_buffer,
				      list);
		pr_debug("%s %s %d %d\n", pool_buf->name, buf,
			 strnlen(buf, count - 1) == strlen(pool_buf->name),
			 strncmp(buf, pool_buf->name, count - 1));
		if (strnlen(buf, count - 1) == strlen(pool_buf->name) &&
		    !strncmp(buf, pool_buf->name, count - 1)) {
			early_boot_release(pool_buf);
		}
	}

	return count;
}
static int early_boot_release_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct list_head *buf2, *buf1;
	struct size_pool_early_boot_buffer *pool_buf;
	int len = 0;
	list_for_each_safe (buf2, buf1, earlyboot_bufs) {
		pool_buf = list_entry(buf2, struct size_pool_early_boot_buffer,
				      list);
		len += sprintf(buf + len, "%s 0x%x~0x%x\n", pool_buf->name,
			       pool_buf->addr, pool_buf->addr + pool_buf->size);
	}
	return len;
}
static DEVICE_ATTR_RW(early_boot_release);

struct ion_platform_heap {
	phys_addr_t base;
	size_t size;
	void *priv;
};
static const struct file_operations fall_to_big_pool;
static const struct file_operations log_show_fops;
static const struct file_operations debug_show_fops;
static const struct file_operations statistics_show_fops;
static const struct file_operations caller_record_depth;
static const struct file_operations callers_show_fops;

int reserved_release_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	int idx;
	int len = 0;
	struct gen_pool *pool = NULL;
	struct gen_pool_chunk *chunk;
	struct ion_size_pool_heap *size_pool_heap = dev_get_drvdata(dev);
	int any_inuse = 0;
	uint32_t start = ~0, end = 0;
	mutex_lock(&size_pool_heap->buffer_lock);
	for (idx = 0; idx < size_pool_heap->pool_cnt; idx++) {
		pool = size_pool_heap->pool[idx];
		if (!pool)
			continue;
		if (gen_pool_avail(pool) != gen_pool_size(pool)) {
			len += sprintf(buf + len, "pool %d not empty\n", idx);
			any_inuse++;
		}
		list_for_each_entry_rcu (chunk, &pool->chunks, next_chunk) {
			if (start > chunk->start_addr)
				start = chunk->start_addr;
			if (end < chunk->end_addr)
				end = chunk->end_addr;
		}
	}
	mutex_unlock(&size_pool_heap->buffer_lock);
	if (any_inuse)
		len += sprintf(buf + len, "abort\n");
	else {
		phys_addr_t paddr = start;
		unsigned long tmp;
		for (idx = 0; idx < size_pool_heap->pool_cnt; idx++) {
			pool = size_pool_heap->pool[idx];
			if (!pool)
				continue;
			tmp = gen_pool_alloc(pool, gen_pool_size(pool));
			if (!tmp)
				len += sprintf(
					buf + len,
					"failed to fill up pool to disable next allocation\n");
		}
		for (paddr = start; paddr < end; paddr += PAGE_SIZE) {
			free_reserved_page(phys_to_page(paddr));
		}
		len += sprintf(buf + len, "release %x to %x\n", start, end);
	}

	return len;
}
static DEVICE_ATTR_RO(reserved_release);
static struct attribute *ctrl_attributes[] = {
	&dev_attr_reserved_release.attr,
	&dev_attr_early_boot_release.attr,
	NULL
};
const static struct attribute_group ctrl_attr_group = {
	.attrs = ctrl_attributes,
};

struct ion_heap *ion_size_pool_heap_create(struct ion_platform_heap *heap_data)
{
	struct ion_size_pool_heap *size_pool_heap;
	int ret;

	struct page *page;
	size_t size;
	struct device *dev;
	int i;
	size_t sub_pool_offset;

	page = pfn_to_page(PFN_DOWN(heap_data->base));
	size = heap_data->size;

	size_pool_heap = kzalloc(sizeof(*size_pool_heap), GFP_KERNEL);
	if (!size_pool_heap) {
		return ERR_PTR(-ENOMEM);
	}

	dev = heap_data->priv;
	size_pool_heap->ctrl_dev =
		device_create(ctrl_cls, dev, MKDEV(0, 0), NULL, "size_pool");
	if (!size_pool_heap->ctrl_dev)
		goto heap;
	ret = sysfs_create_group(&size_pool_heap->ctrl_dev->kobj,
				 &ctrl_attr_group);
	if (ret) {
		kobject_put(&size_pool_heap->ctrl_dev->kobj);
		goto ctrl_dev;
	}
	dev_set_drvdata(size_pool_heap->ctrl_dev, size_pool_heap);

	ret = read_settings(heap_data->priv, size_pool_heap);

	size_pool_heap->pool =
		devm_kcalloc(dev, size_pool_heap->pool_cnt,
			     sizeof(*size_pool_heap->pool[0]), GFP_KERNEL);
	if (!size_pool_heap->pool) {
		goto ctrl_dev;
	}
	sub_pool_offset = 0;
	size_pool_heap->base = heap_data->base;
	for (i = 0; i < size_pool_heap->pool_cnt; i++) {
		uint32_t pool_size = size_pool_heap->sizes[i];
		if (i == 0 && pool_size == 0) {
			/*
			 * cma for small pool[0] size could be zero
			 * levae a dummy pool to help looping later easier
			 */
			size_pool_heap->pool[i] = NULL;
			continue;
		}
		dev_dbg(dev, "pool[0]:%d@%d of %zu\n", pool_size,
			sub_pool_offset, heap_data->size);
		if (sub_pool_offset + pool_size > heap_data->size)
			goto pools;
		size_pool_heap->pool[i] = gen_pool_create(PAGE_SHIFT, -1);
		if (!size_pool_heap->pool[i]) {
			goto pools;
		}
		gen_pool_add(size_pool_heap->pool[i],
			     size_pool_heap->base + sub_pool_offset, pool_size,
			     -1);
		sub_pool_offset += pool_size;
	}
	INIT_LIST_HEAD(&size_pool_heap->earlyboot_bufs);
	earlyboot_bufs = &size_pool_heap->earlyboot_bufs;
	import_earlyboot_buffers(dev, size_pool_heap);
	ctrl_dev = size_pool_heap->ctrl_dev;

	size_pool_heap->heap.ops = &size_pool_heap_ops;
	size_pool_heap->heap.buf_ops.map_dma_buf = size_pool_map_dma_buf;
	size_pool_heap->heap.buf_ops.unmap_dma_buf = size_pool_unmap_dma_buf;
	size_pool_heap->heap.type = ION_HEAP_TYPE_CUSTOM;
	mutex_init(&working_param_lock);
	size_pool_heap->debug_f_root = debugfs_create_dir("size_pool", NULL);
	INIT_LIST_HEAD(&size_pool_heap->buffers);
	mutex_init(&size_pool_heap->buffer_lock);
	if (size_pool_heap->debug_f_root) {
		debugfs_create_file("fall_to_big_pool", 0644,
				    size_pool_heap->debug_f_root,
				    size_pool_heap, &fall_to_big_pool);
		debugfs_create_file("logs", 0644, size_pool_heap->debug_f_root,
				    size_pool_heap, &log_show_fops);
		debugfs_create_file("layout", 0644,
				    size_pool_heap->debug_f_root,
				    size_pool_heap, &debug_show_fops);
		debugfs_create_file("statistics", 0644,
				    size_pool_heap->debug_f_root,
				    size_pool_heap, &statistics_show_fops);
		debugfs_create_file("buf_callers", 0644,
				    size_pool_heap->debug_f_root,
				    size_pool_heap, &callers_show_fops);
		debugfs_create_file("caller_record_depth", 0644,
				    size_pool_heap->debug_f_root,
				    size_pool_heap, &caller_record_depth);
	}
	size_pool_heap->log_idx = 0;

	return &size_pool_heap->heap;
pools:
	for (; i > 0; i--) {
		if (size_pool_heap->pool[i - 1])
			gen_pool_destroy(size_pool_heap->pool[i - 1]);
	}
ctrl_dev:
	device_unregister(size_pool_heap->ctrl_dev);
heap:
	kfree(size_pool_heap);
	return ERR_PTR(-ENOMEM);
}

void ion_size_pool_heap_destroy(struct ion_heap *heap)
{
	struct ion_size_pool_heap *size_pool_heap =
		container_of(heap, struct ion_size_pool_heap, heap);

	int i;

	if (size_pool_heap->debug_f_root) {
		debugfs_remove_recursive(size_pool_heap->debug_f_root);
	}
	for (i = size_pool_heap->pool_cnt; i > 0; i--) {
		if (size_pool_heap->pool[i - 1])
			gen_pool_destroy(size_pool_heap->pool[i - 1]);
	}
	device_unregister(size_pool_heap->ctrl_dev);
	kfree(size_pool_heap);
	size_pool_heap = NULL;
}

static int fall_back_set(void *data, u64 val)
{
	struct ion_size_pool_heap *heap = data;

	mutex_lock(&working_param_lock);
	heap->fall_to_big_pool = (val != 0);
	mutex_unlock(&working_param_lock);
	return 0;
}

static int fall_back_get(void *data, u64 *val)
{
	struct ion_size_pool_heap *heap = data;

	mutex_lock(&working_param_lock);
	*val = heap->fall_to_big_pool;
	mutex_unlock(&working_param_lock);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(fall_to_big_pool, fall_back_get, fall_back_set,
			"%llu\n");

static int caller_depth_set(void *data, u64 val)
{
	struct ion_size_pool_heap *heap = data;

	mutex_lock(&working_param_lock);
	heap->caller_record_depth = val;
	mutex_unlock(&working_param_lock);
	return 0;
}

static int caller_depth_get(void *data, u64 *val)
{
	struct ion_size_pool_heap *heap = data;

	mutex_lock(&working_param_lock);
	*val = heap->caller_record_depth;
	mutex_unlock(&working_param_lock);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(caller_record_depth, caller_depth_get, caller_depth_set,
			"%llu\n");

static int log_show(struct seq_file *s, void *unused)
{
	struct ion_size_pool_heap *heap = s->private;
	struct alloc_log *log;
	int i, idx;
	for (i = 0, idx = heap->log_idx; i < ALLOC_FREE_LOG_CNT; i++) {
		log = &heap->log[idx];
		if (log->size)
			seq_printf(
				s,
				"%4d:ts:%16llu\taddr:0x%pad, size:%8zu, source:%d, action:%s\n",
				ALLOC_FREE_LOG_CNT - i, log->time_nesc,
				&log->addr, log->size, log->source,
				log->dir ? "alloc" : "free");
		idx = (idx + 1) % ALLOC_FREE_LOG_CNT;
	}
	return 0;
}

static int log_open(struct inode *inode, struct file *file)
{
	return single_open(file, log_show, inode->i_private);
}

static const struct file_operations log_show_fops = {
	.open = log_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int debug_show_open(struct inode *inode, struct file *file)
{
	return single_open(file, ion_size_pool_debug_show, inode->i_private);
}

static const struct file_operations debug_show_fops = {
	.open = debug_show_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int statistics_show_open(struct inode *inode, struct file *file)
{
	return single_open(file, ion_size_pool_statistics_show, inode->i_private);
}

static const struct file_operations statistics_show_fops = {
	.open = statistics_show_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int ion_size_pool_callers_show(struct seq_file *s, void *unused)
{
	struct list_head *buf_lh;
	struct ion_size_pool_heap *size_pool_heap = s->private;
	int i;

	list_for_each (buf_lh, &size_pool_heap->buffers) {
		struct size_pool_buffer_priv *priv = container_of(
			buf_lh, struct size_pool_buffer_priv, list);
		struct ion_buffer *buffer = priv->buf;
		struct stack_info *info;
		phys_addr_t paddr;

		paddr = PFN_PHYS(page_to_pfn(sg_page(buffer->sg_table->sgl)));

		seq_printf(s, "buf 0x%x@%pad\n", buffer->size, &paddr);
		list_for_each_entry (info, &priv->stacks, node) {
			seq_printf(s, "%s",
				   info->type == STACK_ALLOC ?
					   "\talloc stack:\n" :
					   "\tmap_stack:\n");
			for (i = 0; i < info->caller_cnt; i++) {
				seq_printf(s, "\t\t[<%px>] %pS\n",
					   (void *)info->caller[i],
					   (void *)info->caller[i]);
			}
		}
	}

	return 0;
}

static int callers_show_open(struct inode *inode, struct file *file)
{
	return single_open(file, ion_size_pool_callers_show, inode->i_private);
}

static const struct file_operations callers_show_fops = {
	.open = callers_show_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

unsigned long gen_pool_last_fit(unsigned long *map, unsigned long size,
				unsigned long start, unsigned int nr,
				void *data, struct gen_pool *pool,
				unsigned long start_addr)
{
	unsigned long empty_start = 0, empty_end = 0;
	unsigned long candidate = size;
	/* search for last usable area */

	while (empty_start < size) {
		empty_start = find_next_zero_bit(map, size, empty_end);
		empty_end = find_next_bit(map, size, empty_start);
		if (empty_end > empty_start + nr) {
			/* found one better(closer to end) candidate */
			candidate = empty_end - nr;
		}
	};
	return candidate;
}

#include <linux/platform_device.h>
static int ion_size_pool_probe(struct platform_device *pdev)
{
	struct ion_platform_heap heap_data;
	struct ion_heap *heap;
	const char *name;
	u32 dts_reserve_base, dts_reserve_size;
	int ret = 0;
	if (!ctrl_cls) {
		ctrl_cls = class_create(THIS_MODULE, "size_pool_ctrl");
		if (!ctrl_cls) {
			return -ENOMEM;
		}
	}
	ret = of_property_read_u32(pdev->dev.of_node, "heap-base",
				   &dts_reserve_base);
	ret |= of_property_read_u32(pdev->dev.of_node, "heap-size",
				    &dts_reserve_size);
	ret |= of_property_read_string(pdev->dev.of_node, "heap-name", &name);
	if (ret == 0) {
		heap_data.base = dts_reserve_base;
		heap_data.size = dts_reserve_size;
		heap_data.priv = (void *)&pdev->dev;
		heap = ion_size_pool_heap_create(&heap_data);
		heap->name = "size_pool";
		ion_device_add_heap(heap);
		pdev->dev.platform_data = heap;
		return 0;
	} else {
		return -EINVAL;
	}
}
static int ion_size_pool_remove(struct platform_device *pdev)
{
	ion_size_pool_heap_destroy(pdev->dev.platform_data);
	class_destroy(ctrl_cls);
	return 0;
}

static const struct of_device_id ion_size_pool_dt_ids[] = {
	{ .compatible = "allwinner,size_pool" },
	{ /* sentinel */ },
};
static struct platform_driver
	ion_size_pool_driver = { .probe = ion_size_pool_probe,
				 .remove = ion_size_pool_remove,
				 .driver = {
					 .owner = THIS_MODULE,
					 .name = "ion_size_pool",
					 .of_match_table = ion_size_pool_dt_ids,
				 } };

static int __init ion_size_pool_init(void)
{
	return platform_driver_register(&ion_size_pool_driver);
}

static void __exit ion_size_pool_exit(void)
{
	return platform_driver_unregister(&ion_size_pool_driver);
}

#ifdef CONFIG_AW_IPC_FASTBOOT
postcore_initcall(ion_size_pool_init);
#else
subsys_initcall(ion_size_pool_init);
#endif
module_exit(ion_size_pool_exit);
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0.1");
MODULE_AUTHOR("ouayngkun<ouyangkun@allwinnertech.com>");
