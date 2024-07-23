/** \file
 * \brief Example code for Simple Open EtherCAT master
 *
 * Usage : simple_test [ifname1]
 * ifname is NIC interface, f.e. eth0
 *
 * This is a minimal test.
 *
 * (c)Arthur Ketels 2010 - 2011
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include "ethercat.h"
#include <stdbool.h>
#include <math.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/types.h>
#include <linux/module.h>
#include "set.h"
#include "get.h"

#include "../../../../config.h"
#include "../../../../shm/shm.hpp"
#include <fstream>

#define EC_TIMEOUTMON 500
#define NUM 1000
#define TH 2.0
// #define TH2 0.16666666667
#define TH2 1.0
#define MOTOR_NUM 2

char IOmap[4096];
OSAL_THREAD_HANDLE thread1;
int expectedWKC;
boolean needlf;
volatile int wkc;
boolean inOP;
uint8 currentgroup = 0;
boolean forceByteAlignment = FALSE;

typedef struct Cat_data {
    uint8 send[16];
    uint8* recv;
} cat_data;

cat_data motor[MOTOR_NUM];

static volatile int keepRunning = 1;
void signal_handler(int signum) {
  std::cerr<<std::endl<<std::endl<<std::endl<<std::endl<<"catch signum = "<<signum<<std::endl;
    keepRunning = 0;
}

std::vector<double> get_offset(void) {

    std::vector<double> pos_offset;

    char const* tmp1 = getenv( "SMALL_NIMBUS_ID" );
    char const* tmp2 = getenv( "SMALL_NIMBUS_PATH" );
    if( tmp1==NULL )
    {
      std::cerr << "ERROR: SMALL_NIMBUS_IDが設定されていません" << std::endl;
      std::exit(1);
    }
    else if( tmp2==NULL )
    {
      std::cerr << "ERROR: SMALL_NIMBUS_PATHが設定されていません" << std::endl;
      std::exit(1);
    }

    else
    {
      std::string SMALL_NIMBUS_ID = std::getenv("SMALL_NIMBUS_ID");
      std::string SMALL_NIMBUS_PATH = std::getenv("SMALL_NIMBUS_PATH");
      std::cerr << "SMALL_NIMBUS_ID: "<< SMALL_NIMBUS_ID << std::endl;
      std::cerr << "SMALL_NIMBUS_PATH: "<< SMALL_NIMBUS_PATH << std::endl;
      std::string OFFSET_FILE = SMALL_NIMBUS_PATH + "/hardware/unitree_actuator_sdk/example/offset.txt";
      std::ifstream file(OFFSET_FILE);

      // TODO: 現在はSMALL_NIMBUSが1号機と2号機しか存在しないことを仮定してエラー処理をしているが、これをもう少し一般的にチェックできるようにする。
      // https://github.com/proxima-technology/small_nimbus_ws/issues/111
      if( std::stoi(SMALL_NIMBUS_ID)!=1 && std::stoi(SMALL_NIMBUS_ID)!=2 )
      {
        std::cerr << "ERROR: SMALL_NIMBUS_ID should be 1 or 2" << std::endl;
        std::exit(1);
      }
      std::string line;
      std::string column0, column1, column2;
      if (file.is_open()) {
        while (getline(file, line)) {
          std::stringstream ss(line);
          ss >> column0 >> column1 >> column2;
          if (column0==SMALL_NIMBUS_ID){
            pos_offset.push_back(std::stod(column1));
            pos_offset.push_back(std::stod(column2));
            break;
          }
        }
      file.close();
      std::cerr << "関節原点オフセットファイル\n"<< OFFSET_FILE << "\nを開くことができました。\n" << std::endl;
      }
      else {
        std::cerr << "ERROR: 関節原点オフセットファイル\n"<< OFFSET_FILE << "\nを開けませんでした。\n" << std::endl;
        std::exit(1);
      }
    }

    return pos_offset;
}
std::vector<double> pos_offset = get_offset();
std::vector<double> legmotor_sensor_shared(num_data_legmotor_sensor, 0.0);
std::vector<double> legmotor_command_shared(num_data_legmotor_command, 0.0);

/*送信関数*/
void set_output(uint16 slave_no, uint8 module_index, uint8* value)
{
    set_crc(value);  // CRCの値を計算
    uint8* data_ptr;

    data_ptr = ec_slave[slave_no].outputs;
    data_ptr += module_index * 16;
    for (int i = 0; i < 16; i++) {
        *data_ptr++ = *value++;
    }
}

/*
    初期化関数
    使用するモータによりidの値を変える
    idはRS485で使用するid
*/
void set_init()
{
    /*RS485通信で使うidの変更*/
    set_id(0, motor[0].send);
    set_id(1, motor[1].send);
    set_id(0, motor[2].send);
    // set_id(0, motor[3].send);

    /*指令値をすべて0に設定*/
    for (int i = 0; i < MOTOR_NUM; i++) {
        set_mode(1, motor[i].send);
        set_torque(0.00, motor[i].send);
        set_speed(0, motor[i].send);
        set_K_P(0, motor[i].send);
        set_K_W(0, motor[i].send);
        set_position(0, motor[i].send);
    }
}

/*
    計測用関数
*/
void mesure(double* dat, double* ave, double* var, double* max, int* overcnt, int* overcnt2, double* min)
{
    double sum = 0;
    *max = 0;
    *min = 1000;
    *overcnt = 0;
    *overcnt2 = 0;
    for (int i = 0; i < NUM; i++) {
        if (*max < dat[i]) {
            *max = dat[i];
        }
        if (*min > dat[i]) {
            *min = dat[i];
        }
        if (dat[i] >= TH) {
            *overcnt += 1;
        }
        if (dat[i] >= TH2) {
            *overcnt2 += 1;
        }
        sum += dat[i];
    }
    *ave = sum / NUM;
    sum = 0;
    for (int i = 0; i < NUM; i++) {
        double j = dat[i] - *ave;
        sum += j * j;
    }
    *var = sum / NUM;
}

/*
    ethercat通信するスレッド
*/
void simpletest(char* ifname)
{
    int i, oloop, iloop, chk;
    needlf = FALSE;
    inOP = FALSE;

    // clock_t st_clock[MOTOR_NUM] = {0}, end_clock[MOTOR_NUM] = {0};
    uint8 check[MOTOR_NUM];
    bool recv_fin[MOTOR_NUM];
    for (int i = 0; i < MOTOR_NUM; i++) {
        check[i] = 255;
        recv_fin[i] = TRUE;
    }
    double time_count[MOTOR_NUM][NUM];
    int time_index[MOTOR_NUM] = {0};
    int now_time[MOTOR_NUM] = {0};
    double max_time[MOTOR_NUM] = {0};
    double min_time[MOTOR_NUM] = {0};
    double ave_time[MOTOR_NUM] = {0};
    double var_time[MOTOR_NUM] = {0};
    int over_num[MOTOR_NUM] = {0};
    int over_num2[MOTOR_NUM] = {0};

    printf("\033[2J\033[1;1H");  // 画面クリア
    printf("Starting simple test\n");

    int is_host = 1;
    ProcComm *proc_comm_sensor;
    ProcComm *proc_comm_command;
    proc_comm_sensor = new ProcComm(filename_data_legmotor_sensor, id_data_legmotor_sensor, num_data_legmotor_sensor, is_host);
    proc_comm_command = new ProcComm(filename_data_legmotor_command, id_data_legmotor_command, num_data_legmotor_command, is_host);
    int runtime_offset_rotation_count[MOTOR_NUM] = {0, 0};
    bool need_initialize_runtime_offset_rotation_count[MOTOR_NUM] = {true, true};

    /* initialise SOEM, bind socket to ifname */
    if (ec_init(ifname)) {
        printf("ec_init on %s succeeded.\n", ifname);
        /* find and auto-config slaves */


        if (ec_config_init(FALSE) > 0) {
            printf("%d slaves found and configured.\n", ec_slavecount);

            if (forceByteAlignment) {
                ec_config_map_aligned(&IOmap);
            } else {
                ec_config_map(&IOmap);
            }

            ec_configdc();

            printf("Slaves mapped, state to SAFE_OP.\n");
            /* wait for all slaves to reach SAFE_OP state */
            ec_statecheck(0, EC_STATE_SAFE_OP, EC_TIMEOUTSTATE * 4);

            oloop = ec_slave[0].Obytes;
            if ((oloop == 0) && (ec_slave[0].Obits > 0))
                oloop = 1;
            if (oloop > 64)
                oloop = 64;
            iloop = ec_slave[0].Ibytes;
            if ((iloop == 0) && (ec_slave[0].Ibits > 0))
                iloop = 1;
            if (iloop > 64)
                iloop = 64;

            printf("segments : %d : %d %d %d %d\n", ec_group[0].nsegments, ec_group[0].IOsegment[0], ec_group[0].IOsegment[1], ec_group[0].IOsegment[2], ec_group[0].IOsegment[3]);

            printf("Request operational state for all slaves\n");
            expectedWKC = (ec_group[0].outputsWKC * 2) + ec_group[0].inputsWKC;
            printf("Calculated workcounter %d\n", expectedWKC);
            printf("in%d out%d", oloop, iloop);
            ec_slave[0].state = EC_STATE_OPERATIONAL;
            /* send one valid process data to make outputs in slaves happy*/
            ec_send_processdata();
            ec_receive_processdata(EC_TIMEOUTRET);
            /* request OP state for all slaves */
            ec_writestate(0);
            chk = 200;
            /* wait for all slaves to reach OP state */
            while (chk-- && (ec_slave[0].state != EC_STATE_OPERATIONAL)){
                ec_send_processdata();
                ec_receive_processdata(EC_TIMEOUTRET);
                ec_statecheck(0, EC_STATE_OPERATIONAL, 50000);
            };
            if (ec_slave[0].state == EC_STATE_OPERATIONAL) {
                // printf("\nfdsjfakl\n");
                // printf("\a");
                printf("Operational state reached for all slaves.\n");
                printf("hello");
                // printf("\a");
                printf("\033[10;1H");
                printf("motor INFO");
                printf("\033[%d;1H", MOTOR_NUM + 13);
                printf("data transmission speed");

                inOP = TRUE;
                set_init();

                for (uint i = 0; i < MOTOR_NUM; i++) {
                    motor[i].recv = get_recv_pointer(i);
                }
                clock_t cyc_f = 0, cyc_f_pre = 0;
                float tor = 0;
                float tor2 = 0;
                struct timespec t_st[MOTOR_NUM], t_end[MOTOR_NUM];

                /* cyclic loop */
                for (;;) {
                    cyc_f = clock();
                    // printf("\033[%d;1H", 30);
                    double elapsedtime = (double)(cyc_f - cyc_f_pre) / CLOCKS_PER_SEC;

                    legmotor_command_shared = proc_comm_command->read_stdvec();

                    for (int i = 0; i < MOTOR_NUM; i++) {
                        if (recv_fin[i]) {
                            recv_fin[i] = FALSE;
                            motor[i].send[15] = check[i];

                            double torque_control = legmotor_command_shared[TORQUE_CMD_IDX*NUM_LEGMOTOR + i];
                            // TODO: 位置制御モードの実装
                            /*
                            double position_control_ratio = legmotor_command_shared[POSITION_CONTROL_MODE_RATIO_IDX*NUM_LEGMOTOR + i]; // [0,1] // 0 is using torque contol, 1 is using position control.
                            double target_pos = legmotor_command_shared[POSITION_TARGET_IDX*NUM_LEGMOTOR + i];
                            double kp = legmotor_command_shared[P_GAIN_IDX*NUM_LEGMOTOR + i];
                            double kd = legmotor_command_shared[D_GAIN_IDX*NUM_LEGMOTOR + i];
                            double ki = legmotor_command_shared[I_GAIN_IDX*NUM_LEGMOTOR + i];
                            double position_control = - kp*(measured_pos-target_pos) - kd*measured_vel; // todo: implement I control
                            double total_control = (1.0 - position_control_ratio) * torque_control + position_control_ratio * position_control;
                            */
                            double total_control = torque_control;

                            double torque_max = 23.5;
                            total_control = std::max(-torque_max, std::min(torque_max, total_control));

                            // reverse just before send
                            if(1==i)
                            {
                              total_control = - total_control;
                            }

                            /*指令値セット*/
                            #if (ENABLE_LEGMOTOR == 1)
                            set_mode(1, motor[i].send);
                            set_torque(total_control, motor[i].send);
                            // set_torque(0, motor[i].send);
                            set_speed(0, motor[i].send);
                            set_K_P(0, motor[i].send);
                            set_K_W(0, motor[i].send);
                            set_position(0, motor[i].send);
                            /***********************/
                            // st_clock[i] = clock();
                            clock_gettime(CLOCK_MONOTONIC, &t_st[i]);
                            set_output(1, i, motor[i].send);
                            #endif
                        }
                    }

                    ec_send_processdata();
                    wkc = ec_receive_processdata(EC_TIMEOUTRET);
                    if (wkc >= expectedWKC) {
                        for (int cnt = 0; cnt < MOTOR_NUM; cnt++) {
                            if (check[cnt] == *(motor[cnt].recv + 14)) {
                                // if (true) {
                                // end_clock[cnt] = clock();
                                clock_gettime(CLOCK_MONOTONIC, &t_end[cnt]);
                                recv_fin[cnt] = TRUE;
                                if (check[cnt] == 0) {
                                    check[cnt] = 255;
                                } else {
                                    check[cnt]--;
                                }
                                /* 受信データ表示
                                    id      :モータナンバー(unitreeのidとは違うもの)
                                    torque  :トルク
                                    anglevel:角速度
                                    angle   :角度
                                    temp    :温度
                                */
                                if (check_CRC(motor[cnt].recv)) {  // CRCチェック
                                    double raw_position = get_position(motor[cnt].recv);
                                    // pos_offset[cnt] = 0.0; // only for debug, to be deleted
                                    if(need_initialize_runtime_offset_rotation_count[cnt])
                                    {
                                      while(1)
                                      {
                                        legmotor_sensor_shared[POSITION_OBS_IDX*NUM_LEGMOTOR + cnt] = pos_offset[cnt] + raw_position / 6.33 + runtime_offset_rotation_count[cnt]*2*M_PI/6.33;
                                        if(std::abs(legmotor_sensor_shared[cnt])<(-1e-6 + M_PI/6.33))
                                        {
                                          break;
                                        }
                                        if(legmotor_sensor_shared[cnt]<-(M_PI/6.33))
                                        {
                                          runtime_offset_rotation_count[cnt]++;
                                        }
                                        if(legmotor_sensor_shared[cnt]>(M_PI/6.33))
                                        {
                                          runtime_offset_rotation_count[cnt]--;
                                        }
                                      }
                                      need_initialize_runtime_offset_rotation_count[cnt] = false;
                                    }
                                    legmotor_sensor_shared[POSITION_OBS_IDX*NUM_LEGMOTOR + cnt] = pos_offset[cnt] + raw_position / 6.33 + runtime_offset_rotation_count[cnt]*2*M_PI/6.33;

                                    legmotor_sensor_shared[VELOCITY_OBS_IDX*NUM_LEGMOTOR + cnt] = get_angular_vel(motor[cnt].recv) / 6.33;
                                    legmotor_sensor_shared[TORQUE_OBS_IDX*NUM_LEGMOTOR + cnt] = get_torque(motor[cnt].recv) * 6.33;
                                    legmotor_sensor_shared[TEMPERATURE_OBS_IDX*NUM_LEGMOTOR + cnt] = get_temp(motor[cnt].recv);
                                    // reverse just after read
                                    if(1==cnt)
                                    {
                                      legmotor_sensor_shared[POSITION_OBS_IDX*NUM_LEGMOTOR + cnt] = - legmotor_sensor_shared[POSITION_OBS_IDX*NUM_LEGMOTOR + cnt];
                                      legmotor_sensor_shared[VELOCITY_OBS_IDX*NUM_LEGMOTOR + cnt] = - legmotor_sensor_shared[VELOCITY_OBS_IDX*NUM_LEGMOTOR + cnt];
                                      legmotor_sensor_shared[TORQUE_OBS_IDX*NUM_LEGMOTOR + cnt] = - legmotor_sensor_shared[TORQUE_OBS_IDX*NUM_LEGMOTOR + cnt];
                                    }

                                    printf("\033[%d;1H", cnt + 12);

                                    char message[20];
                                    printf("\033[0K");
                                    // TODO: いい感じのprint文を実装（unitree_sdkの方も参考に）
                                    /*フィードバック値表示*/
                                    printf("id: %2d, angle: %12.6lf(rad), anglevel: %12.6lf(rad/s), torque: %10.6lf(Nm), temp: %3f℃ , error: %s\n", cnt,
                                      // get_position(motor[cnt].recv), get_angular_vel(motor[cnt].recv), get_torque(motor[cnt].recv), get_temp(motor[cnt].recv),
                                      legmotor_sensor_shared[POSITION_OBS_IDX*NUM_LEGMOTOR + cnt], legmotor_sensor_shared[VELOCITY_OBS_IDX*NUM_LEGMOTOR + cnt], legmotor_sensor_shared[TORQUE_OBS_IDX*NUM_LEGMOTOR + cnt], legmotor_sensor_shared[TEMPERATURE_OBS_IDX*NUM_LEGMOTOR + cnt],
                                      check_err(motor[cnt].recv, message));
                                    printf("\033[%d;1H\033[0K", MOTOR_NUM + 12 + cnt);
                                    time_count[cnt][time_index[cnt]] = (double)(t_end[cnt].tv_nsec - t_st[cnt].tv_nsec) / 1000000;
                                    if (time_count[cnt][time_index[cnt]] < 0) {
                                        time_count[cnt][time_index[cnt]] += 1000;
                                    }
                                    now_time[cnt] = time_index[cnt];
                                    time_index[cnt]++;
                                    if (time_index[cnt] == NUM) {
                                        time_index[cnt] = 0;
                                        mesure(time_count[cnt], &(ave_time[cnt]), &(var_time[cnt]), &(max_time[cnt]), &(over_num[cnt]), &over_num2[cnt], &min_time[cnt]);
                                    }
                                }
                                else
                                {
                                    printf("\033[%d;1H", MOTOR_NUM + 12 + cnt);
                                    printf("id %d CRC_error", cnt);
                                    printf("\a");
                                    check[cnt]++;
                                }
                            }
                        } // End of for (int cnt = 0; cnt < MOTOR_NUM; cnt++)
                        proc_comm_sensor->write_stdvec(legmotor_sensor_shared);

                        /*計測時間表示*/
                        /*
                        for (int cnt = 0; cnt < MOTOR_NUM; cnt++) {
                            printf("\033[%d;1H", MOTOR_NUM + 15 + cnt);
                            printf("\033[0K");
                            printf("id %d: time %fms ,", cnt, time_count[cnt][now_time[cnt]]);
                            printf("ave %8.6fms ,var %8.6fms ,max %8.6fms ,min %8.6fms ,over ratio(%5.2lfkHz) %7.4f %% ,over ratio(%5.2lfkHz) %7.4f %%\n", ave_time[cnt], var_time[cnt], max_time[cnt], min_time[cnt], 1.0 / (float)TH, (float)over_num[cnt] / (float)NUM * 100.0, 1.0 / (float)TH2, (float)over_num2[cnt] / (float)NUM * 100.0);
                        }
                        */
                        needlf = TRUE;
                    } // End of if (wkc >= expectedWKC)
                    else
                    {
                        printf("wkc error\n");
                    }
                    if (0==keepRunning) break;
                    // osal_usleep(50);
                } // End of cyclic loop
                inOP = FALSE;
            } // End of if (ec_slave[0].state == EC_STATE_OPERATIONAL)
            else
            {
                printf("Not all slaves reached operational state.\n");
                ec_readstate();
                for (i = 1; i <= ec_slavecount; i++) {
                    if (ec_slave[i].state != EC_STATE_OPERATIONAL) {
                        printf("Slave %d State=0x%2.2x StatusCode=0x%4.4x : %s\n",
                            i, ec_slave[i].state, ec_slave[i].ALstatuscode, ec_ALstatuscode2string(ec_slave[i].ALstatuscode));
                    }
                }
            }
            printf("\n\nRequest init state for all slaves\n");
            ec_slave[0].state = EC_STATE_INIT;
            /* request INIT state for all slaves */
            ec_writestate(0);
        } // End of if (ec_config_init(FALSE) > 0)
        else
        {
            printf("No slaves found!\n");
        }
        printf("End simple test, close socket\n");
        printf("keepRunning = %d \n",keepRunning);

        // ゼロ指令値を送信して終了
        for (int i = 0; i < MOTOR_NUM; i++) {
          if (recv_fin[i]) {
            recv_fin[i] = FALSE;
            motor[i].send[15] = check[i];
            /*指令値セット*/
            set_mode(1, motor[i].send);
            set_torque(0, motor[i].send);
            set_speed(0, motor[i].send);
            set_K_P(0, motor[i].send);
            set_K_W(0, motor[i].send);
            set_position(0, motor[i].send);
            set_output(1, i, motor[i].send);
          }
        }

        /* stop SOEM, close socket */
        ec_close();
    } // End of if (ec_init(ifname))
    else
    {
        printf("No socket connection on %s\nExecute as root\n", ifname);
    }

    delete proc_comm_sensor;
    delete proc_comm_command;
}

OSAL_THREAD_FUNC ecatcheck(void* ptr)
{
    int slave;
    (void)ptr; /* Not used */

    while (1) {
        if (inOP && ((wkc < expectedWKC) || ec_group[currentgroup].docheckstate)) {
            if (needlf) {
                needlf = FALSE;
                printf("\n");
            }
            /* one ore more slaves are not responding */
            ec_group[currentgroup].docheckstate = FALSE;
            ec_readstate();
            for (slave = 1; slave <= ec_slavecount; slave++) {
                if ((ec_slave[slave].group == currentgroup) && (ec_slave[slave].state != EC_STATE_OPERATIONAL)) {
                    ec_group[currentgroup].docheckstate = TRUE;
                    if (ec_slave[slave].state == (EC_STATE_SAFE_OP + EC_STATE_ERROR)) {
                        //printf("\033[%d;1H", MOTOR_NUM + 20);
                        printf("ERROR : slave %d is in SAFE_OP + ERROR, attempting ack.\n", slave);
                        ec_slave[slave].state = (EC_STATE_SAFE_OP + EC_STATE_ACK);
                        ec_writestate(slave);
                    } else if (ec_slave[slave].state == EC_STATE_SAFE_OP) {
                        //printf("\033[%d;1H", MOTOR_NUM + 20);
                        printf("WARNING : slave %d is in SAFE_OP, change to OPERATIONAL.\n", slave);
                        ec_slave[slave].state = EC_STATE_OPERATIONAL;
                        ec_writestate(slave);
                    } else if (ec_slave[slave].state > EC_STATE_NONE) {
                        if (ec_reconfig_slave(slave, EC_TIMEOUTMON)) {
                            ec_slave[slave].islost = FALSE;
                            //printf("\033[%d;1H", MOTOR_NUM + 20);
                            printf("MESSAGE : slave %d reconfigured\n", slave);
                        }
                    } else if (!ec_slave[slave].islost) {
                        /* re-check state */
                        ec_statecheck(slave, EC_STATE_OPERATIONAL, EC_TIMEOUTRET);
                        if (ec_slave[slave].state == EC_STATE_NONE) {
                            ec_slave[slave].islost = TRUE;
                            printf("ERROR : slave %d lost\n", slave);
                        }
                    }
                }
                if (ec_slave[slave].islost) {
                    if (ec_slave[slave].state == EC_STATE_NONE) {
                        if (ec_recover_slave(slave, EC_TIMEOUTMON)) {
                            ec_slave[slave].islost = FALSE;
                            printf("MESSAGE : slave %d recovered\n", slave);
                        }
                    } else {
                        ec_slave[slave].islost = FALSE;
                        printf("MESSAGE : slave %d found\n", slave);
                    }
                }
            }
            // if (!ec_group[currentgroup].docheckstate)
            //     printf("OK : all slaves resumed OPERATIONAL.\n");
        }
        osal_usleep(10000);
    }
}

int main(int argc, char* argv[])
{
    printf("SOEM (Simple Open EtherCAT Master)\nSimple test\n");

    std::cout<<"ENABLE_LEGMOTOR: "<<ENABLE_LEGMOTOR<<std::endl;

    signal(SIGINT, signal_handler); // killed by ctrl+C
    signal(SIGHUP, signal_handler); // killed by tmux kill-server

    #if (ENABLE_LEGMOTOR > 0)
    if (argc > 1) {
        /* create thread to handle slave error handling in OP */
        // osal_thread_create(&thread1, 128000, &ecatcheck, NULL);
        osal_thread_create(&thread1, 128000, (void *)&ecatcheck, NULL);
        /* start cyclic part */
        simpletest(argv[1]);
    } else {
        ec_adaptert* adapter = NULL;
        printf("Usage: simple_test ifname1\nifname = eth0 for example\n");

        printf("\nAvailable adapters:\n");
        adapter = ec_find_adapters();
        while (adapter != NULL) {
            printf("    - %s  (%s)\n", adapter->name, adapter->desc);
            adapter = adapter->next;
        }
        ec_free_adapters(adapter);
    }
    #endif

    #if (ENABLE_LEGMOTOR == 0)
    // TODO: ENABLE=0の場合の実装
    #endif

    printf("End program\n");
    return (0);
}
