#include <stdint.h>
#include <errno.h>
#include <stdio.h>

#include <osmocore/msgb.h>
#include <osmocore/rsl.h>
#include <osmocore/tlv.h>
#include <osmocore/protocol/gsm_04_08.h>

#include <osmocom/lapdm.h>
#include <osmocom/rslms.h>
#include <osmocom/layer3.h>
#include <osmocom/osmocom_data.h>
#include <osmocom/layer2.h>

/* send location updating request * (as part of RSLms EST IND /
   LAPDm SABME) */
static int gsm48_tx_loc_upd_req(struct osmocom_ms *ms, uint8_t chan_nr)
{
	struct msgb *msg = msgb_alloc_headroom(256, 16, "loc_upd_req");
	struct gsm48_hdr *gh;
	struct gsm48_loc_upd_req *lu_r;

	gh = (struct gsm48_hdr *) msgb_put(msg, sizeof(*gh));
	gh->proto_discr = GSM48_PDISC_MM;
	gh->msg_type = GSM48_MT_MM_LOC_UPD_REQUEST;
	lu_r = (struct gsm48_loc_upd_req *) msgb_put(msg, sizeof(*lu_r));
	lu_r->type = GSM48_LUPD_IMSI_ATT;
	lu_r->key_seq = 0;
	/* FIXME: set LAI and CM1 */
	/* FIXME: set MI */
	lu_r->mi_len = 0;

	return rslms_tx_rll_req_l3(ms, RSL_MT_EST_REQ, chan_nr, 0, msg);
}

static int gsm48_rx_imm_ass(struct msgb *msg, struct osmocom_ms *ms)
{
	struct gsm48_imm_ass *ia = msgb_l3(msg);
	uint8_t ch_type, ch_subch, ch_ts;
	uint16_t arfcn;

	rsl_dec_chan_nr(ia->chan_desc.chan_nr, &ch_type, &ch_subch, &ch_ts);
	arfcn = ia->chan_desc.h0.arfcn_low | (ia->chan_desc.h0.arfcn_high << 8);

	printf("GSM48 IMM ASS (ra=0x%02x, chan_nr=0x%02x, ARFCN=%u, TS=%u, SS=%u, TSC=%u) ",
		ia->req_ref.ra, ia->chan_desc.chan_nr, arfcn, ch_ts, ch_subch,
		ia->chan_desc.h0.tsc);

	/* FIXME: compare RA and GSM time with when we sent RACH req */

	/* check if we can support this type of channel at the moment */
	if (ch_type != RSL_CHAN_SDCCH4_ACCH || ch_ts != 0 ||
	    ia->chan_desc.h0.h == 1) {
		printf("UNSUPPORTED!\n");
		return 0;
	}

	/* request L1 to go to dedicated mode on assigned channel */
	tx_ph_dm_est_req(ms, arfcn, ia->chan_desc.chan_nr);

	/* request L2 to establish the SAPI0 connection */
	gsm48_tx_loc_upd_req(ms, ia->chan_desc.chan_nr);

	return 0;
}

int gsm48_rx_ccch(struct msgb *msg, struct osmocom_ms *ms)
{
	struct gsm48_system_information_type_header *sih = msgb_l3(msg);
	int rc = 0;

	if (sih->rr_protocol_discriminator != GSM48_PDISC_RR)
		printf("PCH pdisc != RR\n");
	
	switch (sih->system_information) {
	case GSM48_MT_RR_PAG_REQ_1:
	case GSM48_MT_RR_PAG_REQ_2:
	case GSM48_MT_RR_PAG_REQ_3:
		/* FIXME: implement decoding of paging request */
		break;
	case GSM48_MT_RR_IMM_ASS:
		rc = gsm48_rx_imm_ass(msg, ms);
		break;
	default:
		printf("unknown PCH/AGCH type 0x%02x\n", sih->system_information);
		rc = -EINVAL;
	}

	return rc;
}

int gsm48_rx_dcch(struct msgb *msg, struct osmocom_ms *ms)
{
	return -EINVAL;
}