/**
 ****************************************************************************************
 *
 * @file usr_design.c
 *
 * @brief Product related design.
 *
 * Copyright (C) Quintic 2012-2013
 *
 * $Rev: 1.0 $
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup  USR
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include <stdint.h>
#include "app_env.h"
#include "led.h"
#include "uart.h"
#include "lib.h"
#include "usr_design.h"
#include "gpio.h"
#include "button.h"
#if (defined(QN_ADV_WDT))
#include "wdt.h"
#endif
#include "sleep.h"
#include	"buzz.h"
#if (FB_JOYSTICKS)
#include "joysticks.h"
#endif
#if 	(CONFIG_ENABLE_DRIVER_MPU6050 == TRUE)
#include 	"i2c.h"
#include	"mpu6050.h"		
#endif
#if	(FB_BIT)
#include 	"timer.h"
#include	"buzz.h"
#endif

/*
 * MACRO DEFINITIONS
 ****************************************************************************************
 */

#define LED_ON_DUR_ADV_FAST        2
#define LED_OFF_DUR_ADV_FAST       (uint16_t)((GAP_ADV_FAST_INTV2*0.625)/10)
#define LED_ON_DUR_ADV_SLOW        2
#define LED_OFF_DUR_ADV_SLOW       (uint16_t)((GAP_ADV_SLOW_INTV*0.625)/10)
#define LED_ON_DUR_CON          0xffff
#define LED_OFF_DUR_CON                   0
#define LED_ON_DUR_IDLE                   0
#define LED_OFF_DUR_IDLE                  0xffff

#define APP_HEART_RATE_MEASUREMENT_TO     1400 // 14s
#define APP_HRPS_ENERGY_EXPENDED_STEP     50

#define EVENT_BUTTON1_PRESS_ID            0

///IOS Connection Parameter
#define IOS_CONN_INTV_MAX                              0x0080
#define IOS_CONN_INTV_MIN                              0x0040
#define IOS_SLAVE_LATENCY                              0x0000
#define IOS_STO_MULT                                   0x012c

/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */
#if (defined(QN_ADV_WDT))
static void adv_wdt_to_handler(void)
{
    ke_state_set(TASK_APP, APP_IDLE);

    // start adv
    app_gap_adv_start_req(GAP_GEN_DISCOVERABLE|GAP_UND_CONNECTABLE,
                          app_env.adv_data, app_set_adv_data(GAP_GEN_DISCOVERABLE),
                          app_env.scanrsp_data, app_set_scan_rsp_data(app_get_local_service_flag()),
                          GAP_ADV_FAST_INTV1, GAP_ADV_FAST_INTV2);
}
struct usr_env_tag usr_env = {LED_ON_DUR_IDLE, LED_OFF_DUR_IDLE, false, adv_wdt_to_handler,0,0,0,0};
#else
struct usr_env_tag usr_env = {LED_ON_DUR_IDLE, LED_OFF_DUR_IDLE,0,0,0,0};
#endif

struct test_data_tag test_data = {0,0,0,0,0,0,0};

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief   Led1 for BLE status
 ****************************************************************************************
 */
static void usr_led1_set(uint16_t timer_on, uint16_t timer_off)
{
    usr_env.led1_on_dur = timer_on;
    usr_env.led1_off_dur = timer_off;

    if (timer_on == 0 || timer_off == 0)
    {
        if (timer_on == 0)
        {
            led_set(1, LED_OFF);
        }
        if (timer_off == 0)
        {
            led_set(1, LED_ON);
        }
        ke_timer_clear(APP_SYS_LED_1_TIMER, TASK_APP);
    }
    else
    {
        led_set(1, LED_OFF);
        ke_timer_set(APP_SYS_LED_1_TIMER, TASK_APP, timer_off);
    }
}

/**
 ****************************************************************************************
 * @brief   Led 1 flash process
 ****************************************************************************************
 */
static void usr_led1_process(void)
{
    if(led_get(1) == LED_ON)
    {
        led_set(1, LED_OFF);
        ke_timer_set(APP_SYS_LED_1_TIMER, TASK_APP, usr_env.led1_off_dur);
    }
    else
    {
        led_set(1, LED_ON);
        ke_timer_set(APP_SYS_LED_1_TIMER, TASK_APP, usr_env.led1_on_dur);
    }
}

#if	(FB_BIT)
/**
 ****************************************************************************************
 * @brief   Led 1 flash process
 ****************************************************************************************
 */
int app_test_led_process_timer_handler(ke_msg_id_t const msgid, void const *param,
                               ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    if(led_get(2) == LED_ON && led_get(3) == LED_OFF)
    {
        led_set(2, LED_OFF);
        led_set(3, LED_ON);
        ke_timer_set(APP_TEST_SYS_LED_TIMER, TASK_APP, 100);
    }
    else
    if(led_get(2) == LED_OFF && led_get(3) == LED_ON)		
    {
        led_set(2, LED_ON);
        led_set(3, LED_OFF);
        ke_timer_set(APP_TEST_SYS_LED_TIMER, TASK_APP,100);
    }
		return(KE_MSG_CONSUMED);
}
#endif

/**
 ****************************************************************************************
 * @brief   Show proxr alert. User can add their own code here to show alert.
 ****************************************************************************************
 */
static void usr_proxr_alert(struct proxr_alert_ind *param)
{
    // If it is PROXR_LLS_CHAR writting indication, don't alert,
    // otherwise (PROXR_IAS_CHAR, disconnect) to alert
    if (param->char_code == PROXR_IAS_CHAR || app_proxr_env->enabled == false)
    {
        if(param->alert_lvl == 2)
        {
            buzzer_on(BUZZ_VOL_HIGH);
            ke_timer_set(APP_PROXR_ALERT_STOP_TIMER, TASK_APP, 500);    // 5 seconds
        }
        else if(param->alert_lvl == 1)
        {
            buzzer_on(BUZZ_VOL_LOW);
            ke_timer_set(APP_PROXR_ALERT_STOP_TIMER, TASK_APP, 500);    // 5 seconds
        }
        else
        {
            buzzer_off();
            ke_timer_clear(APP_PROXR_ALERT_STOP_TIMER, TASK_APP);
        }
    }

}


/**
 ****************************************************************************************
 * @brief   Application task message handler
 ****************************************************************************************
 */
void app_task_msg_hdl(ke_msg_id_t const msgid, void const *param)
{
    switch(msgid)
    {
        case GAP_SET_MODE_REQ_CMP_EVT:
            if(APP_IDLE == ke_state_get(TASK_APP))
            {
                usr_led1_set(LED_ON_DUR_ADV_FAST, LED_OFF_DUR_ADV_FAST);
                ke_timer_set(APP_ADV_INTV_UPDATE_TIMER, TASK_APP, 30 * 100);
#if (defined(QN_ADV_WDT))
                usr_env.adv_wdt_enable = true;
#endif
            }
            else if(APP_ADV == ke_state_get(TASK_APP))
            {
                usr_led1_set(LED_ON_DUR_ADV_SLOW, LED_OFF_DUR_ADV_SLOW);
#if (defined(QN_ADV_WDT))
                usr_env.adv_wdt_enable = true;
#endif
            }
						
						usr_env.test_flag = 0;
						
            break;

        case GAP_ADV_REQ_CMP_EVT:
            usr_led1_set(LED_ON_DUR_IDLE, LED_OFF_DUR_IDLE);
            ke_timer_clear(APP_ADV_INTV_UPDATE_TIMER, TASK_APP);
            break;

        case GAP_DISCON_CMP_EVT:
            usr_led1_set(LED_ON_DUR_IDLE, LED_OFF_DUR_IDLE);
						if (usr_env.usr_discon.nb < DISCON_COUNT_MAX)
						{
								//QPRINTF("reason is 0x%X\r\n",((struct gap_discon_cmp_evt*)param)->reason);
								usr_env.usr_discon.reason[usr_env.usr_discon.nb + 1] = ((struct gap_discon_cmp_evt*)param)->reason;
						}
						else
								usr_env.usr_discon.nb = 98;
						usr_env.usr_discon.nb++;
						if (!usr_env.test_flag)
            // start adv
            app_gap_adv_start_req(GAP_GEN_DISCOVERABLE|GAP_UND_CONNECTABLE,
                    app_env.adv_data, app_set_adv_data(GAP_GEN_DISCOVERABLE),
                    app_env.scanrsp_data, app_set_scan_rsp_data(app_get_local_service_flag()),
                    GAP_ADV_FAST_INTV1, GAP_ADV_FAST_INTV2);
            break;

        case GAP_LE_CREATE_CONN_REQ_CMP_EVT:
            if(((struct gap_le_create_conn_req_cmp_evt *)param)->conn_info.status == CO_ERROR_NO_ERROR)
            {
                if(GAP_PERIPHERAL_SLV == app_get_role())
                {
                    ke_timer_clear(APP_ADV_INTV_UPDATE_TIMER, TASK_APP);
                    usr_led1_set(LED_ON_DUR_CON, LED_OFF_DUR_CON);
#if (defined(QN_ADV_WDT))
                    usr_env.adv_wdt_enable = false;
#endif

                    // Update cnx parameters
                    //if (((struct gap_le_create_conn_req_cmp_evt *)param)->conn_info.con_interval >  IOS_CONN_INTV_MAX)
                    {
                        // Update connection parameters here
                        struct gap_conn_param_update conn_par;
                        /// Connection interval minimum
                        conn_par.intv_min = IOS_CONN_INTV_MIN;
                        /// Connection interval maximum
                        conn_par.intv_max = IOS_CONN_INTV_MAX;
                        /// Latency
                        conn_par.latency = IOS_SLAVE_LATENCY;
                        /// Supervision timeout, Time = N * 10 msec
                        conn_par.time_out = IOS_STO_MULT;
                        app_gap_param_update_req(((struct gap_le_create_conn_req_cmp_evt *)param)->conn_info.conhdl, &conn_par);
                    }
                }
            }
            break;
				case QPPS_DAVA_VAL_IND:
				{
//						if (((struct qpps_data_val_ind*)param)->data[0] == 'C' )
//							usr_env.test_flag  = 1;
//						else
//							if 		(((struct qpps_data_val_ind*)param)->data[0] == 'T' )				
//									ke_timer_set(APP_MPU6050_TEMPERATURE_TEST_TIMER,TASK_APP,2);
						//app_qpps_data_send(app_qpps_env->conhdl,0,((struct qpps_data_val_ind*)param)->length,((struct qpps_data_val_ind*)param)->data);
#if	(FB_BIT)
						switch(((struct qpps_data_val_ind*)param)->data[0])
						{
								case	'T'	:
								case	't'	:
								{
										ke_timer_set(APP_MPU6050_TEMPERATURE_TEST_TIMER,TASK_APP,2);
								}break;
								case	'P'	:
								case	'p'	:
								{
										ke_timer_set(APP_TEST_PASSED_TIMER,TASK_APP,2);
								}break;
								case	'N'	:
								case	'n'	:
								{
										usr_env.test_result = ((struct qpps_data_val_ind*)param)->data[1];
										ke_timer_set(APP_TEST_ERROR_TIMER,TASK_APP,2);
								}break;
								case	'D'	:
								case	'd'	:
								{		
											QPRINTF("discon timer is %d\r\n",usr_env.usr_discon.nb);
											if (usr_env.usr_discon.nb > 0)
											{
													app_qpps_data_send(app_qpps_env->conhdl,0,(usr_env.usr_discon.nb + 1),usr_env.usr_discon.reason);
											}
											else
											{
													app_qpps_data_send(app_qpps_env->conhdl,0,1,usr_env.usr_discon.reason);
											}
								}
								break;
								case	'C'	:
								case	'c'	:
								{
										app_gap_discon_req(app_env.dev_rec->conhdl);
								}break;
								
								
								default	:break;
						}
						usr_env.time++;
#endif
						for (uint8_t k = 0;k < ((struct qpps_data_val_ind*)param)->length;k++)
							QPRINTF("%c",((struct qpps_data_val_ind*)param)->data[k]);
						QPRINTF("\r\n");
				}
					break;

        case QPPS_DISABLE_IND:
            break;

        case QPPS_CFG_INDNTF_IND:
            break;
				case PROXR_ALERT_IND:
            usr_proxr_alert((struct proxr_alert_ind*)param);
            break;

#if	(BLE_OTA_SERVER)						
        case OTAS_TRANSIMIT_STATUS_IND:
            QPRINTF(" APP get OTA transmit status = %d , describe = %d \r\n" , ((struct otas_transimit_status_ind*)param)->status,
                                                                              ((struct otas_transimit_status_ind*)param)->status_des);
            
            //only need response once when ota status is in ota status start request
            if(((struct otas_transimit_status_ind*)param)->status == OTA_STATUS_START_REQ)  
            {
                app_ota_ctrl_resp(START_OTA);
            }
            break;
//end
#endif

        default:
            break;
    }
}

/**
 ****************************************************************************************
 * @brief Handles LED status timer.
 *
 * @param[in] msgid      APP_SYS_UART_DATA_IND
 * @param[in] param      Pointer to struct app_uart_data_ind
 * @param[in] dest_id    TASK_APP
 * @param[in] src_id     TASK_APP
 *
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
int app_led_timer_handler(ke_msg_id_t const msgid, void const *param,
                               ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    if(msgid == APP_SYS_LED_1_TIMER)
    {
        usr_led1_process();
    }

    return (KE_MSG_CONSUMED);
}


/**
 ****************************************************************************************
 * @brief Handles LED status timer.
 *
 * @param[in] msgid      APP_SYS_UART_DATA_IND
 * @param[in] param      Pointer to struct app_uart_data_ind
 * @param[in] dest_id    TASK_APP
 * @param[in] src_id     TASK_APP
 *
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
int app_mpu6050_temperature_test_timer_handler(ke_msg_id_t const msgid, void const *param,
                               ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
		usr_env.mpu6050_data.temp_data = mpu6050_get_data(MPU6050_ADDR,TEMP_OUT_H);
		ke_timer_set(APP_MPU6050_ADD_READ_TIMER,TASK_APP,2);
		ke_timer_clear(APP_MPU6050_TEMPERATURE_TEST_TIMER,TASK_APP);

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief Handles LED status timer.
 *
 * @param[in] msgid      APP_SYS_UART_DATA_IND
 * @param[in] param      Pointer to struct app_uart_data_ind
 * @param[in] dest_id    TASK_APP
 * @param[in] src_id     TASK_APP
 *
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
int app_mpu6050_addr_read_test_timer_handler(ke_msg_id_t const msgid, void const *param,
                               ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
		usr_env.mpu6050_data.addr_data = mpu6050_get_data(MPU6050_ADDR,WHO_AM_I);
		//QPRINTF("0x%02X\r\n",usr_env.mpu6050_data.addr_data);
		ke_timer_clear(APP_MPU6050_ADD_READ_TIMER,TASK_APP);
#if	(FB_BIT)
		ke_timer_set(APP_TEST_DATA_SEND_TIMER,TASK_APP,2);
#endif

    return (KE_MSG_CONSUMED);
}
/**
 ****************************************************************************************
 * @brief Handles LED status timer.
 *
 * @param[in] msgid      APP_SYS_UART_DATA_IND
 * @param[in] param      Pointer to struct app_uart_data_ind
 * @param[in] dest_id    TASK_APP
 * @param[in] src_id     TASK_APP
 *
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
int app_test_data_send_timer_handler(ke_msg_id_t const msgid, void const *param,
                               ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
		uint8_t val[12];
//		test_data.time_flag = 'T';
//		test_data.time = usr_env.time;
//		test_data.mpu6050_flag = 'P';
//		test_data.mpu6050_addr_flag = 'A';
//		test_data.mpu6050_addr = usr_env.mpu6050_data.addr_data;
//		test_data.mpu6050_temp_flag = 'T';
//		test_data.mpu6050_temp = usr_env.mpu6050_data.temp_data;
//		memcpy(val,&test_data,sizeof(struct test_data_tag));
//		QPRINTF("sizeof is %d",sizeof(struct test_data_tag));
		val[0] = 'T';
		val[1] = usr_env.time >> 24;
		val[2] = usr_env.time >> 16;
		val[3] = usr_env.time >> 8;
		val[4] = usr_env.time & 0x000000ff;
		val[5] = 'P';
		val[6] = 'A';
		val[7] = usr_env.mpu6050_data.addr_data >> 8;
		val[8] = usr_env.mpu6050_data.addr_data & 0x00ff;		
		val[9] = 'T';
		val[10] = usr_env.mpu6050_data.temp_data >> 8;
		val[11] = usr_env.mpu6050_data.temp_data & 0x00ff;	
		//QPRINTF("0x%X\r\n",test_data.mpu6050_addr);
		app_qpps_data_send(app_qpps_env->conhdl,0,12,val);
    return (KE_MSG_CONSUMED);
}


/**
 ****************************************************************************************
 * @brief Handles LED status timer.
 *
 * @param[in] msgid      APP_SYS_UART_DATA_IND
 * @param[in] param      Pointer to struct app_uart_data_ind
 * @param[in] dest_id    TASK_APP
 * @param[in] src_id     TASK_APP
 *
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
int app_test_passed_timer_handler(ke_msg_id_t const msgid, void const *param,
                               ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
		led_set(2,LED_ON);
		led_set(3,LED_ON);
		usr_env.test_flag  = 1;
		app_qpps_data_send(app_qpps_env->conhdl,0,12,(uint8_t *)"Test passed!");
    return (KE_MSG_CONSUMED);
}

#if	(FB_BIT)
/**
 ****************************************************************************************
 * @brief Handles LED status timer.
 *
 * @param[in] msgid      APP_SYS_UART_DATA_IND
 * @param[in] param      Pointer to struct app_uart_data_ind
 * @param[in] dest_id    TASK_APP
 * @param[in] src_id     TASK_APP
 *
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
int app_test_error_timer_handler(ke_msg_id_t const msgid, void const *param,
                               ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
		switch(usr_env.test_result)
		{
			case	0x30+0x01:
			{
					buzzer_off();
					ke_timer_clear(APP_TEST_SYS_LED_TIMER,TASK_APP);
					led_set(2,LED_ON);
					led_set(3,LED_OFF);
			}break;
			case	0x30+0x02:
			{
					buzzer_off();
					ke_timer_clear(APP_TEST_SYS_LED_TIMER,TASK_APP);
					led_set(2,LED_OFF);
					led_set(3,LED_ON);					
			}break;
			case	0x30+0x04:
			{
					buzzer_on(BUZZ_VOL_HIGH);
					ke_timer_clear(APP_TEST_SYS_LED_TIMER,TASK_APP);
					led_set(2,LED_OFF);
					led_set(3,LED_OFF);					
			}break;
			case	0x30+0x03:
			{
					buzzer_off();
					led_set(2,LED_ON);
					led_set(3,LED_OFF);
					ke_timer_set(APP_TEST_SYS_LED_TIMER, TASK_APP,100);
			}break;
			case	0x30+0x05:
			{
					buzzer_off();
					ke_timer_clear(APP_TEST_SYS_LED_TIMER,TASK_APP);
					led_set(2,LED_ON);
					led_set(3,LED_OFF);
					buzzer_on(BUZZ_VOL_HIGH);
			}break;
			case	0x30+0x06:
			{
					buzzer_off();
					ke_timer_clear(APP_TEST_SYS_LED_TIMER,TASK_APP);
					led_set(2,LED_OFF);
					led_set(3,LED_ON);
					buzzer_on(BUZZ_VOL_HIGH);
			}break;
			case	0x30+0x07:
			{
					led_set(2,LED_ON);
					led_set(3,LED_OFF);
					ke_timer_set(APP_TEST_SYS_LED_TIMER, TASK_APP,100);
					buzzer_on(BUZZ_VOL_HIGH);
			}break;
			
			default	:
			{
					led_set(2,LED_OFF);
					led_set(3,LED_OFF);
					buzzer_off();
					ke_timer_clear(APP_TEST_SYS_LED_TIMER,TASK_APP);
			}
			break;
		}
		usr_env.test_flag  = 1;
		app_qpps_data_send(app_qpps_env->conhdl,0,11,(uint8_t *)"Test error!");
    return (KE_MSG_CONSUMED);
}
#endif

/**
 ****************************************************************************************
 * @brief Handles advertising mode timer event.
 *
 * @param[in] msgid     APP_ADV_INTV_UPDATE_TIMER
 * @param[in] param     None
 * @param[in] dest_id   TASK_APP
 * @param[in] src_id    TASK_APP
 *
 * @return If the message was consumed or not.
 * @description
 *
 * This handler is used to inform the application that first phase of adversting mode is timeout.
 ****************************************************************************************
 */
int app_gap_adv_intv_update_timer_handler(ke_msg_id_t const msgid, void const *param,
                                          ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    if(APP_ADV == ke_state_get(TASK_APP))
    {
        usr_led1_set(LED_ON_DUR_IDLE, LED_OFF_DUR_IDLE);

        // Update Advertising Parameters
        app_gap_adv_start_req(GAP_GEN_DISCOVERABLE|GAP_UND_CONNECTABLE, 
                                app_env.adv_data, app_set_adv_data(GAP_GEN_DISCOVERABLE), 
                                app_env.scanrsp_data, app_set_scan_rsp_data(app_get_local_service_flag()),
                                GAP_ADV_SLOW_INTV, GAP_ADV_SLOW_INTV);
    }

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief   Restore peripheral setting after wakeup
 ****************************************************************************************
 */
void usr_sleep_restore(void)
{
#if 	(CONFIG_ENABLE_DRIVER_MPU6050 == TRUE)
		i2c_init(I2C_SCL_RATIO(40000), usr_env.i2cbuffer, 104);
#endif
#if	(FB_BIT)
		timer_init(QN_TIMER0, timer0_callback);
#endif
#if QN_DBG_PRINT
    uart_init(QN_DEBUG_UART, USARTx_CLK(0), UART_9600);
    uart_tx_enable(QN_DEBUG_UART, MASK_ENABLE);
    uart_rx_enable(QN_DEBUG_UART, MASK_ENABLE);
#endif

#if (defined(QN_ADV_WDT))
    if(usr_env.adv_wdt_enable)
    {
        wdt_init(1007616, WDT_INT_MOD); // 30.75s
    }
#endif
}

///**
// ****************************************************************************************
// * @brief Handles LED status timer.
// *
// * @param[in] msgid      APP_SYS_UART_DATA_IND
// * @param[in] param      Pointer to struct app_uart_data_ind
// * @param[in] dest_id    TASK_APP
// * @param[in] src_id     TASK_APP
// *
// * @return If the message was consumed or not.
// ****************************************************************************************
// */
//int app_led_timer_handler(ke_msg_id_t const msgid, void const *param,
//                               ke_task_id_t const dest_id, ke_task_id_t const src_id)
//{
//    if(msgid == APP_SYS_LED_1_TIMER)
//    {
//        usr_led1_process();
//    }

//    return (KE_MSG_CONSUMED);
//}


/**
 ****************************************************************************************
 * @brief Handles alert stop timer.
 *
 * @param[in] msgid     APP_PROXR_ALERT_STOP_TO
 * @param[in] param     Null
 * @param[in] dest_id   TASK_APP
 * @param[in] src_id    TASK_NONE
 *
 * @return If the message was consumed or not.
 * @description
 * This handler is used to request the Application to start the alert on the device
 * considering the indicated alert level. The handler may be triggered on two conditions:
 * The IAS alert level characteristic has been written to a valid value, in which case
 * alert_lvl will be set to the IAS alert level value.
 * A disconnection with a reason other than the normal local/remote link terminations has
 * been received, in which case alert_lvl will be set to the LLS alert level value.
 * The Application actions following reception of this indication are strictly implementation
 * specific (it may try to reconnect to the peer and stop alert upon that, or timeout the
 * alert after acertain time, please see the specification)
 ****************************************************************************************
 */
int app_proxr_alert_to_handler(ke_msg_id_t const msgid,
                               void *param,
                               ke_task_id_t const dest_id,
                               ke_task_id_t const src_id)
{
    // Stop proxr alert
    buzzer_off();

    //app_proxr_env->alert_lvl = 0;
    QPRINTF("alert_stop_timer_handler.\r\n");
    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief Handles button press after cancel the jitter.
 *
 * @param[in] msgid     Id of the message received
 * @param[in] param     None
 * @param[in] dest_id   TASK_APP
 * @param[in] src_id    TASK_APP
 *
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
int app_button_timer_handler(ke_msg_id_t const msgid, void const *param,
                               ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    switch(msgid)
    {
        case APP_SYS_BUTTON_1_TIMER:
            // make sure the button is pressed
            if(gpio_read_pin(BUTTON1_PIN) == GPIO_LOW)
            {
                if(APP_IDLE == ke_state_get(TASK_APP))
                {
                    if(!app_qpps_env->enabled)
                    {
                        // start adv
                        app_gap_adv_start_req(GAP_GEN_DISCOVERABLE|GAP_UND_CONNECTABLE,
                                app_env.adv_data, app_set_adv_data(GAP_GEN_DISCOVERABLE),
                                app_env.scanrsp_data, app_set_scan_rsp_data(app_get_local_service_flag()),
                                GAP_ADV_FAST_INTV1, GAP_ADV_FAST_INTV2);

#if (QN_DEEP_SLEEP_EN)
                        // prevent entering into deep sleep mode
                        sleep_set_pm(PM_SLEEP);
#endif
                    }
                }
                else if(APP_ADV == ke_state_get(TASK_APP))
                {
                    // stop adv
                    app_gap_adv_stop_req();

#if (QN_DEEP_SLEEP_EN)
                    // allow entering into deep sleep mode
                    sleep_set_pm(PM_DEEP_SLEEP);
#endif
                }
            }
            break;

        default:
            ASSERT_ERR(0);
            break;
    }

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief Handles button press before key debounce.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
void app_event_button1_press_handler(void)
{
#if ((QN_DEEP_SLEEP_EN) && (!QN_32K_RCO))
    if (sleep_env.deep_sleep)
    {
        sleep_env.deep_sleep = false;
        // start 32k xtal wakeup timer
        wakeup_32k_xtal_start_timer();
    }
#endif

    // delay 20ms to debounce
#if (FB_JOYSTICKS)
    ke_timer_set(APP_KEY_SCAN_TIMER,TASK_APP,2);
#else
    ke_timer_set(APP_SYS_BUTTON_1_TIMER, TASK_APP, 2);
#endif
    ke_evt_clear(1UL << EVENT_BUTTON1_PRESS_ID);
}

/**
 ****************************************************************************************
 * @brief   Button 1 click callback
 * @description
 *  Button 1 is used to enter adv mode.
 ****************************************************************************************
 */
void usr_button1_cb(void)
{
    // If BLE is in the sleep mode, wakeup it.
    if(ble_ext_wakeup_allow())
    {
#if ((QN_DEEP_SLEEP_EN) && (!QN_32K_RCO))
        if (sleep_env.deep_sleep)
        {
            wakeup_32k_xtal_switch_clk();
        }
#endif

        sw_wakeup_ble_hw();

    }

#if (FB_JOYSTICKS)
    usr_button_env.button_st = button_press;
#endif
    // key debounce:
    // We can set a soft timer to debounce.
    // After wakeup BLE, the timer is not calibrated immediately and it is not precise.
    // So We set a event, in the event handle, set the soft timer.
    ke_evt_set(1UL << EVENT_BUTTON1_PRESS_ID);
}

void timer0_callback(void)
{
		usr_env.time++;
}

/**
 ****************************************************************************************
 * @brief   All GPIO interrupt callback
 ****************************************************************************************
 */
void gpio_interrupt_callback(enum gpio_pin pin)
{
    switch(pin)
    {
        case BUTTON1_PIN:
            //Button 1 is used to enter adv mode.
            usr_button1_cb();
            break;

#if (defined(QN_TEST_CTRL_PIN))
        case QN_TEST_CTRL_PIN:
            //When test controll pin is changed to low level, this function will reboot system.
            gpio_disable_interrupt(QN_TEST_CTRL_PIN);
            syscon_SetCRSS(QN_SYSCON, SYSCON_MASK_REBOOT_SYS);
            break;
#endif

        default:
            break;
    }
}


/**
 ****************************************************************************************
 * @brief   User initialize
 ****************************************************************************************
 */
void usr_init(void)
{
		usr_env.usr_discon.nb = 0;
		usr_env.usr_discon.reason[0] = 'R';
#if 	(CONFIG_ENABLE_DRIVER_MPU6050 == TRUE)
		mpu6050_init();
#endif
#if	(FB_BIT)
		usr_env.time = 0;
    timer_config(QN_TIMER0, TIMER_PSCAL_DIV, TIMER_COUNT_MS(1000, TIMER_PSCAL_DIV));
    timer_enable(QN_TIMER0, MASK_DISABLE);
#endif	

    if(KE_EVENT_OK != ke_evt_callback_set(EVENT_BUTTON1_PRESS_ID, 
                                            app_event_button1_press_handler))
    {
        ASSERT_ERR(0);
    }
#if 	(FB_JOYSTICKS)
		if(KE_EVENT_OK != ke_evt_callback_set(EVENT_ADC_KEY_SAMPLE_CMP_ID,
                                            app_event_adc_key_sample_cmp_handler))
    {
        ASSERT_ERR(0);
    }
#endif
}

/// @} USR

