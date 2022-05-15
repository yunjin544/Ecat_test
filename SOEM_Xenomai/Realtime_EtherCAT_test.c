#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>

#include <alchemy/task.h>
#include <alchemy/timer.h>
#include "ethercat.h"


#define CLOCK_RES 1e-9 
#define EC_TIMEOUTMON 500
#define LOOP_PERIOD 5e5
#define NSEC_PER_SEC 1000000000

char IOmap[64];
int expectedWKC;
int chk;
volatile int wkc;
OSAL_THREAD_HANDLE thread1, thread2;
boolean LED_Control = TRUE;
boolean needlf;
boolean inOP;
uint8 currentgroup = 0;
int32 toff, gl_delta;
RT_TASK loop_task;


void ECat_init(char *ifname);
void ECat_PDO_LOOP(void *arg);

OSAL_THREAD_FUNC Ecatcheck(void *ptr);


int main(int argc, char *argv[])
{
   int ctime;
   if (argc > 1)
   {
      
      ECat_init(argv[1]);
      osal_thread_create(&thread1, 128000, &Ecatcheck, NULL);
      rt_task_create(&loop_task, "Ecat Loop", 0, 99, 0);
      rt_task_start(&loop_task, &ECat_PDO_LOOP, 0);
      pause();

   }
   else
   {
      ec_adaptert *adapter = NULL;
      printf("Usage: simple_test ifname1\nifname = eth0 for example\n");

      printf("\nAvailable adapters:\n");
      adapter = ec_find_adapters();
      while (adapter != NULL)
      {
         printf("    - %s  (%s)\n", adapter->name, adapter->desc);
         adapter = adapter->next;
      }
      ec_free_adapters(adapter);
   }

   return 0;
}

void ECat_init(char *ifname)
{

   if (ec_init(ifname) > 0)
   {
      printf("Starting Ecat DRCL Master Test\n");

      if (ec_config_init(FALSE) > 0)
      {
         printf("%d slaves found and configured.\n", ec_slavecount);
      }

      ec_config_map(&IOmap);
      printf("Slaves mapped, state to SAFE_OP.\n");
      ec_statecheck(0, EC_STATE_SAFE_OP, EC_TIMEOUTSTATE * 4);

      printf("segments : %d : %d %d %d %d\n", ec_group[0].nsegments, ec_group[0].IOsegment[0], ec_group[0].IOsegment[1], ec_group[0].IOsegment[2], ec_group[0].IOsegment[3]);
      expectedWKC = (ec_group[0].outputsWKC * 2) + ec_group[0].inputsWKC;
      printf("Calculated workcounter %d\n", expectedWKC);
      ec_slave[0].state = EC_STATE_OPERATIONAL;
      ec_send_processdata();
      ec_receive_processdata(EC_TIMEOUTRET);
      ec_writestate(0);
      chk = 200;
      ec_send_processdata();
      ec_receive_processdata(EC_TIMEOUTRET);
      ec_statecheck(0, EC_STATE_OPERATIONAL, 50000);
      while (chk-- && (ec_slave[0].state != EC_STATE_OPERATIONAL))
         ;
   }
}


void ECat_PDO_LOOP(void *arg)
{
   RT_TASK *curtask;
   rt_task_set_periodic(NULL, TM_NOW, LOOP_PERIOD);
   ec_slave[0].outputs[0] = LED_Control;
   ec_send_processdata();
   while (1)
   {

      wkc = ec_receive_processdata(EC_TIMEOUTRET);

      
      ec_slave[0].outputs[0] = !(ec_slave[0].outputs[0]);

      //ec_dcsync0(0, TRUE, cycletime, toff);

      ec_send_processdata();

      rt_task_wait_period(NULL);
   }
}

OSAL_THREAD_FUNC Ecatcheck(void *ptr)
{
   int slave;
   (void)ptr; /* Not used */

   while (1)
   {
      if (inOP && ((wkc < expectedWKC) || ec_group[currentgroup].docheckstate))
      {
         if (needlf)
         {
            needlf = FALSE;
            printf("\n");
         }
         /* one ore more slaves are not responding */
         ec_group[currentgroup].docheckstate = FALSE;
         ec_readstate();
         for (slave = 1; slave <= ec_slavecount; slave++)
         {
            if ((ec_slave[slave].group == currentgroup) && (ec_slave[slave].state != EC_STATE_OPERATIONAL))
            {
               ec_group[currentgroup].docheckstate = TRUE;
               if (ec_slave[slave].state == (EC_STATE_SAFE_OP + EC_STATE_ERROR))
               {
                  printf("ERROR : slave %d is in SAFE_OP + ERROR, attempting ack.\n", slave);
                  ec_slave[slave].state = (EC_STATE_SAFE_OP + EC_STATE_ACK);
                  ec_writestate(slave);
               }
               else if (ec_slave[slave].state == EC_STATE_SAFE_OP)
               {
                  printf("WARNING : slave %d is in SAFE_OP, change to OPERATIONAL.\n", slave);
                  ec_slave[slave].state = EC_STATE_OPERATIONAL;
                  ec_writestate(slave);
               }
               else if (ec_slave[slave].state > EC_STATE_NONE)
               {
                  if (ec_reconfig_slave(slave, EC_TIMEOUTMON))
                  {
                     ec_slave[slave].islost = FALSE;
                     printf("MESSAGE : slave %d reconfigured\n", slave);
                  }
               }
               else if (!ec_slave[slave].islost)
               {
                  /* re-check state */
                  ec_statecheck(slave, EC_STATE_OPERATIONAL, EC_TIMEOUTRET);
                  if (ec_slave[slave].state == EC_STATE_NONE)
                  {
                     ec_slave[slave].islost = TRUE;
                     printf("ERROR : slave %d lost\n", slave);
                  }
               }
            }
            if (ec_slave[slave].islost)
            {
               if (ec_slave[slave].state == EC_STATE_NONE)
               {
                  if (ec_recover_slave(slave, EC_TIMEOUTMON))
                  {
                     ec_slave[slave].islost = FALSE;
                     printf("MESSAGE : slave %d recovered\n", slave);
                  }
               }
               else
               {
                  ec_slave[slave].islost = FALSE;
                  printf("MESSAGE : slave %d found\n", slave);
               }
            }
         }
         if (!ec_group[currentgroup].docheckstate)
            printf("OK : all slaves resumed OPERATIONAL.\n");
      }
   }
}
