/* Extended Module Player
 * Copyright (C) 1996-2016 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See the COPYING
 * file for more information.
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <xmp.h>
#include "common.h"
#include "keys.h"

static int max_channels = -1;

void info_help(void)
{
char helptext[940];
// TODO handle XMP_*_(BACK)|(NEXT)_2
sprintf(helptext,
"COMMAND KEYS SUMMARY\n"
"     Space      Pause/unpause\n"
"    %c, Esc      Stop module and quit the player\n"
"    %c, Right    Advance to next order\n"
"    %c, Left     Return to previous order\n"
"    %c, Up       Advance to next module\n"
"    %c, Down     Return to previous module\n"
"    %c, %c        Display available commands\n"
"    %c - %c       Mute/unmute channels\n"
"      %c         Unmute all channels\n"
"      %c         Display current mixer type\n"
"      %c         Enable amiga mixer\n"
"      %c         Display current sequence\n"
"      %c         Toggle subsong explorer mode\n"
"      %c         Toggle loop mode\n"
"      %c         Display module information\n"
"      %c         Display combined instrument/sample list\n"
"      %c         Display instrument list\n"
"      %c         Display sample list\n"
"      %c         Display comment, if any\n"
"      %c         Play previous sequence\n"
"      %c         Play next sequence\n"
#ifdef XMP_CLEAR
#if XMP_CLEAR >= 32 && XMP_CLEAR <= 126
"      %c         Clear the screen\n"
#else
"      0x%X       Clear the screen\n"
#endif
#endif
#ifdef XMP_COMPACT
#if XMP_COMPACT >= 32 && XMP_COMPACT <= 126
"      %c         Use compact display\n"
#else
"      0x%X       Use compact display\n"
#endif
#endif
,

	XMP_QUIT, XMP_PAT_NEXT, XMP_PAT_BACK,
	XMP_PAT_NEXT, XMP_MOD_BACK, XMP_HELP, XMP_HELP_2,
	XMP_MUTE_1, XMP_MUTE_10, XMP_MUTE_ALL,
	XMP_MIXER, XMP_AMIGA, XMP_CURR_SEQ,
	XMP_EXPLORER, XMP_LOOP, XMP_MODULE_INFO,
	XMP_FULL_INFO, XMP_INST_INFO, XMP_SAMPLE_INFO,
	XMP_COMMENT, XMP_SEQ_BACK, XMP_SEQ_NEXT
#ifdef XMP_CLEAR
, XMP_CLEAR
#endif
#ifdef XMP_CLEAR
, XMP_COMPACT
#endif

);
report(helptext);
}

void info_mod(struct xmp_module_info *mi, int mode)
{
	int i;
	int num_seq;
	int total_time;

	report("Module name  : %s\n", mi->mod->name);
	report("Module type  : %s", mi->mod->type);

	if (mode != XMP_MODE_AUTO) {
		struct player_mode *pm;
		for (pm = pmode; pm->name != NULL; pm++) {
			if (pm->mode == mode) {
				report(" [play as:%s]", pm->desc);
				break;
			}
		}
	}

	report("\nModule length: %d patterns\n", mi->mod->len);
	report("Patterns     : %d\n", mi->mod->pat);
	report("Instruments  : %d\n", mi->mod->ins);
	report("Samples      : %d\n", mi->mod->smp);
	report("Channels     : %d [ ", mi->mod->chn);

	for (i = 0; i < mi->mod->chn; i++) {
		if (mi->mod->xxc[i].flg & XMP_CHANNEL_SYNTH) {
			report("S ");
		} else if (mi->mod->xxc[i].flg & XMP_CHANNEL_MUTE) {
			report("- ");
		} else if (mi->mod->xxc[i].flg & XMP_CHANNEL_SURROUND) {
			report("^ ");
		} else {
			report("%x ", mi->mod->xxc[i].pan >> 4);
		}
	}
	report("]\n");

	total_time = mi->seq_data[0].duration;

	report("Duration     : %dmin%02ds", (total_time + 500) / 60000,
					((total_time + 500) / 1000) % 60);

	/* Check non-zero-length sequences */
	num_seq = 0;
	for (i = 0; i <  mi->num_sequences; i++) {
		if (mi->seq_data[i].duration > 0)
			num_seq++;
	}

	if (num_seq > 1) {
		report(" (main sequence)\n");
		for (i = 1; i < mi->num_sequences; i++) {
			int dur = mi->seq_data[i].duration;

			if (dur == 0) {
				continue;
			}

			report("               %dmin%02ds "
				"(sequence %d at position %02X)\n",
				(dur + 500) / 60000, ((dur + 500) / 1000) % 60,
				i, mi->seq_data[i].entry_point);
		}
	} else {
		report("\n");
	}
}

void info_frame_init(void)
{
	max_channels = 0;
}

#define MSG_SIZE 80
static int msg_timer = 0;
static char msg_text[MSG_SIZE];

void info_message(char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	msg_timer = 300000;
	vsnprintf(msg_text, MSG_SIZE, format, ap);
	va_end(ap);
}

static void fix_info_02x(int val, char *buf)
{
	if (val <= 0xff) {
		snprintf(buf, 3, "%02X", val);
	} else if (val <= 0xfff) {
		snprintf(buf, 3, "+%X", val >> 8);
	} else {
		strcpy(buf, "++");
	}
}

void info_frame(struct xmp_module_info *mi, struct xmp_frame_info *fi, struct control *ctl, int reprint)
{
	static int ord = -1, spd = -1, bpm = -1;
	char rowstr[3], numrowstr[3];
	char chnstr[3], maxchnstr[3];
	int time;
	char x;

	if (fi->virt_used > max_channels)
		max_channels = fi->virt_used;

	if (!reprint && fi->frame != 0)
		return;

	time = fi->time / 100;

	/* Show mixer type */
	x = ' ';
	if (ctl->amiga_mixer) {
		switch (ctl->mixer_type) {
		case XMP_MIXER_STANDARD:
			x = '-';
			break;
		case XMP_MIXER_A500:
			x = 'A';
			break;
		case XMP_MIXER_A500F:
			x = 'F';
			break;
		default:
			x = 'x';
		}
	}

	if (msg_timer > 0) {
		report("\r%-61.61s %c%c%c", msg_text,
			ctl->explore ? 'Z' : ' ',
			ctl->loop ? 'L' : ' ', x);
		msg_timer -= fi->frame_time * fi->speed / 6;
		if (msg_timer == 0) {
			msg_timer--;
		} else {
			goto print_time;
		}
	}

	if (msg_timer < 0) {
		reprint = 1;
		msg_timer = 0;
	}

	if (reprint || fi->pos != ord || fi->bpm != bpm || fi->speed != spd) {
		if(ctl->compact == 1) {
			report("\r[%02X/%02X]       0:00:00.0",
			fi->pattern, mi->mod->pat - 1);
		} else report("\rSpeed[%02X] BPM[%02X] Pos[%02X/%02X] "
			 "Pat[%02X/%02X] Row[  /  ] Chn[  /  ]      0:00:00.0",
					fi->speed, fi->bpm,
					fi->pos, mi->mod->len - 1,
					fi->pattern, mi->mod->pat - 1);
		ord = fi->pos;
		bpm = fi->bpm;
		spd = fi->speed;
	}

	fix_info_02x(fi->row, rowstr);
	fix_info_02x(fi->num_rows - 1, numrowstr);
	fix_info_02x(fi->virt_used, chnstr);
	fix_info_02x(max_channels, maxchnstr);

	if(ctl-> compact == 1) {
	report("\b\b\b\b\b\b\b\b\b\b\b\b\b\b"
	       "%c%c%c",
		ctl->explore ? 'Z' : ' ', ctl->loop ? 'L' : ' ', x);
	} else report("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b"
	       "%2.2s/%2.2s] Chn[%2.2s/%2.2s] %c%c%c",
		rowstr, numrowstr, chnstr, maxchnstr,
		ctl->explore ? 'Z' : ' ', ctl->loop ? 'L' : ' ', x);

    print_time:

	if (ctl->pause) {
		report(" - PAUSED -");
	} else {
		report("%3d:%02d:%02d.%d",
			(int)(time / (60 * 600)), (int)((time / 600) % 60),
			(int)((time / 10) % 60), (int)(time % 10));
	}

	fflush(stdout);
}

void info_ins_smp(struct xmp_module_info *mi)
{
	int i, j;
	struct xmp_module *mod = mi->mod;

	report("Instruments and samples:\n");
	report("   Instrument name                  Smp  Size  Loop  End    Vol Fine Xpo Pan\n");
	for (i = 0; i < mod->ins; i++) {
		struct xmp_instrument *ins = &mod->xxi[i];

		if (strlen(ins->name) == 0 && ins->nsm == 0) {
			continue;
		}

		report("%02x %-32.32s ", i + 1, ins->name);

		for (j = 0; j < ins->nsm; j++) {
			struct xmp_subinstrument *sub = &ins->sub[j];
			struct xmp_sample *smp = &mod->xxs[sub->sid];

			if (j > 0) {
				if (smp->len == 0) {
					continue;
				}
				report("%36.36s", " ");
			}

			report("[%02x] %05x%c%05x %05x%c V%02x %+04d %+03d P%02x\n",
				sub->sid + 1,
				smp->len,
				smp->flg & XMP_SAMPLE_16BIT ? '+' : ' ',
				smp->lps,
				smp->lpe,
				smp->flg & XMP_SAMPLE_LOOP ?
					smp->flg & XMP_SAMPLE_LOOP_BIDIR ?
						'B' : 'L' : ' ',
				sub->vol,
				sub->fin,
				sub->xpo,
				sub->pan & 0xff);
		}

		if (j == 0) {
			report("[  ] ----- ----- -----  --- ---- --- ---\n");
		}

	}
}

void info_instruments(struct xmp_module_info *mi)
{
	int i, j;
	struct xmp_module *mod = mi->mod;

	report("Instruments:\n");
	report("   Instrument name                  Vl Fade Env Ns Sub  Gv Vl Fine Xpo Pan Sm\n");
	for (i = 0; i < mod->ins; i++) {
		struct xmp_instrument *ins = &mod->xxi[i];

		if (strlen(ins->name) == 0 && ins->nsm == 0) {
			continue;
		}

		report("%02x %-32.32s %02x %04x %c%c%c %02x ",
			i + 1, ins->name, ins->vol, ins->rls,
			ins->aei.flg & XMP_ENVELOPE_ON ? 'A' : '-',
			ins->fei.flg & XMP_ENVELOPE_ON ? 'F' : '-',
			ins->pei.flg & XMP_ENVELOPE_ON ? 'P' : '-',
			ins->nsm
		);

		for (j = 0; j < ins->nsm; j++) {
			struct xmp_subinstrument *sub = &ins->sub[j];
			struct xmp_sample *smp = &mod->xxs[sub->sid];

			if (j > 0) {
				if (smp->len == 0) {
					continue;
				}
				report("%51.51s", " ");
			}

			report("[%02x] %02x %02x %+04d %+03d P%02x %02x\n",
				j + 1,
				sub->gvl,
				sub->vol,
				sub->fin,
				sub->xpo,
				sub->pan & 0xff,
				sub->sid);
		}

		if (j == 0) {
			report("[  ] -- -- ---- --- --- --\n");
		}

	}
}

void info_samples(struct xmp_module_info *mi)
{
	int i;
	struct xmp_module *mod = mi->mod;

	report("Samples:\n");
	report("   Sample name                      Length Start  End    Flags\n");
	for (i = 0; i < mod->smp; i++) {
		struct xmp_sample *smp = &mod->xxs[i];

		if (strlen(smp->name) == 0 && smp->len == 0) {
			continue;
		}

		report("%02x %-32.32s %06x %06x %06x %s %s %s\n",
			i + 1, smp->name,
			smp->len,
			smp->lps,
			smp->lpe,
			smp->flg & XMP_SAMPLE_16BIT ? "16" : "--",
			smp->flg & XMP_SAMPLE_LOOP  ? "L"  : "-",
			smp->flg & XMP_SAMPLE_LOOP_BIDIR ? "B" : "-");
	}
}

void info_comment(struct xmp_module_info *mi)
{
	char *c = mi->comment;

	if (mi->comment == NULL) {
		report("No comment.\n");
		return;
	}

	while (*c != 0) {
		report("> ");
		do {
			if (*c == 0)
				break;
			report("%c", *c);
		} while (*c++ != '\n');
	}		
	report("\n\n");
}

