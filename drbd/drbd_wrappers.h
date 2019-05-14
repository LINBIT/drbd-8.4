#ifndef _DRBD_WRAPPERS_H
#define _DRBD_WRAPPERS_H

#include "compat.h"
#include <linux/ctype.h>
#include <linux/net.h>
#include <linux/version.h>
#include <linux/crypto.h>
#include <linux/netlink.h>
#include <linux/idr.h>
#include <linux/fs.h>
#include <linux/bio.h>
#include <linux/slab.h>
#include <linux/completion.h>
#include <linux/proc_fs.h>
#include <linux/blkdev.h>

#ifndef pr_fmt
#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt
#endif

/* {{{ pr_* macros */
/* some very old kernels don't have them, or at least not all of them */
#ifndef pr_emerg
#define pr_emerg(fmt, ...) \
		printk(KERN_EMERG pr_fmt(fmt), ##__VA_ARGS__)
#endif
#ifndef pr_alert
#define pr_alert(fmt, ...) \
		printk(KERN_ALERT pr_fmt(fmt), ##__VA_ARGS__)
#endif
#ifndef pr_crit
#define pr_crit(fmt, ...) \
		printk(KERN_CRIT pr_fmt(fmt), ##__VA_ARGS__)
#endif
#ifndef pr_err
#define pr_err(fmt, ...) \
		printk(KERN_ERR pr_fmt(fmt), ##__VA_ARGS__)
#endif
#ifndef pr_warning
#define pr_warning(fmt, ...) \
		printk(KERN_WARNING pr_fmt(fmt), ##__VA_ARGS__)
#endif
#ifndef pr_warn
#define pr_warn pr_warning
#endif
#ifndef pr_notice
#define pr_notice(fmt, ...) \
		printk(KERN_NOTICE pr_fmt(fmt), ##__VA_ARGS__)
#endif
#ifndef pr_info
#define pr_info(fmt, ...) \
		printk(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__)
#endif
#ifndef pr_cont
#define pr_cont(fmt, ...) \
		printk(KERN_CONT fmt, ##__VA_ARGS__)
#endif

/* pr_devel() should produce zero code unless DEBUG is defined */
#ifndef pr_devel
#ifdef DEBUG
#define pr_devel(fmt, ...) \
		printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#else
#define pr_devel(fmt, ...) \
		no_printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#endif
#endif
/* }}} pr_* macros */


#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
# error "At least kernel version 2.6.18 (with patches) required"
#endif

/* The history of blkdev_issue_flush()

   It had 2 arguments before fbd9b09a177a481eda256447c881f014f29034fe,
   after it had 4 arguments. (With that commit came BLKDEV_IFL_WAIT)

   It had 4 arguments before dd3932eddf428571762596e17b65f5dc92ca361b,
   after it got 3 arguments. (With that commit came BLKDEV_DISCARD_SECURE
   and BLKDEV_IFL_WAIT disappeared again.) */
#ifndef BLKDEV_IFL_WAIT
#ifndef BLKDEV_DISCARD_SECURE
/* before fbd9b09a177 */
#define blkdev_issue_flush(b, gfpf, s)	blkdev_issue_flush(b, s)
#endif
/* after dd3932eddf4 no define at all */
#else
/* between fbd9b09a177 and dd3932eddf4 */
#define blkdev_issue_flush(b, gfpf, s)	blkdev_issue_flush(b, gfpf, s, BLKDEV_IFL_WAIT)
#endif

#ifndef __GFP_RECLAIM
#define __GFP_RECLAIM __GFP_WAIT
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,31)
static inline unsigned short queue_logical_block_size(struct request_queue *q)
{
	int retval = 512;
	if (q && q->hardsect_size)
		retval = q->hardsect_size;
	return retval;
}

/* we only have hardsect_size yet */
static inline unsigned short queue_physical_block_size(struct request_queue *q)
{
	return queue_logical_block_size(q);
}

static inline unsigned short bdev_logical_block_size(struct block_device *bdev)
{
	return queue_logical_block_size(bdev_get_queue(bdev));
}

static inline unsigned int queue_io_min(struct request_queue *q)
{
	return queue_logical_block_size(q);
}

static inline unsigned int queue_io_opt(struct request_queue *q)
{
	return queue_logical_block_size(q);
}

static inline int queue_alignment_offset(struct request_queue *q)
{
	return 0;
}

static inline unsigned int queue_max_hw_sectors(struct request_queue *q)
{
	return q->max_hw_sectors;
}

static inline unsigned int queue_max_sectors(struct request_queue *q)
{
	return q->max_sectors;
}

static inline void blk_queue_logical_block_size(struct request_queue *q, unsigned short size)
{
	q->hardsect_size = size;
}
#endif
#ifndef COMPAT_QUEUE_LIMITS_HAS_DISCARD_ZEROES_DATA
static inline unsigned int queue_discard_zeroes_data(struct request_queue *q)
{
	return 0;
}
#endif

#ifndef COMPAT_HAVE_BDEV_DISCARD_ALIGNMENT
static inline int bdev_discard_alignment(struct block_device *bdev)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,33)
	return 0;
#else
	struct request_queue *q = bdev_get_queue(bdev);

	if (bdev != bdev->bd_contains)
		return bdev->bd_part->discard_alignment;

	return q->limits.discard_alignment;
#endif
}
#endif

#ifdef COMPAT_HAVE_BLK_QC_T_MAKE_REQUEST
/* in Commit dece16353ef47d8d33f5302bc158072a9d65e26f
 * make_request() becomes type blk_qc_t. */
#define MAKE_REQUEST_TYPE blk_qc_t
#define MAKE_REQUEST_RETURN return BLK_QC_T_NONE
#else
#ifdef COMPAT_HAVE_VOID_MAKE_REQUEST
/* in Commit 5a7bbad27a410350e64a2d7f5ec18fc73836c14f (between Linux-3.1 and 3.2)
   make_request() becomes type void. Before it had type int. */
#define MAKE_REQUEST_TYPE void
#define MAKE_REQUEST_RETURN return
#else
#define MAKE_REQUEST_TYPE int
#define MAKE_REQUEST_RETURN return 0
#endif
#endif

#ifndef COMPAT_HAVE_FMODE_T
typedef unsigned __bitwise__ fmode_t;
#endif

#ifndef COMPAT_HAVE_BLKDEV_GET_BY_PATH
/* see kernel 2.6.37,
 * d4d7762 block: clean up blkdev_get() wrappers and their users
 * e525fd8 block: make blkdev_get/put() handle exclusive access
 * and kernel 2.6.28
 * 30c40d2 [PATCH] propagate mode through open_bdev_excl/close_bdev_excl
 * Also note that there is no FMODE_EXCL before
 * 86d434d [PATCH] eliminate use of ->f_flags in block methods
 */
#ifndef COMPAT_HAVE_OPEN_BDEV_EXCLUSIVE
#ifndef FMODE_EXCL
#define FMODE_EXCL 0
#endif
static inline
struct block_device *open_bdev_exclusive(const char *path, fmode_t mode, void *holder)
{
	/* drbd does not open readonly, but try to be correct, anyways */
	return open_bdev_excl(path, (mode & FMODE_WRITE) ? 0 : MS_RDONLY, holder);
}
static inline
void close_bdev_exclusive(struct block_device *bdev, fmode_t mode)
{
	/* mode ignored. */
	close_bdev_excl(bdev);
}
#endif
static inline struct block_device *blkdev_get_by_path(const char *path,
		fmode_t mode, void *holder)
{
	return open_bdev_exclusive(path, mode, holder);
}

static inline int drbd_blkdev_put(struct block_device *bdev, fmode_t mode)
{
	/* blkdev_put != close_bdev_exclusive, in general, so this is obviously
	 * not correct, and there should be some if (mode & FMODE_EXCL) ...
	 * But this is the only way it is used in DRBD,
	 * and for <= 2.6.27, there is no FMODE_EXCL anyways. */
	close_bdev_exclusive(bdev, mode);

	/* blkdev_put seems to not have useful return values,
	 * close_bdev_exclusive is void. */
	return 0;
}
#define blkdev_put(b, m)	drbd_blkdev_put(b, m)
#endif

#ifdef COMPAT_HAVE_BIO_BI_STATUS
static inline void drbd_bio_endio(struct bio *bio, blk_status_t status)
{
	bio->bi_status = status;
	bio_endio(bio);
}
#define BIO_ENDIO_TYPE void
#define BIO_ENDIO_ARGS(b) (b)
#define BIO_ENDIO_FN_START	\
	blk_status_t status = bio->bi_status
#define BIO_ENDIO_FN_RETURN return

#else

typedef u8 __bitwise blk_status_t;
#define	BLK_STS_OK 0
#define BLK_STS_NOTSUPP		((__force blk_status_t)1)
#define BLK_STS_MEDIUM		((__force blk_status_t)7)
#define BLK_STS_RESOURCE	((__force blk_status_t)9)
#define BLK_STS_IOERR		((__force blk_status_t)10)
static int blk_status_to_errno(blk_status_t status)
{
	return  status == BLK_STS_OK ? 0 :
		status == BLK_STS_RESOURCE ? -ENOMEM :
		status == BLK_STS_NOTSUPP ? -EOPNOTSUPP :
		-EIO;
}
static inline blk_status_t errno_to_blk_status(int errno)
{
	blk_status_t status =
		errno == 0 ? BLK_STS_OK :
		errno == -ENOMEM ? BLK_STS_RESOURCE :
		errno == -EOPNOTSUPP ? BLK_STS_NOTSUPP :
		BLK_STS_IOERR;

	return status;
}

#ifdef COMPAT_HAVE_BIO_BI_ERROR
static inline void drbd_bio_endio(struct bio *bio, blk_status_t status)
{
	bio->bi_error = blk_status_to_errno(status);
	bio_endio(bio);
}
#define BIO_ENDIO_TYPE void
#define BIO_ENDIO_ARGS(b) (b)
#define BIO_ENDIO_FN_START	\
	blk_status_t status = errno_to_blk_status(bio->bi_error)
#define BIO_ENDIO_FN_RETURN return

#elif LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
/* Before Linux-2.6.24 bio_endio() had the size of the bio as second argument.
   See 6712ecf8f648118c3363c142196418f89a510b90 */
static inline void drbd_bio_endio(struct bio *bio, blk_status_t status)
{
	bio_endio(bio, bio->bi_size, blk_status_to_errno(status));
}
#define BIO_ENDIO_TYPE int
#define BIO_ENDIO_ARGS(b) (b, unsigned int bytes_done, int error)
#define BIO_ENDIO_FN_START	\
	int status = errno_to_blk_status(error); \
	int uptodate = bio_flagged(bio, BIO_UPTODATE); \
	if (!error && !uptodate) { error = -EIO; status = BLK_STS_IOERR; } \
	if (bio->bi_size) return 1
#define BIO_ENDIO_FN_RETURN return 0

#else
static inline void drbd_bio_endio(struct bio *bio, blk_status_t status)
{
	bio_endio(bio, blk_status_to_errno(status));
}
#define BIO_ENDIO_TYPE void
#define BIO_ENDIO_ARGS(b) (b, int error)
#define BIO_ENDIO_FN_START	\
	int status = errno_to_blk_status(error); \
	int uptodate = bio_flagged(bio, BIO_UPTODATE); \
	if (!error && !uptodate) { error = -EIO; status = BLK_STS_IOERR; }
#define BIO_ENDIO_FN_RETURN return

#endif
#endif

/* bi_end_io handlers */
extern BIO_ENDIO_TYPE drbd_md_endio BIO_ENDIO_ARGS(struct bio *bio);
extern BIO_ENDIO_TYPE drbd_peer_request_endio BIO_ENDIO_ARGS(struct bio *bio);
extern BIO_ENDIO_TYPE drbd_request_endio BIO_ENDIO_ARGS(struct bio *bio);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)
/* Before 2.6.23 (with 20c2df83d25c6a95affe6157a4c9cac4cf5ffaac) kmem_cache_create had a
   ctor and a dtor */
#define kmem_cache_create(N,S,A,F,C) kmem_cache_create(N,S,A,F,C,NULL)
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,26)
# undef HAVE_bvec_merge_data
# define HAVE_bvec_merge_data 1
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
static inline void sg_set_page(struct scatterlist *sg, struct page *page,
			       unsigned int len, unsigned int offset)
{
	sg->page   = page;
	sg->offset = offset;
	sg->length = len;
}

#define sg_init_table(S,N) ({})

#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28)
# define BD_OPS_USE_FMODE
#endif

/* how to get to the kobj of a gendisk.
 * see also upstream commits
 * edfaa7c36574f1bf09c65ad602412db9da5f96bf
 * ed9e1982347b36573cd622ee5f4e2a7ccd79b3fd
 * 548b10eb2959c96cef6fc29fc96e0931eeb53bc5
 */
#ifndef dev_to_disk
# define disk_to_kobj(disk) (&(disk)->kobj)
#else
# ifndef disk_to_dev
#  define disk_to_dev(disk) (&(disk)->dev)
# endif
# define disk_to_kobj(disk) (&disk_to_dev(disk)->kobj)
#endif

static inline int drbd_backing_bdev_events(struct gendisk *disk)
{
#if defined(__disk_stat_inc)
	/* older kernel */
	return (int)disk_stat_read(disk, sectors[0])
	     + (int)disk_stat_read(disk, sectors[1]);
#else
	/* recent kernel */
	return (int)part_stat_read(&disk->part0, sectors[0])
	     + (int)part_stat_read(&disk->part0, sectors[1]);
#endif
}

#ifndef COMPAT_HAVE_SOCK_SHUTDOWN
#define COMPAT_HAVE_SOCK_SHUTDOWN 1
enum sock_shutdown_cmd {
	SHUT_RD = 0,
	SHUT_WR = 1,
	SHUT_RDWR = 2,
};
static inline int kernel_sock_shutdown(struct socket *sock, enum sock_shutdown_cmd how)
{
	return sock->ops->shutdown(sock, how);
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)
static inline void drbd_unregister_blkdev(unsigned int major, const char *name)
{
	int ret = unregister_blkdev(major, name);
	if (ret)
		pr_err("unregister of device failed\n");
}
#else
#define drbd_unregister_blkdev unregister_blkdev
#endif

#ifndef barrier_data
#define barrier_data(ptr) barrier()
#endif

#if !defined(CRYPTO_ALG_ASYNC)
/* With Linux-2.6.19 the crypto API changed! */
/* This is not a generic backport of the new api, it just implements
   the corner case of "hmac(xxx)".  */

#define CRYPTO_ALG_ASYNC 4711
#define CRYPTO_ALG_TYPE_HASH CRYPTO_ALG_TYPE_DIGEST

struct crypto_hash {
	struct crypto_tfm *base;
	const u8 *key;
	int keylen;
};

struct hash_desc {
	struct crypto_hash *tfm;
	u32 flags;
};

static inline struct crypto_hash *
crypto_alloc_hash(char *alg_name, u32 type, u32 mask)
{
	struct crypto_hash *ch;
	char *closing_bracket;

	/* "hmac(xxx)" is in alg_name we need that xxx. */
	closing_bracket = strchr(alg_name, ')');
	if (!closing_bracket) {
		ch = kmalloc(sizeof(struct crypto_hash), GFP_KERNEL);
		if (!ch)
			return ERR_PTR(-ENOMEM);
		ch->base = crypto_alloc_tfm(alg_name, 0);
		if (ch->base == NULL) {
			kfree(ch);
			return ERR_PTR(-ENOMEM);
		}
		return ch;
	}
	if (closing_bracket-alg_name < 6)
		return ERR_PTR(-ENOENT);

	ch = kmalloc(sizeof(struct crypto_hash), GFP_KERNEL);
	if (!ch)
		return ERR_PTR(-ENOMEM);

	*closing_bracket = 0;
	ch->base = crypto_alloc_tfm(alg_name + 5, 0);
	*closing_bracket = ')';

	if (ch->base == NULL) {
		kfree(ch);
		return ERR_PTR(-ENOMEM);
	}

	return ch;
}

static inline int
crypto_hash_setkey(struct crypto_hash *hash, const u8 *key, unsigned int keylen)
{
	hash->key = key;
	hash->keylen = keylen;

	return 0;
}

static inline int
crypto_hash_digest(struct hash_desc *desc, struct scatterlist *sg,
		   unsigned int nbytes, u8 *out)
{

	crypto_hmac(desc->tfm->base, (u8 *)desc->tfm->key,
		    &desc->tfm->keylen, sg, 1 /* ! */ , out);
	/* ! this is not generic. Would need to convert nbytes -> nsg */

	return 0;
}

static inline void crypto_free_hash(struct crypto_hash *tfm)
{
	if (!tfm)
		return;
	crypto_free_tfm(tfm->base);
	kfree(tfm);
}

static inline unsigned int crypto_hash_digestsize(struct crypto_hash *tfm)
{
	return crypto_tfm_alg_digestsize(tfm->base);
}

static inline struct crypto_tfm *crypto_hash_tfm(struct crypto_hash *tfm)
{
	return tfm->base;
}

static inline int crypto_hash_init(struct hash_desc *desc)
{
	crypto_digest_init(desc->tfm->base);
	return 0;
}

static inline int crypto_hash_update(struct hash_desc *desc,
				     struct scatterlist *sg,
				     unsigned int nbytes)
{
	crypto_digest_update(desc->tfm->base,sg,1 /* ! */ );
	/* ! this is not generic. Would need to convert nbytes -> nsg */

	return 0;
}

static inline int crypto_hash_final(struct hash_desc *desc, u8 *out)
{
	crypto_digest_final(desc->tfm->base, out);
	return 0;
}

/* ... and the api changed once again ...
 * Makes _our_ usage of it appear to work on RHEL 2.6.18.
 * HACK! Certainly not a generic "backport" or compat layer! */

#define crypto_ahash crypto_hash
#define crypto_ahash_digestsize crypto_hash_digestsize
#define crypto_alloc_ahash(name, dummy1, dummy2)	crypto_alloc_hash(name, dummy1, dummy2)
#define crypto_alloc_shash(name, dummy1, dummy2)	crypto_alloc_hash(name, dummy1, dummy2)
#define crypto_free_ahash crypto_free_hash
#define crypto_free_shash crypto_free_hash

#define AHASH_REQUEST_ON_STACK(name, tfm) \
	struct hash_desc __##name##_desc;			\
	struct hash_desc *name = (void*)&__##name##_desc

#define ahash_request_set_tfm(req, tfm)	\
	do { req->tfm = tfm; } while (0)
#define ahash_request_set_callback(req, _flags, completion, data)	\
	do { req->flags = _flags; } while (0)

#define crypto_ahash_init(req)	crypto_hash_init(req)

#define ahash_request_set_crypt(req, src, result, nbytes) do {	\
		if (src)					\
			crypto_hash_update(req, src, nbytes);	\
		if (result)					\
			crypto_hash_final(req, result);		\
	} while (0)

#define crypto_ahash_update(req)			do { } while (0)
#define crypto_ahash_final(req)				do { } while (0)
#define crypto_ahash_finup(req)				do { } while (0)

static inline void hash_desc_zero(struct hash_desc *desc)
{
	memset(desc, 0, sizeof(*desc));
	barrier_data(desc);
}

#define ahash_request_zero(req)		hash_desc_zero(req)
#define shash_desc_zero(desc)		hash_desc_zero(desc)

#define SHASH_DESC_ON_STACK(name, tfm) \
	struct hash_desc __##name##_desc;			\
	struct hash_desc *name = (void*)&__##name##_desc

#define crypto_shash_setkey	crypto_hash_setkey
#define crypto_shash_digestsize crypto_hash_digestsize

#include <linux/scatterlist.h>
static inline int crypto_shash_digest(struct hash_desc *desc, u8 *data,
				      unsigned int len, u8 *out)
{
	struct scatterlist sg;
	sg_init_table(&sg, 1);
	sg_set_buf(&sg, data, len);
	return crypto_hash_digest(desc, &sg, sg.length, out);
}

#define COMPAT_CRYPTO_BACKPORT_2_6_18_IN_USE 1

#endif

#ifndef COMPAT_HAVE_VZALLOC
static inline void *vzalloc(unsigned long size)
{
	void *rv = vmalloc(size);
	if (rv)
		memset(rv, 0, size);

	return rv;
}
#endif

#ifndef COMPAT_HAVE_UMH_WAIT_PROC
/* On Jul 17 2007 with commit 86313c4 usermodehelper: Tidy up waiting,
 * UMH_WAIT_PROC was added as an enum value of 1.
 * On Mar 23 2012 with commit 9d944ef3 that got changed to a define of 2. */
#define UMH_WAIT_PROC 1
#endif

/* see upstream commit 2d3854a37e8b767a51aba38ed6d22817b0631e33 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,30)
#ifndef cpumask_bits
#ifndef COMPAT_HAVE_NR_CPU_IDS
#define nr_cpu_ids NR_CPUS
#endif
#define nr_cpumask_bits nr_cpu_ids

typedef cpumask_t cpumask_var_t[1];
#define cpumask_bits(maskp) ((unsigned long*)(maskp))
#define cpu_online_mask &(cpu_online_map)

static inline void cpumask_clear(cpumask_t *dstp)
{
	bitmap_zero(cpumask_bits(dstp), NR_CPUS);
}

static inline int cpumask_equal(const cpumask_t *src1p,
				const cpumask_t *src2p)
{
	return bitmap_equal(cpumask_bits(src1p), cpumask_bits(src2p),
						 nr_cpumask_bits);
}

static inline void cpumask_copy(cpumask_t *dstp,
				cpumask_t *srcp)
{
	bitmap_copy(cpumask_bits(dstp), cpumask_bits(srcp), nr_cpumask_bits);
}

static inline unsigned int cpumask_weight(const cpumask_t *srcp)
{
	return bitmap_weight(cpumask_bits(srcp), nr_cpumask_bits);
}

static inline void cpumask_set_cpu(unsigned int cpu, cpumask_t *dstp)
{
	set_bit(cpu, cpumask_bits(dstp));
}

static inline void cpumask_setall(cpumask_t *dstp)
{
	bitmap_fill(cpumask_bits(dstp), nr_cpumask_bits);
}

static inline int cpumask_and(cpumask_t *dstp,
			       const cpumask_t *src1p,
			       const cpumask_t *src2p)
{
	bitmap_and(cpumask_bits(dstp), cpumask_bits(src1p),
				       cpumask_bits(src2p), nr_cpumask_bits);
	/* some older kernels have void bitmap_and().
	 * we don't care for the return value anyways. */
	return 1;
}

static inline void free_cpumask_var(cpumask_var_t mask)
{
}
#endif
/* see upstream commit 0281b5dc0350cbf6dd21ed558a33cccce77abc02 */
#ifdef CONFIG_CPUMASK_OFFSTACK
static inline int zalloc_cpumask_var(cpumask_var_t *mask, gfp_t flags)
{
	return alloc_cpumask_var(mask, flags | __GFP_ZERO);
}
#else
static inline int zalloc_cpumask_var(cpumask_var_t *mask, gfp_t flags)
{
	cpumask_clear(*mask);
	return 1;
}
#endif
/* see upstream commit cd8ba7cd9be0192348c2836cb6645d9b2cd2bfd2 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26)
/* As macro because RH has it in 2.6.18-128.4.1.el5, but not exported to modules !?!? */
#define set_cpus_allowed_ptr(P, NM) set_cpus_allowed(P, *NM)
#endif
#endif


#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
#define bitmap_parse(BUF, BUFLEN, MASKP, NMASK) \
	backport_bitmap_parse(BUF, BUFLEN, 0, MASKP, NMASK)

#define CHUNKSZ                         32
#define nbits_to_hold_value(val)        fls(val)
#define unhex(c)                        (isdigit(c) ? (c - '0') : (toupper(c) - 'A' + 10))

static inline int backport_bitmap_parse(const char *buf, unsigned int buflen,
		int is_user, unsigned long *maskp,
		int nmaskbits)
{
	int c, old_c, totaldigits, ndigits, nchunks, nbits;
	u32 chunk;
	const char __user *ubuf = buf;

	bitmap_zero(maskp, nmaskbits);

	nchunks = nbits = totaldigits = c = 0;
	do {
		chunk = ndigits = 0;

		/* Get the next chunk of the bitmap */
		while (buflen) {
			old_c = c;
			if (is_user) {
				if (__get_user(c, ubuf++))
					return -EFAULT;
			}
			else
				c = *buf++;
			buflen--;
			if (isspace(c))
				continue;

			/*
			 * If the last character was a space and the current
			 * character isn't '\0', we've got embedded whitespace.
			 * This is a no-no, so throw an error.
			 */
			if (totaldigits && c && isspace(old_c))
				return -EINVAL;

			/* A '\0' or a ',' signal the end of the chunk */
			if (c == '\0' || c == ',')
				break;

			if (!isxdigit(c))
				return -EINVAL;

			/*
			 * Make sure there are at least 4 free bits in 'chunk'.
			 * If not, this hexdigit will overflow 'chunk', so
			 * throw an error.
			 */
			if (chunk & ~((1UL << (CHUNKSZ - 4)) - 1))
				return -EOVERFLOW;

			chunk = (chunk << 4) | unhex(c);
			ndigits++; totaldigits++;
		}
		if (ndigits == 0)
			return -EINVAL;
		if (nchunks == 0 && chunk == 0)
			continue;

		bitmap_shift_left(maskp, maskp, CHUNKSZ, nmaskbits);
		*maskp |= chunk;
		nchunks++;
		nbits += (nchunks == 1) ? nbits_to_hold_value(chunk) : CHUNKSZ;
		if (nbits > nmaskbits)
			return -EOVERFLOW;
	} while (buflen && c == ',');

	return 0;
}
#endif

#ifndef net_random
#define random32 net_random
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,30)
#define BDI_async_congested BDI_write_congested
#define BDI_sync_congested  BDI_read_congested
#endif

/* see upstream commits
 * 2d3a4e3666325a9709cc8ea2e88151394e8f20fc (in 2.6.25-rc1)
 * 59b7435149eab2dd06dd678742faff6049cb655f (in 2.6.26-rc1)
 * this "backport" does not close the race that lead to the API change,
 * but only provides an equivalent function call.
 */
#ifndef COMPAT_HAVE_PROC_CREATE_DATA
static inline struct proc_dir_entry *proc_create_data(const char *name,
	mode_t mode, struct proc_dir_entry *parent,
	struct file_operations *proc_fops, void *data)
{
	struct proc_dir_entry *pde = create_proc_entry(name, mode, parent);
	if (pde) {
		pde->proc_fops = proc_fops;
		pde->data = data;
	}
	return pde;
}

#endif

#ifndef COMPAT_HAVE_BLK_QUEUE_MAX_HW_SECTORS
static inline void blk_queue_max_hw_sectors(struct request_queue *q, unsigned int max)
{
	blk_queue_max_sectors(q, max);
}
#elif defined(COMPAT_USE_BLK_QUEUE_MAX_SECTORS_ANYWAYS)
	/* For kernel versions 2.6.31 to 2.6.33 inclusive, even though
	 * blk_queue_max_hw_sectors is present, we actually need to use
	 * blk_queue_max_sectors to set max_hw_sectors. :-(
	 * RHEL6 2.6.32 chose to be different and already has eliminated
	 * blk_queue_max_sectors as upstream 2.6.34 did.
	 */
#define blk_queue_max_hw_sectors(q, max)	blk_queue_max_sectors(q, max)
#endif

#ifndef COMPAT_HAVE_BLK_QUEUE_MAX_SEGMENTS
static inline void blk_queue_max_segments(struct request_queue *q, unsigned short max_segments)
{
	blk_queue_max_phys_segments(q, max_segments);
	blk_queue_max_hw_segments(q, max_segments);
#define BLK_MAX_SEGMENTS MAX_HW_SEGMENTS /* or max MAX_PHYS_SEGMENTS. Probably does not matter */
}
#endif

/* How do we tell the block layer to pass down flush/fua? */
#ifndef COMPAT_HAVE_BLK_QUEUE_WRITE_CACHE
static inline void blk_queue_write_cache(struct request_queue *q, bool enabled, bool fua)
{
#if defined(REQ_FLUSH) && !defined(REQ_HARDBARRIER)
/* Linux version 2.6.37 up to 4.7
 * needs blk_queue_flush() to announce driver support */
	blk_queue_flush(q, (enabled ? REQ_FLUSH : 0) | (fua ? REQ_FUA : 0));
#else
/* Older kernels either flag affected bios with BIO_RW_BARRIER, or do not know
 * how to handle this at all. No need to "announce" driver support. */
#endif
}
#endif

#define _adjust_ra_pages(qrap, brap) do { \
	if (qrap != brap) { \
		drbd_info(device, "Adjusting my ra_pages to backing device's (%lu -> %lu)\n", qrap, brap); \
		qrap = brap; \
	} \
} while(0)

#ifdef BDI_CAP_STABLE_WRITES /* >= v3.9 */
#define set_bdi_cap_stable_writes(cap)	do { (cap) |= BDI_CAP_STABLE_WRITES; } while (0)
#else /* < v3.9 */
#define set_bdi_cap_stable_writes(cap)	do { } while (0)
#endif

#ifdef COMPAT_HAVE_POINTER_BACKING_DEV_INFO /* >= v4.11 */
#define bdi_from_device(device) (device->ldev->backing_bdev->bd_disk->queue->backing_dev_info)
#define init_bdev_info(bdev_info, drbd_congested, device) do { \
	(bdev_info)->congested_fn = drbd_congested; \
	(bdev_info)->congested_data = device; \
	set_bdi_cap_stable_writes(bdev_info->capabilities); \
} while(0)
#define adjust_ra_pages(q, b) _adjust_ra_pages((q)->backing_dev_info->ra_pages, (b)->backing_dev_info->ra_pages)
#else /* < v4.11 */
#define bdi_rw_congested(BDI) bdi_rw_congested(&BDI)
#define bdi_congested(BDI, BDI_BITS) bdi_congested(&BDI, (BDI_BITS))
#define bdi_from_device(device) (&device->ldev->backing_bdev->bd_disk->queue->backing_dev_info)
#define init_bdev_info(bdev_info, drbd_congested, device) do { \
	(bdev_info).congested_fn = drbd_congested; \
	(bdev_info).congested_data = device; \
	set_bdi_cap_stable_writes((bdev_info).capabilities); \
} while(0)
#define adjust_ra_pages(q, b) _adjust_ra_pages((q)->backing_dev_info.ra_pages, (b)->backing_dev_info.ra_pages)
#endif


/* bio -> bi_rw/bi_opf REQ_* and BIO_RW_* REQ_OP_* compat stuff {{{1 */
/* REQ_* and BIO_RW_* flags have been moved around in the tree,
 * and have finally been "merged" with
 * 7b6d91daee5cac6402186ff224c3af39d79f4a0e and
 * 7cc015811ef8992dfcce314d0ed9642bc18143d1
 * We communicate between different systems,
 * so we have to somehow semantically map the bi_opf flags
 * bi_opf (some kernel version) -> data packet flags -> bi_opf (other kernel version)
 */

#if defined(COMPAT_HAVE_BIO_SET_OP_ATTRS) && \
	!(defined(RHEL_RELEASE_CODE /* 7.4 broke our compat detection here */) && \
			LINUX_VERSION_CODE < KERNEL_VERSION(4,8,0))

/* [4.8 ... ] Linux 4.8 split bio OPs and FLAGs {{{2 */

#define DRBD_REQ_PREFLUSH	REQ_PREFLUSH
#define DRBD_REQ_FUA		REQ_FUA
#define DRBD_REQ_SYNC		REQ_SYNC

	/* long gone */
#define DRBD_REQ_HARDBARRIER	0
#define DRBD_REQ_UNPLUG		0

	/* became an op, no longer flag */
#define DRBD_REQ_DISCARD	0
#define DRBD_REQ_WSAME		0

/* Gone in Linux 4.10 */
#ifndef WRITE_SYNC
#define WRITE_SYNC REQ_SYNC
#endif

#define COMPAT_WRITE_SAME_CAPABLE

#ifndef COMPAT_HAVE_REQ_OP_WRITE_ZEROES
#define REQ_OP_WRITE_ZEROES (-3u)
#endif

#elif defined(BIO_FLUSH)
/* RHEL 6.1 ("not quite 2.6.32") backported FLUSH/FUA as BIO_RW_FLUSH/FUA {{{2
 * and at that time also introduced the defines BIO_FLUSH/FUA.
 * There is also REQ_FLUSH/FUA, but these do NOT share
 * the same value space as the bio rw flags, yet.
 */

#define DRBD_REQ_PREFLUSH	(1UL << BIO_RW_FLUSH)
#define DRBD_REQ_FUA		(1UL << BIO_RW_FUA)
#define DRBD_REQ_HARDBARRIER	(1UL << BIO_RW_BARRIER)
#define DRBD_REQ_DISCARD	(1UL << BIO_RW_DISCARD)
#define DRBD_REQ_SYNC		(1UL << BIO_RW_SYNCIO)
#define DRBD_REQ_UNPLUG		(1UL << BIO_RW_UNPLUG)

#define REQ_RAHEAD		(1UL << BIO_RW_AHEAD)

#elif defined(REQ_FLUSH)	/* [2.6.36 .. 4.7] introduced in 2.6.36, {{{2
				 * now equivalent to bi_rw */

#define DRBD_REQ_SYNC		REQ_SYNC
#define DRBD_REQ_PREFLUSH	REQ_FLUSH
#define DRBD_REQ_FUA		REQ_FUA
#define DRBD_REQ_DISCARD	REQ_DISCARD
/* REQ_HARDBARRIER has been around for a long time,
 * without being directly related to bi_rw.
 * so the ifdef is only usful inside the ifdef REQ_FLUSH!
 * commit 7cc0158 (v2.6.36-rc1) made it a bi_rw flag, ...  */
#ifdef REQ_HARDBARRIER
#define DRBD_REQ_HARDBARRIER	REQ_HARDBARRIER
#else
/* ... but REQ_HARDBARRIER was removed again in 02e031c (v2.6.37-rc4). */
#define DRBD_REQ_HARDBARRIER	0
#endif

/* again: testing on this _inside_ the ifdef REQ_FLUSH,
 * see 721a960 block: kill off REQ_UNPLUG */
#ifdef REQ_UNPLUG
#define DRBD_REQ_UNPLUG		REQ_UNPLUG
#else
#define DRBD_REQ_UNPLUG		0
#endif

#ifdef REQ_WRITE_SAME
#define DRBD_REQ_WSAME		REQ_WRITE_SAME
#define COMPAT_WRITE_SAME_CAPABLE
#endif

#else				/* [<=2.6.35] "older", and hopefully not {{{2
				 * "partially backported" kernel */

#define REQ_RAHEAD		(1UL << BIO_RW_AHEAD)

#if defined(BIO_RW_SYNC)
/* see upstream commits
 * 213d9417fec62ef4c3675621b9364a667954d4dd,
 * 93dbb393503d53cd226e5e1f0088fe8f4dbaa2b8
 * later, the defines even became an enum ;-) */
#define DRBD_REQ_SYNC		(1UL << BIO_RW_SYNC)
#define DRBD_REQ_UNPLUG		(1UL << BIO_RW_SYNC)
#else
/* cannot test on defined(BIO_RW_SYNCIO), it may be an enum */
#define DRBD_REQ_SYNC		(1UL << BIO_RW_SYNCIO)
#define DRBD_REQ_UNPLUG		(1UL << BIO_RW_UNPLUG)
#endif

#define DRBD_REQ_PREFLUSH	(1UL << BIO_RW_BARRIER)
/* REQ_FUA has been around for a longer time,
 * without a direct equivalent in bi_rw. */
#define DRBD_REQ_FUA		(1UL << BIO_RW_BARRIER)
#define DRBD_REQ_HARDBARRIER	(1UL << BIO_RW_BARRIER)

#define COMPAT_MAYBE_RETRY_HARDBARRIER

/* we don't support DISCARDS yet, anyways.
 * cannot test on defined(BIO_RW_DISCARD), it may be an enum */
#define DRBD_REQ_DISCARD	0
#endif

#ifdef REQ_NOUNMAP
#define DRBD_REQ_NOUNMAP REQ_NOUNMAP
#else
#define DRBD_REQ_NOUNMAP	0
#endif

/* this results in:
	bi_opf  -> dp_flags

< 2.6.28
	SYNC	-> SYNC|UNPLUG
	BARRIER	-> FUA|FLUSH
	there is no DISCARD
2.6.28
	SYNC	-> SYNC|UNPLUG
	BARRIER	-> FUA|FLUSH
	DISCARD	-> DISCARD
2.6.29
	SYNCIO	-> SYNC
	UNPLUG	-> UNPLUG
	BARRIER	-> FUA|FLUSH
	DISCARD	-> DISCARD
2.6.36
	SYNC	-> SYNC
	UNPLUG	-> UNPLUG
	FUA	-> FUA
	FLUSH	-> FLUSH
	DISCARD	-> DISCARD
--------------------------------------
	dp_flags   -> bi_rw
< 2.6.28
	SYNC	-> SYNC (and unplug)
	UNPLUG	-> SYNC (and unplug)
	FUA	-> BARRIER
	FLUSH	-> BARRIER
	there is no DISCARD,
	it will be silently ignored on the receiving side.
2.6.28
	SYNC	-> SYNC (and unplug)
	UNPLUG	-> SYNC (and unplug)
	FUA	-> BARRIER
	FLUSH	-> BARRIER
	DISCARD -> DISCARD
	(if that fails, we handle it like any other IO error)
2.6.29
	SYNC	-> SYNCIO
	UNPLUG	-> UNPLUG
	FUA	-> BARRIER
	FLUSH	-> BARRIER
	DISCARD -> DISCARD
2.6.36
	SYNC	-> SYNC
	UNPLUG	-> UNPLUG
	FUA	-> FUA
	FLUSH	-> FLUSH
	DISCARD	-> DISCARD
*/

/* fallback defines for older kernels {{{2 */

#ifndef DRBD_REQ_WSAME
#define DRBD_REQ_WSAME		0
#endif

#ifndef WRITE_FLUSH
#ifndef WRITE_SYNC
#error  FIXME WRITE_SYNC undefined??
#endif
#define WRITE_FLUSH       (WRITE_SYNC | DRBD_REQ_PREFLUSH)
#endif

#ifndef REQ_NOIDLE
/* introduced in aeb6faf (2.6.30), relevant for CFQ */
#define REQ_NOIDLE 0
#endif


#if defined(COMPAT_HAVE_BIO_SET_OP_ATTRS) /* compat for Linux before 4.8 {{{2 */
#if (defined(RHEL_RELEASE_CODE /* 7.4 broke our compat detection here */) && \
			LINUX_VERSION_CODE < KERNEL_VERSION(4,8,0))
/* Thank you RHEL 7.4 for backporting just enough to break existing compat code,
 * but not enough to make it work for us without additional compat code.
 */
#define COMPAT_NEED_BI_OPF_AND_SUBMIT_BIO_COMPAT_DEFINES 1
# ifdef COMPAT_HAVE_REQ_OP_WRITE_ZEROES
#  error "unexpectedly defined REQ_OP_WRITE_ZEROES, double check compat wrappers!"
# else
#  define  REQ_OP_WRITE_ZEROES (-3u)
# endif
#endif
#else /* !defined(COMPAT_HAVE_BIO_SET_OP_ATTRS) */
#define COMPAT_NEED_BI_OPF_AND_SUBMIT_BIO_COMPAT_DEFINES 1

#ifndef REQ_WRITE
/* before 2.6.36 */
#define REQ_WRITE 1
#endif

enum req_op {
       REQ_OP_READ,				/* 0 */
       REQ_OP_WRITE		= REQ_WRITE,	/* 1 */

       /* Not yet a distinguished op,
	* but identified via FLUSH/FUA flags.
	* If at all. */
       REQ_OP_FLUSH		= REQ_OP_WRITE,

	/* These may be not supported in older kernels.
	 * In that case, the DRBD_REQ_* will be 0,
	 * bio_op() aka. op_from_rq_bits() will never return these,
	 * and we map the REQ_OP_* to something stupid.
	 */
       REQ_OP_DISCARD		= DRBD_REQ_DISCARD ?: -1,
       REQ_OP_WRITE_SAME	= DRBD_REQ_WSAME   ?: -2,
       REQ_OP_WRITE_ZEROES	= -3,

       /* REQ_OP_SECURE_ERASE: does not matter to us,
	* I don't see how we could support that anyways. */
};
#define bio_op(bio)                            (op_from_rq_bits((bio)->bi_rw))

static inline void bio_set_op_attrs(struct bio *bio, const int op, const long flags)
{
	/* If we explicitly issue discards or write_same, we use
	 * blkdev_issue_discard() and blkdev_issue_write_same() helpers.
	 * If we implicitly submit them, we just pass on a cloned bio to
	 * generic_make_request().  We expect to use bio_set_op_attrs() with
	 * REQ_OP_READ or REQ_OP_WRITE only. */
	BUG_ON(!(op == REQ_OP_READ || op == REQ_OP_WRITE));
	bio->bi_rw |= (op | flags);
}

static inline int op_from_rq_bits(u64 flags)
{
	if (flags & DRBD_REQ_DISCARD)
		return REQ_OP_DISCARD;
	else if (flags & DRBD_REQ_WSAME)
		return REQ_OP_WRITE_SAME;
	else if (flags & REQ_WRITE)
		return REQ_OP_WRITE;
	else
		return REQ_OP_READ;
}
#endif

#ifdef COMPAT_NEED_BI_OPF_AND_SUBMIT_BIO_COMPAT_DEFINES
#define bi_opf bi_rw
#define submit_bio(__bio)	submit_bio(__bio->bi_rw, __bio)
/* see comment in above compat enum req_op */
#define REQ_OP_FLUSH		REQ_OP_WRITE
#endif
/* }}}1 bio -> bi_rw/bi_opf REQ_* and BIO_RW_* REQ_OP_* compat stuff */

#ifndef CONFIG_DYNAMIC_DEBUG
/* At least in 2.6.34 the function macro dynamic_dev_dbg() is broken when compiling
   without CONFIG_DYNAMIC_DEBUG. It has 'format' in the argument list, it references
   to 'fmt' in its body. */
#ifdef dynamic_dev_dbg
#undef dynamic_dev_dbg
#define dynamic_dev_dbg(dev, fmt, ...)                               \
        do { if (0) dev_printk(KERN_DEBUG, dev, fmt, ##__VA_ARGS__); } while (0)
#endif
#endif

#ifndef min_not_zero
#define min_not_zero(x, y) ({			\
	typeof(x) __x = (x);			\
	typeof(y) __y = (y);			\
	__x == 0 ? __y : ((__y == 0) ? __x : min(__x, __y)); })
#endif

/* Introduced with 2.6.26. See include/linux/jiffies.h */
#ifndef time_is_before_eq_jiffies
#define time_is_before_jiffies(a) time_after(jiffies, a)
#define time_is_after_jiffies(a) time_before(jiffies, a)
#define time_is_before_eq_jiffies(a) time_after_eq(jiffies, a)
#define time_is_after_eq_jiffies(a) time_before_eq(jiffies, a)
#endif

#ifndef time_in_range
#define time_in_range(a,b,c) \
	(time_after_eq(a,b) && \
	 time_before_eq(a,c))
#endif

#ifdef COMPAT_BIO_SPLIT_HAS_BIO_SPLIT_POOL_PARAMETER
#define bio_split(bi, first_sectors) bio_split(bi, bio_split_pool, first_sectors)
#endif

#ifdef COMPAT_HAVE_BIOSET_NEED_BVECS
/* upstream commit
 * 011067b05668 blk: replace bioset_create_nobvec() with a flags arg to bioset_create()
 *
 * bioset_create(pool_size, front_pad, flags);
 */
#elif defined(COMPAT_HAVE_BIOSET_CREATE_FRONT_PAD)
/* see comments in compat/tests/have_bioset_create_front_pad.c */
/* struct bio_set *bioset_create(unsigned int pool_size, unsigned int front_pad) */
#define bioset_create(pool_size, front_pad, flags)	bioset_create(pool_size, front_pad)
#elif defined(COMPAT_BIOSET_CREATE_HAS_THREE_PARAMETERS)
/* struct bio_set *bioset_create(int bio_pool_size, int bvec_pool_size, int scale) */
#define bioset_create(pool_size, front_pad, flags)	bioset_create(pool_size, pool_size, 1)
#else
/* struct bio_set *bioset_create(int bio_pool_size, int bvec_pool_size) */
#define bioset_create(pool_size, front_pad, flags)	bioset_create(pool_size, 1)
#endif


#if !(defined(COMPAT_HAVE_RB_AUGMENT_FUNCTIONS) && \
      defined(AUGMENTED_RBTREE_SYMBOLS_EXPORTED))

/*
 * Make sure the replacements for the augmented rbtree helper functions do not
 * clash with functions the kernel implements but does not export.
 */
#define rb_augment_f drbd_rb_augment_f
#define rb_augment_path drbd_rb_augment_path
#define rb_augment_insert drbd_rb_augment_insert
#define rb_augment_erase_begin drbd_rb_augment_erase_begin
#define rb_augment_erase_end drbd_rb_augment_erase_end

typedef void (*rb_augment_f)(struct rb_node *node, void *data);

static inline void rb_augment_path(struct rb_node *node, rb_augment_f func, void *data)
{
	struct rb_node *parent;

up:
	func(node, data);
	parent = rb_parent(node);
	if (!parent)
		return;

	if (node == parent->rb_left && parent->rb_right)
		func(parent->rb_right, data);
	else if (parent->rb_left)
		func(parent->rb_left, data);

	node = parent;
	goto up;
}

/*
 * after inserting @node into the tree, update the tree to account for
 * both the new entry and any damage done by rebalance
 */
static inline void rb_augment_insert(struct rb_node *node, rb_augment_f func, void *data)
{
	if (node->rb_left)
		node = node->rb_left;
	else if (node->rb_right)
		node = node->rb_right;

	rb_augment_path(node, func, data);
}

/*
 * before removing the node, find the deepest node on the rebalance path
 * that will still be there after @node gets removed
 */
static inline struct rb_node *rb_augment_erase_begin(struct rb_node *node)
{
	struct rb_node *deepest;

	if (!node->rb_right && !node->rb_left)
		deepest = rb_parent(node);
	else if (!node->rb_right)
		deepest = node->rb_left;
	else if (!node->rb_left)
		deepest = node->rb_right;
	else {
		deepest = rb_next(node);
		if (deepest->rb_right)
			deepest = deepest->rb_right;
		else if (rb_parent(deepest) != node)
			deepest = rb_parent(deepest);
	}

	return deepest;
}

/*
 * after removal, update the tree to account for the removed entry
 * and any rebalance damage.
 */
static inline void rb_augment_erase_end(struct rb_node *node, rb_augment_f func, void *data)
{
	if (node)
		rb_augment_path(node, func, data);
}
#endif

/*
 * In commit c4945b9e (v2.6.39-rc1), the little-endian bit operations have been
 * renamed to be less weird.
 */
#ifndef COMPAT_HAVE_FIND_NEXT_ZERO_BIT_LE
#define find_next_zero_bit_le(addr, size, offset) \
	generic_find_next_zero_le_bit(addr, size, offset)
#define find_next_bit_le(addr, size, offset) \
	generic_find_next_le_bit(addr, size, offset)
#define test_bit_le(nr, addr) \
	generic_test_le_bit(nr, addr)
#define __test_and_set_bit_le(nr, addr) \
	generic___test_and_set_le_bit(nr, addr)
#define __test_and_clear_bit_le(nr, addr) \
	generic___test_and_clear_le_bit(nr, addr)
#endif

#ifndef IDR_GET_NEXT_EXPORTED
/* Body in compat/idr.c */
extern void *idr_get_next(struct idr *idp, int *nextidp);
#endif

#ifndef RCU_INITIALIZER
#define RCU_INITIALIZER(v) (typeof(*(v)) *)(v)
#endif
#ifndef RCU_INIT_POINTER
#define RCU_INIT_POINTER(p, v) \
	do { \
		p = RCU_INITIALIZER(v); \
	} while (0)
#endif

#ifndef list_entry_rcu
#ifndef rcu_dereference_raw
/* see c26d34a rcu: Add lockdep-enabled variants of rcu_dereference() */
#define rcu_dereference_raw(p) rcu_dereference(p)
#endif
#define list_entry_rcu(ptr, type, member) \
	({typeof (*ptr) *__ptr = (typeof (*ptr) __force *)ptr; \
	 container_of((typeof(ptr))rcu_dereference_raw(__ptr), type, member); \
	})
#endif

#ifndef list_for_each_entry_continue_rcu
#define list_for_each_entry_continue_rcu(pos, head, member) 		\
	for (pos = list_entry_rcu(pos->member.next, typeof(*pos), member); \
	     prefetch(pos->member.next), &pos->member != (head);	\
	     pos = list_entry_rcu(pos->member.next, typeof(*pos), member))
#endif

#ifndef list_next_entry
/* introduced in 008208c (v3.13-rc1) */
#define list_next_entry(pos, member) \
        list_entry((pos)->member.next, typeof(*(pos)), member)
#endif

/*
 * Introduced in 930631ed (v2.6.19-rc1).
 */
#ifndef DIV_ROUND_UP
#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))
#endif

/*
 * IS_ALIGNED() was added to <linux/kernel.h> in mainline commit 0c0e6195 (and
 * improved in f10db627); 2.6.24-rc1.
 */
#ifndef IS_ALIGNED
#define IS_ALIGNED(x, a) (((x) & ((typeof(x))(a) - 1)) == 0)
#endif

/*
 * NLA_TYPE_MASK and nla_type() were added to <linux/netlink.h> in mainline
 * commit 8f4c1f9b; v2.6.24-rc1.  Before that, none of the nlattr->nla_type
 * flags had a special meaning.
 */

#ifndef NLA_TYPE_MASK
#define NLA_TYPE_MASK ~0

static inline int nla_type(const struct nlattr *nla)
{
	return nla->nla_type & NLA_TYPE_MASK;
}

#endif

/*
 * nlmsg_hdr was added to <linux/netlink.h> in mainline commit b529ccf2
 * (v2.6.22-rc1).
 */

#ifndef COMPAT_HAVE_NLMSG_HDR
static inline struct nlmsghdr *nlmsg_hdr(const struct sk_buff *skb)
{
	return (struct nlmsghdr *)skb->data;
}
#endif

/*
 * v4.12 fceb6435e852 netlink: pass extended ACK struct to parsing functions
 * and some preparation commits introduce a new "netlink extended ack" error
 * reporting mechanism. For now, only work around that here.  As trigger, use
 * NETLINK_MAX_COOKIE_LEN introduced somewhere in the middle of that patchset.
 */
#ifndef NETLINK_MAX_COOKIE_LEN
#include <net/netlink.h>
#define nla_parse_nested(tb, maxtype, nla, policy, extack) \
	nla_parse_nested(tb, maxtype, nla, policy)
#endif

/*
 * genlmsg_reply() was added to <net/genetlink.h> in mainline commit 81878d27
 * (v2.6.20-rc2).
 */

#ifndef COMPAT_HAVE_GENLMSG_REPLY
#include <net/genetlink.h>

static inline int genlmsg_reply(struct sk_buff *skb, struct genl_info *info)
{
	return genlmsg_unicast(skb, info->snd_pid);
}
#endif

/*
 * genlmsg_msg_size() and genlmsg_total_size() were added to <net/genetlink.h>
 * in mainline commit 17db952c (v2.6.19-rc1).
 */

#ifndef COMPAT_HAVE_GENLMSG_MSG_SIZE
#include <linux/netlink.h>
#include <linux/genetlink.h>

static inline int genlmsg_msg_size(int payload)
{
	return GENL_HDRLEN + payload;
}

static inline int genlmsg_total_size(int payload)
{
	return NLMSG_ALIGN(genlmsg_msg_size(payload));
}
#endif

/*
 * genlmsg_new() was added to <net/genetlink.h> in mainline commit 3dabc715
 * (v2.6.20-rc2).
 */

#ifndef COMPAT_HAVE_GENLMSG_NEW
#include <net/genetlink.h>

static inline struct sk_buff *genlmsg_new(size_t payload, gfp_t flags)
{
	return nlmsg_new(genlmsg_total_size(payload), flags);
}
#endif

/*
 * genlmsg_put() was introduced in mainline commit 482a8524 (v2.6.15-rc1) and
 * changed in 17c157c8 (v2.6.20-rc2).  genlmsg_put_reply() was introduced in
 * 17c157c8.  We replace the compat_genlmsg_put() from 482a8524.
 */

#ifndef COMPAT_HAVE_GENLMSG_PUT_REPLY
#include <net/genetlink.h>

static inline void *compat_genlmsg_put(struct sk_buff *skb, u32 pid, u32 seq,
				       struct genl_family *family, int flags,
				       u8 cmd)
{
	struct nlmsghdr *nlh;
	struct genlmsghdr *hdr;

	nlh = nlmsg_put(skb, pid, seq, family->id, GENL_HDRLEN +
			family->hdrsize, flags);
	if (nlh == NULL)
		return NULL;

	hdr = nlmsg_data(nlh);
	hdr->cmd = cmd;
	hdr->version = family->version;
	hdr->reserved = 0;

	return (char *) hdr + GENL_HDRLEN;
}

#define genlmsg_put compat_genlmsg_put

static inline void *genlmsg_put_reply(struct sk_buff *skb,
                                      struct genl_info *info,
                                      struct genl_family *family,
                                      int flags, u8 cmd)
{
	return genlmsg_put(skb, info->snd_pid, info->snd_seq, family,
			   flags, cmd);
}
#endif

/*
 * compat_genlmsg_multicast() got a gfp_t parameter in mainline commit d387f6ad
 * (v2.6.19-rc1).
 */

#ifdef COMPAT_NEED_GENLMSG_MULTICAST_WRAPPER
#include <net/genetlink.h>

static inline int compat_genlmsg_multicast(struct sk_buff *skb, u32 pid,
					   unsigned int group, gfp_t flags)
{
	return genlmsg_multicast(skb, pid, group);
}

#define genlmsg_multicast compat_genlmsg_multicast

#endif

/*
 * Dynamic generic netlink multicast groups were introduced in mainline commit
 * 2dbba6f7 (v2.6.23-rc1).  Before that, netlink had a fixed number of 32
 * multicast groups.  Use an arbitrary hard-coded group number for that case.
 */

#ifndef COMPAT_HAVE_CTRL_ATTR_MCAST_GROUPS

struct genl_multicast_group {
	struct genl_family	*family;	/* private */
        struct list_head	list;		/* private */
        char			name[GENL_NAMSIZ];
	u32			id;
};

static inline int genl_register_mc_group(struct genl_family *family,
					 struct genl_multicast_group *grp)
{
	grp->id = 1;
	return 0;
}

static inline void genl_unregister_mc_group(struct genl_family *family,
					    struct genl_multicast_group *grp)
{
}

#endif

#ifndef COMPAT_HAVE_IS_ERR_OR_NULL
static inline long __must_check IS_ERR_OR_NULL(const void *ptr)
{
	return !ptr || IS_ERR_VALUE((unsigned long)ptr);
}
#endif

#ifndef disk_to_dev
/* disk_to_dev was introduced with 2.6.27. Before that the kobj was directly in gendisk */
static inline struct kobject *drbd_kobj_of_disk(struct gendisk *disk)
{
	return &disk->kobj;
}
#else
static inline struct kobject *drbd_kobj_of_disk(struct gendisk *disk)
{
	return &disk_to_dev(disk)->kobj;
}
#endif

#ifndef ULLONG_MAX
/* introduced in 2.6.18 */
#define ULLONG_MAX (~0ULL)
#endif

#ifndef SK_CAN_REUSE
/* This constant was introduced by Pavel Emelyanov <xemul@parallels.com> on
   Thu Apr 19 03:39:36 2012 +0000. Before the release of linux-3.5
   commit 4a17fd52 sock: Introduce named constants for sk_reuse */
#define SK_CAN_REUSE   1
#endif

#ifndef COMPAT_HAVE_KREF_GET_UNLESS_ZERO
static inline int __must_check kref_get_unless_zero(struct kref *kref)
{
	return atomic_add_unless(&kref->refcount, 1, 0);
}
#endif

#ifndef COMPAT_HAVE_REFCOUNT_INC
#define refcount_inc(R) atomic_inc(R)
#define refcount_read(R) atomic_read(R)
#define refcount_dec_and_test(R) atomic_dec_and_test(R)
#endif

#ifndef KREF_INIT
#define KREF_INIT(N) { ATOMIC_INIT(N) }
#endif

#ifndef KOBJECT_CREATE_AND_ADD_EXPORTED
struct kobject *kobject_create_and_add(const char *name, struct kobject *parent);
int kobject_init_and_add(struct kobject *kobj, struct kobj_type *ktype,
			 struct kobject *parent, const char *name);
#endif

#ifdef COMPAT_KMAP_ATOMIC_PAGE_ONLY
/* see 980c19e3
 * highmem: mark k[un]map_atomic() with two arguments as deprecated */
#define drbd_kmap_atomic(page, km)	kmap_atomic(page)
#define drbd_kunmap_atomic(addr, km)	kunmap_atomic(addr)
#else
#define drbd_kmap_atomic(page, km)	kmap_atomic(page, km)
#define drbd_kunmap_atomic(addr, km)	kunmap_atomic(addr, km)
#endif

#ifdef COMPAT_HAVE_NETLINK_SKB_PARMS_PORTID
#define NETLINK_CB_PORTID(skb) NETLINK_CB(skb).portid
#else
#define NETLINK_CB_PORTID(skb) NETLINK_CB(skb).pid
#endif


#ifndef COMPAT_HAVE_LIST_SPLICE_TAIL_INIT
static inline void __backported_list_splice(const struct list_head *list,
					    struct list_head *prev,
					    struct list_head *next)
{
        struct list_head *first = list->next;
        struct list_head *last = list->prev;

        first->prev = prev;
        prev->next = first;

        last->next = next;
        next->prev = last;
}

static inline void list_splice_tail_init(struct list_head *list,
                                         struct list_head *head)
{
        if (!list_empty(list)) {
		__backported_list_splice(list, head->prev, head);
                INIT_LIST_HEAD(list);
        }
}
#endif

#ifndef list_first_entry
#define list_first_entry(ptr, type, member) \
	list_entry((ptr)->next, type, member)
#endif

#ifndef list_first_entry_or_null
#define list_first_entry_or_null(ptr, type, member) \
	(!list_empty(ptr) ? list_first_entry(ptr, type, member) : NULL)
#endif

#ifndef COMPAT_HAVE_IDR_ALLOC
static inline int idr_alloc(struct idr *idr, void *ptr, int start, int end, gfp_t gfp_mask)
{
	int rv, got;

	if (!idr_pre_get(idr, gfp_mask))
		return -ENOMEM;
	rv = idr_get_new_above(idr, ptr, start, &got);
	if (rv < 0)
		return rv;

	if (got >= end) {
		idr_remove(idr, got);
		return -ENOSPC;
	}

	return got;
}
#endif

#ifndef COMPAT_HAVE_IDR_FOR_EACH_ENTRY
/**
 * idr_for_each_entry - iterate over an idr's elements of a given type
 * @idp:     idr handle
 * @entry:   the type * to use as cursor
 * @id:      id entry's key
 *
 * @entry and @id do not need to be initialized before the loop, and
 * after normal terminatinon @entry is left with the value NULL.  This
 * is convenient for a "not found" value.
  */
#define idr_for_each_entry(idp, entry, id)                     \
       for (id = 0; ((entry) = idr_get_next(idp, &(id))) != NULL; ++id)
#endif

#ifndef idr_for_each_entry_continue
#define idr_for_each_entry_continue(idp, entry, id)                     \
	for (entry = (typeof(entry))idr_get_next((idp), &(id));         \
	     entry;                                                     \
	     ++id, entry = (typeof(entry))idr_get_next((idp), &(id)))
#endif

#ifndef COMPAT_HAVE_IDR_IS_EMPTY
static inline bool idr_is_empty(struct idr *idr)
{
	int id = 0;
	return idr_get_next(idr, &id) == NULL;
}
#endif

#ifndef COMPAT_HAVE_PRANDOM_U32
static inline u32 prandom_u32(void)
{
	return random32();
}
#endif

#ifndef COMPAT_HAVE_PROC_PDE_DATA
#define PDE_DATA(inode) PDE(inode)->data
#endif

#ifndef COMPAT_HAVE_TASK_PID_NR
#include <linux/sched.h>

static inline pid_t task_pid_nr(struct task_struct *tsk)
{
	return tsk->pid;
}
#endif

#ifndef for_each_cpu
# define for_each_cpu(cpu, mask) for_each_cpu_mask(cpu, mask)
#endif

#ifndef COMPAT_HAVE_CPUMASK_EMPTY
#include <linux/cpumask.h>
#define cpumask_empty(mask) cpus_empty(mask)
#endif

#ifndef blk_queue_discard
#  define blk_queue_discard(q)   (0)
#endif

#ifndef BLKDEV_ISSUE_ZEROOUT_EXPORTED
/* Was introduced with 2.6.34 */
extern int blkdev_issue_zeroout(struct block_device *bdev, sector_t sector,
				sector_t nr_sects, gfp_t gfp_mask);
#define blkdev_issue_zeroout(BDEV, SS, NS, GFP, flags /* = NOUNMAP */) \
	blkdev_issue_zeroout(BDEV, SS, NS, GFP)
#else
/* synopsis changed a few times, though */
#if  defined(BLKDEV_ZERO_NOUNMAP)
/* >= v4.12 */
/* use blkdev_issue_zeroout() as written out in the actual source code.
 * right now, we only use it with flags = BLKDEV_ZERO_NOUNMAP */
#elif defined(COMPAT_BLKDEV_ISSUE_ZEROOUT_BLKDEV_IFL_WAIT)
/* cannot yet tell it to use (or not use) discard,
 * but must tell it to be synchronous */
#define blkdev_issue_zeroout(BDEV, SS, NS, GFP, flags) \
	blkdev_issue_zeroout(BDEV, SS, NS, GFP, BLKDEV_IFL_WAIT)
#elif  defined(COMPAT_BLKDEV_ISSUE_ZEROOUT_DISCARD)
/* no BLKDEV_ZERO_NOUNMAP as last parameter, but a bool discard instead */
/* still need to define BLKDEV_ZERO_NOUNMAP, to compare against 0 */
#define BLKDEV_ZERO_NOUNMAP 1
#define blkdev_issue_zeroout(BDEV, SS, NS, GFP, flags /* = NOUNMAP */) \
	blkdev_issue_zeroout(BDEV, SS, NS, GFP, (flags) == 0 /* bool discard */)
#else /* !defined(COMPAT_BLKDEV_ISSUE_ZEROOUT_DISCARD) */
#define blkdev_issue_zeroout(BDEV, SS, NS, GFP, discard) \
	blkdev_issue_zeroout(BDEV, SS, NS, GFP)
#endif
#endif

#ifdef COMPAT_INIT_WORK_HAS_THREE_ARGUMENTS
#define COMPAT_INIT_WORK(a, b) INIT_WORK(a, b, a)
#else
#define COMPAT_INIT_WORK(a, b) INIT_WORK(a, b)
#endif

#ifndef COMPAT_HAVE_GENL_LOCK
static inline void genl_lock(void)  { }
static inline void genl_unlock(void)  { }
#endif

#ifdef COMPAT_HAVE_STRUCT_QUEUE_LIMITS
#define DRBD_QUEUE_LIMITS(q) (&(q)->limits)
#define LIMIT_TYPE struct queue_limits
#else
#define DRBD_QUEUE_LIMITS(q) (q)
#define LIMIT_TYPE struct request_queue
#endif

#ifndef COMPAT_HAVE_BLK_SET_STACKING_LIMITS
static inline void blk_set_stacking_limits(LIMIT_TYPE *lim)
{
# ifdef COMPAT_QUEUE_LIMITS_HAS_DISCARD_ZEROES_DATA
	lim->discard_zeroes_data = 1;
# endif
}
#endif

#ifdef COMPAT_HAVE_STRUCT_BVEC_ITER
/* since Linux 3.14 we have a new way to iterate a bio
   Mainline commits:
   7988613b0 block: Convert bio_for_each_segment() to bvec_iter
   4f024f379 block: Abstract out bvec iterator
 */
#define DRBD_BIO_VEC_TYPE struct bio_vec
#define DRBD_ITER_TYPE struct bvec_iter
#define BVD .
#define DRBD_BIO_BI_SECTOR(BIO) ((BIO)->bi_iter.bi_sector)
#define DRBD_BIO_BI_SIZE(BIO) ((BIO)->bi_iter.bi_size)
#else
#define DRBD_BIO_VEC_TYPE struct bio_vec *
#define DRBD_ITER_TYPE int
#define BVD ->
#define DRBD_BIO_BI_SECTOR(BIO) ((BIO)->bi_sector)
#define DRBD_BIO_BI_SIZE(BIO) ((BIO)->bi_size)

/* Attention: The backward comp version of this macro accesses bio from
   calling namespace */
#define bio_iter_last(BVEC, ITER) ((ITER) == bio->bi_vcnt - 1)
#endif

#ifndef wait_event_cmd
/* wait_event_cmd() is available since 3.12 */

#define __wait_event_cmd(wq, condition, cmd1, cmd2)			\
do  {									\
	DEFINE_WAIT(__wait);						\
									\
	for (;;) {							\
		prepare_to_wait(&wq, &__wait, TASK_UNINTERRUPTIBLE);	\
		if (condition)						\
			break;						\
		cmd1;							\
		schedule();						\
		cmd2;							\
	}								\
	finish_wait(&wq, &__wait);					\
} while (0)

#define wait_event_cmd(wq, condition, cmd1, cmd2)		\
do {								\
	if (condition)						\
		break;						\
	__wait_event_cmd(wq, condition, cmd1, cmd2);		\
} while (0)
#endif

#if defined(COMPAT_HAVE_GENERIC_START_IO_ACCT_Q_RW_SECT_PART)
/* void generic_start_io_acct(struct request_queue *q,
 *		int rw, unsigned long sectors, struct hd_struct *part); */
#elif defined(COMPAT_HAVE_GENERIC_START_IO_ACCT_RW_SECT_PART)
/* void generic_start_io_acct(
 *		int rw, unsigned long sectors, struct hd_struct *part); */
#define generic_start_io_acct(q, rw, sect, part) generic_start_io_acct(rw, sect, part)
#define generic_end_io_acct(q, rw, part, start) generic_end_io_acct(rw, part, start)

#elif defined(__disk_stat_inc)
/* too old, we don't care */
#warning "io accounting disabled"
#else

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,32)
#define part_inc_in_flight(A, B) part_inc_in_flight(A)
#define part_dec_in_flight(A, B) part_dec_in_flight(A)
#endif

static inline void generic_start_io_acct(struct request_queue *q,
		int rw, unsigned long sectors, struct hd_struct *part)
{
	int cpu;

	cpu = part_stat_lock();
	part_round_stats(cpu, part);
	part_stat_inc(cpu, part, ios[rw]);
	part_stat_add(cpu, part, sectors[rw], sectors);
	(void) cpu; /* The macro invocations above want the cpu argument, I do not like
		       the compiler warning about cpu only assigned but never used... */
	/* part_inc_in_flight(part, rw); */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,32)
	{ BUILD_BUG_ON(sizeof(atomic_t) != sizeof(part->in_flight)); }
	atomic_inc((atomic_t*)&part->in_flight);
#else
	{ BUILD_BUG_ON(sizeof(atomic_t) != sizeof(part->in_flight[0])); }
	atomic_inc((atomic_t*)&part->in_flight[rw]);
#endif
	part_stat_unlock();
}

static inline void generic_end_io_acct(struct request_queue *q,
		int rw, struct hd_struct *part, unsigned long start_time)
{
	unsigned long duration = jiffies - start_time;
	int cpu;

	cpu = part_stat_lock();
	part_stat_add(cpu, part, ticks[rw], duration);
	part_round_stats(cpu, part);
	/* part_dec_in_flight(part, rw); */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,32)
	atomic_dec((atomic_t*)&part->in_flight);
#else
	atomic_dec((atomic_t*)&part->in_flight[rw]);
#endif
	part_stat_unlock();
}
#endif /* __disk_stat_inc, COMPAT_HAVE_GENERIC_START_IO_ACCT ... */


#ifndef COMPAT_SOCK_CREATE_KERN_HAS_FIVE_PARAMETERS
#define sock_create_kern(N,F,T,P,S) sock_create_kern(F,T,P,S)
#endif

#ifndef COMPAT_HAVE_WB_CONGESTED_ENUM
#define WB_async_congested BDI_async_congested
#define WB_sync_congested BDI_sync_congested
#endif

#ifndef COMPAT_HAVE_SIMPLE_POSITIVE
#include <linux/dcache.h>
static inline int simple_positive(struct dentry *dentry)
{
        return dentry->d_inode && !d_unhashed(dentry);
}
#endif

#ifndef COMPAT_HAVE_IS_VMALLOC_ADDR
static inline int is_vmalloc_addr(const void *x)
{
#ifdef CONFIG_MMU
	unsigned long addr = (unsigned long)x;
	return addr >= VMALLOC_START && addr < VMALLOC_END;
#else
	return 0;
#endif
}
#endif

#ifndef COMPAT_HAVE_KVFREE
#include <linux/mm.h>
static inline void kvfree(void /* intentionally discarded const */ *addr)
{
	if (is_vmalloc_addr(addr))
		vfree(addr);
	else
		kfree(addr);
}
#endif

#ifdef blk_queue_plugged
/* pre 7eaceac block: remove per-queue plugging
 * Code has been converted over to the new explicit on-stack plugging ...
 *
 * provide dummy struct blk_plug and blk_start_plug/blk_finish_plug,
 * so the main code won't be cluttered with ifdef.
 */
struct blk_plug { };
#if 0
static void blk_start_plug(struct blk_plug *plug) {};
static void blk_finish_plug(struct blk_plug *plug) {};
#else
#define blk_start_plug(plug) do { (void)plug; } while (0)
#define blk_finish_plug(plug) do { } while (0)
#endif
#endif

#ifdef COMPAT_NEED_D_INODE
static inline struct inode *d_inode(struct dentry *dentry)
{
	return dentry->d_inode;
}
#endif

#ifndef COMPAT_HAVE_INODE_LOCK
/* up to kernel 2.6.38 inclusive, there was a
 * linux/writeback.h:extern spinlock_t inode_lock;
 * which was implicitly included.
 * avoid error: 'inode_lock' redeclared as different kind of symbol */
#define inode_lock(i) drbd_inode_lock(i)
static inline void inode_lock(struct inode *inode)
{
	mutex_lock(&inode->i_mutex);
}

static inline void inode_unlock(struct inode *inode)
{
	mutex_unlock(&inode->i_mutex);
}
#endif

#if !(defined(COMPAT_HAVE_AHASH_REQUEST_ON_STACK) && \
	defined(COMPAT_HAVE_SHASH_DESC_ON_STACK) &&  \
	defined(COMPAT_HAVE_SHASH_DESC_ZERO)) && \
	!defined(COMPAT_CRYPTO_BACKPORT_2_6_18_IN_USE)
#include <crypto/hash.h>

#ifndef COMPAT_HAVE_AHASH_REQUEST_ON_STACK
#define AHASH_REQUEST_ON_STACK(name, ahash) \
	char __##name##_desc[sizeof(struct ahash_request) + \
		crypto_ahash_reqsize(ahash)] CRYPTO_MINALIGN_ATTR; \
	struct ahash_request *name = (void *)__##name##_desc
#endif

#ifndef COMPAT_HAVE_SHASH_DESC_ON_STACK
#define SHASH_DESC_ON_STACK(shash, ctx)				  \
	char __##shash##_desc[sizeof(struct shash_desc) +	  \
		crypto_shash_descsize(ctx)] CRYPTO_MINALIGN_ATTR; \
	struct shash_desc *shash = (struct shash_desc *)__##shash##_desc
#endif

#ifndef COMPAT_HAVE_SHASH_DESC_ZERO
static inline void ahash_request_zero(struct ahash_request *req)
{
	/* memzero_explicit(...) */
	memset(req, 0, sizeof(*req) + crypto_ahash_reqsize(crypto_ahash_reqtfm(req)));
	barrier_data(req);
}

static inline void shash_desc_zero(struct shash_desc *desc)
{
	/* memzero_explicit(...) */
	memset(desc, 0, sizeof(*desc) + crypto_shash_descsize(desc->tfm));
	barrier_data(desc);
}
#endif
#endif

#ifndef COMPAT_HAVE_KMALLOC_ARRAY
#define kmalloc_array(a, b, c) kmalloc((a) * (b), (c))
#endif

#ifndef COMPAT_HAVE_BIO_CLONE_FAST
#define bio_clone_fast(bio, gfp, bio_set) bio_clone(bio, gfp)
#endif

#ifndef bio_set_dev
#define bio_set_dev(bio, bdev) (bio)->bi_bdev = (bdev)
#endif

#ifdef COMPAT_HAVE_TIMER_SETUP
/* starting with v4.16 new timer interface*/
#define DRBD_TIMER_FN_ARG struct timer_list *t
#define DRBD_TIMER_ARG2OBJ(OBJ, MEMBER) from_timer(OBJ, t, MEMBER)
#define drbd_timer_setup(OBJ, MEMBER, TIMER_FN) timer_setup(&OBJ->MEMBER, TIMER_FN, 0)
#define DRBD_TIMER_CALL_ARG(OBJ, MEMBER) &OBJ->MEMBER
#else
/* timer interface before v4.16 */
#define DRBD_TIMER_FN_ARG unsigned long data
#define DRBD_TIMER_ARG2OBJ(OBJ, MEMBER) (struct drbd_##OBJ *) data
#define drbd_timer_setup(OBJ, MEMBER, TIMER_FN) setup_timer(&OBJ->MEMBER, TIMER_FN, (unsigned long)OBJ)
#define DRBD_TIMER_CALL_ARG(OBJ, MEMBER) (unsigned long) OBJ
#endif

#endif
