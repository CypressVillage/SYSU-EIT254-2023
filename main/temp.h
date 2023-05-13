#ifndef _TEMP_H
#define _TEMP_H

float PT100_ResToTemp_L(float resistance);
float PT100_ResToTemp(float resistance);
float NTC_103KF_3435_ResToTemp_L(float resistance);
float NTC_103KF_3435_ResToTemp(float resistance);
float NTC_103KF_3950_ResToTemp_L(float resistance);
float NTC_103KF_3950_ResToTemp(float resistance);

#endif
