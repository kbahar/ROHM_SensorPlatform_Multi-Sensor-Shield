/* mbed Microcontroller Library
 * Copyright (c) 2006-2013 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
 
 /*
 * Code Example for ROHM Mutli-Sensor Shield on the Nordic Semiconductor nRF51-DK
 * 
 *  Added Functions for interfacing with ROHM's Multi-Sensor Shield Board
 *  Supports the following Sensor Devices
 *      > BDE0600G Temperature Sensor
 *      > BM1383GLV Pressure Sensor
 *      > BU52014 Hall Sensor
 *      > ML8511 UV Sensor
 *      > RPR-0521 ALS/PROX Sensor
 *      > BH1745NUC Color Sensor
 *      > KMX62 Accel/Mag Sensor
 *      > KX122 Accel Sensor
 *      > KXG03 Gyro (Currently Unavailable as IC hasn't docked yet)
 *
 *  Last Upadtaed: 9/11/15
 *
 *  New Code: 
 *      Added a Section in "Main" to act as initialization
 *      Added to the "Periodic Callback" to read sensor data and return to Phone/Host
 *  
 *  Additional information about the ROHM MultiSensor Shield Board can be found at the following link:
 *      https://github.com/ROHMUSDC/ROHM_SensorPlatform_Multi-Sensor-Shield  
 */ 
 
#define AnalogTemp  //BDE0600
#define AnalogUV    //ML8511
#define HallSensor  //BU52011
#define RPR0521     //RPR0521
#define KMX62                
#define color
#define KX022
#define Pressure

#include "mbed.h"
#include "BLEDevice.h"
#include "UARTService.h"
#include "nrf_temp.h"
#include "I2C.h"

#define MAX_REPLY_LEN           (UARTService::BLE_UART_SERVICE_MAX_DATA_LEN)    //Actually equal to 20
#define SENSOR_READ_INTERVAL_S  (5.0F) 
#define ADV_INTERVAL_MS         (1000UL)
#define UART_BAUD_RATE          (19200UL)
#define DEVICE_NAME             ("DEMO SENSOR") // This can be read AFTER connecting to the device.
#define SHORT_NAME              ("ROHMKRIS")    // Keep this short: max 8 chars if a 128bit UUID is also advertised.

#define DEBUG(...)              { m_serial_port.printf(__VA_ARGS__); }

// Function Prototypes
void PBTrigger();   //Interrupt function for PB4

// Global Variables
BLEDevice   m_ble;
Serial      m_serial_port(p9, p11);  // TX pin, RX pin
DigitalOut  m_cmd_led(LED1);
DigitalOut  m_error_led(LED2);
UARTService *m_uart_service_ptr;
DigitalIn   testButton(p20);
InterruptIn sw4Press(p20);
I2C         i2c(p30,p7);

//Sensor Variables
/*
AnalogIn    BH1620_ALS(p1);     //No Analog ALS on the shield
uint16_t    BH1620_ALS_value;
float       BH1620_output;
*/

AnalogIn    BDE0600_Temp(p3);   //p2 on the prior evk, p3 on the shield
uint16_t    BDE0600_Temp_value;
float       BDE0600_output;

AnalogIn    ML8511_UV(p5);      //p3 on prior EVK, p5 on the shield
uint16_t    ML8511_UV_value;
float       ML8511_output;

DigitalIn   Hall_GPIO0(p14);    //
DigitalIn   Hall_GPIO1(p15);    //
int         Hall_Return1;
int         Hall_Return0;

bool        RepStart = true;
bool        NoRepStart = false;

int i=1;


#ifdef RPR0521     //als digital
int         RPR0521_addr_w = 0x70;          //7bit addr = 0x38, with write bit 0
int         RPR0521_addr_r = 0x71;          //7bit addr = 0x38, with read bit 1

char        RPR0521_ModeControl[2] = {0x41, 0xE6};  
char        RPR0521_ALSPSControl[2] = {0x42, 0x03};
char        RPR0521_Persist[2] = {0x43, 0x20};
char        RPR0521_Addr_ReadData = 0x44;
char        RPR0521_Content_ReadData[6];

int         RPR0521_PS_RAWOUT = 0;
float       RPR0521_PS_OUT = 0;
int         RPR0521_ALS_D0_RAWOUT = 0;
int         RPR0521_ALS_D1_RAWOUT = 0;
float       RPR0521_ALS_DataRatio = 0;
float       RPR0521_ALS_OUT = 0;
#endif

#ifdef KMX62
int         KMX62_addr_w = 0x1C;          //7bit addr = 0x38, with write bit 0
int         KMX62_addr_r = 0x1D;          //7bit addr = 0x38, with read bit 1

char        KMX62_CNTL2[2] = {0x3A, 0x5F};
char        KMX62_Addr_Accel_ReadData = 0x0A;
char        KMX62_Content_Accel_ReadData[6];
char        KMX62_Addr_Mag_ReadData = 0x10;
char        KMX62_Content_Mag_ReadData[6];

short int         MEMS_Accel_Xout = 0;
short int         MEMS_Accel_Yout = 0;
short int         MEMS_Accel_Zout = 0;
double       MEMS_Accel_Conv_Xout = 0;
double       MEMS_Accel_Conv_Yout = 0;
double       MEMS_Accel_Conv_Zout = 0;
short int         MEMS_Mag_Xout = 0;
short int         MEMS_Mag_Yout = 0;
short int         MEMS_Mag_Zout = 0;
float       MEMS_Mag_Conv_Xout = 0;
float       MEMS_Mag_Conv_Yout = 0;
float       MEMS_Mag_Conv_Zout = 0;
#endif

#ifdef color
int         BH1745_addr_w = 0x72;   //write 
int         BH1745_addr_r = 0x73;   //read 

char        BH1745_persistence[2] = {0x61, 0x03};
char        BH1745_mode1[2] = {0x41, 0x00};
char        BH1745_mode2[2] = {0x42, 0x92};
char        BH1745_mode3[2] = {0x43, 0x02};

char        BH1745_Content_ReadData[6];
char        BH1745_Addr_color_ReadData = 0x50;

int        BH1745_Red;
int        BH1745_Blue;
int        BH1745_Green;

#endif

#ifdef KX022
int         KX022_addr_w = 0x3C;   //write 
int         KX022_addr_r = 0x3D;   //read 

char        KX022_Accel_CNTL1[2] = {0x18, 0x41};
char        KX022_Accel_ODCNTL[2] = {0x1B, 0x02};
char        KX022_Accel_CNTL3[2] = {0x1A, 0xD8};
char        KX022_Accel_TILT_TIMER[2] = {0x22, 0x01};
char        KX022_Accel_CNTL2[2] = {0x18, 0xC1};

char        KX022_Content_ReadData[6];
char        KX022_Addr_Accel_ReadData = 0x06;           

float        KX022_Accel_X;
float        KX022_Accel_Y;                               
float        KX022_Accel_Z;

short int KX022_Accel_X_RawOUT = 0;
short int KX022_Accel_Y_RawOUT = 0;
short int KX022_Accel_Z_RawOUT = 0;

int KX022_Accel_X_LB = 0;
int KX022_Accel_X_HB = 0;
int KX022_Accel_Y_LB = 0;
int KX022_Accel_Y_HB = 0;
int KX022_Accel_Z_LB = 0;
int KX022_Accel_Z_HB = 0;

#endif

#ifdef Pressure
int         Press_addr_w = 0xBA;   //write 
int         Press_addr_r = 0xBB;   //read 

char        PWR_DOWN[2] = {0x12, 0x01};
char        SLEEP[2] = {0x13, 0x01};
char        Mode_Control[2] = {0x14, 0xC4};

char        Press_Content_ReadData[6];
char        Press_Addr_ReadData =0x1A;

int         BM1383_Temp_highByte;
int         BM1383_Temp_lowByte;
int         BM1383_Pres_highByte;
int         BM1383_Pres_lowByte;
int         BM1383_Pres_leastByte; 

float       BM1383_Temp_Out;
float       BM1383_Temp_Conv_Out;
float       BM1383_Pres_Conv_Out;

float       BM1383_Var;
float       BM1383_Deci;
#endif

/**
 * This callback is used whenever a disconnection occurs.
 */
void disconnectionCallback(Gap::Handle_t handle, Gap::DisconnectionReason_t reason)
{
    switch (reason) {
    case Gap::REMOTE_USER_TERMINATED_CONNECTION:
        DEBUG("Disconnected (REMOTE_USER_TERMINATED_CONNECTION)\n\r");
        break;
    case Gap::LOCAL_HOST_TERMINATED_CONNECTION:
        DEBUG("Disconnected (LOCAL_HOST_TERMINATED_CONNECTION)\n\r");
        break;
    case Gap::CONN_INTERVAL_UNACCEPTABLE:
        DEBUG("Disconnected (CONN_INTERVAL_UNACCEPTABLE)\n\r");
        break;
    }

    DEBUG("Restarting the advertising process\n\r");
    m_ble.startAdvertising();
}


/**
 * This callback is used whenever the host writes data to one of our GATT characteristics.
 */
void dataWrittenCallback(const GattCharacteristicWriteCBParams *params)
{
    // Ensure that initialization is finished and the host has written to the TX characteristic.
    if ((m_uart_service_ptr != NULL) && (params->charHandle == m_uart_service_ptr->getTXCharacteristicHandle())) {
        uint8_t  buf[MAX_REPLY_LEN];
        uint32_t len = 0;

        if (1 == params->len) {
            switch (params->data[0]) {
            case '0':
                m_cmd_led = m_cmd_led ^ 1;
                len = snprintf((char*) buf, MAX_REPLY_LEN, "OK... LED ON");
                break;
            case '1':
                m_cmd_led = m_cmd_led ^ 1;
                len = snprintf((char*) buf, MAX_REPLY_LEN, "OK... LED OFF");
                break;
            case 'a':
                //len = snprintf((char*) buf, MAX_REPLY_LEN, "ALSRaw = %d", BH1620_ALS_value);
                break;
            case 'b':
                //len = snprintf((char*) buf, MAX_REPLY_LEN, "ALS = %.2f lx", BH1620_output);
                break;
            default:
                len = snprintf((char*) buf, MAX_REPLY_LEN, "ERROR");
                break;
            }
        }
        else
        {
            len = snprintf((char*) buf, MAX_REPLY_LEN, "ERROR");
        }

        m_ble.updateCharacteristicValue(m_uart_service_ptr->getRXCharacteristicHandle(), buf, len);

        DEBUG("%d bytes received from host\n\r", params->len);
    }
}


/**
 * This callback is used whenever a write to a GATT characteristic causes data to be sent to the host.
 */
void dataSentCallback(unsigned count)
{
    // NOTE: The count always seems to be 1 regardless of data.
    DEBUG("%d bytes sent to host\n\r", count);
}


/**
 * This callback is scheduled to be called periodically via a low-priority interrupt.
 */
void periodicCallback(void)
{
    uint8_t  buf[MAX_REPLY_LEN];
    uint32_t len = 0;
    

if(i==1)
{
/*
#ifdef AnalogALS
    if (m_ble.getGapState().connected) {
        BH1620_ALS_value = BH1620_ALS.read_u16();
        BH1620_output = (float)BH1620_ALS_value * 1.543;
        
        len = snprintf((char*) buf, MAX_REPLY_LEN, "Analog ALS = %.2f lx", BH1620_output);
        m_ble.updateCharacteristicValue(m_uart_service_ptr->getRXCharacteristicHandle(), buf, len);
    }
#endif
*/

#ifdef color
if (m_ble.getGapState().connected) {
        //Read color Portion from the IC
        i2c.write(BH1745_addr_w, &BH1745_Addr_color_ReadData, 1, RepStart);
        i2c.read(BH1745_addr_r, &BH1745_Content_ReadData[0], 6, NoRepStart);
        
        //separate all data read into colors
        
        BH1745_Red = (BH1745_Content_ReadData[1]<<8) | (BH1745_Content_ReadData[0]);
        BH1745_Green = (BH1745_Content_ReadData[3]<<8) | (BH1745_Content_ReadData[2]);
        BH1745_Blue = (BH1745_Content_ReadData[5]<<8) | (BH1745_Content_ReadData[4]);
    
        
        //transmit data 
        len = snprintf((char*) buf, MAX_REPLY_LEN, "Red= %d", BH1745_Red);
        m_ble.updateCharacteristicValue(m_uart_service_ptr->getRXCharacteristicHandle(), buf, len);
         wait_ms(25);
       
        
        len = snprintf((char*) buf, MAX_REPLY_LEN, "Green= %d", BH1745_Green);
        m_ble.updateCharacteristicValue(m_uart_service_ptr->getRXCharacteristicHandle(), buf, len);
        wait_ms(25);
        
        len = snprintf((char*) buf, MAX_REPLY_LEN, "Blue= %d", BH1745_Blue);
        m_ble.updateCharacteristicValue(m_uart_service_ptr->getRXCharacteristicHandle(), buf, len);
        wait_ms(25);   
                     
}
#endif

#ifdef AnalogTemp
    if (m_ble.getGapState().connected) {
        BDE0600_Temp_value = BDE0600_Temp.read_u16();
        BDE0600_output = (float)BDE0600_Temp_value * 0.00283; //(value * (2.9V/1024))
        BDE0600_output = (BDE0600_output-1.753)/(-0.01068) + 30;
        
        len = snprintf((char*) buf, MAX_REPLY_LEN, "Temp = %.2f C", BDE0600_output);
        m_ble.updateCharacteristicValue(m_uart_service_ptr->getRXCharacteristicHandle(), buf, len);
    }
#endif

#ifdef AnalogUV
    if (m_ble.getGapState().connected) {
        ML8511_UV_value = ML8511_UV.read_u16();
        ML8511_output = (float)ML8511_UV_value * 0.0029; //(value * (2.9V/1024))   //Note to self: when playing with this, a negative value is seen... Honestly, I think this has to do with my ADC converstion...
        ML8511_output = (ML8511_output-2.2)/(0.129) + 10;                           // Added +5 to the offset so when inside (aka, no UV, readings show 0)... this is the wrong approach... and the readings don't make sense... Fix this.
        
        len = snprintf((char*) buf, MAX_REPLY_LEN, "UV = %.1f mW/cm2", ML8511_output);
        m_ble.updateCharacteristicValue(m_uart_service_ptr->getRXCharacteristicHandle(), buf, len);
    }
#endif

#ifdef HallSensor
    if (m_ble.getGapState().connected) {
        Hall_Return0 = Hall_GPIO0;
        Hall_Return1 = Hall_GPIO1;
        
        len = snprintf((char*) buf, MAX_REPLY_LEN, "H0 = %d, H1 = %d", Hall_Return0, Hall_Return1);
        m_ble.updateCharacteristicValue(m_uart_service_ptr->getRXCharacteristicHandle(), buf, len);
    }
#endif

#ifdef DigitalALS	//This shouldn't be here...
    if (m_ble.getGapState().connected) {
        i2c.read(ALS_addr_r, ALS_ReturnData_raw, 2);
        ALS_Return = (ALS_ReturnData_raw[0]<<8) | ALS_ReturnData_raw[1];
        ALS_Return = ALS_Return/1.2;
        
        len = snprintf((char*) buf, MAX_REPLY_LEN, "DAL1= %0.2f lx", ALS_Return);
        m_ble.updateCharacteristicValue(m_uart_service_ptr->getRXCharacteristicHandle(), buf, len);
    }
#endif

#ifdef RPR0521       //als digital
    if (m_ble.getGapState().connected) {
        
        i2c.write(RPR0521_addr_w, &RPR0521_Addr_ReadData, 1, RepStart);
        i2c.read(RPR0521_addr_r, &RPR0521_Content_ReadData[0], 6, NoRepStart);
        
        RPR0521_PS_RAWOUT = (RPR0521_Content_ReadData[1]<<8) | (RPR0521_Content_ReadData[0]);
        RPR0521_ALS_D0_RAWOUT = (RPR0521_Content_ReadData[3]<<8) | (RPR0521_Content_ReadData[2]);
        RPR0521_ALS_D1_RAWOUT = (RPR0521_Content_ReadData[5]<<8) | (RPR0521_Content_ReadData[4]);
        RPR0521_ALS_DataRatio = (float)RPR0521_ALS_D1_RAWOUT / (float)RPR0521_ALS_D0_RAWOUT;
         
        if(RPR0521_ALS_DataRatio < 0.595){
            RPR0521_ALS_OUT = (1.682*(float)RPR0521_ALS_D0_RAWOUT - 1.877*(float)RPR0521_ALS_D1_RAWOUT);
        }
        else if(RPR0521_ALS_DataRatio < 1.015){
            RPR0521_ALS_OUT = (0.644*(float)RPR0521_ALS_D0_RAWOUT - 0.132*(float)RPR0521_ALS_D1_RAWOUT);
        }
        else if(RPR0521_ALS_DataRatio < 1.352){
            RPR0521_ALS_OUT = (0.756*(float)RPR0521_ALS_D0_RAWOUT - 0.243*(float)RPR0521_ALS_D1_RAWOUT);
        }
        else if(RPR0521_ALS_DataRatio < 3.053){
            RPR0521_ALS_OUT = (0.766*(float)RPR0521_ALS_D0_RAWOUT - 0.25*(float)RPR0521_ALS_D1_RAWOUT);
        }
        else{
            RPR0521_ALS_OUT = 0;
        }
        
        len = snprintf((char*) buf, MAX_REPLY_LEN, "DALS= %0.2f lx", RPR0521_ALS_OUT);
        m_ble.updateCharacteristicValue(m_uart_service_ptr->getRXCharacteristicHandle(), buf, len);
       
    }
#endif

i++;

}

else if(i==2)
{

#ifdef KMX62
    if (m_ble.getGapState().connected) {
        //Read Accel Portion from the IC
        i2c.write(KMX62_addr_w, &KMX62_Addr_Accel_ReadData, 1, RepStart);
        i2c.read(KMX62_addr_r, &KMX62_Content_Accel_ReadData[0], 6, NoRepStart);

        //Note: The highbyte and low byte return a 14bit value, dropping the two LSB in the Low byte.
        //      However, because we need the signed value, we will adjust the value when converting to "g"
        MEMS_Accel_Xout = (KMX62_Content_Accel_ReadData[1]<<8) | (KMX62_Content_Accel_ReadData[0]);
        MEMS_Accel_Yout = (KMX62_Content_Accel_ReadData[3]<<8) | (KMX62_Content_Accel_ReadData[2]);
        MEMS_Accel_Zout = (KMX62_Content_Accel_ReadData[5]<<8) | (KMX62_Content_Accel_ReadData[4]);
          
        //Note: Conversion to G is as follows:
        //      Axis_ValueInG = MEMS_Accel_axis / 1024
        //      However, since we did not remove the LSB previously, we need to divide by 4 again
        //      Thus, we will divide the output by 4096 (1024*4) to convert and cancel out the LSB
        MEMS_Accel_Conv_Xout = ((float)MEMS_Accel_Xout/4096/2);
        MEMS_Accel_Conv_Yout = ((float)MEMS_Accel_Yout/4096/2);
        MEMS_Accel_Conv_Zout = ((float)MEMS_Accel_Zout/4096/2);

        //Read MAg portion from the IC
        i2c.write(KMX62_addr_w, &KMX62_Addr_Mag_ReadData, 1, RepStart);
        i2c.read(KMX62_addr_r, &KMX62_Content_Mag_ReadData[0], 6, NoRepStart);

        //Note: The highbyte and low byte return a 14bit value, dropping the two LSB in the Low byte.
        //      However, because we need the signed value, we will adjust the value when converting to "g"
        MEMS_Mag_Xout = (KMX62_Content_Mag_ReadData[1]<<8) | (KMX62_Content_Mag_ReadData[0]);
        MEMS_Mag_Yout = (KMX62_Content_Mag_ReadData[3]<<8) | (KMX62_Content_Mag_ReadData[2]);
        MEMS_Mag_Zout = (KMX62_Content_Mag_ReadData[5]<<8) | (KMX62_Content_Mag_ReadData[4]);
        
        //Note: Conversion to G is as follows:
        //      Axis_ValueInG = MEMS_Accel_axis / 1024
        //      However, since we did not remove the LSB previously, we need to divide by 4 again
        //      Thus, we will divide the output by 4095 (1024*4) to convert and cancel out the LSB
        MEMS_Mag_Conv_Xout = (float)MEMS_Mag_Xout/4096*0.146;
        MEMS_Mag_Conv_Yout = (float)MEMS_Mag_Yout/4096*0.146;
        MEMS_Mag_Conv_Zout = (float)MEMS_Mag_Zout/4096*0.146; 

        // transmit data
        

        len = snprintf((char*) buf, MAX_REPLY_LEN, "KMX61SensorData:");
        m_ble.updateCharacteristicValue(m_uart_service_ptr->getRXCharacteristicHandle(), buf, len);
        wait_ms(20);

        len = snprintf((char*) buf, MAX_REPLY_LEN, " AccX= %0.2f g", MEMS_Accel_Conv_Xout);
        m_ble.updateCharacteristicValue(m_uart_service_ptr->getRXCharacteristicHandle(), buf, len);
        wait_ms(20);
        
        len = snprintf((char*) buf, MAX_REPLY_LEN, " AccY= %0.2f g", MEMS_Accel_Conv_Yout);
        m_ble.updateCharacteristicValue(m_uart_service_ptr->getRXCharacteristicHandle(), buf, len);
        wait_ms(20);
        
        len = snprintf((char*) buf, MAX_REPLY_LEN, " AccZ= %0.2f g", MEMS_Accel_Conv_Zout);
        m_ble.updateCharacteristicValue(m_uart_service_ptr->getRXCharacteristicHandle(), buf, len);
        wait_ms(20);
        
        len = snprintf((char*) buf, MAX_REPLY_LEN, " MagX= %0.2f g", MEMS_Mag_Conv_Xout);
        m_ble.updateCharacteristicValue(m_uart_service_ptr->getRXCharacteristicHandle(), buf, len);
        wait_ms(20);
        
        len = snprintf((char*) buf, MAX_REPLY_LEN, " MagY= %0.2f g", MEMS_Mag_Conv_Yout);
        m_ble.updateCharacteristicValue(m_uart_service_ptr->getRXCharacteristicHandle(), buf, len);
        wait_ms(20);
        
        len = snprintf((char*) buf, MAX_REPLY_LEN, " MagZ= %0.2f g", MEMS_Mag_Conv_Zout);
        m_ble.updateCharacteristicValue(m_uart_service_ptr->getRXCharacteristicHandle(), buf, len);
        wait_ms(20); 
        
    }
#endif

i++;

}


else if(i==3)
{



#ifdef KX022
if (m_ble.getGapState().connected) {
        //Read KX022 Portion from the IC
        i2c.write(KX022_addr_w, &KX022_Addr_Accel_ReadData, 1, RepStart);
        i2c.read(KX022_addr_r, &KX022_Content_ReadData[0], 6, NoRepStart);
        
            
        //reconfigure the data (taken from arduino code)
        KX022_Accel_X_RawOUT = (KX022_Content_ReadData[1]<<8) | (KX022_Content_ReadData[0]);
        KX022_Accel_Y_RawOUT = (KX022_Content_ReadData[3]<<8) | (KX022_Content_ReadData[2]);
        KX022_Accel_Z_RawOUT = (KX022_Content_ReadData[5]<<8) | (KX022_Content_ReadData[4]);       

        //apply needed changes (taken from arduino code)
        KX022_Accel_X = (float)KX022_Accel_X_RawOUT / 16384;
        KX022_Accel_Y = (float)KX022_Accel_Y_RawOUT / 16384;
        KX022_Accel_Z = (float)KX022_Accel_Z_RawOUT / 16384;
        
        
        
        //transmit the data
        len = snprintf((char*) buf, MAX_REPLY_LEN, "KX022-X= %0.2f", KX022_Accel_X);
        m_ble.updateCharacteristicValue(m_uart_service_ptr->getRXCharacteristicHandle(), buf, len);
        wait_ms(25);
        
        len = snprintf((char*) buf, MAX_REPLY_LEN, "KX022-Y= %0.2f", KX022_Accel_Y);
        m_ble.updateCharacteristicValue(m_uart_service_ptr->getRXCharacteristicHandle(), buf, len);
        wait_ms(25);
        
        len = snprintf((char*) buf, MAX_REPLY_LEN, "KX022-Z= %0.2f", KX022_Accel_Z);
        m_ble.updateCharacteristicValue(m_uart_service_ptr->getRXCharacteristicHandle(), buf, len);
        wait_ms(25);
        
 }   
#endif

#ifdef Pressure
if (m_ble.getGapState().connected) {
        //Read color Portion from the IC
        i2c.write(Press_addr_w, &Press_Addr_ReadData, 1, RepStart);
        i2c.read(Press_addr_r, &Press_Content_ReadData[0], 6, NoRepStart);
        
       
        
        BM1383_Temp_Out = (Press_Content_ReadData[1]<<8) | (Press_Content_ReadData[0]);
        BM1383_Temp_Conv_Out = (float)BM1383_Temp_Out/32;
        
        BM1383_Var  = (Press_Content_ReadData[2]<<3) | (Press_Content_ReadData[3] >> 5);
        BM1383_Deci = ((Press_Content_ReadData[3] & 0x1f) << 6 | ((Press_Content_ReadData[4] >> 2)));
        BM1383_Deci = (float)BM1383_Deci* 0.00048828125;  //0.00048828125 = 2^-11
        BM1383_Pres_Conv_Out = (BM1383_Var + BM1383_Deci);   //question pending here...
        
         len = snprintf((char*) buf, MAX_REPLY_LEN, "Temp_out= %0.2f", BM1383_Temp_Out);
         m_ble.updateCharacteristicValue(m_uart_service_ptr->getRXCharacteristicHandle(), buf, len);
         wait_ms(25);
        
         len = snprintf((char*) buf, MAX_REPLY_LEN, "Temp_conv= %0.2f", BM1383_Temp_Conv_Out);
         m_ble.updateCharacteristicValue(m_uart_service_ptr->getRXCharacteristicHandle(), buf, len);
         wait_ms(25);
        
         len = snprintf((char*) buf, MAX_REPLY_LEN, "Press_conv= %0.2f", BM1383_Pres_Conv_Out);
         m_ble.updateCharacteristicValue(m_uart_service_ptr->getRXCharacteristicHandle(), buf, len);
         wait_ms(25);
}        
#endif  
      
i=1;

}



}
void error(ble_error_t err, uint32_t line)
{
    m_error_led = 1;
    DEBUG("Error %d on line number %d\n\r", err, line);
}

void PBTrigger()
{
    uint8_t  buf[MAX_REPLY_LEN];
    uint32_t len = 0;
    
    m_cmd_led = !m_cmd_led;
    
    
    if (m_ble.getGapState().connected) {
        /*
        BH1620_ALS_value = BH1620_ALS.read_u16();
        BH1620_output = (float)BH1620_ALS_value * 1.543;
        
        len = snprintf((char*) buf, MAX_REPLY_LEN, "ALS = %.2f lx", BH1620_output);
        m_ble.updateCharacteristicValue(m_uart_service_ptr->getRXCharacteristicHandle(), buf, len);
        */
    }
}

int main(void)
{
    ble_error_t err;
    Ticker      ticker;

    m_serial_port.baud(UART_BAUD_RATE);

    DEBUG("Initialising...\n\r");

    m_cmd_led      = 0;
    m_error_led    = 0;
    //BH1620_ALS_value    = 0;

    ticker.attach(periodicCallback, SENSOR_READ_INTERVAL_S);

    sw4Press.fall(&PBTrigger);

#ifdef RPR0521       //als digital
  // 1. Mode Control (0x41), write (0xC6): ALS EN, PS EN, 100ms measurement for ALS and PS, PS_PULSE=1
  // 2. ALS_PS_CONTROL (0x42), write (0x03): LED Current = 200mA
  // 3. PERSIST (0x43), write (0x20): PS Gain x4  
    i2c.write(RPR0521_addr_w, &RPR0521_ModeControl[0], 2, false);
    i2c.write(RPR0521_addr_w, &RPR0521_ALSPSControl[0], 2, false);
    i2c.write(RPR0521_addr_w, &RPR0521_Persist[0], 2, false);
#endif

#ifdef KMX62
  // 1. CNTL2 (0x3A), write (0x5F): 4g, Max RES, EN temp mag and accel
    i2c.write(KMX62_addr_w, &KMX62_CNTL2[0], 2, false);
#endif

#ifdef color
  // 1. CNTL2 (0x3A), write (0x5F): 4g, Max RES, EN temp mag and accel
    i2c.write(BH1745_addr_w, &BH1745_persistence[0], 2, false);
    i2c.write(BH1745_addr_w, &BH1745_mode1[0], 2, false);
    i2c.write(BH1745_addr_w, &BH1745_mode2[0], 2, false);
    i2c.write(BH1745_addr_w, &BH1745_mode3[0], 2, false);
#endif

#ifdef KX022

i2c.write(KX022_addr_w, &KX022_Accel_CNTL1[0], 2, false);
i2c.write(KX022_addr_w, &KX022_Accel_ODCNTL[0], 2, false);
i2c.write(KX022_addr_w, &KX022_Accel_CNTL3[0], 2, false);
i2c.write(KX022_addr_w, &KX022_Accel_TILT_TIMER[0], 2, false);
i2c.write(KX022_addr_w, &KX022_Accel_CNTL2[0], 2, false);

#endif

#ifdef Pressure

i2c.write(Press_addr_w, &PWR_DOWN[0], 2, false);
i2c.write(Press_addr_w, &SLEEP[0], 2, false);
i2c.write(Press_addr_w, &Mode_Control[0], 2, false);

#endif

//Start BTLE Initialization Section
    m_ble.init();
    m_ble.onDisconnection(disconnectionCallback);
    m_ble.onDataWritten(dataWrittenCallback);
    m_ble.onDataSent(dataSentCallback);

    // Set the TX power in dBm units.
    // Possible values (in decreasing order): 4, 0, -4, -8, -12, -16, -20.
    err = m_ble.setTxPower(4);
    if (BLE_ERROR_NONE != err) {
        error(err, __LINE__);
    }

    // Setup advertising (GAP stuff).
    err = m_ble.setDeviceName(DEVICE_NAME);
    if (BLE_ERROR_NONE != err) {
        error(err, __LINE__);
    }

    err = m_ble.accumulateAdvertisingPayload(GapAdvertisingData::BREDR_NOT_SUPPORTED);
    if (BLE_ERROR_NONE != err) {
        error(err, __LINE__);
    }

    m_ble.setAdvertisingType(GapAdvertisingParams::ADV_CONNECTABLE_UNDIRECTED);

    err = m_ble.accumulateAdvertisingPayload(GapAdvertisingData::SHORTENED_LOCAL_NAME,
                                                (const uint8_t *)SHORT_NAME,
                                                (sizeof(SHORT_NAME) - 1));
    if (BLE_ERROR_NONE != err) {
        error(err, __LINE__);
    }

    err = m_ble.accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LIST_128BIT_SERVICE_IDS,
                                                (const uint8_t *)UARTServiceUUID_reversed,
                                                sizeof(UARTServiceUUID_reversed));
    if (BLE_ERROR_NONE != err) {
        error(err, __LINE__);
    }

    m_ble.setAdvertisingInterval(Gap::MSEC_TO_ADVERTISEMENT_DURATION_UNITS(ADV_INTERVAL_MS));
    m_ble.startAdvertising();

    // Create a UARTService object (GATT stuff).
    UARTService uartService(m_ble);
    m_uart_service_ptr = &uartService;

    while (true) {
        m_ble.waitForEvent();
    }
}
    
    
    
    