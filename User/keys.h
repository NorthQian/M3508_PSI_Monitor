#ifndef _KEYS_H_
#define _KEYS_H_

#include "main.h"


#define ADC_RESOLUTION 65535
// #define key_htim htim6
// extern TIM_HandleTypeDef key_htim;

/**
 * @brief Key struct
 *
 */

typedef struct
{
    GPIO_TypeDef *GPIOx;    // GPIO PORT
    uint16_t GPIO_Pin;      // GPIO PIN
    uint8_t key_stg;        // use for tim
    uint8_t key_flag;       // use for judge
    uint8_t key_push_level; // the gpio level when pushing

} KeyInstance;

typedef struct {

 uint8_t key_stg;        // use for tim
    uint8_t key_flag;       // use for judge
    float val;
    float normval;

}KeyAInstance;

extern KeyInstance KEY0_Instance;
// extern KeyAInstance KEY_UP;
// extern KeyAInstance KEY_DOWN;
// extern KeyAInstance KEY_PRESS;
// extern KeyAInstance KEY_LEFT;
// extern KeyAInstance KEY_RIGHT;


void Key_Scan_Init(uint8_t Freq_TIM);
void Key_Progress(void);
void Key_Scan(KeyInstance *p);
void KeyA_Scan(KeyAInstance *p,float adc_value);

#endif