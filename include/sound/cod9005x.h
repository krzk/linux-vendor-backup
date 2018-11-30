#ifndef __SOUND_COD9005X_H__
#define __SOUND_COD9005X_H__

#define COD9005X_MICBIAS1	0
#define COD9005X_MICBIAS2	1

extern void cod9005x_call_notifier(int irq1, int irq2, int irq3, int irq4, int status1,
		int param1, int param2, int param3, int param4, int param5);
int cod9005x_mic_bias_ev(struct snd_soc_codec *codec,int mic_bias, int event);

#endif /* __SOUND_COD9005X_H__ */
