/*
 * escore-vs.h  --  voice sense interface for Audience earSmart chips
 *
 * Copyright 2011-2013 Audience, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ESCORE_VS_H
#define _ESCORE_VS_H

#include <linux/firmware.h>

/* Maximum size of keyword parameter block in bytes. */
#define ES_VS_KEYWORD_PARAM_MAX 512
#define MAX_NO_OF_VS_KW		5

struct escore_voice_sense {
	int vs_wakeup_keyword;
	int vs_irq;
	struct firmware *vs;
	struct firmware *bkg;
	struct firmware *kw[MAX_NO_OF_VS_KW];
	u16 vs_active_keywords;
	u16 vs_get_event;
	struct mutex vs_event_mutex;
	u16 cvs_preset;
};

extern int escore_vs_init(struct escore_priv *escore);
void escore_vs_exit(struct escore_priv *escore);
extern int escore_vs_load(struct escore_priv *escore);
extern int escore_get_vs_sleep(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol);
extern int escore_put_vs_sleep(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol);
extern int escore_get_vs_wakeup_keyword(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol);
extern int escore_put_vs_wakeup_keyword(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol);
extern int escore_put_vs_stored_keyword(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol);
int escore_put_cvs_preset_value(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol);
int escore_get_cvs_preset_value(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol);
int escore_vs_sleep_enable(struct escore_priv *escore);
int escore_put_vs_activate_keyword(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol);
int escore_get_vs_activate_keyword(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol);
int escore_vs_request_firmware(struct escore_priv *escore,
				const char *vs_filename);
void escore_vs_release_firmware(struct escore_priv *escore);
int escore_vs_request_bkg(struct escore_priv *escore, const char *vs_filename);
void escore_vs_release_bkg(struct escore_priv *escore);
int escore_vs_request_keywords(struct escore_priv *escore);
void escore_vs_release_keywords(struct escore_priv *escore);
int escore_vs_write_bkg_and_keywords(struct escore_priv *escore);
int escore_vs_put_control_enum(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol);
int escore_vs_get_control_enum(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol);
int escore_vs_put_control_value(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol);
int escore_vs_get_control_value(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol);


#endif /* _ESCORE_VS_H */
