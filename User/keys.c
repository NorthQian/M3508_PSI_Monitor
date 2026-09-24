#include "keys.h"

KeyInstance KEY0_Instance = {GPIOA, GPIO_PIN_15, 0, 0, GPIO_PIN_RESET};
//KeyAInstance KEY_UP={0,0,1.96,3.25};
//KeyAInstance KEY_DOWN={0,0,2.61,3.25};
//KeyAInstance KEY_PRESS={0,0,0.1,3.25};
//KeyAInstance KEY_LEFT={0,0,0.65,3.25};
//KeyAInstance KEY_RIGHT={0,0,1.31,3.25};

/**
 * @brief Initial TIM use for key scan
 * @note Do not forget NVIC settings
 * @param Freq_TIM TIM AHB Frequent
 * @param htim  pointer to TIM
 */

// void Key_Scan_Init(uint8_t Freq_TIM)
// {

//     key_htim.Instance->PSC = Freq_TIM * 100 - 1;
//     key_htim.Instance->ARR = 100;
//     /* get 100hz Tim*/
//     HAL_TIM_Base_Start_IT(&key_htim);
// }

/**
 * @brief put it to TIM Handler
 * 
 */
// void Key_Progress(void)
// {

//     if (__HAL_TIM_GET_FLAG(&key_htim, TIM_IT_UPDATE))
//     {
//         Key_Scan(&KEY0_Instance);
//         // KeyA_Scan(&KEY_DOWN,ADC_Val[1]);
//         // KeyA_Scan(&KEY_PRESS,ADC_Val[1]);
//         // KeyA_Scan(&KEY_LEFT,ADC_Val[1]);
//         // KeyA_Scan(&KEY_RIGHT,ADC_Val[1]);
//         // KeyA_Scan(&KEY_UP,ADC_Val[1]);
        
//         __HAL_TIM_CLEAR_FLAG(&key_htim, TIM_IT_UPDATE);
//     }
// }

/**
 * @brief Key scan funtion
 * 
 * @param Key_Instance
 */
void Key_Scan(KeyInstance *p)
{
    switch (p->key_stg)
    {
    case 0:
        if (HAL_GPIO_ReadPin(p->GPIOx, p->GPIO_Pin) == p->key_push_level)
        {
            p->key_stg = 1;
        }
        else
        {
            p->key_stg = 0;
        }

        break;

    case 1:
        if (HAL_GPIO_ReadPin(p->GPIOx, p->GPIO_Pin) == p->key_push_level)
        {
            p->key_stg = 2;
        }
        else
        {
            p->key_stg = 0;
        }

        break;

    case 2:
        if (HAL_GPIO_ReadPin(p->GPIOx, p->GPIO_Pin) != p->key_push_level)
        {
            p->key_stg = 0;

            p->key_flag = 1;
				
        }
        else
        {
            p->key_stg = 2;
        }

        break;
    }
}
void KeyA_Scan(KeyAInstance *p,float adc_value){

switch (p->key_stg)
    {
    case 0:
        if (((adc_value*3.3f/ADC_RESOLUTION)>(p->val-0.1))&&((adc_value*3.3f/ADC_RESOLUTION)<(p->val+0.1)))
        {
            p->key_stg = 1;
        }
        else
        {
            p->key_stg = 0;
        }

        break;

    case 1:
        if (((adc_value*3.3f/ADC_RESOLUTION)>(p->val-0.1))&&((adc_value*3.3f/ADC_RESOLUTION)<(p->val+0.1)))
        {
            p->key_stg = 2;
        }
        else
        {
            p->key_stg = 0;
        }

        break;

    case 2:
        if (((adc_value*3.3f/ADC_RESOLUTION)>(p->normval-0.1))&&((adc_value*3.3f/ADC_RESOLUTION)<(p->normval+0.1)))
        {
            p->key_stg = 0;

            p->key_flag = 1;
					
        }
        else
        {
            p->key_stg = 2;
        }

        break;
    }



}