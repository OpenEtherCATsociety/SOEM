#include "ethercat.h"

/*
    受信data配列取得関数
*/
uint8* get_recv_pointer(uint8 motor_id)
{
    return ec_slave[1].inputs + motor_id * 15;
}

/*トルク取得関数*/
double get_torque(uint8* motor_data)
{
    double act_torque;
    act_torque = ((int16)(motor_data[1] + ((uint16)(motor_data[2]) << 8))) / 256.0;
    return act_torque;
}

/*角速度取得関数*/
double get_angular_vel(uint8* motor_data)
{
    double act_angl_vel;
    act_angl_vel = ((int16)(motor_data[3] + ((uint16)(motor_data[4]) << 8))) / 256.0 * 2 * M_PI;
    return act_angl_vel;
}

/*角度取得関数*/
double get_position(uint8* motor_data)
{
    double act_angl;
    act_angl = ((int32)(motor_data[5] + ((uint32)(motor_data[6]) << 8) + ((uint32)(motor_data[7]) << 16) + ((uint32)motor_data[8] << 24))) / 32768.0 * 2 * M_PI;
    return act_angl;
}

/*温度取得関数*/
int8 get_temp(uint8* motor_data)
{
    return (int8)(motor_data[9]);
}

// /*errorチェック*/
// bool check_err(uint8* motor_data, uint8 motor_id)
// {
//     uint8 err = motor_data[10] | 0x7;
//     switch (err) {
//     case 1:
//         printf("ID%d: Overheat\n", motor_id);
//         return TRUE;
//         break;
//     case 2:
//         printf("ID%d: Overcurrent\n", motor_id);
//         return TRUE;
//         break;
//     case 3:
//         printf("ID%d: Overvoltage\n", motor_id);
//         return TRUE;
//         break;
//     case 4:
//         printf("ID%d: Encoder fault\n", motor_id);
//         return TRUE;
//         break;
//     default:
//         return FALSE;
//         break;
//     }
// }

/*errorチェック*/
char* check_err(uint8* motor_data, char* message)
{
    uint8 err = motor_data[10] | 0x7;
    switch (err) {
    case 1:
        strcpy(message, "Overheat\0");
        return message;
        break;
    case 2:
        strcpy(message, "Overcurrent\0");
        return message;
        break;
    case 3:
        strcpy(message, "Overvoltage\0");
        return message;
        break;
    case 4:
        strcpy(message, "Encoder fault\0");
        return message;
        break;
    default:
        strcpy(message, "No error\0");
        return message;
        break;
    }
}

/*CRCチェック*/
bool check_CRC(uint8* motor_data)
{
    uint16 crc = 0;
    crc = crc_ccitt_byte(crc, 0xFD);
    crc = crc_ccitt_byte(crc, 0xEE);
    for (int i = 0; i < 12; i++)
        crc = crc_ccitt_byte(crc, motor_data[i]);
    uint16 check = motor_data[12] + ((uint16)(motor_data[13]) << 8);
    return (check == crc);
}
