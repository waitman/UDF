/*-
 * Copyright (c) 2012 Will DeVries
 * Copyright (c) 2006, 2008 Reinoud Zandijk
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */


#include <sys/cdefs.h>
#include <sys/endian.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/iconv.h>

#include "ecma167-udf.h"
#include "udf.h"
#include "udf_subr.h"
#include "udf_mount.h"

#define VTOI(vnode) ((struct udf_node *) (vnode)->v_data)

#if 0
#define UDF_SET_SYSTEMFILE(vp) 			\
	/* XXXAD Is the vnode locked? */	\
	(vp)->v_vflag |= VV_SYSTEM;		\
	vref(vp);				\
	vput(vp);
#endif

extern struct iconv_functions *udf2_iconv;
extern int syncer_maxdelay;     /* maximum delay time */
extern int (**udf_vnodeop_p)(void *);

static int	udf_leapyear(int year);

#if 0
#if 1

#if 0
static void
udf_dumpblob(boid *blob, uint32_t dlen)
{
	int i, j;

	printf("blob = %p\n", blob);
	printf("dump of %d bytes\n", dlen);

	for (i = 0; i < dlen; i+ = 16) {
		printf("%04x ", i);
		for (j = 0; j < 16; j++) {
			if (i+j < dlen) {
				printf("%02x ", blob[i+j]);
			} else {
				printf("   ");
			}
		}
		for (j = 0; j < 16; j++) {
			if (i+j < dlen) {
				if (blob[i+j]>32 && blob[i+j]! = 127) {
					printf("%c", blob[i+j]);
				} else {
					printf(".");
				}
			}
		}
		printf("\n");
	}
	printf("\n");
	Debugger();
}
#endif

static void
udf_dump_discinfo(struct udf_mount *ump)
{
	char   bits[128];
	struct mmc_discinfo *di = &ump->discinfo;

	if ((udf_verbose & UDF_DEBUG_VOLUMES) == 0)
		return;

	printf("Device/media info  :\n");
	printf("\tMMC profile        0x%02x\n", di->mmc_profile);
	printf("\tderived class      %d\n", di->mmc_class);
	printf("\tsector size        %d\n", di->sector_size);
	printf("\tdisc state         %d\n", di->disc_state);
	printf("\tlast ses state     %d\n", di->last_session_state);
	printf("\tbg format state    %d\n", di->bg_format_state);
	printf("\tfrst track         %d\n", di->first_track);
	printf("\tfst on last ses    %d\n", di->first_track_last_session);
	printf("\tlst on last ses    %d\n", di->last_track_last_session);
	printf("\tlink block penalty %d\n", di->link_block_penalty);
	snprintb(bits, sizeof(bits), MMC_DFLAGS_FLAGBITS, di->disc_flags);
	printf("\tdisc flags         %s\n", bits);
	printf("\tdisc id            %x\n", di->disc_id);
	printf("\tdisc barcode       %"PRIx64"\n", di->disc_barcode);

	printf("\tnum sessions       %d\n", di->num_sessions);
	printf("\tnum tracks         %d\n", di->num_tracks);

	snprintb(bits, sizeof(bits), MMC_CAP_FLAGBITS, di->mmc_cur);
	printf("\tcapabilities cur   %s\n", bits);
	snprintb(bits, sizeof(bits), MMC_CAP_FLAGBITS, di->mmc_cap);
	printf("\tcapabilities cap   %s\n", bits);
}

static void
udf_dump_trackinfo(struct mmc_trackinfo *trackinfo)
{
	char   bits[128];

	if ((udf_verbose & UDF_DEBUG_VOLUMES) == 0)
		return;

	printf("Trackinfo for track %d:\n", trackinfo->tracknr);
	printf("\tsessionnr           %d\n", trackinfo->sessionnr);
	printf("\ttrack mode          %d\n", trackinfo->track_mode);
	printf("\tdata mode           %d\n", trackinfo->data_mode);
	snprintb(bits, sizeof(bits), MMC_TRACKINFO_FLAGBITS, trackinfo->flags);
	printf("\tflags               %s\n", bits);

	printf("\ttrack start         %d\n", trackinfo->track_start);
	printf("\tnext_writable       %d\n", trackinfo->next_writable);
	printf("\tfree_blocks         %d\n", trackinfo->free_blocks);
	printf("\tpacket_size         %d\n", trackinfo->packet_size);
	printf("\ttrack size          %d\n", trackinfo->track_size);
	printf("\tlast recorded block %d\n", trackinfo->last_recorded);
}

#else
#define udf_dump_discinfo(a);
#define udf_dump_trackinfo(a);
#endif

/* not called often */
int
udf_update_discinfo(struct udf_mount *ump)
{
	struct vnode *devvp = ump->devvp;
	struct partinfo dpart;
	struct mmc_discinfo *di;
	int error;

	DPRINTF(VOLUMES, ("read/update disc info\n"));
	di = &ump->discinfo;
	memset(di, 0, sizeof(struct mmc_discinfo));

	/* check if we're on a MMC capable device, i.e. CD/DVD */
	error = VOP_IOCTL(devvp, MMCGETDISCINFO, di, FKIOCTL, NOCRED);
	if (error == 0) {
		udf_dump_discinfo(ump);
		return (0);
	}

	/* disc partition support */
	error = VOP_IOCTL(devvp, DIOCGPART, &dpart, FREAD, NOCRED);
	if (error != 0)
		return (ENODEV);

	/* set up a disc info profile for partitions */
	di->mmc_profile		= 0x01;	/* disc type */
	di->mmc_class		= MMC_CLASS_DISC;
	di->disc_state		= MMC_STATE_CLOSED;
	di->last_session_state	= MMC_STATE_CLOSED;
	di->bg_format_state	= MMC_BGFSTATE_COMPLETED;
	di->link_block_penalty	= 0;

	di->mmc_cur     = MMC_CAP_RECORDABLE | MMC_CAP_REWRITABLE |
		MMC_CAP_ZEROLINKBLK | MMC_CAP_HW_DEFECTFREE;
	di->mmc_cap    = di->mmc_cur;
	di->disc_flags = MMC_DFLAGS_UNRESTRICTED;

	/* TODO problem with last_possible_lba on resizable VND; request */
	di->last_possible_lba = dpart.part->p_size;
	di->sector_size       = dpart.disklab->d_secsize;

	di->num_sessions = 1;
	di->num_tracks   = 1;

	di->first_track  = 1;
	di->first_track_last_session = di->last_track_last_session = 1;

	udf_dump_discinfo(ump);
	return (0);
}

int
udf_update_trackinfo(struct udf_mount *ump, struct mmc_trackinfo *ti)
{
	struct vnode *devvp = ump->devvp;
	struct mmc_discinfo *di = &ump->discinfo;
	int error, class;

	DPRINTF(VOLUMES, ("read track info\n"));

	class = di->mmc_class;
	if (class != MMC_CLASS_DISC) {
		/* tracknr specified in struct ti */
		error = VOP_IOCTL(devvp, MMCGETTRACKINFO, ti, FKIOCTL, NOCRED);
		return (error);
	}

	/* disc partition support */
	if (ti->tracknr != 1)
		return (EIO);

	/* create fake ti (TODO check for resized vnds) */
	ti->sessionnr  = 1;

	ti->track_mode = 0;	/* XXX */
	ti->data_mode  = 0;	/* XXX */
	ti->flags = MMC_TRACKINFO_LRA_VALID | MMC_TRACKINFO_NWA_VALID;

	ti->track_start    = 0;
	ti->packet_size    = 1;

	/* TODO support for resizable vnd */
	ti->track_size    = di->last_possible_lba;
	ti->next_writable = di->last_possible_lba;
	ti->last_recorded = ti->next_writable;
	ti->free_blocks   = 0;

	return (0);
}

int
udf_setup_writeparams(struct udf_mount *ump)
{
	struct mmc_writeparams mmc_writeparams;
	int error;

	if (ump->discinfo.mmc_class == MMC_CLASS_DISC)
		return (0);

	/*
	 * only CD burning normally needs setting up, but other disc types
	 * might need other settings to be made. The MMC framework will set up
	 * the nessisary recording parameters according to the disc
	 * characteristics read in. Modifications can be made in the discinfo
	 * structure passed to change the nature of the disc.
	 */

	memset(&mmc_writeparams, 0, sizeof(struct mmc_writeparams));
	mmc_writeparams.mmc_class  = ump->discinfo.mmc_class;
	mmc_writeparams.mmc_cur    = ump->discinfo.mmc_cur;

	/*
	 * UDF dictates first track to determine track mode for the whole
	 * disc. [UDF 1.50/6.10.1.1, UDF 1.50/6.10.2.1]
	 * To prevent problems with a `reserved' track in front we start with
	 * the 2nd track and if that is not valid, go for the 1st.
	 */
	mmc_writeparams.tracknr = 2;
	mmc_writeparams.data_mode  = MMC_DATAMODE_DEFAULT;	/* XA disc */
	mmc_writeparams.track_mode = MMC_TRACKMODE_DEFAULT;	/* data */

	error = VOP_IOCTL(ump->devvp, MMCSETUPWRITEPARAMS, &mmc_writeparams,
			FKIOCTL, NOCRED);
	if (error != 0) {
		mmc_writeparams.tracknr = 1;
		error = VOP_IOCTL(ump->devvp, MMCSETUPWRITEPARAMS,
				&mmc_writeparams, FKIOCTL, NOCRED);
	}
	return (error);
}

int
udf_synchronise_caches(struct udf_mount *ump)
{
	struct mmc_op mmc_op;

	DPRINTF(CALL, ("udf_synchronise_caches()\n"));

	if (ump->vfs_mountp->mnt_flag & MNT_RDONLY)
		return (0);

	/* discs are done now */
	if (ump->discinfo.mmc_class == MMC_CLASS_DISC)
		return (0);

	memset(&mmc_op, 0, sizeof(struct mmc_op));
	mmc_op.operation = MMC_OP_SYNCHRONISECACHE;

	/* ignore return code */
	(void) VOP_IOCTL(ump->devvp, MMCOP, &mmc_op, FKIOCTL, NOCRED);

	return (0);
}

/* track/session searching for mounting */
int
udf_search_tracks(struct udf_mount *ump, struct udf_args *args,
		  int *first_tracknr, int *last_tracknr)
{
	struct mmc_trackinfo trackinfo;
	uint32_t tracknr, start_track, num_tracks;
	int error;

	/* if negative, sessionnr is relative to last session */
	if (args->sessionnr < 0) {
		args->sessionnr += ump->discinfo.num_sessions;
	}

	/* sanity */
	if (args->sessionnr < 0)
		args->sessionnr = 0;
	if (args->sessionnr > ump->discinfo.num_sessions)
		args->sessionnr = ump->discinfo.num_sessions;

	/* search the tracks for this session, zero session nr indicates last */
	if (args->sessionnr == 0)
		args->sessionnr = ump->discinfo.num_sessions;
	if (ump->discinfo.last_session_state == MMC_STATE_EMPTY)
		args->sessionnr--;

	/* sanity again */
	if (args->sessionnr < 0)
		args->sessionnr = 0;

	/* search the first and last track of the specified session */
	num_tracks  = ump->discinfo.num_tracks;
	start_track = ump->discinfo.first_track;

	/* search for first track of this session */
	for (tracknr = start_track; tracknr <= num_tracks; tracknr++) {
		/* get track info */
		trackinfo.tracknr = tracknr;
		error = udf_update_trackinfo(ump, &trackinfo);
		if (error != 0)
			return (error);

		if (trackinfo.sessionnr == args->sessionnr)
			break;
	}
	*first_tracknr = tracknr;

	/* search for last track of this session */
	for (;tracknr <= num_tracks; tracknr++) {
		/* get track info */
		trackinfo.tracknr = tracknr;
		error = udf_update_trackinfo(ump, &trackinfo);
		if (error || (trackinfo.sessionnr != args->sessionnr)) {
			tracknr--;
			break;
		}
	}
	if (tracknr > num_tracks)
		tracknr--;

	*last_tracknr = tracknr;

	if (*last_tracknr < *first_tracknr) {
		printf( "udf_search_tracks: sanity check on drive+disc failed, "
			"drive returned garbage\n");
		return (EINVAL);
	}

	assert(*last_tracknr >= *first_tracknr);
	return (0);
}

/*
 * NOTE: this is the only routine in this file that directly peeks into the
 * metadata file but since its at a larval state of the mount it can't hurt.
 *
 * XXX candidate for udf_allocation.c
 * XXX clean me up!, change to new node reading code.
 */

static void
udf_check_track_metadata_overlap(struct udf_mount *ump,
	struct mmc_trackinfo *trackinfo)
{
	struct part_desc *part;
	struct file_entry      *fe;
	struct extfile_entry   *efe;
	struct short_ad        *s_ad;
	struct long_ad         *l_ad;
	uint32_t track_start, track_end;
	uint32_t phys_part_start, phys_part_end, part_start, part_end;
	uint32_t sector_size, len, alloclen, plb_num;
	uint8_t *pos;
	int addr_type, icblen, icbflags, flags;

	/* get our track extents */
	track_start = trackinfo->track_start;
	track_end   = track_start + trackinfo->track_size;

	/* get our base partition extent */
	KASSERT(ump->node_part == ump->fids_part);
	part = ump->partitions[ump->node_part];
	phys_part_start = le32toh(part->start_loc);
	phys_part_end   = phys_part_start + le32toh(part->part_len);

	/* no use if its outside the physical partition */
	if ((phys_part_start >= track_end) || (phys_part_end < track_start))
		return;

	/*
	 * now follow all extents in the fe/efe to see if they refer to this
	 * track
	 */

	sector_size = ump->discinfo.sector_size;

	/* XXX should we claim exclusive access to the metafile ? */
	/* TODO: move to new node read code */
	fe  = ump->metadata_node->fe;
	efe = ump->metadata_node->efe;
	if (fe != NULL) {
		alloclen = le32toh(fe->l_ad);
		pos      = &fe->data[0] + le32toh(fe->l_ea);
		icbflags = le16toh(fe->icbtag.flags);
	} else {
		assert(efe != NULL);
		alloclen = le32toh(efe->l_ad);
		pos      = &efe->data[0] + le32toh(efe->l_ea);
		icbflags = le16toh(efe->icbtag.flags);
	}
	addr_type = icbflags & UDF_ICB_TAG_FLAGS_ALLOC_MASK;

	while (alloclen) {
		if (addr_type == UDF_ICB_SHORT_ALLOC) {
			icblen = sizeof(struct short_ad);
			s_ad   = (struct short_ad *) pos;
			len        = le32toh(s_ad->len);
			plb_num    = le32toh(s_ad->lb_num);
		} else {
			/* should not be present, but why not */
			icblen = sizeof(struct long_ad);
			l_ad   = (struct long_ad *) pos;
			len        = le32toh(l_ad->len);
			plb_num    = le32toh(l_ad->loc.lb_num);
			/* pvpart_num = le16toh(l_ad->loc.part_num); */
		}
		/* process extent */
		flags   = UDF_EXT_FLAGS(len);
		len     = UDF_EXT_LEN(len);

		part_start = phys_part_start + plb_num;
		part_end   = part_start + (len / sector_size);

		if ((part_start >= track_start) && (part_end <= track_end)) {
			/* extent is enclosed within this track */
			ump->metadata_track = *trackinfo;
			return;
		}

		pos        += icblen;
		alloclen   -= icblen;
	}
}

int
udf_search_writing_tracks(struct udf_mount *ump)
{
	struct vnode *devvp = ump->devvp;
	struct mmc_trackinfo trackinfo;
	struct mmc_op        mmc_op;
	struct part_desc *part;
	uint32_t tracknr, start_track, num_tracks;
	uint32_t track_start, track_end, part_start, part_end;
	int node_alloc, error;

	/*
	 * in the CD/(HD)DVD/BD recordable device model a few tracks within
	 * the last session might be open but in the UDF device model at most
	 * three tracks can be open: a reserved track for delayed ISO VRS
	 * writing, a data track and a metadata track. We search here for the
	 * data track and the metadata track. Note that the reserved track is
	 * troublesome but can be detected by its small size of < 512 sectors.
	 */

	/* update discinfo since it might have changed */
	error = udf_update_discinfo(ump);
	if (error != 0)
		return (error);

	num_tracks  = ump->discinfo.num_tracks;
	start_track = ump->discinfo.first_track;

	/* fetch info on first and possibly only track */
	trackinfo.tracknr = start_track;
	error = udf_update_trackinfo(ump, &trackinfo);
	if (error != 0)
		return (error);

	/* copy results to our mount point */
	ump->data_track     = trackinfo;
	ump->metadata_track = trackinfo;

	/* if not sequential, we're done */
	if (num_tracks == 1)
		return (0);

	for (tracknr = start_track;tracknr <= num_tracks; tracknr++) {
		/* get track info */
		trackinfo.tracknr = tracknr;
		error = udf_update_trackinfo(ump, &trackinfo);
		if (error != 0)
			return (error);

		/*
		 * If this track is marked damaged, ask for repair. This is an
		 * optional command, so ignore its error but report warning.
		 */
		if (trackinfo.flags & MMC_TRACKINFO_DAMAGED) {
			memset(&mmc_op, 0, sizeof(mmc_op));
			mmc_op.operation   = MMC_OP_REPAIRTRACK;
			mmc_op.mmc_profile = ump->discinfo.mmc_profile;
			mmc_op.tracknr     = tracknr;
			error = VOP_IOCTL(devvp, MMCOP, &mmc_op, FKIOCTL, NOCRED);
			if (error != 0)
				(void)printf("Drive can't explicitly repair "
					"damaged track %d, but it might "
					"autorepair\n", tracknr);

			/* reget track info */
			error = udf_update_trackinfo(ump, &trackinfo);
			if (error != 0)
				return (error);
		}
		if ((trackinfo.flags & MMC_TRACKINFO_NWA_VALID) == 0)
			continue;

		track_start = trackinfo.track_start;
		track_end   = track_start + trackinfo.track_size;

		/* check for overlap on data partition */
		part = ump->partitions[ump->data_part];
		part_start = le32toh(part->start_loc);
		part_end   = part_start + le32toh(part->part_len);
		if ((part_start < track_end) && (part_end > track_start)) {
			ump->data_track = trackinfo;
			/* TODO check if UDF partition data_part is writable */
		}

		/* check for overlap on metadata partition */
		node_alloc = ump->vtop_alloc[ump->node_part];
		if ((node_alloc == UDF_ALLOC_METASEQUENTIAL) ||
		    (node_alloc == UDF_ALLOC_METABITMAP)) {
			udf_check_track_metadata_overlap(ump, &trackinfo);
		} else {
			ump->metadata_track = trackinfo;
		}
	}

	if ((ump->data_track.flags & MMC_TRACKINFO_NWA_VALID) == 0)
		return (EROFS);

	if ((ump->metadata_track.flags & MMC_TRACKINFO_NWA_VALID) == 0)
		return (EROFS);

	return (0);
}
#endif

/*
 * Check if the blob starts with a good UDF tag. Tags are protected by a
 * checksum over the reader except one byte at position 4 that is the checksum
 * itself.
 */
int
udf_check_tag(void *blob)
{
	struct desc_tag *tag = blob;
	uint8_t cnt, *pos, sum;

	/* check TAG header checksum */
	pos = (uint8_t *)tag;
	sum = 0;

	for (cnt = 0; cnt < 16; cnt++) {
		if (cnt != 4)
			sum += *pos;
		pos++;
	}
	if (sum != tag->cksum) {
		/* bad tag header checksum; this is not a valid tag */
		return (EINVAL);
	}

	return (0);
}

/*
 * check tag payload will check descriptor CRC as specified.
 * If the descriptor is too long, it will return EIO otherwise EINVAL.
 */
int
udf_check_tag_payload(void *blob, uint32_t max_length)
{
	struct desc_tag *tag = blob;
	uint16_t crc, crc_len;

	crc_len = le16toh(tag->desc_crc_len);

	/* check payload CRC if applicable */
	if (crc_len == 0)
		return (0);

	if (crc_len > max_length)
		return (EIO);

	crc = udf_cksum(((uint8_t *)tag) + UDF_DESC_TAG_LENGTH, crc_len);
	if (crc != le16toh(tag->desc_crc)) {
		/* bad payload CRC; this is a broken tag */
		return (EINVAL);
	}

	return (0);
}

void
udf_validate_tag_sum(void *blob)
{
	struct desc_tag *tag = blob;
	uint8_t cnt, *pos, sum;

	/* calculate TAG header checksum */
	pos = (uint8_t *)tag;
	sum = 0;

	for (cnt = 0; cnt < 16; cnt++) {
		if (cnt != 4) sum += *pos;
		pos++;
	}
	tag->cksum = sum;	/* 8 bit */
}

/* assumes sector number of descriptor to be saved already present */
void
udf_validate_tag_and_crc_sums(void *blob)
{
	struct desc_tag *tag = blob;
	uint16_t crc, crc_len;
	uint8_t *btag = (uint8_t *)tag;

	crc_len = le16toh(tag->desc_crc_len);

	/* check payload CRC if applicable */
	if (crc_len > 0) {
		crc = udf_cksum(btag + UDF_DESC_TAG_LENGTH, crc_len);
		tag->desc_crc = htole16(crc);
	}

	/* calculate TAG header checksum */
	udf_validate_tag_sum(blob);
}

/*
 * XXX note the different semantics from udfclient: for FIDs it still rounds
 * up to sectors. Use udf_fidsize() for a correct length.
 */
int
udf_tagsize(union dscrptr *dscr, uint32_t lb_size)
{
	uint32_t elmsz, num_lb, size, tag_id;

	tag_id = le16toh(dscr->tag.id);

	switch (tag_id) {
	case TAGID_LOGVOL:
		size = sizeof(struct logvol_desc) - 1;
		size += le32toh(dscr->lvd.mt_l);
		break;
	case TAGID_UNALLOC_SPACE:
		elmsz = sizeof(struct extent_ad);
		size = sizeof(struct unalloc_sp_desc) - elmsz;
		size += le32toh(dscr->usd.alloc_desc_num) * elmsz;
		break;
	case TAGID_FID:
		size = UDF_FID_SIZE + dscr->fid.l_fi + le16toh(dscr->fid.l_iu);
		size = (size + 3) & ~3;
		break;
	case TAGID_LOGVOL_INTEGRITY:
		size = sizeof(struct logvol_int_desc) - sizeof(uint32_t);
		size += le32toh(dscr->lvid.l_iu);
		size += (2 * le32toh(dscr->lvid.num_part) * sizeof(uint32_t));
		break;
	case TAGID_SPACE_BITMAP:
		size = sizeof(struct space_bitmap_desc) - 1;
		size += le32toh(dscr->sbd.num_bytes);
		break;
	case TAGID_SPARING_TABLE:
		elmsz = sizeof(struct spare_map_entry);
		size = sizeof(struct udf_sparing_table) - elmsz;
		size += le16toh(dscr->spt.rt_l) * elmsz;
		break;
	case TAGID_FENTRY:
		size = sizeof(struct file_entry);
		size += le32toh(dscr->fe.l_ea) + le32toh(dscr->fe.l_ad)-1;
		break;
	case TAGID_EXTFENTRY:
		size = sizeof(struct extfile_entry);
		size += le32toh(dscr->efe.l_ea) + le32toh(dscr->efe.l_ad)-1;
		break;
	case TAGID_FSD:
		size = sizeof(struct fileset_desc);
		break;
	default:
		size = sizeof(union dscrptr);
		break;
	}

	if ((size == 0) || (lb_size == 0))
		return (0);

	if (lb_size == 1)
		return (size);

	/* round up in sectors */
	num_lb = (size + lb_size -1) / lb_size;

	return (num_lb * lb_size);
}

int
udf_fidsize(struct fileid_desc *fid)
{
	uint32_t size;

	if (le16toh(fid->tag.id) != TAGID_FID)
		panic("got udf_fidsize on non FID\n");

	size = UDF_FID_SIZE + fid->l_fi + le16toh(fid->l_iu);
	size = (size + 3) & ~3;

	return (size);
}

void
udf_lock_node(struct udf_node *udf_node, int flag, char const *fname, 
    const int lineno)
{
#if 0
	int ret;

	mutex_enter(&udf_node->node_mutex);
	/* wait until free */
	while (udf_node->i_flags & IN_LOCKED) {
		ret = cv_timedwait(&udf_node->node_lock, &udf_node->node_mutex, hz/8);
		/* TODO check if we should return error; abort */
		if (ret == EWOULDBLOCK) {
			DPRINTF(LOCKING, ( "udf_lock_node: udf_node %p would block "
				"wanted at %s:%d, previously locked at %s:%d\n",
				udf_node, fname, lineno, 
				udf_node->lock_fname, udf_node->lock_lineno));
		}
	}
	/* grab */
	udf_node->i_flags |= IN_LOCKED | flag;
	/* debug */
	udf_node->lock_fname  = fname;
	udf_node->lock_lineno = lineno;

	mutex_exit(&udf_node->node_mutex);
#endif
}

void
udf_unlock_node(struct udf_node *udf_node, int flag)
{
#if 0
	mutex_enter(&udf_node->node_mutex);
	udf_node->i_flags &= ~(IN_LOCKED | flag);
	cv_broadcast(&udf_node->node_lock);
	mutex_exit(&udf_node->node_mutex);
#endif
}

static int
udf_read_anchor(struct udf_mount *ump, uint32_t sector, struct anchor_vdp **dst)
{
	int error;

	error = udf_read_phys_dscr(ump, sector, M_UDFTEMP,
	    (union dscrptr **)dst);
	if (error == 0) {
		/* blank terminator blocks are not allowed here */
		if (*dst == NULL)
			return (ENOENT);
		if (le16toh((*dst)->tag.id) != TAGID_ANCHOR) {
			error = ENOENT;
			free(*dst, M_UDFTEMP);
			*dst = NULL;
		}
	}

	return (error);
}

void
udf_print_anchors(struct udf_mount *ump)
{
	struct anchor_vdp *a;
	int i = 0;

	for (i = 0; ump->anchors[i] != NULL; i++) ;

	printf("Number of anchors: %d\n", i);

	for (i = 0; i < UDF_ANCHORS && ump->anchors[i] != NULL; i++) {
		a = ump->anchors[i];
		printf("\tTag ID: %d\n",a->tag.id);
		printf("\tDescriptor Version: %d\n",a->tag.descriptor_ver);
		printf("\tTag Serial number: %d\n", a->tag.serial_num);
		printf("\tTag Location: %d\n", a->tag.tag_loc);

		printf("\tmain volumn descriptor seq extent: %d, len: %d\n",
			a->main_vds_ex.loc, a->main_vds_ex.len);
		
		printf("\tmain volumn descriptor seq extent: %d, len: %d\n",
			a->reserve_vds_ex.loc, a->reserve_vds_ex.len);
	}
}

#if 0
int 
udf_last_session_info()
{
	struct udf_args *args = &ump->mount_args;
	struct mmc_trackinfo first_track;
	struct mmc_trackinfo second_track;
	struct mmc_trackinfo last_track;
	uint32_t track_start;
	uint32_t track_end;
	int first_tracknr, last_tracknr;
	int error;

	/* search the first and last track of the specified session */
	error = udf_search_tracks(ump, args, &first_tracknr, &last_tracknr);
	if (error == 0) {
		first_track.tracknr = first_tracknr;
		error = udf_update_trackinfo(ump, &first_track);
	}
	if (error == 0) {
		last_track.tracknr = last_tracknr;
		error = udf_update_trackinfo(ump, &last_track);
	}
	if ((!error) && (first_tracknr != last_tracknr)) {
		second_track.tracknr = first_tracknr+1;
		error = udf_update_trackinfo(ump, &second_track);
	}
	if (error != 0) {
		printf("UDF mount: reading disc geometry failed\n");
		return (0);
	}

	track_start = first_track.track_start;

	/* `end' is not as straitforward as start. */
	track_end =   last_track.track_start
		    + last_track.track_size - last_track.free_blocks - 1;

	if (ump->discinfo.mmc_cur & MMC_CAP_SEQUENTIAL) {
		/* end of track is not straitforward here */
		if (last_track.flags & MMC_TRACKINFO_LRA_VALID)
			track_end = last_track.last_recorded;
		else if (last_track.flags & MMC_TRACKINFO_NWA_VALID)
			track_end = last_track.next_writable
				    - ump->discinfo.link_block_penalty;
	}

	/* its no use reading a blank track */
	if (first_track.flags & MMC_TRACKINFO_BLANK)
		ump->first_trackblank = 1;

	/* get our packet size */
	ump->packet_size = first_track.packet_size;
	if (first_track.flags & MMC_TRACKINFO_BLANK)
		ump->packet_size = second_track.packet_size;

	if (ump->packet_size <= 1) {
		/* take max, but not bigger than 64 */
		ump->packet_size = MAXPHYS / ump->discinfo.sector_size;
		ump->packet_size = MIN(ump->packet_size, 64);
	}
	KASSERT(ump->packet_size >= 1);

	/* VATs are only recorded on sequential media, but initialise */
	ump->first_possible_vat_location = track_start + 2;
	ump->last_possible_vat_location  = track_end + last_track.packet_size;
	ump->session_start = track_start;
	ump->session_end = track_end
}
#endif

int
udf_read_anchors(struct udf_mount *ump)
{
	struct anchor_vdp **anchorsp;
	int first_anchor, anch, error, ok;
	uint32_t positions[4], session_end, session_start;

	session_start = ump->session_start;
	session_end = ump->session_end;

	/* read anchors start+256, start+512, end-256, end */
	positions[0] = session_start + 256;
	positions[1] = session_end - 256;
	positions[2] = session_end;
	/* XXX shouldn't +512 be prefered above +256 for compat with Roxio CD */
	positions[3] = session_start + 512; /* [UDF 2.60/6.11.2] */

	ok = 0;
	anchorsp = ump->anchors;
	first_anchor = 0;
	if (ump->first_trackblank)
		first_anchor = 1;
	for (anch = first_anchor; anch < 4; anch++) {
		if (positions[anch] <= session_end) {
			error = udf_read_anchor(ump, positions[anch], anchorsp);
			if (error == 0) {
				anchorsp++;
				ok++;
			} else if (anch == 2) {
				printf("UDF mount: No anchor at end of volumn."
				    "(This is not an error in the volumn.)\n");
			}
		}
	}

	return (ok);
}

#if 0
int
udf_get_c_type(struct udf_node *udf_node)
{
	int isdir, what;

	isdir  = (udf_node->vnode->v_type == VDIR);
	what   = isdir ? UDF_C_FIDS : UDF_C_USERDATA;

	if (udf_node->ump)
		if (udf_node == udf_node->ump->metadatabitmap_node)
			what = UDF_C_METADATA_SBM;

	return (what);
}

int
udf_get_record_vpart(struct udf_mount *ump, int udf_c_type)
{
	int vpart_num;

	vpart_num = ump->data_part;
	if (udf_c_type == UDF_C_NODE)
		vpart_num = ump->node_part;
	if (udf_c_type == UDF_C_FIDS)
		vpart_num = ump->fids_part;

	return (vpart_num);
}
#endif

/* we dont try to be smart; we just record the parts */
#define UDF_UPDATE_DSCR(name, dscr) \
	if (name) \
		free(name, M_UDFTEMP); \
	name = dscr;

static int
udf_process_vds_descriptor(struct udf_mount *ump, union dscrptr *dscr)
{
	struct part_desc *part;
	uint16_t phys_part, raw_phys_part;

	switch (le16toh(dscr->tag.id)) {
	case TAGID_PRI_VOL:		/* primary partition */
		UDF_UPDATE_DSCR(ump->primary_vol, &dscr->pvd);
		break;
	case TAGID_LOGVOL:		/* logical volume */
		UDF_UPDATE_DSCR(ump->logical_vol, &dscr->lvd);
		break;
	case TAGID_UNALLOC_SPACE:	/* unallocated space */
		UDF_UPDATE_DSCR(ump->unallocated, &dscr->usd);
		break;
	case TAGID_IMP_VOL:		/* implementation */
		/* XXX do we care about multiple impl. descr ? */
		UDF_UPDATE_DSCR(ump->implementation, &dscr->ivd);
		break;
	case TAGID_PARTITION:		/* physical partition */
		/* not much use if its not allocated */
		if ((le16toh(dscr->pd.flags) & UDF_PART_FLAG_ALLOCATED) == 0) {
			free(dscr, M_UDFTEMP);
			break;
		}

		/*
		 * BUGALERT: some rogue implementations use random physical
		 * partition numbers to break other implementations so lookup
		 * the number.
		 */
		raw_phys_part = le16toh(dscr->pd.part_num);
		for (phys_part = 0; phys_part < UDF_PARTITIONS; phys_part++) {
			part = ump->partitions[phys_part];
			if (part == NULL)
				break;
			if (le16toh(part->part_num) == raw_phys_part)
				break;
		}
		if (phys_part == UDF_PARTITIONS) {
			free(dscr, M_UDFTEMP);
			return (EINVAL);
		}

		UDF_UPDATE_DSCR(ump->partitions[phys_part], &dscr->pd);
		break;
	case TAGID_VOL:		/* volume space extender; rare	*/
		free(dscr, M_UDFTEMP);
		break;
	default:
		free(dscr, M_UDFTEMP);
	}

	return (0);
}
#undef UDF_UPDATE_DSCR

static int
udf_read_vds_extent(struct udf_mount *ump, uint32_t loc, uint32_t len)
{
	union dscrptr *dscr;
	int error;
	uint32_t dscr_size, sector_size;

	sector_size = ump->sector_size;

	/* loc is sectornr, len is in bytes */
	error = EIO;
	while (len) {
		error = udf_read_phys_dscr(ump, loc, M_UDFTEMP, &dscr);
		if (error != 0) {
			if (!dscr)
				free(dscr, M_UDFTEMP);
			return (error);
		}

		/* blank block is a terminator */
		if (dscr == NULL)
			return (0);

		/* TERM descriptor is a terminator */
		if (le16toh(dscr->tag.id) == TAGID_TERM) {
			free(dscr, M_UDFTEMP);
			return (0);
		}

		/* process all others */
		dscr_size = udf_tagsize(dscr, sector_size);

		/* dscr is assigned into ump */
		error = udf_process_vds_descriptor(ump, dscr);
		if (error != 0) 
			break;
		/* assert((dscr_size % sector_size) == 0); */

		len -= dscr_size;
		loc += dscr_size / sector_size;
	}

	return (error);
}

int
udf_read_vds_space(struct udf_mount *ump)
{
	/* struct udf_args *args = &ump->mount_args; */
	struct anchor_vdp *anchor, *anchor2;
	size_t size;
	int error;
	uint32_t main_len, main_loc, reserve_len, reserve_loc;

	/*
	 * read in VDS space provided by the anchors; if one descriptor read
	 * fails, try the mirror sector.
	 *
	 * check if 2nd anchor is different from 1st; if so, go for 2nd. This
	 * avoids the `compatibility features' of DirectCD that may confuse
	 * stuff completely.
	 */

	anchor = ump->anchors[0];
	anchor2 = ump->anchors[1];

	if (anchor2) {
		size = sizeof(struct extent_ad);
		if (memcmp(&anchor->main_vds_ex, &anchor2->main_vds_ex, size))
			anchor = anchor2;
		/* reserve is specified to be a literal copy of main */
	}

	main_loc = le32toh(anchor->main_vds_ex.loc);
	main_len = le32toh(anchor->main_vds_ex.len);

	reserve_loc = le32toh(anchor->reserve_vds_ex.loc);
	reserve_len = le32toh(anchor->reserve_vds_ex.len);

	error = udf_read_vds_extent(ump, main_loc, main_len);
	if (error != 0) {
		printf("UDF mount: reading in reserve VDS extent\n");
		error = udf_read_vds_extent(ump, reserve_loc, reserve_len);
	}

	return (error);
}

/*
 * Read in the logical volume integrity sequence pointed to by our logical
 * volume descriptor. Its a sequence that can be extended using fields in the
 * integrity descriptor itself. On sequential media only one is found, on
 * rewritable media a sequence of descriptors can be found as a form of
 * history keeping and on non sequential write-once media the chain is vital
 * to allow more and more descriptors to be written. The last descriptor
 * written in an extent needs to claim space for a new extent.
 */

static int
udf_retrieve_lvint(struct udf_mount *ump)
{
	union dscrptr *dscr;
	struct logvol_int_desc *lvint;
	struct udf_lvintq *trace;
	int dscr_type, error, trace_len;
	uint32_t lbnum, lb_size, len;

	lb_size = le32toh(ump->logical_vol->lb_size);
	len = le32toh(ump->logical_vol->integrity_seq_loc.len);
	lbnum = le32toh(ump->logical_vol->integrity_seq_loc.loc);

	/* clean trace */
	memset(ump->lvint_trace, 0,
	    UDF_LVDINT_SEGMENTS * sizeof(struct udf_lvintq));

	trace_len = 0;
	trace = ump->lvint_trace;
	trace->start = lbnum;
	trace->end = lbnum + len / lb_size;
	trace->pos = 0;
	trace->wpos = 0;

	lvint = NULL;
	dscr = NULL;
	error = 0;
	while (len) {
		trace->pos  = lbnum - trace->start;
		trace->wpos = trace->pos + 1;

		/* read in our integrity descriptor */
		error = udf_read_phys_dscr(ump, lbnum, M_UDFTEMP, &dscr);
		if (error == 0) {
			if (dscr == NULL) {
				trace->wpos = trace->pos;
				break;		/* empty terminates */
			}
			dscr_type = le16toh(dscr->tag.id);
			if (dscr_type == TAGID_TERM) {
				trace->wpos = trace->pos;
				break;		/* clean terminator */
			}
			if (dscr_type != TAGID_LOGVOL_INTEGRITY) {
				/* fatal... corrupt disc */
				error = ENOENT;
				break;
			}
			if (lvint)
				free(lvint, M_UDFTEMP);
			lvint = &dscr->lvid;
			dscr = NULL;
		} /* else hope for the best... maybe the next is ok */

		/* proceed sequential */
		lbnum += 1;
		len -= lb_size;

		/* are we linking to a new piece? */
		if (dscr && lvint->next_extent.len) {
			len = le32toh(lvint->next_extent.len);
			lbnum = le32toh(lvint->next_extent.loc);

			if (trace_len >= UDF_LVDINT_SEGMENTS - 1) {
				/* IEK! segment link full... */
				error = EINVAL;
			} else {
				trace++;
				trace_len++;

				trace->start = lbnum;
				trace->end = lbnum + len / lb_size;
				trace->pos = 0;
				trace->wpos = 0;
			}
		}
	}

	/* clean up the mess, esp. when there is an error */
	if (dscr)
		free(dscr, M_UDFTEMP);

	if (error != 0 && lvint != NULL) {
		free(lvint, M_UDFTEMP);
		lvint = NULL;
	}

	if (lvint == NULL)
		error = ENOENT;

	ump->logvol_integrity = lvint;

	return (error);
}

#if 0
static int
udf_loose_lvint_history(struct udf_mount *ump)
{
	union dscrptr **bufs, *dscr, *last_dscr;
	struct udf_lvintq *trace, *in_trace, *out_trace;
	struct logvol_int_desc *lvint;
	uint32_t in_ext, in_pos, in_len;
	uint32_t out_ext, out_wpos, out_len;
	uint32_t lb_size, packet_size, lb_num;
	uint32_t len, start;
	int ext, minext, extlen, cnt, cpy_len, dscr_type;
	int losing;
	int error;

	DPRINTF(VOLUMES, ("need to lose some lvint history\n"));

	lb_size = le32toh(ump->logical_vol->lb_size);
	packet_size = ump->data_track.packet_size;	/* XXX data track */

	/* search smallest extent */
	trace = &ump->lvint_trace[0];
	minext = trace->end - trace->start;
	for (ext = 1; ext < UDF_LVDINT_SEGMENTS; ext++) {
		trace = &ump->lvint_trace[ext];
		extlen = trace->end - trace->start;
		if (extlen == 0)
			break;
		minext = MIN(minext, extlen);
	}
	losing = MIN(minext, UDF_LVINT_LOSSAGE);
	/* no sense wiping all */
	if (losing == minext)
		losing--;

	DPRINTF(VOLUMES, ("\tlosing %d entries\n", losing));

	/* get buffer for pieces */
	bufs = malloc(UDF_LVDINT_SEGMENTS * sizeof(void *), M_UDFTEMP, M_WAITOK);

	in_ext    = 0;
	in_pos    = losing;
	in_trace  = &ump->lvint_trace[in_ext];
	in_len    = in_trace->end - in_trace->start;
	out_ext   = 0;
	out_wpos  = 0;
	out_trace = &ump->lvint_trace[out_ext];
	out_len   = out_trace->end - out_trace->start;

	last_dscr = NULL;
	for (;;) {
		out_trace->pos  = out_wpos;
		out_trace->wpos = out_trace->pos;
		if (in_pos >= in_len) {
			in_ext++;
			in_pos = 0;
			in_trace = &ump->lvint_trace[in_ext];
			in_len   = in_trace->end - in_trace->start;
		}
		if (out_wpos >= out_len) {
			out_ext++;
			out_wpos = 0;
			out_trace = &ump->lvint_trace[out_ext];
			out_len   = out_trace->end - out_trace->start;
		}
		/* copy overlap contents */
		cpy_len = MIN(in_len - in_pos, out_len - out_wpos);
		cpy_len = MIN(cpy_len, in_len - in_trace->pos);
		if (cpy_len == 0)
			break;

		/* copy */
		DPRINTF(VOLUMES, ("\treading %d lvid descriptors\n", cpy_len));
		for (cnt = 0; cnt < cpy_len; cnt++) {
			/* read in our integrity descriptor */
			lb_num = in_trace->start + in_pos + cnt;
			error = udf_read_phys_dscr(ump, lb_num, M_UDFTEMP,
				&dscr);
			if (error != 0) {
				/* copy last one */
				dscr = last_dscr;
			}
			bufs[cnt] = dscr;
			if (error == 0) {
				if (dscr == NULL) {
					out_trace->pos  = out_wpos + cnt;
					out_trace->wpos = out_trace->pos;
					break;		/* empty terminates */
				}
				dscr_type = le16toh(dscr->tag.id);
				if (dscr_type == TAGID_TERM) {
					out_trace->pos  = out_wpos + cnt;
					out_trace->wpos = out_trace->pos;
					break;		/* clean terminator */
				}
				if (dscr_type != TAGID_LOGVOL_INTEGRITY) {
					panic(  "UDF integrity sequence "
						"corrupted while mounted!\n");
				}
				last_dscr = dscr;
			}
		}

		/* patch up if first entry was on error */
		if (bufs[0] == NULL) {
			for (cnt = 0; cnt < cpy_len; cnt++)
				if (bufs[cnt] != NULL)
					break;
			last_dscr = bufs[cnt];
			for (; cnt > 0; cnt--) {
				bufs[cnt] = last_dscr;
			}
		}

		/* glue + write out */
		DPRINTF(VOLUMES, ("\twriting %d lvid descriptors\n", cpy_len));
		for (cnt = 0; cnt < cpy_len; cnt++) {
			lb_num = out_trace->start + out_wpos + cnt;
			lvint  = &bufs[cnt]->lvid;

			/* set continuation */
			len = 0;
			start = 0;
			if (out_wpos + cnt == out_len) {
				/* get continuation */
				trace = &ump->lvint_trace[out_ext+1];
				len   = trace->end - trace->start;
				start = trace->start;
			}
			lvint->next_extent.len = htole32(len);
			lvint->next_extent.loc = htole32(start);

			lb_num = trace->start + trace->wpos;
			error = udf_write_phys_dscr_sync(ump, NULL, UDF_C_DSCR,
				bufs[cnt], lb_num, lb_num);
			DPRINTFIF(VOLUMES, error,
				("error writing lvint lb_num\n"));
		}

		/* free non repeating descriptors */
		last_dscr = NULL;
		for (cnt = 0; cnt < cpy_len; cnt++) {
			if (bufs[cnt] != last_dscr)
				free(bufs[cnt], M_UDFTEMP);
			last_dscr = bufs[cnt];
		}

		/* advance */
		in_pos   += cpy_len;
		out_wpos += cpy_len;
	}

	free(bufs, M_UDFTEMP);

	return (0);
}

static int
udf_writeout_lvint(struct udf_mount *ump, int lvflag)
{
	struct udf_lvintq *trace;
	struct timeval  now_v;
	struct timespec now_s;
	uint32_t sector;
	int logvol_integrity;
	int space, error;

	DPRINTF(VOLUMES, ("writing out logvol integrity descriptor\n"));

again:
	/* get free space in last chunk */
	trace = ump->lvint_trace;
	while (trace->wpos > (trace->end - trace->start)) {
		DPRINTF(VOLUMES, ("skip : start = %d, end = %d, pos = %d, "
				  "wpos = %d\n", trace->start, trace->end,
				  trace->pos, trace->wpos));
		trace++;
	}

	/* check if there is space to append */
	space = (trace->end - trace->start) - trace->wpos;
	DPRINTF(VOLUMES, ("write start = %d, end = %d, pos = %d, wpos = %d, "
			  "space = %d\n", trace->start, trace->end, trace->pos,
			  trace->wpos, space));

	/* get state */
	logvol_integrity = le32toh(ump->logvol_integrity->integrity_type);
	if (logvol_integrity == UDF_INTEGRITY_CLOSED) {
		if ((space < 3) && (lvflag & UDF_APPENDONLY_LVINT)) {
			/* don't allow this logvol to be opened */
			/* TODO extent LVINT space if possible */
			return (EROFS);
		}
	}

	if (space < 1) {
		if (lvflag & UDF_APPENDONLY_LVINT)
			return (EROFS);
		/* loose history by re-writing extents */
		error = udf_loose_lvint_history(ump);
		if (error != 0)
			return (error);
		goto again;
	}

	/* update our integrity descriptor to identify us and timestamp it */
	DPRINTF(VOLUMES, ("updating integrity descriptor\n"));
	microtime(&now_v);
	TIMEVAL_TO_TIMESPEC(&now_v, &now_s);
	udf_timespec_to_timestamp(&now_s, &ump->logvol_integrity->time);
	udf_set_regid(&ump->logvol_info->impl_id, IMPL_NAME);
	udf_add_impl_regid(ump, &ump->logvol_info->impl_id);

	/* writeout integrity descriptor */
	sector = trace->start + trace->wpos;
	error = udf_write_phys_dscr_sync(ump, NULL, UDF_C_DSCR,
			(union dscrptr *) ump->logvol_integrity,
			sector, sector);
	DPRINTF(VOLUMES, ("writeout lvint : error = %d\n", error));
	if (error != 0)
		return (error);

	/* advance write position */
	trace->wpos++; space--;
	if (space >= 1) {
		/* append terminator */
		sector = trace->start + trace->wpos;
		error = udf_write_terminator(ump, sector);

		DPRINTF(VOLUMES, ("write terminator : error = %d\n", error));
	}

	space = (trace->end - trace->start) - trace->wpos;
	DPRINTF(VOLUMES, ("write start = %d, end = %d, pos = %d, wpos = %d, "
		"space = %d\n", trace->start, trace->end, trace->pos,
		trace->wpos, space));
	DPRINTF(VOLUMES, ("finished writing out logvol integrity descriptor "
		"successfull\n"));

	return (error);
}

static int
udf_read_physical_partition_spacetables(struct udf_mount *ump)
{
	union dscrptr        *dscr;
	/* struct udf_args *args = &ump->mount_args; */
	struct part_desc     *partd;
	struct part_hdr_desc *parthdr;
	struct udf_bitmap    *bitmap;
	uint32_t phys_part;
	uint32_t lb_num, len;
	int error, dscr_type;

	/* unallocated space map */
	for (phys_part = 0; phys_part < UDF_PARTITIONS; phys_part++) {
		partd = ump->partitions[phys_part];
		if (partd == NULL)
			continue;
		parthdr = &partd->_impl_use.part_hdr;

		lb_num  = le32toh(partd->start_loc);
		lb_num += le32toh(parthdr->unalloc_space_bitmap.lb_num);
		len     = le32toh(parthdr->unalloc_space_bitmap.len);
		if (len == 0)
			continue;

		DPRINTF(VOLUMES, ("Read unalloc. space bitmap %d\n", lb_num));
		error = udf_read_phys_dscr(ump, lb_num, M_UDFTEMP, &dscr);
		if (!error && dscr) {
			/* analyse */
			dscr_type = le16toh(dscr->tag.id);
			if (dscr_type == TAGID_SPACE_BITMAP) {
				DPRINTF(VOLUMES, ("Accepting space bitmap\n"));
				ump->part_unalloc_dscr[phys_part] = &dscr->sbd;

				/* fill in ump->part_unalloc_bits */
				bitmap = &ump->part_unalloc_bits[phys_part];
				bitmap->blob  = (uint8_t *) dscr;
				bitmap->bits  = dscr->sbd.data;
				bitmap->max_offset = le32toh(dscr->sbd.num_bits);
				bitmap->pages = NULL;	/* TODO */
				bitmap->data_pos     = 0;
				bitmap->metadata_pos = 0;
			} else {
				free(dscr, M_UDFTEMP);

				printf( "UDF mount: error reading unallocated "
					"space bitmap\n");
				return (EROFS);
			}
		} else {
			/* blank not allowed */
			printf("UDF mount: blank unallocated space bitmap\n");
			return (EROFS);
		}
	}

	/* unallocated space table (not supported) */
	for (phys_part = 0; phys_part < UDF_PARTITIONS; phys_part++) {
		partd = ump->partitions[phys_part];
		if (partd == NULL)
			continue;
		parthdr = &partd->_impl_use.part_hdr;
	
		len     = le32toh(parthdr->unalloc_space_table.len);
		if (len) {
			printf("UDF mount: space tables not supported\n");
			return (EROFS);
		}
	}

	/* freed space map */
	for (phys_part = 0; phys_part < UDF_PARTITIONS; phys_part++) {
		partd = ump->partitions[phys_part];
		if (partd == NULL)
			continue;
		parthdr = &partd->_impl_use.part_hdr;

		/* freed space map */
		lb_num  = le32toh(partd->start_loc);
		lb_num += le32toh(parthdr->freed_space_bitmap.lb_num);
		len     = le32toh(parthdr->freed_space_bitmap.len);
		if (len == 0)
			continue;

		DPRINTF(VOLUMES, ("Read unalloc. space bitmap %d\n", lb_num));
		error = udf_read_phys_dscr(ump, lb_num, M_UDFTEMP, &dscr);
		if (!error && dscr) {
			/* analyse */
			dscr_type = le16toh(dscr->tag.id);
			if (dscr_type == TAGID_SPACE_BITMAP) {
				DPRINTF(VOLUMES, ("Accepting space bitmap\n"));
				ump->part_freed_dscr[phys_part] = &dscr->sbd;

				/* fill in ump->part_freed_bits */
				bitmap = &ump->part_unalloc_bits[phys_part];
				bitmap->blob  = (uint8_t *) dscr;
				bitmap->bits  = dscr->sbd.data;
				bitmap->max_offset = le32toh(dscr->sbd.num_bits);
				bitmap->pages = NULL;	/* TODO */
				bitmap->data_pos     = 0;
				bitmap->metadata_pos = 0;
			} else {
				free(dscr, M_UDFTEMP);

				printf( "UDF mount: error reading freed  "
					"space bitmap\n");
				return (EROFS);
			}
		} else {
			/* blank not allowed */
			printf("UDF mount: blank freed space bitmap\n");
			return (EROFS);
		}
	}

	/* freed space table (not supported) */
	for (phys_part = 0; phys_part < UDF_PARTITIONS; phys_part++) {
		partd = ump->partitions[phys_part];
		if (partd == NULL)
			continue;
		parthdr = &partd->_impl_use.part_hdr;
	
		len     = le32toh(parthdr->freed_space_table.len);
		if (len) {
			printf("UDF mount: space tables not supported\n");
			return (EROFS);
		}
	}

	return (0);
}

/* TODO implement async writeout */
int
udf_write_physical_partition_spacetables(struct udf_mount *ump, int waitfor)
{
	union dscrptr        *dscr;
	/* struct udf_args *args = &ump->mount_args; */
	struct part_desc     *partd;
	struct part_hdr_desc *parthdr;
	uint32_t phys_part;
	uint32_t lb_num, len, ptov;
	int error_all, error;

	error_all = 0;
	/* unallocated space map */
	for (phys_part = 0; phys_part < UDF_PARTITIONS; phys_part++) {
		partd = ump->partitions[phys_part];
		if (partd == NULL)
			continue;
		parthdr = &partd->_impl_use.part_hdr;

		ptov   = le32toh(partd->start_loc);
		lb_num = le32toh(parthdr->unalloc_space_bitmap.lb_num);
		len    = le32toh(parthdr->unalloc_space_bitmap.len);
		if (len == 0)
			continue;

		DPRINTF(VOLUMES, ("Write unalloc. space bitmap %d\n",
			lb_num + ptov));
		dscr = (union dscrptr *) ump->part_unalloc_dscr[phys_part];
		error = udf_write_phys_dscr_sync(ump, NULL, UDF_C_DSCR,
				(union dscrptr *) dscr,
				ptov + lb_num, lb_num);
		if (error != 0) {
			DPRINTF(VOLUMES, ("\tfailed!! (error %d)\n", error));
			error_all = error;
		}
	}

	/* freed space map */
	for (phys_part = 0; phys_part < UDF_PARTITIONS; phys_part++) {
		partd = ump->partitions[phys_part];
		if (partd == NULL)
			continue;
		parthdr = &partd->_impl_use.part_hdr;

		/* freed space map */
		ptov   = le32toh(partd->start_loc);
		lb_num = le32toh(parthdr->freed_space_bitmap.lb_num);
		len    = le32toh(parthdr->freed_space_bitmap.len);
		if (len == 0)
			continue;

		DPRINTF(VOLUMES, ("Write freed space bitmap %d\n",
			lb_num + ptov));
		dscr = (union dscrptr *) ump->part_freed_dscr[phys_part];
		error = udf_write_phys_dscr_sync(ump, NULL, UDF_C_DSCR,
				(union dscrptr *) dscr,
				ptov + lb_num, lb_num);
		if (error != 0) {
			DPRINTF(VOLUMES, ("\tfailed!! (error %d)\n", error));
			error_all = error;
		}
	}

	return (error_all);
}
#endif

#if 0
static int
udf_read_metadata_partition_spacetable(struct udf_mount *ump)
{
	struct udf_node	     *bitmap_node;
	union dscrptr        *dscr;
	struct udf_bitmap    *bitmap;
	uint64_t inflen;
	int error, dscr_type;

	bitmap_node = ump->metadatabitmap_node;

	/* only read in when metadata bitmap node is read in */
	if (bitmap_node == NULL)
		return (0);

	if (bitmap_node->fe != NULL) {
		inflen = le64toh(bitmap_node->fe->inf_len);
	} else {
		KASSERT(bitmap_node->efe, "error occured here");
		inflen = le64toh(bitmap_node->efe->inf_len);
	}

	DPRINTF(VOLUMES, ("Reading metadata space bitmap for "
		"%"PRIu64" bytes\n", inflen));

	/* allocate space for bitmap */
	dscr = malloc(inflen, M_UDFTEMP, M_WAITOK); /*M_CANFAIL was removed from third param*/
	if (!dscr)
		return (ENOMEM);

	/* set vnode type to regular file or we can't read from it! */
	bitmap_node->vnode->v_type = VREG;

	/* read in complete metadata bitmap file */
	error = udf_read_node(bitmap_node, (uint8_t *)dscr, 0, inflen);
	/*error = vn_rdwr(UIO_READ, bitmap_node->vnode,
			dscr,
			inflen, 0,
			UIO_SYSSPACE,
			IO_SYNC | IO_NODELOCKED | IO_ALTSEMANTICS, FSCRED,
			NULL, NULL);*/
	if (error != 0) {
		DPRINTF(VOLUMES, ("Error reading metadata space bitmap\n"));
		goto errorout;
	}

	/* analyse */
	dscr_type = le16toh(dscr->tag.id);
	if (dscr_type == TAGID_SPACE_BITMAP) {
		DPRINTF(VOLUMES, ("Accepting metadata space bitmap\n"));
		ump->metadata_unalloc_dscr = &dscr->sbd;

		/* fill in bitmap bits */
		bitmap = &ump->metadata_unalloc_bits;
		bitmap->blob  = (uint8_t *) dscr;
		bitmap->bits  = dscr->sbd.data;
		bitmap->max_offset = le32toh(dscr->sbd.num_bits);
		bitmap->pages = NULL;	/* TODO */
		bitmap->data_pos     = 0;
		bitmap->metadata_pos = 0;
	} else {
		DPRINTF(VOLUMES, ("No valid bitmap found!\n"));
		goto errorout;
	}

	return (0);

errorout:
	free(dscr, M_UDFTEMP);
	printf( "UDF mount: error reading unallocated "
		"space bitmap for metadata partition\n");

	return (EROFS);
}

int
udf_write_metadata_partition_spacetable(struct udf_mount *ump, int waitfor)
{
	struct udf_node	     *bitmap_node;
	union dscrptr        *dscr;
	uint64_t inflen, new_inflen;
	int dummy, error;

	bitmap_node = ump->metadatabitmap_node;

	/* only write out when metadata bitmap node is known */
	if (bitmap_node == NULL)
		return (0);

	if (bitmap_node->fe != NULL) {
		inflen = le64toh(bitmap_node->fe->inf_len);
	} else {
		KASSERT(bitmap_node->efe);
		inflen = le64toh(bitmap_node->efe->inf_len);
	}

	/* reduce length to zero */
	dscr = (union dscrptr *) ump->metadata_unalloc_dscr;
	new_inflen = udf_tagsize(dscr, 1);

	DPRINTF(VOLUMES, ("Resize and write out metadata space bitmap from "
		"%"PRIu64" to %"PRIu64" bytes\n", inflen, new_inflen));

	error = udf_resize_node(bitmap_node, new_inflen, &dummy);
	if (error != 0)
		printf("Error resizing metadata space bitmap\n");

	error = vn_rdwr(UIO_WRITE, bitmap_node->vnode,
			dscr,
			new_inflen, 0,
			UIO_SYSSPACE,
			IO_NODELOCKED | IO_ALTSEMANTICS, FSCRED,
			NULL, NULL);

	bitmap_node->i_flags |= IN_MODIFIED;
	vflushbuf(bitmap_node->vnode, 1 /* sync */);

	error = VOP_FSYNC(bitmap_node->vnode,
			FSCRED, FSYNC_WAIT, 0, 0);

	if (error != 0)
		printf( "Error writing out metadata partition unalloced "
			"space bitmap!\n");

	return (error);
}
#endif

/*
 * Checks if ump's vds information is correct and complete
 */
int
udf_process_vds(struct udf_mount *ump) {
	/* struct udf_args *args = &ump->mount_args; */
	union udf_pmap *mapping;
	struct logvol_int_desc *lvint;
	struct part_desc *part;
	struct udf_logvol_info *lvinfo;
	int error, log_part, phys_part, pmap_size, pmap_stype, pmap_type;
	int len, maps_on, n_meta, n_phys, n_spar, n_virt, raw_phys_part;
	uint32_t mt_l, n_pm;
	char *domain_name, *map_name; /* bits[128]; */
	const char *check_name;
	uint8_t *pmap_pos;

	/* we need at least one primary and one logical volume descriptor */
	if ((ump->primary_vol == NULL) || (ump->logical_vol) == NULL)
		return (EINVAL);

	/* we need at least one partition descriptor */
	if (ump->partitions[0] == NULL)
		return (EINVAL);

/*
Check that character set is correct?

	chsp = &ump->logical_vol->desc_charset;
	is_osta_typ0  = (chsp->type == 0);
	is_osta_typ0 &= (strcmp((char *) chsp->inf, osta_id) == 0);
*/

	/* check logical volume sector size verses device sector size */
	if (le32toh(ump->logical_vol->lb_size) != ump->sector_size) {
		printf("UDF mount: format violation, lb_size != sector size\n");
		return (EINVAL);
	}

	/* check domain name */
	domain_name = ump->logical_vol->domain_id.id;
	if (strncmp(domain_name, "*OSTA UDF Compliant", 20)) {
		printf("mount_udf: disc not OSTA UDF Compliant, aborting\n");
		return (EINVAL);
	}

	/*
	 * We need at least one logvol integrity descriptor recorded.  Note
	 * that its OK to have an open logical volume integrity here. The VAT
	 * will close/update the integrity.
	 */
	error = udf_retrieve_lvint(ump);
	if (error != 0)
		return (EINVAL); // previously it always returned this on error.

	/* process derived structures */
	n_pm = le32toh(ump->logical_vol->n_pm);   /* num partmaps         */
	lvint = ump->logvol_integrity;
	lvinfo = (struct udf_logvol_info *)(&lvint->tables[2 * n_pm]);
	ump->logvol_info = lvinfo;

	/* TODO check udf versions? */

	/*
	 * check logvol mappings: effective virt->log partmap translation
	 * check and recording of the mapping results. Saves expensive
	 * strncmp() in tight places.
	 */
	n_pm = le32toh(ump->logical_vol->n_pm);   /* num partmaps         */
	mt_l = le32toh(ump->logical_vol->mt_l);   /* partmaps data length */
	pmap_pos = ump->logical_vol->maps;

	if (n_pm > UDF_PMAPS) {
		printf("UDF mount: too many mappings\n");
		return (EINVAL);
	}

	/* count types and set partition numbers */
	ump->data_part = ump->node_part = ump->fids_part = 0;
	n_phys = n_virt = n_spar = n_meta = 0;
	for (log_part = 0; log_part < n_pm; log_part++) {
		mapping = (union udf_pmap *)pmap_pos;
		pmap_stype = pmap_pos[0];
		pmap_size = pmap_pos[1];

		switch (pmap_stype) {
		case 1:	/* physical mapping */
			/* volseq = le16toh(mapping->pm1.vol_seq_num); */
			raw_phys_part = le16toh(mapping->pm1.part_num);
			pmap_type = UDF_VTOP_TYPE_PHYS;
			n_phys++;
			ump->data_part = log_part;
			ump->node_part = log_part;
			ump->fids_part = log_part;
			break;
		case 2: /* virtual/sparable/meta mapping */
			map_name = mapping->pm2.part_id.id;
			/* volseq = le16toh(mapping->pm2.vol_seq_num); */
			raw_phys_part = le16toh(mapping->pm2.part_num);
			pmap_type = UDF_VTOP_TYPE_UNKNOWN;
			len = UDF_REGID_ID_SIZE;

			check_name = "*UDF Virtual Partition";
			if (strncmp(map_name, check_name, len) == 0) {
				pmap_type = UDF_VTOP_TYPE_VIRT;
				n_virt++;
				ump->node_part = log_part;
				break;
			}
			check_name = "*UDF Sparable Partition";
			if (strncmp(map_name, check_name, len) == 0) {
				pmap_type = UDF_VTOP_TYPE_SPARABLE;
				n_spar++;
				ump->data_part = log_part;
				ump->node_part = log_part;
				ump->fids_part = log_part;
				break;
			}
			check_name = "*UDF Metadata Partition";
			if (strncmp(map_name, check_name, len) == 0) {
printf("*UDF Metadata Partition\n");
				pmap_type = UDF_VTOP_TYPE_META;
				n_meta++;
				ump->node_part = log_part;
				ump->fids_part = log_part;
				break;
			}
			break;
		default:
			return (EINVAL);
		}

		/*
		 * BUGALERT: some rogue implementations use random physical
		 * partition numbers to break other implementations so lookup
		 * the number.
		 */
		for (phys_part = 0; phys_part < UDF_PARTITIONS; phys_part++) {
			part = ump->partitions[phys_part];
			if (part == NULL)
				continue;
			if (le16toh(part->part_num) == raw_phys_part)
				break;
		}

		if (phys_part == UDF_PARTITIONS)
			return (EINVAL);
		if (pmap_type == UDF_VTOP_TYPE_UNKNOWN)
			return (EINVAL);

		ump->vtop[log_part] = phys_part;
		ump->vtop_tp[log_part] = pmap_type;

		pmap_pos += pmap_size;
	}
	/* not winning the beauty contest */
	ump->vtop_tp[UDF_VTOP_RAWPART] = UDF_VTOP_TYPE_RAW;

	/* test some basic UDF assertions/requirements */
	if ((n_virt > 1) || (n_spar > 1) || (n_meta > 1))
		return (EINVAL);

	if (n_virt) {
		if ((n_phys == 0) || n_spar || n_meta)
			return (EINVAL);
	}
	if (n_spar + n_phys == 0)
		return (EINVAL);

	/* select allocation type for each logical partition */
	for (log_part = 0; log_part < n_pm; log_part++) {
		maps_on = ump->vtop[log_part];
		switch (ump->vtop_tp[log_part]) {
		case UDF_VTOP_TYPE_PHYS:
			/* assert(maps_on == log_part); */
			ump->vtop_alloc[log_part] = UDF_ALLOC_SPACEMAP;
			break;
		case UDF_VTOP_TYPE_VIRT:
			ump->vtop_alloc[log_part] = UDF_ALLOC_VAT;
			ump->vtop_alloc[maps_on] = UDF_ALLOC_SEQUENTIAL;
			break;
		case UDF_VTOP_TYPE_SPARABLE:
			/* assert(maps_on == log_part); */
			ump->vtop_alloc[log_part] = UDF_ALLOC_SPACEMAP;
			break;
		case UDF_VTOP_TYPE_META:
			ump->vtop_alloc[log_part] = UDF_ALLOC_METABITMAP;
#if 0
			if (ump->discinfo.mmc_cur & MMC_CAP_PSEUDOOVERWRITE) {
				/* special case for UDF 2.60 */
				ump->vtop_alloc[log_part] = UDF_ALLOC_METASEQUENTIAL;
				ump->vtop_alloc[maps_on]  = UDF_ALLOC_SEQUENTIAL;
			}
#endif
			break;
		default:
			panic("bad alloction type in udf's ump->vtop\n");
		}
	}

	/* determine logical volume open/closure actions */
#if 0
	if (n_virt) {
		ump->lvopen  = 0;
		if (ump->discinfo.last_session_state == MMC_STATE_EMPTY)
			ump->lvopen |= UDF_OPEN_SESSION ;
		ump->lvclose = UDF_WRITE_VAT;
		if (ump->mount_args.udfmflags & UDFMNT_CLOSESESSION)
			ump->lvclose |= UDF_CLOSE_SESSION;
	} else {
		/* `normal' rewritable or non sequential media */
		ump->lvopen  = UDF_WRITE_LVINT;
		ump->lvclose = UDF_WRITE_LVINT;
		if ((ump->discinfo.mmc_cur & MMC_CAP_REWRITABLE) == 0)
			ump->lvopen  |= UDF_APPENDONLY_LVINT;
	}
#endif

	/*
	 * Determine sheduler error behaviour. For virtual partitions, update
	 * the trackinfo; for sparable partitions replace a whole block on the
	 * sparable table. Allways requeue.
	 */
	ump->lvreadwrite = 0;
	if (n_virt)
		ump->lvreadwrite = UDF_UPDATE_TRACKINFO;
	if (n_spar)
		ump->lvreadwrite = UDF_REMAP_BLOCK;

#if 0  
	/*
	 * Select our sheduler
	 */
	ump->strategy = &udf_strat_rmw;
	if (n_virt || (ump->discinfo.mmc_cur & MMC_CAP_PSEUDOOVERWRITE))
		ump->strategy = &udf_strat_sequential;
	if ((ump->discinfo.mmc_class == MMC_CLASS_DISC) ||
		(ump->discinfo.mmc_class == MMC_CLASS_UNKN))
			ump->strategy = &udf_strat_direct;
	if (n_spar)
		ump->strategy = &udf_strat_rmw;

	ump->strategy = &udf_strat_readonly;

#if 0  
	/* read-only access won't benefit from the other shedulers */
	if (ump->vfs_mountp->mnt_flag & MNT_RDONLY)
		ump->strategy = &udf_strat_direct;
#endif

	snprintb(bits, sizeof(bits), UDFLOGVOL_BITS, ump->lvopen);
	DPRINTF(VOLUMES, ("\tactions on logvol open  %s\n", bits));
	snprintb(bits, sizeof(bits), UDFLOGVOL_BITS, ump->lvclose);
	DPRINTF(VOLUMES, ("\tactions on logvol close %s\n", bits));
	snprintb(bits, sizeof(bits), UDFONERROR_BITS, ump->lvreadwrite);
	DPRINTF(VOLUMES, ("\tactions on logvol errors %s\n", bits));

	DPRINTF(VOLUMES, ("\tselected sheduler `%s`\n", 
		(ump->strategy == &udf_strat_direct) ? "Direct" :
		(ump->strategy == &udf_strat_sequential) ? "Sequential" :
		(ump->strategy == &udf_strat_rmw) ? "RMW" : "UNKNOWN!"));
#endif
	/* signal its OK for now */
	return (0);
}

/*
 * Update logical volume name in all structures that keep a record of it. We
 * use memmove since each of them might be specified as a source.
 *
 * Note that it doesn't update the VAT structure!
 */
static void
udf_update_logvolname(struct udf_mount *ump, char *logvol_id)
{
	struct logvol_desc *lvd = NULL;
	struct fileset_desc *fsd = NULL;
	struct udf_lv_info *lvi = NULL;

	lvd = ump->logical_vol;
	fsd = ump->fileset_desc;
	if (ump->implementation)
		lvi = &ump->implementation->_impl_use.lv_info;

	/* logvol's id might be specified as origional so use memmove here */
	memmove(lvd->logvol_id, logvol_id, 128);
	if (fsd)
		memmove(fsd->logvol_id, logvol_id, 128);
	if (lvi)
		memmove(lvi->logvol_id, logvol_id, 128);
}

#if 0
void
udf_inittag(struct udf_mount *ump, struct desc_tag *tag, int tagid,
	uint32_t sector)
{
	assert(ump->logical_vol);

	tag->id 		= htole16(tagid);
	tag->descriptor_ver	= ump->logical_vol->tag.descriptor_ver;
	tag->cksum		= 0;
	tag->reserved		= 0;
	tag->serial_num		= ump->logical_vol->tag.serial_num;
	tag->tag_loc            = htole32(sector);
}

uint64_t
udf_advance_uniqueid(struct udf_mount *ump)
{
	uint64_t unique_id;

	mutex_enter(&ump->logvol_mutex);
	unique_id = le64toh(ump->logvol_integrity->lvint_next_unique_id);
	if (unique_id < 0x10)
		unique_id = 0x10;
	ump->logvol_integrity->lvint_next_unique_id = htole64(unique_id + 1);
	mutex_exit(&ump->logvol_mutex);

	return (unique_id);
}

static void
udf_adjust_filecount(struct udf_node *udf_node, int sign)
{
	struct udf_mount *ump = udf_node->ump;
	uint32_t num_dirs, num_files;
	int udf_file_type;

	/* get file type */
	if (udf_node->fe != NULL) {
		udf_file_type = udf_node->fe->icbtag.file_type;
	} else {
		udf_file_type = udf_node->efe->icbtag.file_type;
	}

	/* adjust file count */
	mutex_enter(&ump->allocate_mutex);
	if (udf_file_type == UDF_ICB_FILETYPE_DIRECTORY) {
		num_dirs = le32toh(ump->logvol_info->num_directories);
		ump->logvol_info->num_directories =
			htole32((num_dirs + sign));
	} else {
		num_files = le32toh(ump->logvol_info->num_files);
		ump->logvol_info->num_files =
			htole32((num_files + sign));
	}
	mutex_exit(&ump->allocate_mutex);
}

void
udf_osta_charset(struct charspec *charspec)
{
	memset(charspec, 0, sizeof(struct charspec));
	charspec->type = 0;
	strcpy((char *) charspec->inf, "OSTA Compressed Unicode");
}

/* first call udf_set_regid and then the suffix */
void
udf_set_regid(struct regid *regid, char const *name)
{
	memset(regid, 0, sizeof(struct regid));
	regid->flags    = 0;		/* not dirty and not protected */
	strcpy((char *) regid->id, name);
}

void
udf_add_domain_regid(struct udf_mount *ump, struct regid *regid)
{
	uint16_t *ver;

	ver  = (uint16_t *) regid->id_suffix;
	*ver = ump->logvol_info->min_udf_readver;
}

void
udf_add_udf_regid(struct udf_mount *ump, struct regid *regid)
{
	uint16_t *ver;

	ver  = (uint16_t *) regid->id_suffix;
	*ver = ump->logvol_info->min_udf_readver;

	regid->id_suffix[2] = 4;	/* unix */
	regid->id_suffix[3] = 8;	/* NetBSD */
}

void
udf_add_impl_regid(struct udf_mount *ump, struct regid *regid)
{
	regid->id_suffix[0] = 4;	/* unix */
	regid->id_suffix[1] = 8;	/* NetBSD */
}

void
udf_add_app_regid(struct udf_mount *ump, struct regid *regid)
{
	regid->id_suffix[0] = APP_VERSION_MAIN;
	regid->id_suffix[1] = APP_VERSION_SUB;
}

static int
udf_create_parentfid(struct udf_mount *ump, struct fileid_desc *fid,
	struct long_ad *parent, uint64_t unique_id)
{
	/* the size of an empty FID is 38 but needs to be a multiple of 4 */
	int fidsize = 40;

	udf_inittag(ump, &fid->tag, TAGID_FID, le32toh(parent->loc.lb_num));
	fid->file_version_num = htole16(1);	/* UDF 2.3.4.1 */
	fid->file_char = UDF_FILE_CHAR_DIR | UDF_FILE_CHAR_PAR;
	fid->icb = *parent;
	fid->icb.longad_uniqueid = htole32((uint32_t) unique_id);
	fid->tag.desc_crc_len = htole16(fidsize - UDF_DESC_TAG_LENGTH);
	(void) udf_validate_tag_and_crc_sums((union dscrptr *) fid);

	return (fidsize);
}
#endif

/*
 * Extended attribute support. UDF knows of 3 places for extended attributes:
 *
 * (a) inside the file's (e)fe in the length of the extended attribute area
 * before the allocation descriptors/filedata
 *
 * (b) in a file referenced by (e)fe->ext_attr_icb and 
 *
 * (c) in the e(fe)'s associated stream directory that can hold various
 * sub-files. In the stream directory a few fixed named subfiles are reserved
 * for NT/Unix ACL's and OS/2 attributes.
 * 
 * NOTE: Extended attributes are read randomly but allways written
 * *atomicaly*. For ACL's this interface is propably different but not known
 * to me yet.
 *
 * Order of extended attributes in a space :
 *   ECMA 167 EAs
 *   Non block aligned Implementation Use EAs
 *   Block aligned Implementation Use EAs
 *   Application Use EAs
 */

static int
udf_impl_extattr_check(struct impl_extattr_entry *implext)
{
	uint16_t *spos;

	if (strncmp(implext->imp_id.id, "*UDF", 4) == 0) {
		/* checksum valid? */
		spos = (uint16_t *)implext->data;
		if (le16toh(*spos) != udf_ea_cksum((uint8_t *)implext))
			return (EINVAL);
	}

	return (0);
}

static void
udf_calc_impl_extattr_checksum(struct impl_extattr_entry *implext)
{
	uint16_t *spos;

	if (strncmp(implext->imp_id.id, "*UDF", 4) == 0) {
		/* set checksum */
		spos = (uint16_t *)implext->data;
		*spos = le16toh(udf_ea_cksum((uint8_t *)implext));
	}
}

int
udf_extattr_search_intern(struct udf_node *node, uint32_t sattr,
    char const *sattrname, uint32_t *offsetp, uint32_t *lengthp)
{
	struct extattrhdr_desc *eahdr;
	struct extattr_entry *attrhdr;
	struct impl_extattr_entry *implext;
	int error;
	int32_t l_ea;
	uint32_t a_l, offset, sector_size;
	uint8_t *pos;

	/* get mountpoint */
	sector_size = node->ump->sector_size;

	/* get information from fe/efe */
	if (node->fe != NULL) {
		l_ea = le32toh(node->fe->l_ea);
		eahdr = (struct extattrhdr_desc *) node->fe->data;
	} else {
		l_ea = le32toh(node->efe->l_ea);
		eahdr = (struct extattrhdr_desc *) node->efe->data;
	}

	/* something recorded here? */
	if (l_ea == 0)
		return (ENOENT);

	/* check extended attribute tag; what to do if it fails? */
	error = udf_check_tag(eahdr);
	if (error != 0)
		return (EINVAL);
	if (le16toh(eahdr->tag.id) != TAGID_EXTATTR_HDR)
		return (EINVAL);
	error = udf_check_tag_payload(eahdr, sizeof(struct extattrhdr_desc));
	if (error != 0)
		return (EINVAL);

	/* looking for Ecma-167 attributes? */
	offset = sizeof(struct extattrhdr_desc);

	/* looking for either implemenation use or application use */
	if (sattr == 2048) {				/* [4/48.10.8] */
		offset = le32toh(eahdr->impl_attr_loc);
		if (offset == UDF_IMPL_ATTR_LOC_NOT_PRESENT)
			return (ENOENT);
	}
	if (sattr == 65536) {				/* [4/48.10.9] */
		offset = le32toh(eahdr->appl_attr_loc);
		if (offset == UDF_APPL_ATTR_LOC_NOT_PRESENT)
			return (ENOENT);
	}

	/* paranoia check offset and l_ea */
	if (l_ea + offset >= sector_size - sizeof(struct extattr_entry))
		return (EINVAL);

	/* find our extended attribute  */
	l_ea -= offset;
	pos = (uint8_t *)eahdr + offset;

	while (l_ea >= sizeof(struct extattr_entry)) {
		attrhdr = (struct extattr_entry *)pos;
		implext = (struct impl_extattr_entry *)pos;

		/* get complete attribute length and check for roque values */
		a_l = le32toh(attrhdr->a_l);
		if ((a_l == 0) || (a_l > l_ea))
			return (EINVAL);

		if (attrhdr->type != sattr)
			goto next_attribute;

		/* we might have found it! */
		if (attrhdr->type < 2048) {	/* Ecma-167 attribute */
			*offsetp = offset;
			*lengthp = a_l;
			return (0);		/* success */
		}

		/*
		 * Implementation use and application use extended attributes
		 * have a name to identify. They share the same structure only
		 * UDF implementation use extended attributes have a checksum
		 * we need to check
		 */

		if (strcmp(implext->imp_id.id, sattrname) == 0) {
			/* we have found our appl/implementation attribute */
			*offsetp = offset;
			*lengthp = a_l;
			return (0);		/* success */
		}

next_attribute:
		/* next attribute */
		pos += a_l;
		l_ea -= a_l;
		offset += a_l;
	}
	/* not found */
	return (ENOENT);
}

#if 0
static void
udf_extattr_insert_internal(struct udf_mount *ump, union dscrptr *dscr,
	struct extattr_entry *extattr)
{
	struct file_entry      *fe;
	struct extfile_entry   *efe;
	struct extattrhdr_desc *extattrhdr;
	struct impl_extattr_entry *implext;
	uint32_t impl_attr_loc, appl_attr_loc, l_ea, a_l, exthdr_len;
	uint32_t *l_eap, l_ad;
	uint16_t *spos;
	uint8_t *bpos, *data;

	if (le16toh(dscr->tag.id) == TAGID_FENTRY) {
		fe    = &dscr->fe;
		data  = fe->data;
		l_eap = &fe->l_ea;
		l_ad  = le32toh(fe->l_ad);
	} else if (le16toh(dscr->tag.id) == TAGID_EXTFENTRY) {
		efe   = &dscr->efe;
		data  = efe->data;
		l_eap = &efe->l_ea;
		l_ad  = le32toh(efe->l_ad);
	} else {
		panic("Bad tag passed to udf_extattr_insert_internal");
	}

	/* can't append already written to file descriptors yet */
	assert(l_ad == 0);

	/* should have a header! */
	extattrhdr = (struct extattrhdr_desc *) data;
	l_ea = le32toh(*l_eap);
	if (l_ea == 0) {
		/* create empty extended attribute header */
		exthdr_len = sizeof(struct extattrhdr_desc);

		udf_inittag(ump, &extattrhdr->tag, TAGID_EXTATTR_HDR,
			/* loc */ 0);
		extattrhdr->impl_attr_loc = htole32(exthdr_len);
		extattrhdr->appl_attr_loc = htole32(exthdr_len);
		extattrhdr->tag.desc_crc_len = htole16(8);

		/* record extended attribute header length */
		l_ea = exthdr_len;
		*l_eap = htole32(l_ea);
	}

	/* extract locations */
	impl_attr_loc = le32toh(extattrhdr->impl_attr_loc);
	appl_attr_loc = le32toh(extattrhdr->appl_attr_loc);
	if (impl_attr_loc == UDF_IMPL_ATTR_LOC_NOT_PRESENT)
		impl_attr_loc = l_ea;
	if (appl_attr_loc == UDF_IMPL_ATTR_LOC_NOT_PRESENT)
		appl_attr_loc = l_ea;

	/* Ecma 167 EAs */
	if (le32toh(extattr->type) < 2048) {
		assert(impl_attr_loc == l_ea);
		assert(appl_attr_loc == l_ea);
	}

	/* implementation use extended attributes */
	if (le32toh(extattr->type) == 2048) {
		assert(appl_attr_loc == l_ea);

		/* calculate and write extended attribute header checksum */
		implext = (struct impl_extattr_entry *) extattr;
		assert(le32toh(implext->iu_l) == 4);	/* [UDF 3.3.4.5] */
		spos = (uint16_t *) implext->data;
		*spos = htole16(udf_ea_cksum((uint8_t *) implext));
	}

	/* application use extended attributes */
	assert(le32toh(extattr->type) != 65536);
	assert(appl_attr_loc == l_ea);

	/* append the attribute at the end of the current space */
	bpos = data + le32toh(*l_eap);
	a_l  = le32toh(extattr->a_l);

	/* update impl. attribute locations */
	if (le32toh(extattr->type) < 2048) {
		impl_attr_loc = l_ea + a_l;
		appl_attr_loc = l_ea + a_l;
	}
	if (le32toh(extattr->type) == 2048) {
		appl_attr_loc = l_ea + a_l;
	}

	/* copy and advance */
	memcpy(bpos, extattr, a_l);
	l_ea += a_l;
	*l_eap = htole32(l_ea);

	/* do the `dance` again backwards */
	if (le16toh(ump->logical_vol->tag.descriptor_ver) != 2) {
		if (impl_attr_loc == l_ea)
			impl_attr_loc = UDF_IMPL_ATTR_LOC_NOT_PRESENT;
		if (appl_attr_loc == l_ea)
			appl_attr_loc = UDF_APPL_ATTR_LOC_NOT_PRESENT;
	}

	/* store offsets */
	extattrhdr->impl_attr_loc = htole32(impl_attr_loc);
	extattrhdr->appl_attr_loc = htole32(appl_attr_loc);
}
#endif

static int 
udf_update_lvid_from_vat_extattr(struct udf_node *vat_node)
{
	struct impl_extattr_entry *implext;
	struct vatlvext_extattr_entry lvext;
	struct udf_mount *ump;
	struct udf_logvol_info *lvinfo;
	uint64_t vat_uniqueid;
	int error;
	uint32_t a_l, offset;
	const char *extstr = "*UDF VAT LVExtension";
	uint8_t *ea_start, *lvextpos;

	/* get mountpoint and lvinfo */
	ump = vat_node->ump;
	lvinfo = ump->logvol_info;

	/* get information from fe/efe */
	if (vat_node->fe != NULL) {
		vat_uniqueid = le64toh(vat_node->fe->unique_id);
		ea_start = vat_node->fe->data;
	} else {
		vat_uniqueid = le64toh(vat_node->efe->unique_id);
		ea_start = vat_node->efe->data;
	}

	error = udf_extattr_search_intern(vat_node, 2048, extstr, &offset, 
	    &a_l);
	if (error !=0)
		return (error);

	implext = (struct impl_extattr_entry *)(ea_start + offset);
	error = udf_impl_extattr_check(implext);
	if (error != 0)
		return (error);

	/* paranoia */
	if (a_l != sizeof(*implext) - 1 + le32toh(implext->iu_l) +
	    sizeof(lvext))
		return (EINVAL);

	/*
	 * we have found our "VAT LVExtension attribute. BUT due to a
	 * bug in the specification it might not be word aligned so
	 * copy first to avoid panics on some machines (!!)
	 */
	lvextpos = implext->data + le32toh(implext->iu_l);
	memcpy(&lvext, lvextpos, sizeof(lvext));

	/* check if it was updated the last time */
	if (le64toh(lvext.unique_id_chk) == vat_uniqueid) {
		lvinfo->num_files = lvext.num_files;
		lvinfo->num_directories = lvext.num_directories;
		udf_update_logvolname(ump, lvext.logvol_id);
	} else {
		/* replace VAT LVExt by free space EA */
		memset(implext->imp_id.id, 0, UDF_REGID_ID_SIZE);
		strcpy(implext->imp_id.id, "*UDF FreeEASpace");
		udf_calc_impl_extattr_checksum(implext);
	}

	return (0);
}

#if 0
static int 
udf_update_vat_extattr_from_lvid(struct udf_node *vat_node)
{
	struct udf_mount       *ump;
	struct udf_logvol_info *lvinfo;
	struct impl_extattr_entry     *implext;
	struct vatlvext_extattr_entry  lvext;
	const char *extstr = "*UDF VAT LVExtension";
	uint64_t    vat_uniqueid;
	uint32_t    offset, a_l;
	uint8_t    *ea_start, *lvextpos;
	int         error;

	/* get mountpoint and lvinfo */
	ump    = vat_node->ump;
	lvinfo = ump->logvol_info;

	/* get information from fe/efe */
	if (vat_node->fe != NULL) {
		vat_uniqueid = le64toh(vat_node->fe->unique_id);
		ea_start     = vat_node->fe->data;
	} else {
		vat_uniqueid = le64toh(vat_node->efe->unique_id);
		ea_start     = vat_node->efe->data;
	}

	error = udf_extattr_search_intern(vat_node, 2048, extstr, &offset, &a_l);
	if (error != 0)
		return (error);
	/* found, it existed */

	/* paranoia */
	implext = (struct impl_extattr_entry *) (ea_start + offset);
	error = udf_impl_extattr_check(implext);
	if (error != 0) {
		DPRINTF(VOLUMES, ("VAT LVExtension bad on update\n"));
		return (error);
	}
	/* it is correct */

	/*
	 * we have found our "VAT LVExtension attribute. BUT due to a
	 * bug in the specification it might not be word aligned so
	 * copy first to avoid panics on some machines (!!)
	 */
	DPRINTF(VOLUMES, ("Updating VAT LVExtension attr\n"));
	lvextpos = implext->data + le32toh(implext->iu_l);

	lvext.unique_id_chk   = vat_uniqueid;
	lvext.num_files       = lvinfo->num_files;
	lvext.num_directories = lvinfo->num_directories;
	memmove(lvext.logvol_id, ump->logical_vol->logvol_id, 128);

	memcpy(lvextpos, &lvext, sizeof(lvext));

	return (0);
}
#endif

int
udf_vat_read(struct udf_node *vat_node, uint8_t *blob, int size, 
    uint32_t offset)
{
	struct udf_mount *ump = vat_node->ump;

/*	mutex_enter(&ump->allocate_mutex); */
	if (offset + size > ump->vat_offset + ump->vat_entries * 4)
		return (EINVAL);
	memcpy(blob, ump->vat_table + offset, size);
/*	mutex_exit(&ump->allocate_mutex); */

	return (0);
}

#if 0
int
udf_vat_write(struct udf_node *vat_node, uint8_t *blob, int size, uint32_t offset)
{
	struct udf_mount *ump = vat_node->ump;
	uint32_t offset_high;
	uint8_t *new_vat_table;

	/* extent VAT allocation if needed */
	offset_high = offset + size;
	if (offset_high >= ump->vat_table_alloc_len) {
		/* realloc */
		new_vat_table = realloc(ump->vat_table,
			ump->vat_table_alloc_len + UDF_VAT_CHUNKSIZE,
			M_UDFTEMP, M_WAITOK | M_CANFAIL);
		if (!new_vat_table) {
			printf("udf_vat_write: can't extent VAT, out of mem\n");
			return (ENOMEM);
		}
		ump->vat_table = new_vat_table;
		ump->vat_table_alloc_len += UDF_VAT_CHUNKSIZE;
	}
	ump->vat_table_len = MAX(ump->vat_table_len, offset_high);

	memcpy(ump->vat_table + offset, blob, size);
	return (0);
}

/* TODO support previous VAT location writeout */
static int
udf_update_vat_descriptor(struct udf_mount *ump)
{
	struct udf_node *vat_node = ump->vat_node;
	struct udf_logvol_info *lvinfo = ump->logvol_info;
	struct icb_tag *icbtag;
	struct udf_oldvat_tail *oldvat_tl;
	struct udf_vat *vat;
	uint64_t unique_id;
	uint32_t lb_size;
	uint8_t *raw_vat;
	int filetype, error;

	KASSERT(vat_node);
	KASSERT(lvinfo);
	lb_size = le32toh(ump->logical_vol->lb_size);

	/* get our new unique_id */
	unique_id = udf_advance_uniqueid(ump);

	/* get information from fe/efe */
	if (vat_node->fe != NULL) {
		icbtag    = &vat_node->fe->icbtag;
		vat_node->fe->unique_id = htole64(unique_id);
	} else {
		icbtag = &vat_node->efe->icbtag;
		vat_node->efe->unique_id = htole64(unique_id);
	}

	/* Check icb filetype! it has to be 0 or UDF_ICB_FILETYPE_VAT */
	filetype = icbtag->file_type;
	KASSERT((filetype == 0) || (filetype == UDF_ICB_FILETYPE_VAT));

	/* allocate piece to process head or tail of VAT file */
	raw_vat = malloc(lb_size, M_UDFTEMP, M_WAITOK);

	if (filetype == 0) {
		/*
		 * Update "*UDF VAT LVExtension" extended attribute from the
		 * lvint if present.
		 */
		udf_update_vat_extattr_from_lvid(vat_node);

		/* setup identifying regid */
		oldvat_tl = (struct udf_oldvat_tail *) raw_vat;
		memset(oldvat_tl, 0, sizeof(struct udf_oldvat_tail));

		udf_set_regid(&oldvat_tl->id, "*UDF Virtual Alloc Tbl");
		udf_add_udf_regid(ump, &oldvat_tl->id);
		oldvat_tl->prev_vat = htole32(0xffffffff);

		/* write out new tail of virtual allocation table file */
		error = udf_vat_write(vat_node, raw_vat,
			sizeof(struct udf_oldvat_tail), ump->vat_entries * 4);
	} else {
		/* compose the VAT2 header */
		vat = (struct udf_vat *) raw_vat;
		memset(vat, 0, sizeof(struct udf_vat));

		vat->header_len       = htole16(152);	/* as per spec */
		vat->impl_use_len     = htole16(0);
		memmove(vat->logvol_id, ump->logical_vol->logvol_id, 128);
		vat->prev_vat         = htole32(0xffffffff);
		vat->num_files        = lvinfo->num_files;
		vat->num_directories  = lvinfo->num_directories;
		vat->min_udf_readver  = lvinfo->min_udf_readver;
		vat->min_udf_writever = lvinfo->min_udf_writever;
		vat->max_udf_writever = lvinfo->max_udf_writever;

		error = udf_vat_write(vat_node, raw_vat,
			sizeof(struct udf_vat), 0);
	}
	free(raw_vat, M_UDFTEMP);

	return (error);	/* success! */
}

int
udf_writeout_vat(struct udf_mount *ump)
{
	struct udf_node *vat_node = ump->vat_node;
	uint32_t vat_length;
	int error;

	KASSERT(vat_node);

	DPRINTF(CALL, ("udf_writeout_vat\n"));

/*	mutex_enter(&ump->allocate_mutex); */
	udf_update_vat_descriptor(ump);

	/* write out the VAT contents ; TODO intelligent writing */
	vat_length = ump->vat_table_len;
	error = vn_rdwr(UIO_WRITE, vat_node->vnode,
		ump->vat_table, ump->vat_table_len, 0,
		UIO_SYSSPACE, IO_NODELOCKED, FSCRED, NULL, NULL);
	if (error != 0) {
		printf("udf_writeout_vat: failed to write out VAT contents\n");
		goto out;
	}

/*	mutex_exit(&ump->allocate_mutex); */

	vflushbuf(ump->vat_node->vnode, 1 /* sync */);
	error = VOP_FSYNC(ump->vat_node->vnode,
			FSCRED, FSYNC_WAIT, 0, 0);
	if (error != 0)
		printf("udf_writeout_vat: error writing VAT node!\n");
out:

	return (error);
}
#endif

/*
 * Read in relevant pieces of VAT file and check if its indeed a VAT file
 * descriptor. If OK, read in complete VAT file.
 */

static int
udf_check_for_vat(struct udf_node *vat_node)
{
	struct udf_mount *ump;
	struct icb_tag   *icbtag;
	struct timestamp *mtime;
	struct udf_vat   *vat;
	struct udf_oldvat_tail *oldvat_tl;
	struct udf_logvol_info *lvinfo;
	uint64_t  unique_id;
	int error, filetype;
	uint32_t vat_entries, vat_length, vat_offset, vat_table_alloc_len;
	uint32_t *raw_vat, sector_size;
	char *regid_name;
	uint8_t *vat_table;

	/* vat_length is really 64 bits though impossible */

	if (vat_node == NULL)
		return (ENOENT);

	/* get mount info */
	ump = vat_node->ump;
	sector_size = le32toh(ump->logical_vol->lb_size);

	/* check assertions */
	/* assert(vat_node->fe != NULL || vat_node->efe != NULL); */
	/* assert(ump->logvol_integrity); */

	/* set vnode type to regular file or we can't read from it! */
	/* vat_node->vnode->v_type = VREG; */

	/* get information from fe/efe */
	if (vat_node->fe != NULL) {
		vat_length = le64toh(vat_node->fe->inf_len);
		icbtag = &vat_node->fe->icbtag;
		mtime = &vat_node->fe->mtime;
		unique_id = le64toh(vat_node->fe->unique_id);
	} else {
		vat_length = le64toh(vat_node->efe->inf_len);
		icbtag = &vat_node->efe->icbtag;
		mtime = &vat_node->efe->mtime;
		unique_id = le64toh(vat_node->efe->unique_id);
	}

	/* Check icb filetype! it has to be 0 or UDF_ICB_FILETYPE_VAT */
	filetype = icbtag->file_type;
	if ((filetype != 0) && (filetype != UDF_ICB_FILETYPE_VAT))
		return (ENOENT);

	vat_table_alloc_len =
		((vat_length + UDF_VAT_CHUNKSIZE - 1) / UDF_VAT_CHUNKSIZE)
			* UDF_VAT_CHUNKSIZE;

	if (vat_table_alloc_len > UDF_VAT_ALLOC_LIMIT) {
		printf("Allocation of %d bytes failed for VAT; length exceeded"
		    " implementation limit.\n", vat_table_alloc_len);
		return (ENOMEM);
	}
	vat_table = malloc(vat_table_alloc_len, M_UDFTEMP, M_WAITOK); 

	/* allocate piece to read in head or tail of VAT file */
	raw_vat = malloc(sector_size, M_UDFTEMP, M_WAITOK);

	/*
	 * check contents of the file if its the old 1.50 VAT table format.
	 * Its notoriously broken and allthough some implementations support an
	 * extention as defined in the UDF 1.50 errata document, its doubtfull
	 * to be useable since a lot of implementations don't maintain it.
	 */
	lvinfo = ump->logvol_info;

	if (filetype == 0) {
		/* definition */
		vat_offset = 0;
		vat_entries = (vat_length - 36) / 4;

		/* read in tail of virtual allocation table file */
		error = udf_read_node(vat_node, (uint8_t *)raw_vat, 
		    vat_entries * 4, sizeof(struct udf_oldvat_tail));
		/*error = vn_rdwr(UIO_READ, vat_node->vnode,
				(uint8_t *) raw_vat,
				sizeof(struct udf_oldvat_tail),
				vat_entries * 4,
				UIO_SYSSPACE, IO_SYNC | IO_NODELOCKED, FSCRED,
				NULL, NULL, curthread);*/
		if (error != 0)
			goto out;

		/* check 1.50 VAT */
		oldvat_tl = (struct udf_oldvat_tail *)raw_vat;
		regid_name = (char *)oldvat_tl->id.id;
		error = strncmp(regid_name, "*UDF Virtual Alloc Tbl", 22);
		if (error != 0) {
			error = ENOENT;
			goto out;
		}

		/*
		 * update LVID from "*UDF VAT LVExtension" extended attribute
		 * if present.
		 */
		udf_update_lvid_from_vat_extattr(vat_node);
	} else {
		/* read in head of virtual allocation table file */
		error = udf_read_node(vat_node, (uint8_t *)raw_vat, 0, 
		    sizeof(struct udf_vat));
		/*error = vn_rdwr(UIO_READ, vat_node->vnode,
				(uint8_t *) raw_vat,
				sizeof(struct udf_vat), 0,
				UIO_SYSSPACE, IO_SYNC | IO_NODELOCKED, FSCRED,
				NULL, NULL, curthread);*/
		if (error != 0)
			goto out;

		/* definition */
		vat = (struct udf_vat *)raw_vat;
		vat_offset = vat->header_len;
		vat_entries = (vat_length - vat_offset) / 4;

		/* assert(lvinfo); */
		lvinfo->num_files = vat->num_files;
		lvinfo->num_directories = vat->num_directories;
		lvinfo->min_udf_readver = vat->min_udf_readver;
		lvinfo->min_udf_writever = vat->min_udf_writever;
		lvinfo->max_udf_writever = vat->max_udf_writever;
	
		udf_update_logvolname(ump, vat->logvol_id);
	}

	/* read in complete VAT file */
	error = udf_read_node(vat_node, vat_table, 0, vat_length);
	/*error = vn_rdwr(UIO_READ, vat_node->vnode,
			vat_table,
			vat_length, 0,
			UIO_SYSSPACE, IO_SYNC | IO_NODELOCKED, FSCRED,
			NULL, NULL, curthread);*/
	if (error != 0)
		printf("read in of complete VAT file failed (error %d)\n",
		    error);
	if (error != 0)
		goto out;

	ump->logvol_integrity->lvint_next_unique_id = htole64(unique_id);
	ump->logvol_integrity->integrity_type = htole32(UDF_INTEGRITY_CLOSED);
	ump->logvol_integrity->time = *mtime;

	ump->vat_table_len = vat_length;
	ump->vat_table_alloc_len = vat_table_alloc_len;
	ump->vat_table = vat_table;
	ump->vat_offset = vat_offset;
	ump->vat_entries = vat_entries;
	ump->vat_last_free_lb = 0;		/* start at beginning */

out:
	if (error != 0) {
		if (vat_table != NULL)
			free(vat_table, M_UDFTEMP);
	}
	free(raw_vat, M_UDFTEMP);

	return (error);
}

static int
udf_search_vat(struct udf_mount *ump)
{
	union dscrptr *dscr;
	/* struct vnode *vp; */
	struct long_ad icb_loc;
	struct udf_node *vat_node;
	int error;
	uint32_t early_vat_loc, vat_loc;
	uint16_t tagid;
	uint8_t file_type;

	vat_node = NULL;

	early_vat_loc = ump->first_possible_vat_location;
	vat_loc = ump->last_possible_vat_location;

	/* start looking from the end of the range */
	do {
		if (vat_node != NULL) {
			udf_dispose_node(vat_node);
			vat_node = NULL;
		}

		error = udf_read_phys_dscr(ump, vat_loc, M_UDFTEMP, &dscr);
		if (!error && dscr) { /* dscr will be null if zeros were read */
			tagid = le16toh(dscr->tag.id);
			file_type = 0;
			if (tagid == TAGID_FENTRY)
			    file_type = dscr->fe.icbtag.file_type;
			else if (tagid == TAGID_EXTFENTRY)
			    file_type = dscr->efe.icbtag.file_type; 
			free(dscr, M_UDFTEMP);

			if (file_type == 248)
			{
				icb_loc.loc.part_num = 
				    htole16(UDF_VTOP_RAWPART);
				icb_loc.loc.lb_num = htole32(vat_loc);
				error = udf_get_node(ump, icb_loc, &vat_node);
				if (error == 0)
					error = udf_check_for_vat(vat_node);
				if (error == 0)
					break;
			}
		}
printf("VAT not found at last possible location\n");
		vat_loc--;	/* walk backwards */
	} while (vat_loc >= early_vat_loc);

	/* keep our VAT node around */
	ump->vat_node = vat_node;

	return (error);
}

static int
udf_read_sparables(struct udf_mount *ump, union udf_pmap *mapping)
{
	union dscrptr *dscr;
	struct part_map_spare *pms = &mapping->pms;
	int error, spar;
	uint32_t lb_num;

	/*
	 * The partition mapping passed on to us specifies the information we
	 * need to locate and initialise the sparable partition mapping
	 * information we need.
	 */

	ump->sparable_packet_size = le16toh(pms->packet_len);
	/*KASSERT(ump->sparable_packet_size >= ump->packet_size); */   /* XXX */

	for (spar = 0; spar < pms->n_st; spar++) {
		lb_num = pms->st_loc[spar];
		error = udf_read_phys_dscr(ump, lb_num, M_UDFTEMP, &dscr);
		if (!error && dscr) {
			if (le16toh(dscr->tag.id) == TAGID_SPARING_TABLE) {
				if (ump->sparing_table)
					free(ump->sparing_table, M_UDFTEMP);
				ump->sparing_table = &dscr->spt;
				dscr = NULL;
				break;	/* we're done */
			}
		}
		if (dscr)
			free(dscr, M_UDFTEMP);
	}

	if (ump->sparing_table)
		return (0);

	return (ENOENT);
}

static int
udf_read_metadata_nodes(struct udf_mount *ump, union udf_pmap *mapping)
{
	struct part_map_meta *pmm = &mapping->pmm;
	struct long_ad icb_loc;
	int error = 0;

	/* extract our allocation parameters set up on format */
	ump->metadata_alloc_unit_size = le32toh(mapping->pmm.alloc_unit_size);
	ump->metadata_alignment_unit_size = 
	    le16toh(mapping->pmm.alignment_unit_size);
	ump->metadata_flags = mapping->pmm.flags;

	/* DPRINTF(VOLUMES, ("Reading in Metadata files\n")); */
	icb_loc.loc.part_num = pmm->part_num;
	icb_loc.loc.lb_num = pmm->meta_file_lbn;
	/* DPRINTF(VOLUMES, ("Metadata file\n")); */
	udf_get_node(ump, icb_loc, &ump->metadata_node);
#if 0
	if (ump->metadata_node) {
		vp = ump->metadata_node->vnode;
		UDF_SET_SYSTEMFILE(vp);
	}
#endif

	icb_loc.loc.lb_num = pmm->meta_mirror_file_lbn;
	if (icb_loc.loc.lb_num != -1) {
		/* DPRINTF(VOLUMES, ("Metadata copy file\n")); */
		udf_get_node(ump, icb_loc, &ump->metadatamirror_node);
#if 0
		if (ump->metadatamirror_node) {
			vp = ump->metadatamirror_node->vnode;
			UDF_SET_SYSTEMFILE(vp);
		}
#endif
	}

	icb_loc.loc.lb_num = pmm->meta_bitmap_file_lbn;
	if (icb_loc.loc.lb_num != -1) {
		/* DPRINTF(VOLUMES, ("Metadata bitmap file\n")); */
		udf_get_node(ump, icb_loc, &ump->metadatabitmap_node);
#if 0
		if (ump->metadatabitmap_node) {
			vp = ump->metadatabitmap_node->vnode;
			UDF_SET_SYSTEMFILE(vp);
		}
#endif
	}

	/* if we're mounting read-only we relax the requirements */
	if (ump->vfs_mountp->mnt_flag & MNT_RDONLY) {
		error = EFAULT;
		if (ump->metadata_node != NULL)
			error = 0;
		if (ump->metadata_node == NULL && 
		    ump->metadatamirror_node != NULL) {
			printf( "udf mount: Metadata file not readable, "
				"substituting Metadata copy file\n");
			ump->metadata_node = ump->metadatamirror_node;
			ump->metadatamirror_node = NULL;
			error = 0;
		}
	} else {
		/* mounting read/write */
		/* XXX DISABLED! metadata writing is not working yet XXX */
		if (error != 0)
			error = EROFS;
	}
	/* DPRINTFIF(VOLUMES, error, ("udf mount: failed to read "
				   "metadata files\n")); */
	return (error);
}

int
udf_read_vds_tables(struct udf_mount *ump)
{
	union udf_pmap *mapping;
	int pmap_size, error;
	uint32_t log_part, mt_l, n_pm;
	uint8_t *pmap_pos;

	error = 0;

	/* Iterate (again) over the part mappings for locations   */
	n_pm = le32toh(ump->logical_vol->n_pm);   /* num partmaps         */
	mt_l = le32toh(ump->logical_vol->mt_l);   /* partmaps data length */
	pmap_pos = ump->logical_vol->maps;

	for (log_part = 0; log_part < n_pm; log_part++) {
		mapping = (union udf_pmap *) pmap_pos;
		switch (ump->vtop_tp[log_part]) {
		case UDF_VTOP_TYPE_PHYS:
			/* nothing */
			break;
		case UDF_VTOP_TYPE_VIRT:
			/* search and load VAT */
			error = udf_search_vat(ump);
			if (error != 0)
				return (ENOENT);
			break;
		case UDF_VTOP_TYPE_SPARABLE:
			/* load one of the sparable tables */
			error = udf_read_sparables(ump, mapping);
			if (error != 0)
				return (ENOENT);
			break;
		case UDF_VTOP_TYPE_META:
			/* load the associated file descriptors */
			error = udf_read_metadata_nodes(ump, mapping);
			if (error != 0)
				return (ENOENT);
			break;
		default:
			break;
		}
		pmap_size = pmap_pos[1];
		pmap_pos += pmap_size;
	}

	/* read in and check unallocated and free space info if writing */
#if 0
	if ((ump->vfs_mountp->mnt_flag & MNT_RDONLY) == 0) {
		error = udf_read_physical_partition_spacetables(ump);
		if (error != 0)
			return (error);

		/* also read in metadata partition spacebitmap if defined */
		error = udf_read_metadata_partition_spacetable(ump);
		return (error);
	}
#endif
	return (0);
}

int
udf_read_rootdirs(struct udf_mount *ump)
{
	union dscrptr *dscr;
	/* struct udf_args *args = &ump->mount_args; */
	struct mount *mp;
	struct vnode *rootdir_node, *streamdir_node;
	struct long_ad *dir_loc, fsd_loc;
	ino_t ino;
	int dscr_type, error;
	uint32_t dummy, fsd_len, lb_num;

	mp = ump->vfs_mountp;

	/* TODO implement FSD reading in separate function like integrity? */
	/* get fileset descriptor sequence */
	fsd_loc = ump->logical_vol->lv_fsd_loc;
	fsd_len = le32toh(fsd_loc.len);

	dscr = NULL;
	error = 0;
	while (fsd_len > 0 || error != 0) {
		/* translate fsd_loc to lb_num */
		error = udf_translate_vtop(ump, &fsd_loc, &lb_num, &dummy);
		if (error != 0)
			break;
		error = udf_read_phys_dscr(ump, lb_num, M_UDFTEMP, &dscr);
		/* end markers */
		if (error != 0 || dscr == NULL)
			break;

		/* analyse */
		dscr_type = le16toh(dscr->tag.id);
		if (dscr_type == TAGID_TERM)
			break;
		if (dscr_type != TAGID_FSD) {
			free(dscr, M_UDFTEMP);
			return (ENOENT);
		}

		/*
		 * TODO check for multiple fileset descriptors; its only
		 * picking the last now. Also check for FSD
		 * correctness/interpretability
		 */

		/* update */
		if (ump->fileset_desc != NULL) {
			free(ump->fileset_desc, M_UDFTEMP);
		}
		ump->fileset_desc = &dscr->fsd;
		dscr = NULL;

		/* continue to the next fsd */
		fsd_len -= ump->sector_size;
		fsd_loc.loc.lb_num = htole32(le32toh(fsd_loc.loc.lb_num) + 1);

		/* follow up to fsd->next_ex (long_ad) if its not null */
		if (le32toh(ump->fileset_desc->next_ex.len)) {
			fsd_loc = ump->fileset_desc->next_ex;
			fsd_len = le32toh(ump->fileset_desc->next_ex.len);
		}
	}
	if (dscr != NULL)
		free(dscr, M_UDFTEMP);

	/* there has to be one */
	if (ump->fileset_desc == NULL)
		return (ENOENT);

	udf_update_logvolname(ump, ump->logical_vol->logvol_id);

	/*
	 * Now the FSD is known, read in the rootdirectory and if one exists,
	 * the system stream dir. Some files in the system streamdir are not
	 * wanted in this implementation since they are not maintained. If
	 * writing is enabled we'll delete these files if they exist.
	 */

	rootdir_node = streamdir_node = NULL;
	dir_loc = NULL;

	/* try to read in the rootdir */
	dir_loc = &ump->fileset_desc->rootdir_icb;
	error = udf_get_node_id(*dir_loc, &ino);
	if (error == 0)
		error = udf_vget(mp, ino, LK_EXCLUSIVE, &rootdir_node);
	if (error != 0)
		return (ENOENT);

	/* aparently it read in fine */

	/*
	 * Try the system stream directory; not very likely in the ones we
	 * test, but for completeness.
	 */
	dir_loc = &ump->fileset_desc->streamdir_icb;
	if (le32toh(dir_loc->len)) {
		printf("udf_read_rootdirs: streamdir defined ");
		error = udf_get_node_id(*dir_loc, &ino);
		if (error == 0)
			error = udf_vget(mp, ino, LK_EXCLUSIVE, 
			    &streamdir_node);
		if (error != 0)
			printf("but error in streamdir reading\n");
		else {
			printf("but ignored\n");
			/*
			 * TODO process streamdir `baddies' i.e. files we dont
			 * want if R/W
			 */
		}
	}

	/* release the vnodes again; they'll be auto-recycled later */
	if (streamdir_node != NULL)
		vput(streamdir_node);
	if (rootdir_node != NULL)
		vput(rootdir_node);

	return (0);
}

/* 
 * To make absolutely sure we are NOT returning zero, add one.  This can fail,
 * but in final version should probably never fail.
 */
int
udf_get_node_id(const struct long_ad icbptr, ino_t *ino)
{
	uint32_t blkn;
	uint16_t part;

	/* Just for now, this should be done in another way. */
	blkn = le32toh(icbptr.loc.lb_num);
	part = le16toh(icbptr.loc.part_num);

	if ((blkn + 1) & 0xE0000000) {
		printf("Block number too large to convert to inode number.\n");
		return EDOOFUS;
	}
	if (part & 0xFFF8) {
		printf("Partition number too large to convert to inode "
		    "number.\n");
		return EDOOFUS;
	}

	*ino = (blkn + 1) | (part << 29);
	//printf("Raw blkno: %u, raw part: %u\n", icbptr->loc.lb_num, icbptr->loc.part_num);
	//printf("udf_get_node_id -- blkno: %u, part: %u, ino: %u\n", blkn, part, ino);

	return (0);
}

void
udf_get_node_longad(const ino_t ino, struct long_ad *icbptr)
{
	uint32_t blkn, ino2;
	uint16_t part;

	/* Just for now, this should be done in another way. */
	ino2 = ino;
	blkn = (ino2 & 0x1FFFFFFF) - 1;
	part = (ino2 & 0xE0000000) >> 29;

	icbptr->loc.lb_num = htole32(blkn);
	icbptr->loc.part_num = htole16(part);

	//printf("Raw blkno: %u, raw part: %u\n", icbptr->loc.lb_num, icbptr->loc.part_num);
	//printf("udf_get_node_longad -- blkno: %u, part: %u, ino: %u\n", blkn, part, ino2);
}

#if 0
int
udf_compare_icb(const struct long_ad *a, const struct long_ad *b)
{
	if (le16toh(a->loc.part_num) < le16toh(b->loc.part_num))
		return (-1);
	if (le16toh(a->loc.part_num) > le16toh(b->loc.part_num))
		return (1);

	if (le32toh(a->loc.lb_num) < le32toh(b->loc.lb_num))
		return (-1);
	if (le32toh(a->loc.lb_num) > le32toh(b->loc.lb_num))
		return (1);

	return (0);
}

static int
udf_compare_rbnodes(const struct rb_node *a, const struct rb_node *b)
{
	struct udf_node *a_node = RBTOUDFNODE(a);
	struct udf_node *b_node = RBTOUDFNODE(b);

	return (udf_compare_icb(&a_node->loc, &b_node->loc));
}

static int
udf_compare_rbnode_icb(const struct rb_node *a, const void *key)
{
	struct udf_node *a_node = RBTOUDFNODE(a);
	const struct long_ad * const icb = key;

	return (udf_compare_icb(&a_node->loc, icb));
}

static const struct rb_tree_ops udf_node_rbtree_ops = {
	.rbto_compare_nodes = udf_compare_rbnodes,
	.rbto_compare_key   = udf_compare_rbnode_icb,
};

void
udf_init_nodes_tree(struct udf_mount *ump)
{
	rb_tree_init(&ump->udf_node_tree, &udf_node_rbtree_ops);
}

static struct udf_node *
udf_node_lookup(struct udf_mount *ump, struct long_ad *icbptr)
{
	struct rb_node  *rb_node;
	struct udf_node *udf_node;
	struct vnode *vp;

loop:
	mutex_enter(&ump->ihash_lock);

	rb_node = rb_tree_find_node(&ump->udf_node_tree, icbptr);
	if (rb_node) {
		udf_node = RBTOUDFNODE(rb_node);
		vp = udf_node->vnode;
		assert(vp);
		mutex_enter(&vp->v_interlock);
		mutex_exit(&ump->ihash_lock);
		if (vget(vp, LK_EXCLUSIVE))
			goto loop;
		return (udf_node);
	}
	mutex_exit(&ump->ihash_lock);

	return (NULL);
}

static void
udf_register_node(struct udf_node *udf_node)
{
	struct udf_mount *ump = udf_node->ump;

	/* add node to the rb tree */
	mutex_enter(&ump->ihash_lock);
		rb_tree_insert_node(&ump->udf_node_tree, &udf_node->rbnode);
	mutex_exit(&ump->ihash_lock);
}

static void
udf_deregister_node(struct udf_node *udf_node) 
{
	struct udf_mount *ump = udf_node->ump;

	/* remove node from the rb tree */
	mutex_enter(&ump->ihash_lock);
		rb_tree_remove_node(&ump->udf_node_tree, &udf_node->rbnode);
	mutex_exit(&ump->ihash_lock);
}

static int
udf_validate_session_start(struct udf_mount *ump)
{
	struct mmc_trackinfo trackinfo;
	struct vrs_desc *vrs;
	uint32_t tracknr, sessionnr, sector, sector_size;
	uint32_t iso9660_vrs, write_track_start;
	uint8_t *buffer, *blank, *pos;
	int blks, max_sectors, vrs_len;
	int error;

	/* disc appendable? */
	if (ump->discinfo.disc_state == MMC_STATE_FULL)
		return (EROFS);

	/* already written here? if so, there should be an ISO VDS */
	if (ump->discinfo.last_session_state == MMC_STATE_INCOMPLETE)
		return (0);

	/*
	 * Check if the first track of the session is blank and if so, copy or
	 * create a dummy ISO descriptor so the disc is valid again.
	 */

	tracknr = ump->discinfo.first_track_last_session;
	memset(&trackinfo, 0, sizeof(struct mmc_trackinfo));
	trackinfo.tracknr = tracknr;
	error = udf_update_trackinfo(ump, &trackinfo);
	if (error != 0)
		return (error);

	udf_dump_trackinfo(&trackinfo);
	KASSERT(trackinfo.flags & (MMC_TRACKINFO_BLANK | MMC_TRACKINFO_RESERVED));
	KASSERT(trackinfo.sessionnr > 1);

	KASSERT(trackinfo.flags & MMC_TRACKINFO_NWA_VALID);
	write_track_start = trackinfo.next_writable;

	/* we have to copy the ISO VRS from a former session */

	/* sessionnr should be the session we're mounting */
	sessionnr = ump->mount_args.sessionnr;

	/* start at the first track */
	tracknr   = ump->discinfo.first_track;
	while (tracknr <= ump->discinfo.num_tracks) {
		trackinfo.tracknr = tracknr;
		error = udf_update_trackinfo(ump, &trackinfo);
		if (error != 0)
			return (error);
		if (trackinfo.sessionnr == sessionnr)
			break;
		tracknr++;
	}
	if (trackinfo.sessionnr != sessionnr)
		return (ENOENT);

	udf_dump_trackinfo(&trackinfo);

        /*
         * location of iso9660 vrs is defined as first sector AFTER 32kb,
         * minimum ISO `sector size' 2048
         */
	sector_size = ump->discinfo.sector_size;
	iso9660_vrs = ((32*1024 + sector_size - 1) / sector_size)
		 + trackinfo.track_start;

	buffer = malloc(UDF_ISO_VRS_SIZE, M_UDFTEMP, M_WAITOK);
	max_sectors = UDF_ISO_VRS_SIZE / sector_size;
	blks = MAX(1, 2048 / sector_size);

	error = 0;
	for (sector = 0; sector < max_sectors; sector += blks) {
		pos = buffer + sector * sector_size;
		error = udf_read_phys_sectors(ump, UDF_C_DSCR, pos,
			iso9660_vrs + sector, blks);
		if (error != 0)
			break;
		/* check this ISO descriptor */
		vrs = (struct vrs_desc *) pos;
		if (strncmp(vrs->identifier, VRS_CD001, 5) == 0)
			continue;
		if (strncmp(vrs->identifier, VRS_CDW02, 5) == 0)
			continue;
		if (strncmp(vrs->identifier, VRS_BEA01, 5) == 0)
			continue;
		if (strncmp(vrs->identifier, VRS_NSR02, 5) == 0)
			continue;
		if (strncmp(vrs->identifier, VRS_NSR03, 5) == 0)
			continue;
		if (strncmp(vrs->identifier, VRS_TEA01, 5) == 0)
			break;
		/* now what? for now, end of sequence */
		break;
	}
	vrs_len = sector + blks;
	if (error != 0) {
		memset(buffer, 0, UDF_ISO_VRS_SIZE);

		vrs = (struct vrs_desc *) (buffer);
		vrs->struct_type = 0;
		vrs->version     = 1;
		memcpy(vrs->identifier,VRS_BEA01, 5);

		vrs = (struct vrs_desc *) (buffer + 2048);
		vrs->struct_type = 0;
		vrs->version     = 1;
		if (le16toh(ump->logical_vol->tag.descriptor_ver) == 2) {
			memcpy(vrs->identifier,VRS_NSR02, 5);
		} else {
			memcpy(vrs->identifier,VRS_NSR03, 5);
		}

		vrs = (struct vrs_desc *) (buffer + 4096);
		vrs->struct_type = 0;
		vrs->version     = 1;
		memcpy(vrs->identifier, VRS_TEA01, 5);

		vrs_len = 3*blks;
	}

        /*
         * location of iso9660 vrs is defined as first sector AFTER 32kb,
         * minimum ISO `sector size' 2048
         */
	sector_size = ump->discinfo.sector_size;
	iso9660_vrs = ((32*1024 + sector_size - 1) / sector_size)
		 + write_track_start;

	/* write out 32 kb */
	blank = malloc(sector_size, M_UDFTEMP, M_WAITOK);
	memset(blank, 0, sector_size);
	error = 0;
	for (sector = write_track_start; sector < iso9660_vrs; sector ++) {
		error = udf_write_phys_sectors(ump, UDF_C_ABSOLUTE,
			blank, sector, 1);
		if (error != 0)
			break;
	}
	if (error == 0) {
		/* write out our ISO VRS */
		KASSERT(sector == iso9660_vrs);
		error = udf_write_phys_sectors(ump, UDF_C_ABSOLUTE, buffer,
				sector, vrs_len);
		sector += vrs_len;
	}
	if (error == 0) {
		/* fill upto the first anchor at S+256 */
		for (; sector < write_track_start+256; sector++) {
			error = udf_write_phys_sectors(ump, UDF_C_ABSOLUTE,
				blank, sector, 1);
			if (error != 0)
				break;
		}
	}
	if (error == 0) {
		/* write out anchor; write at ABSOLUTE place! */
		error = udf_write_phys_dscr_sync(ump, NULL, UDF_C_ABSOLUTE,
			(union dscrptr *) ump->anchors[0], sector, sector);
		if (error != 0)
			printf("writeout of anchor failed!\n");
	}

	free(blank, M_UDFTEMP);
	free(buffer, M_UDFTEMP);

	if (error != 0)
		printf("udf_open_session: error writing iso vrs! : "
				"leaving disc in compromised state!\n");

	/* synchronise device caches */
	(void) udf_synchronise_caches(ump);

	return (error);
}

int
udf_open_logvol(struct udf_mount *ump)
{
	int logvol_integrity;
	int error;

	/* already/still open? */
	logvol_integrity = le32toh(ump->logvol_integrity->integrity_type);
	if (logvol_integrity == UDF_INTEGRITY_OPEN)
		return (0);

	/* can we open it ? */
	if (ump->vfs_mountp->mnt_flag & MNT_RDONLY)
		return (EROFS);

	/* setup write parameters */
	if ((error = udf_setup_writeparams(ump)) != 0)
		return (error);

	/* determine data and metadata tracks (most likely same) */
	error = udf_search_writing_tracks(ump);
	if (error != 0) {
		/* most likely lack of space */
		printf("udf_open_logvol: error searching writing tracks\n");
		return (EROFS);
	}

	/* writeout/update lvint on disc or only in memory */
	if (ump->lvopen & UDF_OPEN_SESSION) {
		/* TODO optional track reservation opening */
		error = udf_validate_session_start(ump);
		if (error != 0)
			return (error);

		/* determine data and metadata tracks again */
		error = udf_search_writing_tracks(ump);
	}

	/* mark it open */
	ump->logvol_integrity->integrity_type = htole32(UDF_INTEGRITY_OPEN);

	/* do we need to write it out? */
	if (ump->lvopen & UDF_WRITE_LVINT) {
		error = udf_writeout_lvint(ump, ump->lvopen);
		/* if we couldn't write it mark it closed again */
		if (error != 0) {
			ump->logvol_integrity->integrity_type =
						htole32(UDF_INTEGRITY_CLOSED);
			return (error);
		}
	}

	return (0);
}
#endif

#if 0
int
udf_close_logvol(struct udf_mount *ump, int mntflags)
{
	struct vnode *devvp = ump->devvp;
	struct mmc_op mmc_op;
	int logvol_integrity;
	int error = 0, error1 = 0, error2 = 0;
	int tracknr;
	int nvats, n, nok;

	/* already/still closed? */
	logvol_integrity = le32toh(ump->logvol_integrity->integrity_type);
	if (logvol_integrity == UDF_INTEGRITY_CLOSED)
		return (0);

	/* writeout/update lvint or write out VAT */
	DPRINTF(VOLUMES, ("udf_close_logvol: closing logical volume\n"));
#ifdef DIAGNOSTIC
	if (ump->lvclose & UDF_CLOSE_SESSION)
		KASSERT(ump->lvclose & UDF_WRITE_VAT);
#endif

	if (ump->lvclose & UDF_WRITE_VAT) {
		DPRINTF(VOLUMES, ("lvclose & UDF_WRITE_VAT\n"));

		/* write out the VAT data and all its descriptors */
		DPRINTF(VOLUMES, ("writeout vat_node\n"));
		udf_writeout_vat(ump);
		vflushbuf(ump->vat_node->vnode, 1 /* sync */);

		(void) VOP_FSYNC(ump->vat_node->vnode,
				FSCRED, FSYNC_WAIT, 0, 0);

		if (ump->lvclose & UDF_CLOSE_SESSION) {
			DPRINTF(VOLUMES, ("udf_close_logvol: closing session "
				"as requested\n"));
		}

		/* at least two DVD packets and 3 CD-R packets */
		nvats = 32;

#if notyet
		/*
		 * TODO calculate the available space and if the disc is
		 * allmost full, write out till end-256-1 with banks, write
		 * AVDP and fill up with VATs, then close session and close
		 * disc.
		 */
		if (ump->lvclose & UDF_FINALISE_DISC) {
			error = udf_write_phys_dscr_sync(ump, NULL,
					UDF_C_FLOAT_DSCR,
					(union dscrptr *) ump->anchors[0],
					0, 0);
			if (error != 0)
				printf("writeout of anchor failed!\n");

			/* pad space with VAT ICBs */
			nvats = 256;
		}
#endif

		/* write out a number of VAT nodes */
		nok = 0;
		for (n = 0; n < nvats; n++) {
			/* will now only write last FE/EFE */
			ump->vat_node->i_flags |= IN_MODIFIED;
			error = VOP_FSYNC(ump->vat_node->vnode,
					FSCRED, FSYNC_WAIT, 0, 0);
			if (error == 0)
				nok++;
		}
		if (nok < 14) {
			/* arbitrary; but at least one or two CD frames */
			printf("writeout of at least 14 VATs failed\n");
			return (error);
		}
	}

	/* NOTE the disc is in a (minimal) valid state now; no erroring out */

	/* finish closing of session */
	if (ump->lvclose & UDF_CLOSE_SESSION) {
		error = udf_validate_session_start(ump);
		if (error != 0)
			return (error);

		(void) udf_synchronise_caches(ump);

		/* close all associated tracks */
		tracknr = ump->discinfo.first_track_last_session;
		error = 0;
		while (tracknr <= ump->discinfo.last_track_last_session) {
			DPRINTF(VOLUMES, ("\tclosing possible open "
				"track %d\n", tracknr));
			memset(&mmc_op, 0, sizeof(mmc_op));
			mmc_op.operation   = MMC_OP_CLOSETRACK;
			mmc_op.mmc_profile = ump->discinfo.mmc_profile;
			mmc_op.tracknr     = tracknr;
			error = VOP_IOCTL(devvp, MMCOP, &mmc_op,
					FKIOCTL, NOCRED);
			if (error != 0)
				printf("udf_close_logvol: closing of "
					"track %d failed\n", tracknr);
			tracknr ++;
		}
		if (error == 0) {
			DPRINTF(VOLUMES, ("closing session\n"));
			memset(&mmc_op, 0, sizeof(mmc_op));
			mmc_op.operation   = MMC_OP_CLOSESESSION;
			mmc_op.mmc_profile = ump->discinfo.mmc_profile;
			mmc_op.sessionnr   = ump->discinfo.num_sessions;
			error = VOP_IOCTL(devvp, MMCOP, &mmc_op,
					FKIOCTL, NOCRED);
			if (error != 0)
				printf("udf_close_logvol: closing of session"
						"failed\n");
		}
		if (error == 0)
			ump->lvopen |= UDF_OPEN_SESSION;
		if (error != 0) {
			printf("udf_close_logvol: leaving disc as it is\n");
			ump->lvclose &= ~UDF_FINALISE_DISC;
		}
	}

	if (ump->lvclose & UDF_FINALISE_DISC) {
		memset(&mmc_op, 0, sizeof(mmc_op));
		mmc_op.operation   = MMC_OP_FINALISEDISC;
		mmc_op.mmc_profile = ump->discinfo.mmc_profile;
		mmc_op.sessionnr   = ump->discinfo.num_sessions;
		error = VOP_IOCTL(devvp, MMCOP, &mmc_op,
				FKIOCTL, NOCRED);
		if (error != 0)
			printf("udf_close_logvol: finalising disc"
					"failed\n");
	}

	/* write out partition bitmaps if requested */
	if (ump->lvclose & UDF_WRITE_PART_BITMAPS) {
		/* sync writeout metadata spacetable if existing */
		error1 = udf_write_metadata_partition_spacetable(ump, true);
		if (error1)
			printf( "udf_close_logvol: writeout of metadata space "
				"bitmap failed\n");

		/* sync writeout partition spacetables */
		error2 = udf_write_physical_partition_spacetables(ump, true);
		if (error2)
			printf( "udf_close_logvol: writeout of space tables "
				"failed\n");

		if (error1 || error2)
			return (error1 | error2);

		ump->lvclose &= ~UDF_WRITE_PART_BITMAPS;
	}

	/* write out metadata partition nodes if requested */
	if (ump->lvclose & UDF_WRITE_METAPART_NODES) {
		/* sync writeout metadata descriptor node */
		error1 = udf_writeout_node(ump->metadata_node, FSYNC_WAIT);
		if (error1)
			printf( "udf_close_logvol: writeout of metadata partition "
				"node failed\n");

		/* duplicate metadata partition descriptor if needed */
		udf_synchronise_metadatamirror_node(ump);

		/* sync writeout metadatamirror descriptor node */
		error2 = udf_writeout_node(ump->metadatamirror_node, FSYNC_WAIT);
		if (error2)
			printf( "udf_close_logvol: writeout of metadata partition "
				"mirror node failed\n");

		if (error1 || error2)
			return (error1 | error2);

		ump->lvclose &= ~UDF_WRITE_METAPART_NODES;
	}

	/* mark it closed */
	ump->logvol_integrity->integrity_type = htole32(UDF_INTEGRITY_CLOSED);

	/* do we need to write out the logical volume integrity? */
	if (ump->lvclose & UDF_WRITE_LVINT)
		error = udf_writeout_lvint(ump, ump->lvopen);
	if (error != 0) {
		/* HELP now what? mark it open again for now */
		ump->logvol_integrity->integrity_type =
			htole32(UDF_INTEGRITY_OPEN);
		return (error);
	}

	(void) udf_synchronise_caches(ump);

	return (0);
}
#endif

/*
 * Genfs interfacing
 *
 * static const struct genfs_ops udf_genfsops = {
 * 	.gop_size = genfs_size,
 * 		size of transfers
 * 	.gop_alloc = udf_gop_alloc,
 * 		allocate len bytes at offset
 * 	.gop_write = genfs_gop_write,
 * 		putpages interface code
 * 	.gop_markupdate = udf_gop_markupdate,
 * 		set update/modify flags etc.
 * }
 */

/*
 * Genfs interface. These four functions are the only ones defined though not
 * documented... great....
 */

/*
 * Called for allocating an extent of the file either by VOP_WRITE() or by
 * genfs filling up gaps.
 */
#if 0
static int
udf_gop_alloc(struct vnode *vp, off_t off,
    off_t len, int flags, kauth_cred_t cred)
{
	struct udf_node *udf_node = VTOI(vp);
	struct udf_mount *ump = udf_node->ump;
	uint64_t lb_start, lb_end;
	uint32_t lb_size, num_lb;
	int udf_c_type, vpart_num, can_fail;
	int error;

	DPRINTF(ALLOC, ("udf_gop_alloc called for offset %"PRIu64" for %"PRIu64" bytes, %s\n",
		off, len, flags? "SYNC":"NONE"));

	/*
	 * request the pages of our vnode and see how many pages will need to
	 * be allocated and reserve that space
	 */
	lb_size  = le32toh(udf_node->ump->logical_vol->lb_size);
	lb_start = off / lb_size;
	lb_end   = (off + len + lb_size -1) / lb_size;
	num_lb   = lb_end - lb_start;

	udf_c_type = udf_get_c_type(udf_node);
	vpart_num  = udf_get_record_vpart(ump, udf_c_type);

	/* all requests can fail */
	can_fail   = true;

	/* fid's (directories) can't fail */
	if (udf_c_type == UDF_C_FIDS)
		can_fail   = false;

	/* system files can't fail */
	if (vp->v_vflag & VV_SYSTEM)
		can_fail = false;

	error = udf_reserve_space(ump, udf_node, udf_c_type,
		vpart_num, num_lb, can_fail);

	DPRINTF(ALLOC, ("\tlb_start %"PRIu64", lb_end %"PRIu64", num_lb %d\n",
		lb_start, lb_end, num_lb));

	return (error);
}

/*
 * callback from genfs to update our flags
 */
static void
udf_gop_markupdate(struct vnode *vp, int flags)
{
	struct udf_node *udf_node = VTOI(vp);
	u_long mask = 0;

	if ((flags & GOP_UPDATE_ACCESSED) != 0) {
		mask = IN_ACCESS;
	}
	if ((flags & GOP_UPDATE_MODIFIED) != 0) {
		if (vp->v_type == VREG) {
			mask |= IN_CHANGE | IN_UPDATE;
		} else {
			mask |= IN_MODIFY;
		}
	}
	if (mask) {
		udf_node->i_flags |= mask;
	}
}

static const struct genfs_ops udf_genfsops = {
	.gop_size = genfs_size,
	.gop_alloc = udf_gop_alloc,
	.gop_write = genfs_gop_write_rwmap,
	.gop_markupdate = udf_gop_markupdate,
};

int
udf_write_terminator(struct udf_mount *ump, uint32_t sector)
{
	union dscrptr *dscr;
	int error;

	dscr = malloc(ump->discinfo.sector_size, M_UDFTEMP, M_WAITOK|M_ZERO);
	udf_inittag(ump, &dscr->tag, TAGID_TERM, sector);

	/* CRC length for an anchor is 512 - tag length; defined in Ecma 167 */
	dscr->tag.desc_crc_len = le16toh(512-UDF_DESC_TAG_LENGTH);
	(void) udf_validate_tag_and_crc_sums(dscr);

	error = udf_write_phys_dscr_sync(ump, NULL, UDF_C_DSCR,
			dscr, sector, sector);

	free(dscr, M_UDFTEMP);

	return (error);
}
#endif

/* UDF<->unix converters */

static mode_t
udf_perm_to_unix_mode(uint32_t perm)
{
	mode_t mode;

	mode = ((perm & UDF_FENTRY_PERM_USER_MASK));
	mode |= ((perm & UDF_FENTRY_PERM_GRP_MASK) >> 2);
	mode |= ((perm & UDF_FENTRY_PERM_OWNER_MASK) >> 4);

	return (mode);
}

#if 0
static uint32_t
unix_mode_to_udf_perm(mode_t mode)
{
	uint32_t perm;
	
	perm  = ((mode & S_IRWXO)     );
	perm |= ((mode & S_IRWXG) << 2);
	perm |= ((mode & S_IRWXU) << 4);
	perm |= ((mode & S_IWOTH) << 3);
	perm |= ((mode & S_IWGRP) << 5);
	perm |= ((mode & S_IWUSR) << 7);

	return (perm);
}
#endif

static uint32_t
udf_icb_to_unix_filetype(uint32_t icbftype)
{
	switch (icbftype) {
	case UDF_ICB_FILETYPE_DIRECTORY:
	case UDF_ICB_FILETYPE_STREAMDIR:
		return (S_IFDIR);
	case UDF_ICB_FILETYPE_FIFO:
		return (S_IFIFO);
	case UDF_ICB_FILETYPE_CHARDEVICE:
		return (S_IFCHR);
	case UDF_ICB_FILETYPE_BLOCKDEVICE:
		return (S_IFBLK);
	case UDF_ICB_FILETYPE_RANDOMACCESS:
	case UDF_ICB_FILETYPE_REALTIME:
		return (S_IFREG);
	case UDF_ICB_FILETYPE_SYMLINK:
		return (S_IFLNK);
	case UDF_ICB_FILETYPE_SOCKET:
		return (S_IFSOCK);
	}
	/* no idea what this is */
	return (0);
}

/* These timestamp_to_timespec functions are done. */

static int 
udf_leapyear(int year)
{
	int i;

	i = (year % 400 == 0) ? 1 : 0;
	i |= (year % 100 == 0) ? 0 : 1; 
	i &= (year % 4 == 0) ? 1 : 0;

	return (i);
}

void
udf_timestamp_to_timespec(struct udf_mount *ump,
			  struct timestamp *timestamp,
			  struct timespec  *timespec)
{
	time_t secs;
	const int days_to_mon[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243,
	    273, 304, 334};
	uint32_t nsecs, usecs;
	uint16_t tz, year;

	year = le16toh(timestamp->year);
	if (year < 1970 || timestamp->month < 1 || timestamp->month > 12) {
		timespec->tv_sec = 0;
		timespec->tv_nsec = 0;
		return;
	}

	secs = timestamp->second;
	secs += timestamp->minute * 60;
	secs += timestamp->hour * 3600;
	secs += (timestamp->day - 1) * 3600 * 24;
	secs += days_to_mon[timestamp->month - 1] * 3600 * 24;

	secs += (year - 1970) * 365 * 3600 * 24;
	secs += ((year - 1 - 1968) / 4) * 3600 * 24;

	if (year > 2100) {
		secs -= (((year - 1 - 2100) / 100) + 1) * 3600 * 24;
	}
	if (year > 2400) {
		secs += (((year - 1 - 2400) / 400) + 1) * 3600 * 24;
	}
	if (timestamp->month > 2) {
		secs += (time_t)udf_leapyear(year) * 3600 * 24;
	}

	usecs = timestamp->usec + 100 * timestamp->hund_usec + 
	    10000 * timestamp->centisec;
	nsecs = usecs * 1000;

	/*
	 * Calculate the time zone.  The timezone is 12 bit signed 2's
	 * compliment, so we gotta do some extra magic to handle it right.
	 */
	tz  = le16toh(timestamp->type_tz);
	tz &= 0x0fff;		/* only lower 12 bits are significant */
	if (tz & 0x0800)	/* sign extention */
		tz |= 0xf000;

	/*
	 * TODO check timezone conversion 
	 * check if we are specified a timezone to convert 
	 */
	if (le16toh(timestamp->type_tz) & 0x1000) {
		if ((int16_t)tz != -2047)
			secs -= (int16_t)tz * 60;
	} /* else {
		secs -= ump->mount_args.gmtoff;
	} */

	timespec->tv_sec = secs;
	timespec->tv_nsec = nsecs;
}

#if 0
void
udf_timespec_to_timestamp(struct timespec *timespec, struct timestamp *timestamp)
{
	struct clock_ymdhms ymdhms;
	uint32_t husec, usec, csec;

	(void) clock_secs_to_ymdhms(timespec->tv_sec, &ymdhms);

	usec   = timespec->tv_nsec / 1000;
	husec  =  usec / 100;
	usec  -= husec * 100;				/* only 0-99 in usec  */
	csec   = husec / 100;				/* only 0-99 in csec  */
	husec -=  csec * 100;				/* only 0-99 in husec */

	/* set method 1 for CUT/GMT */
	timestamp->type_tz	= htole16((1<<12) + 0);
	timestamp->year		= htole16(ymdhms.dt_year);
	timestamp->month	= ymdhms.dt_mon;
	timestamp->day		= ymdhms.dt_day;
	timestamp->hour		= ymdhms.dt_hour;
	timestamp->minute	= ymdhms.dt_min;
	timestamp->second	= ymdhms.dt_sec;
	timestamp->centisec	= csec;
	timestamp->hund_usec	= husec;
	timestamp->usec		= usec;
}
#endif

/*
 * Attribute and filetypes converters with get/set pairs
 */

uint32_t
udf_getaccessmode(struct udf_node *udf_node)
{
	struct file_entry *fe = udf_node->fe;
	struct extfile_entry *efe = udf_node->efe;
	uint32_t ftype, icbftype, mode, udf_perm;
	uint16_t icbflags;

	UDF_LOCK_NODE(udf_node, 0);

	if (fe != NULL) {
		udf_perm = le32toh(fe->perm);
		icbftype = fe->icbtag.file_type;
		icbflags = le16toh(fe->icbtag.flags);
	} else {
		/*assert(udf_node->efe != NULL); */
		udf_perm = le32toh(efe->perm);
		icbftype = efe->icbtag.file_type;
		icbflags = le16toh(efe->icbtag.flags);
	}

	mode = udf_perm_to_unix_mode(udf_perm);
	ftype = udf_icb_to_unix_filetype(icbftype);

	/* set suid, sgid, sticky from flags in fe/efe */
	if (icbflags & UDF_ICB_TAG_FLAGS_SETUID)
		mode |= S_ISUID;
	if (icbflags & UDF_ICB_TAG_FLAGS_SETGID)
		mode |= S_ISGID;
	if (icbflags & UDF_ICB_TAG_FLAGS_STICKY)
		mode |= S_ISVTX;

	UDF_UNLOCK_NODE(udf_node, 0);

	return (mode | ftype);
}

#if 0
void
udf_setaccessmode(struct udf_node *udf_node, mode_t mode)
{
	struct file_entry    *fe  = udf_node->fe;
	struct extfile_entry *efe = udf_node->efe;
	uint32_t udf_perm;
	uint16_t icbflags;

	UDF_LOCK_NODE(udf_node, 0);
	udf_perm = unix_mode_to_udf_perm(mode & ALLPERMS);
	if (fe != NULL) {
		icbflags = le16toh(fe->icbtag.flags);
	} else {
		icbflags = le16toh(efe->icbtag.flags);
	}

	icbflags &= ~UDF_ICB_TAG_FLAGS_SETUID;
	icbflags &= ~UDF_ICB_TAG_FLAGS_SETGID;
	icbflags &= ~UDF_ICB_TAG_FLAGS_STICKY;
	if (mode & S_ISUID)
		icbflags |= UDF_ICB_TAG_FLAGS_SETUID;
	if (mode & S_ISGID)
		icbflags |= UDF_ICB_TAG_FLAGS_SETGID;
	if (mode & S_ISVTX)
		icbflags |= UDF_ICB_TAG_FLAGS_STICKY;

	if (fe != NULL) {
		fe->perm  = htole32(udf_perm);
		fe->icbtag.flags  = htole16(icbflags);
	} else {
		efe->perm = htole32(udf_perm);
		efe->icbtag.flags = htole16(icbflags);
	}

	UDF_UNLOCK_NODE(udf_node, 0);
}

void
udf_getownership(struct udf_node *udf_node, uid_t *uidp, gid_t *gidp)
{
	struct udf_mount     *ump = udf_node->ump;
	struct file_entry    *fe  = udf_node->fe;
	struct extfile_entry *efe = udf_node->efe;
	uid_t uid;
	gid_t gid;

	UDF_LOCK_NODE(udf_node, 0);
	if (fe != NULL) {
		uid = (uid_t)le32toh(fe->uid);
		gid = (gid_t)le32toh(fe->gid);
	} else {
		assert(udf_node->efe != NULL);
		uid = (uid_t)le32toh(efe->uid);
		gid = (gid_t)le32toh(efe->gid);
	}
	
	/* do the uid/gid translation game */
	if (uid == (uid_t) -1)
		uid = ump->mount_args.anon_uid;
	if (gid == (gid_t) -1)
		gid = ump->mount_args.anon_gid;

	*uidp = uid;
	*gidp = gid;

	UDF_UNLOCK_NODE(udf_node, 0);
}

void
udf_setownership(struct udf_node *udf_node, uid_t uid, gid_t gid)
{
	struct udf_mount     *ump = udf_node->ump;
	struct file_entry    *fe  = udf_node->fe;
	struct extfile_entry *efe = udf_node->efe;
	uid_t nobody_uid;
	gid_t nobody_gid;

	UDF_LOCK_NODE(udf_node, 0);

	/* do the uid/gid translation game */
	nobody_uid = ump->mount_args.nobody_uid;
	nobody_gid = ump->mount_args.nobody_gid;
	if (uid == nobody_uid)
		uid = (uid_t) -1;
	if (gid == nobody_gid)
		gid = (gid_t) -1;

	if (fe != NULL) {
		fe->uid  = htole32((uint32_t) uid);
		fe->gid  = htole32((uint32_t) gid);
	} else {
		efe->uid = htole32((uint32_t) uid);
		efe->gid = htole32((uint32_t) gid);
	}

	UDF_UNLOCK_NODE(udf_node, 0);
}

static int
dirhash_fill(struct udf_node *dir_node)
{
	struct vnode *dvp = dir_node->vnode;
	struct dirhash *dirh;
	struct file_entry    *fe  = dir_node->fe;
	struct extfile_entry *efe = dir_node->efe;
	struct fileid_desc *fid;
	struct dirent *dirent;
	uint64_t file_size, pre_diroffset, diroffset;
	uint32_t lb_size;
	int error;

	/* make sure we have a dirhash to work on */
	dirh = dir_node->dir_hash;
	KASSERT(dirh);
	KASSERT(dirh->refcnt > 0);

	if (dirh->flags & DIRH_BROKEN)
		return (EIO);
	if (dirh->flags & DIRH_COMPLETE)
		return (0);

	/* make sure we have a clean dirhash to add to */
	dirhash_purge_entries(dirh);

	/* get directory filesize */
	if (fe != NULL) {
		file_size = le64toh(fe->inf_len);
	} else {
		assert(efe != NULL);
		file_size = le64toh(efe->inf_len);
	}

	/* allocate temporary space for fid */
	lb_size = le32toh(dir_node->ump->logical_vol->lb_size);
	fid = malloc(lb_size, M_UDFTEMP, M_WAITOK);

	/* allocate temporary space for dirent */
	dirent = malloc(sizeof(struct dirent), M_UDFTEMP, M_WAITOK);

	error = 0;
	diroffset = 0;
	while (diroffset < file_size) {
		/* transfer a new fid/dirent */
		pre_diroffset = diroffset;
		error = udf_read_fid_stream(dvp, &diroffset, fid, dirent);
		if (error != 0) {
			/* TODO what to do? continue but not add? */
			dirh->flags |= DIRH_BROKEN;
			dirhash_purge_entries(dirh);
			break;
		}

		if ((fid->file_char & UDF_FILE_CHAR_DEL)) {
			/* register deleted extent for reuse */
			dirhash_enter_freed(dirh, pre_diroffset,
				udf_fidsize(fid));
		} else {
			/* append to the dirhash */
			dirhash_enter(dirh, dirent, pre_diroffset,
				udf_fidsize(fid), 0);
		}
	}
	dirh->flags |= DIRH_COMPLETE;

	free(fid, M_UDFTEMP);
	free(dirent, M_UDFTEMP);

	return (error);
}

/*
 * Directory read and manipulation functions.
 *
 */

int 
udf_lookup_name_in_dir(struct vnode *vp, const char *name, int namelen,
       struct long_ad *icb_loc, int *found)
{
	struct udf_node  *dir_node = VTOI(vp);
	struct dirhash       *dirh;
	struct dirhash_entry *dirh_ep;
	struct fileid_desc *fid;
	struct dirent *dirent;
	uint64_t diroffset;
	uint32_t lb_size;
	int hit, error;

	/* set default return */
	*found = 0;

	/* get our dirhash and make sure its read in */
	dirhash_get(&dir_node->dir_hash);
	error = dirhash_fill(dir_node);
	if (error != 0) {
		dirhash_put(dir_node->dir_hash);
		return (error);
	}
	dirh = dir_node->dir_hash;

	/* allocate temporary space for fid */
	lb_size = le32toh(dir_node->ump->logical_vol->lb_size);
	fid     = malloc(lb_size, M_UDFTEMP, M_WAITOK);
	dirent  = malloc(sizeof(struct dirent), M_UDFTEMP, M_WAITOK);

	/* search our dirhash hits */
	memset(icb_loc, 0, sizeof(*icb_loc));
	dirh_ep = NULL;
	for (;;) {
		hit = dirhash_lookup(dirh, name, namelen, &dirh_ep);
		/* if no hit, abort the search */
		if (!hit)
			break;

		/* check this hit */
		diroffset = dirh_ep->offset;

		/* transfer a new fid/dirent */
		error = udf_read_fid_stream(vp, &diroffset, fid, dirent);
		if (error != 0)
			break;

		/* see if its our entry */
		KASSERT(dirent->d_namlen == namelen);
		if (strncmp(dirent->d_name, name, namelen) == 0) {
			*found = 1;
			*icb_loc = fid->icb;
			break;
		}
	}
	free(fid, M_UDFTEMP);
	free(dirent, M_UDFTEMP);

	dirhash_put(dir_node->dir_hash);

	return (error);
}

static int
udf_create_new_fe(struct udf_mount *ump, struct file_entry *fe, int file_type,
	struct long_ad *node_icb, struct long_ad *parent_icb,
	uint64_t parent_unique_id)
{
	struct timespec now;
	struct icb_tag *icb;
	struct filetimes_extattr_entry *ft_extattr;
	uint64_t unique_id;
	uint32_t fidsize, lb_num;
	uint8_t *bpos;
	int crclen, attrlen;

	lb_num = le32toh(node_icb->loc.lb_num);
	udf_inittag(ump, &fe->tag, TAGID_FENTRY, lb_num);
	icb = &fe->icbtag;

	/*
	 * Always use strategy type 4 unless on WORM wich we don't support
	 * (yet). Fill in defaults and set for internal allocation of data.
	 */
	icb->strat_type      = htole16(4);
	icb->max_num_entries = htole16(1);
	icb->file_type       = file_type;	/* 8 bit */
	icb->flags           = htole16(UDF_ICB_INTERN_ALLOC);

	fe->perm     = htole32(0x7fff);	/* all is allowed   */
	fe->link_cnt = htole16(0);		/* explicit setting */

	fe->ckpoint  = htole32(1);		/* user supplied file version */

	vfs_timestamp(&now);
	udf_timespec_to_timestamp(&now, &fe->atime);
	udf_timespec_to_timestamp(&now, &fe->attrtime);
	udf_timespec_to_timestamp(&now, &fe->mtime);

	udf_set_regid(&fe->imp_id, IMPL_NAME);
	udf_add_impl_regid(ump, &fe->imp_id);

	unique_id = udf_advance_uniqueid(ump);
	fe->unique_id = htole64(unique_id);
	fe->l_ea = htole32(0);

	/* create extended attribute to record our creation time */
	attrlen = UDF_FILETIMES_ATTR_SIZE(1);
	ft_extattr = malloc(attrlen, M_UDFTEMP, M_WAITOK);
	memset(ft_extattr, 0, attrlen);
	ft_extattr->hdr.type = htole32(UDF_FILETIMES_ATTR_NO);
	ft_extattr->hdr.subtype = 1;	/* [4/48.10.5] */
	ft_extattr->hdr.a_l = htole32(UDF_FILETIMES_ATTR_SIZE(1));
	ft_extattr->d_l     = htole32(UDF_TIMESTAMP_SIZE); /* one item */
	ft_extattr->existence = UDF_FILETIMES_FILE_CREATION;
	udf_timespec_to_timestamp(&now, &ft_extattr->times[0]);

	udf_extattr_insert_internal(ump, (union dscrptr *) fe,
		(struct extattr_entry *) ft_extattr);
	free(ft_extattr, M_UDFTEMP);

	/* if its a directory, create '..' */
	bpos = (uint8_t *) fe->data + le32toh(fe->l_ea);
	fidsize = 0;
	if (file_type == UDF_ICB_FILETYPE_DIRECTORY) {
		fidsize = udf_create_parentfid(ump,
			(struct fileid_desc *) bpos, parent_icb,
			parent_unique_id);
	}

	/* record fidlength information */
	fe->inf_len = htole64(fidsize);
	fe->l_ad    = htole32(fidsize);
	fe->logblks_rec = htole64(0);	/* intern */

	crclen  = sizeof(struct file_entry) - 1 - UDF_DESC_TAG_LENGTH;
	crclen += le32toh(fe->l_ea) + fidsize;
	fe->tag.desc_crc_len = htole16(crclen);

	(void) udf_validate_tag_and_crc_sums((union dscrptr *) fe);

	return (fidsize);
}

static int
udf_create_new_efe(struct udf_mount *ump, struct extfile_entry *efe,
	int file_type, struct long_ad *node_icb, struct long_ad *parent_icb,
	uint64_t parent_unique_id)
{
	struct timespec now;
	struct icb_tag *icb;
	uint64_t unique_id;
	uint32_t fidsize, lb_num;
	uint8_t *bpos;
	int crclen;

	lb_num = le32toh(node_icb->loc.lb_num);
	udf_inittag(ump, &efe->tag, TAGID_EXTFENTRY, lb_num);
	icb = &efe->icbtag;

	/*
	 * Always use strategy type 4 unless on WORM wich we don't support
	 * (yet). Fill in defaults and set for internal allocation of data.
	 */
	icb->strat_type      = htole16(4);
	icb->max_num_entries = htole16(1);
	icb->file_type       = file_type;	/* 8 bit */
	icb->flags           = htole16(UDF_ICB_INTERN_ALLOC);

	efe->perm     = htole32(0x7fff);	/* all is allowed   */
	efe->link_cnt = htole16(0);		/* explicit setting */

	efe->ckpoint  = htole32(1);		/* user supplied file version */

	vfs_timestamp(&now);
	udf_timespec_to_timestamp(&now, &efe->ctime);
	udf_timespec_to_timestamp(&now, &efe->atime);
	udf_timespec_to_timestamp(&now, &efe->attrtime);
	udf_timespec_to_timestamp(&now, &efe->mtime);

	udf_set_regid(&efe->imp_id, IMPL_NAME);
	udf_add_impl_regid(ump, &efe->imp_id);

	unique_id = udf_advance_uniqueid(ump);
	efe->unique_id = htole64(unique_id);
	efe->l_ea = htole32(0);

	/* if its a directory, create '..' */
	bpos = (uint8_t *) efe->data + le32toh(efe->l_ea);
	fidsize = 0;
	if (file_type == UDF_ICB_FILETYPE_DIRECTORY) {
		fidsize = udf_create_parentfid(ump,
			(struct fileid_desc *) bpos, parent_icb,
			parent_unique_id);
	}

	/* record fidlength information */
	efe->obj_size = htole64(fidsize);
	efe->inf_len  = htole64(fidsize);
	efe->l_ad     = htole32(fidsize);
	efe->logblks_rec = htole64(0);		/* intern */

	crclen  = sizeof(struct extfile_entry) - 1 - UDF_DESC_TAG_LENGTH;
	crclen += le32toh(efe->l_ea) + fidsize;
	efe->tag.desc_crc_len = htole16(crclen);

	(void) udf_validate_tag_and_crc_sums((union dscrptr *) efe);

	return (fidsize);
}

int
udf_dir_detach(struct udf_mount *ump, struct udf_node *dir_node,
	struct udf_node *udf_node, struct componentname *cnp)
{
	struct vnode *dvp = dir_node->vnode;
	struct dirhash       *dirh;
	struct dirhash_entry *dirh_ep;
	struct file_entry    *fe  = dir_node->fe;
	struct extfile_entry *efe = dir_node->efe;
	struct fileid_desc *fid;
	struct dirent *dirent;
	uint64_t file_size, diroffset;
	uint32_t lb_size, fidsize;
	int found, error;
	char const *name  = cnp->cn_nameptr;
	int namelen = cnp->cn_namelen;
	int hit, refcnt;

	/* get our dirhash and make sure its read in */
	dirhash_get(&dir_node->dir_hash);
	error = dirhash_fill(dir_node);
	if (error != 0) {
		dirhash_put(dir_node->dir_hash);
		return (error);
	}
	dirh = dir_node->dir_hash;

	/* get directory filesize */
	if (fe != NULL) {
		file_size = le64toh(fe->inf_len);
	} else {
		assert(efe != NULL);
		file_size = le64toh(efe->inf_len);
	}

	/* allocate temporary space for fid */
	lb_size = le32toh(dir_node->ump->logical_vol->lb_size);
	fid     = malloc(lb_size, M_UDFTEMP, M_WAITOK);
	dirent  = malloc(sizeof(struct dirent), M_UDFTEMP, M_WAITOK);

	/* search our dirhash hits */
	found = 0;
	dirh_ep = NULL;
	for (;;) {
		hit = dirhash_lookup(dirh, name, namelen, &dirh_ep);
		/* if no hit, abort the search */
		if (!hit)
			break;

		/* check this hit */
		diroffset = dirh_ep->offset;

		/* transfer a new fid/dirent */
		error = udf_read_fid_stream(dvp, &diroffset, fid, dirent);
		if (error != 0)
			break;

		/* see if its our entry */
		KASSERT(dirent->d_namlen == namelen);
		if (strncmp(dirent->d_name, name, namelen) == 0) {
			found = 1;
			break;
		}
	}

	if (!found)
		error = ENOENT;
	if (error != 0)
		goto error_out;

	/* mark deleted */
	fid->file_char |= UDF_FILE_CHAR_DEL;
#ifdef UDF_COMPLETE_DELETE
	memset(&fid->icb, 0, sizeof(fid->icb));
#endif
	(void) udf_validate_tag_and_crc_sums((union dscrptr *) fid);

	/* get size of fid and compensate for the read_fid_stream advance */
	fidsize = udf_fidsize(fid);
	diroffset -= fidsize;

	/* write out */
	error = vn_rdwr(UIO_WRITE, dir_node->vnode,
			fid, fidsize, diroffset, 
			UIO_SYSSPACE, IO_ALTSEMANTICS | IO_NODELOCKED,
			FSCRED, NULL, NULL);
	if (error != 0)
		goto error_out;

	/* get reference count of attached node */
	if (udf_node->fe != NULL) {
		refcnt = le16toh(udf_node->fe->link_cnt);
	} else {
		KASSERT(udf_node->efe != NULL);
		refcnt = le16toh(udf_node->efe->link_cnt);
	}
#ifdef UDF_COMPLETE_DELETE
	/* substract reference counter in attached node */
	refcnt -= 1;
	if (udf_node->fe != NULL) {
		udf_node->fe->link_cnt = htole16(refcnt);
	} else {
		udf_node->efe->link_cnt = htole16(refcnt);
	}

	/* prevent writeout when refcnt == 0 */
	if (refcnt == 0)
		udf_node->i_flags |= IN_DELETED;

	if (fid->file_char & UDF_FILE_CHAR_DIR) {
		int drefcnt;

		/* substract reference counter in directory node */
		/* note subtract 2 (?) for its was also backreferenced */
		if (dir_node->fe != NULL) {
			drefcnt  = le16toh(dir_node->fe->link_cnt);
			drefcnt -= 1;
			dir_node->fe->link_cnt = htole16(drefcnt);
		} else {
			KASSERT(dir_node->efe != NULL);
			drefcnt  = le16toh(dir_node->efe->link_cnt);
			drefcnt -= 1;
			dir_node->efe->link_cnt = htole16(drefcnt);
		}
	}

	udf_node->i_flags |= IN_MODIFIED;
	dir_node->i_flags |= IN_MODIFIED;
#endif
	/* if it is/was a hardlink adjust the file count */
	if (refcnt > 0)
		udf_adjust_filecount(udf_node, -1);

	/* remove from the dirhash */
	dirhash_remove(dirh, dirent, diroffset,
		udf_fidsize(fid));

error_out:
	free(fid, M_UDFTEMP);
	free(dirent, M_UDFTEMP);

	dirhash_put(dir_node->dir_hash);

	return (error);
}

int
udf_dir_update_rootentry(struct udf_mount *ump, struct udf_node *dir_node,
	struct udf_node *new_parent_node)
{
	struct vnode *dvp = dir_node->vnode;
	struct dirhash       *dirh;
	struct dirhash_entry *dirh_ep;
	struct file_entry    *fe;
	struct extfile_entry *efe;
	struct fileid_desc *fid;
	struct dirent *dirent;
	uint64_t file_size, diroffset;
	uint64_t new_parent_unique_id;
	uint32_t lb_size, fidsize;
	int found, error;
	char const *name  = "..";
	int namelen = 2;
	int hit;

	/* get our dirhash and make sure its read in */
	dirhash_get(&dir_node->dir_hash);
	error = dirhash_fill(dir_node);
	if (error != 0) {
		dirhash_put(dir_node->dir_hash);
		return (error);
	}
	dirh = dir_node->dir_hash;

	/* get new parent's unique ID */
	fe  = new_parent_node->fe;
	efe = new_parent_node->efe;
	if (fe != NULL) {
		new_parent_unique_id = le64toh(fe->unique_id);
	} else {
		assert(efe != NULL);
		new_parent_unique_id = le64toh(efe->unique_id);
	}

	/* get directory filesize */
	fe  = dir_node->fe;
	efe = dir_node->efe;
	if (fe != NULL) {
		file_size = le64toh(fe->inf_len);
	} else {
		assert(efe != NULL);
		file_size = le64toh(efe->inf_len);
	}

	/* allocate temporary space for fid */
	lb_size = le32toh(dir_node->ump->logical_vol->lb_size);
	fid     = malloc(lb_size, M_UDFTEMP, M_WAITOK);
	dirent  = malloc(sizeof(struct dirent), M_UDFTEMP, M_WAITOK);

	/*
	 * NOTE the standard does not dictate the FID entry '..' should be
	 * first, though in practice it will most likely be.
	 */

	/* search our dirhash hits */
	found = 0;
	dirh_ep = NULL;
	for (;;) {
		hit = dirhash_lookup(dirh, name, namelen, &dirh_ep);
		/* if no hit, abort the search */
		if (!hit)
			break;

		/* check this hit */
		diroffset = dirh_ep->offset;

		/* transfer a new fid/dirent */
		error = udf_read_fid_stream(dvp, &diroffset, fid, dirent);
		if (error != 0)
			break;

		/* see if its our entry */
		KASSERT(dirent->d_namlen == namelen);
		if (strncmp(dirent->d_name, name, namelen) == 0) {
			found = 1;
			break;
		}
	}

	if (!found)
		error = ENOENT;
	if (error != 0)
		goto error_out;

	/* update our ICB to the new parent, hit of lower 32 bits of uniqueid */
	fid->icb = new_parent_node->write_loc;
	fid->icb.longad_uniqueid = htole32(new_parent_unique_id);

	(void) udf_validate_tag_and_crc_sums((union dscrptr *) fid);

	/* get size of fid and compensate for the read_fid_stream advance */
	fidsize = udf_fidsize(fid);
	diroffset -= fidsize;

	/* write out */
	error = vn_rdwr(UIO_WRITE, dir_node->vnode,
			fid, fidsize, diroffset, 
			UIO_SYSSPACE, IO_ALTSEMANTICS | IO_NODELOCKED,
			FSCRED, NULL, NULL);

	/* nothing to be done in the dirhash */

error_out:
	free(fid, M_UDFTEMP);
	free(dirent, M_UDFTEMP);

	dirhash_put(dir_node->dir_hash);

	return (error);
}

/*
 * We are not allowed to split the fid tag itself over an logical block so
 * check the space remaining in the logical block.
 *
 * We try to select the smallest candidate for recycling or when none is
 * found, append a new one at the end of the directory.
 */

int
udf_dir_attach(struct udf_mount *ump, struct udf_node *dir_node,
	struct udf_node *udf_node, struct vattr *vap, struct componentname *cnp)
{
	struct vnode *dvp = dir_node->vnode;
	struct dirhash       *dirh;
	struct dirhash_entry *dirh_ep;
	struct fileid_desc   *fid;
	struct icb_tag       *icbtag;
	struct charspec osta_charspec;
	struct dirent   dirent;
	uint64_t unique_id, dir_size;
	uint64_t fid_pos, end_fid_pos, chosen_fid_pos;
	uint32_t chosen_size, chosen_size_diff;
	int lb_size, lb_rest, fidsize, this_fidsize, size_diff;
	int file_char, refcnt, icbflags, addr_type, hit, error;

	/* get our dirhash and make sure its read in */
	dirhash_get(&dir_node->dir_hash);
	error = dirhash_fill(dir_node);
	if (error != 0) {
		dirhash_put(dir_node->dir_hash);
		return (error);
	}
	dirh = dir_node->dir_hash;

	/* get info */
	lb_size = le32toh(ump->logical_vol->lb_size);
	udf_osta_charset(&osta_charspec);

	if (dir_node->fe != NULL) {
		dir_size = le64toh(dir_node->fe->inf_len);
		icbtag   = &dir_node->fe->icbtag;
	} else {
		dir_size = le64toh(dir_node->efe->inf_len);
		icbtag   = &dir_node->efe->icbtag;
	}

	icbflags   = le16toh(icbtag->flags);
	addr_type  = icbflags & UDF_ICB_TAG_FLAGS_ALLOC_MASK;

	if (udf_node->fe != NULL) {
		unique_id = le64toh(udf_node->fe->unique_id);
		refcnt    = le16toh(udf_node->fe->link_cnt);
	} else {
		unique_id = le64toh(udf_node->efe->unique_id);
		refcnt    = le16toh(udf_node->efe->link_cnt);
	}

	if (refcnt > 0) {
		unique_id = udf_advance_uniqueid(ump);
		udf_adjust_filecount(udf_node, 1);
	}

	/* determine file characteristics */
	file_char = 0;	/* visible non deleted file and not stream metadata */
	if (vap->va_type == VDIR)
		file_char = UDF_FILE_CHAR_DIR;

	/* malloc scrap buffer */
	fid = malloc(lb_size, M_UDFTEMP, M_WAITOK|M_ZERO);

	/* calculate _minimum_ fid size */
	unix_to_udf_name((char *) fid->data, &fid->l_fi,
		cnp->cn_nameptr, cnp->cn_namelen, &osta_charspec);
	fidsize = UDF_FID_SIZE + fid->l_fi;
	fidsize = (fidsize + 3) & ~3;		/* multiple of 4 */

	/* find position that will fit the FID */
	chosen_fid_pos   = dir_size;
	chosen_size      = 0;
	chosen_size_diff = UINT_MAX;

	/* shut up gcc */
	dirent.d_namlen = 0;

	/* search our dirhash hits */
	error = 0;
	dirh_ep = NULL;
	for (;;) {
		hit = dirhash_lookup_freed(dirh, fidsize, &dirh_ep);
		/* if no hit, abort the search */
		if (!hit)
			break;

		/* check this hit for size */
		this_fidsize = dirh_ep->entry_size;

		/* check this hit */
		fid_pos     = dirh_ep->offset;
		end_fid_pos = fid_pos + this_fidsize;
		size_diff   = this_fidsize - fidsize;
		lb_rest = lb_size - (end_fid_pos % lb_size);

#ifndef UDF_COMPLETE_DELETE
		/* transfer a new fid/dirent */
		error = udf_read_fid_stream(vp, &fid_pos, fid, dirent);
		if (error != 0)
			goto error_out;

		/* only reuse entries that are wiped */
		/* check if the len + loc are marked zero */
		if (le32toh(fid->icb.len) != 0)
			continue;
		if (le32toh(fid->icb.loc.lb_num) != 0)
			continue;
		if (le16toh(fid->icb.loc.part_num) != 0)
			continue;
#endif	/* UDF_COMPLETE_DELETE */

		/* select if not splitting the tag and its smaller */
		if ((size_diff >= 0)  &&
			(size_diff < chosen_size_diff) &&
			(lb_rest >= sizeof(struct desc_tag)))
		{
			/* UDF 2.3.4.2+3 specifies rules for iu size */
			if ((size_diff == 0) || (size_diff >= 32)) {
				chosen_fid_pos   = fid_pos;
				chosen_size      = this_fidsize;
				chosen_size_diff = size_diff;
			}
		}
	}


	/* extend directory if no other candidate found */
	if (chosen_size == 0) {
		chosen_fid_pos   = dir_size;
		chosen_size      = fidsize;
		chosen_size_diff = 0;

		/* special case UDF 2.00+ 2.3.4.4, no splitting up fid tag */
		if (addr_type == UDF_ICB_INTERN_ALLOC) {
			/* pre-grow directory to see if we're to switch */
			udf_grow_node(dir_node, dir_size + chosen_size);

			icbflags   = le16toh(icbtag->flags);
			addr_type  = icbflags & UDF_ICB_TAG_FLAGS_ALLOC_MASK;
		}

		/* make sure the next fid desc_tag won't be splitted */
		if (addr_type != UDF_ICB_INTERN_ALLOC) {
			end_fid_pos = chosen_fid_pos + chosen_size;
			lb_rest = lb_size - (end_fid_pos % lb_size);

			/* pad with implementation use regid if needed */
			if (lb_rest < sizeof(struct desc_tag))
				chosen_size += 32;
		}
	}
	chosen_size_diff = chosen_size - fidsize;

	/* populate the FID */
	memset(fid, 0, lb_size);
	udf_inittag(ump, &fid->tag, TAGID_FID, 0);
	fid->file_version_num    = htole16(1);	/* UDF 2.3.4.1 */
	fid->file_char           = file_char;
	fid->icb                 = udf_node->loc;
	fid->icb.longad_uniqueid = htole32((uint32_t) unique_id);
	fid->l_iu                = htole16(0);

	if (chosen_size > fidsize) {
		/* insert implementation-use regid to space it correctly */
		fid->l_iu = htole16(chosen_size_diff);

		/* set implementation use */
		udf_set_regid((struct regid *) fid->data, IMPL_NAME);
		udf_add_impl_regid(ump, (struct regid *) fid->data);
	}

	/* fill in name */
	unix_to_udf_name((char *) fid->data + le16toh(fid->l_iu),
		&fid->l_fi, cnp->cn_nameptr, cnp->cn_namelen, &osta_charspec);

	fid->tag.desc_crc_len = htole16(chosen_size - UDF_DESC_TAG_LENGTH);
	(void) udf_validate_tag_and_crc_sums((union dscrptr *) fid);

	/* writeout FID/update parent directory */
	error = vn_rdwr(UIO_WRITE, dvp,
			fid, chosen_size, chosen_fid_pos, 
			UIO_SYSSPACE, IO_ALTSEMANTICS | IO_NODELOCKED,
			FSCRED, NULL, NULL);

	if (error != 0)
		goto error_out;

	/* add reference counter in attached node */
	if (udf_node->fe != NULL) {
		refcnt = le16toh(udf_node->fe->link_cnt);
		udf_node->fe->link_cnt = htole16(refcnt+1);
	} else {
		KASSERT(udf_node->efe != NULL);
		refcnt = le16toh(udf_node->efe->link_cnt);
		udf_node->efe->link_cnt = htole16(refcnt+1);
	}

	/* mark not deleted if it was... just in case, but do warn */
	if (udf_node->i_flags & IN_DELETED) {
		printf("udf: warning, marking a file undeleted\n");
		udf_node->i_flags &= ~IN_DELETED;
	}

	if (file_char & UDF_FILE_CHAR_DIR) {
		/* add reference counter in directory node for '..' */
		if (dir_node->fe != NULL) {
			refcnt = le16toh(dir_node->fe->link_cnt);
			refcnt++;
			dir_node->fe->link_cnt = htole16(refcnt);
		} else {
			KASSERT(dir_node->efe != NULL);
			refcnt = le16toh(dir_node->efe->link_cnt);
			refcnt++;
			dir_node->efe->link_cnt = htole16(refcnt);
		}
	}

	/* append to the dirhash */
	dirent.d_namlen = cnp->cn_namelen;
	memcpy(dirent.d_name, cnp->cn_nameptr, cnp->cn_namelen);
	dirhash_enter(dirh, &dirent, chosen_fid_pos,
		udf_fidsize(fid), 1);

	/* note updates */
	udf_node->i_flags |= IN_CHANGE | IN_MODIFY; /* | IN_CREATE? */
	/* VN_KNOTE(udf_node,  ...) */
	udf_update(udf_node->vnode, NULL, NULL, NULL, 0);

error_out:
	free(fid, M_UDFTEMP);

	dirhash_put(dir_node->dir_hash);

	return (error);
}
#endif

#if 0
int
udf_vget(struct mount *mp, ino_t ino, int flags, struct vnode **vpp)
{
	printf("%s ran\n", __func__);

	int error;
	struct udf_node *unode;
	struct udf_mount *ump;
	struct vnode *nvp;
	struct long_ad node_icb_loc;
	int udf_file_type;

	unode = uma_zalloc(udf_zone_node, M_WAITOK | M_ZERO);
	unode = udf_alloc_node()
	
	unode->hash_id = ino;

	lockmgr(nvp->v_vnlock, LK_EXCLUSIVE, NULL);
	if ((error = insmntque(nvp, mp)) != 0) {
		uma_zfree(udf_zone_node, unode);
		return (error);
	} 

	/* TODO: Does this leak unode or vnodes? */
	error = vfs_hash_insert(nvp, ino, flags, curthread, vpp, NULL, NULL);
	if (error || *vpp != NULL)
		return (error);

	/* 
	 * Load read and set up the unode structure.
	 */
 	udf_get_node_longad(ino, &node_icb_loc);
	error = udf_get_node(ump, &node_icb_loc, unode);
	if (error != 0) {
		vgone(nvp);
		vput(nvp);
		ungetnewvnode(nvp);
	}
	return
}
#endif

/*
 * Each node can have an attached streamdir node though not recursively. These
 * are otherwise known as named substreams/named extended attributes that have
 * no size limitations.
 *
 * `Normal' extended attributes are indicated with a number and are recorded
 * in either the fe/efe descriptor itself for small descriptors or recorded in
 * the attached extended attribute file. Since these spaces can get
 * fragmented, care ought to be taken.
 *
 * Since the size of the space reserved for allocation descriptors is limited,
 * there is a mechanim provided for extending this space; this is done by a
 * special extent to allow schrinking of the allocations without breaking the
 * linkage to the allocation extent descriptor.
 */

int
udf_get_node(struct udf_mount *ump, struct long_ad icb_loc,
    struct udf_node **ppunode)
{
	union dscrptr *dscr;
	struct long_ad last_fe_icb_loc;
	struct udf_node *udf_node;
	uint64_t file_size;
	int dscr_type, eof, error, needs_indirect, slot, strat, strat4096;
	uint32_t dummy, lb_size, sector;
	uint8_t  *file_data;

	/* garbage check: translate udf_node_icb_loc to sectornr */
	error = udf_translate_vtop(ump, &icb_loc, &sector, &dummy);
	if (error != 0)
		return (EINVAL);

	/* initialise crosslinks, note location of fe/efe for hashing */
	udf_node = udf_alloc_node();
	udf_node->ump = ump;
	udf_node->loc = icb_loc;
	udf_node->lockf = 0;
/*	mutex_init(&udf_node->node_mutex, MUTEX_DEFAULT, IPL_NONE); */
/*	cv_init(&udf_node->node_lock, "udf_nlk"); */
	udf_node->outstanding_bufs = 0;
	udf_node->outstanding_nodedscr = 0;
	udf_node->uncommitted_lbs = 0;

	/* safe to unlock, the entry is in the hash table, vnode is locked */
/*	mutex_exit(&ump->get_node_lock); */

	needs_indirect = 0;
	strat4096 = 0;
	file_size = 0;
	file_data = NULL;
	lb_size = le32toh(ump->logical_vol->lb_size);

	do {
		/* try to read in fe/efe */
		/* error = udf_read_logvol_dscr(ump, &icb_loc, &dscr); */
		error = udf_translate_vtop(ump, &icb_loc, &sector, &dummy);
		if (error == 0)
			error = udf_read_phys_dscr(ump, sector, M_UDFTEMP, 
			    &dscr);

		/* blank sector marks end of sequence, check this */
		if (dscr == NULL && strat4096 == 0)
			error = ENOENT;

		/* break if read error or blank sector */
		if (error != 0 || dscr == NULL)
			break;

		/* process descriptor based on the descriptor type */
		dscr_type = le16toh(dscr->tag.id);

		/* if dealing with an indirect entry, follow the link */
		if (dscr_type == TAGID_INDIRECTENTRY) {
			needs_indirect = 0;
			/* udf_free_logvol_dscr(ump, &icb_loc, dscr); */
			free(dscr, M_UDFTEMP);
			/* end of udf_free_logvol_decr replacement */
			icb_loc = dscr->inde.indirect_icb;
			continue;
		}

		/* only file entries and extended file entries allowed here */
		if ((dscr_type != TAGID_FENTRY) &&
		    (dscr_type != TAGID_EXTFENTRY)) {
			/* udf_free_logvol_dscr(ump, &icb_loc, dscr); */
			free(dscr, M_UDFTEMP);
			/* end of udf_free_logvol_decr replacement */
			error = ENOENT;
			break;
		}

		/* KASSERT(udf_tagsize(dscr, lb_size) == lb_size); */

		/* choose this one */
		last_fe_icb_loc = icb_loc;
		
		/* record and process/update (ext)fentry */
		file_data = NULL;
		if (dscr_type == TAGID_FENTRY) {
			/*if (udf_node->fe != NULL)
				udf_free_logvol_dscr(ump, &last_fe_icb_loc,
					udf_node->fe); */
			if (udf_node->fe != NULL)
				free(udf_node->fe, M_UDFTEMP);
			/* end of udf_free_logvol_decr replacement */
			udf_node->fe = &dscr->fe;
			strat = le16toh(udf_node->fe->icbtag.strat_type);
			file_size = le64toh(udf_node->fe->inf_len);
			file_data = udf_node->fe->data;
		} else {
			/*if (udf_node->efe != NULL)
				udf_free_logvol_dscr(ump, &last_fe_icb_loc,
					udf_node->efe); */
			if (udf_node->efe != NULL)
				free(udf_node->efe, M_UDFTEMP);
			/* end of udf_free_logvol_decr replacement */
			udf_node->efe = &dscr->efe;
			strat = le16toh(udf_node->efe->icbtag.strat_type);
			file_size = le64toh(udf_node->efe->inf_len);
			file_data = udf_node->efe->data;
		}

		/* check recording strategy (structure) */

		/*
		 * Strategy 4096 is a daisy linked chain terminating with an
		 * unrecorded sector or a TERM descriptor. The next
		 * descriptor is to be found in the sector that follows the
		 * current sector.
		 */
		if (strat == 4096) {
			strat4096 = 1;
			needs_indirect = 1;

			icb_loc.loc.lb_num = le32toh(icb_loc.loc.lb_num) + 1;
		}

		/*
		 * Strategy 4 is the normal strategy and terminates, but if
		 * we're in strategy 4096, we can't have strategy 4 mixed in
		 */

		if (strat == 4) {
			if (strat4096 != 0) {
				error = EINVAL;
				break;
			}
			break;		/* done */
		}
	} while (!error);

	/* first round of cleanup code */
	if (error != 0) {
		udf_dispose_node(udf_node);
		return (EINVAL);
	}

	/* assert no references to dscr anymore beyong this point */
	/* assert((udf_node->fe != NULL) || (udf_node->efe != NULL)); */
	dscr = NULL;

	/*
	 * Remember where to record an updated version of the descriptor. If
	 * there is a sequence of indirect entries, icb_loc will have been
	 * updated. Its the write disipline to allocate new space and to make
	 * sure the chain is maintained.
	 *
	 * `needs_indirect' flags if the next location is to be filled with
	 * with an indirect entry.
	 */
	udf_node->write_loc = icb_loc;
	udf_node->needs_indirect = needs_indirect;

	/*
	 * Go trough all allocations extents of this descriptor and when
	 * encountering a redirect read in the allocation extension. These are
	 * daisy-chained.
	 */
	UDF_LOCK_NODE(udf_node, 0);
	udf_node->num_extensions = 0;

	error = 0;
	slot = 0;
	for (;;) {
		udf_get_adslot(udf_node, slot, &icb_loc, &eof);
		if (eof != 0)
			break;
		slot++;

		if (UDF_EXT_FLAGS(le32toh(icb_loc.len)) != UDF_EXT_REDIRECT)
			continue;

		if (udf_node->num_extensions >= UDF_MAX_ALLOC_EXTENTS) {
			error = EINVAL;
			break;
		}

		/* length can only be *one* lb : UDF 2.50/2.3.7.1 */
		if (UDF_EXT_LEN(le32toh(icb_loc.len)) != lb_size) {
			error = EINVAL;
			break;
		}

		/* load in allocation extent */
		/* error = udf_read_logvol_dscr(ump, &icb_loc, &dscr); */
		error = udf_translate_vtop(ump, &icb_loc, &sector, &dummy);
		if (error == 0)
			error = udf_read_phys_dscr(ump, sector, M_UDFTEMP, 
			    &dscr);
		if (error != 0 || dscr == NULL)
			break;

		/* process read-in descriptor */
		dscr_type = le16toh(dscr->tag.id);

		if (dscr_type != TAGID_ALLOCEXTENT) {
			/* udf_free_logvol_dscr(ump, &icb_loc, dscr); */
			free(dscr, M_UDFTEMP);
			/* end of udf_free_logvol_decr replacement */
			error = ENOENT;
			break;
		}

		udf_node->ext[udf_node->num_extensions] = &dscr->aee;
		udf_node->ext_loc[udf_node->num_extensions] = icb_loc;

		udf_node->num_extensions++;

	} /* while */
	UDF_UNLOCK_NODE(udf_node, 0);

	/* second round of cleanup code */
	if (error != 0) {
		/* recycle udf_node */
		udf_dispose_node(udf_node);
		return (EINVAL);		/* error code ok? */
	}

	/* TODO ext attr and streamdir udf_nodes */

	*ppunode = udf_node;

	return (0);
}

#if 0
int
udf_writeout_node(struct udf_node *udf_node, int waitfor)
{
	union dscrptr *dscr;
	struct long_ad *loc;
	int extnr, error;

	DPRINTF(NODE, ("udf_writeout_node called\n"));

	KASSERT(udf_node->outstanding_bufs == 0);
	KASSERT(udf_node->outstanding_nodedscr == 0);

	KASSERT(LIST_EMPTY(&udf_node->vnode->v_dirtyblkhd));

	if (udf_node->i_flags & IN_DELETED) {
		DPRINTF(NODE, ("\tnode deleted; not writing out\n"));
		udf_cleanup_reservation(udf_node);
		return (0);
	}

	/* lock node; unlocked in callback */
	UDF_LOCK_NODE(udf_node, 0);

	/* remove pending reservations, we're written out */
	udf_cleanup_reservation(udf_node);

	/* at least one descriptor writeout */
	udf_node->outstanding_nodedscr = 1;

	/* we're going to write out the descriptor so clear the flags */
	udf_node->i_flags &= ~(IN_MODIFIED | IN_ACCESSED);

	/* if we were rebuild, write out the allocation extents */
	if (udf_node->i_flags & IN_NODE_REBUILD) {
		/* mark outstanding node descriptors and issue them */
		udf_node->outstanding_nodedscr += udf_node->num_extensions;
		for (extnr = 0; extnr < udf_node->num_extensions; extnr++) {
			loc = &udf_node->ext_loc[extnr];
			dscr = (union dscrptr *) udf_node->ext[extnr];
			error = udf_write_logvol_dscr(udf_node, dscr, loc, 0);
			if (error != 0)
				return (error);
		}
		/* mark allocation extents written out */
		udf_node->i_flags &= ~(IN_NODE_REBUILD);
	}

	if (udf_node->fe != NULL) {
		KASSERT(udf_node->efe == NULL);
		dscr = (union dscrptr *) udf_node->fe;
	} else {
		KASSERT(udf_node->efe != NULL);
		KASSERT(udf_node->fe == NULL);
		dscr = (union dscrptr *) udf_node->efe;
	}
	KASSERT(dscr);

	loc = &udf_node->write_loc;
	error = udf_write_logvol_dscr(udf_node, dscr, loc, waitfor);

	return (error);
}
#endif

int
udf_dispose_node(struct udf_node *udf_node)
{
	int extnr;

	if (udf_node == NULL)
		return (0);

/*	udf_cleanup_reservation(udf_node); */

	/* TODO extended attributes and streamdir */

	/* free associated memory and the node itself */
	for (extnr = 0; extnr < udf_node->num_extensions; extnr++) {
		/*udf_free_logvol_dscr(udf_node->ump, &udf_node->ext_loc[extnr],
			udf_node->ext[extnr]); */
		free(udf_node->ext[extnr], M_UDFTEMP);
		/*end of udf_free_logvol_decr replacement */
		udf_node->ext[extnr] = (void *)0xdeadcccc;
	}

	/*if (udf_node->fe != NULL)
		udf_free_logvol_dscr(udf_node->ump, &udf_node->loc,
			udf_node->fe); */
	if (udf_node->fe != NULL)
		free(udf_node->fe, M_UDFTEMP);
	/*end of udf_free_logvol_decr replacement */

	/*if (udf_node->efe != NULL)
		udf_free_logvol_dscr(udf_node->ump, &udf_node->loc,
			udf_node->efe); */
	if (udf_node->efe != NULL)
		free(udf_node->efe, M_UDFTEMP);
	/*end of udf_free_logvol_decr replacement */

	udf_node->fe = (void *)0xdeadaaaa;
	udf_node->efe = (void *)0xdeadbbbb;
	udf_node->ump = (void *)0xdeadbeef;
	udf_free_node(udf_node);

	return (0);
}

/*
 * create a new node using the specified vnodeops, vap and cnp but with the
 * udf_file_type. This allows special files to be created. Use with care.
 */

#if 0
static int
udf_create_node_raw(struct vnode *dvp, struct vnode **vpp, int udf_file_type,
	int (**vnodeops)(void *), struct vattr *vap, struct componentname *cnp)
{
	union dscrptr *dscr;
	struct udf_node *dir_node = VTOI(dvp);
	struct udf_node *udf_node;
	struct udf_mount *ump = dir_node->ump;
	struct vnode *nvp;
	struct long_ad node_icb_loc;
	uint64_t parent_unique_id;
	uint64_t lmapping;
	uint32_t lb_size, lb_num;
	uint16_t vpart_num;
	uid_t uid;
	gid_t gid, parent_gid;
	int fid_size, error;

	lb_size = le32toh(ump->logical_vol->lb_size);
	*vpp = NULL;

	/* allocate vnode */
	error = getnewvnode(VT_UDF, ump->vfs_mountp, vnodeops, &nvp);
        if (error != 0)
		return (error);

	/* lock node */
	error = vn_lock(nvp, LK_EXCLUSIVE | LK_RETRY);
	if (error != 0)
		goto error_out_unget;

	/* reserve space for one logical block */
	vpart_num = ump->node_part;
	error = udf_reserve_space(ump, NULL, UDF_C_NODE,
		vpart_num, 1, /* can_fail */ true);
	if (error != 0)
		goto error_out_unlock;

	/* allocate node */
	error = udf_allocate_space(ump, NULL, UDF_C_NODE,
			vpart_num, 1, &lmapping);
	if (error != 0)
		goto error_out_unreserve;
	lb_num = lmapping;

	/* initialise pointer to location */
	memset(&node_icb_loc, 0, sizeof(struct long_ad));
	node_icb_loc.len = htole32(lb_size);
	node_icb_loc.loc.lb_num   = htole32(lb_num);
	node_icb_loc.loc.part_num = htole16(vpart_num);

	/* build udf_node (do initialise!) */
	udf_node = pool_get(&udf_node_pool, PR_WAITOK);
	memset(udf_node, 0, sizeof(struct udf_node));

	/* initialise crosslinks, note location of fe/efe for hashing */
	/* bugalert: synchronise with udf_get_node() */
	udf_node->ump       = ump;
	udf_node->vnode     = nvp;
	nvp->v_data         = udf_node;
	udf_node->loc       = node_icb_loc;
	udf_node->write_loc = node_icb_loc;
	udf_node->lockf     = 0;
	mutex_init(&udf_node->node_mutex, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&udf_node->node_lock, "udf_nlk");
	udf_node->outstanding_bufs = 0;
	udf_node->outstanding_nodedscr = 0;
	udf_node->uncommitted_lbs = 0;

	/* initialise genfs */
	genfs_node_init(nvp, &udf_genfsops);

	/* insert into the hash lookup */
	udf_register_node(udf_node);

	/* get parent's unique ID for refering '..' if its a directory */
	if (dir_node->fe != NULL) {
		parent_unique_id = le64toh(dir_node->fe->unique_id);
		parent_gid       = (gid_t) le32toh(dir_node->fe->gid);
	} else {
		parent_unique_id = le64toh(dir_node->efe->unique_id);
		parent_gid       = (gid_t) le32toh(dir_node->efe->gid);
	}

	/* get descriptor */
	udf_create_logvol_dscr(ump, udf_node, &node_icb_loc, &dscr);

	/* choose a fe or an efe for it */
	if (le16toh(ump->logical_vol->tag.descriptor_ver) == 2) {
		udf_node->fe = &dscr->fe;
		fid_size = udf_create_new_fe(ump, udf_node->fe,
			udf_file_type, &udf_node->loc,
			&dir_node->loc, parent_unique_id);
		/* TODO add extended attribute for creation time */
	} else {
		udf_node->efe = &dscr->efe;
		fid_size = udf_create_new_efe(ump, udf_node->efe,
			udf_file_type, &udf_node->loc,
			&dir_node->loc, parent_unique_id);
	}
	KASSERT(dscr->tag.tag_loc == udf_node->loc.loc.lb_num);

	/* update vnode's size and type */
	nvp->v_type = vap->va_type;
	uvm_vnp_setsize(nvp, fid_size);

	/* set access mode */
	udf_setaccessmode(udf_node, vap->va_mode);

	/* set ownership */
	uid = kauth_cred_geteuid(cnp->cn_cred);
	gid = parent_gid;
	udf_setownership(udf_node, uid, gid);

	error = udf_dir_attach(ump, dir_node, udf_node, vap, cnp);
	if (error != 0) {
		/* free disc allocation for node */
		udf_free_allocated_space(ump, lb_num, vpart_num, 1);

		/* recycle udf_node */
		udf_dispose_node(udf_node);
		vput(nvp);

		*vpp = NULL;
		return (error);
	}

	/* adjust file count */
	udf_adjust_filecount(udf_node, 1);

	/* return result */
	*vpp = nvp;

	return (0);

error_out_unreserve:
	udf_do_unreserve_space(ump, NULL, vpart_num, 1);

error_out_unlock:
	VOP_UNLOCK(nvp);

error_out_unget:
	nvp->v_data = NULL;
	ungetnewvnode(nvp);

	return (error);
}

int
udf_create_node(struct vnode *dvp, struct vnode **vpp, struct vattr *vap,
	struct componentname *cnp)
{
	int (**vnodeops)(void *);
	int udf_file_type;

	/* what type are we creating ? */
	vnodeops = udf_vnodeop_p;
	/* start with a default */
	udf_file_type = UDF_ICB_FILETYPE_RANDOMACCESS;

	*vpp = NULL;

	switch (vap->va_type) {
	case VREG:
		udf_file_type = UDF_ICB_FILETYPE_RANDOMACCESS;
		break;
	case VDIR:
		udf_file_type = UDF_ICB_FILETYPE_DIRECTORY;
		break;
	case VLNK:
		udf_file_type = UDF_ICB_FILETYPE_SYMLINK;
		break;
	case VBLK:
		udf_file_type = UDF_ICB_FILETYPE_BLOCKDEVICE;
		/* specfs */
		return (ENOTSUP);
		break;
	case VCHR:
		udf_file_type = UDF_ICB_FILETYPE_CHARDEVICE;
		/* specfs */
		return (ENOTSUP);
		break;
	case VFIFO:
		udf_file_type = UDF_ICB_FILETYPE_FIFO;
		/* specfs */
		return (ENOTSUP);
		break;
	case VSOCK:
		udf_file_type = UDF_ICB_FILETYPE_SOCKET;
		/* specfs */
		return (ENOTSUP);
		break;
	case VNON:
	case VBAD:
	default:
		/* nothing; can we even create these? */
		return (EINVAL);
	}

	return (udf_create_node_raw(dvp, vpp, udf_file_type, vnodeops, vap, cnp));
}

static void
udf_free_descriptor_space(struct udf_node *udf_node, struct long_ad *loc, void *mem)
{
	struct udf_mount *ump = udf_node->ump;
	uint32_t lb_size, lb_num, len, num_lb;
	uint16_t vpart_num;

	/* is there really one? */
	if (mem == NULL)
		return;

	/* got a descriptor here */
	len       = UDF_EXT_LEN(le32toh(loc->len));
	lb_num    = le32toh(loc->loc.lb_num);
	vpart_num = le16toh(loc->loc.part_num);

	lb_size = le32toh(ump->logical_vol->lb_size);
	num_lb = (len + lb_size -1) / lb_size;

	udf_free_allocated_space(ump, lb_num, vpart_num, num_lb);
}

void
udf_delete_node(struct udf_node *udf_node)
{
	void *dscr;
	struct udf_mount *ump;
	struct long_ad *loc;
	int extnr, lvint, dummy;

	ump = udf_node->ump;

	/* paranoia check on integrity; should be open!; we could panic */
	lvint = le32toh(udf_node->ump->logvol_integrity->integrity_type);
	if (lvint == UDF_INTEGRITY_CLOSED)
		printf("\tIntegrity was CLOSED!\n");

	/* whatever the node type, change its size to zero */
	(void) udf_resize_node(udf_node, 0, &dummy);

	/* force it to be `clean'; no use writing it out */
	udf_node->i_flags &= ~(IN_MODIFIED | IN_ACCESSED | IN_ACCESS |
		IN_CHANGE | IN_UPDATE | IN_MODIFY);

	/* adjust file count */
	udf_adjust_filecount(udf_node, -1);

	/*
	 * Free its allocated descriptors; memory will be released when
	 * vop_reclaim() is called.
	 */
	loc = &udf_node->loc;

	dscr = udf_node->fe;
	udf_free_descriptor_space(udf_node, loc, dscr);
	dscr = udf_node->efe;
	udf_free_descriptor_space(udf_node, loc, dscr);

	for (extnr = 0; extnr < UDF_MAX_ALLOC_EXTENTS; extnr++) {
		dscr =  udf_node->ext[extnr];
		loc  = &udf_node->ext_loc[extnr];
		udf_free_descriptor_space(udf_node, loc, dscr);
	}
}

/* set new filesize; node but be LOCKED on entry and is locked on exit */
int
udf_resize_node(struct udf_node *udf_node, uint64_t new_size, int *extended)
{
	struct file_entry    *fe  = udf_node->fe;
	struct extfile_entry *efe = udf_node->efe;
	uint64_t file_size;
	int error;

	if (fe != NULL) {
		file_size  = le64toh(fe->inf_len);
	} else {
		assert(udf_node->efe != NULL);
		file_size  = le64toh(efe->inf_len);
	}

	/* if not changing, we're done */
	if (file_size == new_size)
		return (0);

	*extended = (new_size > file_size);
	if (*extended) {
		error = udf_grow_node(udf_node, new_size);
	} else {
		error = udf_shrink_node(udf_node, new_size);
	}

	return (error);
}

void
udf_itimes(struct udf_node *udf_node, struct timespec *acc,
	struct timespec *mod, struct timespec *birth)
{
	struct timespec now;
	struct file_entry    *fe;
	struct extfile_entry *efe;
	struct filetimes_extattr_entry *ft_extattr;
	struct timestamp *atime, *mtime, *attrtime, *ctime;
	struct timestamp  fe_ctime;
	struct timespec   cur_birth;
	uint32_t offset, a_l;
	uint8_t *filedata;
	int error;

	/* protect against rogue values */
	if (!udf_node)
		return;

	fe  = udf_node->fe;
	efe = udf_node->efe;

	if (!(udf_node->i_flags & (IN_ACCESS|IN_CHANGE|IN_UPDATE|IN_MODIFY)))
		return;

	/* get descriptor information */
	if (fe != NULL) {
		atime    = &fe->atime;
		mtime    = &fe->mtime;
		attrtime = &fe->attrtime;
		filedata = fe->data;

		/* initial save dummy setting */
		ctime    = &fe_ctime;

		/* check our extended attribute if present */
		error = udf_extattr_search_intern(udf_node,
			UDF_FILETIMES_ATTR_NO, "", &offset, &a_l);
		if (error == 0) {
			ft_extattr = (struct filetimes_extattr_entry *)
				(filedata + offset);
			if (ft_extattr->existence & UDF_FILETIMES_FILE_CREATION)
				ctime = &ft_extattr->times[0];
		}
		/* TODO create the extended attribute if not found ? */
	} else {
		assert(udf_node->efe != NULL);
		atime    = &efe->atime;
		mtime    = &efe->mtime;
		attrtime = &efe->attrtime;
		ctime    = &efe->ctime;
	}

	vfs_timestamp(&now);

	/* set access time */
	if (udf_node->i_flags & IN_ACCESS) {
		if (acc == NULL)
			acc = &now;
		udf_timespec_to_timestamp(acc, atime);
	}

	/* set modification time */
	if (udf_node->i_flags & (IN_UPDATE | IN_MODIFY)) {
		if (mod == NULL)
			mod = &now;
		udf_timespec_to_timestamp(mod, mtime);

		/* ensure birthtime is older than set modification! */
		udf_timestamp_to_timespec(udf_node->ump, ctime, &cur_birth);
		if ((cur_birth.tv_sec > mod->tv_sec) ||
			  ((cur_birth.tv_sec == mod->tv_sec) &&
			     (cur_birth.tv_nsec > mod->tv_nsec))) {
			udf_timespec_to_timestamp(mod, ctime);
		}
	}

	/* update birthtime if specified */
	/* XXX we asume here that given birthtime is older than mod */
	if (birth && (birth->tv_sec != VNOVAL)) {
		udf_timespec_to_timestamp(birth, ctime);
	}

	/* set change time */
	if (udf_node->i_flags & (IN_CHANGE | IN_MODIFY))
		udf_timespec_to_timestamp(&now, attrtime);

	/* notify updates to the node itself */
	if (udf_node->i_flags & (IN_ACCESS | IN_MODIFY))
		udf_node->i_flags |= IN_ACCESSED;
	if (udf_node->i_flags & (IN_UPDATE | IN_CHANGE))
		udf_node->i_flags |= IN_MODIFIED;

	/* clear modification flags */
	udf_node->i_flags &= ~(IN_ACCESS | IN_CHANGE | IN_UPDATE | IN_MODIFY);
}

int
udf_update(struct vnode *vp, struct timespec *acc,
	struct timespec *mod, struct timespec *birth, int updflags)
{
	union dscrptr *dscrptr;
	struct udf_node  *udf_node = VTOI(vp);
	struct udf_mount *ump = udf_node->ump;
	struct regid     *impl_id;
	int mnt_async = (vp->v_mount->mnt_flag & MNT_ASYNC);
	int waitfor, flags;

#ifdef DEBUG
	char bits[128];
	DPRINTF(CALL, ("udf_update(node, %p, %p, %p, %d)\n", acc, mod, birth,
		updflags));
	snprintb(bits, sizeof(bits), IN_FLAGBITS, udf_node->i_flags);
	DPRINTF(CALL, ("\tnode flags %s\n", bits));
	DPRINTF(CALL, ("\t\tmnt_async = %d\n", mnt_async));
#endif

	/* set our times */
	udf_itimes(udf_node, acc, mod, birth);

	/* set our implementation id */
	if (udf_node->fe != NULL) {
		dscrptr = (union dscrptr *) udf_node->fe;
		impl_id = &udf_node->fe->imp_id;
	} else {
		dscrptr = (union dscrptr *) udf_node->efe;
		impl_id = &udf_node->efe->imp_id;
	}

	/* set our ID */
	udf_set_regid(impl_id, IMPL_NAME);
	udf_add_impl_regid(ump, impl_id);

	/* update our crc! on RMW we are not allowed to change a thing */
	udf_validate_tag_and_crc_sums(dscrptr);

	/* if called when mounted readonly, never write back */
	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		return (0);

	/* check if the node is dirty 'enough'*/
	if (updflags & UPDATE_CLOSE) {
		flags = udf_node->i_flags & (IN_MODIFIED | IN_ACCESSED);
	} else {
		flags = udf_node->i_flags & IN_MODIFIED;
	}
	if (flags == 0)
		return (0);

	/* determine if we need to write sync or async */
	waitfor = 0;
	if ((flags & IN_MODIFIED) && (mnt_async == 0)) {
		/* sync mounted */
		waitfor = updflags & UPDATE_WAIT;
		if (updflags & UPDATE_DIROP)
			waitfor |= UPDATE_WAIT;
	}
	if (waitfor)
		return (VOP_FSYNC(vp, FSCRED, FSYNC_WAIT, 0,0));

	return (0);
}
#endif

/*
 * Read one fid and process it into a dirent and advance to the next (*fid)
 * has to be allocated a logical block in size, (*dirent) struct dirent length
 */

int
udf_read_fid_stream(struct vnode *vp, uint64_t *offset,
		struct fileid_desc *fid)
{
	struct udf_node *dir_node = VTOI(vp);
	struct udf_mount *ump = dir_node->ump;
	struct file_entry *fe = dir_node->fe;
	struct extfile_entry *efe = dir_node->efe;
	uint64_t file_size;
	int error;
	uint32_t fid_size, lb_size;

/*	assert(fid);
	assert(dirent);
	assert(dir_node);
	assert(offset);
	assert(*offset != 1); */

	/* check if we're past the end of the directory */
	if (fe != NULL)
		file_size = le64toh(fe->inf_len);
	else {
		/* assert(dir_node->efe != NULL); */
		file_size = le64toh(efe->inf_len);
	}
	if (*offset >= file_size)
		return (EINVAL);

	/* get maximum length of FID descriptor */
	lb_size = le32toh(ump->logical_vol->lb_size);

	/* initialise return values */
	fid_size = 0;
	memset(fid, 0, lb_size);

	if (file_size - *offset < UDF_FID_SIZE) {
		/* short dir ... */
		return (EIO);
	}

	error = udf_read_node(dir_node, (uint8_t *)fid, *offset, 
	    MIN(file_size - (*offset), lb_size));
	/*error = vn_rdwr(UIO_READ, vp,
			fid, MIN(file_size - (*offset), lb_size), *offset,
			UIO_SYSSPACE,  IO_NODELOCKED, FSCRED, 0, // IO_ALTSEMANTICS |
			NULL, curthread);*/
	if (error != 0)
		return (error);

	/*
	 * Check if we got a whole descriptor.
	 * TODO Try to `resync' directory stream when something is very wrong.
	 */

	/* check if our FID header is OK */
	error = udf_check_tag(fid);
	if (error != 0) {
		goto brokendir;
	}

	if (le16toh(fid->tag.id) != TAGID_FID) {
		error = EIO;
		goto brokendir;
	}

	/* check for length */
	fid_size = udf_fidsize(fid);
	if (file_size - (*offset) < fid_size) {
		error = EIO;
		goto brokendir;
	}

	/* check FID contents */
	error = udf_check_tag_payload((union dscrptr *)fid, lb_size);
brokendir:
	if (error != 0) {
		/* note that is sometimes a bit quick to report */
		/* RESYNC? */
		/* TODO: use udf_resync_fid_stream */
		return (EIO);
	}

	/* advance */
	*offset += fid_size;

	return (error);
}

#if 0
static void
udf_sync_pass(struct udf_mount *ump, kauth_cred_t cred, int waitfor,
	int pass, int *ndirty)
{
	struct udf_node *udf_node, *n_udf_node;
	struct vnode *vp;
	int vdirty, error;
	int on_type, on_flags, on_vnode;

derailed:
	KASSERT(mutex_owned(&mntvnode_lock));

	DPRINTF(SYNC, ("sync_pass %d\n", pass));
	udf_node = RBTOUDFNODE(RB_TREE_MIN(&ump->udf_node_tree));
	for (;udf_node; udf_node = n_udf_node) {
		DPRINTF(SYNC, ("."));

		udf_node->i_flags &= ~IN_SYNCED;
		vp = udf_node->vnode;

		mutex_enter(&vp->v_interlock);
		n_udf_node = RBTOUDFNODE(rb_tree_iterate(
			&ump->udf_node_tree, &udf_node->rbnode,
			RB_DIR_RIGHT));

		if (n_udf_node)
			n_udf_node->i_flags |= IN_SYNCED;

		/* system nodes are not synced this way */
		if (vp->v_vflag & VV_SYSTEM) {
			mutex_exit(&vp->v_interlock);
			continue;
		}

		/* check if its dirty enough to even try */
		on_type  = (waitfor == MNT_LAZY || vp->v_type == VNON);
		on_flags = ((udf_node->i_flags &
			(IN_ACCESSED | IN_UPDATE | IN_MODIFIED)) == 0);
		on_vnode = LIST_EMPTY(&vp->v_dirtyblkhd)
			&& UVM_OBJ_IS_CLEAN(&vp->v_uobj);
		if (on_type || (on_flags || on_vnode)) { /* XXX */
			/* not dirty (enough?) */
			mutex_exit(&vp->v_interlock);
			continue;
		}

		mutex_exit(&mntvnode_lock);
		error = vget(vp, LK_EXCLUSIVE | LK_NOWAIT);
		if (error != 0) {
			mutex_enter(&mntvnode_lock);
			if (error == ENOENT)
				goto derailed;
			*ndirty += 1;
			continue;
		}

		switch (pass) {
		case 1:
			VOP_FSYNC(vp, cred, 0 | FSYNC_DATAONLY,0,0);
			break;
		case 2:
			vdirty = vp->v_numoutput;
			if (vp->v_tag == VT_UDF)
				vdirty += udf_node->outstanding_bufs +
					udf_node->outstanding_nodedscr;
			if (vdirty == 0)
				VOP_FSYNC(vp, cred, 0,0,0);
			*ndirty += vdirty;
			break;
		case 3:
			vdirty = vp->v_numoutput;
			if (vp->v_tag == VT_UDF)
				vdirty += udf_node->outstanding_bufs +
					udf_node->outstanding_nodedscr;
			*ndirty += vdirty;
			break;
		}

		vput(vp);
		mutex_enter(&mntvnode_lock);
	}
	DPRINTF(SYNC, ("END sync_pass %d\n", pass));
}

void
udf_do_sync(struct udf_mount *ump, kauth_cred_t cred, int waitfor)
{
	int dummy, ndirty;

	mutex_enter(&mntvnode_lock);
recount:
	dummy = 0;
	DPRINTF(CALL, ("issue VOP_FSYNC(DATA only) on all nodes\n"));
	DPRINTF(SYNC, ("issue VOP_FSYNC(DATA only) on all nodes\n"));
	udf_sync_pass(ump, cred, waitfor, 1, &dummy);

	DPRINTF(CALL, ("issue VOP_FSYNC(COMPLETE) on all finished nodes\n"));
	DPRINTF(SYNC, ("issue VOP_FSYNC(COMPLETE) on all finished nodes\n"));
	udf_sync_pass(ump, cred, waitfor, 2, &dummy);

	if (waitfor == MNT_WAIT) {
		ndirty = ump->devvp->v_numoutput;
		DPRINTF(SYNC, ("counting pending blocks: on devvp %d\n",
			ndirty));
		udf_sync_pass(ump, cred, waitfor, 3, &ndirty);
		DPRINTF(SYNC, ("counted num dirty pending blocks %d\n",
			ndirty));
	
		if (ndirty) {
			/* 1/4 second wait */
			cv_timedwait(&ump->dirtynodes_cv, &mntvnode_lock,
				hz/4);
			goto recount;
		}
	}

	mutex_exit(&mntvnode_lock);
}
#endif

/*
 * Read and write file extent in/from the buffer.
 *
 * The splitup of the extent into seperate request-buffers is to minimise
 * copying around as much as possible.
 *
 * block based file reading and writing
 */

int
udf_read_internal(struct udf_node *node, uint8_t *blob)
{
	struct file_entry *fe = node->fe;
	struct extfile_entry *efe = node->efe;
	struct udf_mount *ump;
	uint64_t inflen;
	int addr_type, icbflags;
	uint32_t sector_size;
	uint8_t *pos;

	/* get extent and do some paranoia checks */
	ump = node->ump;
	sector_size = ump->sector_size;

	if (fe != NULL) {
		inflen = le64toh(fe->inf_len);
		pos = &fe->data[0] + le32toh(fe->l_ea);
		icbflags = le16toh(fe->icbtag.flags);
	} else {
		/*assert(node->efe != NULL); */
		inflen = le64toh(efe->inf_len);
		pos = &efe->data[0] + le32toh(efe->l_ea);
		icbflags = le16toh(efe->icbtag.flags);
	}
	addr_type = icbflags & UDF_ICB_TAG_FLAGS_ALLOC_MASK;

	/*assert(addr_type == UDF_ICB_INTERN_ALLOC); */
	/*assert(inflen < sector_size); */

	/* copy out info */
	memset(blob, 0, sector_size);
	memcpy(blob, pos, inflen);

	return (0);
}

#if 0
static int
udf_write_internal(struct udf_node *node, uint8_t *blob)
{
	struct udf_mount *ump;
	struct file_entry     *fe = node->fe;
	struct extfile_entry *efe = node->efe;
	uint64_t inflen;
	uint32_t sector_size;
	uint8_t  *pos;
	int icbflags, addr_type;

	/* get extent and do some paranoia checks */
	ump = node->ump;
	sector_size = ump->discinfo.sector_size;

	if (fe != NULL) {
		inflen   = le64toh(fe->inf_len);
		pos      = &fe->data[0] + le32toh(fe->l_ea);
		icbflags = le16toh(fe->icbtag.flags);
	} else {
		assert(node->efe != NULL);
		inflen   = le64toh(efe->inf_len);
		pos      = &efe->data[0] + le32toh(efe->l_ea);
		icbflags = le16toh(efe->icbtag.flags);
	}
	addr_type = icbflags & UDF_ICB_TAG_FLAGS_ALLOC_MASK;

	assert(addr_type == UDF_ICB_INTERN_ALLOC);
	assert(inflen < sector_size);

	/* copy in blob */
	/* memset(pos, 0, inflen); */
	memcpy(pos, blob, inflen);

	return (0);
}

void
udf_read_filebuf(struct udf_node *udf_node, struct buf *buf)
{
	struct buf *nestbuf;
	struct udf_mount *ump = udf_node->ump;
	uint64_t   *mapping;
	uint64_t    run_start;
	uint32_t    sector_size;
	uint32_t    buf_offset, sector, rbuflen, rblk;
	uint32_t    from, lblkno;
	uint32_t    sectors;
	uint8_t    *buf_pos;
	int error, run_length, what;

	sector_size = udf_node->ump->discinfo.sector_size;

	from    = buf->b_blkno;
	sectors = buf->b_bcount / sector_size;

	what = udf_get_c_type(udf_node);

	/* assure we have enough translation slots */
	KASSERT(buf->b_bcount / sector_size <= UDF_MAX_MAPPINGS);
	KASSERT(MAXPHYS / sector_size <= UDF_MAX_MAPPINGS);

	if (sectors > UDF_MAX_MAPPINGS) {
		printf("udf_read_filebuf: implementation limit on bufsize\n");
		buf->b_error  = EIO;
		biodone(buf);
		return;
	}

	mapping = malloc(sizeof(*mapping) * UDF_MAX_MAPPINGS, M_UDFTEMP, M_WAITOK);

	error = 0;
	DPRINTF(READ, ("\ttranslate %d-%d\n", from, sectors));
	error = udf_translate_file_extent(udf_node, from, sectors, mapping);
	if (error != 0) {
		buf->b_error  = error;
		biodone(buf);
		goto out;
	}
	DPRINTF(READ, ("\ttranslate extent went OK\n"));

	/* pre-check if its an internal */
	if (*mapping == UDF_TRANS_INTERN) {
		error = udf_read_internal(udf_node, (uint8_t *) buf->b_data);
		if (error != 0)
			buf->b_error  = error;
		biodone(buf);
		goto out;
	}
	DPRINTF(READ, ("\tnot intern\n"));

#ifdef DEBUG
	if (udf_verbose & UDF_DEBUG_TRANSLATE) {
		printf("Returned translation table:\n");
		for (sector = 0; sector < sectors; sector++) {
			printf("%d : %"PRIu64"\n", sector, mapping[sector]);
		}
	}
#endif

	/* request read-in of data from disc sheduler */
	buf->b_resid = buf->b_bcount;
	for (sector = 0; sector < sectors; sector++) {
		buf_offset = sector * sector_size;
		buf_pos    = (uint8_t *) buf->b_data + buf_offset;
		DPRINTF(READ, ("\tprocessing rel sector %d\n", sector));

		/* check if its zero or unmapped to stop reading */
		switch (mapping[sector]) {
		case UDF_TRANS_UNMAPPED:
		case UDF_TRANS_ZERO:
			/* copy zero sector TODO runlength like below */
			memset(buf_pos, 0, sector_size);
			DPRINTF(READ, ("\treturning zero sector\n"));
			nestiobuf_done(buf, sector_size, 0);
			break;
		default:
			DPRINTF(READ, ("\tread sector "
			    "%"PRIu64"\n", mapping[sector]));

			lblkno = from + sector;
			run_start  = mapping[sector];
			run_length = 1;
			while (sector < sectors-1) {
				if (mapping[sector+1] != mapping[sector]+1)
					break;
				run_length++;
				sector++;
			}

			/*
			 * nest an iobuf and mark it for async reading. Since
			 * we're using nested buffers, they can't be cached by
			 * design.
			 */
			rbuflen = run_length * sector_size;
			rblk    = run_start  * (sector_size/DEV_BSIZE);

			nestbuf = getiobuf(NULL, true);
			nestiobuf_setup(buf, nestbuf, buf_offset, rbuflen);
			/* nestbuf is B_ASYNC */

			/* identify this nestbuf */
			nestbuf->b_lblkno   = lblkno;
			assert(nestbuf->b_vp == udf_node->vnode);

			/* CD shedules on raw blkno */
			nestbuf->b_blkno      = rblk;
			nestbuf->b_proc       = NULL;
			nestbuf->b_rawblkno   = rblk;
			nestbuf->b_udf_c_type = what;

			udf_discstrat_queuebuf(ump, nestbuf);
		}
	}
out:
	/* if we're synchronously reading, wait for the completion */
	if ((buf->b_flags & B_ASYNC) == 0)
		biowait(buf);

	DPRINTF(READ, ("\tend of read_filebuf\n"));
	free(mapping, M_UDFTEMP);
	return;
}

void
udf_write_filebuf(struct udf_node *udf_node, struct buf *buf)
{
	struct buf *nestbuf;
	struct udf_mount *ump = udf_node->ump;
	uint64_t   *mapping;
	uint64_t    run_start;
	uint32_t    lb_size;
	uint32_t    buf_offset, lb_num, rbuflen, rblk;
	uint32_t    from, lblkno;
	uint32_t    num_lb;
	int error, run_length, what, s;

	lb_size = le32toh(udf_node->ump->logical_vol->lb_size);

	from   = buf->b_blkno;
	num_lb = buf->b_bcount / lb_size;

	what = udf_get_c_type(udf_node);

	/* assure we have enough translation slots */
	KASSERT(buf->b_bcount / lb_size <= UDF_MAX_MAPPINGS);
	KASSERT(MAXPHYS / lb_size <= UDF_MAX_MAPPINGS);

	if (num_lb > UDF_MAX_MAPPINGS) {
		printf("udf_write_filebuf: implementation limit on bufsize\n");
		buf->b_error  = EIO;
		biodone(buf);
		return;
	}

	mapping = malloc(sizeof(*mapping) * UDF_MAX_MAPPINGS, M_UDFTEMP, M_WAITOK);

	error = 0;
	DPRINTF(WRITE, ("\ttranslate %d-%d\n", from, num_lb));
	error = udf_translate_file_extent(udf_node, from, num_lb, mapping);
	if (error != 0) {
		buf->b_error  = error;
		biodone(buf);
		goto out;
	}
	DPRINTF(WRITE, ("\ttranslate extent went OK\n"));

	/* if its internally mapped, we can write it in the descriptor itself */
	if (*mapping == UDF_TRANS_INTERN) {
		/* TODO paranoia check if we ARE going to have enough space */
		error = udf_write_internal(udf_node, (uint8_t *) buf->b_data);
		if (error != 0)
			buf->b_error  = error;
		biodone(buf);
		goto out;
	}
	DPRINTF(WRITE, ("\tnot intern\n"));

	/* request write out of data to disc sheduler */
	buf->b_resid = buf->b_bcount;
	for (lb_num = 0; lb_num < num_lb; lb_num++) {
		buf_offset = lb_num * lb_size;
		DPRINTF(WRITE, ("\tprocessing rel lb_num %d\n", lb_num));

		/*
		 * Mappings are not that important here. Just before we write
		 * the lb_num we late-allocate them when needed and update the
		 * mapping in the udf_node.
		 */

		/* XXX why not ignore the mapping altogether ? */
		DPRINTF(WRITE, ("\twrite lb_num "
		    "%"PRIu64, mapping[lb_num]));

		lblkno = from + lb_num;
		run_start  = mapping[lb_num];
		run_length = 1;
		while (lb_num < num_lb-1) {
			if (mapping[lb_num+1] != mapping[lb_num]+1)
				if (mapping[lb_num+1] != mapping[lb_num])
					break;
			run_length++;
			lb_num++;
		}
		DPRINTF(WRITE, ("+ %d\n", run_length));

		/* nest an iobuf on the master buffer for the extent */
		rbuflen = run_length * lb_size;
		rblk = run_start * (lb_size/DEV_BSIZE);

		nestbuf = getiobuf(NULL, true);
		nestiobuf_setup(buf, nestbuf, buf_offset, rbuflen);
		/* nestbuf is B_ASYNC */

		/* identify this nestbuf */
		nestbuf->b_lblkno   = lblkno;
		KASSERT(nestbuf->b_vp == udf_node->vnode);

		/* CD shedules on raw blkno */
		nestbuf->b_blkno      = rblk;
		nestbuf->b_proc       = NULL;
		nestbuf->b_rawblkno   = rblk;
		nestbuf->b_udf_c_type = what;

		/* increment our outstanding bufs counter */
		s = splbio();
			udf_node->outstanding_bufs++;
		splx(s);

		udf_discstrat_queuebuf(ump, nestbuf);
	}
out:
	/* if we're synchronously writing, wait for the completion */
	if ((buf->b_flags & B_ASYNC) == 0)
		biowait(buf);

	DPRINTF(WRITE, ("\tend of write_filebuf\n"));
	free(mapping, M_UDFTEMP);
	return;
}
#endif

