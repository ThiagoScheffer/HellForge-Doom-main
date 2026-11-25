#ifndef HELLFORGE_P_HEALTH_RECOVERY_H
#define HELLFORGE_P_HEALTH_RECOVERY_H

typedef enum {
    HEALTH_RECOVERY_CLASSIC = 0,
    HEALTH_RECOVERY_MEDIKITS = 1,
    HEALTH_RECOVERY_ALL = 2
} health_recovery_mode_t;
#ifdef __cplusplus
extern "C" {
#endif
void P_SetHealthRecovery(int mode);
int P_HealthRecoveryMode(void);
int P_HealthRecoveryAllowed(void);
#ifdef __cplusplus
}
#endif
#endif
